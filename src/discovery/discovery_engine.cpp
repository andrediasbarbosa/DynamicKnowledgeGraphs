#include "discovery/discovery_engine.hpp"
#include "llm/llm_provider.hpp"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <limits>
#include <set>
#include <cctype>
#include <numeric>  // for std::iota
#include <cstdint>

namespace kg {

namespace {
std::string to_lower_copy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool looks_like_person(const std::string& label) {
    std::string trimmed = label;
    if (trimmed.empty()) return false;
    std::string lower = to_lower_copy(trimmed);
    if (lower.find("et al") != std::string::npos) return true;
    int word_count = 0;
    int capitalized_words = 0;
    bool has_initial = false;
    std::stringstream ss(trimmed);
    std::string word;
    while (ss >> word) {
        word_count++;
        if (word.size() >= 2 && std::isupper(static_cast<unsigned char>(word[0])) &&
            word[1] == '.') {
            has_initial = true;
            capitalized_words++;
            continue;
        }
        if (!word.empty() && std::isupper(static_cast<unsigned char>(word[0]))) {
            capitalized_words++;
        }
    }
    if (word_count >= 2 && capitalized_words >= 2) return true;
    if (has_initial && word_count >= 2) return true;
    return false;
}

bool looks_like_reference_relation(const std::string& relation) {
    if (relation.empty()) return false;
    std::string lower = to_lower_copy(relation);
    return lower.find("cite") != std::string::npos ||
           lower.find("cited") != std::string::npos ||
           lower.find("citation") != std::string::npos ||
           lower.find("reference") != std::string::npos ||
           lower.find("refer") != std::string::npos ||
           lower.find("bibliograph") != std::string::npos ||
           lower.find("works cited") != std::string::npos;
}

bool looks_like_work(const std::string& label) {
    if (label.empty()) return false;
    int word_count = 0;
    std::stringstream ss(label);
    std::string word;
    while (ss >> word) word_count++;
    if (word_count >= 3) return true;
    std::string lower = to_lower_copy(label);
    return lower.find("introduction") != std::string::npos ||
           lower.find("survey") != std::string::npos ||
           lower.find("paper") != std::string::npos ||
           lower.find("chapter") != std::string::npos;
}

bool nodes_share_edge(const Hypergraph& graph, const std::string& a, const std::string& b) {
    const auto* node_a = graph.get_node(a);
    if (!node_a) return false;
    std::unordered_set<std::string> edges_a(node_a->incident_edges.begin(), node_a->incident_edges.end());
    const auto* node_b = graph.get_node(b);
    if (!node_b) return false;
    for (const auto& eid : node_b->incident_edges) {
        if (edges_a.find(eid) != edges_a.end()) return true;
    }
    return false;
}

std::string join_labels(const Hypergraph& graph, const std::vector<std::string>& nodes, size_t max_items) {
    std::stringstream ss;
    for (size_t i = 0; i < std::min(nodes.size(), max_items); ++i) {
        if (i > 0) ss << ", ";
        const auto* node = graph.get_node(nodes[i]);
        ss << (node ? node->label : nodes[i]);
    }
    if (nodes.size() > max_items) ss << ", ...";
    return ss.str();
}

struct ProjectionGraph {
    std::vector<std::string> node_ids;
    std::unordered_map<std::string, size_t> node_index;
    std::vector<std::unordered_map<size_t, double>> adj;
};

ProjectionGraph build_projection_graph(const Hypergraph& graph) {
    ProjectionGraph proj;
    auto nodes = graph.get_all_nodes();
    proj.node_ids.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        proj.node_ids.push_back(nodes[i].id);
        proj.node_index[nodes[i].id] = i;
    }
    proj.adj.resize(nodes.size());

    auto edges = graph.get_all_edges();
    for (const auto& edge : edges) {
        std::vector<std::string> edge_nodes;
        edge_nodes.reserve(edge.size());
        for (const auto& nid : edge.get_all_nodes()) {
            if (proj.node_index.find(nid) != proj.node_index.end()) {
                edge_nodes.push_back(nid);
            }
        }
        for (size_t i = 0; i < edge_nodes.size(); ++i) {
            size_t a = proj.node_index[edge_nodes[i]];
            for (size_t j = i + 1; j < edge_nodes.size(); ++j) {
                size_t b = proj.node_index[edge_nodes[j]];
                proj.adj[a][b] += 1.0;
                proj.adj[b][a] += 1.0;
            }
        }
    }

    return proj;
}

struct BipartiteGraph {
    size_t num_entities = 0;
    std::vector<std::string> entity_ids;
    std::vector<std::string> edge_ids;
    std::unordered_map<std::string, size_t> entity_index;
    std::vector<std::vector<size_t>> adj;
};

BipartiteGraph build_bipartite_graph(const Hypergraph& graph) {
    BipartiteGraph bi;
    auto nodes = graph.get_all_nodes();
    auto edges = graph.get_all_edges();
    bi.num_entities = nodes.size();
    bi.entity_ids.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        bi.entity_ids.push_back(nodes[i].id);
        bi.entity_index[nodes[i].id] = i;
    }
    bi.edge_ids.reserve(edges.size());
    for (const auto& edge : edges) {
        bi.edge_ids.push_back(edge.id);
    }
    size_t total = nodes.size() + edges.size();
    bi.adj.resize(total);

    for (size_t eidx = 0; eidx < edges.size(); ++eidx) {
        const auto& edge = edges[eidx];
        size_t edge_node_idx = nodes.size() + eidx;
        for (const auto& nid : edge.get_all_nodes()) {
            auto it = bi.entity_index.find(nid);
            if (it == bi.entity_index.end()) continue;
            size_t nidx = it->second;
            bi.adj[nidx].push_back(edge_node_idx);
            bi.adj[edge_node_idx].push_back(nidx);
        }
    }
    return bi;
}

std::vector<std::string> shared_incident_edges(
    const Hypergraph& graph,
    const std::string& a,
    const std::string& b,
    size_t max_edges) {
    std::vector<std::string> shared;
    const auto* node_a = graph.get_node(a);
    const auto* node_b = graph.get_node(b);
    if (!node_a || !node_b) return shared;
    std::unordered_set<std::string> edges_a(node_a->incident_edges.begin(), node_a->incident_edges.end());
    for (const auto& eid : node_b->incident_edges) {
        if (edges_a.find(eid) != edges_a.end()) {
            shared.push_back(eid);
            if (shared.size() >= max_edges) break;
        }
    }
    return shared;
}

std::unordered_set<std::string> collect_neighbors(
    const Hypergraph& graph,
    const std::string& node_id,
    size_t max_neighbors) {
    std::unordered_set<std::string> neighbors;
    const auto* node = graph.get_node(node_id);
    if (!node) return neighbors;
    for (const auto& eid : node->incident_edges) {
        const auto* edge = graph.get_hyperedge(eid);
        if (!edge) continue;
        for (const auto& src : edge->sources) {
            if (src != node_id) neighbors.insert(src);
            if (neighbors.size() >= max_neighbors) return neighbors;
        }
        for (const auto& tgt : edge->targets) {
            if (tgt != node_id) neighbors.insert(tgt);
            if (neighbors.size() >= max_neighbors) return neighbors;
        }
    }
    return neighbors;
}

double jaccard_overlap(const std::unordered_set<std::string>& a,
                       const std::unordered_set<std::string>& b) {
    if (a.empty() && b.empty()) return 0.0;
    size_t intersection = 0;
    if (a.size() < b.size()) {
        for (const auto& item : a) {
            if (b.find(item) != b.end()) intersection++;
        }
    } else {
        for (const auto& item : b) {
            if (a.find(item) != a.end()) intersection++;
        }
    }
    size_t uni = a.size() + b.size() - intersection;
    return uni > 0 ? static_cast<double>(intersection) / static_cast<double>(uni) : 0.0;
}

std::vector<std::string> tokenize_simple(const std::string& text) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (unsigned char c : text) {
        if (std::isalnum(c)) {
            cleaned.push_back(static_cast<char>(std::tolower(c)));
        } else {
            cleaned.push_back(' ');
        }
    }
    std::stringstream ss(cleaned);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) {
        if (token.size() > 3 && token.back() == 's') {
            token.pop_back();
        }
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

std::string normalize_label_key(const std::string& label) {
    auto tokens = tokenize_simple(label);
    std::string norm;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) norm.push_back(' ');
        norm += tokens[i];
    }
    return norm;
}

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::string parse_llm_field(const std::string& content, const std::string& key) {
    std::istringstream iss(content);
    std::string line;
    std::string prefix = key + ":";
    while (std::getline(iss, line)) {
        if (line.rfind(prefix, 0) == 0) {
            return trim_copy(line.substr(prefix.size()));
        }
    }
    return "";
}

double token_jaccard(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.empty() || b.empty()) return 0.0;
    std::unordered_set<std::string> set_a(a.begin(), a.end());
    std::unordered_set<std::string> set_b(b.begin(), b.end());
    return jaccard_overlap(set_a, set_b);
}

double label_token_similarity(const Hypergraph& graph, const std::string& a, const std::string& b) {
    const auto* na = graph.get_node(a);
    const auto* nb = graph.get_node(b);
    if (!na || !nb) return 0.0;
    auto ta = tokenize_simple(na->label);
    auto tb = tokenize_simple(nb->label);
    if (ta.empty() || tb.empty()) return 0.0;
    return token_jaccard(ta, tb);
}

std::string normalize_relation_base(const std::string& relation, bool* negated) {
    if (negated) *negated = false;
    if (relation.empty()) return "";

    static const std::unordered_set<std::string> neg_tokens = {
        "not", "no", "without", "lack", "lacks", "lacking", "absence", "absent",
        "cannot", "cant", "dont", "doesnt", "didnt", "isnt", "arent", "wont", "never", "none"
    };

    auto tokens = tokenize_simple(relation);
    std::vector<std::string> base_tokens;
    base_tokens.reserve(tokens.size());
    for (const auto& token : tokens) {
        if (neg_tokens.find(token) != neg_tokens.end()) {
            if (negated) *negated = true;
            continue;
        }
        base_tokens.push_back(token);
    }
    if (base_tokens.empty()) return "";

    std::string base;
    for (size_t i = 0; i < base_tokens.size(); ++i) {
        if (i > 0) base.push_back(' ');
        base += base_tokens[i];
    }
    return base;
}

std::string edge_signature(const HyperEdge& edge) {
    std::vector<std::string> sources = edge.sources;
    std::vector<std::string> targets = edge.targets;
    std::sort(sources.begin(), sources.end());
    std::sort(targets.begin(), targets.end());
    std::string sig = "S:";
    for (size_t i = 0; i < sources.size(); ++i) {
        if (i > 0) sig.push_back(',');
        sig += sources[i];
    }
    sig += "|T:";
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) sig.push_back(',');
        sig += targets[i];
    }
    return sig;
}

std::vector<Insight> select_by_target(const std::vector<Insight>& insights, size_t target) {
    if (target == 0 || insights.size() <= target) {
        return insights;
    }
    std::vector<Insight> sorted = insights;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    double threshold = sorted[target - 1].score;
    std::vector<Insight> out;
    out.reserve(sorted.size());
    for (const auto& ins : sorted) {
        if (ins.score >= threshold) {
            out.push_back(ins);
        }
    }
    if (out.size() < target) {
        out.assign(sorted.begin(), sorted.begin() + target);
    }
    return out;
}
} // namespace

DiscoveryEngine::DiscoveryEngine(const Hypergraph& graph, const HypergraphIndex& index)
    : graph_(graph), index_(index) {
    // Generate default run_id
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "run_" << std::put_time(std::gmtime(&time), "%Y%m%d_%H%M%S");
    run_id_ = ss.str();
}

std::string DiscoveryEngine::make_insight_id(InsightType type) {
    std::stringstream ss;
    ss << run_id_ << ":" << insight_type_to_string(type) << ":"
       << std::setfill('0') << std::setw(6) << (++insight_counter_);
    return ss.str();
}

std::string DiscoveryEngine::get_node_label(const std::string& node_id) const {
    const auto* node = graph_.get_node(node_id);
    return node ? node->label : "";
}

std::vector<std::string> DiscoveryEngine::get_chunk_ids(const std::vector<std::string>& edge_ids) const {
    std::unordered_set<std::string> chunks;
    for (const auto& eid : edge_ids) {
        const auto* edge = graph_.get_hyperedge(eid);
        if (edge && !edge->source_chunk_id.empty()) {
            chunks.insert(edge->source_chunk_id);
        }
    }
    return std::vector<std::string>(chunks.begin(), chunks.end());
}

bool DiscoveryEngine::is_author_reference_insight(const Insight& insight) const {
    if (insight.seed_nodes.empty() || insight.witness_edges.empty()) {
        return false;
    }

    size_t author_count = 0;
    for (const auto& node_id : insight.seed_nodes) {
        if (looks_like_person(get_node_label(node_id))) {
            author_count++;
        }
    }
    if (author_count != insight.seed_nodes.size()) {
        return false;
    }

    for (const auto& edge_id : insight.witness_edges) {
        const auto* edge = graph_.get_hyperedge(edge_id);
        if (edge && looks_like_reference_relation(edge->relation)) {
            return true;
        }
    }

    return false;
}

void DiscoveryEngine::report_progress(const std::string& stage, int current, int total) {
    if (progress_cb_) {
        progress_cb_(stage, current, total);
    }
}

double DiscoveryEngine::compute_score(const Insight& insight) {
    // Score = weighted combination of support, novelty, specificity

    // Support: log of witness edges
    double support = insight.witness_edges.empty() ? 0.0 :
        std::log(1.0 + insight.witness_edges.size()) / std::log(10.0);

    // Novelty: based on average co-occurrence (lower = more novel)
    double novelty = 0.5;
    if (insight.seed_nodes.size() >= 2) {
        double total_cooc = 0;
        int pairs = 0;
        for (size_t i = 0; i < insight.seed_nodes.size(); ++i) {
            for (size_t j = i + 1; j < insight.seed_nodes.size(); ++j) {
                total_cooc += index_.get_cooccurrence(insight.seed_nodes[i], insight.seed_nodes[j]);
                pairs++;
            }
        }
        double avg_cooc = pairs > 0 ? total_cooc / pairs : 0;
        novelty = 1.0 / (1.0 + std::log(1.0 + avg_cooc));
    }

    // Specificity: inverse of average degree
    double specificity = 0.5;
    if (!insight.seed_nodes.empty()) {
        double total_degree = 0;
        for (const auto& nid : insight.seed_nodes) {
            const auto* node = graph_.get_node(nid);
            if (node) total_degree += node->degree;
        }
        double avg_degree = total_degree / insight.seed_nodes.size();
        specificity = 1.0 / (1.0 + std::log(1.0 + avg_degree));
    }

    return 0.4 * support + 0.35 * novelty + 0.25 * specificity;
}

// ============== BRIDGE DETECTION ==============
std::vector<Insight> DiscoveryEngine::find_bridges() {
    std::vector<Insight> results;
    report_progress("Finding bridges", 0, 100);

    int s = config_.bridge_s_threshold;
    auto it = index_.s_components.find(s);
    if (it == index_.s_components.end() || it->second.size() < 2) {
        return results;
    }

    const auto& components = it->second;
    report_progress("Finding bridges", 10, 100);

    std::unordered_map<std::string, int> edge_to_component;
    for (size_t ci = 0; ci < components.size(); ++ci) {
        for (const auto& eid : components[ci]) {
            edge_to_component[eid] = static_cast<int>(ci);
        }
    }

    std::unordered_map<std::string, std::set<int>> node_components;
    auto all_nodes = graph_.get_all_nodes();
    for (const auto& node : all_nodes) {
        std::set<int> comps;
        for (const auto& eid : node.incident_edges) {
            auto cit = edge_to_component.find(eid);
            if (cit != edge_to_component.end()) {
                comps.insert(cit->second);
            }
        }
        if (comps.size() >= 2) {
            node_components[node.id] = comps;
        }
    }

    report_progress("Finding bridges", 50, 100);

    std::vector<std::pair<std::string, size_t>> candidates;
    for (const auto& [node_id, comps] : node_components) {
        candidates.emplace_back(node_id, comps.size());
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t i = 0; i < std::min(candidates.size(), config_.bridge_max_candidates); ++i) {
        const std::string& node_id = candidates[i].first;
        const auto& comps = node_components[node_id];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::BRIDGE);
        ins.type = InsightType::BRIDGE;
        ins.seed_nodes = {node_id};
        ins.seed_labels = {get_node_label(node_id)};

        const auto* node = graph_.get_node(node_id);
        if (node) {
            ins.witness_edges = node->incident_edges;
            ins.witness_nodes.push_back(node_id);

            for (const auto& eid : node->incident_edges) {
                const auto* edge = graph_.get_hyperedge(eid);
                if (edge) {
                    for (const auto& src : edge->sources) ins.witness_nodes.push_back(src);
                    for (const auto& tgt : edge->targets) ins.witness_nodes.push_back(tgt);
                }
            }
            std::sort(ins.witness_nodes.begin(), ins.witness_nodes.end());
            ins.witness_nodes.erase(
                std::unique(ins.witness_nodes.begin(), ins.witness_nodes.end()),
                ins.witness_nodes.end());
        }

        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"cross_s" + std::to_string(s) + "_components"};
        ins.description = "Bridge node connecting " + std::to_string(comps.size()) +
                         " s=" + std::to_string(s) + " components";

        ins.score_breakdown["support"] = static_cast<double>(ins.witness_edges.size());
        ins.score_breakdown["novelty"] = 1.0 / comps.size();
        ins.score_breakdown["specificity"] = 1.0;
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Finding bridges", 100, 100);
    return results;
}

// ============== COMPLETION DETECTION ==============
std::vector<Insight> DiscoveryEngine::find_completions() {
    std::vector<Insight> results;
    report_progress("Finding completions", 0, 100);

    auto all_edges = graph_.get_all_edges();

    std::map<std::pair<std::string, std::string>, std::vector<std::string>> pair_edges;

    for (const auto& edge : all_edges) {
        std::vector<std::string> edge_nodes;
        for (const auto& s : edge.sources) edge_nodes.push_back(s);
        for (const auto& t : edge.targets) edge_nodes.push_back(t);

        for (size_t i = 0; i < edge_nodes.size(); ++i) {
            for (size_t j = i + 1; j < edge_nodes.size(); ++j) {
                std::string a = edge_nodes[i] < edge_nodes[j] ? edge_nodes[i] : edge_nodes[j];
                std::string b = edge_nodes[i] < edge_nodes[j] ? edge_nodes[j] : edge_nodes[i];
                pair_edges[{a, b}].push_back(edge.id);
            }
        }
    }

    report_progress("Finding completions", 60, 100);

    for (auto& [pair, edges] : pair_edges) {
        std::sort(edges.begin(), edges.end());
        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    }

    std::vector<std::tuple<std::string, std::string, std::set<std::string>, std::vector<std::string>>> candidates;

    for (const auto& [pair, edges] : pair_edges) {
        if (edges.size() < static_cast<size_t>(config_.completion_min_shared_edges)) continue;

        std::set<std::string> third_entities;
        for (const auto& eid : edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& n : edge->sources) {
                if (n != pair.first && n != pair.second) third_entities.insert(n);
            }
            for (const auto& n : edge->targets) {
                if (n != pair.first && n != pair.second) third_entities.insert(n);
            }
        }

        if (third_entities.size() >= 2) {
            candidates.emplace_back(pair.first, pair.second, third_entities, edges);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) {
            return std::get<2>(a).size() > std::get<2>(b).size();
        });

    for (size_t i = 0; i < std::min(candidates.size(), config_.completion_max_candidates); ++i) {
        const auto& [n1, n2, third, edges] = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::COMPLETION);
        ins.type = InsightType::COMPLETION;
        ins.seed_nodes = {n1, n2};
        ins.seed_labels = {get_node_label(n1), get_node_label(n2)};

        ins.witness_nodes = {n1, n2};
        int count = 0;
        for (const auto& t : third) {
            ins.witness_nodes.push_back(t);
            if (++count >= 5) break;
        }

        ins.witness_edges = edges;
        ins.evidence_chunk_ids = get_chunk_ids(edges);
        ins.novelty_tags = {"slot_filling"};
        ins.description = "Completion pattern: " + get_node_label(n1) + " + " +
                         get_node_label(n2) + " with " + std::to_string(third.size()) +
                         " potential slot fillers";

        ins.score_breakdown["support"] = static_cast<double>(edges.size());
        ins.score_breakdown["novelty"] = static_cast<double>(third.size());
        ins.score_breakdown["specificity"] = 1.0 / (1.0 + edges.size());
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Finding completions", 100, 100);
    return results;
}

// ============== MOTIF MINING ==============
std::vector<Insight> DiscoveryEngine::find_motifs() {
    std::vector<Insight> results;
    report_progress("Finding motifs", 0, 100);

    auto all_edges = graph_.get_all_edges();

    std::map<std::set<std::string>, int> pattern_counts;

    for (const auto& edge : all_edges) {
        std::set<std::string> entities;
        for (const auto& src : edge.sources) entities.insert(src);
        for (const auto& tgt : edge.targets) entities.insert(tgt);

        std::vector<std::string> ent_vec(entities.begin(), entities.end());
        for (size_t i = 0; i < ent_vec.size() && i < 10; ++i) {
            for (size_t j = i + 1; j < ent_vec.size() && j < 10; ++j) {
                pattern_counts[{ent_vec[i], ent_vec[j]}]++;
            }
        }
    }

    report_progress("Finding motifs", 40, 100);

    std::vector<std::pair<std::set<std::string>, int>> frequent_pairs;
    for (const auto& [pattern, count] : pattern_counts) {
        if (count >= config_.motif_min_support) {
            frequent_pairs.emplace_back(pattern, count);
        }
    }

    report_progress("Finding motifs", 70, 100);

    double total_edges = static_cast<double>(graph_.num_edges());
    std::vector<std::tuple<std::set<std::string>, int, double>> motif_candidates;

    for (const auto& [pattern, count] : frequent_pairs) {
        std::vector<std::string> nodes(pattern.begin(), pattern.end());
        double expected = 1.0;
        for (const auto& n : nodes) {
            const auto* node = graph_.get_node(n);
            if (node) {
                expected *= node->degree / total_edges;
            }
        }
        expected *= total_edges;
        double lift = (expected > 0) ? count / expected : 0;

        if (lift >= config_.motif_min_lift) {
            motif_candidates.emplace_back(pattern, count, lift);
        }
    }

    std::sort(motif_candidates.begin(), motif_candidates.end(),
        [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

    for (size_t i = 0; i < std::min(motif_candidates.size(), config_.motif_max_candidates); ++i) {
        const auto& [pattern, count, lift] = motif_candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::MOTIF);
        ins.type = InsightType::MOTIF;
        ins.seed_nodes = std::vector<std::string>(pattern.begin(), pattern.end());
        for (const auto& n : ins.seed_nodes) {
            ins.seed_labels.push_back(get_node_label(n));
        }

        ins.witness_nodes = ins.seed_nodes;
        ins.novelty_tags = {"frequent_cooccurrence", "high_lift"};

        std::string labels_str;
        for (size_t j = 0; j < ins.seed_labels.size(); ++j) {
            if (j > 0) labels_str += ", ";
            labels_str += ins.seed_labels[j];
        }
        ins.description = "Motif: {" + labels_str + "} with support=" +
                         std::to_string(count) + ", lift=" +
                         std::to_string(static_cast<int>(lift * 100) / 100.0);

        ins.score_breakdown["support"] = static_cast<double>(count);
        ins.score_breakdown["novelty"] = lift;
        ins.score_breakdown["specificity"] = 1.0 / pattern.size();
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Finding motifs", 100, 100);
    return results;
}

// ============== SUBSTITUTION DETECTION ==============
std::vector<Insight> DiscoveryEngine::find_substitutions() {
    std::vector<Insight> results;
    report_progress("Finding substitutions", 0, 100);

    auto all_edges = graph_.get_all_edges();

    std::vector<std::tuple<std::string, std::string, std::string, std::string, double>> candidates;

    report_progress("Finding substitutions", 20, 100);

    size_t sample_limit = std::min(size_t(1000), all_edges.size());
    std::vector<HyperEdge> sampled_edges;
    if (all_edges.size() > sample_limit) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::sample(all_edges.begin(), all_edges.end(),
                   std::back_inserter(sampled_edges), sample_limit, gen);
    } else {
        sampled_edges = all_edges;
    }

    for (size_t i = 0; i < sampled_edges.size(); ++i) {
        if (i % 100 == 0) {
            report_progress("Finding substitutions", 20 + (60 * i / sampled_edges.size()), 100);
        }

        const auto& e1 = sampled_edges[i];
        std::set<std::string> e1_nodes;
        for (const auto& n : e1.sources) e1_nodes.insert(n);
        for (const auto& n : e1.targets) e1_nodes.insert(n);

        for (size_t j = i + 1; j < sampled_edges.size(); ++j) {
            const auto& e2 = sampled_edges[j];

            if (e1.relation != e2.relation) continue;

            std::set<std::string> e2_nodes;
            for (const auto& n : e2.sources) e2_nodes.insert(n);
            for (const auto& n : e2.targets) e2_nodes.insert(n);

            std::set<std::string> intersection, union_set;
            std::set_intersection(e1_nodes.begin(), e1_nodes.end(),
                                 e2_nodes.begin(), e2_nodes.end(),
                                 std::inserter(intersection, intersection.begin()));
            std::set_union(e1_nodes.begin(), e1_nodes.end(),
                          e2_nodes.begin(), e2_nodes.end(),
                          std::inserter(union_set, union_set.begin()));

            double similarity = union_set.empty() ? 0 :
                static_cast<double>(intersection.size()) / union_set.size();

            if (intersection.size() == e1_nodes.size() - 1 &&
                intersection.size() == e2_nodes.size() - 1 &&
                similarity >= config_.substitution_similarity_threshold) {

                std::set<std::string> diff1, diff2;
                std::set_difference(e1_nodes.begin(), e1_nodes.end(),
                                   intersection.begin(), intersection.end(),
                                   std::inserter(diff1, diff1.begin()));
                std::set_difference(e2_nodes.begin(), e2_nodes.end(),
                                   intersection.begin(), intersection.end(),
                                   std::inserter(diff2, diff2.begin()));

                if (diff1.size() == 1 && diff2.size() == 1) {
                    candidates.emplace_back(
                        e1.id, e2.id,
                        *diff1.begin(), *diff2.begin(), similarity);
                }
            }
        }
    }

    report_progress("Finding substitutions", 80, 100);

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return std::get<4>(a) > std::get<4>(b); });

    for (size_t i = 0; i < std::min(candidates.size(), config_.substitution_max_candidates); ++i) {
        const auto& [e1, e2, from, to, sim] = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::SUBSTITUTION);
        ins.type = InsightType::SUBSTITUTION;
        ins.seed_nodes = {from, to};
        ins.seed_labels = {get_node_label(from), get_node_label(to)};
        ins.witness_edges = {e1, e2};

        const auto* edge1 = graph_.get_hyperedge(e1);
        if (edge1) {
            for (const auto& n : edge1->sources) {
                if (n != from) ins.witness_nodes.push_back(n);
            }
            for (const auto& n : edge1->targets) {
                if (n != from) ins.witness_nodes.push_back(n);
            }
        }
        ins.witness_nodes.push_back(from);
        ins.witness_nodes.push_back(to);

        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"entity_substitution"};
        ins.description = "Substitution: " + get_node_label(from) + " <-> " +
                         get_node_label(to) + " in relation '" +
                         (edge1 ? edge1->relation : "") + "'";

        ins.score_breakdown["support"] = 2;
        ins.score_breakdown["novelty"] = 1.0 - sim;
        ins.score_breakdown["specificity"] = sim;
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Finding substitutions", 100, 100);
    return results;
}

// ============== CONTRADICTION DETECTION ==============
std::vector<Insight> DiscoveryEngine::find_contradictions() {
    std::vector<Insight> results;
    report_progress("Finding contradictions", 0, 100);

    struct ContradictionGroup {
        std::string base_relation;
        std::string pos_relation;
        std::string neg_relation;
        std::vector<std::string> pos_edges;
        std::vector<std::string> neg_edges;
    };

    std::unordered_map<std::string, ContradictionGroup> groups;
    auto all_edges = graph_.get_all_edges();

    for (const auto& edge : all_edges) {
        bool is_negated = false;
        std::string base = normalize_relation_base(edge.relation, &is_negated);
        if (base.empty()) continue;

        std::string key = base + "|" + edge_signature(edge);
        auto& group = groups[key];
        group.base_relation = base;
        if (is_negated) {
            if (group.neg_relation.empty()) group.neg_relation = edge.relation;
            group.neg_edges.push_back(edge.id);
        } else {
            if (group.pos_relation.empty()) group.pos_relation = edge.relation;
            group.pos_edges.push_back(edge.id);
        }
    }

    report_progress("Finding contradictions", 50, 100);

    struct CandidateGroup {
        std::string key;
        size_t total_edges = 0;
    };
    std::vector<CandidateGroup> candidates;
    candidates.reserve(groups.size());
    for (const auto& [key, group] : groups) {
        if (!group.pos_edges.empty() && !group.neg_edges.empty()) {
            candidates.push_back({key, group.pos_edges.size() + group.neg_edges.size()});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const CandidateGroup& a, const CandidateGroup& b) { return a.total_edges > b.total_edges; });

    size_t limit = std::min(config_.contradiction_max_candidates, candidates.size());
    for (size_t i = 0; i < limit; ++i) {
        const auto& group = groups[candidates[i].key];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CONTRADICTION);
        ins.type = InsightType::CONTRADICTION;

        std::vector<std::string> witness_edges;
        size_t max_evidence = std::max<size_t>(2, config_.contradiction_max_evidence_edges);
        for (const auto& eid : group.pos_edges) {
            if (witness_edges.size() >= max_evidence) break;
            witness_edges.push_back(eid);
        }
        for (const auto& eid : group.neg_edges) {
            if (witness_edges.size() >= max_evidence) break;
            witness_edges.push_back(eid);
        }
        ins.witness_edges = witness_edges;

        std::unordered_set<std::string> node_set;
        for (const auto& eid : ins.witness_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& n : edge->sources) node_set.insert(n);
            for (const auto& n : edge->targets) node_set.insert(n);
        }
        ins.seed_nodes.assign(node_set.begin(), node_set.end());
        for (const auto& nid : ins.seed_nodes) {
            ins.seed_labels.push_back(get_node_label(nid));
        }
        ins.witness_nodes = ins.seed_nodes;

        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"contradiction", "relation=" + group.base_relation};

        std::string pos_rel = group.pos_relation.empty() ? "affirmed" : group.pos_relation;
        std::string neg_rel = group.neg_relation.empty() ? "negated" : group.neg_relation;
        ins.description = "Contradiction: relation '" + group.base_relation +
            "' appears both affirmed ('" + pos_rel + "') and negated ('" + neg_rel + "').";

        double balance = 1.0;
        if (!group.pos_edges.empty() && !group.neg_edges.empty()) {
            double min_side = static_cast<double>(std::min(group.pos_edges.size(), group.neg_edges.size()));
            double max_side = static_cast<double>(std::max(group.pos_edges.size(), group.neg_edges.size()));
            balance = max_side > 0.0 ? min_side / max_side : 1.0;
        }
        ins.score_breakdown["support"] = static_cast<double>(group.pos_edges.size() + group.neg_edges.size());
        ins.score_breakdown["novelty"] = 1.0 + balance;
        ins.score_breakdown["specificity"] = ins.seed_nodes.empty() ? 0.0 : 1.0 / ins.seed_nodes.size();
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Finding contradictions", 100, 100);
    return results;
}

// ============== ENTITY RESOLUTION ==============
std::vector<Insight> DiscoveryEngine::find_entity_resolutions() {
    std::vector<Insight> results;
    report_progress("Finding entity resolutions", 0, 100);

    struct NodeInfo {
        std::string id;
        std::string label;
        std::string norm;
        std::vector<std::string> tokens;
    };

    auto all_nodes = graph_.get_all_nodes();
    std::vector<NodeInfo> nodes;
    nodes.reserve(all_nodes.size());

    std::unordered_map<std::string, std::vector<size_t>> norm_to_indices;
    std::unordered_map<std::string, std::vector<size_t>> token_to_indices;

    for (size_t i = 0; i < all_nodes.size(); ++i) {
        const auto& node = all_nodes[i];
        NodeInfo info;
        info.id = node.id;
        info.label = node.label;
        info.norm = normalize_label_key(node.label);
        info.tokens = tokenize_simple(node.label);
        nodes.push_back(info);

        if (!info.norm.empty()) {
            norm_to_indices[info.norm].push_back(i);
        }
        for (const auto& token : info.tokens) {
            token_to_indices[token].push_back(i);
        }
    }

    struct Candidate {
        size_t a;
        size_t b;
        double label_similarity;
        double neighbor_overlap;
        double score;
    };

    std::unordered_set<std::string> seen_pairs;
    std::vector<Candidate> candidates;

    auto make_pair_key = [](const std::string& a, const std::string& b) {
        return a < b ? a + "|" + b : b + "|" + a;
    };

    auto add_candidate = [&](size_t i, size_t j, double label_sim, double neighbor_overlap) {
        if (i == j) return;
        const auto& id_a = nodes[i].id;
        const auto& id_b = nodes[j].id;
        std::string key = make_pair_key(id_a, id_b);
        if (seen_pairs.find(key) != seen_pairs.end()) return;
        seen_pairs.insert(key);

        double score = 0.7 * label_sim + 0.3 * neighbor_overlap;
        candidates.push_back({i, j, label_sim, neighbor_overlap, score});
    };

    // Exact normalized label matches
    for (const auto& [norm, idxs] : norm_to_indices) {
        if (idxs.size() < 2) continue;
        for (size_t i = 1; i < idxs.size(); ++i) {
            add_candidate(idxs[0], idxs[i], 1.0, 0.0);
            if (candidates.size() >= config_.entity_resolution_max_candidates * 2) break;
        }
        if (candidates.size() >= config_.entity_resolution_max_candidates * 2) break;
    }

    report_progress("Finding entity resolutions", 35, 100);

    // Fuzzy matches based on token overlap and neighbor similarity
    std::unordered_map<std::string, std::unordered_set<std::string>> neighbor_cache;
    std::unordered_set<std::string> neighbor_cache_ready;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& info = nodes[i];
        if (info.tokens.empty()) continue;

        std::unordered_set<size_t> candidate_idxs;
        for (const auto& token : info.tokens) {
            const auto it = token_to_indices.find(token);
            if (it == token_to_indices.end()) continue;
            for (auto idx : it->second) {
                if (idx != i) candidate_idxs.insert(idx);
            }
        }

        size_t checked = 0;
        for (auto j : candidate_idxs) {
            if (j <= i) continue;
            if (++checked > 400) break;

            double label_sim = token_jaccard(info.tokens, nodes[j].tokens);
            if (label_sim < config_.entity_resolution_min_label_similarity) {
                // Check for strong prefix match
                const std::string& na = info.norm;
                const std::string& nb = nodes[j].norm;
                if (!na.empty() && !nb.empty()) {
                    const std::string& shorter = na.size() < nb.size() ? na : nb;
                    const std::string& longer = na.size() < nb.size() ? nb : na;
                    if (longer.rfind(shorter, 0) == 0) {
                        double ratio = shorter.size() / static_cast<double>(longer.size());
                        label_sim = std::max(label_sim, ratio);
                    }
                }
            }

            if (label_sim < config_.entity_resolution_min_label_similarity) continue;

            if (neighbor_cache_ready.find(info.id) == neighbor_cache_ready.end()) {
                neighbor_cache[info.id] = collect_neighbors(graph_, info.id, config_.entity_resolution_neighbor_limit);
                neighbor_cache_ready.insert(info.id);
            }
            if (neighbor_cache_ready.find(nodes[j].id) == neighbor_cache_ready.end()) {
                neighbor_cache[nodes[j].id] = collect_neighbors(graph_, nodes[j].id, config_.entity_resolution_neighbor_limit);
                neighbor_cache_ready.insert(nodes[j].id);
            }
            const auto& neighbors_a = neighbor_cache[info.id];
            const auto& neighbors_b = neighbor_cache[nodes[j].id];

            double neighbor_overlap = jaccard_overlap(neighbors_a, neighbors_b);
            if (neighbor_overlap < config_.entity_resolution_min_neighbor_overlap &&
                label_sim < 0.95) {
                continue;
            }

            add_candidate(i, j, label_sim, neighbor_overlap);
        }
    }

    report_progress("Finding entity resolutions", 70, 100);

    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    size_t limit = std::min(config_.entity_resolution_max_candidates, candidates.size());
    for (size_t i = 0; i < limit; ++i) {
        const auto& c = candidates[i];
        const auto& node_a = nodes[c.a];
        const auto& node_b = nodes[c.b];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::ENTITY_RESOLUTION);
        ins.type = InsightType::ENTITY_RESOLUTION;
        ins.seed_nodes = {node_a.id, node_b.id};
        ins.seed_labels = {node_a.label, node_b.label};

        std::unordered_set<std::string> edge_set_a;
        std::unordered_set<std::string> edge_set_b;

        if (const auto* na = graph_.get_node(node_a.id)) {
            edge_set_a.insert(na->incident_edges.begin(), na->incident_edges.end());
        }
        if (const auto* nb = graph_.get_node(node_b.id)) {
            edge_set_b.insert(nb->incident_edges.begin(), nb->incident_edges.end());
        }

        std::vector<std::string> witness_edges;
        for (const auto& eid : edge_set_a) {
            if (edge_set_b.find(eid) != edge_set_b.end()) {
                witness_edges.push_back(eid);
                if (witness_edges.size() >= 6) break;
            }
        }

        if (witness_edges.empty()) {
            size_t added = 0;
            for (const auto& eid : edge_set_a) {
                witness_edges.push_back(eid);
                if (++added >= 3) break;
            }
            added = 0;
            for (const auto& eid : edge_set_b) {
                witness_edges.push_back(eid);
                if (++added >= 3) break;
            }
        }

        ins.witness_edges = witness_edges;

        std::unordered_set<std::string> node_set;
        for (const auto& eid : witness_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& n : edge->sources) node_set.insert(n);
            for (const auto& n : edge->targets) node_set.insert(n);
        }
        node_set.insert(node_a.id);
        node_set.insert(node_b.id);
        ins.witness_nodes.assign(node_set.begin(), node_set.end());

        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"entity_resolution", "possible_duplicate"};
        std::stringstream desc;
        desc << "Entity resolution: '" << node_a.label << "' and '" << node_b.label
             << "' are likely duplicates (label similarity=" << std::fixed << std::setprecision(2)
             << c.label_similarity << ", neighbor overlap=" << c.neighbor_overlap << ").";
        ins.description = desc.str();

        ins.score_breakdown["label_similarity"] = c.label_similarity;
        ins.score_breakdown["neighbor_overlap"] = c.neighbor_overlap;
        ins.score = c.score;

        results.push_back(std::move(ins));
    }

    report_progress("Finding entity resolutions", 100, 100);
    return results;
}

// ============== CORE-PERIPHERY / HUB-AUTHORITY ==============
std::vector<Insight> DiscoveryEngine::find_core_periphery() {
    std::vector<Insight> results;
    report_progress("Finding core-periphery", 0, 100);

    struct NodeScore {
        std::string id;
        std::string label;
        int degree = 0;
        int in_degree = 0;
        int out_degree = 0;
        double degree_norm = 0.0;
        double hub_score = 0.0;
        double authority_score = 0.0;
        double integration_score = 0.0;
        double core_score = 0.0;
        double periphery_score = 0.0;
    };

    auto nodes = graph_.get_all_nodes();
    auto edges = graph_.get_all_edges();

    std::unordered_map<std::string, int> in_counts;
    std::unordered_map<std::string, int> out_counts;
    for (const auto& edge : edges) {
        for (const auto& src : edge.sources) {
            out_counts[src]++;
        }
        for (const auto& tgt : edge.targets) {
            in_counts[tgt]++;
        }
    }

    int max_degree = 1;
    int max_in = 1;
    int max_out = 1;
    for (const auto& node : nodes) {
        max_degree = std::max(max_degree, node.degree);
        max_in = std::max(max_in, in_counts[node.id]);
        max_out = std::max(max_out, out_counts[node.id]);
    }

    int hub_set = std::max(1, config_.core_periphery_hub_set_size);
    auto integration_scores = graph_.compute_hub_integration_scores(hub_set);
    int max_integration = 1;
    for (const auto& [id, score] : integration_scores) {
        max_integration = std::max(max_integration, score);
    }

    std::vector<NodeScore> scored;
    scored.reserve(nodes.size());
    for (const auto& node : nodes) {
        NodeScore ns;
        ns.id = node.id;
        ns.label = node.label;
        ns.degree = node.degree;
        ns.in_degree = in_counts[node.id];
        ns.out_degree = out_counts[node.id];
        ns.degree_norm = static_cast<double>(ns.degree) / max_degree;
        ns.hub_score = static_cast<double>(ns.out_degree) / max_out;
        ns.authority_score = static_cast<double>(ns.in_degree) / max_in;
        auto it = integration_scores.find(node.id);
        ns.integration_score = it != integration_scores.end()
            ? static_cast<double>(it->second) / max_integration
            : 0.0;
        double role_strength = std::max(ns.hub_score, ns.authority_score);
        ns.core_score = 0.5 * ns.degree_norm + 0.3 * ns.integration_score + 0.2 * role_strength;
        ns.periphery_score = 0.6 * (1.0 - ns.degree_norm) + 0.4 * (1.0 - role_strength);
        scored.push_back(ns);
    }

    report_progress("Finding core-periphery", 50, 100);

    std::vector<NodeScore> core_sorted = scored;
    std::sort(core_sorted.begin(), core_sorted.end(),
              [](const NodeScore& a, const NodeScore& b) { return a.core_score > b.core_score; });

    std::vector<NodeScore> periphery_sorted = scored;
    std::sort(periphery_sorted.begin(), periphery_sorted.end(),
              [](const NodeScore& a, const NodeScore& b) { return a.periphery_score > b.periphery_score; });

    size_t top_k = std::min(config_.core_periphery_top_k, core_sorted.size());
    size_t bottom_k = std::min(config_.core_periphery_bottom_k, periphery_sorted.size());

    auto build_insight = [&](const NodeScore& ns, bool is_core) {
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CORE_PERIPHERY);
        ins.type = InsightType::CORE_PERIPHERY;
        ins.seed_nodes = {ns.id};
        ins.seed_labels = {ns.label.empty() ? ns.id : ns.label};

        const auto* node = graph_.get_node(ns.id);
        if (node) {
            for (const auto& eid : node->incident_edges) {
                if (ins.witness_edges.size() >= config_.core_periphery_max_evidence_edges) break;
                ins.witness_edges.push_back(eid);
            }
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        std::unordered_set<std::string> witness_nodes;
        for (const auto& eid : ins.witness_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& n : edge->sources) witness_nodes.insert(n);
            for (const auto& n : edge->targets) witness_nodes.insert(n);
        }
        witness_nodes.insert(ns.id);
        ins.witness_nodes.assign(witness_nodes.begin(), witness_nodes.end());

        ins.score_breakdown["degree_norm"] = ns.degree_norm;
        ins.score_breakdown["hub_score"] = ns.hub_score;
        ins.score_breakdown["authority_score"] = ns.authority_score;
        ins.score_breakdown["integration_score"] = ns.integration_score;

        if (is_core) {
            ins.score_breakdown["core_score"] = ns.core_score;
            ins.score = ns.core_score;
            if (ns.hub_score >= ns.authority_score) {
                ins.novelty_tags = {"core", "hub"};
            } else {
                ins.novelty_tags = {"core", "authority"};
            }
            std::stringstream desc;
            desc << "Core node: " << (ns.label.empty() ? ns.id : ns.label)
                 << " (degree=" << ns.degree << ", hub=" << std::fixed << std::setprecision(2)
                 << ns.hub_score << ", authority=" << ns.authority_score << ").";
            ins.description = desc.str();
        } else {
            ins.score_breakdown["periphery_score"] = ns.periphery_score;
            ins.score = ns.periphery_score;
            ins.novelty_tags = {"periphery"};
            std::stringstream desc;
            desc << "Periphery node: " << (ns.label.empty() ? ns.id : ns.label)
                 << " (degree=" << ns.degree << ", hub=" << std::fixed << std::setprecision(2)
                 << ns.hub_score << ", authority=" << ns.authority_score << ").";
            ins.description = desc.str();
        }

        return ins;
    };

    for (size_t i = 0; i < top_k; ++i) {
        results.push_back(build_insight(core_sorted[i], true));
    }
    for (size_t i = 0; i < bottom_k; ++i) {
        results.push_back(build_insight(periphery_sorted[i], false));
    }

    report_progress("Finding core-periphery", 100, 100);
    return results;
}

// ============== TEXT SEMANTIC SIMILARITY ==============
std::vector<Insight> DiscoveryEngine::find_text_similarity_links() {
    std::vector<Insight> results;
    report_progress("Finding text similarity links", 0, 100);

    struct NodeVec {
        std::string id;
        std::string label;
        std::unordered_map<std::string, double> weights;
        double norm = 1.0;
    };

    auto nodes = graph_.get_all_nodes();
    if (nodes.size() < 2) return results;

    std::unordered_map<std::string, int> doc_freq;
    std::vector<std::vector<std::string>> tokens_by_node;
    tokens_by_node.reserve(nodes.size());

    for (const auto& node : nodes) {
        auto tokens = tokenize_simple(node.label);
        std::unordered_set<std::string> unique(tokens.begin(), tokens.end());
        for (const auto& token : unique) {
            doc_freq[token]++;
        }
        tokens_by_node.push_back(std::move(tokens));
    }

    std::vector<NodeVec> vectors;
    vectors.reserve(nodes.size());

    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        const auto& tokens = tokens_by_node[i];
        if (tokens.empty()) {
            vectors.push_back({node.id, node.label, {}, 1.0});
            continue;
        }

        std::unordered_map<std::string, double> tf;
        for (const auto& token : tokens) {
            tf[token] += 1.0;
        }

        NodeVec vec;
        vec.id = node.id;
        vec.label = node.label;

        double norm = 0.0;
        for (const auto& [token, count] : tf) {
            auto it = doc_freq.find(token);
            if (it == doc_freq.end()) continue;
            double idf = std::log((nodes.size() + 1.0) / (it->second + 1.0)) + 1.0;
            double weight = (count / tokens.size()) * idf;
            vec.weights[token] = weight;
            norm += weight * weight;
        }
        vec.norm = std::sqrt(norm);
        if (vec.norm <= 0.0) vec.norm = 1.0;
        vectors.push_back(std::move(vec));
    }

    report_progress("Finding text similarity links", 40, 100);

    std::unordered_map<std::string, std::vector<std::pair<size_t, double>>> postings;
    for (size_t i = 0; i < vectors.size(); ++i) {
        for (const auto& [token, weight] : vectors[i].weights) {
            postings[token].push_back({i, weight});
        }
    }

    struct Candidate {
        size_t i;
        size_t j;
        double score;
    };
    std::vector<Candidate> candidates;

    std::unordered_set<std::string> seen_pairs;
    auto make_key = [](size_t a, size_t b) {
        return a < b ? std::to_string(a) + "|" + std::to_string(b)
                     : std::to_string(b) + "|" + std::to_string(a);
    };

    for (size_t i = 0; i < vectors.size(); ++i) {
        std::unordered_map<size_t, double> scores;
        for (const auto& [token, weight] : vectors[i].weights) {
            auto it = postings.find(token);
            if (it == postings.end()) continue;
            for (const auto& [j, w] : it->second) {
                if (i == j) continue;
                scores[j] += weight * w;
            }
        }

        std::vector<std::pair<size_t, double>> scored;
        scored.reserve(scores.size());
        for (const auto& [j, dot] : scores) {
            double denom = vectors[i].norm * vectors[j].norm;
            if (denom <= 0.0) continue;
            double sim = dot / denom;
            if (sim >= config_.text_similarity_min_score) {
                scored.push_back({j, sim});
            }
        }

        std::sort(scored.begin(), scored.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        size_t keep = std::min(config_.text_similarity_top_k_per_node, scored.size());
        for (size_t k = 0; k < keep; ++k) {
            size_t j = scored[k].first;
            double sim = scored[k].second;
            std::string key = make_key(i, j);
            if (seen_pairs.find(key) != seen_pairs.end()) continue;
            seen_pairs.insert(key);
            candidates.push_back({i, j, sim});
        }

        if (candidates.size() >= config_.text_similarity_max_candidates * 2) break;
    }

    report_progress("Finding text similarity links", 75, 100);

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    size_t limit = std::min(config_.text_similarity_max_candidates, candidates.size());
    for (size_t idx = 0; idx < limit; ++idx) {
        const auto& cand = candidates[idx];
        const auto& a = vectors[cand.i];
        const auto& b = vectors[cand.j];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::TEXT_SIMILARITY);
        ins.type = InsightType::TEXT_SIMILARITY;
        ins.seed_nodes = {a.id, b.id};
        ins.seed_labels = {a.label.empty() ? a.id : a.label, b.label.empty() ? b.id : b.label};

        const auto* node_a = graph_.get_node(a.id);
        if (node_a) {
            for (const auto& eid : node_a->incident_edges) {
                if (ins.witness_edges.size() >= 3) break;
                ins.witness_edges.push_back(eid);
            }
        }
        const auto* node_b = graph_.get_node(b.id);
        if (node_b) {
            for (const auto& eid : node_b->incident_edges) {
                if (ins.witness_edges.size() >= 6) break;
                ins.witness_edges.push_back(eid);
            }
        }

        std::unordered_set<std::string> witness_nodes;
        for (const auto& eid : ins.witness_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& n : edge->sources) witness_nodes.insert(n);
            for (const auto& n : edge->targets) witness_nodes.insert(n);
        }
        witness_nodes.insert(a.id);
        witness_nodes.insert(b.id);
        ins.witness_nodes.assign(witness_nodes.begin(), witness_nodes.end());

        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"text_similarity"};
        std::stringstream desc;
        desc << "Text similarity link: '" << ins.seed_labels[0] << "' ~ '" << ins.seed_labels[1]
             << "' (cosine=" << std::fixed << std::setprecision(2) << cand.score << ").";
        ins.description = desc.str();

        ins.score_breakdown["text_similarity"] = cand.score;
        ins.score = cand.score;

        results.push_back(std::move(ins));
    }

    report_progress("Finding text similarity links", 100, 100);
    return results;
}

// ============== ARGUMENT-SUPPORTED RELATIONS ==============
std::vector<Insight> DiscoveryEngine::find_argument_support_relations() {
    std::vector<Insight> results;
    report_progress("Finding argument-supported relations", 0, 100);

    auto hubs = index_.get_top_hubs(25);
    std::vector<std::string> seeds = hubs;
    if (seeds.empty()) {
        for (const auto& node : graph_.get_all_nodes()) {
            seeds.push_back(node.id);
            if (seeds.size() >= 25) break;
        }
    }

    struct Candidate {
        std::string a;
        std::string b;
        std::vector<HyperEdge> path;
    };
    std::vector<Candidate> candidates;

    for (size_t i = 0; i < seeds.size(); ++i) {
        const auto& seed = seeds[i];
        auto neighbors = collect_neighbors(graph_, seed, 80);
        for (const auto& mid : neighbors) {
            auto neighbors2 = collect_neighbors(graph_, mid, 60);
            for (const auto& target : neighbors2) {
                if (seed == target) continue;
                if (nodes_share_edge(graph_, seed, target)) continue;

                auto path = graph_.find_shortest_path(seed, target, 1);
                if (path.empty() || static_cast<int>(path.size()) > config_.argument_support_max_path_length) continue;

                candidates.push_back({seed, target, path});
                if (candidates.size() >= config_.argument_support_max_candidates) break;
            }
            if (candidates.size() >= config_.argument_support_max_candidates) break;
        }
        if (candidates.size() >= config_.argument_support_max_candidates) break;
    }

    report_progress("Finding argument-supported relations", 60, 100);

    for (const auto& cand : candidates) {
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::ARGUMENT_SUPPORT);
        ins.type = InsightType::ARGUMENT_SUPPORT;
        ins.seed_nodes = {cand.a, cand.b};
        ins.seed_labels = {get_node_label(cand.a), get_node_label(cand.b)};

        for (const auto& edge : cand.path) {
            if (ins.witness_edges.size() >= config_.argument_support_max_evidence_edges) break;
            ins.witness_edges.push_back(edge.id);
        }

        std::unordered_set<std::string> witness_nodes;
        for (const auto& edge : cand.path) {
            for (const auto& n : edge.sources) witness_nodes.insert(n);
            for (const auto& n : edge.targets) witness_nodes.insert(n);
        }
        witness_nodes.insert(cand.a);
        witness_nodes.insert(cand.b);
        ins.witness_nodes.assign(witness_nodes.begin(), witness_nodes.end());

        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"argument_support"};

        std::stringstream path_desc;
        path_desc << "Path evidence: ";
        for (size_t i = 0; i < cand.path.size(); ++i) {
            if (i > 0) path_desc << " -> ";
            path_desc << cand.path[i].relation;
        }

        std::string default_desc = "Argument-supported relation between " + ins.seed_labels[0] +
            " and " + ins.seed_labels[1] + ". " + path_desc.str() + ".";

        if (llm_provider_) {
            std::stringstream prompt;
            prompt << "You are a knowledge graph analyst. Propose a plausible relation label between the two entities "
                   << "based only on the evidence path.\n";
            prompt << "Entity A: " << ins.seed_labels[0] << "\n";
            prompt << "Entity B: " << ins.seed_labels[1] << "\n";
            prompt << "Evidence path relations: " << path_desc.str() << "\n";
            prompt << "Intermediate entities: " << join_labels(graph_, ins.witness_nodes, 8) << "\n";
            prompt << "Return format:\nRelation: <short relation label>\nExplanation: <1-2 sentences>\n";

            std::vector<Message> messages = {
                Message(Message::Role::System, "You propose concise relation labels grounded in evidence."),
                Message(Message::Role::User, prompt.str())
            };
            LLMResponse response = llm_provider_->chat(messages);
            if (response.success && !response.content.empty()) {
                ins.description = response.content;
                std::string relation_label;
                std::istringstream iss(response.content);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.rfind("Relation:", 0) == 0) {
                        relation_label = line.substr(std::string("Relation:").size());
                        relation_label = trim_copy(relation_label);
                        break;
                    }
                }
                if (!relation_label.empty()) {
                    ins.llm = nlohmann::json{{"relation_label", relation_label}};
                }
            } else {
                ins.description = default_desc;
            }
        } else {
            ins.description = default_desc;
        }

        ins.score_breakdown["support"] = static_cast<double>(cand.path.size());
        ins.score_breakdown["specificity"] = 1.0 / (1.0 + cand.path.size());
        ins.score = compute_score(ins);
        results.push_back(std::move(ins));
    }

    report_progress("Finding argument-supported relations", 100, 100);
    return results;
}

// ============== ACTIVE LEARNING FOR GRAPHS ==============
std::vector<Insight> DiscoveryEngine::find_active_learning_queries() {
    std::vector<Insight> results;
    report_progress("Finding active learning queries", 0, 100);

    struct QueryCandidate {
        std::string edge_id;
        std::string relation;
        std::vector<std::string> nodes;
        double confidence;
        double impact;
    };

    std::vector<QueryCandidate> candidates;
    auto all_edges = graph_.get_all_edges();

    for (const auto& edge : all_edges) {
        if (edge.confidence >= config_.active_learning_confidence_threshold) continue;
        std::vector<std::string> nodes;
        for (const auto& s : edge.sources) nodes.push_back(s);
        for (const auto& t : edge.targets) nodes.push_back(t);
        double avg_degree = 0.0;
        for (const auto& n : nodes) {
            const auto* node = graph_.get_node(n);
            if (node) avg_degree += node->degree;
        }
        if (!nodes.empty()) avg_degree /= nodes.size();
        candidates.push_back({edge.id, edge.relation, nodes, edge.confidence, avg_degree});
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.confidence != b.confidence) return a.confidence < b.confidence;
        return a.impact > b.impact;
    });

    size_t limit = std::min(config_.active_learning_max_queries, candidates.size());
    for (size_t i = 0; i < limit; ++i) {
        const auto& cand = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::ACTIVE_LEARNING);
        ins.type = InsightType::ACTIVE_LEARNING;
        ins.seed_nodes = cand.nodes;
        ins.seed_labels = {};
        for (const auto& n : cand.nodes) {
            ins.seed_labels.push_back(get_node_label(n));
        }
        ins.witness_edges = {cand.edge_id};
        ins.witness_nodes = cand.nodes;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.score_breakdown["confidence"] = cand.confidence;
        ins.score_breakdown["impact"] = cand.impact;
        ins.score = 1.0 - cand.confidence;
        ins.novelty_tags = {"active_learning"};

        std::string default_desc = "Verify relation '" + cand.relation + "' between " +
            join_labels(graph_, cand.nodes, 4) + ".";

        if (llm_provider_) {
            std::stringstream prompt;
            prompt << "You are helping prioritize graph validation. Create a short verification question.\n";
            prompt << "Relation: " << cand.relation << "\n";
            prompt << "Entities: " << join_labels(graph_, cand.nodes, 6) << "\n";
            prompt << "Return a single concise question.";

            std::vector<Message> messages = {
                Message(Message::Role::System, "Write concise validation questions."),
                Message(Message::Role::User, prompt.str())
            };
            LLMResponse response = llm_provider_->chat(messages);
            if (response.success && !response.content.empty()) {
                ins.description = response.content;
                ins.llm = nlohmann::json{{"query", response.content}};
            } else {
                ins.description = default_desc;
            }
        } else {
            ins.description = default_desc;
        }

        results.push_back(std::move(ins));
    }

    report_progress("Finding active learning queries", 100, 100);
    return results;
}

// ============== METHOD/OUTCOME NODE DISCOVERY ==============
std::vector<Insight> DiscoveryEngine::find_method_outcome_nodes() {
    std::vector<Insight> results;
    report_progress("Finding method/outcome nodes", 0, 100);

    const std::unordered_set<std::string> method_terms = {
        "method", "methods", "approach", "approaches", "algorithm", "algorithms", "procedure",
        "technique", "model", "framework", "pipeline", "strategy"
    };
    const std::unordered_set<std::string> outcome_terms = {
        "result", "results", "outcome", "outcomes", "effect", "effects", "impact",
        "performance", "accuracy", "improvement", "gain", "increase", "decrease"
    };

    struct Candidate {
        std::string id;
        std::string label;
        bool method_hint;
        bool outcome_hint;
        int degree;
    };

    std::vector<Candidate> candidates;
    for (const auto& node : graph_.get_all_nodes()) {
        std::string lower = to_lower_copy(node.label);
        bool method_hint = false;
        bool outcome_hint = false;
        for (const auto& term : method_terms) {
            if (lower.find(term) != std::string::npos) {
                method_hint = true;
                break;
            }
        }
        for (const auto& term : outcome_terms) {
            if (lower.find(term) != std::string::npos) {
                outcome_hint = true;
                break;
            }
        }
        if (!method_hint && !outcome_hint) continue;
        candidates.push_back({node.id, node.label, method_hint, outcome_hint, node.degree});
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.degree > b.degree;
    });

    size_t limit = std::min(config_.method_outcome_max_candidates, candidates.size());
    for (size_t i = 0; i < limit; ++i) {
        const auto& cand = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::METHOD_OUTCOME);
        ins.type = InsightType::METHOD_OUTCOME;
        ins.seed_nodes = {cand.id};
        ins.seed_labels = {cand.label};

        const auto* node = graph_.get_node(cand.id);
        if (node) {
            for (const auto& eid : node->incident_edges) {
                if (ins.witness_edges.size() >= 6) break;
                ins.witness_edges.push_back(eid);
            }
        }
        ins.witness_nodes = ins.seed_nodes;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        std::string default_role = cand.method_hint ? "method" : "outcome";
        std::string default_desc = "Entity '" + cand.label + "' appears to be a " + default_role + " in the domain.";

        if (llm_provider_) {
            std::stringstream context;
            context << "Entity: " << cand.label << "\n";
            context << "Incident relations: ";
            int added = 0;
            if (node) {
                for (const auto& eid : node->incident_edges) {
                    const auto* edge = graph_.get_hyperedge(eid);
                    if (!edge) continue;
                    if (added++ > 0) context << ", ";
                    context << edge->relation;
                    if (added >= 6) break;
                }
            }
            context << "\n";
            context << "Classify as one of: method, outcome, neither. Return format:\n";
            context << "Label: <method|outcome|neither>\nReason: <1 sentence>\n";

            std::vector<Message> messages = {
                Message(Message::Role::System, "You are a precise classifier of entity roles."),
                Message(Message::Role::User, context.str())
            };
            LLMResponse response = llm_provider_->chat(messages);
            if (response.success && !response.content.empty()) {
                ins.description = response.content;
                std::string role = default_role;
                std::istringstream iss(response.content);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.rfind("Label:", 0) == 0) {
                        role = trim_copy(line.substr(std::string("Label:").size()));
                        break;
                    }
                }
                if (role == "method") {
                    ins.novelty_tags = {"method"};
                    ins.score = 0.8;
                } else if (role == "outcome") {
                    ins.novelty_tags = {"outcome"};
                    ins.score = 0.8;
                } else {
                    continue;
                }
                ins.llm = nlohmann::json{{"role", role}};
            } else {
                ins.description = default_desc;
                ins.novelty_tags = {default_role};
                ins.score = 0.6;
            }
        } else {
            ins.description = default_desc;
            ins.novelty_tags = {default_role};
            ins.score = 0.6;
        }

        results.push_back(std::move(ins));
    }

    report_progress("Finding method/outcome nodes", 100, 100);
    return results;
}

// ============== CENTRALITY (BIPARTITE PAGERANK) ==============
std::vector<Insight> DiscoveryEngine::find_centrality_nodes() {
    std::vector<Insight> results;
    report_progress("Centrality analysis", 0, 100);

    auto bi = build_bipartite_graph(graph_);
    size_t total_nodes = bi.adj.size();
    if (total_nodes == 0 || bi.num_entities == 0) {
        report_progress("Centrality analysis", 100, 100);
        return results;
    }

    std::vector<double> pr(total_nodes, 1.0 / static_cast<double>(total_nodes));
    std::vector<size_t> deg(total_nodes, 0);
    for (size_t i = 0; i < total_nodes; ++i) {
        deg[i] = bi.adj[i].size();
    }

    double damping = config_.centrality_damping;
    int iterations = config_.centrality_iterations;
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<double> next(total_nodes, (1.0 - damping) / static_cast<double>(total_nodes));
        double dangling = 0.0;
        for (size_t i = 0; i < total_nodes; ++i) {
            if (deg[i] == 0) dangling += pr[i];
        }
        double dangling_contrib = damping * dangling / static_cast<double>(total_nodes);

        for (size_t i = 0; i < total_nodes; ++i) {
            if (deg[i] == 0) continue;
            double share = damping * pr[i] / static_cast<double>(deg[i]);
            for (size_t nbr : bi.adj[i]) {
                next[nbr] += share;
            }
        }

        for (size_t i = 0; i < total_nodes; ++i) {
            next[i] += dangling_contrib;
        }

        pr.swap(next);
        if ((iter + 1) % 5 == 0) {
            int pct = static_cast<int>(80.0 * (iter + 1) / std::max(1, iterations));
            report_progress("Centrality analysis", pct, 100);
        }
    }

    std::vector<std::pair<std::string, double>> ranked;
    ranked.reserve(bi.num_entities);
    double max_score = 0.0;
    for (size_t i = 0; i < bi.num_entities; ++i) {
        double score = pr[i];
        max_score = std::max(max_score, score);
        ranked.emplace_back(bi.entity_ids[i], score);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    size_t limit = std::min(ranked.size(), config_.centrality_top_k);
    for (size_t i = 0; i < limit; ++i) {
        const auto& [node_id, raw_score] = ranked[i];
        double score = (max_score > 0.0) ? (raw_score / max_score) : raw_score;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CENTRALITY);
        ins.type = InsightType::CENTRALITY;
        ins.seed_nodes = {node_id};
        ins.seed_labels = {get_node_label(node_id)};

        auto incident = graph_.get_incident_edges(node_id);
        for (size_t j = 0; j < incident.size() && j < config_.centrality_max_evidence_edges; ++j) {
            ins.witness_edges.push_back(incident[j].id);
        }

        std::unordered_set<std::string> witness_nodes;
        witness_nodes.insert(node_id);
        for (const auto& eid : ins.witness_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& nid : edge->get_all_nodes()) {
                witness_nodes.insert(nid);
            }
        }
        ins.witness_nodes.assign(witness_nodes.begin(), witness_nodes.end());
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        ins.score_breakdown["centrality"] = score;
        ins.score = score;
        ins.description = "High centrality node in bipartite PageRank: " + get_node_label(node_id);
        ins.novelty_tags = {"centrality", "bipartite_pagerank"};

        results.push_back(std::move(ins));
    }

    report_progress("Centrality analysis", 100, 100);
    return results;
}

// ============== COMMUNITY DETECTION (LOUVAIN) ==============
std::vector<Insight> DiscoveryEngine::find_community_structures() {
    std::vector<Insight> results;
    report_progress("Community detection", 0, 100);

    ProjectionGraph proj = build_projection_graph(graph_);
    size_t n = proj.node_ids.size();
    if (n == 0) {
        report_progress("Community detection", 100, 100);
        return results;
    }

    std::vector<int> community(n);
    std::iota(community.begin(), community.end(), 0);

    std::vector<double> k(n, 0.0);
    double m2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        for (const auto& [_, w] : proj.adj[i]) sum += w;
        k[i] = sum;
        m2 += sum;
    }
    if (m2 == 0.0) {
        report_progress("Community detection", 100, 100);
        return results;
    }

    std::vector<double> tot(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        tot[community[i]] += k[i];
    }

    auto neigh_comm_weights = [&](size_t i) {
        std::unordered_map<int, double> map;
        for (const auto& [j, w] : proj.adj[i]) {
            int cj = community[j];
            map[cj] += w;
        }
        return map;
    };

    auto modularity_gain = [&](size_t i, double ki_in, double totc) {
        return (config_.community_detection_resolution * (ki_in / m2)) -
               ((k[i] * totc) / (m2 * m2));
    };

    bool improved = true;
    int passes = 0;
    while (improved && passes < 15) {
        improved = false;
        passes++;

        for (size_t i = 0; i < n; ++i) {
            int ci = community[i];
            auto neigh = neigh_comm_weights(i);
            tot[ci] -= k[i];

            int best_c = ci;
            double best_gain = 0.0;
            for (const auto& [c, ki_in] : neigh) {
                double gain = modularity_gain(i, ki_in, tot[c]);
                if (gain > best_gain) {
                    best_gain = gain;
                    best_c = c;
                }
            }

            if (best_c != ci) {
                community[i] = best_c;
                improved = true;
            }
            tot[community[i]] += k[i];
        }
    }

    std::unordered_map<int, int> remap;
    int next_id = 0;
    for (size_t i = 0; i < n; ++i) {
        int c = community[i];
        if (remap.find(c) == remap.end()) remap[c] = next_id++;
        community[i] = remap[c];
    }

    std::unordered_map<int, std::vector<size_t>> groups;
    for (size_t i = 0; i < n; ++i) {
        groups[community[i]].push_back(i);
    }

    std::vector<std::pair<int, std::vector<size_t>>> group_list;
    for (auto& [cid, nodes] : groups) {
        if (nodes.size() < config_.community_detection_min_size) continue;
        group_list.emplace_back(cid, std::move(nodes));
    }
    std::sort(group_list.begin(), group_list.end(),
              [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });

    size_t max_size = 1;
    if (!group_list.empty()) max_size = group_list.front().second.size();

    size_t limit = std::min(group_list.size(), config_.community_detection_max_communities);
    for (size_t i = 0; i < limit; ++i) {
        const auto& nodes = group_list[i].second;
        std::vector<std::pair<size_t, double>> ranked;
        ranked.reserve(nodes.size());
        for (auto idx : nodes) {
            ranked.emplace_back(idx, k[idx]);
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::COMMUNITY_DETECTION);
        ins.type = InsightType::COMMUNITY_DETECTION;

        size_t seed_count = std::min<size_t>(3, ranked.size());
        for (size_t j = 0; j < seed_count; ++j) {
            const auto& nid = proj.node_ids[ranked[j].first];
            ins.seed_nodes.push_back(nid);
            ins.seed_labels.push_back(get_node_label(nid));
        }

        for (auto idx : nodes) {
            ins.witness_nodes.push_back(proj.node_ids[idx]);
            if (ins.witness_nodes.size() >= 100) break;
        }

        std::unordered_set<std::string> witness_edges;
        for (const auto& seed : ins.seed_nodes) {
            auto incident = graph_.get_incident_edges(seed);
            for (const auto& edge : incident) {
                witness_edges.insert(edge.id);
                if (witness_edges.size() >= config_.community_detection_max_evidence_edges) break;
            }
            if (witness_edges.size() >= config_.community_detection_max_evidence_edges) break;
        }
        ins.witness_edges.assign(witness_edges.begin(), witness_edges.end());
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        double size_score = static_cast<double>(nodes.size()) / static_cast<double>(max_size);
        ins.score_breakdown["size"] = size_score;
        ins.score = size_score;
        ins.novelty_tags = {"community_detection", "size=" + std::to_string(nodes.size())};

        ins.description = "Community cluster (" + std::to_string(nodes.size()) + " nodes): " +
                          join_labels(graph_, ins.seed_nodes, 3);

        results.push_back(std::move(ins));
    }

    report_progress("Community detection", 100, 100);
    return results;
}

// ============== K-CORE ANALYSIS ==============
std::vector<Insight> DiscoveryEngine::find_k_core_nodes() {
    std::vector<Insight> results;
    report_progress("k-core analysis", 0, 100);

    ProjectionGraph proj = build_projection_graph(graph_);
    size_t n = proj.node_ids.size();
    if (n == 0) {
        report_progress("k-core analysis", 100, 100);
        return results;
    }

    std::vector<int> degree(n, 0);
    for (size_t i = 0; i < n; ++i) degree[i] = static_cast<int>(proj.adj[i].size());

    std::vector<int> core(n, 0);
    std::vector<bool> removed(n, false);
    using Item = std::pair<int, size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    for (size_t i = 0; i < n; ++i) pq.push({degree[i], i});

    while (!pq.empty()) {
        auto [deg, u] = pq.top();
        pq.pop();
        if (removed[u] || deg != degree[u]) continue;
        removed[u] = true;
        core[u] = deg;
        for (const auto& [v, _] : proj.adj[u]) {
            if (removed[v]) continue;
            degree[v] = std::max(0, degree[v] - 1);
            pq.push({degree[v], v});
        }
    }

    int max_core = 0;
    std::vector<std::pair<size_t, int>> ranked;
    for (size_t i = 0; i < n; ++i) {
        if (core[i] >= config_.k_core_min_k) {
            ranked.emplace_back(i, core[i]);
            max_core = std::max(max_core, core[i]);
        }
    }
    std::sort(ranked.begin(), ranked.end(),
              [&](const auto& a, const auto& b) {
                  if (a.second != b.second) return a.second > b.second;
                  return proj.adj[a.first].size() > proj.adj[b.first].size();
              });

    size_t limit = std::min(ranked.size(), config_.k_core_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        size_t idx = ranked[i].first;
        int kcore = ranked[i].second;
        const auto& node_id = proj.node_ids[idx];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::K_CORE);
        ins.type = InsightType::K_CORE;
        ins.seed_nodes = {node_id};
        ins.seed_labels = {get_node_label(node_id)};

        auto incident = graph_.get_incident_edges(node_id);
        for (size_t j = 0; j < incident.size() && j < config_.centrality_max_evidence_edges; ++j) {
            ins.witness_edges.push_back(incident[j].id);
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        for (const auto& [nbr, _] : proj.adj[idx]) {
            ins.witness_nodes.push_back(proj.node_ids[nbr]);
            if (ins.witness_nodes.size() >= 10) break;
        }
        ins.witness_nodes.insert(ins.witness_nodes.begin(), node_id);

        double score = (max_core > 0) ? (static_cast<double>(kcore) / max_core) : 0.0;
        ins.score_breakdown["core"] = static_cast<double>(kcore);
        ins.score = score;
        ins.novelty_tags = {"k_core", "k=" + std::to_string(kcore)};
        ins.description = "Node " + get_node_label(node_id) +
                          " is in k-core (k=" + std::to_string(kcore) + ")";

        results.push_back(std::move(ins));
    }

    report_progress("k-core analysis", 100, 100);
    return results;
}

// ============== K-TRUSS ANALYSIS ==============
std::vector<Insight> DiscoveryEngine::find_k_truss_edges() {
    std::vector<Insight> results;
    report_progress("k-truss analysis", 0, 100);

    ProjectionGraph proj = build_projection_graph(graph_);
    size_t n = proj.node_ids.size();
    if (n == 0) {
        report_progress("k-truss analysis", 100, 100);
        return results;
    }

    std::vector<std::unordered_set<size_t>> nbrs(n);
    for (size_t i = 0; i < n; ++i) {
        for (const auto& [j, _] : proj.adj[i]) {
            nbrs[i].insert(j);
        }
    }

    auto edge_key = [](size_t a, size_t b) {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
    };

    std::unordered_map<uint64_t, int> support;
    for (size_t u = 0; u < n; ++u) {
        for (const auto& v : nbrs[u]) {
            if (u >= v) continue;
            size_t small = nbrs[u].size() < nbrs[v].size() ? u : v;
            size_t large = (small == u) ? v : u;
            int count = 0;
            for (const auto& w : nbrs[small]) {
                if (w == large) continue;
                if (nbrs[large].find(w) != nbrs[large].end()) count++;
            }
            support[edge_key(u, v)] = count;
        }
    }

    int k = std::max(2, config_.k_truss_min_k);
    int threshold = k - 2;
    std::queue<std::pair<size_t, size_t>> q;
    std::unordered_set<uint64_t> removed;
    for (const auto& [key, sup] : support) {
        if (sup < threshold) {
            size_t u = static_cast<size_t>(key >> 32);
            size_t v = static_cast<size_t>(key & 0xffffffff);
            q.push({u, v});
        }
    }

    while (!q.empty()) {
        auto [u, v] = q.front();
        q.pop();
        uint64_t key = edge_key(u, v);
        if (removed.find(key) != removed.end()) continue;
        removed.insert(key);
        nbrs[u].erase(v);
        nbrs[v].erase(u);

        size_t small = nbrs[u].size() < nbrs[v].size() ? u : v;
        size_t large = (small == u) ? v : u;
        for (const auto& w : nbrs[small]) {
            if (nbrs[large].find(w) == nbrs[large].end()) continue;
            uint64_t k1 = edge_key(u, w);
            uint64_t k2 = edge_key(v, w);
            if (support.find(k1) != support.end()) {
                support[k1] -= 1;
                if (support[k1] < threshold) q.push({std::min(u, w), std::max(u, w)});
            }
            if (support.find(k2) != support.end()) {
                support[k2] -= 1;
                if (support[k2] < threshold) q.push({std::min(v, w), std::max(v, w)});
            }
        }
    }

    struct EdgeItem {
        size_t u;
        size_t v;
        int support;
    };
    std::vector<EdgeItem> edges;
    for (const auto& [key, sup] : support) {
        if (sup < threshold) continue;
        if (removed.find(key) != removed.end()) continue;
        size_t u = static_cast<size_t>(key >> 32);
        size_t v = static_cast<size_t>(key & 0xffffffff);
        edges.push_back({u, v, sup});
    }
    std::sort(edges.begin(), edges.end(),
              [](const auto& a, const auto& b) { return a.support > b.support; });

    size_t limit = std::min(edges.size(), config_.k_truss_max_candidates);
    int max_support = edges.empty() ? 0 : edges.front().support;
    for (size_t i = 0; i < limit; ++i) {
        const auto& e = edges[i];
        const std::string& a = proj.node_ids[e.u];
        const std::string& b = proj.node_ids[e.v];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::K_TRUSS);
        ins.type = InsightType::K_TRUSS;
        ins.seed_nodes = {a, b};
        ins.seed_labels = {get_node_label(a), get_node_label(b)};

        ins.witness_edges = shared_incident_edges(graph_, a, b, config_.centrality_max_evidence_edges);
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        std::vector<std::string> common;
        size_t small = nbrs[e.u].size() < nbrs[e.v].size() ? e.u : e.v;
        size_t large = (small == e.u) ? e.v : e.u;
        for (const auto& w : nbrs[small]) {
            if (nbrs[large].find(w) != nbrs[large].end()) {
                common.push_back(proj.node_ids[w]);
                if (common.size() >= 6) break;
            }
        }
        ins.witness_nodes = {a, b};
        ins.witness_nodes.insert(ins.witness_nodes.end(), common.begin(), common.end());

        double score = (max_support > 0) ? (static_cast<double>(e.support) / max_support) : 0.0;
        ins.score_breakdown["support"] = static_cast<double>(e.support);
        ins.score = score;
        ins.novelty_tags = {"k_truss", "k=" + std::to_string(k)};
        ins.description = "Dense edge in k-truss (k=" + std::to_string(k) + "): " +
                          get_node_label(a) + " - " + get_node_label(b);

        results.push_back(std::move(ins));
    }

    report_progress("k-truss analysis", 100, 100);
    return results;
}

// ============== CLAIM DECOMPOSITION + STANCE ==============
std::vector<Insight> DiscoveryEngine::find_claim_stances() {
    std::vector<Insight> results;
    report_progress("Claim stance", 0, 100);

    if (!llm_provider_) {
        report_progress("Claim stance", 100, 100);
        return results;
    }

    auto edges = graph_.get_all_edges();
    size_t limit = std::min(edges.size(), config_.claim_stance_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        const auto& edge = edges[i];
        if (edge.sources.empty() || edge.targets.empty()) continue;

        std::stringstream prompt;
        prompt << "You are analyzing a knowledge graph claim.\n"
               << "Given the relation and example entities, classify stance and paraphrase the claim.\n"
               << "Relation: " << edge.relation << "\n"
               << "Sources: " << join_labels(graph_, edge.sources, 3) << "\n"
               << "Targets: " << join_labels(graph_, edge.targets, 3) << "\n\n"
               << "Return format:\n"
               << "Stance: <supports|opposes|neutral>\n"
               << "Claim: <short paraphrase>\n"
               << "Confidence: <0-1>\n";

        std::vector<Message> messages = {
            Message(Message::Role::System, "You classify relation stance and paraphrase claims succinctly."),
            Message(Message::Role::User, prompt.str())
        };
        LLMResponse response = llm_provider_->chat(messages);
        if (!response.success || response.content.empty()) continue;

        std::string stance = parse_llm_field(response.content, "Stance");
        std::string claim = parse_llm_field(response.content, "Claim");
        std::string conf_str = parse_llm_field(response.content, "Confidence");
        double confidence = 0.0;
        try { confidence = std::stod(conf_str); } catch (...) { confidence = 0.0; }

        if (confidence < config_.claim_stance_min_confidence) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CLAIM_STANCE);
        ins.type = InsightType::CLAIM_STANCE;
        ins.seed_nodes = {edge.sources[0], edge.targets[0]};
        ins.seed_labels = {get_node_label(edge.sources[0]), get_node_label(edge.targets[0])};
        ins.witness_edges = {edge.id};
        ins.witness_nodes = {edge.sources[0], edge.targets[0]};
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.score_breakdown["confidence"] = confidence;
        ins.score = confidence;
        ins.novelty_tags = {"claim_stance", stance};
        ins.description = claim.empty() ? ("Claim stance: " + stance) : claim;
        ins.llm = nlohmann::json{
            {"stance", stance},
            {"claim", claim},
            {"confidence", confidence}
        };

        results.push_back(std::move(ins));
        if (results.size() >= config_.claim_stance_max_candidates) break;
        if (i % 10 == 0) {
            int pct = static_cast<int>(100.0 * (i + 1) / std::max<size_t>(1, limit));
            report_progress("Claim stance", pct, 100);
        }
    }

    report_progress("Claim stance", 100, 100);
    return results;
}

// ============== RELATION TYPE INDUCTION ==============
std::vector<Insight> DiscoveryEngine::find_relation_induction() {
    std::vector<Insight> results;
    report_progress("Relation induction", 0, 100);

    if (!llm_provider_) {
        report_progress("Relation induction", 100, 100);
        return results;
    }

    auto edges = graph_.get_all_edges();
    std::unordered_map<std::string, std::vector<const HyperEdge*>> by_relation;
    for (const auto& edge : edges) {
        if (edge.relation.empty()) continue;
        by_relation[edge.relation].push_back(&edge);
    }

    std::vector<std::pair<std::string, size_t>> ranked;
    for (const auto& [rel, list] : by_relation) {
        ranked.emplace_back(rel, list.size());
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    size_t rel_limit = std::min(ranked.size(), config_.relation_induction_max_relations);
    for (size_t i = 0; i < rel_limit; ++i) {
        const auto& rel = ranked[i].first;
        const auto& list = by_relation[rel];

        std::stringstream prompt;
        prompt << "You are normalizing relation labels in a knowledge graph.\n"
               << "Given examples, propose a canonical relation type name and a short description.\n"
               << "Relation label: " << rel << "\n"
               << "Examples:\n";
        size_t ex_limit = std::min(list.size(), config_.relation_induction_examples_per_relation);
        for (size_t j = 0; j < ex_limit; ++j) {
            const auto* edge = list[j];
            prompt << "- " << join_labels(graph_, edge->sources, 2)
                   << " -> " << join_labels(graph_, edge->targets, 2) << "\n";
        }
        prompt << "Return format:\n"
               << "Type: <canonical_relation_type>\n"
               << "Description: <short description>\n"
               << "Confidence: <0-1>\n";

        std::vector<Message> messages = {
            Message(Message::Role::System, "You standardize relation labels into canonical types."),
            Message(Message::Role::User, prompt.str())
        };
        LLMResponse response = llm_provider_->chat(messages);
        if (!response.success || response.content.empty()) continue;

        std::string type = parse_llm_field(response.content, "Type");
        std::string desc = parse_llm_field(response.content, "Description");
        std::string conf_str = parse_llm_field(response.content, "Confidence");
        double confidence = 0.0;
        try { confidence = std::stod(conf_str); } catch (...) { confidence = 0.0; }

        if (confidence < config_.relation_induction_min_confidence) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::RELATION_INDUCTION);
        ins.type = InsightType::RELATION_INDUCTION;
        if (!list.empty() && !list[0]->sources.empty() && !list[0]->targets.empty()) {
            ins.seed_nodes = {list[0]->sources[0], list[0]->targets[0]};
            ins.seed_labels = {get_node_label(list[0]->sources[0]), get_node_label(list[0]->targets[0])};
        }
        for (size_t j = 0; j < ex_limit; ++j) {
            ins.witness_edges.push_back(list[j]->id);
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.score_breakdown["confidence"] = confidence;
        ins.score = confidence;
        ins.novelty_tags = {"relation_induction", "type=" + type};
        ins.description = "Relation '" + rel + "' → " + type + ". " + desc;
        ins.llm = nlohmann::json{
            {"relation", rel},
            {"type", type},
            {"description", desc},
            {"confidence", confidence}
        };

        results.push_back(std::move(ins));
        int pct = static_cast<int>(100.0 * (i + 1) / std::max<size_t>(1, rel_limit));
        report_progress("Relation induction", pct, 100);
    }

    report_progress("Relation induction", 100, 100);
    return results;
}

// ============== ANALOGICAL TRANSFER ==============
std::vector<Insight> DiscoveryEngine::find_analogical_transfers() {
    std::vector<Insight> results;
    report_progress("Analogical transfer", 0, 100);

    auto edges = graph_.get_all_edges();
    std::unordered_map<std::string, std::vector<const HyperEdge*>> by_relation;
    for (const auto& edge : edges) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        by_relation[edge.relation].push_back(&edge);
    }

    size_t processed_rel = 0;
    for (const auto& [rel, list] : by_relation) {
        if (list.size() < 2) continue;
        size_t pair_limit = std::min(list.size() * (list.size() - 1) / 2,
                                      config_.analogical_transfer_pairs_per_relation);

        size_t pairs = 0;
        for (size_t i = 0; i < list.size() && pairs < pair_limit; ++i) {
            for (size_t j = i + 1; j < list.size() && pairs < pair_limit; ++j) {
                const auto* e1 = list[i];
                const auto* e2 = list[j];
                const std::string& a = e1->sources[0];
                const std::string& b = e1->targets[0];
                const std::string& c = e2->sources[0];
                const std::string& d = e2->targets[0];

                double sim_src = label_token_similarity(graph_, a, c);
                double sim_tgt = label_token_similarity(graph_, b, d);
                double score = 0.6 * ((sim_src + sim_tgt) / 2.0) + 0.4 * 1.0;

                if (score < config_.analogical_transfer_min_score) {
                    pairs++;
                    continue;
                }

                if (nodes_share_edge(graph_, a, d)) {
                    pairs++;
                    continue;
                }

                Insight ins;
                ins.insight_id = make_insight_id(InsightType::ANALOGICAL_TRANSFER);
                ins.type = InsightType::ANALOGICAL_TRANSFER;
                ins.seed_nodes = {a, d};
                ins.seed_labels = {get_node_label(a), get_node_label(d)};
                ins.witness_edges = {e1->id, e2->id};
                ins.witness_nodes = {a, b, c, d};
                ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
                ins.score_breakdown["similarity_source"] = sim_src;
                ins.score_breakdown["similarity_target"] = sim_tgt;
                ins.score = score;
                ins.novelty_tags = {"analogical_transfer", "relation=" + rel};
                ins.description = "Analogical transfer: '" + get_node_label(a) + "' " + rel +
                                  " '" + get_node_label(b) + "' and '" + get_node_label(c) + "' " +
                                  rel + " '" + get_node_label(d) + "' → suggest '" +
                                  get_node_label(a) + "' " + rel + " '" + get_node_label(d) + "'";

                if (llm_provider_) {
                    std::stringstream prompt;
                    prompt << "Check if the analogical relation is plausible.\n"
                           << "Given:\n"
                           << "- " << get_node_label(a) << " " << rel << " " << get_node_label(b) << "\n"
                           << "- " << get_node_label(c) << " " << rel << " " << get_node_label(d) << "\n"
                           << "Proposed: " << get_node_label(a) << " " << rel << " " << get_node_label(d) << "\n"
                           << "Return format:\n"
                           << "Confidence: <0-1>\n"
                           << "Rationale: <1 sentence>\n";
                    std::vector<Message> messages = {
                        Message(Message::Role::System, "You evaluate analogical transfer plausibility."),
                        Message(Message::Role::User, prompt.str())
                    };
                    LLMResponse response = llm_provider_->chat(messages);
                    if (response.success && !response.content.empty()) {
                        std::string conf_str = parse_llm_field(response.content, "Confidence");
                        std::string rationale = parse_llm_field(response.content, "Rationale");
                        double conf = 0.0;
                        try { conf = std::stod(conf_str); } catch (...) { conf = score; }
                        ins.llm = nlohmann::json{{"confidence", conf}, {"rationale", rationale}};
                        ins.score = 0.5 * ins.score + 0.5 * conf;
                        ins.score_breakdown["llm_confidence"] = conf;
                        if (!rationale.empty()) {
                            ins.description += ". " + rationale;
                        }
                    }
                }

                results.push_back(std::move(ins));
                if (results.size() >= config_.analogical_transfer_max_candidates) {
                    report_progress("Analogical transfer", 100, 100);
                    return results;
                }
                pairs++;
            }
        }

        processed_rel++;
        if (processed_rel % 5 == 0) {
            int pct = static_cast<int>(100.0 * processed_rel / std::max<size_t>(1, by_relation.size()));
            report_progress("Analogical transfer", pct, 100);
        }
    }

    report_progress("Analogical transfer", 100, 100);
    return results;
}

// ============== UNCERTAINTY SAMPLING ==============
std::vector<Insight> DiscoveryEngine::find_uncertainty_samples() {
    std::vector<Insight> results;
    report_progress("Uncertainty sampling", 0, 100);

    auto edges = graph_.get_all_edges();
    struct Candidate {
        std::string edge_id;
        std::string src;
        std::string tgt;
        double uncertainty;
        double confidence;
    };
    std::vector<Candidate> candidates;

    for (const auto& edge : edges) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        double conf = edge.confidence;
        double uncertainty = 1.0 - conf;
        if (uncertainty < config_.uncertainty_sampling_min_uncertainty) continue;
        candidates.push_back({edge.id, edge.sources[0], edge.targets[0], uncertainty, conf});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.uncertainty > b.uncertainty; });

    size_t limit = std::min(candidates.size(), config_.uncertainty_sampling_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        const auto& cand = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::UNCERTAINTY_SAMPLING);
        ins.type = InsightType::UNCERTAINTY_SAMPLING;
        ins.seed_nodes = {cand.src, cand.tgt};
        ins.seed_labels = {get_node_label(cand.src), get_node_label(cand.tgt)};
        ins.witness_edges = {cand.edge_id};
        ins.witness_nodes = {cand.src, cand.tgt};
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.score_breakdown["uncertainty"] = cand.uncertainty;
        ins.score = cand.uncertainty;
        ins.novelty_tags = {"uncertainty_sampling"};
        std::string relation = "";
        const auto* edge_ptr = graph_.get_hyperedge(cand.edge_id);
        if (edge_ptr) relation = edge_ptr->relation;
        ins.description = "Low-confidence relation: verify whether '" + ins.seed_labels[0] +
                          "' " + relation + " '" + ins.seed_labels[1] + "'";
        results.push_back(std::move(ins));
    }

    report_progress("Uncertainty sampling", 100, 100);
    return results;
}

// ============== COUNTERFACTUAL PROBING ==============
std::vector<Insight> DiscoveryEngine::find_counterfactual_probes() {
    std::vector<Insight> results;
    report_progress("Counterfactual probing", 0, 100);

    auto edges = graph_.get_all_edges();
    size_t limit = std::min(edges.size(), config_.counterfactual_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        const auto& edge = edges[i];
        if (edge.sources.empty() || edge.targets.empty()) continue;
        const std::string& src = edge.sources[0];
        const std::string& tgt = edge.targets[0];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::COUNTERFACTUAL);
        ins.type = InsightType::COUNTERFACTUAL;
        ins.seed_nodes = {src, tgt};
        ins.seed_labels = {get_node_label(src), get_node_label(tgt)};
        ins.witness_edges = {edge.id};
        ins.witness_nodes = {src, tgt};
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.novelty_tags = {"counterfactual"};

        std::string question = "What evidence would refute the claim that **" +
                               ins.seed_labels[0] + "** " + edge.relation + " **" +
                               ins.seed_labels[1] + "**?";

        if (llm_provider_) {
            std::stringstream prompt;
            prompt << "Generate a concise counterfactual probe question for the claim:\n"
                   << ins.seed_labels[0] << " " << edge.relation << " " << ins.seed_labels[1] << "\n"
                   << "Return format:\n"
                   << "Question: <question>\n";
            std::vector<Message> messages = {
                Message(Message::Role::System, "You write concise counterfactual probe questions."),
                Message(Message::Role::User, prompt.str())
            };
            LLMResponse response = llm_provider_->chat(messages);
            if (response.success && !response.content.empty()) {
                std::string q = parse_llm_field(response.content, "Question");
                if (!q.empty()) question = q;
                ins.llm = nlohmann::json{{"question", question}};
            }
        }

        ins.description = question;
        ins.score = 0.6;
        results.push_back(std::move(ins));
    }

    report_progress("Counterfactual probing", 100, 100);
    return results;
}

// ============== HYPEREDGE PREDICTION ==============
std::vector<Insight> DiscoveryEngine::find_hyperedge_predictions() {
    std::vector<Insight> results;
    report_progress("Hyperedge prediction", 0, 100);

    auto edges = graph_.get_all_edges();
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> rel_src_targets;
    for (const auto& edge : edges) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                rel_src_targets[edge.relation][src].insert(tgt);
            }
        }
    }

    struct Candidate {
        std::string rel;
        std::string src;
        std::string tgt;
        double score;
        std::vector<std::string> witness_edges;
    };
    std::vector<Candidate> candidates;

    for (const auto& [rel, src_map] : rel_src_targets) {
        std::vector<std::string> sources;
        for (const auto& [src, _] : src_map) sources.push_back(src);
        for (size_t i = 0; i < sources.size(); ++i) {
            const auto& s1 = sources[i];
            const auto& tset1 = src_map.at(s1);
            for (size_t j = i + 1; j < sources.size(); ++j) {
                const auto& s2 = sources[j];
                const auto& tset2 = src_map.at(s2);
                double overlap = jaccard_overlap(tset1, tset2);
                if (overlap < config_.hyperedge_prediction_min_score) continue;
                for (const auto& tgt : tset2) {
                    if (tset1.find(tgt) != tset1.end()) continue;
                    if (nodes_share_edge(graph_, s1, tgt)) continue;
                    candidates.push_back({rel, s1, tgt, overlap, {}});
                }
                for (const auto& tgt : tset1) {
                    if (tset2.find(tgt) != tset2.end()) continue;
                    if (nodes_share_edge(graph_, s2, tgt)) continue;
                    candidates.push_back({rel, s2, tgt, overlap, {}});
                }
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.score > b.score; });

    size_t limit = std::min(candidates.size(), config_.hyperedge_prediction_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        const auto& cand = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::HYPEREDGE_PREDICTION);
        ins.type = InsightType::HYPEREDGE_PREDICTION;
        ins.seed_nodes = {cand.src, cand.tgt};
        ins.seed_labels = {get_node_label(cand.src), get_node_label(cand.tgt)};
        ins.witness_nodes = {cand.src, cand.tgt};
        ins.score_breakdown["overlap"] = cand.score;
        ins.score = cand.score;
        ins.novelty_tags = {"hyperedge_prediction", "relation=" + cand.rel};
        ins.description = "Predicted relation: " + get_node_label(cand.src) + " " +
                          cand.rel + " " + get_node_label(cand.tgt);
        results.push_back(std::move(ins));
    }

    report_progress("Hyperedge prediction", 100, 100);
    return results;
}

// ============== CONSTRAINED RULE MINING ==============
std::vector<Insight> DiscoveryEngine::find_constrained_rules() {
    std::vector<Insight> results;
    report_progress("Constrained rule mining", 0, 100);

    auto edges = graph_.get_all_edges();
    double total_edges = static_cast<double>(graph_.num_edges());
    if (total_edges == 0) return results;

    std::map<std::string, std::vector<std::pair<std::set<std::string>, std::set<std::string>>>> relation_instances;
    for (const auto& edge : edges) {
        if (edge.relation.empty()) continue;
        std::set<std::string> sources(edge.sources.begin(), edge.sources.end());
        std::set<std::string> targets(edge.targets.begin(), edge.targets.end());
        relation_instances[edge.relation].emplace_back(sources, targets);
    }

    std::vector<std::string> relation_types;
    for (const auto& [rel, _] : relation_instances) {
        relation_types.push_back(rel);
    }

    struct RuleCandidate {
        std::string body_relation;
        std::string head_relation;
        std::string shared_role;
        int support;
        double confidence;
        double lift;
        std::vector<std::string> witness_edges;
    };

    std::vector<RuleCandidate> candidates;
    int processed = 0;
    int total_pairs = static_cast<int>(relation_types.size() * relation_types.size());

    for (const auto& body_rel : relation_types) {
        for (const auto& head_rel : relation_types) {
            if (body_rel == head_rel) continue;

            int support = 0;
            int body_count = relation_instances[body_rel].size();
            int head_count = relation_instances[head_rel].size();
            if (body_count < config_.constrained_rule_min_support) continue;
            if (head_count < config_.constrained_rule_min_support) continue;

            for (const auto& body_inst : relation_instances[body_rel]) {
                for (const auto& head_inst : relation_instances[head_rel]) {
                    bool share_source = false;
                    bool share_target = false;
                    for (const auto& s : body_inst.first) {
                        if (head_inst.first.find(s) != head_inst.first.end()) { share_source = true; break; }
                    }
                    for (const auto& t : body_inst.second) {
                        if (head_inst.second.find(t) != head_inst.second.end()) { share_target = true; break; }
                    }
                    if (share_source || share_target) {
                        support++;
                        if (support >= config_.constrained_rule_min_support) break;
                    }
                }
                if (support >= config_.constrained_rule_min_support) break;
            }

            if (support < config_.constrained_rule_min_support) continue;

            double confidence = static_cast<double>(support) / static_cast<double>(body_count);
            double lift = confidence / (static_cast<double>(head_count) / total_edges);

            if (confidence < config_.constrained_rule_min_confidence) continue;
            if (lift < config_.constrained_rule_min_lift) continue;

            RuleCandidate cand;
            cand.body_relation = body_rel;
            cand.head_relation = head_rel;
            cand.shared_role = "shared_entity";
            cand.support = support;
            cand.confidence = confidence;
            cand.lift = lift;
            candidates.push_back(std::move(cand));
        }
        processed++;
        if (processed % 5 == 0) {
            int pct = static_cast<int>(100.0 * processed / std::max(1, total_pairs));
            report_progress("Constrained rule mining", pct, 100);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                  if (a.confidence != b.confidence) return a.confidence > b.confidence;
                  return a.lift > b.lift;
              });

    size_t limit = std::min(candidates.size(), config_.constrained_rule_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        const auto& cand = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CONSTRAINED_RULE);
        ins.type = InsightType::CONSTRAINED_RULE;
        ins.score_breakdown["support"] = cand.support;
        ins.score_breakdown["confidence"] = cand.confidence;
        ins.score_breakdown["lift"] = cand.lift;
        ins.score = cand.confidence;
        ins.novelty_tags = {"constrained_rule"};
        std::stringstream desc;
        desc << "Constrained rule: if " << cand.body_relation << "(X, Y) then "
             << cand.head_relation << "(X, Z) [confidence=" << std::fixed << std::setprecision(2)
             << cand.confidence << ", lift=" << cand.lift << "]";
        ins.description = desc.str();
        results.push_back(std::move(ins));
    }

    report_progress("Constrained rule mining", 100, 100);
    return results;
}

// ============== DIFFUSION RELEVANCE ==============
std::vector<Insight> DiscoveryEngine::compute_diffusion_relevance(const std::string& seed_node) {
    std::vector<Insight> results;
    report_progress("Computing diffusion", 0, 100);

    std::unordered_map<std::string, double> scores;
    scores[seed_node] = 1.0;

    double damping = config_.diffusion_damping;
    int iterations = config_.diffusion_iterations;

    auto all_nodes = graph_.get_all_nodes();
    std::unordered_map<std::string, std::vector<std::string>> node_neighbors;

    for (const auto& node : all_nodes) {
        std::set<std::string> neighbors;
        for (const auto& eid : node.incident_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (edge) {
                for (const auto& n : edge->sources) {
                    if (n != node.id) neighbors.insert(n);
                }
                for (const auto& n : edge->targets) {
                    if (n != node.id) neighbors.insert(n);
                }
            }
        }
        node_neighbors[node.id] = std::vector<std::string>(neighbors.begin(), neighbors.end());
    }

    for (int iter = 0; iter < iterations; ++iter) {
        report_progress("Computing diffusion", (100 * iter) / iterations, 100);

        std::unordered_map<std::string, double> new_scores;

        for (const auto& [node_id, score] : scores) {
            const auto& neighbors = node_neighbors[node_id];
            if (neighbors.empty()) continue;

            double out_score = damping * score / neighbors.size();
            for (const auto& neighbor : neighbors) {
                new_scores[neighbor] += out_score;
            }
        }

        new_scores[seed_node] += (1.0 - damping);
        scores = std::move(new_scores);
    }

    report_progress("Computing diffusion", 90, 100);

    std::vector<std::pair<std::string, double>> ranked;
    for (const auto& [node_id, score] : scores) {
        if (node_id != seed_node) {
            ranked.emplace_back(node_id, score);
        }
    }
    std::sort(ranked.begin(), ranked.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t i = 0; i < std::min(ranked.size(), config_.diffusion_top_k); ++i) {
        const auto& target = ranked[i].first;
        double relevance = ranked[i].second;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::DIFFUSION);
        ins.type = InsightType::DIFFUSION;
        ins.seed_nodes = {seed_node, target};
        ins.seed_labels = {get_node_label(seed_node), get_node_label(target)};
        ins.witness_nodes = {seed_node, target};

        ins.novelty_tags = {"diffusion_relevance"};
        ins.description = "High relevance via random walk: " +
                         get_node_label(seed_node) + " -> " +
                         get_node_label(target) + " (score: " +
                         std::to_string(static_cast<int>(relevance * 1000) / 1000.0) + ")";

        ins.score_breakdown["support"] = 1;
        ins.score_breakdown["novelty"] = relevance;
        ins.score_breakdown["specificity"] = 1.0 / (1.0 + i);
        ins.score = relevance;

        results.push_back(std::move(ins));
    }

    report_progress("Computing diffusion", 100, 100);
    return results;
}

// ============== DIFFUSION DISCOVERY ==============
std::vector<Insight> DiscoveryEngine::find_diffusions() {
    std::vector<Insight> results;
    report_progress("Diffusion analysis", 0, 100);

    // Use top-degree nodes as seeds for diffusion relevance
    size_t seed_count = std::min<size_t>(config_.diffusion_top_k, index_.degree_ranked_nodes.size());
    for (size_t i = 0; i < seed_count; ++i) {
        const std::string& seed = index_.degree_ranked_nodes[i].first;
        auto rel = compute_diffusion_relevance(seed);
        for (auto& ins : rel) {
            results.push_back(std::move(ins));
            if (results.size() >= config_.diffusion_top_k) break;
        }
        if (results.size() >= config_.diffusion_top_k) break;

        if ((i + 1) % 5 == 0 || i + 1 == seed_count) {
            int pct = static_cast<int>(100.0 * (i + 1) / std::max<size_t>(1, seed_count));
            report_progress("Diffusion analysis", pct, 100);
        }
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    if (results.size() > config_.diffusion_top_k) {
        results.resize(config_.diffusion_top_k);
    }

    report_progress("Diffusion analysis", 100, 100);
    return results;
}

// ============== SURPRISE DETECTION ==============
std::vector<Insight> DiscoveryEngine::find_surprise_edges() {
    std::vector<Insight> results;
    report_progress("Finding surprise edges", 0, 100);

    double total_edges = static_cast<double>(graph_.num_edges());
    auto all_edges = graph_.get_all_edges();

    std::vector<std::tuple<std::string, std::set<std::string>, double>> candidates;

    int processed = 0;
    for (const auto& edge : all_edges) {
        if (++processed % 100 == 0) {
            report_progress("Finding surprise edges",
                          static_cast<int>(80 * processed / all_edges.size()), 100);
        }

        std::set<std::string> entities;
        for (const auto& n : edge.sources) entities.insert(n);
        for (const auto& n : edge.targets) entities.insert(n);

        if (entities.size() < 2) continue;

        double expected = 1.0;
        for (const auto& n : entities) {
            const auto* node = graph_.get_node(n);
            if (node) {
                expected *= node->degree / total_edges;
            }
        }

        if (expected < config_.surprise_max_expected_cooccurrence) {
            double surprise = 1.0 / (expected + 1e-10);
            candidates.emplace_back(edge.id, entities, surprise);
        }
    }

    report_progress("Finding surprise edges", 85, 100);

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

    for (size_t i = 0; i < std::min(candidates.size(), config_.surprise_max_candidates); ++i) {
        const auto& [edge_id, entities, surprise] = candidates[i];
        const auto* edge = graph_.get_hyperedge(edge_id);

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::SURPRISE);
        ins.type = InsightType::SURPRISE;
        ins.seed_nodes = std::vector<std::string>(entities.begin(), entities.end());
        for (const auto& n : ins.seed_nodes) {
            ins.seed_labels.push_back(get_node_label(n));
        }
        ins.witness_edges = {edge_id};
        ins.witness_nodes = ins.seed_nodes;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        ins.novelty_tags = {"surprising_combination"};

        std::string labels_str;
        for (size_t j = 0; j < ins.seed_labels.size() && j < 4; ++j) {
            if (j > 0) labels_str += ", ";
            labels_str += ins.seed_labels[j];
        }
        if (ins.seed_labels.size() > 4) labels_str += ", ...";

        ins.description = "Surprising combination: {" + labels_str + "} in relation '" +
                         (edge ? edge->relation : "") + "'";

        ins.score_breakdown["support"] = 1;
        ins.score_breakdown["novelty"] = std::min(10.0, std::log(surprise));
        ins.score_breakdown["specificity"] = 1.0 / entities.size();
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Finding surprise edges", 100, 100);
    return results;
}

// ============== RULE MINING (ASSOCIATION RULES) ==============
std::vector<Insight> DiscoveryEngine::find_rules() {
    std::vector<Insight> results;
    report_progress("Mining association rules", 0, 100);

    auto all_edges = graph_.get_all_edges();
    double total_edges = static_cast<double>(graph_.num_edges());

    // Step 1: Build relation -> entity pairs index
    // For each relation type, collect all (source_set, target_set) pairs
    std::map<std::string, std::vector<std::pair<std::set<std::string>, std::set<std::string>>>> relation_instances;

    for (const auto& edge : all_edges) {
        if (edge.relation.empty()) continue;
        std::set<std::string> sources(edge.sources.begin(), edge.sources.end());
        std::set<std::string> targets(edge.targets.begin(), edge.targets.end());
        relation_instances[edge.relation].emplace_back(sources, targets);
    }

    report_progress("Mining association rules", 20, 100);

    // Step 2: Mine rules of the form: relation1(X, Y) => relation2(X, Z) or relation2(Y, Z)
    // This finds patterns like: "if teaches(X, Y), then influences(X, Z)"

    struct RuleCandidate {
        std::string body_relation;      // relation in rule body
        std::string head_relation;      // relation in rule head
        std::string shared_role;        // "source" or "target" - which role is shared
        int support;                    // number of times rule fires
        double confidence;              // P(head | body)
        double lift;                    // confidence / P(head)
        std::vector<std::string> example_entities;  // example shared entities
        std::vector<std::string> witness_edges;
    };

    std::vector<RuleCandidate> candidates;

    // For each pair of relation types, check for co-occurrence patterns
    std::vector<std::string> relation_types;
    for (const auto& [rel, _] : relation_instances) {
        relation_types.push_back(rel);
    }

    int processed = 0;
    int total_pairs = static_cast<int>(relation_types.size() * relation_types.size());

    for (const auto& body_rel : relation_types) {
        for (const auto& head_rel : relation_types) {
            if (body_rel == head_rel) continue;

            processed++;
            if (processed % 50 == 0) {
                report_progress("Mining association rules",
                              20 + (60 * processed / total_pairs), 100);
            }

            const auto& body_instances = relation_instances[body_rel];
            const auto& head_instances = relation_instances[head_rel];

            // Build index of head relation by source entities
            std::map<std::string, std::vector<size_t>> head_by_source;
            std::map<std::string, std::vector<size_t>> head_by_target;
            for (size_t i = 0; i < head_instances.size(); ++i) {
                for (const auto& s : head_instances[i].first) {
                    head_by_source[s].push_back(i);
                }
                for (const auto& t : head_instances[i].second) {
                    head_by_target[t].push_back(i);
                }
            }

            // Check rule: body_rel(X, _) => head_rel(X, _) [shared source]
            int support_source = 0;
            std::set<std::string> shared_source_entities;
            for (const auto& [body_sources, body_targets] : body_instances) {
                for (const auto& src : body_sources) {
                    if (head_by_source.count(src) > 0) {
                        support_source++;
                        shared_source_entities.insert(src);
                        break;
                    }
                }
            }

            if (support_source >= config_.rule_min_support) {
                double confidence = static_cast<double>(support_source) / body_instances.size();
                double head_prob = static_cast<double>(head_instances.size()) / total_edges;
                double lift = (head_prob > 0) ? confidence / head_prob : 0;

                if (confidence >= config_.rule_min_confidence && lift >= config_.rule_min_lift) {
                    RuleCandidate rule;
                    rule.body_relation = body_rel;
                    rule.head_relation = head_rel;
                    rule.shared_role = "source";
                    rule.support = support_source;
                    rule.confidence = confidence;
                    rule.lift = lift;

                    int count = 0;
                    for (const auto& e : shared_source_entities) {
                        rule.example_entities.push_back(e);
                        if (++count >= 5) break;
                    }

                    candidates.push_back(rule);
                }
            }

            // Check rule: body_rel(_, Y) => head_rel(_, Y) [shared target]
            int support_target = 0;
            std::set<std::string> shared_target_entities;
            for (const auto& [body_sources, body_targets] : body_instances) {
                for (const auto& tgt : body_targets) {
                    if (head_by_target.count(tgt) > 0) {
                        support_target++;
                        shared_target_entities.insert(tgt);
                        break;
                    }
                }
            }

            if (support_target >= config_.rule_min_support) {
                double confidence = static_cast<double>(support_target) / body_instances.size();
                double head_prob = static_cast<double>(head_instances.size()) / total_edges;
                double lift = (head_prob > 0) ? confidence / head_prob : 0;

                if (confidence >= config_.rule_min_confidence && lift >= config_.rule_min_lift) {
                    RuleCandidate rule;
                    rule.body_relation = body_rel;
                    rule.head_relation = head_rel;
                    rule.shared_role = "target";
                    rule.support = support_target;
                    rule.confidence = confidence;
                    rule.lift = lift;

                    int count = 0;
                    for (const auto& e : shared_target_entities) {
                        rule.example_entities.push_back(e);
                        if (++count >= 5) break;
                    }

                    candidates.push_back(rule);
                }
            }
        }
    }

    report_progress("Mining association rules", 85, 100);

    // Sort by lift * confidence (combined quality score)
    std::sort(candidates.begin(), candidates.end(),
        [](const RuleCandidate& a, const RuleCandidate& b) {
            return (a.lift * a.confidence) > (b.lift * b.confidence);
        });

    // Convert to insights
    for (size_t i = 0; i < std::min(candidates.size(), config_.rule_max_candidates); ++i) {
        const auto& rule = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::RULE);
        ins.type = InsightType::RULE;

        // Seed nodes are the example entities that satisfy the rule
        ins.seed_nodes = rule.example_entities;
        for (const auto& e : rule.example_entities) {
            ins.seed_labels.push_back(get_node_label(e));
        }

        ins.witness_nodes = rule.example_entities;
        ins.novelty_tags = {"association_rule", "relation_pattern"};

        // Build description
        std::stringstream desc;
        desc << "Rule: " << rule.body_relation << "(X, _) => " << rule.head_relation << "(X, _)";
        if (rule.shared_role == "target") {
            desc.str("");
            desc << "Rule: " << rule.body_relation << "(_, Y) => " << rule.head_relation << "(_, Y)";
        }
        desc << " [support=" << rule.support
             << ", confidence=" << std::fixed << std::setprecision(2) << rule.confidence
             << ", lift=" << std::setprecision(2) << rule.lift << "]";
        ins.description = desc.str();

        ins.score_breakdown["support"] = static_cast<double>(rule.support);
        ins.score_breakdown["novelty"] = rule.lift;
        ins.score_breakdown["specificity"] = rule.confidence;
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    report_progress("Mining association rules", 100, 100);
    return results;
}

// ============== PATH RANKING (PATH-BASED LINK PREDICTION) ==============
std::vector<Insight> DiscoveryEngine::find_path_rankings() {
    std::vector<Insight> results;
    report_progress("Path ranking", 0, 100);

    std::vector<std::string> candidates;
    if (!index_.degree_ranked_nodes.empty()) {
        for (size_t i = 0; i < std::min(config_.path_rank_max_seed_nodes, index_.degree_ranked_nodes.size()); ++i) {
            candidates.push_back(index_.degree_ranked_nodes[i].first);
        }
    } else {
        auto nodes = graph_.get_all_nodes();
        std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) {
            return a.degree > b.degree;
        });
        for (size_t i = 0; i < std::min(config_.path_rank_max_seed_nodes, nodes.size()); ++i) {
            candidates.push_back(nodes[i].id);
        }
    }

    if (candidates.size() < 2) {
        return results;
    }

    size_t total_pairs = candidates.size() * (candidates.size() - 1) / 2;
    size_t max_pairs = std::min(config_.path_rank_max_pairs, total_pairs);
    size_t checked = 0;

    for (size_t i = 0; i < candidates.size(); ++i) {
        for (size_t j = i + 1; j < candidates.size(); ++j) {
            if (checked >= max_pairs) break;
            checked++;

            if (checked % 50 == 0 || checked == max_pairs) {
                int pct = 5 + static_cast<int>(90.0 * checked / std::max<size_t>(1, max_pairs));
                report_progress("Path ranking", pct, 100);
            }

            const std::string& a = candidates[i];
            const std::string& b = candidates[j];

            if (index_.get_cooccurrence(a, b) > 0) {
                continue;
            }

            auto paths = graph_.find_k_shortest_paths(
                a, b, config_.path_rank_k, config_.path_rank_min_intersection);
            if (!paths.found || paths.paths.empty()) {
                continue;
            }

            int min_len = std::numeric_limits<int>::max();
            double score_sum = 0.0;
            std::set<std::string> edge_ids;
            std::set<std::string> node_ids;
            int path_count = 0;

            for (const auto& path : paths.paths) {
                if (path.empty() || static_cast<int>(path.size()) > config_.path_rank_max_hops) {
                    continue;
                }
                path_count++;
                min_len = std::min(min_len, static_cast<int>(path.size()));
                score_sum += 1.0 / static_cast<double>(path.size());

                for (const auto& edge : path) {
                    if (edge_ids.size() >= config_.path_rank_max_witness_edges) {
                        break;
                    }
                    if (edge_ids.insert(edge.id).second) {
                        const auto* e = graph_.get_hyperedge(edge.id);
                        if (e) {
                            for (const auto& src : e->sources) node_ids.insert(src);
                            for (const auto& tgt : e->targets) node_ids.insert(tgt);
                        }
                    }
                }
            }

            if (path_count == 0 || score_sum < config_.path_rank_min_score) {
                continue;
            }

            Insight ins;
            ins.insight_id = make_insight_id(InsightType::PATH_RANK);
            ins.type = InsightType::PATH_RANK;
            ins.seed_nodes = {a, b};
            std::string label_a = get_node_label(a);
            std::string label_b = get_node_label(b);
            ins.seed_labels = {label_a.empty() ? a : label_a, label_b.empty() ? b : label_b};
            ins.witness_edges.assign(edge_ids.begin(), edge_ids.end());
            if (ins.witness_edges.size() < config_.path_rank_min_evidence_edges) {
                continue;
            }
            ins.witness_nodes.assign(node_ids.begin(), node_ids.end());
            ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
            ins.novelty_tags = {"path_rank", "paths=" + std::to_string(path_count)};

            std::stringstream desc;
            desc << "PathRank: " << ins.seed_labels[0] << " <-> " << ins.seed_labels[1]
                 << " via " << path_count << " paths (min_len=" << min_len << ")";
            ins.description = desc.str();

            ins.score_breakdown["support"] = static_cast<double>(path_count);
            ins.score_breakdown["novelty"] = min_len > 0 ? (1.0 / min_len) : 0.0;
            ins.score_breakdown["specificity"] = score_sum;
            ins.score = compute_score(ins);

            results.push_back(std::move(ins));
        }
        if (checked >= max_pairs) break;
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.path_rank_max_candidates) {
        results.resize(config_.path_rank_max_candidates);
    }

    report_progress("Path ranking", 100, 100);
    return results;
}

// ============== HYPOTHESIS SYNTHESIS ==============
std::vector<Insight> DiscoveryEngine::find_hypotheses(const InsightCollection& collection) {
    std::vector<Insight> results;
    if (collection.insights.empty() || config_.hypothesis_count == 0) return results;

    struct HypothesisCandidate {
        std::vector<std::string> nodes;
        std::vector<std::string> labels;
        std::unordered_set<InsightType> signals;
        std::vector<std::string> witness_edges;
        std::vector<std::string> witness_nodes;
        std::vector<std::string> evidence_chunk_ids;
        std::vector<std::string> tags;
        std::map<std::string, double> scores;
        double score = 0.0;
        bool is_merge = false;
        bool has_contradiction = false;
    };

    auto make_pair_key = [](const std::string& a, const std::string& b) {
        return a < b ? a + "|" + b : b + "|" + a;
    };

    std::unordered_map<std::string, double> core_scores;
    std::unordered_map<std::string, double> degree_norm;
    int max_degree = 1;
    auto all_nodes = graph_.get_all_nodes();
    for (const auto& node : all_nodes) {
        max_degree = std::max(max_degree, node.degree);
    }
    for (const auto& node : all_nodes) {
        degree_norm[node.id] = static_cast<double>(node.degree) / max_degree;
    }

    std::unordered_set<std::string> contradiction_pairs;
    std::unordered_map<std::string, double> text_similarity_pairs;

    for (const auto& ins : collection.insights) {
        if (ins.type == InsightType::CORE_PERIPHERY && !ins.seed_nodes.empty()) {
            double core_score = ins.score;
            for (const auto& tag : ins.novelty_tags) {
                if (tag == "periphery") {
                    core_score = 0.0;
                    break;
                }
            }
            core_scores[ins.seed_nodes[0]] = core_score;
        }

        if (ins.type == InsightType::CONTRADICTION) {
            for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
                for (size_t j = i + 1; j < ins.seed_nodes.size(); ++j) {
                    contradiction_pairs.insert(make_pair_key(ins.seed_nodes[i], ins.seed_nodes[j]));
                }
            }
        }

        if (ins.type == InsightType::TEXT_SIMILARITY && ins.seed_nodes.size() >= 2) {
            text_similarity_pairs[make_pair_key(ins.seed_nodes[0], ins.seed_nodes[1])] = ins.score;
        }
    }

    std::unordered_map<std::string, HypothesisCandidate> candidates;
    std::unordered_map<std::string, HypothesisCandidate> merge_candidates;

    auto add_evidence = [&](HypothesisCandidate& cand, const Insight& ins) {
        for (const auto& eid : ins.witness_edges) {
            if (cand.witness_edges.size() >= 12) break;
            cand.witness_edges.push_back(eid);
        }
        for (const auto& nid : ins.witness_nodes) {
            if (cand.witness_nodes.size() >= 20) break;
            cand.witness_nodes.push_back(nid);
        }
        for (const auto& cid : ins.evidence_chunk_ids) {
            if (cand.evidence_chunk_ids.size() >= 10) break;
            cand.evidence_chunk_ids.push_back(cid);
        }
    };

    auto register_pair = [&](const Insight& ins, bool is_merge) {
        if (ins.seed_nodes.size() < 2) return;
        std::string key = make_pair_key(ins.seed_nodes[0], ins.seed_nodes[1]);
        auto& cand = is_merge ? merge_candidates[key] : candidates[key];
        if (cand.nodes.empty()) {
            cand.nodes = {ins.seed_nodes[0], ins.seed_nodes[1]};
            cand.labels = {get_node_label(ins.seed_nodes[0]), get_node_label(ins.seed_nodes[1])};
            cand.is_merge = is_merge;
        }
        cand.signals.insert(ins.type);
        add_evidence(cand, ins);
        if (ins.type == InsightType::ENTITY_RESOLUTION) {
            auto it_label = ins.score_breakdown.find("label_similarity");
            if (it_label != ins.score_breakdown.end()) {
                cand.scores["label_similarity"] = it_label->second;
            }
            auto it_neighbor = ins.score_breakdown.find("neighbor_overlap");
            if (it_neighbor != ins.score_breakdown.end()) {
                cand.scores["neighbor_overlap"] = it_neighbor->second;
            }
        }
    };

    const std::unordered_set<InsightType> link_signals = {
        InsightType::COMPLETION,
        InsightType::PATH_RANK,
        InsightType::EMBEDDING_LINK,
        InsightType::COMMUNITY_LINK,
        InsightType::TEXT_SIMILARITY,
        InsightType::SURPRISE
    };

    for (const auto& ins : collection.insights) {
        if (link_signals.find(ins.type) != link_signals.end()) {
            register_pair(ins, false);
        }
        if (ins.type == InsightType::ENTITY_RESOLUTION) {
            register_pair(ins, true);
        }
    }

    auto finalize_candidate = [&](HypothesisCandidate& cand) {
        std::string key = make_pair_key(cand.nodes[0], cand.nodes[1]);
        cand.has_contradiction = contradiction_pairs.find(key) != contradiction_pairs.end();

        double semantic = 0.0;
        auto it_sem = text_similarity_pairs.find(key);
        if (it_sem != text_similarity_pairs.end()) semantic = it_sem->second;

        double centrality = 0.0;
        double c1 = core_scores.count(cand.nodes[0]) ? core_scores[cand.nodes[0]] : degree_norm[cand.nodes[0]];
        double c2 = core_scores.count(cand.nodes[1]) ? core_scores[cand.nodes[1]] : degree_norm[cand.nodes[1]];
        centrality = 0.5 * (c1 + c2);

        double consistency = cand.has_contradiction ? 0.3 : 1.0;

        double support = 0.0;
        double max_signals = static_cast<double>(link_signals.size());
        support = max_signals > 0 ? cand.signals.size() / max_signals : 0.0;

        cand.scores["support"] = support;
        cand.scores["semantic"] = semantic;
        cand.scores["centrality"] = centrality;
        cand.scores["consistency"] = consistency;

        if (cand.is_merge) {
            double label_sim = 0.0;
            auto it_label = cand.scores.find("label_similarity");
            if (it_label != cand.scores.end()) label_sim = it_label->second;
            cand.score = 0.5 * label_sim + 0.3 * semantic + 0.2 * centrality;
        } else {
            cand.score = 0.35 * support + 0.25 * consistency + 0.2 * centrality + 0.2 * semantic;
        }
    };

    for (auto& [key, cand] : candidates) {
        finalize_candidate(cand);
    }
    for (auto& [key, cand] : merge_candidates) {
        finalize_candidate(cand);
    }

    std::vector<HypothesisCandidate> all_candidates;
    all_candidates.reserve(candidates.size() + merge_candidates.size());
    for (auto& [key, cand] : candidates) all_candidates.push_back(cand);
    for (auto& [key, cand] : merge_candidates) all_candidates.push_back(cand);

    std::sort(all_candidates.begin(), all_candidates.end(),
              [](const HypothesisCandidate& a, const HypothesisCandidate& b) { return a.score > b.score; });

    size_t limit = std::min(config_.hypothesis_count, all_candidates.size());
    for (size_t i = 0; i < limit; ++i) {
        auto& cand = all_candidates[i];
        Insight hyp;
        hyp.insight_id = make_insight_id(InsightType::HYPOTHESIS);
        hyp.type = InsightType::HYPOTHESIS;
        hyp.seed_nodes = cand.nodes;
        hyp.seed_labels = cand.labels;
        hyp.witness_edges = cand.witness_edges;
        hyp.witness_nodes = cand.witness_nodes;
        hyp.evidence_chunk_ids = cand.evidence_chunk_ids;
        hyp.score_breakdown = cand.scores;
        hyp.score = cand.score;
        hyp.novelty_tags = {"hypothesis", cand.is_merge ? "merge_candidate" : "relation_candidate"};
        if (cand.has_contradiction) {
            hyp.novelty_tags.push_back("conflict");
        }

        std::stringstream default_desc;
        if (cand.is_merge) {
            default_desc << "Hypothesis: " << hyp.seed_labels[0] << " and " << hyp.seed_labels[1]
                         << " may refer to the same underlying entity based on label and context similarity.";
        } else {
            default_desc << "Hypothesis: " << hyp.seed_labels[0] << " is meaningfully related to "
                         << hyp.seed_labels[1] << " based on multiple structural signals.";
        }

        if (llm_provider_) {
            std::stringstream prompt;
            prompt << "You are a scientific analyst. Synthesize a testable hypothesis based on graph evidence.\n";
            prompt << "Entities: " << hyp.seed_labels[0] << " | " << hyp.seed_labels[1] << "\n";
            prompt << "Candidate type: " << (cand.is_merge ? "merge/alias" : "relationship") << "\n";
            prompt << "Signals: ";
            bool first = true;
            for (const auto& sig : cand.signals) {
                if (!first) prompt << ", ";
                prompt << insight_type_to_string(sig);
                first = false;
            }
            if (cand.signals.empty()) prompt << "none";
            prompt << "\n";
            prompt << "Scores: support=" << std::fixed << std::setprecision(2) << cand.scores["support"]
                   << ", semantic=" << cand.scores["semantic"]
                   << ", centrality=" << cand.scores["centrality"]
                   << ", consistency=" << cand.scores["consistency"] << "\n";
            if (cand.has_contradiction) {
                prompt << "Note: contradictory evidence exists.\n";
            }
            prompt << "Write 2-3 concise sentences. Sentence 1 states the hypothesis. "
                   << "Sentence 2 notes supporting evidence. Sentence 3 (optional) notes uncertainty. "
                   << "No bullet points or markdown.\n";

            std::vector<Message> messages = {
                Message(Message::Role::System, "You write precise, testable scientific hypotheses."),
                Message(Message::Role::User, prompt.str())
            };

            LLMResponse response = llm_provider_->chat(messages);
            if (response.success && !response.content.empty()) {
                hyp.description = response.content;
                hyp.llm = nlohmann::json{
                    {"hypothesis", response.content},
                    {"signals", nlohmann::json::array()}
                };
                for (const auto& sig : cand.signals) {
                    hyp.llm["signals"].push_back(insight_type_to_string(sig));
                }
            } else {
                hyp.description = default_desc.str();
            }
        } else {
            hyp.description = default_desc.str();
        }

        results.push_back(std::move(hyp));
    }

    return results;
}

// ============== EMBEDDING LINK PREDICTION (TransE) ==============

// L2 normalize a vector
void DiscoveryEngine::normalize_vector(std::vector<double>& vec) const {
    double norm = 0.0;
    for (double v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10) {
        for (double& v : vec) v /= norm;
    }
}

// Extract triples from hypergraph - convert hyperedges to binary relations
std::vector<DiscoveryEngine::Triple> DiscoveryEngine::extract_triples(EmbeddingModel& model) const {
    std::vector<Triple> triples;

    // Build entity and relation mappings
    std::set<std::string> entities;
    std::set<std::string> relations;

    // Collect all entities (nodes) and relations (edge labels/types)
    auto all_nodes = graph_.get_all_nodes();
    for (const auto& node : all_nodes) {
        entities.insert(node.id);
    }

    auto all_edges = graph_.get_all_edges();
    for (const auto& edge : all_edges) {
        // Use edge relation as relation type
        std::string rel = edge.relation.empty() ? "related_to" : edge.relation;
        relations.insert(rel);
    }

    // Build index mappings
    size_t idx = 0;
    for (const auto& e : entities) {
        model.entity_to_idx[e] = idx;
        model.idx_to_entity.push_back(e);
        idx++;
    }

    idx = 0;
    for (const auto& r : relations) {
        model.relation_to_idx[r] = idx;
        model.idx_to_relation.push_back(r);
        idx++;
    }

    // Convert hyperedges to binary triples
    // For a hyperedge with sources [A, B] and targets [C, D], generate triples
    // connecting sources to targets via the relation
    for (const auto& edge : all_edges) {
        std::string rel = edge.relation.empty() ? "related_to" : edge.relation;
        size_t rel_idx = model.relation_to_idx[rel];

        // Get all nodes in this edge
        std::set<std::string> all_nodes_in_edge = edge.get_all_nodes();
        std::vector<std::string> nodes(all_nodes_in_edge.begin(), all_nodes_in_edge.end());

        // Generate triples for all pairs of nodes in the hyperedge
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                auto it_h = model.entity_to_idx.find(nodes[i]);
                auto it_t = model.entity_to_idx.find(nodes[j]);
                if (it_h != model.entity_to_idx.end() && it_t != model.entity_to_idx.end()) {
                    Triple t;
                    t.head = it_h->second;
                    t.relation = rel_idx;
                    t.tail = it_t->second;
                    triples.push_back(t);
                    // Also add reverse for undirected relations
                    Triple t_rev;
                    t_rev.head = it_t->second;
                    t_rev.relation = rel_idx;
                    t_rev.tail = it_h->second;
                    triples.push_back(t_rev);
                }
            }
        }
    }

    return triples;
}

// Initialize embeddings randomly using Xavier initialization
void DiscoveryEngine::init_embeddings(EmbeddingModel& model, size_t num_entities, size_t num_relations) const {
    std::random_device rd;
    std::mt19937 gen(rd());
    double bound = std::sqrt(6.0 / config_.embedding_dim);
    std::uniform_real_distribution<double> dist(-bound, bound);

    model.entity_embeddings.resize(num_entities);
    for (size_t i = 0; i < num_entities; ++i) {
        model.entity_embeddings[i].resize(config_.embedding_dim);
        for (size_t d = 0; d < config_.embedding_dim; ++d) {
            model.entity_embeddings[i][d] = dist(gen);
        }
        normalize_vector(model.entity_embeddings[i]);
    }

    model.relation_embeddings.resize(num_relations);
    for (size_t i = 0; i < num_relations; ++i) {
        model.relation_embeddings[i].resize(config_.embedding_dim);
        for (size_t d = 0; d < config_.embedding_dim; ++d) {
            model.relation_embeddings[i][d] = dist(gen);
        }
        // Relations are not normalized in TransE
    }
}

// TransE scoring function: returns distance ||h + r - t||
// Lower score = more plausible triple
double DiscoveryEngine::transe_score(const EmbeddingModel& model, const Triple& triple) const {
    double dist = 0.0;
    const auto& h = model.entity_embeddings[triple.head];
    const auto& r = model.relation_embeddings[triple.relation];
    const auto& t = model.entity_embeddings[triple.tail];

    for (size_t d = 0; d < config_.embedding_dim; ++d) {
        double diff = h[d] + r[d] - t[d];
        dist += diff * diff;
    }
    return std::sqrt(dist);
}

// Generate corrupted triple (negative sample)
DiscoveryEngine::Triple DiscoveryEngine::corrupt_triple(const Triple& triple, size_t num_entities) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> entity_dist(0, num_entities - 1);
    std::uniform_real_distribution<double> coin(0.0, 1.0);

    Triple corrupted = triple;
    if (coin(gen) < 0.5) {
        // Corrupt head
        corrupted.head = entity_dist(gen);
        while (corrupted.head == triple.head) {
            corrupted.head = entity_dist(gen);
        }
    } else {
        // Corrupt tail
        corrupted.tail = entity_dist(gen);
        while (corrupted.tail == triple.tail) {
            corrupted.tail = entity_dist(gen);
        }
    }
    return corrupted;
}

// Train TransE model using SGD with margin-based ranking loss
void DiscoveryEngine::train_transe(EmbeddingModel& model, const std::vector<Triple>& triples) {
    if (triples.empty()) return;

    size_t num_entities = model.entity_embeddings.size();
    double lr = config_.embedding_learning_rate;
    double margin = config_.embedding_margin;

    std::random_device rd;
    std::mt19937 gen(rd());

    for (size_t epoch = 0; epoch < config_.embedding_epochs; ++epoch) {
        // Shuffle triples for each epoch
        std::vector<size_t> indices(triples.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), gen);

        double total_loss = 0.0;

        for (size_t batch_start = 0; batch_start < triples.size(); batch_start += config_.embedding_batch_size) {
            size_t batch_end = std::min(batch_start + config_.embedding_batch_size, triples.size());

            for (size_t idx = batch_start; idx < batch_end; ++idx) {
                const Triple& pos_triple = triples[indices[idx]];

                // Generate negative samples
                for (size_t neg = 0; neg < config_.embedding_neg_samples; ++neg) {
                    Triple neg_triple = corrupt_triple(pos_triple, num_entities);

                    double pos_score = transe_score(model, pos_triple);
                    double neg_score = transe_score(model, neg_triple);

                    // Margin-based ranking loss: max(0, margin + pos_score - neg_score)
                    double loss = margin + pos_score - neg_score;
                    if (loss > 0) {
                        total_loss += loss;

                        // Gradient update
                        auto& h_pos = model.entity_embeddings[pos_triple.head];
                        auto& t_pos = model.entity_embeddings[pos_triple.tail];
                        auto& r = model.relation_embeddings[pos_triple.relation];
                        auto& h_neg = model.entity_embeddings[neg_triple.head];
                        auto& t_neg = model.entity_embeddings[neg_triple.tail];

                        for (size_t d = 0; d < config_.embedding_dim; ++d) {
                            // Gradient for positive triple: minimize h + r - t
                            double grad_pos = 2.0 * (h_pos[d] + r[d] - t_pos[d]);
                            // Gradient for negative triple: maximize h + r - t (so subtract)
                            double grad_neg = 2.0 * (h_neg[d] + r[d] - t_neg[d]);

                            // Update embeddings
                            h_pos[d] -= lr * grad_pos;
                            t_pos[d] += lr * grad_pos;
                            r[d] -= lr * (grad_pos - grad_neg);
                            h_neg[d] += lr * grad_neg;
                            t_neg[d] -= lr * grad_neg;
                        }
                    }
                }
            }
        }

        // Normalize entity embeddings after each epoch
        for (auto& emb : model.entity_embeddings) {
            normalize_vector(emb);
        }

        // Report progress every 10 epochs
        if (epoch % 10 == 0 || epoch == config_.embedding_epochs - 1) {
            report_progress("TransE training", static_cast<int>(epoch + 1),
                          static_cast<int>(config_.embedding_epochs));
        }
    }
}

// Predict missing links by scoring all possible (h, r, t) combinations
std::vector<std::pair<DiscoveryEngine::Triple, double>> DiscoveryEngine::predict_links(
    const EmbeddingModel& model,
    const std::vector<Triple>& existing_triples) const {

    // Build set of existing triples for fast lookup
    std::set<std::tuple<size_t, size_t, size_t>> existing;
    for (const auto& t : existing_triples) {
        existing.insert({t.head, t.relation, t.tail});
    }

    std::vector<std::pair<Triple, double>> predictions;
    size_t num_entities = model.entity_embeddings.size();
    size_t num_relations = model.relation_embeddings.size();

    // For efficiency, only consider high-degree entities as candidates
    // Use the index to get top entities by degree
    std::vector<size_t> candidate_entities;
    size_t max_candidates = std::min(size_t(100), num_entities);

    for (size_t i = 0; i < std::min(index_.degree_ranked_nodes.size(), max_candidates); ++i) {
        const auto& node_id = index_.degree_ranked_nodes[i].first;
        auto it = model.entity_to_idx.find(node_id);
        if (it != model.entity_to_idx.end()) {
            candidate_entities.push_back(it->second);
        }
    }

    // If we don't have enough from index, add more
    if (candidate_entities.size() < max_candidates) {
        for (size_t i = 0; i < num_entities && candidate_entities.size() < max_candidates; ++i) {
            if (std::find(candidate_entities.begin(), candidate_entities.end(), i) == candidate_entities.end()) {
                candidate_entities.push_back(i);
            }
        }
    }

    // Score candidate links
    for (size_t h : candidate_entities) {
        for (size_t t : candidate_entities) {
            if (h == t) continue;

            for (size_t r = 0; r < num_relations; ++r) {
                // Skip if link already exists
                if (existing.count({h, r, t})) continue;

                Triple candidate{h, r, t};
                double dist = transe_score(model, candidate);

                // Convert distance to plausibility score (lower distance = higher score)
                // Use sigmoid-like transformation
                double plausibility = 1.0 / (1.0 + dist);

                if (plausibility >= config_.embedding_min_score) {
                    predictions.push_back({candidate, plausibility});
                }
            }
        }
    }

    // Sort by plausibility (descending)
    std::sort(predictions.begin(), predictions.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Keep top candidates
    if (predictions.size() > config_.embedding_max_candidates) {
        predictions.resize(config_.embedding_max_candidates);
    }

    return predictions;
}

std::vector<Insight> DiscoveryEngine::find_embedding_links() {
    std::vector<Insight> results;
    report_progress("Embedding link prediction", 0, 100);

    // Build embedding model
    EmbeddingModel model;

    // Extract triples from hypergraph
    report_progress("Extracting triples", 5, 100);
    std::vector<Triple> triples = extract_triples(model);

    if (triples.empty()) {
        report_progress("Embedding link prediction", 100, 100);
        return results;
    }

    size_t num_entities = model.entity_embeddings.size() > 0 ?
        model.entity_embeddings.size() : model.idx_to_entity.size();
    size_t num_relations = model.idx_to_relation.size();

    if (num_entities < 2 || num_relations < 1) {
        report_progress("Embedding link prediction", 100, 100);
        return results;
    }

    // Initialize embeddings
    report_progress("Initializing embeddings", 10, 100);
    init_embeddings(model, num_entities, num_relations);

    // Train TransE model
    train_transe(model, triples);

    // Build allowed relations by frequency
    std::unordered_set<std::string> allowed_relations;
    if (config_.embedding_allowed_relations_top_k > 0) {
        std::vector<std::pair<std::string, size_t>> rel_counts;
        rel_counts.reserve(index_.relation_to_edges.size());
        for (const auto& [rel, edges] : index_.relation_to_edges) {
            rel_counts.emplace_back(rel, edges.size());
        }
        std::sort(rel_counts.begin(), rel_counts.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < std::min(config_.embedding_allowed_relations_top_k, rel_counts.size()); ++i) {
            allowed_relations.insert(to_lower_copy(rel_counts[i].first));
        }
    }

    // Predict missing links
    report_progress("Predicting links", 90, 100);
    auto predictions = predict_links(model, triples);

    // Convert predictions to insights
    for (const auto& [pred_triple, plausibility] : predictions) {
        if (results.size() >= config_.embedding_max_candidates) break;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::EMBEDDING_LINK);
        ins.type = InsightType::EMBEDDING_LINK;

        // Get entity and relation labels
        const std::string& head_id = model.idx_to_entity[pred_triple.head];
        const std::string& tail_id = model.idx_to_entity[pred_triple.tail];
        const std::string& rel_label = model.idx_to_relation[pred_triple.relation];
        const std::string rel_lower = to_lower_copy(rel_label);

        if (!allowed_relations.empty() && allowed_relations.find(rel_lower) == allowed_relations.end()) {
            continue;
        }

        ins.seed_nodes = {head_id, tail_id};
        ins.seed_labels = {get_node_label(head_id), get_node_label(tail_id)};

        if (rel_lower.find("co-auth") != std::string::npos) {
            bool head_person = looks_like_person(ins.seed_labels[0]);
            bool tail_person = looks_like_person(ins.seed_labels[1]);
            bool head_work = looks_like_work(ins.seed_labels[0]);
            bool tail_work = looks_like_work(ins.seed_labels[1]);
            if (!((head_person && tail_person) || (head_person && tail_work) || (tail_person && head_work))) {
                continue;
            }
        }

        // Find witness edges (edges containing either head or tail)
        std::set<std::string> witness_set;
        if (const auto* h_node = graph_.get_node(head_id)) {
            for (const auto& eid : h_node->incident_edges) {
                witness_set.insert(eid);
                if (witness_set.size() >= 10) break;
            }
        }
        if (const auto* t_node = graph_.get_node(tail_id)) {
            for (const auto& eid : t_node->incident_edges) {
                witness_set.insert(eid);
                if (witness_set.size() >= 20) break;
            }
        }
        ins.witness_edges = std::vector<std::string>(witness_set.begin(), witness_set.end());
        if (ins.witness_edges.size() < config_.embedding_min_evidence_edges) {
            continue;
        }
        ins.witness_nodes = ins.seed_nodes;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

        if (config_.embedding_min_neighbor_overlap > 0.0) {
            auto head_neighbors = collect_neighbors(graph_, head_id, 200);
            auto tail_neighbors = collect_neighbors(graph_, tail_id, 200);
            double overlap = jaccard_overlap(head_neighbors, tail_neighbors);
            if (overlap < config_.embedding_min_neighbor_overlap) {
                continue;
            }
        }

        // Build description
        std::stringstream desc;
        desc << "TransE predicts: " << ins.seed_labels[0]
             << " --[" << rel_label << "]--> " << ins.seed_labels[1]
             << " (plausibility: " << std::fixed << std::setprecision(3) << plausibility << ")";
        ins.description = desc.str();

        // Novelty tags
        ins.novelty_tags = {"embedding_link", "transe", "relation=" + rel_label};

        // Score breakdown
        ins.score_breakdown["support"] = static_cast<double>(witness_set.size());
        ins.score_breakdown["novelty"] = plausibility;  // TransE plausibility as novelty
        ins.score_breakdown["specificity"] = 1.0;  // TransE predictions are specific
        ins.score = compute_score(ins);

        results.push_back(std::move(ins));
    }

    // Sort by score
    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    report_progress("Embedding link prediction", 100, 100);
    return results;
}

// ============== COMMUNITY-AWARE CROSS-CLUSTER LINKS ==============
std::vector<Insight> DiscoveryEngine::find_community_links() {
    std::vector<Insight> results;
    report_progress("Community links", 0, 100);

    int s = config_.community_s_threshold;
    auto it = index_.s_components.find(s);
    if (it == index_.s_components.end() || it->second.size() < 2) {
        return results;
    }

    const auto& components = it->second;
    std::vector<std::vector<std::string>> component_nodes;
    component_nodes.reserve(components.size());

    // Build node lists per component
    for (const auto& comp_edges : components) {
        std::unordered_map<std::string, int> node_local_degree;
        for (const auto& eid : comp_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            for (const auto& src : edge->sources) node_local_degree[src]++;
            for (const auto& tgt : edge->targets) node_local_degree[tgt]++;
        }

        std::vector<std::pair<std::string, int>> ranked(node_local_degree.begin(), node_local_degree.end());
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        std::vector<std::string> top_nodes;
        for (size_t i = 0; i < std::min(config_.community_top_nodes_per_component, ranked.size()); ++i) {
            top_nodes.push_back(ranked[i].first);
        }
        component_nodes.push_back(std::move(top_nodes));
    }

    auto relation_signature = [&](const std::string& node_id, const std::set<std::string>& comp_edges) {
        std::unordered_set<std::string> rels;
        const auto* node = graph_.get_node(node_id);
        if (!node) return rels;
        for (const auto& eid : node->incident_edges) {
            if (!comp_edges.empty() && comp_edges.find(eid) == comp_edges.end()) continue;
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            rels.insert(to_lower_copy(edge->relation));
        }
        return rels;
    };

    size_t total_pairs = 0;
    for (size_t i = 0; i < component_nodes.size(); ++i) {
        for (size_t j = i + 1; j < component_nodes.size(); ++j) {
            total_pairs += component_nodes[i].size() * component_nodes[j].size();
        }
    }
    size_t checked = 0;

    for (size_t i = 0; i < component_nodes.size(); ++i) {
        for (size_t j = i + 1; j < component_nodes.size(); ++j) {
            const auto& comp_a = component_nodes[i];
            const auto& comp_b = component_nodes[j];
            const auto& comp_edges_a = components[i];
            const auto& comp_edges_b = components[j];

            for (const auto& a : comp_a) {
                for (const auto& b : comp_b) {
                    checked++;
                    if (checked % 200 == 0 || checked == total_pairs) {
                        int pct = 5 + static_cast<int>(90.0 * checked / std::max<size_t>(1, total_pairs));
                        report_progress("Community links", pct, 100);
                    }

                    if (index_.get_cooccurrence(a, b) > 0) continue;

                    auto rel_a = relation_signature(a, comp_edges_a);
                    auto rel_b = relation_signature(b, comp_edges_b);
                    double overlap = jaccard_overlap(rel_a, rel_b);
                    if (overlap < config_.community_min_relation_overlap) continue;

                    Insight ins;
                    ins.insight_id = make_insight_id(InsightType::COMMUNITY_LINK);
                    ins.type = InsightType::COMMUNITY_LINK;
                    ins.seed_nodes = {a, b};
                    std::string label_a = get_node_label(a);
                    std::string label_b = get_node_label(b);
                    ins.seed_labels = {label_a.empty() ? a : label_a, label_b.empty() ? b : label_b};

                    std::set<std::string> witness_set;
                    if (const auto* n = graph_.get_node(a)) {
                        for (const auto& eid : n->incident_edges) {
                            witness_set.insert(eid);
                            if (witness_set.size() >= 10) break;
                        }
                    }
                    if (const auto* n = graph_.get_node(b)) {
                        for (const auto& eid : n->incident_edges) {
                            witness_set.insert(eid);
                            if (witness_set.size() >= 20) break;
                        }
                    }

                    ins.witness_edges = std::vector<std::string>(witness_set.begin(), witness_set.end());
                    if (ins.witness_edges.size() < 2) continue;
                    ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);

                    std::stringstream desc;
                    desc << "Community link: " << ins.seed_labels[0] << " <-> " << ins.seed_labels[1]
                         << " (relation overlap=" << std::fixed << std::setprecision(2) << overlap << ")";
                    ins.description = desc.str();
                    ins.novelty_tags = {"community_link", "s=" + std::to_string(s)};

                    ins.score_breakdown["support"] = static_cast<double>(ins.witness_edges.size());
                    ins.score_breakdown["novelty"] = overlap;
                    ins.score_breakdown["specificity"] = 1.0;
                    ins.score = compute_score(ins);

                    results.push_back(std::move(ins));
                    if (results.size() >= config_.community_max_candidates) {
                        report_progress("Community links", 100, 100);
                        return results;
                    }
                }
            }
        }
    }

    report_progress("Community links", 100, 100);
    return results;
}

std::vector<Insight> DiscoveryEngine::find_author_reference_chains() {
    std::vector<Insight> insights;
    std::unordered_map<std::string, std::unordered_set<std::string>> adjacency;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> support_edges;

    for (const auto& edge : graph_.get_all_edges()) {
        if (!looks_like_reference_relation(edge.relation)) continue;

        for (const auto& src : edge.sources) {
            if (!looks_like_person(get_node_label(src))) continue;
            for (const auto& tgt : edge.targets) {
                if (src == tgt) continue;
                if (!looks_like_person(get_node_label(tgt))) continue;
                adjacency[src].insert(tgt);
                support_edges[src][tgt].push_back(edge.id);
            }
        }
    }

    std::unordered_set<std::string> seen;
    for (const auto& [author, neighbors] : adjacency) {
        for (const auto& mid : neighbors) {
            auto it_mid = adjacency.find(mid);
            if (it_mid == adjacency.end()) continue;
            for (const auto& dst : it_mid->second) {
                if (author == dst || mid == dst) continue;
                std::string key = author + "->" + mid + "->" + dst;
                if (seen.find(key) != seen.end()) continue;
                seen.insert(key);

                const auto& edges_ab = support_edges[author][mid];
                const auto& edges_bc = support_edges[mid][dst];
                if (edges_ab.empty() || edges_bc.empty()) continue;

                std::unordered_set<std::string> witness_set(edges_ab.begin(), edges_ab.end());
                witness_set.insert(edges_bc.begin(), edges_bc.end());

                Insight ins;
                ins.insight_id = make_insight_id(InsightType::AUTHOR_CHAIN);
                ins.type = InsightType::AUTHOR_CHAIN;
                ins.seed_nodes = {author, mid, dst};
                ins.seed_labels = {get_node_label(author), get_node_label(mid), get_node_label(dst)};
                ins.witness_edges.assign(witness_set.begin(), witness_set.end());
                ins.witness_nodes = ins.seed_nodes;
                ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
                ins.score_breakdown["support"] =
                    (static_cast<double>(edges_ab.size()) + static_cast<double>(edges_bc.size())) / 2.0;
                ins.score = ins.score_breakdown["support"];
                ins.description = "Reference chain: " + ins.seed_labels[0] + " -> " +
                                  ins.seed_labels[1] + " -> " + ins.seed_labels[2];

                insights.push_back(std::move(ins));
                if (insights.size() >= config_.author_chain_max_candidates) {
                    return insights;
                }
            }
        }
    }

    return insights;
}

// ============== RUN MULTIPLE OPERATORS ==============
InsightCollection DiscoveryEngine::run_operators(const std::vector<std::string>& operators) {
    InsightCollection collection;
    collection.run_id = run_id_;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    collection.created_utc = ss.str();

    for (const auto& op : operators) {
        std::vector<Insight> insights;

        bool is_author_chain_op = (op == "author_chain" || op == "authorchain" || op == "author-chains");

        if (op == "bridges" || op == "bridge") {
            insights = find_bridges();
        } else if (op == "completions" || op == "completion") {
            insights = find_completions();
        } else if (op == "motifs" || op == "motif") {
            insights = find_motifs();
        } else if (op == "substitutions" || op == "substitution") {
            insights = find_substitutions();
        } else if (op == "contradictions" || op == "contradiction") {
            insights = find_contradictions();
        } else if (op == "entity_resolution" || op == "entity-resolution" || op == "entityresolution" || op == "dedup") {
            insights = find_entity_resolutions();
        } else if (op == "core_periphery" || op == "core-periphery" || op == "coreperiphery" ||
                   op == "hub_authority" || op == "hub-authority") {
            insights = find_core_periphery();
        } else if (op == "text_similarity" || op == "text-similarity" || op == "textsimilarity" ||
                   op == "semantic" || op == "semantic_similarity") {
            insights = find_text_similarity_links();
        } else if (op == "argument_support" || op == "argument-support" || op == "argument") {
            insights = find_argument_support_relations();
        } else if (op == "active_learning" || op == "active-learning" || op == "active") {
            insights = find_active_learning_queries();
        } else if (op == "method_outcome" || op == "method-outcome" || op == "method" || op == "outcome") {
            insights = find_method_outcome_nodes();
        } else if (op == "centrality" || op == "centrality_rank" || op == "centrality_rankings") {
            insights = find_centrality_nodes();
        } else if (op == "community_detection" || op == "community-detection" || op == "communities") {
            insights = find_community_structures();
        } else if (op == "k_core" || op == "k-core" || op == "core") {
            insights = find_k_core_nodes();
        } else if (op == "k_truss" || op == "k-truss" || op == "truss") {
            insights = find_k_truss_edges();
        } else if (op == "claim_stance" || op == "claim-stance" || op == "stance") {
            insights = find_claim_stances();
        } else if (op == "relation_induction" || op == "relation-induction" || op == "relation_type") {
            insights = find_relation_induction();
        } else if (op == "analogical_transfer" || op == "analogical-transfer" || op == "analogy") {
            insights = find_analogical_transfers();
        } else if (op == "uncertainty_sampling" || op == "uncertainty-sampling" || op == "uncertainty") {
            insights = find_uncertainty_samples();
        } else if (op == "counterfactual" || op == "counterfactual-probing") {
            insights = find_counterfactual_probes();
        } else if (op == "hyperedge_prediction" || op == "hyperedge-prediction" || op == "hyperedge") {
            insights = find_hyperedge_predictions();
        } else if (op == "constrained_rule" || op == "constrained-rule" || op == "rule_constrained") {
            insights = find_constrained_rules();
        } else if (op == "diffusion" || op == "diffusions") {
            insights = find_diffusions();
        } else if (op == "surprise" || op == "surprises") {
            insights = find_surprise_edges();
        } else if (op == "rules" || op == "rule") {
            insights = find_rules();
        } else if (op == "community" || op == "community_link" || op == "community-links") {
            insights = find_community_links();
        } else if (op == "hypothesis" || op == "hypotheses") {
            insights = find_hypotheses(collection);
        } else if (op == "pathrank" || op == "path_rank" || op == "path-ranking") {
            insights = find_path_rankings();
        } else if (op == "embedding" || op == "embedding_link" || op == "transe" || op == "embeddings") {
            insights = find_embedding_links();
        } else if (is_author_chain_op) {
            insights = find_author_reference_chains();
        }

        if (!is_author_chain_op) {
            insights.erase(
                std::remove_if(insights.begin(), insights.end(),
                               [this](const Insight& ins) { return is_author_reference_insight(ins); }),
                insights.end());
        }

        if (config_.adaptive_thresholds && config_.target_insights_per_operator > 0) {
            insights = select_by_target(insights, config_.target_insights_per_operator);
        }

        collection.insights.insert(collection.insights.end(),
                                  insights.begin(), insights.end());
    }

    std::sort(collection.insights.begin(), collection.insights.end(),
        [](const auto& a, const auto& b) { return a.score > b.score; });

    if (config_.adaptive_thresholds && config_.target_total_insights > 0) {
        collection.insights = select_by_target(collection.insights, config_.target_total_insights);
    } else if (collection.insights.size() > config_.max_total_insights) {
        collection.insights.resize(config_.max_total_insights);
    }

    return collection;
}

InsightCollection DiscoveryEngine::run_all() {
    return run_operators({"bridges", "completions", "motifs", "substitutions", "contradictions",
                          "entity_resolution", "core_periphery", "text_similarity", "argument_support",
                          "active_learning", "method_outcome", "centrality", "community_detection",
                          "k_core", "k_truss", "claim_stance", "relation_induction", "analogical_transfer",
                          "uncertainty_sampling", "counterfactual", "hyperedge_prediction",
                          "diffusion", "surprise", "rules", "community",
                          "pathrank", "embedding", "author_chain", "hypotheses"});
}

} // namespace kg

#include "discovery/discovery_engine.hpp"
#include "discovery/operator_registry.hpp"
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
#include <tuple>

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

bool looks_like_authorship_relation(const std::string& relation) {
    if (relation.empty()) return false;
    std::string lower = to_lower_copy(relation);
    return lower.find("author") != std::string::npos ||
           lower.find("wrote") != std::string::npos ||
           lower.find("written by") != std::string::npos ||
           lower.find("published by") != std::string::npos ||
           lower.find("contributor") != std::string::npos;
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

bool is_mechanistic_relation_text(const std::string& relation) {
    if (relation.empty()) return false;
    std::string rel = to_lower_copy(relation);
    for (auto& c : rel) {
        if (c == '_' || c == '-') c = ' ';
    }
    static const std::vector<std::string> keywords = {
        "cause", "causal", "lead", "result", "trigger", "induce", "drive", "mediate",
        "mechanis", "pathway", "regulat", "activate", "inhibit", "modulat", "effect", "impact"
    };
    for (const auto& kw : keywords) {
        if (rel.find(kw) != std::string::npos) return true;
    }
    return false;
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

std::vector<std::string> DiscoveryEngine::get_source_documents(const std::vector<std::string>& edge_ids) const {
    std::unordered_set<std::string> docs;
    for (const auto& eid : edge_ids) {
        const auto* edge = graph_.get_hyperedge(eid);
        if (edge && !edge->source_document.empty()) {
            docs.insert(edge->source_document);
        }
    }
    return std::vector<std::string>(docs.begin(), docs.end());
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
        ins.set_type(InsightType::BRIDGE);
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
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::COMPLETION);
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
        ins.source_documents = get_source_documents(edges);
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
        ins.set_type(InsightType::MOTIF);
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
        ins.set_type(InsightType::SUBSTITUTION);
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
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::CONTRADICTION);

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
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::ENTITY_RESOLUTION);
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
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::CORE_PERIPHERY);
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
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::TEXT_SIMILARITY);
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
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::ARGUMENT_SUPPORT);
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
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::ACTIVE_LEARNING);
        ins.seed_nodes = cand.nodes;
        ins.seed_labels = {};
        for (const auto& n : cand.nodes) {
            ins.seed_labels.push_back(get_node_label(n));
        }
        ins.witness_edges = {cand.edge_id};
        ins.witness_nodes = cand.nodes;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::METHOD_OUTCOME);
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
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::CENTRALITY);
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
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::COMMUNITY_DETECTION);

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
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::K_CORE);
        ins.seed_nodes = {node_id};
        ins.seed_labels = {get_node_label(node_id)};

        auto incident = graph_.get_incident_edges(node_id);
        for (size_t j = 0; j < incident.size() && j < config_.centrality_max_evidence_edges; ++j) {
            ins.witness_edges.push_back(incident[j].id);
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::K_TRUSS);
        ins.seed_nodes = {a, b};
        ins.seed_labels = {get_node_label(a), get_node_label(b)};

        ins.witness_edges = shared_incident_edges(graph_, a, b, config_.centrality_max_evidence_edges);
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::CLAIM_STANCE);
        ins.seed_nodes = {edge.sources[0], edge.targets[0]};
        ins.seed_labels = {get_node_label(edge.sources[0]), get_node_label(edge.targets[0])};
        ins.witness_edges = {edge.id};
        ins.witness_nodes = {edge.sources[0], edge.targets[0]};
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::RELATION_INDUCTION);
        if (!list.empty() && !list[0]->sources.empty() && !list[0]->targets.empty()) {
            ins.seed_nodes = {list[0]->sources[0], list[0]->targets[0]};
            ins.seed_labels = {get_node_label(list[0]->sources[0]), get_node_label(list[0]->targets[0])};
            ins.witness_nodes = ins.seed_nodes;
        }
        for (size_t j = 0; j < ex_limit; ++j) {
            ins.witness_edges.push_back(list[j]->id);
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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

// REMOVED: Low-value insight (2 insights across 9 runs)
#if 0
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
                ins.set_type(InsightType::ANALOGICAL_TRANSFER);
                ins.seed_nodes = {a, d};
                ins.seed_labels = {get_node_label(a), get_node_label(d)};
                ins.witness_edges = {e1->id, e2->id};
                ins.witness_nodes = {a, b, c, d};
                ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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
#endif // REMOVED: analogical_transfer

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
        ins.set_type(InsightType::UNCERTAINTY_SAMPLING);
        ins.seed_nodes = {cand.src, cand.tgt};
        ins.seed_labels = {get_node_label(cand.src), get_node_label(cand.tgt)};
        ins.witness_edges = {cand.edge_id};
        ins.witness_nodes = {cand.src, cand.tgt};
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::COUNTERFACTUAL);
        ins.seed_nodes = {src, tgt};
        ins.seed_labels = {get_node_label(src), get_node_label(tgt)};
        ins.witness_edges = {edge.id};
        ins.witness_nodes = {src, tgt};
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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
        ins.set_type(InsightType::HYPEREDGE_PREDICTION);
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
        ins.set_type(InsightType::DIFFUSION);
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
        ins.set_type(InsightType::SURPRISE);
        ins.seed_nodes = std::vector<std::string>(entities.begin(), entities.end());
        for (const auto& n : ins.seed_nodes) {
            ins.seed_labels.push_back(get_node_label(n));
        }
        ins.witness_edges = {edge_id};
        ins.witness_nodes = ins.seed_nodes;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

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
        ins.set_type(InsightType::RULE);

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
            ins.set_type(InsightType::PATH_RANK);
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
        ins.source_documents = get_source_documents(ins.witness_edges);
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

// ============== INTERSECTION-CONSTRAINED HYPOTHESIS BRIDGES ==============
std::vector<Insight> DiscoveryEngine::find_intersection_hypothesis_bridges() {
    std::vector<Insight> results;
    report_progress("Intersection-constrained traversal", 0, 100);

    std::vector<std::string> seed_nodes;
    if (!index_.degree_ranked_nodes.empty()) {
        size_t limit = std::min(config_.intersection_bridge_max_seed_nodes, index_.degree_ranked_nodes.size());
        for (size_t i = 0; i < limit; ++i) {
            seed_nodes.push_back(index_.degree_ranked_nodes[i].first);
        }
    } else {
        auto nodes = graph_.get_all_nodes();
        std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) {
            return a.degree > b.degree;
        });
        size_t limit = std::min(config_.intersection_bridge_max_seed_nodes, nodes.size());
        for (size_t i = 0; i < limit; ++i) {
            seed_nodes.push_back(nodes[i].id);
        }
    }

    if (seed_nodes.size() < 2) {
        report_progress("Intersection-constrained traversal", 100, 100);
        return results;
    }

    size_t total_pairs = seed_nodes.size() * (seed_nodes.size() - 1) / 2;
    size_t max_pairs = std::min(config_.intersection_bridge_max_pairs, total_pairs);
    if (max_pairs == 0) {
        report_progress("Intersection-constrained traversal", 100, 100);
        return results;
    }

    const int min_intersection = std::max(1, config_.intersection_bridge_min_intersection);
    const int k_paths = std::max(1, config_.intersection_bridge_k_paths);
    size_t checked = 0;

    auto find_shortest_intersection_paths =
        [&](const std::string& start_node, const std::string& end_node, int max_paths) {
            std::vector<std::vector<std::string>> collected_paths;
            if (!graph_.has_node(start_node) || !graph_.has_node(end_node) || max_paths <= 0) {
                return collected_paths;
            }

            auto start_edges = graph_.get_incident_edges(start_node);
            if (start_edges.empty()) return collected_paths;

            std::queue<std::string> q;
            std::unordered_map<std::string, int> depth;
            std::unordered_map<std::string, std::vector<std::string>> parents;
            std::set<std::string> goal_edges;
            int goal_depth = std::numeric_limits<int>::max();

            for (const auto& edge : start_edges) {
                q.push(edge.id);
                depth[edge.id] = 0;
                parents[edge.id] = {};
            }

            while (!q.empty()) {
                std::string current_id = q.front();
                q.pop();

                auto depth_it = depth.find(current_id);
                if (depth_it == depth.end()) continue;
                int d = depth_it->second;
                if (d > goal_depth) continue;

                const auto* current_edge = graph_.get_hyperedge(current_id);
                if (!current_edge) continue;

                if (current_edge->contains_node(end_node)) {
                    goal_depth = d;
                    goal_edges.insert(current_id);
                    continue;
                }
                if (d >= goal_depth) continue;

                std::set<std::string> candidate_neighbors;
                for (const auto& node_id : current_edge->get_all_nodes()) {
                    auto incident = graph_.get_incident_edges(node_id);
                    for (const auto& incident_edge : incident) {
                        if (incident_edge.id != current_id) {
                            candidate_neighbors.insert(incident_edge.id);
                        }
                    }
                }

                for (const auto& neighbor_id : candidate_neighbors) {
                    const auto* neighbor_edge = graph_.get_hyperedge(neighbor_id);
                    if (!neighbor_edge) continue;
                    if (static_cast<int>(current_edge->intersection(*neighbor_edge).size()) < min_intersection) {
                        continue;
                    }

                    int nd = d + 1;
                    auto nd_it = depth.find(neighbor_id);
                    if (nd_it == depth.end()) {
                        depth[neighbor_id] = nd;
                        parents[neighbor_id].push_back(current_id);
                        q.push(neighbor_id);
                    } else if (nd == nd_it->second) {
                        parents[neighbor_id].push_back(current_id);
                    }
                }
            }

            if (goal_edges.empty()) return collected_paths;

            std::vector<std::string> rev_path;
            std::function<void(const std::string&)> backtrack = [&](const std::string& edge_id) {
                if (collected_paths.size() >= static_cast<size_t>(max_paths)) return;
                rev_path.push_back(edge_id);

                auto pit = parents.find(edge_id);
                if (pit == parents.end() || pit->second.empty()) {
                    collected_paths.emplace_back(rev_path.rbegin(), rev_path.rend());
                } else {
                    for (const auto& parent_id : pit->second) {
                        if (collected_paths.size() >= static_cast<size_t>(max_paths)) break;
                        backtrack(parent_id);
                    }
                }

                rev_path.pop_back();
            };

            for (const auto& goal_edge_id : goal_edges) {
                if (collected_paths.size() >= static_cast<size_t>(max_paths)) break;
                backtrack(goal_edge_id);
            }

            std::set<std::string> seen;
            std::vector<std::vector<std::string>> unique_paths;
            for (const auto& path_ids : collected_paths) {
                std::ostringstream key;
                for (size_t i = 0; i < path_ids.size(); ++i) {
                    if (i > 0) key << "->";
                    key << path_ids[i];
                }
                if (seen.insert(key.str()).second) {
                    unique_paths.push_back(path_ids);
                }
            }
            return unique_paths;
        };

    for (size_t i = 0; i < seed_nodes.size(); ++i) {
        for (size_t j = i + 1; j < seed_nodes.size(); ++j) {
            if (checked >= max_pairs) break;
            checked++;

            if (checked % 50 == 0 || checked == max_pairs) {
                int pct = 5 + static_cast<int>(90.0 * checked / std::max<size_t>(1, max_pairs));
                report_progress("Intersection-constrained traversal", pct, 100);
            }

            const std::string& a = seed_nodes[i];
            const std::string& b = seed_nodes[j];

            // Semantically distant only; skip pairs that already co-occur directly.
            if (index_.get_cooccurrence(a, b) > 0 || nodes_share_edge(graph_, a, b)) {
                continue;
            }

            double semantic_similarity = std::clamp(label_token_similarity(graph_, a, b), 0.0, 1.0);
            double semantic_distance = 1.0 - semantic_similarity;
            if (semantic_distance < config_.intersection_bridge_min_semantic_distance) {
                continue;
            }

            auto shortest_paths = find_shortest_intersection_paths(a, b, k_paths);
            if (shortest_paths.empty()) {
                continue;
            }

            size_t valid_paths = 0;
            int shortest_len = std::numeric_limits<int>::max();
            double intersection_sum = 0.0;
            double grounding_sum = 0.0;
            double mechanistic_sum = 0.0;

            std::set<std::string> witness_edges_set;
            std::set<std::string> witness_nodes_set;
            std::unordered_map<std::string, int> bridge_hits;

            for (const auto& path_edge_ids : shortest_paths) {
                if (path_edge_ids.empty()) continue;
                if (static_cast<int>(path_edge_ids.size()) > config_.intersection_bridge_max_hops) continue;

                valid_paths++;
                shortest_len = std::min(shortest_len, static_cast<int>(path_edge_ids.size()));

                double path_confidence = 0.0;
                double path_mechanistic = 0.0;
                double path_intersection = 0.0;
                size_t pairwise_steps = 0;

                for (size_t p = 0; p < path_edge_ids.size(); ++p) {
                    const HyperEdge* edge_ptr = graph_.get_hyperedge(path_edge_ids[p]);
                    if (!edge_ptr) continue;
                    const HyperEdge& edge = *edge_ptr;

                    path_confidence += std::clamp(edge.confidence, 0.0, 1.0);
                    if (is_mechanistic_relation_text(edge.relation)) {
                        path_mechanistic += 1.0;
                    }

                    if (witness_edges_set.size() < config_.intersection_bridge_max_witness_edges) {
                        witness_edges_set.insert(edge.id);
                    }

                    if (witness_nodes_set.size() < config_.intersection_bridge_max_witness_nodes) {
                        for (const auto& src : edge.sources) {
                            if (witness_nodes_set.size() >= config_.intersection_bridge_max_witness_nodes) break;
                            witness_nodes_set.insert(src);
                        }
                    }
                    if (witness_nodes_set.size() < config_.intersection_bridge_max_witness_nodes) {
                        for (const auto& tgt : edge.targets) {
                            if (witness_nodes_set.size() >= config_.intersection_bridge_max_witness_nodes) break;
                            witness_nodes_set.insert(tgt);
                        }
                    }

                    if (p + 1 < path_edge_ids.size()) {
                        const HyperEdge* next_ptr = graph_.get_hyperedge(path_edge_ids[p + 1]);
                        if (!next_ptr) continue;
                        const HyperEdge& next_edge = *next_ptr;
                        auto intersection_nodes = edge.intersection(next_edge);
                        if (static_cast<int>(intersection_nodes.size()) < min_intersection) {
                            continue;
                        }
                        path_intersection += static_cast<double>(intersection_nodes.size());
                        pairwise_steps++;

                        for (const auto& node : intersection_nodes) {
                            bridge_hits[node]++;
                            if (witness_nodes_set.size() < config_.intersection_bridge_max_witness_nodes) {
                                witness_nodes_set.insert(node);
                            }
                        }
                    }
                }

                if (!path_edge_ids.empty()) {
                    path_confidence /= static_cast<double>(path_edge_ids.size());
                    path_mechanistic /= static_cast<double>(path_edge_ids.size());
                }

                double normalized_intersection = 0.0;
                if (pairwise_steps > 0) {
                    double avg_intersection = path_intersection / static_cast<double>(pairwise_steps);
                    normalized_intersection = std::min(
                        1.0,
                        avg_intersection / static_cast<double>(std::max(1, min_intersection))
                    );
                }

                grounding_sum += path_confidence;
                mechanistic_sum += path_mechanistic;
                intersection_sum += normalized_intersection;
            }

            if (valid_paths == 0 || bridge_hits.empty() || witness_edges_set.empty()) {
                continue;
            }

            double path_support = std::min(1.0, static_cast<double>(valid_paths) / static_cast<double>(k_paths));
            double intersection_strength = intersection_sum / static_cast<double>(valid_paths);
            double grounding = grounding_sum / static_cast<double>(valid_paths);
            double mechanistic = mechanistic_sum / static_cast<double>(valid_paths);
            double parsimony = shortest_len > 0 ? (1.0 / static_cast<double>(shortest_len)) : 0.0;

            double score =
                0.30 * semantic_distance +
                0.25 * intersection_strength +
                0.20 * grounding +
                0.15 * mechanistic +
                0.10 * path_support;
            score = 0.85 * score + 0.15 * parsimony;

            if (score < config_.intersection_bridge_min_score) {
                continue;
            }

            std::vector<std::pair<std::string, int>> ranked_bridges(bridge_hits.begin(), bridge_hits.end());
            std::sort(ranked_bridges.begin(), ranked_bridges.end(),
                      [](const auto& lhs, const auto& rhs) {
                          if (lhs.second != rhs.second) return lhs.second > rhs.second;
                          return lhs.first < rhs.first;
                      });

            Insight ins;
            ins.insight_id = make_insight_id(InsightType::HYPOTHESES_2);
            ins.set_type(InsightType::HYPOTHESES_2);
            ins.seed_nodes = {a, b};

            std::string label_a = get_node_label(a);
            std::string label_b = get_node_label(b);
            ins.seed_labels = {label_a.empty() ? a : label_a, label_b.empty() ? b : label_b};

            auto push_witness_node = [&](const std::string& node_id) {
                if (ins.witness_nodes.size() >= config_.intersection_bridge_max_witness_nodes) return;
                if (std::find(ins.witness_nodes.begin(), ins.witness_nodes.end(), node_id) != ins.witness_nodes.end()) return;
                ins.witness_nodes.push_back(node_id);
            };

            push_witness_node(a);
            for (const auto& [bridge_node, _count] : ranked_bridges) {
                if (bridge_node == a || bridge_node == b) continue;
                push_witness_node(bridge_node);
                if (ins.witness_nodes.size() >= config_.intersection_bridge_max_witness_nodes - 1) break;
            }
            push_witness_node(b);
            for (const auto& node : witness_nodes_set) {
                if (ins.witness_nodes.size() >= config_.intersection_bridge_max_witness_nodes) break;
                push_witness_node(node);
            }

            ins.witness_edges.assign(witness_edges_set.begin(), witness_edges_set.end());
            ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

            std::ostringstream desc;
            desc << "Intersection-constrained mechanistic hypothesis (IS>=" << min_intersection
                 << "): '" << ins.seed_labels[0] << "' may influence '" << ins.seed_labels[1]
                 << "' through " << valid_paths << " higher-order pathway(s)";
            if (!ranked_bridges.empty()) {
                desc << " via bridge node(s) ";
                for (size_t r = 0; r < std::min<size_t>(3, ranked_bridges.size()); ++r) {
                    if (r > 0) desc << ", ";
                    desc << "'" << get_node_label(ranked_bridges[r].first) << "'";
                }
                if (ranked_bridges.size() > 3) desc << " and others";
            }
            desc << ".";
            ins.description = desc.str();

            ins.score_breakdown["semantic_distance"] = semantic_distance;
            ins.score_breakdown["intersection_strength"] = intersection_strength;
            ins.score_breakdown["grounding"] = grounding;
            ins.score_breakdown["mechanistic_support"] = mechanistic;
            ins.score_breakdown["path_support"] = path_support;
            ins.score_breakdown["path_parsimony"] = parsimony;
            ins.score_breakdown["bridge_nodes"] = static_cast<double>(bridge_hits.size());
            ins.score = score;
            ins.novelty_tags = {
                "intersection_bridge",
                "is>=" + std::to_string(min_intersection),
                "paths=" + std::to_string(valid_paths)
            };

            results.push_back(std::move(ins));
        }
        if (checked >= max_pairs) break;
    }

    std::sort(results.begin(), results.end(), [](const Insight& lhs, const Insight& rhs) {
        return lhs.score > rhs.score;
    });
    if (results.size() > config_.intersection_bridge_count) {
        results.resize(config_.intersection_bridge_count);
    }

    report_progress("Intersection-constrained traversal", 100, 100);
    return results;
}

// ============== LONG-CHAIN REASONING (MULTI-HOP PATHS) ==============
std::vector<Insight> DiscoveryEngine::find_long_chains() {
    std::vector<Insight> results;
    report_progress("Long-chain reasoning", 0, 100);

    struct EdgeInfo {
        std::string tgt;
        std::string edge_id;
        std::string relation;
        double confidence;
    };

    auto normalize_text = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        for (auto& c : s) {
            if (c == '_' || c == '-') c = ' ';
        }
        return s;
    };

    std::unordered_map<std::string, std::vector<EdgeInfo>> adj;
    std::unordered_map<std::string, std::string> edge_chunk;
    std::unordered_map<std::string, double> edge_confidence;

    for (const auto& edge : graph_.get_all_edges()) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        std::string rel = normalize_text(edge.relation);
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                adj[src].push_back(EdgeInfo{tgt, edge.id, rel, edge.confidence});
                edge_confidence[edge.id] = edge.confidence;
            }
        }
        if (!edge.source_chunk_id.empty()) {
            edge_chunk[edge.id] = edge.source_chunk_id;
        }
    }

    if (adj.empty()) {
        report_progress("Long-chain reasoning", 100, 100);
        return results;
    }

    std::vector<std::string> seeds;
    if (config_.long_chain_max_seed_nodes > 0) {
        auto hubs = graph_.get_top_hubs(static_cast<int>(config_.long_chain_max_seed_nodes));
        for (const auto& [node_id, _] : hubs) {
            seeds.push_back(node_id);
        }
    } else {
        auto nodes = graph_.get_all_nodes();
        for (const auto& node : nodes) seeds.push_back(node.id);
    }

    if (seeds.empty()) {
        report_progress("Long-chain reasoning", 100, 100);
        return results;
    }

    int min_hops = std::max(2, config_.long_chain_min_hops);
    int max_hops = std::max(min_hops, config_.long_chain_max_hops);

    struct ChainCandidate {
        std::vector<std::string> nodes;
        std::vector<std::string> edge_ids;
        std::vector<std::string> relations;
        std::string pattern_key;
        int hops = 0;
        double avg_confidence = 0.0;
        double length_score = 0.0;
        double support = 0.0;
        double novelty = 0.0;
        double final_score = 0.0;
    };

    std::vector<ChainCandidate> candidates;
    std::unordered_map<std::string, int> pattern_counts;
    std::unordered_set<std::string> seen_paths;

    auto join_tokens = [](const std::vector<std::string>& parts, const std::string& sep) {
        std::ostringstream ss;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) ss << sep;
            ss << parts[i];
        }
        return ss.str();
    };

    bool stop = false;
    for (const auto& seed : seeds) {
        if (stop) break;
        std::vector<std::string> path_nodes = {seed};
        std::vector<std::string> path_edges;
        std::vector<std::string> path_rels;
        std::unordered_set<std::string> visited = {seed};

        std::function<void(const std::string&)> dfs = [&](const std::string& node) {
            if (stop) return;
            if (static_cast<int>(path_rels.size()) >= max_hops) return;

            auto it = adj.find(node);
            if (it == adj.end()) return;
            const auto& edges = it->second;
            size_t degree_checked = 0;

            for (const auto& e : edges) {
                if (degree_checked++ >= config_.long_chain_degree_cap) break;
                if (visited.count(e.tgt)) continue;

                visited.insert(e.tgt);
                path_nodes.push_back(e.tgt);
                path_edges.push_back(e.edge_id);
                path_rels.push_back(e.relation);

                int hops = static_cast<int>(path_rels.size());
                if (hops >= min_hops) {
                    std::string path_key = join_tokens(path_nodes, "->");
                    if (seen_paths.insert(path_key).second) {
                        ChainCandidate cand;
                        cand.nodes = path_nodes;
                        cand.edge_ids = path_edges;
                        cand.relations = path_rels;
                        cand.hops = hops;
                        cand.pattern_key = join_tokens(path_rels, "->");
                        double conf_sum = 0.0;
                        for (const auto& edge_id : path_edges) {
                            auto conf_it = edge_confidence.find(edge_id);
                            if (conf_it != edge_confidence.end()) {
                                conf_sum += conf_it->second;
                            }
                        }
                        cand.avg_confidence = path_edges.empty() ? 0.0 : (conf_sum / path_edges.size());
                        pattern_counts[cand.pattern_key]++;
                        candidates.push_back(std::move(cand));
                        if (candidates.size() >= config_.long_chain_max_candidates) {
                            stop = true;
                        }
                    }
                }

                if (!stop) {
                    dfs(e.tgt);
                }

                path_rels.pop_back();
                path_edges.pop_back();
                path_nodes.pop_back();
                visited.erase(e.tgt);

                if (stop) break;
            }
        };

        dfs(seed);
    }

    if (candidates.empty()) {
        report_progress("Long-chain reasoning", 100, 100);
        return results;
    }

    int max_pattern = 0;
    for (const auto& entry : pattern_counts) {
        if (entry.second > max_pattern) max_pattern = entry.second;
    }

    for (auto& cand : candidates) {
        cand.support = (max_pattern > 0) ? static_cast<double>(pattern_counts[cand.pattern_key]) / max_pattern : 0.0;
        cand.novelty = 1.0 - cand.support;
        if (max_hops > min_hops) {
            cand.length_score = static_cast<double>(cand.hops - min_hops + 1) /
                                static_cast<double>(max_hops - min_hops + 1);
        } else {
            cand.length_score = 1.0;
        }

        cand.final_score =
            config_.long_chain_weight_confidence * cand.avg_confidence +
            config_.long_chain_weight_length * cand.length_score +
            config_.long_chain_weight_novelty * cand.novelty;
    }

    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [this](const ChainCandidate& c) {
            return c.avg_confidence < config_.long_chain_min_avg_confidence;
        }), candidates.end());

    std::sort(candidates.begin(), candidates.end(), [](const ChainCandidate& a, const ChainCandidate& b) {
        return a.final_score > b.final_score;
    });

    size_t limit = std::min(candidates.size(), config_.long_chain_max_candidates);
    for (size_t i = 0; i < limit; ++i) {
        const auto& cand = candidates[i];
        if (cand.nodes.size() < 2) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::LONG_CHAIN);
        ins.set_type(InsightType::LONG_CHAIN);
        ins.seed_nodes = {cand.nodes.front(), cand.nodes.back()};
        ins.seed_labels = {get_node_label(cand.nodes.front()), get_node_label(cand.nodes.back())};
        ins.witness_nodes = cand.nodes;
        ins.witness_edges = cand.edge_ids;

        std::ostringstream desc;
        desc << "Long-chain (" << cand.hops << " hops): ";
        for (size_t n = 0; n < cand.nodes.size(); ++n) {
            if (n > 0 && n - 1 < cand.relations.size()) {
                desc << " --" << cand.relations[n - 1] << "--> ";
            }
            desc << "'" << get_node_label(cand.nodes[n]) << "'";
        }
        ins.description = desc.str();

        ins.score_breakdown["avg_confidence"] = cand.avg_confidence;
        ins.score_breakdown["length_score"] = cand.length_score;
        ins.score_breakdown["pattern_support"] = cand.support;
        ins.score_breakdown["novelty"] = cand.novelty;
        ins.score_breakdown["hops"] = static_cast<double>(cand.hops);
        ins.score = cand.final_score;
        ins.novelty_tags = {"long_chain", "hops=" + std::to_string(cand.hops)};

        std::unordered_set<std::string> chunks;
        for (const auto& edge_id : ins.witness_edges) {
            auto it = edge_chunk.find(edge_id);
            if (it != edge_chunk.end()) chunks.insert(it->second);
        }
        for (const auto& chunk : chunks) ins.evidence_chunk_ids.push_back(chunk);

        results.push_back(std::move(ins));
    }

    report_progress("Long-chain reasoning", 100, 100);
    return results;
}

// ============== HYPOTHESIS SYNTHESIS ==============

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
        ins.set_type(InsightType::EMBEDDING_LINK);

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
        ins.source_documents = get_source_documents(ins.witness_edges);

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
                    ins.set_type(InsightType::COMMUNITY_LINK);
                    ins.seed_nodes = {a, b};
                    std::string label_a = get_node_label(a);
                    std::string label_b = get_node_label(b);
                    ins.seed_labels = {label_a.empty() ? a : label_a, label_b.empty() ? b : label_b};
                    ins.witness_nodes = ins.seed_nodes;

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
        ins.source_documents = get_source_documents(ins.witness_edges);

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
                ins.set_type(InsightType::AUTHOR_CHAIN);
                ins.seed_nodes = {author, mid, dst};
                ins.seed_labels = {get_node_label(author), get_node_label(mid), get_node_label(dst)};
                ins.witness_edges.assign(witness_set.begin(), witness_set.end());
                ins.witness_nodes = ins.seed_nodes;
                ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
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

// REMOVED: Low-value insight (1 insight across 9 runs)
#if 0
// ============== CO-AUTHORSHIP NETWORKS ==============
std::vector<Insight> DiscoveryEngine::find_co_authorship_networks() {
    std::vector<Insight> insights;

    // Map: (author1, author2) -> number of shared works/documents
    std::map<std::pair<std::string, std::string>, int> collaboration_counts;
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> collaboration_edges;
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> collaboration_docs;

    // METHOD 1: Explicit co-authorship edges in the graph
    for (const auto& edge : graph_.get_all_edges()) {
        if (!looks_like_authorship_relation(edge.relation)) continue;

        // Check for co-authored edges: author1 <-[co-authored]-> author2
        if (edge.sources.size() >= 2) {
            for (size_t i = 0; i < edge.sources.size(); ++i) {
                for (size_t j = i + 1; j < edge.sources.size(); ++j) {
                    const auto& author1 = edge.sources[i];
                    const auto& author2 = edge.sources[j];
                    if (!looks_like_person(get_node_label(author1)) ||
                        !looks_like_person(get_node_label(author2))) continue;

                    auto key = author1 < author2 ? std::make_pair(author1, author2)
                                                  : std::make_pair(author2, author1);
                    collaboration_counts[key]++;
                    collaboration_edges[key].push_back(edge.id);
                }
            }
        }

        // Check for shared works: both authors connect to same work
        // author1 -[authored]-> work <-[authored]- author2
        for (const auto& work : edge.targets) {
            if (!looks_like_work(get_node_label(work))) continue;

            std::vector<std::string> work_authors;
            for (const auto& src : edge.sources) {
                if (looks_like_person(get_node_label(src))) {
                    work_authors.push_back(src);
                }
            }

            for (size_t i = 0; i < work_authors.size(); ++i) {
                for (size_t j = i + 1; j < work_authors.size(); ++j) {
                    const auto& author1 = work_authors[i];
                    const auto& author2 = work_authors[j];
                    auto key = author1 < author2 ? std::make_pair(author1, author2)
                                                  : std::make_pair(author2, author1);
                    collaboration_counts[key]++;
                    collaboration_edges[key].push_back(edge.id);
                }
            }
        }
    }

    // METHOD 2: Document co-occurrence (authors appearing together in same chunks)
    std::unordered_map<std::string, std::unordered_set<std::string>> author_documents;

    // First pass: build author->documents map
    for (const auto& edge : graph_.get_all_edges()) {
        for (const auto& src : edge.sources) {
            if (looks_like_person(get_node_label(src))) {
                // Get chunk IDs for this edge to determine documents
                auto chunks = get_chunk_ids({edge.id});
                for (const auto& chunk : chunks) {
                    author_documents[src].insert(chunk);
                }
            }
        }
        for (const auto& tgt : edge.targets) {
            if (looks_like_person(get_node_label(tgt))) {
                auto chunks = get_chunk_ids({edge.id});
                for (const auto& chunk : chunks) {
                    author_documents[tgt].insert(chunk);
                }
            }
        }
    }

    // Second pass: find co-authorship (authors appearing in same documents)
    std::vector<std::string> authors;
    for (const auto& [author, docs] : author_documents) {
        authors.push_back(author);
    }

    for (size_t i = 0; i < authors.size(); ++i) {
        for (size_t j = i + 1; j < authors.size(); ++j) {
            const auto& author1 = authors[i];
            const auto& author2 = authors[j];
            const auto& docs1 = author_documents[author1];
            const auto& docs2 = author_documents[author2];

            // Find shared documents
            std::vector<std::string> shared_docs;
            for (const auto& doc : docs1) {
                if (docs2.find(doc) != docs2.end()) {
                    shared_docs.push_back(doc);
                }
            }

            if (shared_docs.size() > 0) {
                auto key = author1 < author2 ? std::make_pair(author1, author2)
                                              : std::make_pair(author2, author1);
                collaboration_counts[key] += static_cast<int>(shared_docs.size());
                collaboration_docs[key] = shared_docs;
            }
        }
    }

    // Sort by collaboration strength
    std::vector<std::pair<std::pair<std::string, std::string>, int>> sorted_collabs(
        collaboration_counts.begin(), collaboration_counts.end());
    std::sort(sorted_collabs.begin(), sorted_collabs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Generate insights for top collaborations
    for (const auto& [author_pair, count] : sorted_collabs) {
        if (count < config_.co_authorship_min_collaborations) continue;
        if (insights.size() >= config_.co_authorship_max_candidates) break;

        const auto& author1 = author_pair.first;
        const auto& author2 = author_pair.second;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CO_AUTHORSHIP);
        ins.set_type(InsightType::CO_AUTHORSHIP);
        ins.seed_nodes = {author1, author2};
        ins.seed_labels = {get_node_label(author1), get_node_label(author2)};

        // Add witness edges from graph
        if (collaboration_edges.find(author_pair) != collaboration_edges.end()) {
            ins.witness_edges = collaboration_edges[author_pair];
        }

        // Add evidence from documents
        if (collaboration_docs.find(author_pair) != collaboration_docs.end()) {
            ins.evidence_chunk_ids = collaboration_docs[author_pair];
        }

        ins.score_breakdown["collaborations"] = static_cast<double>(count);
        ins.score = std::min(static_cast<double>(count) / 5.0, 1.0); // Normalize score
        ins.description = "Co-authors: " + ins.seed_labels[0] + " & " +
                         ins.seed_labels[1] + " (" + std::to_string(count) + " joint works)";

        insights.push_back(std::move(ins));
    }

    return insights;
}
#endif // REMOVED: co_authorship

// ============== CITATION IMPACT ANALYSIS ==============
std::vector<Insight> DiscoveryEngine::find_citation_impact() {
    std::vector<Insight> insights;

    // Map: author/work -> incoming citation/authorship count
    std::unordered_map<std::string, int> impact_counts;
    std::unordered_map<std::string, std::vector<std::string>> contributors;  // citers or authors
    std::unordered_map<std::string, std::string> impact_type;  // "citations" or "authorships"

    // Count incoming citations
    for (const auto& edge : graph_.get_all_edges()) {
        if (looks_like_reference_relation(edge.relation)) {
            // edge.sources cite edge.targets
            for (const auto& cited : edge.targets) {
                if (looks_like_person(get_node_label(cited)) || looks_like_work(get_node_label(cited))) {
                    impact_counts[cited]++;
                    impact_type[cited] = "citations";
                    for (const auto& citer : edge.sources) {
                        if (looks_like_person(get_node_label(citer))) {
                            contributors[cited].push_back(citer);
                        }
                    }
                }
            }
        }
        // Also count authorship relations (highly authored works = influential)
        else if (looks_like_authorship_relation(edge.relation)) {
            // edge.sources authored edge.targets
            for (const auto& work : edge.targets) {
                if (looks_like_work(get_node_label(work))) {
                    impact_counts[work]++;
                    if (impact_type.find(work) == impact_type.end()) {
                        impact_type[work] = "authorships";
                    }
                    for (const auto& author : edge.sources) {
                        if (looks_like_person(get_node_label(author))) {
                            contributors[work].push_back(author);
                        }
                    }
                }
            }
        }
    }

    // Sort by impact count
    std::vector<std::pair<std::string, int>> sorted_impact(
        impact_counts.begin(), impact_counts.end());
    std::sort(sorted_impact.begin(), sorted_impact.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Generate insights for highly cited/authored entities
    size_t count = 0;
    for (const auto& [entity_id, impact_count] : sorted_impact) {
        if (count >= config_.citation_impact_top_k) break;
        if (impact_count < config_.citation_impact_min_citations) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CITATION_IMPACT);
        ins.set_type(InsightType::CITATION_IMPACT);
        ins.seed_nodes = {entity_id};
        ins.seed_labels = {get_node_label(entity_id)};

        // Add citing authors or authors as witness nodes
        if (contributors.find(entity_id) != contributors.end()) {
            const auto& contribs = contributors[entity_id];
            ins.witness_nodes.assign(contribs.begin(), contribs.end());
            // Limit to top 10 for description
            size_t max_contribs = std::min(size_t(10), contribs.size());
            for (size_t i = 0; i < max_contribs; ++i) {
                ins.seed_labels.push_back(get_node_label(contribs[i]));
            }
        }

        std::string metric_type = impact_type[entity_id];  // "citations" or "authorships"
        ins.score_breakdown[metric_type] = static_cast<double>(impact_count);
        ins.score = std::min(static_cast<double>(impact_count) / 10.0, 1.0); // Normalize score

        std::string entity_type = looks_like_person(get_node_label(entity_id)) ? "Author" : "Work";
        ins.description = entity_type + ": " + get_node_label(entity_id) +
                         " (" + std::to_string(impact_count) + " " + metric_type + ")";

        insights.push_back(std::move(ins));
        count++;
    }

    return insights;
}

// ============== MULTI-RESOLUTION COMMUNITY DETECTION ==============
std::vector<Insight> DiscoveryEngine::find_multi_resolution_communities() {
    std::vector<Insight> insights;

    // Build node-to-node projected graph from hypergraph
    std::unordered_map<std::string, std::unordered_set<std::string>> adjacency;
    for (const auto& edge : graph_.get_all_edges()) {
        std::vector<std::string> all_nodes;
        all_nodes.insert(all_nodes.end(), edge.sources.begin(), edge.sources.end());
        all_nodes.insert(all_nodes.end(), edge.targets.begin(), edge.targets.end());

        for (size_t i = 0; i < all_nodes.size(); ++i) {
            for (size_t j = i + 1; j < all_nodes.size(); ++j) {
                adjacency[all_nodes[i]].insert(all_nodes[j]);
                adjacency[all_nodes[j]].insert(all_nodes[i]);
            }
        }
    }

    if (adjacency.empty()) return insights;

    // Store communities at each resolution level
    struct CommunityLevel {
        double resolution;
        std::unordered_map<std::string, int> node_to_community;
        std::map<int, std::vector<std::string>> community_members;
        int num_communities;
    };

    std::vector<CommunityLevel> levels;

    // Run community detection at multiple resolutions
    for (double resolution : config_.multi_resolution_scales) {
        CommunityLevel level;
        level.resolution = resolution;

        // Simple modularity-based community detection (simplified Louvain)
        // Initialize: each node in its own community
        int next_community_id = 0;
        for (const auto& [node, neighbors] : adjacency) {
            level.node_to_community[node] = next_community_id++;
        }

        // Iteratively merge communities to maximize modularity (simplified)
        bool changed = true;
        int iterations = 0;
        while (changed && iterations < 10) {
            changed = false;
            for (const auto& [node, neighbors] : adjacency) {
                // Find best community for this node
                std::map<int, int> community_links;
                for (const auto& neighbor : neighbors) {
                    if (level.node_to_community.find(neighbor) != level.node_to_community.end()) {
                        community_links[level.node_to_community[neighbor]]++;
                    }
                }

                // Move to community with most links (weighted by resolution)
                int best_community = level.node_to_community[node];
                int best_links = 0;
                for (const auto& [comm_id, link_count] : community_links) {
                    if (link_count > best_links) {
                        best_links = link_count;
                        best_community = comm_id;
                    }
                }

                if (best_community != level.node_to_community[node]) {
                    level.node_to_community[node] = best_community;
                    changed = true;
                }
            }
            iterations++;
        }

        // Build community member lists
        for (const auto& [node, comm_id] : level.node_to_community) {
            level.community_members[comm_id].push_back(node);
        }

        // Remove small communities
        for (auto it = level.community_members.begin(); it != level.community_members.end(); ) {
            if (it->second.size() < config_.multi_resolution_min_community_size) {
                it = level.community_members.erase(it);
            } else {
                ++it;
            }
        }

        level.num_communities = static_cast<int>(level.community_members.size());
        levels.push_back(level);
    }

    // Generate insights showing hierarchical structure
    for (size_t i = 0; i < levels.size(); ++i) {
        const auto& level = levels[i];

        for (const auto& [comm_id, members] : level.community_members) {
            if (insights.size() >= config_.multi_resolution_max_levels) break;

            Insight ins;
            ins.insight_id = make_insight_id(InsightType::MULTI_RESOLUTION_COMMUNITY);
            ins.set_type(InsightType::MULTI_RESOLUTION_COMMUNITY);

            // Sample up to 10 members
            size_t sample_size = std::min(size_t(10), members.size());
            for (size_t j = 0; j < sample_size; ++j) {
                ins.seed_nodes.push_back(members[j]);
                ins.seed_labels.push_back(get_node_label(members[j]));
            }
            ins.witness_nodes = ins.seed_nodes;

            ins.score_breakdown["resolution"] = level.resolution;
            ins.score_breakdown["size"] = static_cast<double>(members.size());
            ins.score_breakdown["level"] = static_cast<double>(i);
            ins.score = std::min(static_cast<double>(members.size()) / 20.0, 1.0);

            ins.description = "Community at resolution " + std::to_string(level.resolution) +
                            " with " + std::to_string(members.size()) + " members";

            insights.push_back(std::move(ins));
        }
    }

    return insights;
}

// ============== CROSS-COMMUNITY BRIDGE MAPPING ==============
std::vector<Insight> DiscoveryEngine::find_cross_community_bridge_maps(const InsightCollection& collection) {
    std::vector<Insight> insights;

    // Extract bridges from previous insights
    std::unordered_set<std::string> bridge_nodes;
    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::BRIDGE || insight.type == InsightType::DOMAIN_BRIDGE) {
            for (const auto& node : insight.seed_nodes) {
                bridge_nodes.insert(node);
            }
        }
    }

    if (bridge_nodes.empty()) return insights;

    // Extract community assignments from previous insights
    std::unordered_map<std::string, std::unordered_set<int>> node_to_communities;
    int insight_idx = 0;
    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::COMMUNITY_DETECTION ||
            insight.type == InsightType::MULTI_RESOLUTION_COMMUNITY) {
            for (const auto& node : insight.seed_nodes) {
                node_to_communities[node].insert(insight_idx);
            }
            insight_idx++;
        }
    }

    // Map bridges to communities they connect
    for (const auto& bridge : bridge_nodes) {
        // Find neighbors of this bridge
        std::unordered_set<int> connected_communities;

        for (const auto& edge : graph_.get_all_edges()) {
            bool bridge_in_sources = std::find(edge.sources.begin(), edge.sources.end(), bridge) != edge.sources.end();
            bool bridge_in_targets = std::find(edge.targets.begin(), edge.targets.end(), bridge) != edge.targets.end();

            if (!bridge_in_sources && !bridge_in_targets) continue;

            // Find communities of connected nodes
            for (const auto& src : edge.sources) {
                if (src != bridge && node_to_communities.find(src) != node_to_communities.end()) {
                    for (int comm_id : node_to_communities[src]) {
                        connected_communities.insert(comm_id);
                    }
                }
            }
            for (const auto& tgt : edge.targets) {
                if (tgt != bridge && node_to_communities.find(tgt) != node_to_communities.end()) {
                    for (int comm_id : node_to_communities[tgt]) {
                        connected_communities.insert(comm_id);
                    }
                }
            }
        }

        if (connected_communities.size() >= config_.cross_community_bridge_map_min_communities) {
            Insight ins;
            ins.insight_id = make_insight_id(InsightType::CROSS_COMMUNITY_BRIDGE_MAP);
            ins.set_type(InsightType::CROSS_COMMUNITY_BRIDGE_MAP);
            ins.seed_nodes = {bridge};
            ins.seed_labels = {get_node_label(bridge)};
            ins.witness_nodes = {bridge};

            ins.score_breakdown["communities_connected"] = static_cast<double>(connected_communities.size());
            ins.score = std::min(static_cast<double>(connected_communities.size()) / 5.0, 1.0);

            ins.description = "Bridge '" + get_node_label(bridge) + "' connects " +
                            std::to_string(connected_communities.size()) + " communities";

            insights.push_back(std::move(ins));

            if (insights.size() >= config_.cross_community_bridge_map_max_candidates) break;
        }
    }

    return insights;
}

// ============== META-PATTERN DISCOVERY ==============
std::vector<Insight> DiscoveryEngine::find_meta_patterns(const InsightCollection& collection) {
    std::vector<Insight> insights;

    // Extract existing patterns (motifs, k-truss)
    struct Pattern {
        std::vector<std::string> nodes;
        std::string type;
        int size;
    };

    std::vector<Pattern> patterns;
    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::MOTIF || insight.type == InsightType::K_TRUSS) {
            Pattern p;
            p.nodes = insight.seed_nodes;
            p.type = insight_type_to_string(insight.type);
            p.size = static_cast<int>(insight.seed_nodes.size());
            patterns.push_back(p);
        }
    }

    if (patterns.size() < config_.meta_pattern_min_occurrences) return insights;

    // Group patterns by size
    std::map<int, std::vector<Pattern>> patterns_by_size;
    for (const auto& pattern : patterns) {
        patterns_by_size[pattern.size].push_back(pattern);
    }

    // Find structural similarities (simplified: same size patterns)
    for (const auto& [size, size_patterns] : patterns_by_size) {
        if (size_patterns.size() < config_.meta_pattern_min_occurrences) continue;
        if (size > static_cast<int>(config_.meta_pattern_max_size)) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::META_PATTERN);
        ins.set_type(InsightType::META_PATTERN);

        // Sample representative patterns
        size_t sample_size = std::min(size_t(5), size_patterns.size());
        for (size_t i = 0; i < sample_size; ++i) {
            for (const auto& node : size_patterns[i].nodes) {
                if (ins.seed_nodes.size() < 20) {  // Limit total nodes
                    ins.seed_nodes.push_back(node);
                    ins.seed_labels.push_back(get_node_label(node));
                }
            }
        }
        ins.witness_nodes = ins.seed_nodes;

        ins.score_breakdown["occurrences"] = static_cast<double>(size_patterns.size());
        ins.score_breakdown["pattern_size"] = static_cast<double>(size);
        ins.score = std::min(static_cast<double>(size_patterns.size()) / 10.0, 1.0);

        ins.description = "Meta-pattern: " + std::to_string(size_patterns.size()) +
                        " similar " + std::to_string(size) + "-node structures detected";

        insights.push_back(std::move(ins));

        if (insights.size() >= config_.meta_pattern_max_patterns) break;
    }

    return insights;
}

// ============== BRIDGE ANALOGY GENERATION ==============
std::vector<Insight> DiscoveryEngine::find_bridge_analogies(const InsightCollection& collection) {
    std::vector<Insight> insights;

    // Extract bridges and communities from previous insights
    std::vector<std::string> bridge_entities;
    std::map<std::string, std::set<std::string>> bridge_to_communities;
    std::map<std::string, std::set<std::string>> community_members;

    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::BRIDGE) {
            if (!insight.seed_nodes.empty()) {
                bridge_entities.push_back(insight.seed_nodes[0]);
            }
        }
        if (insight.type == InsightType::CROSS_COMMUNITY_BRIDGE_MAP) {
            if (!insight.seed_nodes.empty()) {
                std::string bridge = insight.seed_nodes[0];
                // Extract community IDs from score_breakdown or novelty_tags
                for (const auto& tag : insight.novelty_tags) {
                    if (tag.find("community_") == 0) {
                        bridge_to_communities[bridge].insert(tag);
                    }
                }
            }
        }
        if (insight.type == InsightType::COMMUNITY_DETECTION) {
            // Community members are in seed_nodes
            std::string community_id = "community_" + std::to_string(community_members.size());
            for (const auto& member : insight.seed_nodes) {
                community_members[community_id].insert(member);
            }
        }
    }

    if (bridge_entities.empty()) {
        return insights; // No bridges found
    }

    // For each bridge, analyze relation patterns across connected communities
    struct RelationPattern {
        std::string relation_type;
        std::vector<std::string> source_nodes;
        std::vector<std::string> target_nodes;
        std::string community_id;
    };

    std::map<std::string, std::vector<RelationPattern>> bridge_patterns;

    for (const auto& bridge : bridge_entities) {
        const auto* bridge_node = graph_.get_node(bridge);
        if (!bridge_node) continue;

        // Get all edges involving this bridge
        std::map<std::string, std::vector<RelationPattern>> patterns_by_community;

        for (const auto& edge_id : bridge_node->incident_edges) {
            const auto* edge = graph_.get_hyperedge(edge_id);
            if (!edge) continue;

            // Determine which entities in this edge belong to which communities
            for (const auto& [comm_id, members] : community_members) {
                bool bridge_in_sources = false;
                bool bridge_in_targets = false;

                for (const auto& src : edge->sources) {
                    if (src == bridge) bridge_in_sources = true;
                }
                for (const auto& tgt : edge->targets) {
                    if (tgt == bridge) bridge_in_targets = true;
                }

                if (bridge_in_sources || bridge_in_targets) {
                    RelationPattern pattern;
                    pattern.relation_type = edge->relation;
                    pattern.source_nodes = edge->sources;
                    pattern.target_nodes = edge->targets;
                    pattern.community_id = comm_id;
                    patterns_by_community[comm_id].push_back(pattern);
                }
            }
        }

        bridge_patterns[bridge] = {};
        for (const auto& [comm, patterns] : patterns_by_community) {
            for (const auto& p : patterns) {
                bridge_patterns[bridge].push_back(p);
            }
        }
    }

    // Generate analogies by finding shared relation patterns
    int analogy_count = 0;
    for (const auto& bridge : bridge_entities) {
        if (analogy_count >= static_cast<int>(config_.bridge_analogy_max_candidates)) break;

        const auto& patterns = bridge_patterns[bridge];
        if (patterns.size() < 2) continue;

        // Group patterns by relation type
        std::map<std::string, std::vector<RelationPattern>> by_relation;
        for (const auto& pattern : patterns) {
            by_relation[pattern.relation_type].push_back(pattern);
        }

        // For each relation type, find cross-community analogies
        for (const auto& [relation_type, rel_patterns] : by_relation) {
            if (rel_patterns.size() < config_.bridge_analogy_min_shared_patterns) continue;

            // Find patterns from different communities
            for (size_t i = 0; i < rel_patterns.size() && i < 3; ++i) {
                for (size_t j = i + 1; j < rel_patterns.size() && j < 4; ++j) {
                    if (rel_patterns[i].community_id == rel_patterns[j].community_id) continue;

                    // Create analogy insight
                    Insight ins;
                    ins.insight_id = make_insight_id(InsightType::ANALOGICAL_TRANSFER);
                    ins.set_type(InsightType::ANALOGICAL_TRANSFER);

                    // Collect unique entities from both patterns
                    std::set<std::string> all_entities;
                    for (const auto& src : rel_patterns[i].source_nodes) all_entities.insert(src);
                    for (const auto& tgt : rel_patterns[i].target_nodes) all_entities.insert(tgt);
                    for (const auto& src : rel_patterns[j].source_nodes) all_entities.insert(src);
                    for (const auto& tgt : rel_patterns[j].target_nodes) all_entities.insert(tgt);

                    for (const auto& e : all_entities) {
                        if (ins.seed_nodes.size() < 10) {  // Limit entities
                            ins.seed_nodes.push_back(e);
                            ins.seed_labels.push_back(get_node_label(e));
                        }
                    }

                    // Score based on pattern complexity and bridging strength
                    double complexity_score = std::min(1.0, all_entities.size() / 8.0);
                    double pattern_score = std::min(1.0, by_relation.size() / 3.0);
                    ins.score = (complexity_score + pattern_score) / 2.0;

                    if (ins.score < config_.bridge_analogy_min_confidence) continue;

                    ins.score_breakdown["complexity"] = complexity_score;
                    ins.score_breakdown["pattern_strength"] = pattern_score;
                    ins.score_breakdown["shared_patterns"] = static_cast<double>(by_relation.size());

                    ins.novelty_tags = {"bridge_analogy", "bridge=" + bridge, "relation=" + relation_type};

                    // Generate description
                    std::string domain_a = rel_patterns[i].community_id;
                    std::string domain_b = rel_patterns[j].community_id;

                    std::ostringstream desc;
                    desc << "Bridge analogy via '" << get_node_label(bridge) << "': ";
                    desc << "In " << domain_a << ", entities use '" << relation_type << "' relations. ";
                    desc << "Similarly, in " << domain_b << ", entities also use '" << relation_type << "' relations. ";
                    desc << "This cross-domain pattern suggests analogical reasoning: ";
                    desc << "if X " << relation_type << " Y in " << domain_a << ", ";
                    desc << "then similar entities might " << relation_type << " in " << domain_b << ".";

                    ins.description = desc.str();

                    insights.push_back(std::move(ins));
                    analogy_count++;

                    if (analogy_count >= static_cast<int>(config_.bridge_analogy_max_candidates)) break;
                }
                if (analogy_count >= static_cast<int>(config_.bridge_analogy_max_candidates)) break;
            }
            if (analogy_count >= static_cast<int>(config_.bridge_analogy_max_candidates)) break;
        }
    }

    return insights;
}

// ============== BIAS AUDIT ==============
std::vector<Insight> DiscoveryEngine::compute_bias_audit(const InsightCollection& collection) {
    std::vector<Insight> insights;

    if (collection.insights.size() < config_.bias_audit_min_insights) {
        // Not enough data for meaningful audit
        return insights;
    }

    // ===== 1. Source Document Distribution Analysis =====
    std::map<std::string, int> source_counts;
    std::map<std::string, std::set<InsightType>> source_insight_types;

    for (const auto& insight : collection.insights) {
        for (const auto& source : insight.source_documents) {
            source_counts[source]++;
            source_insight_types[source].insert(insight.type);
        }
    }

    // Calculate statistics
    double total_citations = 0;
    for (const auto& [source, count] : source_counts) {
        total_citations += count;
    }
    double mean_citations = total_citations / source_counts.size();

    // Calculate Gini coefficient (inequality measure)
    std::vector<int> sorted_counts;
    for (const auto& [source, count] : source_counts) {
        sorted_counts.push_back(count);
    }
    std::sort(sorted_counts.begin(), sorted_counts.end());

    double gini = 0.0;
    double sum_of_ranks = 0.0;
    for (size_t i = 0; i < sorted_counts.size(); ++i) {
        sum_of_ranks += (i + 1) * sorted_counts[i];
    }
    if (total_citations > 0) {
        gini = (2.0 * sum_of_ranks) / (sorted_counts.size() * total_citations) -
               (sorted_counts.size() + 1.0) / sorted_counts.size();
    }

    // ===== 2. Insight Type Distribution Analysis =====
    std::map<InsightType, int> type_counts;
    for (const auto& insight : collection.insights) {
        type_counts[insight.type]++;
    }

    // ===== 3. Entity Coverage Analysis =====
    std::map<std::string, int> entity_mentions;
    for (const auto& insight : collection.insights) {
        for (const auto& entity : insight.seed_nodes) {
            entity_mentions[entity]++;
        }
    }

    double total_entity_mentions = 0;
    for (const auto& [entity, count] : entity_mentions) {
        total_entity_mentions += count;
    }
    double mean_entity_mentions = total_entity_mentions / entity_mentions.size();

    // ===== 4. Generate Bias Audit Insight =====
    Insight audit;
    audit.insight_id = make_insight_id(InsightType::BIAS_AUDIT);
    audit.set_type(InsightType::BIAS_AUDIT);
    audit.score = 1.0 - gini; // Higher score = better fairness (lower inequality)

    // Store metrics in score_breakdown
    audit.score_breakdown["gini_coefficient"] = gini;
    audit.score_breakdown["source_diversity"] = static_cast<double>(source_counts.size());
    audit.score_breakdown["mean_citations_per_source"] = mean_citations;
    audit.score_breakdown["total_sources"] = static_cast<double>(source_counts.size());
    audit.score_breakdown["total_insights"] = static_cast<double>(collection.insights.size());
    audit.score_breakdown["entity_diversity"] = static_cast<double>(entity_mentions.size());
    audit.score_breakdown["mean_entity_mentions"] = mean_entity_mentions;

    // Identify over-represented sources
    std::vector<std::string> over_represented;
    std::vector<std::string> under_represented;

    for (const auto& [source, count] : source_counts) {
        if (count > mean_citations * config_.bias_audit_overrepresentation_threshold) {
            over_represented.push_back(source);
            audit.score_breakdown["overrep_" + source] = static_cast<double>(count);
        }
        if (count < mean_citations / config_.bias_audit_overrepresentation_threshold) {
            under_represented.push_back(source);
            audit.score_breakdown["underrep_" + source] = static_cast<double>(count);
        }
    }

    // Identify over-represented entities
    std::vector<std::string> dominant_entities;
    for (const auto& [entity, count] : entity_mentions) {
        if (count > mean_entity_mentions * config_.bias_audit_overrepresentation_threshold) {
            dominant_entities.push_back(entity);
            if (dominant_entities.size() < 10) {  // Limit to top 10
                audit.seed_nodes.push_back(entity);
                audit.seed_labels.push_back(get_node_label(entity));
            }
        }
    }

    // Build description
    std::ostringstream desc;
    desc << "Bias Audit: Analyzed " << collection.insights.size() << " insights across "
         << source_counts.size() << " source documents. ";

    // Gini interpretation
    if (gini < 0.3) {
        desc << "Distribution is relatively balanced (Gini: " << std::fixed << std::setprecision(2) << gini << "). ";
    } else if (gini < 0.5) {
        desc << "Distribution shows moderate inequality (Gini: " << std::fixed << std::setprecision(2) << gini << "). ";
    } else {
        desc << "Distribution shows high inequality (Gini: " << std::fixed << std::setprecision(2) << gini << "). ";
    }

    // Over-representation warnings
    if (!over_represented.empty()) {
        desc << over_represented.size() << " source(s) are over-represented (>"
             << std::fixed << std::setprecision(1) << config_.bias_audit_overrepresentation_threshold
             << "x mean): ";
        for (size_t i = 0; i < std::min(size_t(3), over_represented.size()); ++i) {
            if (i > 0) desc << ", ";
            desc << over_represented[i] << " (" << source_counts[over_represented[i]] << " insights)";
        }
        if (over_represented.size() > 3) {
            desc << " and " << (over_represented.size() - 3) << " others";
        }
        desc << ". ";
    }

    // Under-representation warnings
    if (!under_represented.empty() && under_represented.size() > source_counts.size() / 4) {
        desc << under_represented.size() << " source(s) are under-represented (<"
             << std::fixed << std::setprecision(1) << (1.0 / config_.bias_audit_overrepresentation_threshold)
             << "x mean). ";
    }

    // Entity concentration
    if (!dominant_entities.empty()) {
        desc << dominant_entities.size() << " entities dominate the insights (>"
             << std::fixed << std::setprecision(1) << config_.bias_audit_overrepresentation_threshold
             << "x mean mentions): ";
        for (size_t i = 0; i < std::min(size_t(5), dominant_entities.size()); ++i) {
            if (i > 0) desc << ", ";
            desc << get_node_label(dominant_entities[i]);
        }
        if (dominant_entities.size() > 5) {
            desc << " and " << (dominant_entities.size() - 5) << " others";
        }
        desc << ". ";
    }

    // Recommendations
    desc << "Recommendation: ";
    if (gini > 0.5) {
        desc << "Consider weighting insights inversely by source frequency to balance representation. ";
    }
    if (!over_represented.empty()) {
        desc << "Validate that over-represented sources reflect true importance rather than sampling bias. ";
    }
    if (dominant_entities.size() > entity_mentions.size() / 10) {
        desc << "High entity concentration suggests narrow focus; consider broadening extraction scope.";
    } else {
        desc << "Distribution is reasonable; no major bias concerns detected.";
    }

    audit.description = desc.str();
    audit.novelty_tags = {"bias_audit", "fairness", "representation_analysis"};

    insights.push_back(std::move(audit));
    return insights;
}

// ============== COMMUNITY RECOMMENDATIONS ==============
std::vector<Insight> DiscoveryEngine::generate_community_recommendations(const InsightCollection& collection) {
    std::vector<Insight> insights;

    // Extract community assignments from previous insights
    std::map<std::string, std::string> entity_to_community;
    std::map<std::string, std::set<std::string>> community_members;

    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::COMMUNITY_DETECTION) {
            // Community ID from insight
            std::string community_id = "comm_" + std::to_string(community_members.size());
            for (const auto& entity : insight.seed_nodes) {
                entity_to_community[entity] = community_id;
                community_members[community_id].insert(entity);
            }
        }
    }

    if (entity_to_community.empty()) {
        // No community structure available
        return insights;
    }

    // Build entity similarity scores based on shared edges
    std::map<std::string, std::map<std::string, double>> entity_similarity;

    auto all_nodes = graph_.get_all_nodes();
    for (const auto& node1 : all_nodes) {
        std::set<std::string> neighbors1;
        for (const auto& edge_id : node1.incident_edges) {
            const auto* edge = graph_.get_hyperedge(edge_id);
            if (edge) {
                for (const auto& src : edge->sources) neighbors1.insert(src);
                for (const auto& tgt : edge->targets) neighbors1.insert(tgt);
            }
        }

        for (const auto& node2 : all_nodes) {
            if (node1.id == node2.id) continue;

            std::set<std::string> neighbors2;
            for (const auto& edge_id : node2.incident_edges) {
                const auto* edge = graph_.get_hyperedge(edge_id);
                if (edge) {
                    for (const auto& src : edge->sources) neighbors2.insert(src);
                    for (const auto& tgt : edge->targets) neighbors2.insert(tgt);
                }
            }

            // Jaccard similarity of neighborhoods
            std::set<std::string> intersection;
            std::set_intersection(neighbors1.begin(), neighbors1.end(),
                                neighbors2.begin(), neighbors2.end(),
                                std::inserter(intersection, intersection.begin()));

            std::set<std::string> union_set;
            std::set_union(neighbors1.begin(), neighbors1.end(),
                         neighbors2.begin(), neighbors2.end(),
                         std::inserter(union_set, union_set.begin()));

            double similarity = union_set.empty() ? 0.0 :
                              static_cast<double>(intersection.size()) / union_set.size();

            if (similarity >= config_.community_recommendation_min_similarity) {
                entity_similarity[node1.id][node2.id] = similarity;
            }
        }
    }

    // Generate recommendations for top entities (by degree)
    std::vector<std::pair<std::string, int>> entity_degrees;
    for (const auto& node : all_nodes) {
        entity_degrees.push_back({node.id, static_cast<int>(node.incident_edges.size())});
    }
    std::sort(entity_degrees.begin(), entity_degrees.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });

    // Recommend for top entities
    size_t max_seed_entities = std::min(size_t(20), entity_degrees.size());
    for (size_t i = 0; i < max_seed_entities; ++i) {
        const std::string& seed_entity = entity_degrees[i].first;

        auto it_comm = entity_to_community.find(seed_entity);
        if (it_comm == entity_to_community.end()) continue;

        std::string seed_community = it_comm->second;

        // Collect recommendations with scores
        struct Recommendation {
            std::string entity;
            double similarity;
            bool same_community;
            double final_score;
        };
        std::vector<Recommendation> recommendations;

        for (const auto& [other_entity, sim] : entity_similarity[seed_entity]) {
            auto it_other = entity_to_community.find(other_entity);
            bool same_comm = (it_other != entity_to_community.end() &&
                            it_other->second == seed_community);

            // Score: combine similarity + novelty bonus for cross-community
            double novelty_bonus = same_comm ? 0.0 : config_.community_recommendation_novelty_weight;
            double final_score = sim * (1.0 + novelty_bonus);

            recommendations.push_back({other_entity, sim, same_comm, final_score});
        }

        // Sort by final score
        std::sort(recommendations.begin(), recommendations.end(),
                 [](const auto& a, const auto& b) { return a.final_score > b.final_score; });

        // Create insight for this entity
        if (recommendations.empty()) continue;

        Insight rec;
        rec.insight_id = make_insight_id(InsightType::COMMUNITY_RECOMMENDATION);
        rec.set_type(InsightType::COMMUNITY_RECOMMENDATION);

        rec.seed_nodes.push_back(seed_entity);
        rec.seed_labels.push_back(get_node_label(seed_entity));

        // Add top recommendations
        size_t count = std::min(recommendations.size(), config_.community_recommendation_max_per_entity);
        for (size_t j = 0; j < count; ++j) {
            rec.witness_nodes.push_back(recommendations[j].entity);
            std::string label = get_node_label(recommendations[j].entity);
            if (label.size() > 50) label = label.substr(0, 47) + "...";

            std::string key = "rec_" + std::to_string(j) + "_score";
            rec.score_breakdown[key] = recommendations[j].final_score;

            if (recommendations[j].same_community) {
                rec.score_breakdown["rec_" + std::to_string(j) + "_type"] = 1.0; // same community
            } else {
                rec.score_breakdown["rec_" + std::to_string(j) + "_type"] = 2.0; // cross community
            }
        }

        rec.score = recommendations[0].final_score; // Use top recommendation score

        // Build description
        std::ostringstream desc;
        desc << "Recommendations for '" << get_node_label(seed_entity) << "': ";

        // Categorize recommendations
        std::vector<std::string> within_community;
        std::vector<std::string> cross_community;

        for (size_t j = 0; j < count; ++j) {
            if (recommendations[j].same_community) {
                within_community.push_back(get_node_label(recommendations[j].entity));
            } else {
                cross_community.push_back(get_node_label(recommendations[j].entity));
            }
        }

        if (!within_community.empty()) {
            desc << "Within-community (familiar): ";
            for (size_t j = 0; j < std::min(size_t(3), within_community.size()); ++j) {
                if (j > 0) desc << ", ";
                desc << within_community[j];
            }
            if (within_community.size() > 3) {
                desc << " and " << (within_community.size() - 3) << " others";
            }
            desc << ". ";
        }

        if (!cross_community.empty()) {
            desc << "Cross-community (novel): ";
            for (size_t j = 0; j < std::min(size_t(3), cross_community.size()); ++j) {
                if (j > 0) desc << ", ";
                desc << cross_community[j];
            }
            if (cross_community.size() > 3) {
                desc << " and " << (cross_community.size() - 3) << " others";
            }
            desc << ". ";
        }

        desc << "Recommendations ranked by similarity + novelty (cross-community bonus: "
             << std::fixed << std::setprecision(1) << (config_.community_recommendation_novelty_weight * 100)
             << "%).";

        rec.description = desc.str();
        rec.novelty_tags = {"community_recommendation", "similarity", "entity=" + seed_entity};

        insights.push_back(std::move(rec));

        if (insights.size() >= 50) break; // Limit total recommendations
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
        // REMOVED: analogical_transfer (low-value: 2 insights across 9 runs)
        // } else if (op == "analogical_transfer" || op == "analogical-transfer" || op == "analogy") {
        //     insights = find_analogical_transfers();
        } else if (op == "uncertainty_sampling" || op == "uncertainty-sampling" || op == "uncertainty") {
            insights = find_uncertainty_samples();
        } else if (op == "counterfactual" || op == "counterfactual-probing") {
            insights = find_counterfactual_probes();
        } else if (op == "hyperedge_prediction" || op == "hyperedge-prediction" || op == "hyperedge") {
            insights = find_hyperedge_predictions();
        } else if (op == "diffusion" || op == "diffusions") {
            insights = find_diffusions();
        } else if (op == "surprise" || op == "surprises") {
            insights = find_surprise_edges();
        } else if (op == "rules" || op == "rule") {
            insights = find_rules();
        } else if (op == "community" || op == "community_link" || op == "community-links") {
            insights = find_community_links();
        } else if (op == "hypotheses_1" || op == "hypotheses-1" || op == "bayesian_hypotheses") {
            insights = find_hypotheses_1(collection);
        } else if (op == "hypotheses_2" || op == "hypotheses-2" || op == "mechanistic_hypotheses") {
            insights = find_hypotheses_2(collection);
        } else if (op == "hypotheses_3" || op == "hypotheses-3" || op == "counterfactual_hypotheses") {
            insights = find_hypotheses_3(collection);
        } else if (op == "causal_chain" || op == "causal-chain" || op == "causal_chains" || op == "causal-chains") {
            insights = find_causal_chains();
        } else if (op == "intervention_point" || op == "intervention-point" || op == "intervention_points" || op == "intervention-points") {
            insights = find_intervention_points();
        } else if (op == "feedback_loop" || op == "feedback-loop" || op == "feedback_loops" || op == "feedback-loops") {
            insights = find_feedback_loops();
        } else if (op == "confounder" || op == "confounders") {
            insights = find_confounders();
        } else if (op == "taxonomy" || op == "taxonomy_induction" || op == "taxonomy-induction") {
            insights = find_taxonomy_induction();
        } else if (op == "domain_bridge" || op == "domain-bridge" || op == "domain_bridges" || op == "domain-bridges") {
            insights = find_domain_bridges();
        } else if (op == "logical_entailment" || op == "logical-entailment" || op == "logical_entailments" || op == "logical-entailments") {
            insights = find_logical_entailments();
        } else if (op == "compositional_reasoning" || op == "compositional-reasoning" || op == "composition") {
            insights = find_compositional_reasoning();
        } else if (op == "explanatory_chain" || op == "explanatory-chain" || op == "explanatory_chains" || op == "explanatory-chains" || op == "explanation") {
            insights = find_explanatory_chains();
        } else if (op == "schema_violation" || op == "schema-violation" || op == "schema_violations" || op == "schema-violations" || op == "schema") {
            insights = find_schema_violations();
        } else if (op == "transitive_closure" || op == "transitive-closure" || op == "transitive" || op == "closure") {
            insights = find_transitive_closure_gaps();
        } else if (op == "meta_path" || op == "meta-path" || op == "meta_paths" || op == "meta-paths" || op == "metapath") {
            insights = find_meta_path_links();
        } else if (op == "meta_path_pattern" || op == "meta-path-pattern" || op == "meta_path_patterns" || op == "meta-path-patterns") {
            insights = find_meta_path_patterns();
        } else if (op == "hypothesis" || op == "hypotheses") {
            // Old find_hypotheses replaced with find_hypotheses_1, _2, _3
            // Use run_all() or specify individual operators
        } else if (op == "pathrank" || op == "path_rank" || op == "path-ranking") {
            insights = find_path_rankings();
        } else if (op == "intersection_bridge" || op == "intersection-bridge" ||
                   op == "intersection_hypothesis_bridge" || op == "intersection-hypothesis-bridge" ||
                   op == "higher_order_bridge" || op == "higher-order-bridge" ||
                   op == "hypergraph_traversal" || op == "hypergraph-traversal") {
            insights = find_intersection_hypothesis_bridges();
        } else if (op == "long_chain" || op == "long-chain" || op == "longchain") {
            insights = find_long_chains();
        } else if (op == "embedding" || op == "embedding_link" || op == "transe" || op == "embeddings") {
            insights = find_embedding_links();
        } else if (is_author_chain_op) {
            insights = find_author_reference_chains();
        // REMOVED: co_authorship (low-value: 1 insight across 9 runs)
        // } else if (op == "co_authorship" || op == "co-authorship" || op == "coauthorship" || op == "collaboration") {
        //     insights = find_co_authorship_networks();
        } else if (op == "citation_impact" || op == "citation-impact" || op == "citations" || op == "impact") {
            insights = find_citation_impact();
        } else if (op == "multi_resolution_community" || op == "multi-resolution-community" || op == "multiresolution" || op == "hierarchical_community") {
            insights = find_multi_resolution_communities();
        } else if (op == "cross_community_bridge_map" || op == "cross-community-bridge-map" || op == "community_bridge_map" || op == "bridge_map") {
            insights = find_cross_community_bridge_maps(collection);
        } else if (op == "meta_pattern" || op == "meta-pattern" || op == "metapattern" || op == "pattern_of_patterns") {
            insights = find_meta_patterns(collection);
        } else if (op == "bridge_analogies" || op == "bridge-analogies" || op == "bridge_analogy" || op == "analogy" || op == "analogies") {
            insights = find_bridge_analogies(collection);
        } else if (op == "bias_audit" || op == "bias-audit" || op == "bias" || op == "fairness" || op == "representation") {
            insights = compute_bias_audit(collection);
        } else if (op == "community_recommendation" || op == "community-recommendation" || op == "recommend" || op == "recommendations") {
            insights = generate_community_recommendations(collection);
        // REMOVED: mechanism_consolidation (low-value: 2 insights across 9 runs)
        // } else if (op == "mechanism_consolidation" || op == "mechanism-consolidation" || op == "mechanism_cluster" || op == "mechanism-cluster") {
        //     insights = find_mechanism_consolidations(collection);
        } else if (op == "evidence_fusion" || op == "evidence-fusion" || op == "evidence_fusion_link" || op == "evidence-fusion-link") {
            insights = find_evidence_fusion_links(collection);
        } else if (op == "meta_path_anomaly" || op == "meta-path-anomaly" || op == "metapath_anomaly" || op == "metapath-anomaly") {
            insights = find_meta_path_anomalies(collection);
        // REMOVED: intervention_bottleneck (low-value: 2 insights across 9 runs)
        // } else if (op == "intervention_bottleneck" || op == "intervention-bottleneck") {
        //     insights = find_intervention_bottlenecks(collection);
        } else if (op == "competing_mechanism" || op == "competing-mechanism" ||
                   op == "competing_mechanisms" || op == "competing-mechanisms") {
            insights = find_competing_mechanisms(collection);
        } else if (op == "schema_repair" || op == "schema-repair" || op == "schema_repairs" || op == "schema-repairs") {
            insights = find_schema_repairs(collection);
        } else if (op == "cross_community_mechanism_bridge" || op == "cross-community-mechanism-bridge" ||
                   op == "cross_community_bridge" || op == "cross-community-bridge") {
            insights = find_cross_community_mechanism_bridges(collection);
        } else if (op == "evidence_debt" || op == "evidence-debt") {
            insights = find_evidence_debt(collection);
        } else if (op == "consensus_frontier" || op == "consensus-frontier") {
            insights = find_consensus_frontier(collection);
        } else if (op == "boundary_condition_map" || op == "boundary-condition-map" || op == "boundary_condition") {
            insights = find_boundary_condition_map(collection);
        } else if (op == "failure_mode_topology" || op == "failure-mode-topology" || op == "failure_mode") {
            insights = find_failure_mode_topology(collection);
        } else if (op == "benchmark_dependence" || op == "benchmark-dependence") {
            insights = find_benchmark_dependence(collection);
        } else if (op == "concept_drift" || op == "concept-drift") {
            insights = find_concept_drift(collection);
        } else if (op == "premise_bottleneck" || op == "premise-bottleneck") {
            insights = find_premise_bottleneck(collection);
        } else if (op == "translation_gap" || op == "translation-gap") {
            insights = find_translation_gap(collection);
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
    return run_operators(all_discovery_operators());
}

// =============================================================================
// HYPOTHESES_1: Bayesian Hypothesis Network (Probabilistic Belief Propagation)
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_hypotheses_1(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Generating Bayesian hypotheses", 0, 100);

    // Stage 1: Build Bayesian Network from insights
    // Nodes = entities, Edges = evidential relationships
    std::map<std::string, std::set<std::string>> evidence_graph;
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> evidence_sources; // Track which insights support each pair
    std::map<std::string, int> node_frequency;

    for (const auto& insight : collection.insights) {
        // Extract entity pairs from various insight types
        if (insight.seed_nodes.size() >= 2) {
            for (size_t i = 0; i < insight.seed_nodes.size(); ++i) {
                for (size_t j = i + 1; j < insight.seed_nodes.size(); ++j) {
                    std::string n1 = insight.seed_nodes[i];
                    std::string n2 = insight.seed_nodes[j];
                    evidence_graph[n1].insert(n2);
                    evidence_graph[n2].insert(n1);
                    evidence_sources[{n1, n2}].push_back(insight.insight_id);
                    node_frequency[n1]++;
                    node_frequency[n2]++;
                }
            }
        }
    }

    report_progress("Generating Bayesian hypotheses", 20, 100);

    // Stage 2: Estimate Conditional Probabilities
    // Compute P(B|A) from co-occurrence patterns
    std::map<std::pair<std::string, std::string>, double> conditional_probs;
    std::map<std::string, double> prior_probs;

    int total_observations = 0;
    for (const auto& [node, freq] : node_frequency) {
        total_observations += freq;
    }

    if (total_observations == 0) {
        std::cerr << "[WARNING] hypotheses_1 requires existing insights with entity pairs.\n";
        std::cerr << "          Found 0 insights with 2+ seed_nodes in the collection.\n";
        std::cerr << "          Run other discovery operators first (e.g., -p \"all\")\n";
        report_progress("Generating Bayesian hypotheses", 100, 100);
        return results;
    }

    // Compute priors P(A) = freq(A) / total
    for (const auto& [node, freq] : node_frequency) {
        prior_probs[node] = static_cast<double>(freq) / total_observations;
    }

    // Compute conditionals P(B|A) = co-occur(A,B) / freq(A)
    for (const auto& [pair, sources] : evidence_sources) {
        std::string n1 = pair.first;
        std::string n2 = pair.second;
        if (node_frequency[n1] > 0) {
            conditional_probs[{n1, n2}] = static_cast<double>(sources.size()) / node_frequency[n1];
        }
        if (node_frequency[n2] > 0) {
            conditional_probs[{n2, n1}] = static_cast<double>(sources.size()) / node_frequency[n2];
        }
    }

    report_progress("Generating Bayesian hypotheses", 40, 100);

    // Stage 3: Belief Propagation (Iterative Message Passing)
    // Initialize beliefs with conditional probabilities
    std::map<std::pair<std::string, std::string>, double> posterior_beliefs;
    for (const auto& [pair, prob] : conditional_probs) {
        posterior_beliefs[pair] = prob;
    }

    // Iterative belief propagation
    for (size_t iter = 0; iter < config_.bayesian_max_iterations; ++iter) {
        std::map<std::pair<std::string, std::string>, double> new_beliefs;
        double max_change = 0.0;

        // Update beliefs using Bayes' rule: P(H|E) ∝ P(E|H) × P(H)
        for (const auto& [pair, likelihood] : conditional_probs) {
            std::string n1 = pair.first;
            std::string n2 = pair.second;

            double prior = prior_probs[n2];
            double evidence_diversity = static_cast<double>(evidence_sources[pair].size()) / 10.0; // Normalize
            evidence_diversity = std::min(evidence_diversity, 1.0);

            // Posterior ∝ likelihood × prior × evidence_diversity
            double posterior = likelihood * prior * (0.5 + 0.5 * evidence_diversity);
            posterior = std::min(posterior, 1.0);

            new_beliefs[pair] = posterior;
            max_change = std::max(max_change, std::abs(posterior - posterior_beliefs[pair]));
        }

        posterior_beliefs = new_beliefs;

        // Check convergence
        if (max_change < config_.bayesian_convergence_threshold) {
            break;
        }
    }

    report_progress("Generating Bayesian hypotheses", 70, 100);

    // Stage 4: Identify High-Posterior Candidates
    struct HypothesisCandidate {
        std::string node1;
        std::string node2;
        double posterior;
        double prior;
        double prior_surprise; // log(posterior/prior)
        double evidence_diversity;
        double uncertainty_reduction;
        std::vector<std::string> supporting_insights;
        double score;
    };

    std::vector<HypothesisCandidate> candidates;

    for (const auto& [pair, posterior] : posterior_beliefs) {
        if (posterior < config_.bayesian_min_posterior) continue;

        std::string n1 = pair.first;
        std::string n2 = pair.second;

        // Skip if already directly connected in graph
        if (evidence_graph[n1].count(n2) > 0) continue;

        HypothesisCandidate cand;
        cand.node1 = n1;
        cand.node2 = n2;
        cand.posterior = posterior;
        cand.prior = prior_probs[n2];

        // Compute prior surprise: log(posterior/prior)
        if (cand.prior > 0.001) {
            cand.prior_surprise = std::log(posterior / cand.prior);
        } else {
            cand.prior_surprise = std::log(posterior / 0.001);
        }

        // Evidence diversity: number of independent signal types
        cand.evidence_diversity = static_cast<double>(evidence_sources[pair].size()) / config_.bayesian_top_evidence_per_hypothesis;
        cand.evidence_diversity = std::min(cand.evidence_diversity, 1.0);

        // Uncertainty reduction: entropy decrease
        double prior_entropy = -cand.prior * std::log2(cand.prior + 1e-10);
        double posterior_entropy = -posterior * std::log2(posterior + 1e-10);
        cand.uncertainty_reduction = std::max(0.0, prior_entropy - posterior_entropy);

        cand.supporting_insights = evidence_sources[pair];

        // Weighted score
        cand.score = config_.bayesian_weight_posterior * posterior +
                     config_.bayesian_weight_evidence_diversity * cand.evidence_diversity +
                     config_.bayesian_weight_prior_surprise * std::min(cand.prior_surprise / 5.0, 1.0) +
                     config_.bayesian_weight_uncertainty_reduction * cand.uncertainty_reduction;

        candidates.push_back(cand);
    }

    // Sort by score
    std::sort(candidates.begin(), candidates.end(), [](const HypothesisCandidate& a, const HypothesisCandidate& b) {
        return a.score > b.score;
    });

    // Take top candidates
    size_t num_hypotheses = std::min(candidates.size(), config_.hypothesis_count);

    for (size_t i = 0; i < num_hypotheses; ++i) {
        const auto& cand = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::HYPOTHESES_1);
        ins.set_type(InsightType::HYPOTHESES_1);

        ins.seed_nodes = {cand.node1, cand.node2};
        ins.seed_labels = {get_node_label(cand.node1), get_node_label(cand.node2)};

        ins.witness_nodes = ins.seed_nodes;
        ins.witness_edges = cand.supporting_insights;

        ins.description = "Bayesian hypothesis: '" + get_node_label(cand.node1) + "' → '" + get_node_label(cand.node2) +
                         "' (posterior=" + std::to_string(cand.posterior) + ")";

        ins.score_breakdown["posterior"] = cand.posterior;
        ins.score_breakdown["prior_surprise"] = cand.prior_surprise;
        ins.score_breakdown["evidence_diversity"] = cand.evidence_diversity;
        ins.score_breakdown["uncertainty_reduction"] = cand.uncertainty_reduction;
        ins.score = cand.score;

        results.push_back(ins);
    }

    report_progress("Generating Bayesian hypotheses", 100, 100);
    return results;
}

// =============================================================================
// HYPOTHESES_2: Typed Mechanistic Chains + Explanation Subgraphs
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_hypotheses_2(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Generating mechanistic hypotheses", 0, 100);
    (void)collection;

    struct EdgeInfo {
        std::string src;
        std::string tgt;
        std::string edge_id;
        std::string relation;
        double confidence;
    };

    auto normalize_text = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        for (auto& c : s) {
            if (c == '_' || c == '-') c = ' ';
        }
        return s;
    };

    auto get_node_type = [&](const std::string& node_id) {
        const auto* node = graph_.get_node(node_id);
        if (!node) return std::string();
        const std::vector<std::string> keys = {"type", "category", "entity_type", "class", "role"};
        for (const auto& key : keys) {
            auto it = node->properties.find(key);
            if (it != node->properties.end() && !it->second.empty()) {
                return normalize_text(it->second);
            }
        }
        return std::string();
    };

    auto matches_role_type = [&](const std::string& node_id,
                                 const std::vector<std::string>& allowed_types) {
        if (allowed_types.empty()) return true;
        std::string node_type = get_node_type(node_id);
        if (node_type.empty()) return config_.hypothesis_2_allow_unknown_types;
        for (const auto& t : allowed_types) {
            if (t == "any") return true;
            if (node_type.find(t) != std::string::npos) return true;
        }
        return false;
    };

    auto relation_matches = [&](const std::string& relation,
                                const std::vector<std::string>& keywords) {
        if (keywords.empty()) return true;
        for (const auto& kw : keywords) {
            if (relation.find(kw) != std::string::npos) return true;
        }
        return false;
    };

    // Build directed edges from hyperedges
    std::vector<EdgeInfo> edges;
    edges.reserve(graph_.get_all_edges().size());
    std::unordered_map<std::string, std::vector<EdgeInfo>> out_edges;
    std::unordered_map<std::string, std::vector<EdgeInfo>> in_edges;
    std::unordered_map<std::string, std::string> edge_chunk;

    for (const auto& edge : graph_.get_all_edges()) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        std::string rel = normalize_text(edge.relation);
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                EdgeInfo info{src, tgt, edge.id, rel, edge.confidence};
                out_edges[src].push_back(info);
                in_edges[tgt].push_back(info);
                edges.push_back(info);
            }
        }
        if (!edge.source_chunk_id.empty()) {
            edge_chunk[edge.id] = edge.source_chunk_id;
        }
    }

    if (edges.empty()) {
        std::cerr << "[WARNING] hypotheses_2 requires graph edges with typed relations.\n";
        std::cerr << "          Found 0 edges in the graph.\n";
        std::cerr << "          Ensure the graph has hyperedges with source and target nodes.\n";
        report_progress("Generating mechanistic hypotheses", 100, 100);
        return results;
    }

    struct ChainTemplate {
        std::string name;
        std::vector<std::string> role_labels;
        std::vector<std::vector<std::string>> role_types;
        std::vector<std::vector<std::string>> relation_keywords;
    };

    std::vector<std::string> causal_keywords = {
        "causes", "cause", "leads to", "results in", "produces", "triggers",
        "influences", "affects", "determines", "drives", "enables", "increases", "decreases"
    };

    std::vector<ChainTemplate> templates;
    templates.push_back({
        "driver_hazard_exposure_impact",
        {"Driver", "Hazard", "Exposure", "Impact"},
        {{"driver"}, {"hazard"}, {"exposure"}, {"impact"}},
        {causal_keywords, causal_keywords, causal_keywords}
    });

    templates.push_back({
        "method_dataset_metric_outcome",
        {"Method", "Dataset", "Metric", "Outcome"},
        {{"method"}, {"dataset", "data"}, {"metric", "score"}, {"outcome", "result", "performance"}},
        {
            {"applies", "uses", "trained on", "applied to"},
            {"measured by", "evaluated by", "benchmark", "metric", "score"},
            {"improves", "yields", "achieves", "outperforms", "results in"}
        }
    });

    templates.push_back({
        "cause_mediator_effect",
        {"Cause", "Mediator", "Effect"},
        {},
        {causal_keywords, causal_keywords}
    });

    report_progress("Generating mechanistic hypotheses", 15, 100);

    struct ChainCandidate {
        std::string template_name;
        std::vector<std::string> role_labels;
        std::vector<std::string> nodes;
        std::vector<std::string> edge_ids;
        std::vector<std::string> relations;
        std::string pattern_key;
        double relation_strength = 0.0;
        double type_fidelity = 0.0;
        double support = 0.0;
        double novelty = 0.0;
        double coherence = 0.0;
        double chain_score = 0.0;
        double explanation_score = 0.0;
        std::vector<std::string> explanation_nodes;
        std::vector<std::string> explanation_edges;
        double explanation_alignment = 0.0;
        double explanation_confidence = 0.0;
        double explanation_density = 0.0;
        double explanation_connectivity = 0.0;
        double final_score = 0.0;
    };

    std::vector<std::string> seed_nodes;
    if (config_.hypothesis_2_max_seed_nodes > 0) {
        auto hubs = graph_.get_top_hubs(static_cast<int>(config_.hypothesis_2_max_seed_nodes));
        for (const auto& [node_id, _] : hubs) {
            seed_nodes.push_back(node_id);
        }
    } else {
        auto nodes = graph_.get_all_nodes();
        seed_nodes.reserve(nodes.size());
        for (const auto& node : nodes) seed_nodes.push_back(node.id);
    }

    std::vector<ChainCandidate> candidates;
    std::unordered_map<std::string, int> pattern_counts;

    auto build_candidates = [&](const ChainTemplate& tmpl) {
        const size_t chain_len = tmpl.role_labels.size();
        if (chain_len < static_cast<size_t>(config_.hypothesis_2_chain_min_length) ||
            chain_len > static_cast<size_t>(config_.hypothesis_2_chain_max_length)) {
            return;
        }

        bool stop = false;
        for (const auto& start : seed_nodes) {
            if (stop) break;
            if (!matches_role_type(start, tmpl.role_types.empty() ? std::vector<std::string>() : tmpl.role_types[0])) {
                continue;
            }

            std::vector<std::string> path_nodes = {start};
            std::vector<EdgeInfo> path_edges;
            std::unordered_set<std::string> visited = {start};

            std::function<void(size_t, const std::string&)> dfs = [&](size_t depth, const std::string& node) {
                if (stop) return;
                if (depth + 1 == chain_len) {
                    ChainCandidate cand;
                    cand.template_name = tmpl.name;
                    cand.role_labels = tmpl.role_labels;
                    cand.nodes = path_nodes;
                    cand.edge_ids.reserve(path_edges.size());
                    cand.relations.reserve(path_edges.size());

                    double conf_sum = 0.0;
                    for (const auto& e : path_edges) {
                        cand.edge_ids.push_back(e.edge_id);
                        cand.relations.push_back(e.relation);
                        conf_sum += e.confidence;
                    }
                    cand.relation_strength = path_edges.empty() ? 0.0 : (conf_sum / path_edges.size());

                    std::ostringstream pattern;
                    for (size_t i = 0; i < cand.relations.size(); ++i) {
                        if (i > 0) pattern << "->";
                        pattern << cand.relations[i];
                    }
                    cand.pattern_key = pattern.str();
                    pattern_counts[cand.pattern_key]++;

                    // Type fidelity
                    double type_hits = 0.0;
                    for (size_t i = 0; i < cand.nodes.size(); ++i) {
                        const auto& allowed = tmpl.role_types.size() > i ? tmpl.role_types[i] : std::vector<std::string>();
                        if (matches_role_type(cand.nodes[i], allowed)) {
                            type_hits += 1.0;
                        }
                    }
                    cand.type_fidelity = cand.nodes.empty() ? 0.0 : type_hits / cand.nodes.size();

                    // Coherence based on relation keyword matches
                    double rel_hits = 0.0;
                    for (size_t i = 0; i < cand.relations.size(); ++i) {
                        const auto& kws = tmpl.relation_keywords.size() > i ? tmpl.relation_keywords[i] : std::vector<std::string>();
                        if (relation_matches(cand.relations[i], kws)) rel_hits += 1.0;
                    }
                    cand.coherence = cand.relations.empty() ? 0.0 : rel_hits / cand.relations.size();

                    candidates.push_back(std::move(cand));
                    if (candidates.size() >= config_.hypothesis_2_max_candidates) {
                        stop = true;
                    }
                    return;
                }

                auto out_it = out_edges.find(node);
                if (out_it == out_edges.end()) return;
                const auto& next_edges = out_it->second;
                size_t degree_checked = 0;
                for (const auto& e : next_edges) {
                    if (degree_checked++ >= config_.hypothesis_2_explanation_degree_cap) break;
                    if (visited.count(e.tgt)) continue;
                    const auto& rel_kws = tmpl.relation_keywords.size() > depth ? tmpl.relation_keywords[depth] : std::vector<std::string>();
                    if (!relation_matches(e.relation, rel_kws)) continue;
                    const auto& allowed_types = tmpl.role_types.size() > (depth + 1) ? tmpl.role_types[depth + 1] : std::vector<std::string>();
                    if (!matches_role_type(e.tgt, allowed_types)) continue;

                    visited.insert(e.tgt);
                    path_nodes.push_back(e.tgt);
                    path_edges.push_back(e);
                    dfs(depth + 1, e.tgt);
                    path_nodes.pop_back();
                    path_edges.pop_back();
                    visited.erase(e.tgt);

                    if (stop) break;
                }
            };

            dfs(0, start);
        }
    };

    for (const auto& tmpl : templates) {
        build_candidates(tmpl);
        if (candidates.size() >= config_.hypothesis_2_max_candidates) break;
    }

    if (candidates.empty() && config_.hypothesis_2_enable_untyped_fallback) {
        ChainTemplate fallback{
            "generic_chain",
            {"Entity A", "Entity B", "Entity C"},
            {},
            {}
        };
        build_candidates(fallback);
    }

    if (candidates.empty()) {
        report_progress("Generating mechanistic hypotheses", 100, 100);
        return results;
    }

    int max_pattern = 0;
    for (const auto& entry : pattern_counts) {
        if (entry.second > max_pattern) max_pattern = entry.second;
    }

    report_progress("Generating mechanistic hypotheses", 40, 100);

    for (auto& cand : candidates) {
        cand.support = (max_pattern > 0) ? static_cast<double>(pattern_counts[cand.pattern_key]) / max_pattern : 0.0;
        cand.novelty = 1.0 - cand.support;

        cand.chain_score =
            config_.hypothesis_2_weight_support * cand.support +
            config_.hypothesis_2_weight_relation_strength * cand.relation_strength +
            config_.hypothesis_2_weight_type_fidelity * cand.type_fidelity +
            config_.hypothesis_2_weight_novelty * cand.novelty +
            config_.hypothesis_2_weight_coherence * cand.coherence;

        std::unordered_set<std::string> expl_nodes(cand.nodes.begin(), cand.nodes.end());
        std::unordered_set<std::string> expl_edges(cand.edge_ids.begin(), cand.edge_ids.end());
        std::vector<EdgeInfo> evidence_candidates;

        std::vector<std::string> keyword_union;
        keyword_union.reserve(cand.relations.size());
        for (const auto& rel : cand.relations) {
            keyword_union.push_back(rel);
        }

        for (const auto& node : cand.nodes) {
            size_t degree_checked = 0;
            auto out_it = out_edges.find(node);
            if (out_it != out_edges.end()) {
                for (const auto& e : out_it->second) {
                    if (degree_checked++ >= config_.hypothesis_2_explanation_degree_cap) break;
                    if (e.confidence < config_.hypothesis_2_explanation_min_confidence) continue;
                    evidence_candidates.push_back(e);
                }
            }
            degree_checked = 0;
            auto in_it = in_edges.find(node);
            if (in_it != in_edges.end()) {
                for (const auto& e : in_it->second) {
                    if (degree_checked++ >= config_.hypothesis_2_explanation_degree_cap) break;
                    if (e.confidence < config_.hypothesis_2_explanation_min_confidence) continue;
                    evidence_candidates.push_back(e);
                }
            }
        }

        auto candidate_score = [&](const EdgeInfo& e) {
            bool aligned = relation_matches(e.relation, keyword_union);
            double align_score = aligned ? 1.0 : 0.4;
            return align_score * 0.6 + e.confidence * 0.4;
        };

        std::sort(evidence_candidates.begin(), evidence_candidates.end(),
                  [&](const EdgeInfo& a, const EdgeInfo& b) {
                      return candidate_score(a) > candidate_score(b);
                  });

        double align_sum = 0.0;
        double conf_sum = 0.0;
        size_t added_edges = 0;

        for (const auto& e : evidence_candidates) {
            if (expl_edges.size() >= config_.hypothesis_2_explanation_max_edges) break;
            if (expl_edges.count(e.edge_id)) continue;

            size_t new_nodes = 0;
            if (!expl_nodes.count(e.src)) new_nodes++;
            if (!expl_nodes.count(e.tgt)) new_nodes++;
            if (expl_nodes.size() + new_nodes > config_.hypothesis_2_explanation_max_nodes) continue;

            expl_edges.insert(e.edge_id);
            expl_nodes.insert(e.src);
            expl_nodes.insert(e.tgt);

            bool aligned = relation_matches(e.relation, keyword_union);
            align_sum += aligned ? 1.0 : 0.0;
            conf_sum += e.confidence;
            added_edges++;
        }

        cand.explanation_nodes.assign(expl_nodes.begin(), expl_nodes.end());
        cand.explanation_edges.assign(expl_edges.begin(), expl_edges.end());

        cand.explanation_alignment = added_edges > 0 ? align_sum / added_edges : 0.0;
        cand.explanation_confidence = added_edges > 0 ? conf_sum / added_edges : 0.0;
        cand.explanation_density = expl_nodes.empty() ? 0.0 : static_cast<double>(expl_edges.size()) / expl_nodes.size();
        cand.explanation_connectivity = cand.nodes.empty() ? 0.0 : 1.0;

        cand.explanation_score =
            0.35 * cand.explanation_alignment +
            0.35 * cand.explanation_confidence +
            0.20 * std::min(1.0, cand.explanation_density / 1.5) +
            0.10 * cand.explanation_connectivity;

        cand.final_score =
            config_.hypothesis_2_weight_chain * cand.chain_score +
            config_.hypothesis_2_weight_explanation * cand.explanation_score;
    }

    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [this](const ChainCandidate& c) {
            return c.chain_score < config_.hypothesis_2_min_chain_score ||
                   c.explanation_score < config_.hypothesis_2_min_explanation_score ||
                   c.final_score < config_.hypothesis_2_min_final_score;
        }), candidates.end());

    if (candidates.empty()) {
        report_progress("Generating mechanistic hypotheses", 100, 100);
        return results;
    }

    std::sort(candidates.begin(), candidates.end(), [](const ChainCandidate& a, const ChainCandidate& b) {
        return a.final_score > b.final_score;
    });

    size_t num_hypotheses = std::min(candidates.size(), config_.hypothesis_2_count);

    for (size_t i = 0; i < num_hypotheses; ++i) {
        const auto& cand = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::HYPOTHESES_2);
        ins.set_type(InsightType::HYPOTHESES_2);

        if (!cand.nodes.empty()) {
            ins.seed_nodes = {cand.nodes.front(), cand.nodes.back()};
            ins.seed_labels = {get_node_label(cand.nodes.front()), get_node_label(cand.nodes.back())};
        }

        ins.witness_nodes = cand.explanation_nodes;
        ins.witness_edges = cand.explanation_edges;

        std::ostringstream desc;
        desc << "Mechanistic chain (" << cand.template_name << "): ";
        for (size_t n = 0; n < cand.nodes.size(); ++n) {
            if (n > 0 && n - 1 < cand.relations.size()) {
                desc << " --" << cand.relations[n - 1] << "--> ";
            }
            if (n < cand.role_labels.size()) {
                desc << cand.role_labels[n] << " ";
            }
            desc << "'" << get_node_label(cand.nodes[n]) << "'";
        }
        size_t extra_nodes = cand.explanation_nodes.size() > cand.nodes.size() ?
                             (cand.explanation_nodes.size() - cand.nodes.size()) : 0;
        size_t extra_edges = cand.explanation_edges.size() > cand.edge_ids.size() ?
                             (cand.explanation_edges.size() - cand.edge_ids.size()) : 0;
        desc << ". Explanation subgraph adds " << extra_nodes << " nodes and "
             << extra_edges << " edges.";
        ins.description = desc.str();

        ins.score_breakdown["chain_support"] = cand.support;
        ins.score_breakdown["relation_strength"] = cand.relation_strength;
        ins.score_breakdown["type_fidelity"] = cand.type_fidelity;
        ins.score_breakdown["novelty"] = cand.novelty;
        ins.score_breakdown["coherence"] = cand.coherence;
        ins.score_breakdown["chain_score"] = cand.chain_score;
        ins.score_breakdown["explanation_alignment"] = cand.explanation_alignment;
        ins.score_breakdown["explanation_confidence"] = cand.explanation_confidence;
        ins.score_breakdown["explanation_density"] = cand.explanation_density;
        ins.score_breakdown["explanation_score"] = cand.explanation_score;
        ins.score = cand.final_score;

        std::unordered_set<std::string> chunks;
        for (const auto& edge_id : ins.witness_edges) {
            auto it = edge_chunk.find(edge_id);
            if (it != edge_chunk.end()) chunks.insert(it->second);
        }
        for (const auto& chunk : chunks) ins.evidence_chunk_ids.push_back(chunk);

        results.push_back(ins);
    }

    report_progress("Generating mechanistic hypotheses", 100, 100);
    return results;
}

// =============================================================================
// HYPOTHESES_3: Counterfactual Causal Hypothesis Generation
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_hypotheses_3(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Generating counterfactual hypotheses", 0, 100);

    // Step 1: Identify causal chains and intervention points from collection
    std::vector<Insight> causal_chains;
    std::vector<Insight> intervention_points;

    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::CAUSAL_CHAIN) {
            causal_chains.push_back(insight);
        } else if (insight.type == InsightType::INTERVENTION_POINT) {
            intervention_points.push_back(insight);
        }
    }

    if (causal_chains.empty() || intervention_points.empty()) {
        std::cerr << "[WARNING] hypotheses_3 requires causal_chain and intervention_point insights.\n";
        std::cerr << "          Found " << causal_chains.size() << " causal_chain(s) and "
                  << intervention_points.size() << " intervention_point(s).\n";
        std::cerr << "          Run with operators: -p \"causal_chains,intervention_points,hypotheses_3\"\n";
        report_progress("Generating counterfactual hypotheses", 100, 100);
        return results; // Need causal infrastructure
    }

    report_progress("Generating counterfactual hypotheses", 20, 100);

    // Step 2: Generate counterfactual candidates
    struct CounterfactualCandidate {
        std::string intervention_node;
        std::vector<std::string> affected_nodes;
        std::vector<std::string> causal_path;
        double effect_size;
        double centrality_score;
        double mechanism_coherence;
        double testability;
        double confounding_risk;
        std::vector<std::string> evidence_edges;
        double final_score;
    };

    std::vector<CounterfactualCandidate> candidates;

    // For each intervention point
    for (const auto& intervention : intervention_points) {
        if (intervention.seed_nodes.empty()) continue;

        std::string int_node = intervention.seed_nodes[0];

        // Find causal chains containing this node (not just starting with it)
        for (const auto& chain : causal_chains) {
            if (chain.seed_nodes.empty()) continue;

            // Find if intervention node appears anywhere in the chain
            auto node_it = std::find(chain.seed_nodes.begin(), chain.seed_nodes.end(), int_node);
            if (node_it == chain.seed_nodes.end()) continue;

            // Get position of intervention node in chain
            size_t int_pos = std::distance(chain.seed_nodes.begin(), node_it);

            CounterfactualCandidate cand;
            cand.intervention_node = int_node;
            cand.causal_path = chain.seed_nodes;
            // Affected nodes are everything AFTER the intervention point
            cand.affected_nodes = std::vector<std::string>(chain.seed_nodes.begin() + int_pos + 1, chain.seed_nodes.end());
            cand.evidence_edges = chain.witness_edges;

            // Estimate effect size based on chain length and centrality
            cand.effect_size = std::min(static_cast<double>(cand.affected_nodes.size()) / 5.0, 1.0);

            // Centrality score from intervention point
            auto it = intervention.score_breakdown.find("criticality");
            cand.centrality_score = (it != intervention.score_breakdown.end()) ? it->second : 0.5;

            // Mechanism coherence (simplified - would use LLM)
            cand.mechanism_coherence = chain.score;

            // Testability based on observability
            cand.testability = std::min(static_cast<double>(cand.evidence_edges.size()) / 5.0, 1.0);

            // Confounding risk (check for common causes)
            cand.confounding_risk = 0.3; // Placeholder

            // Final weighted score
            cand.final_score = config_.hypothesis_3_weight_centrality * cand.centrality_score +
                              config_.hypothesis_3_weight_effect_size * cand.effect_size +
                              config_.hypothesis_3_weight_mechanism * cand.mechanism_coherence +
                              config_.hypothesis_3_weight_testability * cand.testability -
                              config_.hypothesis_3_weight_confounding * cand.confounding_risk;

            candidates.push_back(cand);
        }
    }

    if (candidates.empty()) {
        report_progress("Generating counterfactual hypotheses", 100, 100);
        return results;
    }

    report_progress("Generating counterfactual hypotheses", 60, 100);

    // Filter by minimum thresholds
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [this](const CounterfactualCandidate& c) {
            return c.effect_size < config_.hypothesis_3_min_effect_size;
        }), candidates.end());

    // Sort by final score
    std::sort(candidates.begin(), candidates.end(), [](const CounterfactualCandidate& a, const CounterfactualCandidate& b) {
        return a.final_score > b.final_score;
    });

    // Take top candidates
    size_t num_hypotheses = std::min(candidates.size(), config_.hypothesis_3_count);

    for (size_t i = 0; i < num_hypotheses; ++i) {
        const auto& cand = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::HYPOTHESES_3);
        ins.set_type(InsightType::HYPOTHESES_3);

        ins.seed_nodes = cand.causal_path;
        for (const auto& node : ins.seed_nodes) {
            ins.seed_labels.push_back(get_node_label(node));
        }

        ins.witness_nodes = cand.causal_path;
        ins.witness_edges = cand.evidence_edges;

        std::string effect_desc = (cand.affected_nodes.size() == 1) ? " 1 downstream effect" :
                                 " " + std::to_string(cand.affected_nodes.size()) + " downstream effects";

        ins.description = "Counterfactual hypothesis: Intervene on '" + get_node_label(cand.intervention_node) +
                         "' → predicts" + effect_desc;

        ins.score_breakdown["centrality"] = cand.centrality_score;
        ins.score_breakdown["effect_size"] = cand.effect_size;
        ins.score_breakdown["mechanism"] = cand.mechanism_coherence;
        ins.score_breakdown["testability"] = cand.testability;
        ins.score_breakdown["confounding_risk"] = cand.confounding_risk;
        ins.score = cand.final_score;

        results.push_back(ins);
    }

    report_progress("Generating counterfactual hypotheses", 100, 100);
    return results;
}

// =============================================================================
// MECHANISM CONSOLIDATION: Cluster related mechanistic hypotheses and chains
// =============================================================================

// REMOVED: Low-value insight (2 insights across 9 runs)
#if 0
std::vector<Insight> DiscoveryEngine::find_mechanism_consolidations(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Consolidating mechanisms", 0, 100);

    struct Candidate {
        Insight insight;
        std::unordered_set<std::string> nodes;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(collection.insights.size());

    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::HYPOTHESES_2 &&
            insight.type != InsightType::LONG_CHAIN &&
            insight.type != InsightType::EXPLANATORY_CHAIN) {
            continue;
        }

        std::unordered_set<std::string> nodes;
        for (const auto& n : insight.seed_nodes) nodes.insert(n);
        for (const auto& n : insight.witness_nodes) nodes.insert(n);
        if (nodes.empty()) continue;

        candidates.push_back({insight, std::move(nodes)});
    }

    if (candidates.empty()) {
        report_progress("Consolidating mechanisms", 100, 100);
        return results;
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.insight.score > b.insight.score;
    });

    if (candidates.size() > config_.mechanism_consolidation_max_sources) {
        candidates.resize(config_.mechanism_consolidation_max_sources);
    }

    size_t n = candidates.size();
    std::vector<int> parent(n);
    std::vector<int> rank(n, 0);
    for (size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);

    auto find_root = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    auto union_sets = [&](int a, int b) {
        int ra = find_root(a);
        int rb = find_root(b);
        if (ra == rb) return;
        if (rank[ra] < rank[rb]) {
            parent[ra] = rb;
        } else if (rank[ra] > rank[rb]) {
            parent[rb] = ra;
        } else {
            parent[rb] = ra;
            rank[ra]++;
        }
    };

    auto jaccard = [](const std::unordered_set<std::string>& a,
                      const std::unordered_set<std::string>& b) {
        if (a.empty() || b.empty()) return 0.0;
        const auto* small = &a;
        const auto* large = &b;
        if (a.size() > b.size()) {
            small = &b;
            large = &a;
        }
        size_t intersect = 0;
        for (const auto& v : *small) {
            if (large->count(v)) intersect++;
        }
        size_t uni = a.size() + b.size() - intersect;
        return uni > 0 ? static_cast<double>(intersect) / static_cast<double>(uni) : 0.0;
    };

    std::vector<std::vector<double>> similarity(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            double sim = jaccard(candidates[i].nodes, candidates[j].nodes);
            similarity[i][j] = sim;
            similarity[j][i] = sim;
            if (sim >= config_.mechanism_consolidation_min_similarity) {
                union_sets(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }

    std::unordered_map<int, std::vector<size_t>> clusters;
    for (size_t i = 0; i < n; ++i) {
        clusters[find_root(static_cast<int>(i))].push_back(i);
    }

    struct ClusterResult {
        Insight insight;
        double score = 0.0;
    };
    std::vector<ClusterResult> consolidated;

    for (const auto& entry : clusters) {
        const auto& members = entry.second;
        if (members.size() < config_.mechanism_consolidation_min_cluster_size) continue;

        std::unordered_map<std::string, int> node_counts;
        std::unordered_set<std::string> edge_set;
        std::unordered_set<std::string> chunk_set;
        std::map<InsightType, int> type_counts;
        double score_sum = 0.0;

        for (size_t idx : members) {
            const auto& cand = candidates[idx];
            score_sum += cand.insight.score;
            type_counts[cand.insight.type]++;
            for (const auto& node : cand.nodes) node_counts[node]++;
            for (const auto& edge : cand.insight.witness_edges) {
                if (edge_set.size() >= config_.mechanism_consolidation_max_edges) break;
                edge_set.insert(edge);
            }
            for (const auto& chunk : cand.insight.evidence_chunk_ids) {
                chunk_set.insert(chunk);
            }
        }

        double avg_score = members.empty() ? 0.0 : (score_sum / members.size());

        double cohesion = 1.0;
        if (members.size() > 1) {
            double sum = 0.0;
            size_t pairs = 0;
            for (size_t i = 0; i < members.size(); ++i) {
                for (size_t j = i + 1; j < members.size(); ++j) {
                    sum += similarity[members[i]][members[j]];
                    pairs++;
                }
            }
            cohesion = pairs > 0 ? (sum / pairs) : 1.0;
        }

        double size_score = std::min(1.0, static_cast<double>(members.size()) / 5.0);
        double final_score =
            config_.mechanism_consolidation_weight_size * size_score +
            config_.mechanism_consolidation_weight_cohesion * cohesion +
            config_.mechanism_consolidation_weight_score * avg_score;

        std::vector<std::pair<std::string, int>> ranked_nodes(node_counts.begin(), node_counts.end());
        std::sort(ranked_nodes.begin(), ranked_nodes.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::MECHANISM_CONSOLIDATION);
        ins.set_type(InsightType::MECHANISM_CONSOLIDATION);

        size_t seed_count = std::min<size_t>(2, ranked_nodes.size());
        for (size_t i = 0; i < seed_count; ++i) {
            ins.seed_nodes.push_back(ranked_nodes[i].first);
            ins.seed_labels.push_back(get_node_label(ranked_nodes[i].first));
        }

        size_t max_nodes = std::min(ranked_nodes.size(), config_.mechanism_consolidation_max_nodes);
        for (size_t i = 0; i < max_nodes; ++i) {
            ins.witness_nodes.push_back(ranked_nodes[i].first);
        }

        ins.witness_edges.assign(edge_set.begin(), edge_set.end());
        if (ins.witness_edges.size() > config_.mechanism_consolidation_max_edges) {
            ins.witness_edges.resize(config_.mechanism_consolidation_max_edges);
        }

        ins.evidence_chunk_ids.assign(chunk_set.begin(), chunk_set.end());
        if (ins.evidence_chunk_ids.size() > 10) {
            ins.evidence_chunk_ids.resize(10);
        }

        std::ostringstream desc;
        desc << "Mechanism consolidation: clustered " << members.size() << " insights ("
             << type_counts[InsightType::HYPOTHESES_2] << " mechanistic hypotheses, "
             << type_counts[InsightType::LONG_CHAIN] << " long-chain links, "
             << type_counts[InsightType::EXPLANATORY_CHAIN] << " explanatory chains).";
        if (!ranked_nodes.empty()) {
            desc << " Core nodes: ";
            size_t list_max = std::min<size_t>(3, ranked_nodes.size());
            for (size_t i = 0; i < list_max; ++i) {
                if (i > 0) desc << ", ";
                desc << "'" << get_node_label(ranked_nodes[i].first) << "'";
            }
            if (ranked_nodes.size() > list_max) desc << ", ...";
        }
        ins.description = desc.str();

        ins.score_breakdown["cluster_size"] = static_cast<double>(members.size());
        ins.score_breakdown["cohesion"] = cohesion;
        ins.score_breakdown["avg_member_score"] = avg_score;
        ins.score = final_score;

        ins.novelty_tags = {"mechanism_consolidation", "cluster_size=" + std::to_string(members.size())};

        consolidated.push_back({ins, final_score});
    }

    if (consolidated.empty()) {
        report_progress("Consolidating mechanisms", 100, 100);
        return results;
    }

    std::sort(consolidated.begin(), consolidated.end(), [](const ClusterResult& a, const ClusterResult& b) {
        return a.score > b.score;
    });

    size_t limit = std::min(consolidated.size(), config_.mechanism_consolidation_max_clusters);
    for (size_t i = 0; i < limit; ++i) {
        results.push_back(std::move(consolidated[i].insight));
    }

    report_progress("Consolidating mechanisms", 100, 100);
    return results;
}
#endif // REMOVED: mechanism_consolidation

// =============================================================================
// CAUSAL_CHAIN: Directed Causal Path Detection
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_causal_chains() {
    std::vector<Insight> results;
    report_progress("Finding causal chains", 0, 100);

    // Identify causal keywords in edge relations
    std::set<std::string> causal_keywords = {
        "causes", "leads to", "results in", "produces", "triggers",
        "influences", "affects", "determines", "drives", "enables"
    };

    // Build directed causal graph
    std::map<std::string, std::vector<std::string>> causal_edges;
    std::map<std::pair<std::string, std::string>, std::string> edge_relation;
    std::map<std::string, double> edge_strength;  // Phase 2: Store causal strength

    for (const auto& edge : graph_.get_all_edges()) {
        bool is_causal = false;
        double strength = 0.5;  // Default strength

        // Phase 2: Check for causal metadata first
        if (edge.is_causal()) {
            is_causal = true;
            strength = edge.causal_metadata->get_strength_score();
        } else {
            // Fallback: Check for causal keywords in relation
            std::string rel = edge.relation;
            std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);

            for (const auto& keyword : causal_keywords) {
                if (rel.find(keyword) != std::string::npos) {
                    is_causal = true;
                    break;
                }
            }
        }

        if (is_causal && !edge.sources.empty() && !edge.targets.empty()) {
            std::string src = edge.sources[0];
            std::string tgt = edge.targets[0];
            causal_edges[src].push_back(tgt);
            edge_relation[{src, tgt}] = edge.id;
            edge_strength[edge.id] = strength;  // Phase 2: Store strength
        }
    }

    report_progress("Finding causal chains", 30, 100);

    // Find causal chains (paths of length 2+)
    struct CausalChain {
        std::vector<std::string> path;
        std::vector<std::string> edge_ids;
        double chain_strength;
    };

    std::vector<CausalChain> chains;
    const size_t max_chain_length = 5;

    // DFS to find chains
    for (const auto& [start, _] : causal_edges) {
        std::function<void(std::string, std::vector<std::string>, std::vector<std::string>, std::set<std::string>&)>
        dfs = [&](std::string node, std::vector<std::string> path, std::vector<std::string> edges, std::set<std::string>& visited) {
            if (path.size() >= max_chain_length) return;

            for (const auto& next : causal_edges[node]) {
                if (visited.count(next)) continue; // Avoid cycles in chain detection

                std::vector<std::string> new_path = path;
                new_path.push_back(next);

                std::vector<std::string> new_edges = edges;
                auto it = edge_relation.find({node, next});
                if (it != edge_relation.end()) {
                    new_edges.push_back(it->second);
                }

                if (new_path.size() >= 3) { // Chain of at least 3 nodes
                    CausalChain chain;
                    chain.path = new_path;
                    chain.edge_ids = new_edges;

                    // Phase 2: Calculate chain strength from edge strengths
                    double total_strength = 0.0;
                    int edge_count = 0;
                    for (const auto& eid : new_edges) {
                        auto it = edge_strength.find(eid);
                        if (it != edge_strength.end()) {
                            total_strength += it->second;
                            edge_count++;
                        }
                    }
                    // Average strength, penalized by chain length
                    chain.chain_strength = edge_count > 0 ?
                        (total_strength / edge_count) * (1.0 / std::sqrt(new_path.size())) :
                        1.0 / new_path.size();

                    chains.push_back(chain);
                }

                std::set<std::string> new_visited = visited;
                new_visited.insert(next);
                dfs(next, new_path, new_edges, new_visited);
            }
        };

        std::vector<std::string> initial_path = {start};
        std::set<std::string> visited = {start};
        dfs(start, initial_path, {}, visited);

        if (chains.size() > 200) break; // Limit total chains
    }

    report_progress("Finding causal chains", 70, 100);

    // Phase 2: Sort by chain strength first, then length
    std::sort(chains.begin(), chains.end(), [](const CausalChain& a, const CausalChain& b) {
        // Primary: Sort by strength (higher is better)
        if (std::abs(a.chain_strength - b.chain_strength) > 0.01) {
            return a.chain_strength > b.chain_strength;
        }
        // Secondary: Sort by path length (longer is more informative)
        return a.path.size() > b.path.size();
    });

    // Create insights
    size_t max_chains = std::min(chains.size(), static_cast<size_t>(config_.max_total_insights / 5));
    for (size_t i = 0; i < max_chains; ++i) {
        const auto& chain = chains[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CAUSAL_CHAIN);
        ins.set_type(InsightType::CAUSAL_CHAIN);
        ins.seed_nodes = chain.path;
        for (const auto& node : chain.path) {
            ins.seed_labels.push_back(get_node_label(node));
        }
        ins.witness_nodes = chain.path;
        ins.witness_edges = chain.edge_ids;

        ins.description = "Causal chain: " + get_node_label(chain.path[0]) + " → ... → " +
                         get_node_label(chain.path.back()) + " (" + std::to_string(chain.path.size()) + " steps)";

        ins.score = chain.chain_strength * 0.5 + (static_cast<double>(chain.path.size()) / max_chain_length) * 0.5;
        ins.score_breakdown["chain_length"] = chain.path.size();
        ins.score_breakdown["chain_strength"] = chain.chain_strength;

        results.push_back(ins);
    }

    report_progress("Finding causal chains", 100, 100);
    return results;
}

// =============================================================================
// INTERVENTION_POINT: Critical Control Points in Causal Pathways
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_intervention_points() {
    std::vector<Insight> results;
    report_progress("Finding intervention points", 0, 100);

    // Build causal graph (same as causal_chains)
    std::set<std::string> causal_keywords = {
        "causes", "leads to", "results in", "produces", "triggers",
        "influences", "affects", "determines", "drives", "enables"
    };

    std::map<std::string, std::vector<std::string>> causal_edges;
    std::set<std::string> causal_nodes;
    std::map<std::pair<std::string, std::string>, double> edge_importance;  // Phase 2: Track link importance

    for (const auto& edge : graph_.get_all_edges()) {
        bool is_causal = false;
        double importance = 1.0;  // Default importance

        // Phase 2: Check for causal metadata first
        if (edge.is_causal()) {
            is_causal = true;
            const auto& causal = *edge.causal_metadata;

            // Calculate importance based on type and strength
            double type_weight = 1.0;
            if (causal.type == CausalRelationType::NECESSARY) {
                type_weight = 2.0;  // NECESSARY links are high-priority interventions
            } else if (causal.type == CausalRelationType::SUFFICIENT) {
                type_weight = 1.5;  // SUFFICIENT links are also important
            } else if (causal.type == CausalRelationType::DIRECT_CAUSE) {
                type_weight = 1.3;
            }

            importance = causal.get_strength_score() * type_weight;
        } else {
            // Fallback: Check for causal keywords in relation
            std::string rel = edge.relation;
            std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);

            for (const auto& keyword : causal_keywords) {
                if (rel.find(keyword) != std::string::npos) {
                    is_causal = true;
                    break;
                }
            }
        }

        if (is_causal && !edge.sources.empty() && !edge.targets.empty()) {
            std::string src = edge.sources[0];
            std::string tgt = edge.targets[0];
            causal_edges[src].push_back(tgt);
            causal_nodes.insert(src);
            causal_nodes.insert(tgt);
            edge_importance[{src, tgt}] = importance;  // Phase 2: Store importance
        }
    }

    report_progress("Finding intervention points", 30, 100);

    // Phase 2: Calculate importance-weighted betweenness centrality
    std::map<std::string, double> betweenness;
    std::map<std::string, double> weighted_importance;  // Sum of importance through node

    for (const auto& node : causal_nodes) {
        betweenness[node] = 0.0;
        weighted_importance[node] = 0.0;
    }

    // Betweenness: count paths that go through each node
    for (const auto& start : causal_nodes) {
        std::map<std::string, int> path_count;

        // BFS from start node
        std::queue<std::string> q;
        std::set<std::string> visited;
        q.push(start);
        visited.insert(start);

        while (!q.empty()) {
            std::string current = q.front();
            q.pop();

            for (const auto& next : causal_edges[current]) {
                if (!visited.count(next)) {
                    visited.insert(next);
                    q.push(next);
                    path_count[next]++;

                    // Phase 2: Accumulate importance through this path
                    auto imp_it = edge_importance.find({current, next});
                    if (imp_it != edge_importance.end()) {
                        weighted_importance[next] += imp_it->second;
                    }
                }
            }
        }

        // Add to betweenness scores
        for (const auto& [node, count] : path_count) {
            if (node != start) {
                betweenness[node] += count;
            }
        }
    }

    report_progress("Finding intervention points", 70, 100);

    // Also consider out-degree (how many downstream effects)
    std::map<std::string, int> out_degree;
    for (const auto& [src, targets] : causal_edges) {
        out_degree[src] = targets.size();
    }

    // Phase 2: Create insights with importance-weighted scoring
    std::vector<std::pair<std::string, double>> ranked;
    for (const auto& [node, score] : betweenness) {
        if (score > 0) {
            // Phase 2: Combine betweenness, out-degree, and weighted importance
            double normalized_importance = weighted_importance[node] / std::max(1.0, score);
            double combined = score * 0.5 +                           // Path centrality
                            out_degree[node] * 0.2 +                  // Downstream effects
                            normalized_importance * 0.3;              // Causal importance
            ranked.push_back({node, combined});
        }
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    size_t max_results = std::min(ranked.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_results; ++i) {
        const auto& [node, score] = ranked[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::INTERVENTION_POINT);
        ins.set_type(InsightType::INTERVENTION_POINT);
        ins.seed_nodes = {node};
        ins.seed_labels = {get_node_label(node)};
        ins.witness_nodes = {node};

        ins.description = "Intervention point: '" + get_node_label(node) +
                         "' controls " + std::to_string(static_cast<int>(betweenness[node])) + " causal paths";

        double normalized_score = std::min(score / 10.0, 1.0);
        ins.score = normalized_score;
        ins.score_breakdown["betweenness"] = betweenness[node];
        ins.score_breakdown["out_degree"] = out_degree[node];
        ins.score_breakdown["criticality"] = normalized_score;

        results.push_back(ins);
    }

    report_progress("Finding intervention points", 100, 100);
    return results;
}

// =============================================================================
// FEEDBACK_LOOP: Causal Cycles Detection
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_feedback_loops() {
    std::vector<Insight> results;
    report_progress("Finding feedback loops", 0, 100);

    // Build causal/directional graph
    std::set<std::string> directional_keywords = {
        "causes", "leads to", "results in", "produces", "triggers",
        "influences", "affects", "determines", "drives", "enables",
        "feeds into", "reinforces", "amplifies"
    };

    std::map<std::string, std::vector<std::string>> directed_edges;
    std::map<std::pair<std::string, std::string>, std::string> edge_ids;
    std::map<std::string, double> edge_strengths;         // Phase 2: Store edge strengths
    std::map<std::string, bool> edge_immediate;          // Phase 2: Track immediate feedback

    for (const auto& edge : graph_.get_all_edges()) {
        bool is_directional = false;
        double strength = 0.5;  // Default
        bool immediate = false;

        // Phase 2: Check for causal metadata first
        if (edge.is_causal()) {
            is_directional = true;
            const auto& causal = *edge.causal_metadata;
            strength = causal.get_strength_score();
            immediate = (causal.temporality == Temporality::IMMEDIATE);
        } else {
            // Fallback: Check for directional keywords
            std::string rel = edge.relation;
            std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);

            for (const auto& keyword : directional_keywords) {
                if (rel.find(keyword) != std::string::npos) {
                    is_directional = true;
                    break;
                }
            }
        }

        if (is_directional && !edge.sources.empty() && !edge.targets.empty()) {
            std::string src = edge.sources[0];
            std::string tgt = edge.targets[0];
            directed_edges[src].push_back(tgt);
            edge_ids[{src, tgt}] = edge.id;
            edge_strengths[edge.id] = strength;      // Phase 2: Store
            edge_immediate[edge.id] = immediate;     // Phase 2: Store
        }
    }

    report_progress("Finding feedback loops", 30, 100);

    // Phase 2: Enhanced feedback loop structure
    struct FeedbackLoop {
        std::vector<std::string> cycle;
        std::vector<std::string> edges;
        bool is_positive;            // reinforcing vs balancing
        double avg_strength = 0.5;   // Phase 2: Average causal strength
        bool has_immediate = false;  // Phase 2: Has immediate feedback
        bool has_delayed = false;    // Phase 2: Has delayed feedback
    };

    std::vector<FeedbackLoop> loops;
    std::set<std::string> processed_cycles; // Avoid duplicates

    std::function<void(std::string, std::vector<std::string>, std::vector<std::string>, std::set<std::string>&)>
    find_cycles = [&](std::string start, std::vector<std::string> path, std::vector<std::string> edges, std::set<std::string>& visited) {
        if (path.size() > 8) return; // Limit cycle length

        for (const auto& next : directed_edges[path.back()]) {
            // Check if we've completed a cycle back to start
            if (next == start && path.size() >= 3) {
                // Found a cycle
                FeedbackLoop loop;
                loop.cycle = path;
                loop.edges = edges;

                auto it = edge_ids.find({path.back(), start});
                if (it != edge_ids.end()) {
                    loop.edges.push_back(it->second);
                }

                // Phase 2: Calculate loop properties from edge metadata
                double total_strength = 0.0;
                int edge_count = 0;
                int immediate_count = 0;
                int delayed_count = 0;

                for (const auto& eid : loop.edges) {
                    auto str_it = edge_strengths.find(eid);
                    if (str_it != edge_strengths.end()) {
                        total_strength += str_it->second;
                        edge_count++;
                    }

                    auto imm_it = edge_immediate.find(eid);
                    if (imm_it != edge_immediate.end()) {
                        if (imm_it->second) {
                            immediate_count++;
                        } else {
                            delayed_count++;
                        }
                    }
                }

                loop.avg_strength = edge_count > 0 ? total_strength / edge_count : 0.5;
                loop.has_immediate = (immediate_count > 0);
                loop.has_delayed = (delayed_count > 0);
                loop.is_positive = (loop.avg_strength > 0.5); // Strong = reinforcing

                // Create a canonical representation to avoid duplicates
                std::vector<std::string> sorted_cycle = path;
                std::sort(sorted_cycle.begin(), sorted_cycle.end());
                std::string cycle_key;
                for (const auto& n : sorted_cycle) cycle_key += n + ",";

                if (!processed_cycles.count(cycle_key)) {
                    processed_cycles.insert(cycle_key);
                    loops.push_back(loop);
                }
                continue;
            }

            if (visited.count(next) || next == start) continue;

            std::vector<std::string> new_path = path;
            new_path.push_back(next);

            std::vector<std::string> new_edges = edges;
            auto it = edge_ids.find({path.back(), next});
            if (it != edge_ids.end()) {
                new_edges.push_back(it->second);
            }

            std::set<std::string> new_visited = visited;
            new_visited.insert(next);
            find_cycles(start, new_path, new_edges, new_visited);
        }
    };

    int node_count = 0;
    for (const auto& [start, _] : directed_edges) {
        std::set<std::string> visited = {start};
        find_cycles(start, {start}, {}, visited);

        if (++node_count > 50 || loops.size() > 100) break; // Limit search
    }

    report_progress("Finding feedback loops", 70, 100);

    // Phase 2: Sort by strength first, then cycle length
    std::sort(loops.begin(), loops.end(), [](const FeedbackLoop& a, const FeedbackLoop& b) {
        // Primary: Sort by strength (higher = more important)
        if (std::abs(a.avg_strength - b.avg_strength) > 0.1) {
            return a.avg_strength > b.avg_strength;
        }
        // Secondary: Prefer shorter, more direct cycles
        return a.cycle.size() < b.cycle.size();
    });

    // Create insights
    size_t max_loops = std::min(loops.size(), static_cast<size_t>(15));
    for (size_t i = 0; i < max_loops; ++i) {
        const auto& loop = loops[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::FEEDBACK_LOOP);
        ins.set_type(InsightType::FEEDBACK_LOOP);
        ins.seed_nodes = loop.cycle;
        for (const auto& node : loop.cycle) {
            ins.seed_labels.push_back(get_node_label(node));
        }
        ins.witness_nodes = loop.cycle;
        ins.witness_edges = loop.edges;

        std::string loop_type = loop.is_positive ? "reinforcing" : "balancing";
        ins.description = "Feedback loop (" + loop_type + "): " +
                         get_node_label(loop.cycle[0]) + " ⟲ (" +
                         std::to_string(loop.cycle.size()) + " nodes)";

        ins.score = 1.0 / (loop.cycle.size() + 1); // Shorter loops = higher score
        ins.score_breakdown["cycle_length"] = loop.cycle.size();
        ins.score_breakdown["loop_type"] = loop.is_positive ? 1.0 : 0.5;

        results.push_back(ins);
    }

    report_progress("Finding feedback loops", 100, 100);
    return results;
}

// =============================================================================
// CONFOUNDER: Common Cause Detection
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_confounders() {
    std::vector<Insight> results;
    report_progress("Finding confounders", 0, 100);

    // Find pairs of nodes that might have a causal relationship
    std::map<std::string, std::set<std::string>> neighbors;

    for (const auto& edge : graph_.get_all_edges()) {
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                neighbors[src].insert(tgt);
                neighbors[tgt].insert(src);
            }
        }
    }

    report_progress("Finding confounders", 20, 100);

    // Look for nodes that connect to both ends of potential causal pairs
    struct ConfounderCandidate {
        std::string confounder_node;
        std::string node1;
        std::string node2;
        int shared_connections;
        double confounding_strength;
    };

    std::vector<ConfounderCandidate> candidates;
    auto all_nodes = graph_.get_all_nodes();

    for (size_t i = 0; i < all_nodes.size() && i < 100; ++i) {
        for (size_t j = i + 1; j < all_nodes.size() && j < 100; ++j) {
            std::string n1 = all_nodes[i].id;
            std::string n2 = all_nodes[j].id;

            // Find common neighbors (potential confounders)
            std::vector<std::string> common;
            std::set_intersection(neighbors[n1].begin(), neighbors[n1].end(),
                                neighbors[n2].begin(), neighbors[n2].end(),
                                std::back_inserter(common));

            for (const auto& confounder : common) {
                ConfounderCandidate cand;
                cand.confounder_node = confounder;
                cand.node1 = n1;
                cand.node2 = n2;
                cand.shared_connections = common.size();

                // Confounding strength: how many connections does the confounder have
                double degree = neighbors[confounder].size();
                cand.confounding_strength = std::min(degree / 10.0, 1.0);

                candidates.push_back(cand);

                if (candidates.size() > 200) break;
            }
            if (candidates.size() > 200) break;
        }
        if (candidates.size() > 200) break;
    }

    report_progress("Finding confounders", 70, 100);

    // Sort by confounding strength
    std::sort(candidates.begin(), candidates.end(), [](const ConfounderCandidate& a, const ConfounderCandidate& b) {
        return a.confounding_strength > b.confounding_strength;
    });

    // Create insights
    size_t max_confounders = std::min(candidates.size(), static_cast<size_t>(15));
    for (size_t i = 0; i < max_confounders; ++i) {
        const auto& cand = candidates[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CONFOUNDER);
        ins.set_type(InsightType::CONFOUNDER);
        ins.seed_nodes = {cand.confounder_node, cand.node1, cand.node2};
        ins.seed_labels = {
            get_node_label(cand.confounder_node),
            get_node_label(cand.node1),
            get_node_label(cand.node2)
        };
        ins.witness_nodes = ins.seed_nodes;

        ins.description = "Potential confounder: '" + get_node_label(cand.confounder_node) +
                         "' affects both '" + get_node_label(cand.node1) +
                         "' and '" + get_node_label(cand.node2) + "'";

        ins.score = cand.confounding_strength;
        ins.score_breakdown["confounding_strength"] = cand.confounding_strength;
        ins.score_breakdown["shared_connections"] = cand.shared_connections;

        results.push_back(ins);
    }

    report_progress("Finding confounders", 100, 100);
    return results;
}

// =============================================================================
// TAXONOMY: Hierarchical Relationship Induction
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_taxonomy_induction() {
    std::vector<Insight> results;
    report_progress("Finding taxonomy relationships", 0, 100);

    // Phase 2: Explicit hierarchical relations (highest priority)
    std::set<std::string> explicit_hierarchy = {
        "instance_of", "subclass_of", "is_a"
    };

    // Keywords indicating hierarchical relationships (fallback)
    std::set<std::string> taxonomy_keywords = {
        "is a", "is an", "type of", "kind of", "subclass of",
        "part of", "contains", "includes", "has a", "category"
    };

    std::vector<std::tuple<std::string, std::string, std::string, std::string, double>> hierarchical_rels; // child, parent, relation, edge_id, priority

    for (const auto& edge : graph_.get_all_edges()) {
        std::string rel = edge.relation;
        std::string rel_lower = rel;
        std::transform(rel_lower.begin(), rel_lower.end(), rel_lower.begin(), ::tolower);

        bool is_taxonomic = false;
        std::string match_type;
        double priority = 0.7;  // Default priority

        // Phase 2: Check if this is an explicit hierarchical relation (highest priority)
        if (edge.properties.find("relation_type") != edge.properties.end() &&
            edge.properties.at("relation_type") == "hierarchical") {
            is_taxonomic = true;
            match_type = edge.relation;  // Use the actual relation name (instance_of, subclass_of, is_a)
            priority = 1.0;  // Highest priority
        }
        // Check for exact match with explicit hierarchy relations
        else if (explicit_hierarchy.find(rel_lower) != explicit_hierarchy.end()) {
            is_taxonomic = true;
            match_type = rel;
            priority = 1.0;
        }
        // Fallback: Check keywords in relation name
        else {
            for (const auto& keyword : taxonomy_keywords) {
                if (rel_lower.find(keyword) != std::string::npos) {
                    is_taxonomic = true;
                    match_type = keyword;
                    priority = 0.7;
                    break;
                }
            }
        }

        if (is_taxonomic && !edge.sources.empty() && !edge.targets.empty()) {
            // Determine parent-child direction
            // For instance_of and is_a: source is child, target is parent
            // For subclass_of: source is child class, target is parent class
            bool sources_are_children = true;  // Default for hierarchical relations

            // Adjust direction for keyword-based relations
            if (priority < 1.0) {
                sources_are_children = (match_type.find("is a") != std::string::npos ||
                                       match_type.find("type of") != std::string::npos ||
                                       match_type.find("part of") != std::string::npos);
            }

            for (const auto& src : edge.sources) {
                for (const auto& tgt : edge.targets) {
                    if (sources_are_children) {
                        hierarchical_rels.push_back({src, tgt, match_type, edge.id, priority}); // src is child, tgt is parent
                    } else {
                        hierarchical_rels.push_back({tgt, src, match_type, edge.id, priority}); // tgt is child, src is parent
                    }
                }
            }
        }
    }

    report_progress("Finding taxonomy relationships", 50, 100);

    // Sort by priority (prefer explicit hierarchical relations over keyword-based)
    std::sort(hierarchical_rels.begin(), hierarchical_rels.end(),
        [](const auto& a, const auto& b) {
            return std::get<4>(a) > std::get<4>(b); // Sort by priority (highest first)
        });

    // Create insights
    size_t max_taxonomy = std::min(hierarchical_rels.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_taxonomy; ++i) {
        const auto& [child, parent, rel_type, edge_id, priority] = hierarchical_rels[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::TAXONOMY);
        ins.set_type(InsightType::TAXONOMY);
        ins.seed_nodes = {child, parent};
        ins.seed_labels = {get_node_label(child), get_node_label(parent)};
        ins.witness_nodes = {child, parent};
        ins.witness_edges = {edge_id};

        ins.description = "Taxonomy: '" + get_node_label(child) + "' " +
                         rel_type + " '" + get_node_label(parent) + "'";

        // Score based on priority (1.0 for explicit hierarchical, 0.7 for keyword-based)
        double score = priority;
        ins.score = score;
        ins.score_breakdown["relation_type"] = score;
        ins.score_breakdown["confidence"] = score;

        results.push_back(ins);
    }

    report_progress("Finding taxonomy relationships", 100, 100);
    return results;
}

// =============================================================================
// DOMAIN_BRIDGE: Cross-Domain Connector Detection
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_domain_bridges() {
    std::vector<Insight> results;
    report_progress("Finding domain bridges", 0, 100);

    // Use relation diversity as proxy for domain bridging
    // Nodes that connect different relation types are likely bridging domains
    std::map<std::string, std::set<std::string>> node_relation_types;
    std::map<std::string, std::set<std::string>> node_neighbors;

    for (const auto& edge : graph_.get_all_edges()) {
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                node_relation_types[src].insert(edge.relation);
                node_relation_types[tgt].insert(edge.relation);
                node_neighbors[src].insert(tgt);
                node_neighbors[tgt].insert(src);
            }
        }
    }

    report_progress("Finding domain bridges", 30, 100);

    // Calculate bridging score based on:
    // 1. Number of different relation types (domain diversity)
    // 2. Number of s-connected components the node participates in
    // 3. Degree centrality

    struct BridgeCandidate {
        std::string node;
        int relation_diversity;
        int degree;
        double bridge_score;
    };

    std::vector<BridgeCandidate> candidates;

    for (const auto& [node, rel_types] : node_relation_types) {
        if (rel_types.size() >= 2) { // Must connect at least 2 relation types
            BridgeCandidate cand;
            cand.node = node;
            cand.relation_diversity = rel_types.size();
            cand.degree = node_neighbors[node].size();

            // Bridge score: weighted combination of diversity and connectivity
            cand.bridge_score = (cand.relation_diversity * 0.7) + (std::min(cand.degree, 20) * 0.3);

            candidates.push_back(cand);
        }
    }

    report_progress("Finding domain bridges", 60, 100);

    // Sort by bridge score
    std::sort(candidates.begin(), candidates.end(), [](const BridgeCandidate& a, const BridgeCandidate& b) {
        return a.bridge_score > b.bridge_score;
    });

    // Create ranked list for insights
    std::vector<std::pair<std::string, int>> ranked;
    for (const auto& cand : candidates) {
        ranked.push_back({cand.node, cand.relation_diversity});
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // Create insights
    size_t max_bridges = std::min(ranked.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_bridges; ++i) {
        const auto& [node, domain_count] = ranked[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::DOMAIN_BRIDGE);
        ins.set_type(InsightType::DOMAIN_BRIDGE);
        ins.seed_nodes = {node};
        ins.seed_labels = {get_node_label(node)};
        ins.witness_nodes = {node};

        ins.description = "Domain bridge: '" + get_node_label(node) +
                         "' connects " + std::to_string(domain_count) + " different domains";

        double score = std::min(static_cast<double>(domain_count) / 5.0, 1.0);
        ins.score = score;
        ins.score_breakdown["domain_diversity"] = domain_count;
        ins.score_breakdown["bridge_strength"] = score;

        results.push_back(ins);
    }

    report_progress("Finding domain bridges", 100, 100);
    return results;
}

// =============================================================================
// LOGICAL_ENTAILMENT: IF-THEN Rule Induction
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_logical_entailments() {
    std::vector<Insight> results;
    report_progress("Finding logical entailments", 0, 100);

    // Find nodes with multiple incoming edges (potential premises)
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> incoming_edges; // target -> [(source, edge_id)]

    for (const auto& edge : graph_.get_all_edges()) {
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                incoming_edges[tgt].push_back({src, edge.id});
            }
        }
    }

    report_progress("Finding logical entailments", 40, 100);

    // Find nodes with 2+ incoming edges (IF X AND Y THEN Z pattern)
    std::vector<std::tuple<std::vector<std::string>, std::string, std::vector<std::string>>> entailments; // (premises, conclusion, edge_ids)

    for (const auto& [conclusion, premises_data] : incoming_edges) {
        if (premises_data.size() >= 2 && premises_data.size() <= 5) {
            std::vector<std::string> premises;
            std::vector<std::string> edge_ids;

            for (const auto& [premise, edge_id] : premises_data) {
                premises.push_back(premise);
                edge_ids.push_back(edge_id);
            }

            entailments.push_back({premises, conclusion, edge_ids});
        }
    }

    report_progress("Finding logical entailments", 70, 100);

    // Sort by number of premises (more premises = more complex rule)
    std::sort(entailments.begin(), entailments.end(),
        [](const auto& a, const auto& b) {
            return std::get<0>(a).size() > std::get<0>(b).size();
        });

    // Create insights
    size_t max_entailments = std::min(entailments.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_entailments; ++i) {
        const auto& [premises, conclusion, edge_ids] = entailments[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::LOGICAL_ENTAILMENT);
        ins.set_type(InsightType::LOGICAL_ENTAILMENT);

        ins.seed_nodes = premises;
        ins.seed_nodes.push_back(conclusion);

        for (const auto& p : premises) {
            ins.seed_labels.push_back(get_node_label(p));
        }
        ins.seed_labels.push_back(get_node_label(conclusion));

        ins.witness_nodes = ins.seed_nodes;
        ins.witness_edges = edge_ids;

        // Build description
        std::string premise_str = "IF ";
        for (size_t j = 0; j < premises.size(); ++j) {
            premise_str += "'" + get_node_label(premises[j]) + "'";
            if (j < premises.size() - 1) premise_str += " AND ";
        }

        ins.description = premise_str + " THEN '" + get_node_label(conclusion) + "'";

        ins.score = std::min(static_cast<double>(premises.size()) / 3.0, 1.0);
        ins.score_breakdown["premise_count"] = premises.size();
        ins.score_breakdown["rule_complexity"] = ins.score;

        results.push_back(ins);
    }

    report_progress("Finding logical entailments", 100, 100);
    return results;
}

// =============================================================================
// COMPOSITIONAL_REASONING: Relation Algebra (R1 ∘ R2 = R3)
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_compositional_reasoning() {
    std::vector<Insight> results;
    report_progress("Finding compositional patterns", 0, 100);

    // Find 2-hop paths where A→B→C and also A→C directly
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> outgoing; // node -> [(target, relation)]
    std::map<std::pair<std::string, std::string>, std::string> direct_relations; // (src, tgt) -> relation

    for (const auto& edge : graph_.get_all_edges()) {
        if (!edge.sources.empty() && !edge.targets.empty()) {
            std::string src = edge.sources[0];
            std::string tgt = edge.targets[0];
            outgoing[src].push_back({tgt, edge.relation});
            direct_relations[{src, tgt}] = edge.relation;
        }
    }

    report_progress("Finding compositional patterns", 30, 100);

    // Find A→B→C patterns where A→C also exists
    struct CompositionPattern {
        std::string node_a;
        std::string node_b;
        std::string node_c;
        std::string rel_ab;
        std::string rel_bc;
        std::string rel_ac;
    };

    std::vector<CompositionPattern> patterns;

    for (const auto& [node_a, targets_from_a] : outgoing) {
        for (const auto& [node_b, rel_ab] : targets_from_a) {
            if (!outgoing.count(node_b)) continue;

            for (const auto& [node_c, rel_bc] : outgoing[node_b]) {
                if (node_c == node_a) continue; // Skip cycles

                // Check if A→C exists directly
                auto it = direct_relations.find({node_a, node_c});
                if (it != direct_relations.end()) {
                    CompositionPattern pattern;
                    pattern.node_a = node_a;
                    pattern.node_b = node_b;
                    pattern.node_c = node_c;
                    pattern.rel_ab = rel_ab;
                    pattern.rel_bc = rel_bc;
                    pattern.rel_ac = it->second;
                    patterns.push_back(pattern);

                    if (patterns.size() > 100) break;
                }
            }
            if (patterns.size() > 100) break;
        }
        if (patterns.size() > 100) break;
    }

    report_progress("Finding compositional patterns", 70, 100);

    // Create insights
    size_t max_patterns = std::min(patterns.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_patterns; ++i) {
        const auto& pattern = patterns[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::COMPOSITIONAL_REASONING);
        ins.set_type(InsightType::COMPOSITIONAL_REASONING);
        ins.seed_nodes = {pattern.node_a, pattern.node_b, pattern.node_c};
        ins.seed_labels = {
            get_node_label(pattern.node_a),
            get_node_label(pattern.node_b),
            get_node_label(pattern.node_c)
        };
        ins.witness_nodes = ins.seed_nodes;

        ins.description = "Composition: '" + pattern.rel_ab + "' ∘ '" + pattern.rel_bc +
                         "' = '" + pattern.rel_ac + "' (" +
                         get_node_label(pattern.node_a) + "→" +
                         get_node_label(pattern.node_b) + "→" +
                         get_node_label(pattern.node_c) + ")";

        ins.score = 0.8; // Fixed score for compositional patterns
        ins.score_breakdown["path_length"] = 2;
        ins.score_breakdown["composition_valid"] = 1.0;

        results.push_back(ins);
    }

    report_progress("Finding compositional patterns", 100, 100);
    return results;
}
// =============================================================================
// EXPLANATORY_CHAIN: Multi-Hop Explanation Path Discovery
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_explanatory_chains() {
    std::vector<Insight> results;
    report_progress("Finding explanatory chains", 0, 100);

    // Explanatory relation keywords (relations that explain "why" or "how")
    std::set<std::string> explanatory_keywords = {
        "causes", "enables", "requires", "produces", "leads to", "results in",
        "improves", "addresses", "solves", "supports", "facilitates",
        "depends on", "is used by", "applied to", "implements",
        "contributes to", "influences", "affects", "determines"
    };

    auto is_explanatory = [&](const std::string& relation) {
        std::string rel_lower = relation;
        std::transform(rel_lower.begin(), rel_lower.end(), rel_lower.begin(), ::tolower);
        for (const auto& keyword : explanatory_keywords) {
            if (rel_lower.find(keyword) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    // Build directed explanatory graph
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> explan_edges; // node -> [(next_node, edge_id)]
    std::map<std::pair<std::string, std::string>, std::string> edge_relations;

    for (const auto& edge : graph_.get_all_edges()) {
        if (!is_explanatory(edge.relation)) continue;
        if (edge.sources.empty() || edge.targets.empty()) continue;

        std::string src = edge.sources[0];
        std::string tgt = edge.targets[0];
        explan_edges[src].push_back({tgt, edge.id});
        edge_relations[{src, tgt}] = edge.relation;
    }

    if (explan_edges.empty()) {
        report_progress("Finding explanatory chains", 100, 100);
        return results;
    }

    report_progress("Finding explanatory chains", 20, 100);

    // Identify high-value entity pairs to explain
    // Strategy: Find pairs with high betweenness but no direct connection
    std::vector<std::pair<std::string, std::string>> entity_pairs;
    
    // Get high-degree nodes (likely important concepts)
    std::vector<std::pair<std::string, int>> node_degrees;
    for (const auto& [node, neighbors] : explan_edges) {
        int total_degree = neighbors.size();
        node_degrees.push_back({node, total_degree});
    }

    std::sort(node_degrees.begin(), node_degrees.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Select top nodes as candidates
    size_t num_candidates = std::min(static_cast<size_t>(50), node_degrees.size());
    std::set<std::string> candidate_nodes;
    for (size_t i = 0; i < num_candidates; ++i) {
        candidate_nodes.insert(node_degrees[i].first);
    }

    // Generate pairs of candidates without direct connections
    for (const auto& node_a : candidate_nodes) {
        for (const auto& node_b : candidate_nodes) {
            if (node_a >= node_b) continue;
            
            // Check if there's a direct connection
            bool direct_connection = false;
            for (const auto& [neighbor, _] : explan_edges[node_a]) {
                if (neighbor == node_b) {
                    direct_connection = true;
                    break;
                }
            }
            
            if (!direct_connection) {
                entity_pairs.push_back({node_a, node_b});
            }
        }
    }

    report_progress("Finding explanatory chains", 40, 100);

    // Find k-shortest paths for each entity pair
    struct ExplanPath {
        std::vector<std::string> nodes;
        std::vector<std::string> edge_ids;
        std::vector<std::string> relations;
        double coherence_score;
        double novelty_score;
        double simplicity_score;
        double final_score;
    };

    std::vector<ExplanPath> all_paths;
    const size_t max_path_length = 6;
    const size_t max_paths_per_pair = 3;

    // BFS-based k-shortest path finding
    for (const auto& [source, target] : entity_pairs) {
        if (all_paths.size() >= 200) break; // Limit total paths to evaluate

        // BFS to find paths
        std::vector<ExplanPath> paths_for_pair;
        
        struct PathState {
            std::vector<std::string> nodes;
            std::vector<std::string> edge_ids;
            std::set<std::string> visited;
        };

        std::queue<PathState> queue;
        PathState initial;
        initial.nodes = {source};
        initial.visited = {source};
        queue.push(initial);

        while (!queue.empty() && paths_for_pair.size() < max_paths_per_pair) {
            PathState current = queue.front();
            queue.pop();

            std::string last_node = current.nodes.back();
            
            if (last_node == target && current.nodes.size() >= 3) {
                // Found a valid path
                ExplanPath path;
                path.nodes = current.nodes;
                path.edge_ids = current.edge_ids;
                
                // Extract relations
                for (size_t i = 0; i < current.nodes.size() - 1; ++i) {
                    path.relations.push_back(edge_relations[{current.nodes[i], current.nodes[i+1]}]);
                }
                
                paths_for_pair.push_back(path);
                continue;
            }

            if (current.nodes.size() >= max_path_length) continue;

            // Expand to neighbors
            for (const auto& [next_node, edge_id] : explan_edges[last_node]) {
                if (current.visited.count(next_node)) continue;

                PathState new_state;
                new_state.nodes = current.nodes;
                new_state.nodes.push_back(next_node);
                new_state.edge_ids = current.edge_ids;
                new_state.edge_ids.push_back(edge_id);
                new_state.visited = current.visited;
                new_state.visited.insert(next_node);

                queue.push(new_state);
            }
        }

        all_paths.insert(all_paths.end(), paths_for_pair.begin(), paths_for_pair.end());
    }

    report_progress("Finding explanatory chains", 70, 100);

    // Score paths
    for (auto& path : all_paths) {
        // Coherence: prefer paths with semantically related relations
        path.coherence_score = 0.7; // Baseline (would use LLM for better scoring)

        // Novelty: prefer paths with diverse intermediate nodes
        std::set<std::string> unique_intermediates(path.nodes.begin() + 1, path.nodes.end() - 1);
        path.novelty_score = std::min(static_cast<double>(unique_intermediates.size()) / 3.0, 1.0);

        // Simplicity: prefer shorter paths
        path.simplicity_score = 1.0 / static_cast<double>(path.nodes.size());

        // Final weighted score
        path.final_score = 0.4 * path.coherence_score +
                          0.3 * path.novelty_score +
                          0.3 * path.simplicity_score;
    }

    // Sort by final score
    std::sort(all_paths.begin(), all_paths.end(),
              [](const ExplanPath& a, const ExplanPath& b) {
                  return a.final_score > b.final_score;
              });

    // Create insights for top paths
    size_t max_insights = std::min(all_paths.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_insights; ++i) {
        const auto& path = all_paths[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::EXPLANATORY_CHAIN);
        ins.set_type(InsightType::EXPLANATORY_CHAIN);

        ins.seed_nodes = path.nodes;
        ins.seed_labels.clear();
        for (const auto& node : path.nodes) {
            ins.seed_labels.push_back(get_node_label(node));
        }

        ins.witness_nodes = path.nodes;
        ins.witness_edges = path.edge_ids;

        // Build description
        std::stringstream desc;
        desc << "Explanatory path: " << get_node_label(path.nodes[0]);
        for (size_t j = 1; j < path.nodes.size(); ++j) {
            desc << " → [" << path.relations[j-1] << "] → " << get_node_label(path.nodes[j]);
        }

        ins.description = desc.str();

        ins.score = path.final_score;
        ins.score_breakdown["coherence"] = path.coherence_score;
        ins.score_breakdown["novelty"] = path.novelty_score;
        ins.score_breakdown["simplicity"] = path.simplicity_score;
        ins.score_breakdown["path_length"] = path.nodes.size();

        results.push_back(ins);
    }

    report_progress("Finding explanatory chains", 100, 100);
    return results;
}

// =============================================================================
// SCHEMA_VIOLATION: Detect entities/relations violating expected patterns
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_schema_violations() {
    std::vector<Insight> results;
    report_progress("Finding schema violations", 0, 100);

    // Step 1: Learn implicit schema patterns from the graph
    // Track: (entity_type, relation) -> count
    std::map<std::pair<std::string, std::string>, int> type_relation_counts;
    std::map<std::string, int> type_counts;
    std::map<std::string, std::set<std::string>> entity_relations; // entity -> set of relations
    std::map<std::string, std::string> entity_types; // entity -> type (if available)

    // Extract entity types from labels (e.g., "Person: John" -> type="Person")
    auto all_nodes = graph_.get_all_nodes();
    for (const auto& node : all_nodes) {
        std::string label = node.label;
        size_t colon = label.find(':');
        if (colon != std::string::npos && colon > 0) {
            std::string type = label.substr(0, colon);
            // Trim whitespace
            while (!type.empty() && std::isspace(type.back())) type.pop_back();
            while (!type.empty() && std::isspace(type.front())) type.erase(0, 1);
            if (!type.empty()) {
                entity_types[node.id] = type;
                type_counts[type]++;
            }
        }
    }

    report_progress("Finding schema violations", 20, 100);

    // Build schema: what relations do entities of each type typically have?
    // Track unique entities that have each (type, relation) pair
    std::map<std::pair<std::string, std::string>, std::set<std::string>> type_relation_entities;

    for (const auto& edge : graph_.get_all_edges()) {
        std::string relation = edge.relation;
        if (relation.empty()) relation = "RELATED_TO";

        // Track all participants (sources + targets)
        std::set<std::string> participants;
        for (const auto& src : edge.sources) participants.insert(src);
        for (const auto& tgt : edge.targets) participants.insert(tgt);

        for (const auto& node_id : participants) {
            entity_relations[node_id].insert(relation);

            // Track which entities of each type have this relation
            if (entity_types.count(node_id)) {
                std::string type = entity_types[node_id];
                type_relation_entities[{type, relation}].insert(node_id);
            }
        }
    }

    // Convert entity sets to counts
    for (const auto& [key, entities] : type_relation_entities) {
        type_relation_counts[key] = entities.size();
    }

    report_progress("Finding schema violations", 40, 100);

    // Step 2: Compute expected relations for each type (threshold: 30% of entities have it)
    std::map<std::string, std::set<std::string>> expected_relations; // type -> set of expected relations
    for (const auto& [type, count] : type_counts) {
        if (count < 3) continue; // Need at least 3 entities of a type to establish pattern

        for (const auto& [key, rel_count] : type_relation_counts) {
            if (key.first == type) {
                double coverage = (double)rel_count / count;
                if (coverage >= 0.30) { // 30% threshold
                    expected_relations[type].insert(key.second);
                }
            }
        }
    }

    report_progress("Finding schema violations", 60, 100);

    // Step 3: Find violations - entities missing expected relations
    struct Violation {
        std::string entity;
        std::string entity_label;
        std::string entity_type;
        std::string missing_relation;
        double severity; // How common is this relation for this type?
        int type_size;   // How many entities of this type?
    };

    std::vector<Violation> violations;

    for (const auto& [node_id, type] : entity_types) {
        if (!expected_relations.count(type)) continue;

        const auto& expected = expected_relations[type];
        const auto& actual = entity_relations.count(node_id) ? entity_relations[node_id] : std::set<std::string>();

        // Find missing relations
        for (const auto& expected_rel : expected) {
            if (actual.find(expected_rel) == actual.end()) {
                // This entity is missing an expected relation
                double coverage = (double)type_relation_counts[{type, expected_rel}] / type_counts[type];

                Violation v;
                v.entity = node_id;
                v.entity_label = get_node_label(node_id);
                v.entity_type = type;
                v.missing_relation = expected_rel;
                v.severity = coverage; // Higher coverage = more severe violation
                v.type_size = type_counts[type];

                violations.push_back(v);
            }
        }
    }

    report_progress("Finding schema violations", 80, 100);

    // Step 4: Rank violations by severity and convert to insights
    std::sort(violations.begin(), violations.end(), [](const Violation& a, const Violation& b) {
        if (std::abs(a.severity - b.severity) > 0.01) return a.severity > b.severity;
        return a.type_size > b.type_size; // Prefer violations in larger type groups
    });

    // Take top violations (limit to 50)
    size_t max_violations = std::min<size_t>(50, violations.size());
    for (size_t i = 0; i < max_violations; ++i) {
        const auto& v = violations[i];

        Insight ins;
        ins.set_type(InsightType::SCHEMA_VIOLATION);
        ins.seed_nodes = {v.entity};
        ins.seed_labels = {v.entity_label};
        ins.score = v.severity;

        ins.score_breakdown["severity"] = v.severity;
        ins.score_breakdown["type_size"] = v.type_size;
        ins.score_breakdown["coverage"] = v.severity * 100.0; // as percentage

        // Find example entities of same type that DO have this relation (for suggestions)
        std::vector<std::string> examples;
        for (const auto& [other_id, other_type] : entity_types) {
            if (other_type == v.entity_type &&
                entity_relations.count(other_id) &&
                entity_relations[other_id].count(v.missing_relation) &&
                examples.size() < 3) {
                examples.push_back(get_node_label(other_id));
            }
        }

        // Build structured description with embedded metadata
        std::stringstream desc;
        desc << "SCHEMA_VIOLATION|entity_type:" << v.entity_type
             << "|missing_relation:" << v.missing_relation
             << "|expected_coverage:" << std::to_string((int)(v.severity * 100)) << "%";
        if (!examples.empty()) {
            desc << "|examples:" << examples[0];
            if (examples.size() > 1) desc << ", " << examples[1];
            if (examples.size() > 2) desc << ", " << examples[2];
        }
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding schema violations", 100, 100);
    return results;
}


// =============================================================================
// TRANSITIVE_CLOSURE: Find missing links implied by transitive relations
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_transitive_closure_gaps() {
    std::vector<Insight> results;
    report_progress("Finding transitive closure gaps", 0, 100);

    // Step 1: Identify transitive relation types
    // Relations that should satisfy: if A→B and B→C, then A→C
    std::set<std::string> transitive_relations;

    // Common transitive patterns
    std::vector<std::string> transitive_keywords = {
        "is-a", "isa", "subclass", "subtype", "inherits", "extends",
        "part-of", "partof", "contains", "includes",
        "causes", "leads-to", "produces", "results-in",
        "precedes", "before", "after", "follows",
        "implies", "entails", "requires"
    };

    // Scan all edges to find which relations appear transitive
    for (const auto& edge : graph_.get_all_edges()) {
        std::string rel = edge.relation;
        std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);

        // Check if relation name suggests transitivity
        for (const auto& keyword : transitive_keywords) {
            if (rel.find(keyword) != std::string::npos) {
                transitive_relations.insert(edge.relation); // Keep original case
                break;
            }
        }
    }

    report_progress("Finding transitive closure gaps", 20, 100);

    // Step 2: Build directed graphs for each transitive relation type
    std::map<std::string, std::map<std::string, std::set<std::string>>> relation_graphs;
    // relation -> (source -> set of targets)

    std::map<std::string, std::set<std::pair<std::string, std::string>>> existing_links;
    // relation -> set of (source, target) pairs that exist

    for (const auto& edge : graph_.get_all_edges()) {
        if (transitive_relations.count(edge.relation) == 0) continue;

        if (edge.sources.empty() || edge.targets.empty()) continue;

        // For hyperedges, create all source->target combinations
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                relation_graphs[edge.relation][src].insert(tgt);
                existing_links[edge.relation].insert({src, tgt});
            }
        }
    }

    report_progress("Finding transitive closure gaps", 40, 100);

    // Step 3: Find transitive closure gaps
    // For each relation, find chains A→B→C where A→C is missing
    struct Gap {
        std::string relation;
        std::string source;
        std::string intermediate;
        std::string target;
        int chain_count;    // How many paths support this gap
        double confidence;  // Confidence in the inference
    };

    std::vector<Gap> gaps;

    for (const auto& [relation, graph] : relation_graphs) {
        // Track potential gaps: (source, target) -> count of supporting chains
        std::map<std::pair<std::string, std::string>, int> gap_support;
        std::map<std::pair<std::string, std::string>, std::set<std::string>> gap_intermediates;

        // Find chains of length 2: A→B→C
        for (const auto& [node_a, targets_of_a] : graph) {
            for (const auto& node_b : targets_of_a) {
                // Found A→B, now look for B→C
                if (graph.count(node_b)) {
                    for (const auto& node_c : graph.at(node_b)) {
                        // Found A→B→C chain
                        // Check if A→C exists
                        if (existing_links[relation].count({node_a, node_c}) == 0) {
                            // Gap found!
                            gap_support[{node_a, node_c}]++;
                            gap_intermediates[{node_a, node_c}].insert(node_b);
                        }
                    }
                }
            }
        }

        // Convert gaps to Gap structures
        for (const auto& [pair, count] : gap_support) {
            Gap g;
            g.relation = relation;
            g.source = pair.first;
            g.target = pair.second;
            g.chain_count = count;

            // Pick most representative intermediate
            if (!gap_intermediates[pair].empty()) {
                g.intermediate = *gap_intermediates[pair].begin();
            }

            // Compute confidence: more supporting chains = higher confidence
            // Also consider the overall connectivity of the relation graph
            g.confidence = std::min(1.0, 0.5 + 0.1 * count); // Base 0.5, +0.1 per chain, cap at 1.0

            gaps.push_back(g);
        }
    }

    report_progress("Finding transitive closure gaps", 70, 100);

    // Step 4: Rank gaps by confidence and chain count
    std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) {
        if (std::abs(a.confidence - b.confidence) > 0.01) return a.confidence > b.confidence;
        return a.chain_count > b.chain_count;
    });

    // Step 5: Convert top gaps to insights (limit to 50)
    size_t max_gaps = std::min<size_t>(50, gaps.size());
    for (size_t i = 0; i < max_gaps; ++i) {
        const auto& g = gaps[i];

        Insight ins;
        ins.set_type(InsightType::TRANSITIVE_CLOSURE);
        ins.seed_nodes = {g.source, g.target};
        ins.seed_labels = {get_node_label(g.source), get_node_label(g.target)};
        ins.score = g.confidence;

        ins.score_breakdown["confidence"] = g.confidence;
        ins.score_breakdown["chain_count"] = g.chain_count;

        // Build description
        std::stringstream desc;
        desc << "TRANSITIVE_GAP|relation:" << g.relation
             << "|intermediate:" << get_node_label(g.intermediate)
             << "|chains:" << g.chain_count;
        ins.description = desc.str();

        ins.novelty_tags.push_back("transitive_closure");
        ins.novelty_tags.push_back("inferred_" + g.relation);

        results.push_back(ins);
    }

    report_progress("Finding transitive closure gaps", 100, 100);
    return results;
}


// =============================================================================
// META_PATH: Meta-Path Based Link Prediction
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_meta_path_links() {
    std::vector<Insight> results;
    report_progress("Finding meta-path predictions", 0, 100);

    // Meta-paths are sequences of relation types
    // For example: Author→Paper→Venue or Author→Paper→Author (collaboration)

    // Build relation-typed graph
    struct TypedEdge {
        std::string src;
        std::string tgt;
        std::string rel_type;
    };

    std::vector<TypedEdge> typed_edges;
    std::map<std::string, std::set<std::string>> neighbors;

    for (const auto& edge : graph_.get_all_edges()) {
        if (!edge.sources.empty() && !edge.targets.empty()) {
            for (const auto& src : edge.sources) {
                for (const auto& tgt : edge.targets) {
                    typed_edges.push_back({src, tgt, edge.relation});
                    neighbors[src].insert(tgt);
                    neighbors[tgt].insert(src);
                }
            }
        }
    }

    report_progress("Finding meta-path predictions", 30, 100);

    // Find common meta-paths (2-hop with same relation types)
    std::map<std::string, int> meta_path_frequency; // relation1→relation2 -> count
    std::map<std::string, std::vector<std::tuple<std::string, std::string, std::string>>> meta_path_instances; // meta_path -> [(a, b, c)]

    for (const auto& edge1 : typed_edges) {
        for (const auto& edge2 : typed_edges) {
            if (edge1.tgt == edge2.src && edge1.src != edge2.tgt) {
                std::string meta_path = edge1.rel_type + "→" + edge2.rel_type;
                meta_path_frequency[meta_path]++;
                meta_path_instances[meta_path].push_back({edge1.src, edge1.tgt, edge2.tgt});

                if (meta_path_instances[meta_path].size() > 20) break;
            }
        }
    }

    report_progress("Finding meta-path predictions", 60, 100);

    // Use frequent meta-paths to predict missing links
    struct MetaPathPrediction {
        std::string node_a;
        std::string node_c;
        std::string meta_path;
        int support_count;
    };

    std::vector<MetaPathPrediction> predictions;

    for (const auto& [meta_path, instances] : meta_path_instances) {
        if (meta_path_frequency[meta_path] < 3) continue; // Require at least 3 instances

        for (const auto& [a, b, c] : instances) {
            // Predict link from a to c if it doesn't exist
            if (!neighbors[a].count(c)) {
                MetaPathPrediction pred;
                pred.node_a = a;
                pred.node_c = c;
                pred.meta_path = meta_path;
                pred.support_count = meta_path_frequency[meta_path];
                predictions.push_back(pred);

                if (predictions.size() > 50) break;
            }
        }
        if (predictions.size() > 50) break;
    }

    report_progress("Finding meta-path predictions", 80, 100);

    // Sort by support count
    std::sort(predictions.begin(), predictions.end(),
        [](const MetaPathPrediction& a, const MetaPathPrediction& b) {
            return a.support_count > b.support_count;
        });

    // Create insights
    size_t max_predictions = std::min(predictions.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < max_predictions; ++i) {
        const auto& pred = predictions[i];

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::META_PATH);
        ins.set_type(InsightType::META_PATH);
        ins.seed_nodes = {pred.node_a, pred.node_c};
        ins.seed_labels = {get_node_label(pred.node_a), get_node_label(pred.node_c)};
        ins.witness_nodes = ins.seed_nodes;

        ins.description = "Meta-path prediction: '" + get_node_label(pred.node_a) +
                         "' → '" + get_node_label(pred.node_c) + "' via path " + pred.meta_path +
                         " (support: " + std::to_string(pred.support_count) + ")";

        ins.score = std::min(static_cast<double>(pred.support_count) / 10.0, 1.0);
        ins.score_breakdown["support_count"] = pred.support_count;
        ins.score_breakdown["meta_path_strength"] = ins.score;

        results.push_back(ins);
    }

    report_progress("Finding meta-path predictions", 100, 100);
    return results;
}

// =============================================================================
// META_PATH_PATTERN: Global Meta-Path Pattern Mining
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_meta_path_patterns() {
    std::vector<Insight> results;
    report_progress("Mining meta-path patterns", 0, 100);

    struct EdgeInfo {
        std::string tgt;
        std::string relation;
        std::string edge_id;
        double confidence;
    };

    std::unordered_map<std::string, std::vector<EdgeInfo>> adjacency;

    for (const auto& edge : graph_.get_all_edges()) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        std::string rel = edge.relation.empty() ? "related_to" : edge.relation;
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                adjacency[src].push_back({tgt, rel, edge.id, edge.confidence});
            }
        }
    }

    if (adjacency.empty()) {
        report_progress("Mining meta-path patterns", 100, 100);
        return results;
    }

    std::vector<std::string> seeds;
    if (config_.meta_path_pattern_max_seed_nodes > 0) {
        auto hubs = graph_.get_top_hubs(static_cast<int>(config_.meta_path_pattern_max_seed_nodes));
        for (const auto& [node_id, _] : hubs) {
            seeds.push_back(node_id);
        }
    } else {
        auto nodes = graph_.get_all_nodes();
        for (const auto& node : nodes) seeds.push_back(node.id);
    }

    int min_len = std::max(1, config_.meta_path_pattern_min_len);
    int max_len = std::max(min_len, config_.meta_path_pattern_max_len);

    struct Instance {
        std::vector<std::string> nodes;
        std::vector<std::string> edge_ids;
        double avg_confidence = 0.0;
    };

    struct PatternInfo {
        std::unordered_set<std::string> pairs;
        std::vector<Instance> instances;
        double conf_sum = 0.0;
        size_t count = 0;
        int length = 0;
    };

    std::unordered_map<std::string, PatternInfo> patterns;

    auto join_pattern = [](const std::vector<std::string>& parts) {
        std::ostringstream ss;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) ss << "→";
            ss << parts[i];
        }
        return ss.str();
    };

    bool stop = false;
    for (const auto& seed : seeds) {
        if (stop) break;
        std::vector<std::string> path_nodes = {seed};
        std::vector<std::string> path_edges;
        std::vector<std::string> path_rels;
        std::unordered_set<std::string> visited = {seed};
        double conf_sum = 0.0;

        std::function<void(const std::string&, int)> dfs = [&](const std::string& node, int depth) {
            if (depth >= max_len || stop) return;
            auto it = adjacency.find(node);
            if (it == adjacency.end()) return;

            size_t expanded = 0;
            for (const auto& edge : it->second) {
                if (config_.meta_path_pattern_degree_cap > 0 &&
                    expanded++ >= config_.meta_path_pattern_degree_cap) {
                    break;
                }
                if (visited.count(edge.tgt)) continue;

                visited.insert(edge.tgt);
                path_nodes.push_back(edge.tgt);
                path_edges.push_back(edge.edge_id);
                path_rels.push_back(edge.relation);
                conf_sum += edge.confidence;

                int cur_len = static_cast<int>(path_rels.size());
                if (cur_len >= min_len) {
                    std::string pattern = join_pattern(path_rels);
                    auto& info = patterns[pattern];
                    if (info.length == 0) info.length = cur_len;

                    std::string pair_key = path_nodes.front() + "||" + path_nodes.back();
                    info.pairs.insert(pair_key);

                    double avg_conf = path_edges.empty() ? 0.0 : (conf_sum / path_edges.size());
                    info.conf_sum += avg_conf;
                    info.count++;

                    if (info.instances.size() < config_.meta_path_pattern_max_instances) {
                        Instance inst;
                        inst.nodes = path_nodes;
                        inst.edge_ids = path_edges;
                        inst.avg_confidence = avg_conf;
                        info.instances.push_back(std::move(inst));
                    }
                }

                if (cur_len < max_len) {
                    dfs(edge.tgt, cur_len);
                }

                conf_sum -= edge.confidence;
                path_rels.pop_back();
                path_edges.pop_back();
                path_nodes.pop_back();
                visited.erase(edge.tgt);

                if (patterns.size() > config_.meta_path_pattern_max_patterns * 5) {
                    stop = true;
                    break;
                }
            }
        };

        dfs(seed, 0);
    }

    if (patterns.empty()) {
        report_progress("Mining meta-path patterns", 100, 100);
        return results;
    }

    size_t max_support = 0;
    for (const auto& [pattern, info] : patterns) {
        if (info.pairs.size() > max_support) max_support = info.pairs.size();
    }

    struct PatternCandidate {
        std::string pattern;
        size_t support = 0;
        double avg_confidence = 0.0;
        double score = 0.0;
        int length = 0;
        Instance example;
    };

    std::vector<PatternCandidate> candidates;

    for (const auto& [pattern, info] : patterns) {
        size_t support = info.pairs.size();
        if (support < static_cast<size_t>(std::max(1, config_.meta_path_pattern_min_support))) continue;

        double avg_conf = info.count > 0 ? (info.conf_sum / info.count) : 0.0;
        if (avg_conf < config_.meta_path_pattern_min_avg_confidence) continue;

        double support_score = max_support > 0 ? static_cast<double>(support) / max_support : 0.0;
        double length_score = (max_len > min_len)
            ? static_cast<double>(info.length - min_len + 1) / static_cast<double>(max_len - min_len + 1)
            : 1.0;

        double score =
            config_.meta_path_pattern_weight_support * support_score +
            config_.meta_path_pattern_weight_confidence * avg_conf +
            config_.meta_path_pattern_weight_length * length_score;

        PatternCandidate cand;
        cand.pattern = pattern;
        cand.support = support;
        cand.avg_confidence = avg_conf;
        cand.score = score;
        cand.length = info.length;
        if (!info.instances.empty()) cand.example = info.instances.front();
        candidates.push_back(std::move(cand));
    }

    if (candidates.empty()) {
        report_progress("Mining meta-path patterns", 100, 100);
        return results;
    }

    std::sort(candidates.begin(), candidates.end(), [](const PatternCandidate& a, const PatternCandidate& b) {
        return a.score > b.score;
    });

    if (candidates.size() > config_.meta_path_pattern_max_patterns) {
        candidates.resize(config_.meta_path_pattern_max_patterns);
    }

    for (const auto& cand : candidates) {
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::META_PATH_PATTERN);
        ins.set_type(InsightType::META_PATH_PATTERN);

        if (!cand.example.nodes.empty()) {
            ins.seed_nodes = {cand.example.nodes.front(), cand.example.nodes.back()};
            ins.seed_labels = {get_node_label(cand.example.nodes.front()), get_node_label(cand.example.nodes.back())};
            ins.witness_nodes = cand.example.nodes;
            ins.witness_edges = cand.example.edge_ids;
            ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        }

        std::ostringstream desc;
        desc << "Meta-path pattern: " << cand.pattern
             << " (support: " << cand.support
             << ", avg_conf: " << std::fixed << std::setprecision(2) << cand.avg_confidence << ")";
        if (cand.example.nodes.size() >= 2) {
            desc << ". Example: ";
            for (size_t i = 0; i < cand.example.nodes.size(); ++i) {
                if (i > 0) desc << " -> ";
                desc << "'" << get_node_label(cand.example.nodes[i]) << "'";
            }
        }
        ins.description = desc.str();

        ins.score_breakdown["support_count"] = static_cast<double>(cand.support);
        ins.score_breakdown["avg_confidence"] = cand.avg_confidence;
        ins.score_breakdown["length"] = static_cast<double>(cand.length);
        ins.score_breakdown["pattern_score"] = cand.score;
        ins.score = cand.score;
        ins.novelty_tags = {"meta_path_pattern", "pattern=" + cand.pattern};

        results.push_back(std::move(ins));
    }

    report_progress("Mining meta-path patterns", 100, 100);
    return results;
}

// =============================================================================
// EVIDENCE_FUSION_LINK: Multi-operator consensus link discovery
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_evidence_fusion_links(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Evidence fusion links", 0, 100);

    const std::set<InsightType> source_types = {
        InsightType::PATH_RANK,
        InsightType::EMBEDDING_LINK,
        InsightType::RULE,
        InsightType::ARGUMENT_SUPPORT,
        InsightType::COMMUNITY_LINK,
        InsightType::LONG_CHAIN,
        InsightType::META_PATH,
        InsightType::META_PATH_PATTERN,
        InsightType::HYPOTHESES_1,
        InsightType::HYPOTHESES_2,
        InsightType::HYPOTHESES_3,
        InsightType::EXPLANATORY_CHAIN,
        InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE
    };

    struct FusionAggregate {
        std::set<InsightType> operators;
        std::unordered_set<std::string> witness_edges;
        std::unordered_set<std::string> witness_nodes;
        std::unordered_set<std::string> chunk_ids;
        double score_sum = 0.0;
        size_t score_count = 0;
    };

    std::set<InsightType> available_operator_types;
    std::map<std::pair<std::string, std::string>, FusionAggregate> aggregates;

    for (const auto& insight : collection.insights) {
        if (source_types.find(insight.type) == source_types.end()) continue;
        if (insight.seed_nodes.size() < 2) continue;

        std::string a = insight.seed_nodes[0];
        std::string b = insight.seed_nodes[1];
        if (a.empty() || b.empty() || a == b) continue;

        // Evidence fusion is for proposing new links.
        if (nodes_share_edge(graph_, a, b)) continue;

        if (a > b) std::swap(a, b);
        auto key = std::make_pair(a, b);
        auto& agg = aggregates[key];

        agg.operators.insert(insight.type);
        available_operator_types.insert(insight.type);
        agg.score_sum += insight.score;
        agg.score_count++;

        for (const auto& e : insight.witness_edges) agg.witness_edges.insert(e);
        for (const auto& n : insight.witness_nodes) agg.witness_nodes.insert(n);
        for (const auto& c : insight.evidence_chunk_ids) agg.chunk_ids.insert(c);
    }

    if (aggregates.empty()) {
        report_progress("Evidence fusion links", 100, 100);
        return results;
    }

    auto stats = graph_.compute_statistics();

    size_t dynamic_min_sources = std::max<size_t>(2, config_.evidence_fusion_min_sources);
    if (available_operator_types.size() >= 6) dynamic_min_sources++;
    if (stats.num_edges > 2500) dynamic_min_sources++;
    dynamic_min_sources = std::min<size_t>(4, dynamic_min_sources);

    double dynamic_min_score = config_.evidence_fusion_min_score;
    if (stats.avg_node_degree > 8.0) dynamic_min_score += 0.05;
    if (stats.num_edges < 500) dynamic_min_score -= 0.05;
    dynamic_min_score = std::clamp(dynamic_min_score, 0.20, 0.95);

    double max_possible_sources = static_cast<double>(
        std::max<size_t>(dynamic_min_sources, available_operator_types.size()));

    for (const auto& [pair_key, agg] : aggregates) {
        if (agg.operators.size() < dynamic_min_sources || agg.score_count == 0) continue;

        double avg_strength = agg.score_sum / static_cast<double>(agg.score_count);
        double consensus = static_cast<double>(agg.operators.size()) /
                           std::max(1.0, max_possible_sources);
        double diversity = std::min(1.0, static_cast<double>(agg.chunk_ids.size()) / 6.0);

        double final_score =
            config_.evidence_fusion_weight_consensus * consensus +
            config_.evidence_fusion_weight_strength * avg_strength +
            config_.evidence_fusion_weight_diversity * diversity;

        if (final_score < dynamic_min_score) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::EVIDENCE_FUSION_LINK);
        ins.set_type(InsightType::EVIDENCE_FUSION_LINK);
        ins.seed_nodes = {pair_key.first, pair_key.second};
        ins.seed_labels = {get_node_label(pair_key.first), get_node_label(pair_key.second)};

        for (const auto& edge_id : agg.witness_edges) {
            ins.witness_edges.push_back(edge_id);
            if (ins.witness_edges.size() >= config_.evidence_fusion_max_witness_edges) break;
        }

        ins.witness_nodes.push_back(pair_key.first);
        ins.witness_nodes.push_back(pair_key.second);
        for (const auto& node_id : agg.witness_nodes) {
            if (node_id == pair_key.first || node_id == pair_key.second) continue;
            ins.witness_nodes.push_back(node_id);
            if (ins.witness_nodes.size() >= 20) break;
        }

        for (const auto& chunk_id : agg.chunk_ids) {
            ins.evidence_chunk_ids.push_back(chunk_id);
            if (ins.evidence_chunk_ids.size() >= 10) break;
        }

        std::vector<std::string> op_names;
        for (auto type : agg.operators) {
            op_names.push_back(insight_type_to_string(type));
            ins.novelty_tags.push_back("source=" + insight_type_to_string(type));
        }
        std::sort(op_names.begin(), op_names.end());

        std::ostringstream desc;
        desc << "Evidence fusion link: '" << get_node_label(pair_key.first)
             << "' <-> '" << get_node_label(pair_key.second)
             << "' supported by " << agg.operators.size() << " operators (";
        for (size_t i = 0; i < std::min<size_t>(op_names.size(), 4); ++i) {
            if (i > 0) desc << ", ";
            desc << op_names[i];
        }
        if (op_names.size() > 4) desc << ", ...";
        desc << ").";
        ins.description = desc.str();

        ins.score_breakdown["consensus"] = consensus;
        ins.score_breakdown["strength"] = avg_strength;
        ins.score_breakdown["diversity"] = diversity;
        ins.score_breakdown["source_count"] = static_cast<double>(agg.operators.size());
        ins.score_breakdown["dynamic_min_sources"] = static_cast<double>(dynamic_min_sources);
        ins.score_breakdown["dynamic_min_score"] = dynamic_min_score;
        ins.score = final_score;
        ins.novelty_tags.push_back("evidence_fusion");

        results.push_back(std::move(ins));
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.evidence_fusion_max_candidates) {
        results.resize(config_.evidence_fusion_max_candidates);
    }

    report_progress("Evidence fusion links", 100, 100);
    return results;
}

// =============================================================================
// META_PATH_ANOMALY: Rare but plausible relation-sequence anomalies
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_meta_path_anomalies(const InsightCollection& collection) {
    (void)collection;
    std::vector<Insight> results;
    report_progress("Meta-path anomaly", 0, 100);

    struct EdgeInfo {
        std::string tgt;
        std::string relation;
        std::string edge_id;
        double confidence;
    };

    std::unordered_map<std::string, std::vector<EdgeInfo>> adjacency;
    double global_conf_sum = 0.0;
    size_t global_conf_count = 0;

    for (const auto& edge : graph_.get_all_edges()) {
        if (edge.sources.empty() || edge.targets.empty()) continue;
        std::string rel = edge.relation.empty() ? "related_to" : to_lower_copy(edge.relation);
        for (const auto& src : edge.sources) {
            for (const auto& tgt : edge.targets) {
                adjacency[src].push_back({tgt, rel, edge.id, edge.confidence});
                global_conf_sum += edge.confidence;
                global_conf_count++;
            }
        }
    }

    if (adjacency.empty()) {
        report_progress("Meta-path anomaly", 100, 100);
        return results;
    }

    double global_avg_conf = global_conf_count > 0
        ? (global_conf_sum / static_cast<double>(global_conf_count))
        : 0.0;

    std::vector<std::string> seeds;
    if (config_.meta_path_anomaly_max_seed_nodes > 0) {
        auto hubs = graph_.get_top_hubs(static_cast<int>(config_.meta_path_anomaly_max_seed_nodes));
        for (const auto& [node_id, _] : hubs) {
            seeds.push_back(node_id);
        }
    } else {
        auto nodes = graph_.get_all_nodes();
        for (const auto& node : nodes) seeds.push_back(node.id);
    }

    int min_len = std::max(1, config_.meta_path_anomaly_min_len);
    int max_len = std::max(min_len, config_.meta_path_anomaly_max_len);

    struct PatternStats {
        double conf_sum = 0.0;
        size_t conf_count = 0;
        int length = 0;
        std::vector<std::string> example_nodes;
        std::vector<std::string> example_edges;
    };

    std::unordered_map<std::string, PatternStats> stats_by_pattern;
    std::unordered_map<std::string, std::unordered_set<std::string>> support_pairs;

    auto join_pattern = [](const std::vector<std::string>& rels) {
        std::ostringstream oss;
        for (size_t i = 0; i < rels.size(); ++i) {
            if (i > 0) oss << "->";
            oss << rels[i];
        }
        return oss.str();
    };

    bool stop = false;
    for (const auto& seed : seeds) {
        if (stop) break;

        std::vector<std::string> path_nodes = {seed};
        std::vector<std::string> path_edges;
        std::vector<std::string> path_rels;
        std::unordered_set<std::string> visited = {seed};
        double path_conf_sum = 0.0;

        std::function<void(const std::string&)> dfs = [&](const std::string& node) {
            if (stop) return;
            if (static_cast<int>(path_rels.size()) >= max_len) return;

            auto it = adjacency.find(node);
            if (it == adjacency.end()) return;

            size_t expanded = 0;
            for (const auto& edge : it->second) {
                if (config_.meta_path_anomaly_degree_cap > 0 &&
                    expanded++ >= config_.meta_path_anomaly_degree_cap) {
                    break;
                }
                if (visited.count(edge.tgt)) continue;

                visited.insert(edge.tgt);
                path_nodes.push_back(edge.tgt);
                path_edges.push_back(edge.edge_id);
                path_rels.push_back(edge.relation);
                path_conf_sum += edge.confidence;

                int depth = static_cast<int>(path_rels.size());
                if (depth >= min_len) {
                    std::string pattern = join_pattern(path_rels);
                    std::string pair_key = path_nodes.front() + "||" + path_nodes.back();

                    auto& stats = stats_by_pattern[pattern];
                    if (stats.length == 0) stats.length = depth;
                    stats.conf_count++;
                    stats.conf_sum += (path_edges.empty() ? 0.0 : path_conf_sum / path_edges.size());
                    support_pairs[pattern].insert(pair_key);

                    if (stats.example_nodes.empty()) {
                        stats.example_nodes = path_nodes;
                        stats.example_edges = path_edges;
                    }
                }

                if (depth < max_len) {
                    dfs(edge.tgt);
                }

                path_conf_sum -= edge.confidence;
                path_rels.pop_back();
                path_edges.pop_back();
                path_nodes.pop_back();
                visited.erase(edge.tgt);

                if (stats_by_pattern.size() > config_.meta_path_anomaly_max_candidates * 12) {
                    stop = true;
                    break;
                }
            }
        };

        dfs(seed);
    }

    if (stats_by_pattern.empty()) {
        report_progress("Meta-path anomaly", 100, 100);
        return results;
    }

    size_t max_support = 0;
    for (const auto& [pattern, pairs] : support_pairs) {
        max_support = std::max(max_support, pairs.size());
    }
    if (max_support == 0) {
        report_progress("Meta-path anomaly", 100, 100);
        return results;
    }

    auto graph_stats = graph_.compute_statistics();
    int dynamic_min_support = config_.meta_path_anomaly_min_support;
    if (graph_stats.num_edges > 3000) dynamic_min_support += 1;
    dynamic_min_support = std::max(1, dynamic_min_support);

    double dynamic_max_support_ratio = config_.meta_path_anomaly_max_support_ratio;
    if (graph_stats.num_edges < 600) {
        dynamic_max_support_ratio = std::min(0.45, dynamic_max_support_ratio + 0.10);
    }

    double dynamic_min_plausibility = config_.meta_path_anomaly_min_plausibility;
    if (global_avg_conf < 0.45) dynamic_min_plausibility = std::max(0.10, dynamic_min_plausibility - 0.05);
    if (global_avg_conf > 0.75) dynamic_min_plausibility = std::min(0.90, dynamic_min_plausibility + 0.05);

    double dynamic_min_anomaly = config_.meta_path_anomaly_min_anomaly;
    if (max_support > 200) dynamic_min_anomaly = std::min(0.90, dynamic_min_anomaly + 0.05);

    for (const auto& [pattern, stats] : stats_by_pattern) {
        size_t support = support_pairs[pattern].size();
        if (support < static_cast<size_t>(dynamic_min_support)) continue;

        double support_ratio = static_cast<double>(support) / static_cast<double>(max_support);
        if (support_ratio > dynamic_max_support_ratio) continue;

        double anomaly = 1.0 - support_ratio;
        double plausibility = stats.conf_count > 0 ? (stats.conf_sum / static_cast<double>(stats.conf_count)) : 0.0;
        if (anomaly < dynamic_min_anomaly || plausibility < dynamic_min_plausibility) continue;

        double length_score = (max_len > min_len)
            ? static_cast<double>(stats.length - min_len + 1) / static_cast<double>(max_len - min_len + 1)
            : 1.0;

        double final_score =
            config_.meta_path_anomaly_weight_anomaly * anomaly +
            config_.meta_path_anomaly_weight_plausibility * plausibility +
            config_.meta_path_anomaly_weight_length * length_score;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::META_PATH_ANOMALY);
        ins.set_type(InsightType::META_PATH_ANOMALY);

        if (!stats.example_nodes.empty()) {
            ins.seed_nodes = {stats.example_nodes.front(), stats.example_nodes.back()};
            ins.seed_labels = {
                get_node_label(stats.example_nodes.front()),
                get_node_label(stats.example_nodes.back())
            };
            ins.witness_nodes = stats.example_nodes;
            ins.witness_edges = stats.example_edges;
            ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        }

        std::ostringstream desc;
        desc << "Meta-path anomaly: " << pattern
             << " (support_ratio=" << std::fixed << std::setprecision(2) << support_ratio
             << ", anomaly=" << std::setprecision(2) << anomaly
             << ", plausibility=" << std::setprecision(2) << plausibility << ")";
        ins.description = desc.str();

        ins.score_breakdown["support_count"] = static_cast<double>(support);
        ins.score_breakdown["support_ratio"] = support_ratio;
        ins.score_breakdown["anomaly"] = anomaly;
        ins.score_breakdown["plausibility"] = plausibility;
        ins.score_breakdown["length"] = static_cast<double>(stats.length);
        ins.score_breakdown["dynamic_min_anomaly"] = dynamic_min_anomaly;
        ins.score = final_score;
        ins.novelty_tags = {"meta_path_anomaly", "pattern=" + pattern};

        results.push_back(std::move(ins));
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.meta_path_anomaly_max_candidates) {
        results.resize(config_.meta_path_anomaly_max_candidates);
    }

    report_progress("Meta-path anomaly", 100, 100);
    return results;
}

// =============================================================================
// INTERVENTION_BOTTLENECK: Critical bottleneck nodes in causal chains
// =============================================================================

// REMOVED: Low-value insight (2 insights across 9 runs)
#if 0
std::vector<Insight> DiscoveryEngine::find_intervention_bottlenecks(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Intervention bottlenecks", 0, 100);

    struct ChainView {
        std::vector<std::string> nodes;
        std::vector<std::string> edges;
        double score = 0.0;
    };

    std::vector<ChainView> chains;
    chains.reserve(collection.insights.size());

    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::CAUSAL_CHAIN &&
            insight.type != InsightType::HYPOTHESES_3) {
            continue;
        }

        std::vector<std::string> nodes = insight.witness_nodes;
        if (nodes.size() < 3) nodes = insight.seed_nodes;
        if (nodes.size() < 3) continue;

        chains.push_back({nodes, insight.witness_edges, insight.score});
    }

    if (chains.empty()) {
        auto generated = find_causal_chains();
        for (const auto& insight : generated) {
            std::vector<std::string> nodes = insight.witness_nodes;
            if (nodes.size() < 3) nodes = insight.seed_nodes;
            if (nodes.size() < 3) continue;
            chains.push_back({nodes, insight.witness_edges, insight.score});
        }
    }

    if (chains.empty()) {
        report_progress("Intervention bottlenecks", 100, 100);
        return results;
    }

    struct NodeAgg {
        int chain_count = 0;
        double score_sum = 0.0;
        std::unordered_set<std::string> endpoint_pairs;
        std::unordered_set<std::string> witness_edges;
        std::unordered_set<std::string> witness_nodes;
    };

    std::unordered_map<std::string, NodeAgg> node_stats;
    int total_chains = 0;

    for (const auto& chain : chains) {
        if (chain.nodes.size() < 3) continue;
        total_chains++;

        std::string endpoint_key = chain.nodes.front() + "||" + chain.nodes.back();
        for (size_t i = 1; i + 1 < chain.nodes.size(); ++i) {
            const std::string& node_id = chain.nodes[i];
            auto& agg = node_stats[node_id];
            agg.chain_count++;
            agg.score_sum += chain.score;
            agg.endpoint_pairs.insert(endpoint_key);
            agg.witness_nodes.insert(chain.nodes.front());
            agg.witness_nodes.insert(chain.nodes.back());

            for (const auto& eid : chain.edges) {
                agg.witness_edges.insert(eid);
                if (agg.witness_edges.size() >
                    config_.intervention_bottleneck_max_evidence_edges * 4) {
                    break;
                }
            }
        }
    }

    if (total_chains == 0 || node_stats.empty()) {
        report_progress("Intervention bottlenecks", 100, 100);
        return results;
    }

    size_t max_span = 1;
    for (const auto& [_, agg] : node_stats) {
        max_span = std::max(max_span, agg.endpoint_pairs.size());
    }

    double dynamic_min_coverage = config_.intervention_bottleneck_min_coverage;
    if (total_chains < 25) dynamic_min_coverage = std::max(0.03, dynamic_min_coverage - 0.03);
    if (total_chains > 200) dynamic_min_coverage = std::min(0.25, dynamic_min_coverage + 0.05);

    for (const auto& [node_id, agg] : node_stats) {
        double coverage = static_cast<double>(agg.chain_count) / static_cast<double>(total_chains);
        if (coverage < dynamic_min_coverage) continue;

        double chain_strength = agg.chain_count > 0
            ? (agg.score_sum / static_cast<double>(agg.chain_count))
            : 0.0;
        double span = static_cast<double>(agg.endpoint_pairs.size()) / static_cast<double>(max_span);

        double final_score =
            config_.intervention_bottleneck_weight_coverage * coverage +
            config_.intervention_bottleneck_weight_chain_strength * chain_strength +
            config_.intervention_bottleneck_weight_span * span;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::INTERVENTION_BOTTLENECK);
        ins.set_type(InsightType::INTERVENTION_BOTTLENECK);
        ins.seed_nodes = {node_id};
        ins.seed_labels = {get_node_label(node_id)};

        ins.witness_nodes.push_back(node_id);
        for (const auto& wn : agg.witness_nodes) {
            if (wn == node_id) continue;
            ins.witness_nodes.push_back(wn);
            if (ins.witness_nodes.size() >= 10) break;
        }

        for (const auto& eid : agg.witness_edges) {
            ins.witness_edges.push_back(eid);
            if (ins.witness_edges.size() >= config_.intervention_bottleneck_max_evidence_edges) break;
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

        std::ostringstream desc;
        desc << "Intervention bottleneck: '" << get_node_label(node_id)
             << "' appears in " << agg.chain_count << "/" << total_chains
             << " causal chains and connects " << agg.endpoint_pairs.size()
             << " endpoint pairs.";
        ins.description = desc.str();

        ins.score_breakdown["coverage"] = coverage;
        ins.score_breakdown["chain_strength"] = chain_strength;
        ins.score_breakdown["span"] = span;
        ins.score_breakdown["affected_chains"] = static_cast<double>(agg.chain_count);
        ins.score_breakdown["dynamic_min_coverage"] = dynamic_min_coverage;
        ins.score = final_score;
        ins.novelty_tags = {"intervention_bottleneck"};

        results.push_back(std::move(ins));
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.intervention_bottleneck_max_candidates) {
        results.resize(config_.intervention_bottleneck_max_candidates);
    }

    report_progress("Intervention bottlenecks", 100, 100);
    return results;
}
#endif // REMOVED: intervention_bottleneck

// =============================================================================
// COMPETING_MECHANISM: Alternative mechanism sets for same outcome
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_competing_mechanisms(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Competing mechanisms", 0, 100);

    struct Mechanism {
        std::string source;
        std::string outcome;
        std::unordered_set<std::string> nodes;
        std::unordered_set<std::string> edges;
        double score = 0.0;
    };

    std::vector<Mechanism> mechanisms;
    mechanisms.reserve(collection.insights.size());

    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::HYPOTHESES_2 &&
            insight.type != InsightType::HYPOTHESES_3 &&
            insight.type != InsightType::LONG_CHAIN &&
            insight.type != InsightType::EXPLANATORY_CHAIN &&
            insight.type != InsightType::MECHANISM_CONSOLIDATION) {
            continue;
        }
        if (insight.seed_nodes.size() < 2) continue;

        Mechanism m;
        m.source = insight.seed_nodes.front();
        m.outcome = insight.seed_nodes.back();
        m.score = insight.score;

        for (const auto& n : insight.seed_nodes) m.nodes.insert(n);
        for (const auto& n : insight.witness_nodes) m.nodes.insert(n);
        if (m.nodes.size() < 2) continue;

        for (const auto& e : insight.witness_edges) m.edges.insert(e);
        mechanisms.push_back(std::move(m));
    }

    if (mechanisms.empty()) {
        report_progress("Competing mechanisms", 100, 100);
        return results;
    }

    std::unordered_map<std::string, int> contradiction_hits;
    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::CONTRADICTION) continue;
        for (const auto& node_id : insight.seed_nodes) contradiction_hits[node_id]++;
    }

    std::unordered_map<std::string, std::vector<Mechanism>> by_outcome;
    for (const auto& m : mechanisms) {
        by_outcome[m.outcome].push_back(m);
    }

    size_t dynamic_min_alts = config_.competing_mechanism_min_alternatives;
    if (mechanisms.size() > 150) dynamic_min_alts += 1;

    double dynamic_min_divergence = config_.competing_mechanism_min_divergence;
    if (mechanisms.size() < 40) dynamic_min_divergence = std::max(0.25, dynamic_min_divergence - 0.05);

    for (const auto& [outcome, group] : by_outcome) {
        if (group.size() < dynamic_min_alts) continue;

        int best_i = -1;
        int best_j = -1;
        double best_divergence = 0.0;
        double best_support = 0.0;
        double best_pair_score = -1.0;

        for (size_t i = 0; i < group.size(); ++i) {
            for (size_t j = i + 1; j < group.size(); ++j) {
                double overlap = jaccard_overlap(group[i].nodes, group[j].nodes);
                if (overlap > config_.competing_mechanism_max_overlap) continue;

                double divergence = 1.0 - overlap;
                if (divergence < dynamic_min_divergence) continue;

                double support = (group[i].score + group[j].score) / 2.0;
                double pair_score = 0.6 * divergence + 0.4 * support;
                if (pair_score > best_pair_score) {
                    best_pair_score = pair_score;
                    best_i = static_cast<int>(i);
                    best_j = static_cast<int>(j);
                    best_divergence = divergence;
                    best_support = support;
                }
            }
        }

        if (best_i < 0 || best_j < 0) continue;

        int contradiction_count = contradiction_hits[outcome];
        contradiction_count += contradiction_hits[group[best_i].source];
        contradiction_count += contradiction_hits[group[best_j].source];
        double contradiction_penalty = std::min(1.0, static_cast<double>(contradiction_count) / 3.0);

        double final_score =
            config_.competing_mechanism_weight_divergence * best_divergence +
            config_.competing_mechanism_weight_support * best_support -
            config_.competing_mechanism_weight_contradiction_penalty * contradiction_penalty;
        final_score = std::max(0.0, final_score);

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::COMPETING_MECHANISM);
        ins.set_type(InsightType::COMPETING_MECHANISM);
        ins.seed_nodes = {group[best_i].source, group[best_j].source, outcome};
        ins.seed_labels = {
            get_node_label(group[best_i].source),
            get_node_label(group[best_j].source),
            get_node_label(outcome)
        };

        std::unordered_set<std::string> witness_nodes = group[best_i].nodes;
        witness_nodes.insert(group[best_j].nodes.begin(), group[best_j].nodes.end());
        for (const auto& n : witness_nodes) {
            ins.witness_nodes.push_back(n);
            if (ins.witness_nodes.size() >= 20) break;
        }

        std::unordered_set<std::string> witness_edges = group[best_i].edges;
        witness_edges.insert(group[best_j].edges.begin(), group[best_j].edges.end());
        for (const auto& e : witness_edges) {
            ins.witness_edges.push_back(e);
            if (ins.witness_edges.size() >= 30) break;
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

        std::ostringstream desc;
        desc << "Competing mechanisms for outcome '" << get_node_label(outcome) << "': '"
             << get_node_label(group[best_i].source) << "' versus '"
             << get_node_label(group[best_j].source)
             << "' (divergence=" << std::fixed << std::setprecision(2) << best_divergence << ").";
        ins.description = desc.str();

        ins.score_breakdown["alternatives"] = static_cast<double>(group.size());
        ins.score_breakdown["divergence"] = best_divergence;
        ins.score_breakdown["support"] = best_support;
        ins.score_breakdown["contradiction_penalty"] = contradiction_penalty;
        ins.score_breakdown["dynamic_min_divergence"] = dynamic_min_divergence;
        ins.score = final_score;
        ins.novelty_tags = {"competing_mechanism"};

        results.push_back(std::move(ins));
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.competing_mechanism_max_candidates) {
        results.resize(config_.competing_mechanism_max_candidates);
    }

    report_progress("Competing mechanisms", 100, 100);
    return results;
}

// =============================================================================
// SCHEMA_REPAIR: Actionable repair suggestions from violations and closure gaps
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_schema_repairs(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Schema repair", 0, 100);

    auto parse_kv = [](const std::string& text) {
        std::map<std::string, std::string> kv;
        std::stringstream ss(text);
        std::string token;
        while (std::getline(ss, token, '|')) {
            auto pos = token.find(':');
            if (pos == std::string::npos) continue;
            kv[token.substr(0, pos)] = token.substr(pos + 1);
        }
        return kv;
    };

    std::unordered_map<std::string, std::unordered_map<std::string, int>> relation_targets;
    for (const auto& edge : graph_.get_all_edges()) {
        std::string rel = edge.relation.empty() ? "related_to" : edge.relation;
        for (const auto& tgt : edge.targets) {
            relation_targets[rel][tgt]++;
        }
    }

    auto hubs = graph_.get_top_hubs(50);
    auto choose_target = [&](const std::string& relation, const std::string& source) -> std::string {
        auto it = relation_targets.find(relation);
        if (it != relation_targets.end()) {
            std::string best;
            int best_count = -1;
            for (const auto& [node_id, count] : it->second) {
                if (node_id == source) continue;
                if (count > best_count) {
                    best_count = count;
                    best = node_id;
                }
            }
            if (!best.empty()) return best;
        }
        for (const auto& [node_id, _] : hubs) {
            if (node_id != source) return node_id;
        }
        return "";
    };

    struct RepairAgg {
        double gain_sum = 0.0;
        double support_sum = 0.0;
        double consistency_sum = 0.0;
        size_t count = 0;
        std::unordered_set<std::string> witness_edges;
        std::unordered_set<std::string> witness_nodes;
        std::unordered_set<std::string> chunk_ids;
        std::string reason;
    };

    std::map<std::tuple<std::string, std::string, std::string>, RepairAgg> repairs;

    auto add_repair = [&](const std::string& src,
                          const std::string& rel,
                          const std::string& tgt,
                          double gain,
                          double support,
                          double consistency,
                          const Insight& source,
                          const std::string& reason) {
        if (src.empty() || rel.empty() || tgt.empty() || src == tgt) return;
        auto key = std::make_tuple(src, rel, tgt);
        auto& agg = repairs[key];
        agg.gain_sum += gain;
        agg.support_sum += support;
        agg.consistency_sum += consistency;
        agg.count++;
        agg.reason = reason;
        for (const auto& e : source.witness_edges) agg.witness_edges.insert(e);
        for (const auto& n : source.witness_nodes) agg.witness_nodes.insert(n);
        for (const auto& c : source.evidence_chunk_ids) agg.chunk_ids.insert(c);
    };

    for (const auto& insight : collection.insights) {
        if (insight.type == InsightType::TRANSITIVE_CLOSURE) {
            if (insight.seed_nodes.size() < 2) continue;
            auto meta = parse_kv(insight.description);
            std::string relation = meta.count("relation") ? meta["relation"] : "related_to";
            double chain_support = 0.5;
            auto it = insight.score_breakdown.find("chain_count");
            if (it != insight.score_breakdown.end()) {
                chain_support = std::min(1.0, it->second / 5.0);
            }
            add_repair(insight.seed_nodes[0], relation, insight.seed_nodes[1],
                       insight.score, chain_support, 0.90, insight, "transitive_closure");
        } else if (insight.type == InsightType::COMPLETION) {
            if (insight.seed_nodes.size() < 2) continue;
            std::string relation = "related_to";
            for (const auto& tag : insight.novelty_tags) {
                if (tag.rfind("relation=", 0) == 0) {
                    relation = tag.substr(9);
                    break;
                }
            }
            double support = std::min(1.0, static_cast<double>(insight.witness_edges.size()) / 6.0);
            add_repair(insight.seed_nodes[0], relation, insight.seed_nodes[1],
                       std::min(1.0, insight.score * 0.85 + 0.10), support, 0.70, insight, "completion");
        } else if (insight.type == InsightType::SCHEMA_VIOLATION) {
            if (insight.seed_nodes.empty()) continue;
            auto meta = parse_kv(insight.description);
            std::string relation = meta.count("missing_relation") ? meta["missing_relation"] : "";
            if (relation.empty()) continue;
            std::string src = insight.seed_nodes[0];
            std::string tgt = choose_target(relation, src);
            if (tgt.empty()) continue;

            double support = 0.5;
            auto it = insight.score_breakdown.find("severity");
            if (it != insight.score_breakdown.end()) support = std::clamp(it->second, 0.0, 1.0);
            add_repair(src, relation, tgt, insight.score, support, 0.80, insight, "schema_violation");
        }
    }

    if (repairs.empty()) {
        report_progress("Schema repair", 100, 100);
        return results;
    }

    auto stats = graph_.compute_statistics();
    double dynamic_min_gain = config_.schema_repair_min_gain;
    if (stats.num_edges > 5000) dynamic_min_gain += 0.05;
    if (stats.num_edges < 500) dynamic_min_gain -= 0.05;
    dynamic_min_gain = std::clamp(dynamic_min_gain, 0.15, 0.90);

    for (const auto& [key, agg] : repairs) {
        const auto& src = std::get<0>(key);
        const auto& rel = std::get<1>(key);
        const auto& tgt = std::get<2>(key);
        if (agg.count == 0) continue;

        double gain = agg.gain_sum / static_cast<double>(agg.count);
        double support = agg.support_sum / static_cast<double>(agg.count);
        double consistency = agg.consistency_sum / static_cast<double>(agg.count);

        double final_score =
            config_.schema_repair_weight_gain * gain +
            config_.schema_repair_weight_support * support +
            config_.schema_repair_weight_consistency * consistency;
        if (final_score < dynamic_min_gain) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::SCHEMA_REPAIR);
        ins.set_type(InsightType::SCHEMA_REPAIR);
        ins.seed_nodes = {src, tgt};
        ins.seed_labels = {get_node_label(src), get_node_label(tgt)};

        ins.witness_nodes = {src, tgt};
        for (const auto& n : agg.witness_nodes) {
            if (n == src || n == tgt) continue;
            ins.witness_nodes.push_back(n);
            if (ins.witness_nodes.size() >= 14) break;
        }

        for (const auto& e : agg.witness_edges) {
            ins.witness_edges.push_back(e);
            if (ins.witness_edges.size() >= config_.schema_repair_max_witness_edges) break;
        }
        for (const auto& c : agg.chunk_ids) {
            ins.evidence_chunk_ids.push_back(c);
            if (ins.evidence_chunk_ids.size() >= 10) break;
        }

        std::ostringstream desc;
        desc << "Schema repair suggestion: add relation '" << rel << "' between '"
             << get_node_label(src) << "' and '" << get_node_label(tgt)
             << "' (derived from " << agg.reason << ").";
        ins.description = desc.str();

        ins.score_breakdown["gain"] = gain;
        ins.score_breakdown["support"] = support;
        ins.score_breakdown["consistency"] = consistency;
        ins.score_breakdown["dynamic_min_gain"] = dynamic_min_gain;
        ins.score = final_score;
        ins.novelty_tags = {"schema_repair", "relation=" + rel, "reason=" + agg.reason};

        results.push_back(std::move(ins));
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.schema_repair_max_candidates) {
        results.resize(config_.schema_repair_max_candidates);
    }

    report_progress("Schema repair", 100, 100);
    return results;
}

// =============================================================================
// CROSS_COMMUNITY_MECHANISM_BRIDGE: Bridges mechanisms across communities/domains
// =============================================================================

std::vector<Insight> DiscoveryEngine::find_cross_community_mechanism_bridges(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Cross-community mechanism bridges", 0, 100);

    std::unordered_set<std::string> bridge_nodes;
    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::DOMAIN_BRIDGE) continue;
        for (const auto& n : insight.seed_nodes) bridge_nodes.insert(n);
        for (const auto& n : insight.witness_nodes) bridge_nodes.insert(n);
    }

    std::map<std::pair<std::string, std::string>, double> community_pair_scores;
    std::unordered_set<std::string> community_nodes;
    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::COMMUNITY_LINK || insight.seed_nodes.size() < 2) continue;
        std::string a = insight.seed_nodes[0];
        std::string b = insight.seed_nodes[1];
        if (a > b) std::swap(a, b);
        community_pair_scores[{a, b}] = std::max(community_pair_scores[{a, b}], insight.score);
        community_nodes.insert(a);
        community_nodes.insert(b);
    }

    struct Candidate {
        std::string start;
        std::string end;
        std::vector<std::string> witness_nodes;
        std::vector<std::string> witness_edges;
        std::vector<std::string> bridge_hits;
        double support = 0.0;
        double novelty = 0.0;
        double coherence = 0.0;
        double score = 0.0;
    };

    std::vector<Candidate> candidates;

    auto avg_edge_confidence = [&](const std::vector<std::string>& edge_ids) {
        if (edge_ids.empty()) return 0.0;
        double sum = 0.0;
        size_t count = 0;
        for (const auto& eid : edge_ids) {
            const auto* e = graph_.get_hyperedge(eid);
            if (!e) continue;
            sum += e->confidence;
            count++;
        }
        return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
    };

    for (const auto& insight : collection.insights) {
        if (insight.type != InsightType::LONG_CHAIN &&
            insight.type != InsightType::EXPLANATORY_CHAIN &&
            insight.type != InsightType::HYPOTHESES_2 &&
            insight.type != InsightType::MECHANISM_CONSOLIDATION) {
            continue;
        }
        if (insight.seed_nodes.size() < 2) continue;

        std::string start = insight.seed_nodes.front();
        std::string end = insight.seed_nodes.back();
        if (start.empty() || end.empty() || start == end) continue;

        std::vector<std::string> bridge_hits;
        for (const auto& n : insight.witness_nodes) {
            if (bridge_nodes.find(n) != bridge_nodes.end()) bridge_hits.push_back(n);
        }
        for (const auto& n : insight.seed_nodes) {
            if (bridge_nodes.find(n) != bridge_nodes.end()) bridge_hits.push_back(n);
        }
        std::sort(bridge_hits.begin(), bridge_hits.end());
        bridge_hits.erase(std::unique(bridge_hits.begin(), bridge_hits.end()), bridge_hits.end());
        if (bridge_hits.empty()) continue;

        std::string a = start;
        std::string b = end;
        if (a > b) std::swap(a, b);

        double novelty = 0.0;
        auto it = community_pair_scores.find({a, b});
        if (it != community_pair_scores.end()) {
            novelty = std::min(1.0, 0.5 + it->second * 0.5);
        } else {
            bool touches_community = community_nodes.count(a) || community_nodes.count(b);
            novelty = touches_community ? 0.30 : 0.15;
        }

        double support = std::clamp(insight.score, 0.0, 1.0);
        double coherence = avg_edge_confidence(insight.witness_edges);
        if (coherence <= 0.0) {
            coherence = std::min(1.0, static_cast<double>(insight.witness_nodes.size()) / 10.0);
        }

        Candidate cand;
        cand.start = start;
        cand.end = end;
        cand.witness_nodes = insight.witness_nodes;
        cand.witness_edges = insight.witness_edges;
        cand.bridge_hits = std::move(bridge_hits);
        cand.support = support;
        cand.novelty = novelty;
        cand.coherence = coherence;
        candidates.push_back(std::move(cand));
    }

    if (candidates.empty()) {
        report_progress("Cross-community mechanism bridges", 100, 100);
        return results;
    }

    double dynamic_min_novelty = config_.cross_community_bridge_min_novelty;
    if (community_pair_scores.size() > 50) dynamic_min_novelty += 0.05;
    dynamic_min_novelty = std::clamp(dynamic_min_novelty, 0.15, 0.90);

    double dynamic_min_support = config_.cross_community_bridge_min_support;
    if (candidates.size() < 20) dynamic_min_support = std::max(0.10, dynamic_min_support - 0.05);

    std::unordered_set<std::string> seen;
    for (auto& cand : candidates) {
        if (cand.novelty < dynamic_min_novelty || cand.support < dynamic_min_support) continue;

        cand.score =
            config_.cross_community_bridge_weight_novelty * cand.novelty +
            config_.cross_community_bridge_weight_support * cand.support +
            config_.cross_community_bridge_weight_coherence * cand.coherence;

        std::string k1 = cand.start;
        std::string k2 = cand.end;
        if (k1 > k2) std::swap(k1, k2);
        std::string dedup_key = k1 + "||" + k2;
        if (!seen.insert(dedup_key).second) continue;

        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE);
        ins.set_type(InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE);
        ins.seed_nodes = {cand.start, cand.end};
        ins.seed_labels = {get_node_label(cand.start), get_node_label(cand.end)};

        ins.witness_nodes = {cand.start};
        for (const auto& bnode : cand.bridge_hits) {
            if (bnode != cand.start && bnode != cand.end) {
                ins.witness_nodes.push_back(bnode);
                if (ins.witness_nodes.size() >= 8) break;
            }
        }
        ins.witness_nodes.push_back(cand.end);

        for (const auto& e : cand.witness_edges) {
            ins.witness_edges.push_back(e);
            if (ins.witness_edges.size() >= config_.cross_community_bridge_max_witness_edges) break;
        }
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);

        std::ostringstream desc;
        desc << "Cross-community mechanism bridge: '" << get_node_label(cand.start)
             << "' ↔ '" << get_node_label(cand.end) << "' via bridge node(s) ";
        for (size_t i = 0; i < std::min<size_t>(cand.bridge_hits.size(), 2); ++i) {
            if (i > 0) desc << ", ";
            desc << "'" << get_node_label(cand.bridge_hits[i]) << "'";
        }
        if (cand.bridge_hits.size() > 2) desc << ", ...";
        desc << ".";
        ins.description = desc.str();

        ins.score_breakdown["novelty"] = cand.novelty;
        ins.score_breakdown["support"] = cand.support;
        ins.score_breakdown["coherence"] = cand.coherence;
        ins.score_breakdown["bridge_nodes"] = static_cast<double>(cand.bridge_hits.size());
        ins.score = cand.score;
        ins.novelty_tags = {"cross_community_mechanism_bridge"};

        results.push_back(std::move(ins));
    }

    std::sort(results.begin(), results.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });
    if (results.size() > config_.cross_community_bridge_max_candidates) {
        results.resize(config_.cross_community_bridge_max_candidates);
    }

    report_progress("Cross-community mechanism bridges", 100, 100);
    return results;
}

// Phase 2: Apply causal filtering to insights
std::vector<Insight> DiscoveryEngine::apply_causal_filter(const std::vector<Insight>& insights) const {
    // If no filters are active, return all insights
    if (!causal_filter_.has_filters()) {
        return insights;
    }

    std::vector<Insight> filtered;

    for (const auto& insight : insights) {
        // Only filter causal insight types
        if (insight.type != InsightType::CAUSAL_CHAIN &&
            insight.type != InsightType::INTERVENTION_POINT &&
            insight.type != InsightType::FEEDBACK_LOOP &&
            insight.type != InsightType::CONFOUNDER) {
            // Non-causal insights pass through unchanged
            filtered.push_back(insight);
            continue;
        }

        // Check if insight has causal metadata
        bool has_causal_metadata = false;
        bool passes_filter = false;

        // Check all witness edges for causal metadata
        for (const auto& edge_id : insight.witness_edges) {
            const auto* edge = graph_.get_hyperedge(edge_id);
            if (!edge || !edge->is_causal()) {
                continue;
            }

            has_causal_metadata = true;
            const auto& causal = *edge->causal_metadata;

            // Check strength filter
            bool strength_match = causal_filter_.strengths.empty();
            if (!strength_match) {
                std::string strength_str = causal.get_strength_string();
                for (const auto& filter_strength : causal_filter_.strengths) {
                    if (strength_str == filter_strength) {
                        strength_match = true;
                        break;
                    }
                }
            }

            // Check type filter
            bool type_match = causal_filter_.types.empty();
            if (!type_match) {
                std::string type_str = causal.get_type_string();
                for (const auto& filter_type : causal_filter_.types) {
                    if (type_str == filter_type) {
                        type_match = true;
                        break;
                    }
                }
            }

            // Check mechanism type filter
            bool mechanism_match = causal_filter_.mechanism_types.empty();
            if (!mechanism_match && !causal.mechanism_type.empty()) {
                for (const auto& filter_mech : causal_filter_.mechanism_types) {
                    if (causal.mechanism_type == filter_mech) {
                        mechanism_match = true;
                        break;
                    }
                }
            }

            // Check temporality filter
            bool temporality_match = causal_filter_.temporalities.empty();
            if (!temporality_match) {
                std::string temp_str = causal.get_temporality_string();
                for (const auto& filter_temp : causal_filter_.temporalities) {
                    if (temp_str == filter_temp) {
                        temporality_match = true;
                        break;
                    }
                }
            }

            // If all active filters match, this edge passes
            if (strength_match && type_match && mechanism_match && temporality_match) {
                passes_filter = true;
                break;  // Found at least one edge that matches all filters
            }
        }

        // Include insight if:
        // 1. It has no causal metadata (fallback to keyword detection), OR
        // 2. It has causal metadata that passes all filters
        if (!has_causal_metadata || passes_filter) {
            filtered.push_back(insight);
        }
    }

    return filtered;
}

// ============================================================================
// Semantic Deduplication of Insights
// ============================================================================

void DiscoveryEngine::deduplicate_insights(InsightCollection& insights, double similarity_threshold) {
    if (insights.insights.empty()) return;

    // Helper: Normalize label for comparison
    auto normalize_label = [](const std::string& label) -> std::string {
        std::string normalized = label;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

        // Remove trailing filler words
        const std::vector<std::string> fillers = {
            " technique", " techniques", " method", " methods",
            " approach", " approaches", " algorithm", " algorithms",
            " system", " systems", " framework", " frameworks",
            " process", " processes", " procedure", " procedures",
            " strategy", " strategies", " mechanism", " mechanisms"
        };

        for (const auto& filler : fillers) {
            size_t pos = normalized.rfind(filler);
            if (pos != std::string::npos && pos + filler.length() == normalized.length()) {
                normalized = normalized.substr(0, pos);
                break;
            }
        }

        return normalized;
    };

    // Helper: Compute Levenshtein distance
    auto levenshtein_distance = [](const std::string& s1, const std::string& s2) -> int {
        const size_t m = s1.length();
        const size_t n = s2.length();

        if (m == 0) return static_cast<int>(n);
        if (n == 0) return static_cast<int>(m);

        std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

        for (size_t i = 0; i <= m; i++) dp[i][0] = static_cast<int>(i);
        for (size_t j = 0; j <= n; j++) dp[0][j] = static_cast<int>(j);

        for (size_t i = 1; i <= m; i++) {
            for (size_t j = 1; j <= n; j++) {
                int cost = (std::tolower(s1[i-1]) == std::tolower(s2[j-1])) ? 0 : 1;
                dp[i][j] = std::min({
                    dp[i-1][j] + 1,      // deletion
                    dp[i][j-1] + 1,      // insertion
                    dp[i-1][j-1] + cost  // substitution
                });
            }
        }

        return dp[m][n];
    };

    // Helper: Create canonical key for grouping
    auto create_canonical_key = [&normalize_label](const Insight& insight) -> std::string {
        std::string key = insight_type_to_string(insight.type) + ":";

        // Add normalized seed labels (sorted for consistency)
        std::vector<std::string> normalized_labels;
        for (const auto& label : insight.seed_labels) {
            normalized_labels.push_back(normalize_label(label));
        }
        std::sort(normalized_labels.begin(), normalized_labels.end());

        for (const auto& label : normalized_labels) {
            key += label + ",";
        }

        return key;
    };

    // Group insights by canonical key
    std::map<std::string, std::vector<size_t>> groups;

    for (size_t i = 0; i < insights.insights.size(); i++) {
        const auto& insight = insights.insights[i];
        std::string key = create_canonical_key(insight);
        groups[key].push_back(i);
    }

    // Mark duplicates for removal
    std::set<size_t> indices_to_remove;
    int groups_merged = 0;
    int insights_removed = 0;

    for (auto& [key, group_indices] : groups) {
        if (group_indices.size() <= 1) continue;

        // Within each group, check for fuzzy duplicates
        std::vector<std::pair<size_t, double>> scored_insights;
        for (size_t idx : group_indices) {
            scored_insights.push_back({idx, insights.insights[idx].score});
        }

        // Sort by score (descending)
        std::sort(scored_insights.begin(), scored_insights.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        // Keep the highest-scored insight, mark others for removal if similar enough
        size_t keeper_idx = scored_insights[0].first;
        const auto& keeper = insights.insights[keeper_idx];

        for (size_t i = 1; i < scored_insights.size(); i++) {
            size_t candidate_idx = scored_insights[i].first;
            const auto& candidate = insights.insights[candidate_idx];

            // Check if seed_labels are similar enough
            bool is_duplicate = false;

            if (keeper.seed_labels.size() == candidate.seed_labels.size()) {
                // Compare all seed labels
                size_t matching_labels = 0;

                for (size_t j = 0; j < keeper.seed_labels.size(); j++) {
                    const std::string& label1 = keeper.seed_labels[j];
                    const std::string& label2 = candidate.seed_labels[j];

                    // Compute similarity
                    int dist = levenshtein_distance(label1, label2);
                    size_t max_len = std::max(label1.length(), label2.length());
                    double similarity = (max_len > 0) ? (1.0 - (double)dist / max_len) : 1.0;

                    if (similarity >= similarity_threshold) {
                        matching_labels++;
                    }
                }

                // If most labels match, consider it a duplicate
                if (matching_labels >= keeper.seed_labels.size() * 0.8) {
                    is_duplicate = true;
                }
            }

            if (is_duplicate) {
                indices_to_remove.insert(candidate_idx);
                insights_removed++;
            }
        }

        if (indices_to_remove.size() > 0) {
            groups_merged++;
        }
    }

    // Remove duplicates (iterate backwards to preserve indices)
    std::vector<Insight> deduplicated;
    for (size_t i = 0; i < insights.insights.size(); i++) {
        if (indices_to_remove.find(i) == indices_to_remove.end()) {
            deduplicated.push_back(std::move(insights.insights[i]));
        }
    }

    insights.insights = std::move(deduplicated);

    if (insights_removed > 0) {
        std::cout << "  Deduplicated insights: merged " << groups_merged
                  << " groups, removed " << insights_removed << " duplicates\n";
    }
}

// =============================================================================
// EPISTEMIC DISCOVERY OPERATORS
// =============================================================================

namespace {
// Generic-term penalty: skip seeds with uninformative labels
bool is_generic_epistemic_label(const std::string& label) {
    static const std::vector<std::string> generic_terms = {
        "kg", "graph", "method", "entity", "relation", "knowledge", "model",
        "node", "edge", "data", "system", "approach", "technique", "framework"
    };
    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    for (const auto& term : generic_terms) {
        if (lower == term) return true;
    }
    return false;
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// evidence_debt: High-impact nodes with narrow evidence provenance
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_evidence_debt(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding evidence debt", 0, 100);

    if (collection.insights.empty()) return results;

    // Count how many insights each node appears in (impact) and collect their evidence
    struct NodeStats {
        std::string id;
        std::string label;
        int insight_count = 0;
        std::set<std::string> all_sources;
        std::set<std::string> all_chunks;
        std::set<std::string> operator_types;
        std::vector<std::string> witness_edges;
    };

    std::unordered_map<std::string, NodeStats> stats;

    for (const auto& ins : collection.insights) {
        std::string type_str = insight_type_to_string(ins.type);
        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& st = stats[nid];
            st.id = nid;
            if (i < ins.seed_labels.size()) st.label = ins.seed_labels[i];
            st.insight_count++;
            for (const auto& src : ins.source_documents) st.all_sources.insert(src);
            for (const auto& cid : ins.evidence_chunk_ids) st.all_chunks.insert(cid);
            st.operator_types.insert(type_str);
            for (const auto& we : ins.witness_edges) {
                if (st.witness_edges.size() < 8) st.witness_edges.push_back(we);
            }
        }
    }

    // Score: impact * support_gap
    int max_count = 1;
    for (const auto& [id, st] : stats) max_count = std::max(max_count, st.insight_count);

    struct Candidate {
        std::string id;
        std::string label;
        double score;
        double impact_score;
        double support_gap;
        int source_div;
        int chunk_div;
        int op_div;
        std::vector<std::string> witness_edges;
    };

    std::vector<Candidate> candidates;
    for (const auto& [id, st] : stats) {
        if (st.label.empty() || is_generic_epistemic_label(st.label)) continue;
        if (st.insight_count < 2) continue;

        double impact = static_cast<double>(st.insight_count) / max_count;
        int src_div = static_cast<int>(st.all_sources.size());
        int chunk_div = static_cast<int>(st.all_chunks.size());
        int op_div = static_cast<int>(st.operator_types.size());

        // support_gap: high when evidence diversity is low relative to impact
        double max_expected_sources = std::max(1.0, static_cast<double>(st.insight_count) * 0.5);
        double support_gap = 1.0 - std::min(1.0, src_div / max_expected_sources);
        support_gap = std::max(support_gap, chunk_div <= 1 ? 0.8 : 0.2);

        double score = impact * support_gap;
        if (score < 0.15) continue;

        candidates.push_back({id, st.label, score, impact, support_gap,
                              src_div, chunk_div, op_div, st.witness_edges});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::EVIDENCE_DEBT);
        ins.set_type(InsightType::EVIDENCE_DEBT);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["impact_score"] = c.impact_score;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.source_div);
        ins.score_breakdown["chunk_diversity"] = static_cast<double>(c.chunk_div);
        ins.score_breakdown["operator_diversity"] = static_cast<double>(c.op_div);
        ins.score_breakdown["support_gap"] = c.support_gap;
        ins.novelty_tags = {"epistemic", "under_supported", "high_impact"};

        std::stringstream desc;
        desc << "Evidence debt: '" << c.label << "' is highly influential in downstream insights"
             << " but is supported by narrow provenance (sources=" << c.source_div
             << ", chunks=" << c.chunk_div << ") and should be treated as under-validated.";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding evidence debt", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// consensus_frontier: Claim clusters with convergence or active disagreement
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_consensus_frontier(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding consensus frontier", 0, 100);

    if (collection.insights.empty()) return results;

    // Group seed-node pairs that appear in both supporting and contradicting insights
    // Use contradiction and claim_stance insights to measure disagreement
    struct ClaimCluster {
        std::vector<std::string> seed_nodes;
        std::vector<std::string> seed_labels;
        int support_count = 0;
        int oppose_count = 0;
        int neutral_count = 0;
        std::set<std::string> sources;
        std::vector<std::string> witness_edges;
    };

    // Index insights by their primary seed node
    std::unordered_map<std::string, std::vector<size_t>> node_to_insights;
    for (size_t i = 0; i < collection.insights.size(); ++i) {
        for (const auto& nid : collection.insights[i].seed_nodes) {
            node_to_insights[nid].push_back(i);
        }
    }

    std::map<std::string, ClaimCluster> clusters;

    for (size_t i = 0; i < collection.insights.size(); ++i) {
        const auto& ins = collection.insights[i];
        if (ins.seed_nodes.empty()) continue;

        std::string key = ins.seed_nodes[0];
        auto& cl = clusters[key];
        if (cl.seed_nodes.empty()) {
            cl.seed_nodes = ins.seed_nodes;
            cl.seed_labels = ins.seed_labels;
        }

        for (const auto& src : ins.source_documents) cl.sources.insert(src);
        for (const auto& we : ins.witness_edges) {
            if (cl.witness_edges.size() < 8) cl.witness_edges.push_back(we);
        }

        if (ins.type == InsightType::CONTRADICTION || ins.type == InsightType::CLAIM_STANCE) {
            // Check novelty_tags for stance
            bool is_oppose = false;
            for (const auto& t : ins.novelty_tags) {
                if (t == "contra" || t == "oppose" || t == "negative" || t == "contradiction") {
                    is_oppose = true; break;
                }
            }
            if (is_oppose) cl.oppose_count++;
            else cl.support_count++;
        } else {
            cl.neutral_count++;
        }
    }

    struct Candidate {
        std::string key;
        double score;
        double consensus;
        double disagreement;
        double frontier;
        ClaimCluster cl;
    };

    std::vector<Candidate> candidates;
    for (auto& [key, cl] : clusters) {
        if (cl.seed_labels.empty() || is_generic_epistemic_label(cl.seed_labels[0])) continue;
        int total = cl.support_count + cl.oppose_count + cl.neutral_count;
        if (total < 3) continue;

        double src_div = static_cast<double>(cl.sources.size());
        double consensus = total > 0 ? static_cast<double>(cl.support_count) / total : 0.0;
        double disagreement = total > 0 ? static_cast<double>(cl.oppose_count) / total : 0.0;
        double frontier = 1.0 - std::abs(consensus - 0.5) * 2.0; // high when near 50/50

        double score = 0.4 * src_div / std::max(1.0, src_div + 1.0)
                     + 0.3 * disagreement
                     + 0.3 * frontier;
        if (score < 0.1) continue;

        candidates.push_back({key, score, consensus, disagreement, frontier, cl});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CONSENSUS_FRONTIER);
        ins.set_type(InsightType::CONSENSUS_FRONTIER);
        ins.seed_nodes = c.cl.seed_nodes;
        ins.seed_labels = c.cl.seed_labels;
        ins.witness_edges = c.cl.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["support_count"] = static_cast<double>(c.cl.support_count);
        ins.score_breakdown["oppose_count"] = static_cast<double>(c.cl.oppose_count);
        ins.score_breakdown["neutral_count"] = static_cast<double>(c.cl.neutral_count);
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.cl.sources.size());
        ins.score_breakdown["consensus"] = c.consensus;
        ins.score_breakdown["disagreement"] = c.disagreement;
        ins.score_breakdown["frontier_uncertainty"] = c.frontier;
        ins.novelty_tags = {"epistemic", "consensus", "disagreement"};

        std::string label = c.cl.seed_labels.empty() ? c.key : c.cl.seed_labels[0];
        std::stringstream desc;
        desc << "Consensus frontier: the claim family around '" << label
             << "' has partial convergence (consensus=" << std::fixed << std::setprecision(2)
             << c.consensus << ", disagreement=" << c.disagreement
             << ") and remains actively contested across sources.";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding consensus frontier", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// boundary_condition_map: Conditions under which methods/claims hold
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_boundary_condition_map(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding boundary condition map", 0, 100);

    if (collection.insights.empty()) return results;

    // Look for method_outcome insights where the same node appears in both
    // high-score (positive) and lower-score (negative/conditional) insights
    struct NodeProfile {
        std::string id;
        std::string label;
        std::vector<double> scores;
        std::set<std::string> sources;
        std::vector<std::string> witness_edges;
        int method_outcome_count = 0;
        int other_count = 0;
    };

    std::unordered_map<std::string, NodeProfile> profiles;

    for (const auto& ins : collection.insights) {
        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& p = profiles[nid];
            p.id = nid;
            if (i < ins.seed_labels.size()) p.label = ins.seed_labels[i];
            p.scores.push_back(ins.score);
            for (const auto& src : ins.source_documents) p.sources.insert(src);
            for (const auto& we : ins.witness_edges) {
                if (p.witness_edges.size() < 8) p.witness_edges.push_back(we);
            }
            if (ins.type == InsightType::METHOD_OUTCOME) p.method_outcome_count++;
            else p.other_count++;
        }
    }

    struct Candidate {
        std::string id;
        std::string label;
        double score;
        double condition_specificity;
        double scope_gap;
        int src_div;
        NodeProfile p;
    };

    std::vector<Candidate> candidates;
    for (auto& [id, p] : profiles) {
        if (p.label.empty() || is_generic_epistemic_label(p.label)) continue;
        if (p.scores.size() < 3) continue;
        if (p.method_outcome_count < 1) continue;

        // Measure score variance (high variance = appears under different conditions)
        double sum = 0.0, sum2 = 0.0;
        for (double s : p.scores) { sum += s; sum2 += s * s; }
        double mean = sum / p.scores.size();
        double variance = sum2 / p.scores.size() - mean * mean;
        double condition_specificity = std::min(1.0, std::sqrt(variance) * 3.0);

        int src_div = static_cast<int>(p.sources.size());
        double scope_gap = 1.0 - std::min(1.0, static_cast<double>(src_div) / 5.0);
        double score = 0.5 * condition_specificity + 0.3 * scope_gap
                     + 0.2 * std::min(1.0, static_cast<double>(p.method_outcome_count) / 3.0);

        if (score < 0.15) continue;
        candidates.push_back({id, p.label, score, condition_specificity, scope_gap, src_div, p});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::BOUNDARY_CONDITION_MAP);
        ins.set_type(InsightType::BOUNDARY_CONDITION_MAP);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.p.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["condition_specificity"] = c.condition_specificity;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.src_div);
        ins.score_breakdown["scope_gap"] = c.scope_gap;
        ins.score_breakdown["method_outcome_count"] = static_cast<double>(c.p.method_outcome_count);
        ins.novelty_tags = {"epistemic", "boundary_condition", "scope"};

        std::stringstream desc;
        desc << "Boundary condition: '" << c.label
             << "' shows high score variance across method-outcome insights (condition_specificity="
             << std::fixed << std::setprecision(2) << c.condition_specificity
             << "), suggesting its effectiveness is strongly context-dependent.";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding boundary condition map", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// failure_mode_topology: Recurring failure patterns across the graph
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_failure_mode_topology(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding failure mode topology", 0, 100);

    if (collection.insights.empty()) return results;

    // Find nodes that appear in contradiction insights or have low scores,
    // forming recurring failure patterns
    struct NodeFailure {
        std::string id;
        std::string label;
        int contradiction_count = 0;
        int low_score_count = 0;
        std::set<std::string> sources;
        std::vector<std::string> witness_edges;
        double min_score = 1.0;
    };

    std::unordered_map<std::string, NodeFailure> failures;

    for (const auto& ins : collection.insights) {
        bool is_failure = (ins.type == InsightType::CONTRADICTION ||
                           ins.type == InsightType::CLAIM_STANCE ||
                           ins.type == InsightType::SCHEMA_VIOLATION);
        bool is_low_score = (ins.score < 0.3 && ins.score > 0.0);

        if (!is_failure && !is_low_score) continue;

        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& f = failures[nid];
            f.id = nid;
            if (i < ins.seed_labels.size()) f.label = ins.seed_labels[i];
            if (is_failure) f.contradiction_count++;
            if (is_low_score) f.low_score_count++;
            f.min_score = std::min(f.min_score, ins.score);
            for (const auto& src : ins.source_documents) f.sources.insert(src);
            for (const auto& we : ins.witness_edges) {
                if (f.witness_edges.size() < 8) f.witness_edges.push_back(we);
            }
        }
    }

    struct Candidate {
        std::string id;
        std::string label;
        double score;
        int recurrence;
        int src_div;
        NodeFailure f;
    };

    std::vector<Candidate> candidates;
    for (auto& [id, f] : failures) {
        if (f.label.empty() || is_generic_epistemic_label(f.label)) continue;
        int recurrence = f.contradiction_count + f.low_score_count;
        if (recurrence < 1) continue;

        int src_div = static_cast<int>(f.sources.size());
        double failure_recurrence = std::min(1.0, static_cast<double>(recurrence) / 5.0);
        double impact_severity = 1.0 - f.min_score;
        double score = 0.4 * failure_recurrence + 0.3 * impact_severity
                     + 0.3 * std::min(1.0, static_cast<double>(src_div) / 3.0);

        if (score < 0.1) continue;
        candidates.push_back({id, f.label, score, recurrence, src_div, f});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::FAILURE_MODE_TOPOLOGY);
        ins.set_type(InsightType::FAILURE_MODE_TOPOLOGY);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.f.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["failure_recurrence"] = static_cast<double>(c.f.contradiction_count);
        ins.score_breakdown["low_score_count"] = static_cast<double>(c.f.low_score_count);
        ins.score_breakdown["context_diversity"] = static_cast<double>(c.src_div);
        ins.score_breakdown["impact_severity"] = 1.0 - c.f.min_score;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.src_div);
        ins.novelty_tags = {"epistemic", "failure_mode", "negative_result"};

        std::stringstream desc;
        desc << "Failure mode: '" << c.label
             << "' appears in " << c.recurrence
             << " contradiction or low-confidence insights, suggesting a recurring failure pattern.";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding failure mode topology", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// benchmark_dependence: Claims concentrated on narrow evaluation regimes
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_benchmark_dependence(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding benchmark dependence", 0, 100);

    if (collection.insights.empty()) return results;

    // Find nodes from method_outcome insights that appear in only a narrow
    // set of evidence sources (high benchmark concentration)
    struct NodeEval {
        std::string id;
        std::string label;
        std::set<std::string> sources;
        std::set<std::string> chunks;
        int method_outcome_count = 0;
        int total_count = 0;
        std::vector<std::string> witness_edges;
    };

    std::unordered_map<std::string, NodeEval> evals;

    for (const auto& ins : collection.insights) {
        if (ins.type != InsightType::METHOD_OUTCOME) continue;

        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& e = evals[nid];
            e.id = nid;
            if (i < ins.seed_labels.size()) e.label = ins.seed_labels[i];
            e.method_outcome_count++;
            e.total_count++;
            for (const auto& src : ins.source_documents) e.sources.insert(src);
            for (const auto& cid : ins.evidence_chunk_ids) e.chunks.insert(cid);
            for (const auto& we : ins.witness_edges) {
                if (e.witness_edges.size() < 8) e.witness_edges.push_back(we);
            }
        }
    }

    struct Candidate {
        std::string id;
        std::string label;
        double score;
        double benchmark_concentration;
        double generalization_gap;
        int src_div;
        NodeEval e;
    };

    std::vector<Candidate> candidates;
    for (auto& [id, e] : evals) {
        if (e.label.empty() || is_generic_epistemic_label(e.label)) continue;
        if (e.method_outcome_count < 2) continue;

        int src_div = static_cast<int>(e.sources.size());
        int chunk_div = static_cast<int>(e.chunks.size());

        // High concentration = few sources relative to number of outcomes
        double benchmark_concentration = 1.0 - std::min(1.0, static_cast<double>(src_div) / std::max(1, e.method_outcome_count));
        double metric_concentration = 1.0 - std::min(1.0, static_cast<double>(chunk_div) / std::max(1, e.method_outcome_count));
        double generalization_gap = (benchmark_concentration + metric_concentration) / 2.0;

        double score = 0.4 * benchmark_concentration + 0.3 * generalization_gap
                     + 0.3 * std::min(1.0, static_cast<double>(e.method_outcome_count) / 5.0);

        if (score < 0.15) continue;
        candidates.push_back({id, e.label, score, benchmark_concentration, generalization_gap, src_div, e});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::BENCHMARK_DEPENDENCE);
        ins.set_type(InsightType::BENCHMARK_DEPENDENCE);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.e.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["benchmark_concentration"] = c.benchmark_concentration;
        ins.score_breakdown["metric_concentration"] = c.generalization_gap;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.src_div);
        ins.score_breakdown["generalization_gap"] = c.generalization_gap;
        ins.score_breakdown["method_outcome_count"] = static_cast<double>(c.e.method_outcome_count);
        ins.novelty_tags = {"epistemic", "evaluation_brittleness", "benchmark_dependence"};

        std::stringstream desc;
        desc << "Benchmark dependence: claims about '" << c.label
             << "' are concentrated on a narrow evaluation regime (sources=" << c.src_div
             << ", method_outcomes=" << c.e.method_outcome_count
             << ") and may not generalize beyond the observed benchmarks.";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding benchmark dependence", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// concept_drift: Terms with divergent meanings across communities
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_concept_drift(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding concept drift", 0, 100);

    if (collection.insights.empty()) return results;

    // Find nodes that appear in insights from very different communities
    // Use community_detection/community_link insights
    struct NodeCommunity {
        std::string id;
        std::string label;
        std::set<std::string> community_keys; // community insight IDs or seed pairs
        std::set<std::string> sources;
        std::vector<std::string> witness_edges;
        int total_appearances = 0;
    };

    std::unordered_map<std::string, NodeCommunity> node_communities;

    // First pass: collect community context for each node
    for (const auto& ins : collection.insights) {
        bool is_community = (ins.type == InsightType::COMMUNITY_DETECTION ||
                             ins.type == InsightType::COMMUNITY_LINK ||
                             ins.type == InsightType::MULTI_RESOLUTION_COMMUNITY);

        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& nc = node_communities[nid];
            nc.id = nid;
            if (i < ins.seed_labels.size()) nc.label = ins.seed_labels[i];
            nc.total_appearances++;
            for (const auto& src : ins.source_documents) nc.sources.insert(src);
            for (const auto& we : ins.witness_edges) {
                if (nc.witness_edges.size() < 8) nc.witness_edges.push_back(we);
            }
            if (is_community) {
                nc.community_keys.insert(ins.insight_id);
            }
        }
    }

    // Second pass: check graph neighborhoods for divergence
    struct Candidate {
        std::string id;
        std::string label;
        double score;
        double neighborhood_divergence;
        double community_spread;
        int src_div;
        NodeCommunity nc;
    };

    std::vector<Candidate> candidates;

    for (auto& [id, nc] : node_communities) {
        if (nc.label.empty() || is_generic_epistemic_label(nc.label)) continue;
        if (nc.total_appearances < 3) continue;

        // Measure neighborhood diversity in the actual graph
        const auto* node = graph_.get_node(id);
        if (!node) continue;

        std::set<std::string> relation_types;
        int neighbor_count = 0;
        for (const auto& eid : node->incident_edges) {
            const auto* edge = graph_.get_hyperedge(eid);
            if (!edge) continue;
            relation_types.insert(edge->relation);
            neighbor_count++;
        }

        if (neighbor_count < 2) continue;

        // High divergence = many different relation types relative to degree
        double neighborhood_divergence = std::min(1.0,
            static_cast<double>(relation_types.size()) / std::max(1, neighbor_count));

        int src_div = static_cast<int>(nc.sources.size());
        double community_spread = std::min(1.0, static_cast<double>(nc.community_keys.size()) / 3.0);

        double score = 0.4 * neighborhood_divergence + 0.35 * community_spread
                     + 0.25 * std::min(1.0, static_cast<double>(src_div) / 5.0);

        if (score < 0.15) continue;
        candidates.push_back({id, nc.label, score, neighborhood_divergence, community_spread, src_div, nc});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::CONCEPT_DRIFT);
        ins.set_type(InsightType::CONCEPT_DRIFT);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.nc.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["neighborhood_divergence"] = c.neighborhood_divergence;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.src_div);
        ins.score_breakdown["community_spread"] = c.community_spread;
        ins.score_breakdown["total_appearances"] = static_cast<double>(c.nc.total_appearances);
        ins.novelty_tags = {"epistemic", "concept_drift", "semantic_shift"};

        std::stringstream desc;
        desc << "Concept drift: '" << c.label
             << "' is used with materially different local meanings across the graph"
             << " (neighborhood_divergence=" << std::fixed << std::setprecision(2)
             << c.neighborhood_divergence << ", community_spread=" << c.community_spread << ").";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding concept drift", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// premise_bottleneck: Hidden premises that many downstream claims rely on
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_premise_bottleneck(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding premise bottleneck", 0, 100);

    if (collection.insights.empty()) return results;

    // Look at logical_entailment, rule, and argument_support insights
    // Find seed_nodes that appear repeatedly across many such insights
    struct PremiseStats {
        std::string id;
        std::string label;
        int downstream_count = 0;
        std::set<std::string> sources;
        std::set<std::string> operator_types;
        std::vector<std::string> witness_edges;
    };

    std::unordered_map<std::string, PremiseStats> premise_stats;

    static const std::set<InsightType> reasoning_types = {
        InsightType::LOGICAL_ENTAILMENT,
        InsightType::RULE,
        InsightType::ARGUMENT_SUPPORT,
        InsightType::COMPOSITIONAL_REASONING,
        InsightType::EXPLANATORY_CHAIN,
        InsightType::CAUSAL_CHAIN
    };

    for (const auto& ins : collection.insights) {
        if (reasoning_types.find(ins.type) == reasoning_types.end()) continue;

        std::string type_str = insight_type_to_string(ins.type);

        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& ps = premise_stats[nid];
            ps.id = nid;
            if (i < ins.seed_labels.size()) ps.label = ins.seed_labels[i];
            ps.downstream_count++;
            ps.operator_types.insert(type_str);
            for (const auto& src : ins.source_documents) ps.sources.insert(src);
            for (const auto& we : ins.witness_edges) {
                if (ps.witness_edges.size() < 8) ps.witness_edges.push_back(we);
            }
        }
    }

    int max_downstream = 1;
    for (const auto& [id, ps] : premise_stats) {
        max_downstream = std::max(max_downstream, ps.downstream_count);
    }

    struct Candidate {
        std::string id;
        std::string label;
        double score;
        double indispensability;
        int downstream_count;
        int src_div;
        PremiseStats ps;
    };

    std::vector<Candidate> candidates;
    for (auto& [id, ps] : premise_stats) {
        if (ps.label.empty() || is_generic_epistemic_label(ps.label)) continue;
        if (ps.downstream_count < 2) continue;

        double indispensability = static_cast<double>(ps.downstream_count) / max_downstream;
        int src_div = static_cast<int>(ps.sources.size());
        double premise_specificity = std::min(1.0, static_cast<double>(ps.operator_types.size()) / 4.0);

        // Genericity penalty: penalize if it appears in too many operators (might be structural)
        double genericity_penalty = ps.downstream_count > 10
            ? std::min(0.5, static_cast<double>(ps.downstream_count - 10) / 20.0)
            : 0.0;

        double score = 0.4 * indispensability + 0.3 * premise_specificity
                     + 0.2 * std::min(1.0, static_cast<double>(src_div) / 3.0)
                     - 0.1 * genericity_penalty;

        if (score < 0.15) continue;
        candidates.push_back({id, ps.label, score, indispensability, ps.downstream_count, src_div, ps});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::PREMISE_BOTTLENECK);
        ins.set_type(InsightType::PREMISE_BOTTLENECK);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.ps.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["downstream_claim_count"] = static_cast<double>(c.downstream_count);
        ins.score_breakdown["indispensability"] = c.indispensability;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.src_div);
        ins.score_breakdown["operator_diversity"] = static_cast<double>(c.ps.operator_types.size());
        ins.novelty_tags = {"epistemic", "premise", "dependency"};

        std::stringstream desc;
        desc << "Premise bottleneck: '" << c.label
             << "' acts as a hidden premise for " << c.downstream_count
             << " downstream reasoning and integration claims.";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding premise bottleneck", 100, 100);
    return results;
}

// -----------------------------------------------------------------------------
// translation_gap: Theory-rich but practice-weak concept nodes
// -----------------------------------------------------------------------------
std::vector<Insight> DiscoveryEngine::find_translation_gap(const InsightCollection& collection) {
    std::vector<Insight> results;
    report_progress("Finding translation gap", 0, 100);

    if (collection.insights.empty()) return results;

    // Find nodes that have many theoretical relationships (taxonomy, domain_bridge)
    // but few practical/outcome relationships (method_outcome, intervention_point)
    struct NodeBalance {
        std::string id;
        std::string label;
        int theory_count = 0;
        int application_count = 0;
        std::set<std::string> sources;
        std::vector<std::string> witness_edges;
    };

    static const std::set<InsightType> theory_types = {
        InsightType::TAXONOMY,
        InsightType::DOMAIN_BRIDGE,
        InsightType::LOGICAL_ENTAILMENT,
        InsightType::COMPOSITIONAL_REASONING,
        InsightType::RELATION_INDUCTION,
        InsightType::ANALOGICAL_TRANSFER
    };

    static const std::set<InsightType> application_types = {
        InsightType::METHOD_OUTCOME,
        InsightType::INTERVENTION_POINT,
        InsightType::CAUSAL_CHAIN,
        InsightType::FEEDBACK_LOOP,
        InsightType::CONFOUNDER
    };

    std::unordered_map<std::string, NodeBalance> balances;

    for (const auto& ins : collection.insights) {
        bool is_theory = (theory_types.find(ins.type) != theory_types.end());
        bool is_application = (application_types.find(ins.type) != application_types.end());
        if (!is_theory && !is_application) continue;

        for (size_t i = 0; i < ins.seed_nodes.size(); ++i) {
            const auto& nid = ins.seed_nodes[i];
            auto& b = balances[nid];
            b.id = nid;
            if (i < ins.seed_labels.size()) b.label = ins.seed_labels[i];
            if (is_theory) b.theory_count++;
            if (is_application) b.application_count++;
            for (const auto& src : ins.source_documents) b.sources.insert(src);
            for (const auto& we : ins.witness_edges) {
                if (b.witness_edges.size() < 8) b.witness_edges.push_back(we);
            }
        }
    }

    struct Candidate {
        std::string id;
        std::string label;
        double score;
        double theory_strength;
        double application_coverage;
        double gap_size;
        int src_div;
        NodeBalance b;
    };

    std::vector<Candidate> candidates;
    int max_theory = 1;
    for (const auto& [id, b] : balances) max_theory = std::max(max_theory, b.theory_count);

    for (auto& [id, b] : balances) {
        if (b.label.empty() || is_generic_epistemic_label(b.label)) continue;
        if (b.theory_count < 2) continue;
        // Must have significantly more theory than application
        if (b.application_count >= b.theory_count) continue;

        double theory_strength = static_cast<double>(b.theory_count) / max_theory;
        double application_coverage = static_cast<double>(b.application_count)
                                     / std::max(1, b.theory_count);
        double gap_size = 1.0 - application_coverage;
        int src_div = static_cast<int>(b.sources.size());

        double score = 0.35 * theory_strength + 0.35 * gap_size
                     + 0.3 * std::min(1.0, static_cast<double>(src_div) / 3.0);

        if (score < 0.15) continue;
        candidates.push_back({id, b.label, score, theory_strength, application_coverage, gap_size, src_div, b});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    size_t max_results = 15;
    for (size_t i = 0; i < std::min(max_results, candidates.size()); ++i) {
        const auto& c = candidates[i];
        Insight ins;
        ins.insight_id = make_insight_id(InsightType::TRANSLATION_GAP);
        ins.set_type(InsightType::TRANSLATION_GAP);
        ins.seed_nodes = {c.id};
        ins.seed_labels = {c.label};
        ins.witness_edges = c.b.witness_edges;
        ins.evidence_chunk_ids = get_chunk_ids(ins.witness_edges);
        ins.source_documents = get_source_documents(ins.witness_edges);
        ins.score = c.score;
        ins.score_breakdown["theory_strength"] = c.theory_strength;
        ins.score_breakdown["application_coverage"] = c.application_coverage;
        ins.score_breakdown["evaluation_coverage"] = c.application_coverage;
        ins.score_breakdown["gap_size"] = c.gap_size;
        ins.score_breakdown["source_diversity"] = static_cast<double>(c.src_div);
        ins.novelty_tags = {"epistemic", "theory_to_practice", "translation_gap"};

        std::stringstream desc;
        desc << "Translation gap: '" << c.label
             << "' is conceptually well-connected (theory_count=" << c.b.theory_count << ")"
             << " but weakly linked to concrete evaluation and application evidence"
             << " (application_count=" << c.b.application_count << ").";
        ins.description = desc.str();

        results.push_back(ins);
    }

    report_progress("Finding translation gap", 100, 100);
    return results;
}

} // namespace kg

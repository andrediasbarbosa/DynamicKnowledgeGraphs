#include "discovery/discovery_engine.hpp"
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

    std::vector<Insight> sorted = collection.insights;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    std::set<InsightType> used_types;
    size_t added = 0;
    for (const auto& ins : sorted) {
        if (added >= config_.hypothesis_count) break;
        if (ins.type == InsightType::AUTHOR_CHAIN) continue;
        if (used_types.insert(ins.type).second || used_types.size() < 3) {
            Insight hyp;
            hyp.insight_id = make_insight_id(InsightType::HYPOTHESIS);
            hyp.type = InsightType::HYPOTHESIS;
            hyp.seed_nodes = ins.seed_nodes;
            hyp.seed_labels = ins.seed_labels;
            hyp.witness_edges = ins.witness_edges;
            hyp.witness_nodes = ins.witness_nodes;
            hyp.evidence_chunk_ids = ins.evidence_chunk_ids;
            hyp.score_breakdown = ins.score_breakdown;
            hyp.score = ins.score;

            std::stringstream desc;
            desc << "Hypothesis grounded in " << insight_type_to_string(ins.type) << ": ";
            if (ins.seed_labels.size() >= 2) {
                desc << ins.seed_labels[0] << " <-> " << ins.seed_labels[1];
            } else if (!ins.seed_labels.empty()) {
                desc << ins.seed_labels[0];
            } else {
                desc << "key entities";
            }
            if (!ins.description.empty()) {
                desc << " (" << ins.description << ")";
            }
            hyp.description = desc.str();
            hyp.novelty_tags = {"hypothesis", "source=" + insight_type_to_string(ins.type)};

            results.push_back(std::move(hyp));
            added++;
        }
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
    return run_operators({"bridges", "completions", "motifs", "substitutions", "diffusion", "surprise", "rules", "community", "pathrank", "embedding", "author_chain", "hypotheses"});
}

} // namespace kg

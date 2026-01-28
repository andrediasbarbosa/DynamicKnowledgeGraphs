#include "graph/hypergraph.hpp"
#include <algorithm>
#include <queue>
#include <stack>
#include <cmath>
#include <fstream>
#include <sstream>
#include <numeric>
#include <stdexcept>

namespace kg {

// ==========================================
// HyperNode Implementation
// ==========================================

nlohmann::json HyperNode::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["label"] = label;
    j["degree"] = degree;
    j["properties"] = properties;
    j["incident_edges"] = incident_edges;
    if (!embedding.empty()) {
        j["embedding"] = embedding;
    }
    return j;
}

HyperNode HyperNode::from_json(const nlohmann::json& j) {
    HyperNode node;
    node.id = j.at("id").get<std::string>();
    node.label = j.at("label").get<std::string>();
    node.degree = j.value("degree", 0);

    if (j.contains("properties")) {
        node.properties = j["properties"].get<std::map<std::string, std::string>>();
    }
    if (j.contains("incident_edges")) {
        node.incident_edges = j["incident_edges"].get<std::vector<std::string>>();
    }
    if (j.contains("embedding")) {
        node.embedding = j["embedding"].get<std::vector<float>>();
    }

    return node;
}

// ==========================================
// HyperEdge Implementation
// ==========================================

std::set<std::string> HyperEdge::get_all_nodes() const {
    std::set<std::string> all_nodes;
    all_nodes.insert(sources.begin(), sources.end());
    all_nodes.insert(targets.begin(), targets.end());
    return all_nodes;
}

bool HyperEdge::contains_node(const std::string& node_id) const {
    auto in_sources = std::find(sources.begin(), sources.end(), node_id) != sources.end();
    auto in_targets = std::find(targets.begin(), targets.end(), node_id) != targets.end();
    return in_sources || in_targets;
}

std::set<std::string> HyperEdge::intersection(const HyperEdge& other) const {
    std::set<std::string> this_nodes = get_all_nodes();
    std::set<std::string> other_nodes = other.get_all_nodes();

    std::set<std::string> result;
    std::set_intersection(
        this_nodes.begin(), this_nodes.end(),
        other_nodes.begin(), other_nodes.end(),
        std::inserter(result, result.begin())
    );

    return result;
}

bool HyperEdge::is_self_loop() const {
    std::set<std::string> src_set(sources.begin(), sources.end());
    std::set<std::string> tgt_set(targets.begin(), targets.end());
    return src_set == tgt_set;
}

nlohmann::json HyperEdge::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["sources"] = sources;
    j["relation"] = relation;
    j["targets"] = targets;
    j["confidence"] = confidence;
    j["properties"] = properties;

    if (!source_document.empty()) {
        j["source_document"] = source_document;
    }
    if (!source_chunk_id.empty()) {
        j["source_chunk_id"] = source_chunk_id;
    }
    if (source_page >= 0) {
        j["source_page"] = source_page;
    }

    return j;
}

HyperEdge HyperEdge::from_json(const nlohmann::json& j) {
    HyperEdge edge;
    edge.id = j.at("id").get<std::string>();
    edge.sources = j.at("sources").get<std::vector<std::string>>();
    edge.relation = j.at("relation").get<std::string>();
    edge.targets = j.at("targets").get<std::vector<std::string>>();
    edge.confidence = j.value("confidence", 1.0);

    if (j.contains("properties")) {
        edge.properties = j["properties"].get<std::map<std::string, std::string>>();
    }
    if (j.contains("source_document")) {
        edge.source_document = j["source_document"].get<std::string>();
    }
    if (j.contains("source_chunk_id")) {
        edge.source_chunk_id = j["source_chunk_id"].get<std::string>();
    }
    if (j.contains("source_page")) {
        edge.source_page = j["source_page"].get<int>();
    }

    return edge;
}

// ==========================================
// HypergraphStatistics Implementation
// ==========================================

nlohmann::json HypergraphStatistics::to_json() const {
    nlohmann::json j;
    j["num_nodes"] = num_nodes;
    j["num_edges"] = num_edges;
    j["num_duplicate_edges"] = num_duplicate_edges;
    j["avg_edge_size"] = avg_edge_size;
    j["max_edge_size"] = max_edge_size;
    j["min_edge_size"] = min_edge_size;
    j["avg_node_degree"] = avg_node_degree;
    j["max_node_degree"] = max_node_degree;
    j["min_node_degree"] = min_node_degree;
    j["max_edge_intersection"] = max_edge_intersection;
    j["num_pairs_overlap_1"] = num_pairs_overlap_1;
    j["num_pairs_overlap_2"] = num_pairs_overlap_2;
    j["num_pairs_overlap_3"] = num_pairs_overlap_3;

    if (power_law_exponent.has_value()) {
        j["power_law_exponent"] = power_law_exponent.value();
    }
    if (power_law_r_squared.has_value()) {
        j["power_law_r_squared"] = power_law_r_squared.value();
    }

    return j;
}

// ==========================================
// PathSearchResult Implementation
// ==========================================

nlohmann::json PathSearchResult::to_json() const {
    nlohmann::json j;
    j["found"] = found;
    j["num_paths_explored"] = num_paths_explored;
    j["num_paths_found"] = paths.size();

    nlohmann::json paths_json = nlohmann::json::array();
    for (const auto& path : paths) {
        nlohmann::json path_json = nlohmann::json::array();
        for (const auto& edge : path) {
            path_json.push_back(edge.to_json());
        }
        paths_json.push_back(path_json);
    }
    j["paths"] = paths_json;
    j["intersection_nodes"] = intersection_nodes;

    return j;
}

// ==========================================
// Hypergraph Implementation
// ==========================================

std::string Hypergraph::add_hyperedge(const HyperEdge& edge) {
    HyperEdge new_edge = edge;

    // Generate ID if not provided
    if (new_edge.id.empty()) {
        new_edge.id = generate_edge_id();
    }

    // Normalize node IDs in sources and targets for case-insensitive matching
    // This ensures "Knowledge Graph" and "knowledge graph" map to the same node
    for (auto& src : new_edge.sources) {
        std::string original_label = src;
        std::string normalized_id = normalize_node_id(src);

        // Create node if it doesn't exist, preserving original label for display
        if (!has_node(normalized_id)) {
            HyperNode node;
            node.id = normalized_id;
            node.label = original_label;  // Keep original for display
            add_node(node);
        }

        // Update edge to use normalized ID
        src = normalized_id;
    }

    for (auto& tgt : new_edge.targets) {
        std::string original_label = tgt;
        std::string normalized_id = normalize_node_id(tgt);

        // Create node if it doesn't exist, preserving original label for display
        if (!has_node(normalized_id)) {
            HyperNode node;
            node.id = normalized_id;
            node.label = original_label;  // Keep original for display
            add_node(node);
        }

        // Update edge to use normalized ID
        tgt = normalized_id;
    }

    // Add edge to storage
    hyperedges_[new_edge.id] = new_edge;

    update_indices(new_edge);

    return new_edge.id;
}

std::string Hypergraph::add_hyperedge(
    const std::vector<std::string>& sources,
    const std::string& relation,
    const std::vector<std::string>& targets,
    const std::string& source_chunk_id
) {
    HyperEdge edge;
    // Pass original labels - normalization happens in add_hyperedge(const HyperEdge&)
    edge.sources = sources;
    edge.relation = relation;
    edge.targets = targets;
    edge.source_chunk_id = source_chunk_id;

    return add_hyperedge(edge);
}

void Hypergraph::add_node(const HyperNode& node) {
    std::string normalized_id = normalize_node_id(node.id);

    if (nodes_.find(normalized_id) != nodes_.end()) {
        // Node exists, update properties and embedding but keep existing label
        // (preserves the first label seen for display)
        nodes_[normalized_id].properties = node.properties;
        nodes_[normalized_id].embedding = node.embedding;
    } else {
        // New node - use normalized ID but original label
        HyperNode new_node = node;
        new_node.id = normalized_id;
        // Keep the original label for display purposes
        nodes_[normalized_id] = new_node;
    }
}

bool Hypergraph::remove_hyperedge(const std::string& edge_id) {
    auto it = hyperedges_.find(edge_id);
    if (it == hyperedges_.end()) {
        return false;
    }

    remove_from_indices(edge_id);
    hyperedges_.erase(it);

    return true;
}

bool Hypergraph::remove_node(const std::string& node_id) {
    std::string normalized = normalize_node_id(node_id);
    auto it = nodes_.find(normalized);
    if (it == nodes_.end()) {
        return false;
    }

    // Remove all incident edges
    auto incident = get_incident_edges(normalized);
    for (const auto& edge : incident) {
        remove_hyperedge(edge.id);
    }

    nodes_.erase(it);
    node_to_edges_.erase(normalized);

    return true;
}

const HyperNode* Hypergraph::get_node(const std::string& node_id) const {
    std::string normalized = normalize_node_id(node_id);
    auto it = nodes_.find(normalized);
    return it != nodes_.end() ? &it->second : nullptr;
}

HyperNode* Hypergraph::get_node(const std::string& node_id) {
    std::string normalized = normalize_node_id(node_id);
    auto it = nodes_.find(normalized);
    return it != nodes_.end() ? &it->second : nullptr;
}

const HyperEdge* Hypergraph::get_hyperedge(const std::string& edge_id) const {
    auto it = hyperedges_.find(edge_id);
    return it != hyperedges_.end() ? &it->second : nullptr;
}

HyperEdge* Hypergraph::get_hyperedge(const std::string& edge_id) {
    auto it = hyperedges_.find(edge_id);
    return it != hyperedges_.end() ? &it->second : nullptr;
}

std::vector<HyperEdge> Hypergraph::get_incident_edges(const std::string& node_id) const {
    std::vector<HyperEdge> result;

    std::string normalized = normalize_node_id(node_id);
    auto it = node_to_edges_.find(normalized);
    if (it != node_to_edges_.end()) {
        for (const auto& edge_id : it->second) {
            auto edge_it = hyperedges_.find(edge_id);
            if (edge_it != hyperedges_.end()) {
                result.push_back(edge_it->second);
            }
        }
    }

    return result;
}

std::vector<HyperNode> Hypergraph::get_all_nodes() const {
    std::vector<HyperNode> result;
    result.reserve(nodes_.size());

    for (const auto& [id, node] : nodes_) {
        result.push_back(node);
    }

    return result;
}

std::vector<HyperEdge> Hypergraph::get_all_edges() const {
    std::vector<HyperEdge> result;
    result.reserve(hyperedges_.size());

    for (const auto& [id, edge] : hyperedges_) {
        result.push_back(edge);
    }

    return result;
}

bool Hypergraph::has_node(const std::string& node_id) const {
    std::string normalized = normalize_node_id(node_id);
    return nodes_.find(normalized) != nodes_.end();
}

bool Hypergraph::has_edge(const std::string& edge_id) const {
    return hyperedges_.find(edge_id) != hyperedges_.end();
}

// ==========================================
// Graph Operations
// ==========================================

void Hypergraph::merge_similar_nodes(double similarity_threshold) {
    if (similarity_threshold < 0.0 || similarity_threshold > 1.0) {
        throw std::invalid_argument("Similarity threshold must be in [0, 1]");
    }

    // Build similarity graph
    std::map<std::string, std::vector<std::string>> similarity_graph;

    std::vector<std::string> node_ids;
    for (const auto& [id, node] : nodes_) {
        if (!node.embedding.empty()) {
            node_ids.push_back(id);
        }
    }

    // Compute pairwise similarities
    for (size_t i = 0; i < node_ids.size(); ++i) {
        for (size_t j = i + 1; j < node_ids.size(); ++j) {
            const auto& node_i = nodes_[node_ids[i]];
            const auto& node_j = nodes_[node_ids[j]];

            double sim = cosine_similarity(node_i.embedding, node_j.embedding);

            if (sim >= similarity_threshold) {
                similarity_graph[node_ids[i]].push_back(node_ids[j]);
                similarity_graph[node_ids[j]].push_back(node_ids[i]);
            }
        }
    }

    // Find connected components (equivalence classes)
    auto components = find_similarity_components(similarity_graph);

    // Merge each component
    for (const auto& component : components) {
        if (component.empty()) continue;

        // Select representative: highest degree node
        std::string representative = component[0];
        int max_degree = get_node_degree(representative);

        for (const auto& node_id : component) {
            int degree = get_node_degree(node_id);
            if (degree > max_degree) {
                max_degree = degree;
                representative = node_id;
            }
        }

        // Merge all others into representative
        for (const auto& node_id : component) {
            if (node_id != representative) {
                merge_nodes(representative, node_id);
            }
        }
    }
}

size_t Hypergraph::remove_self_loops() {
    size_t removed = 0;

    std::vector<std::string> to_remove;
    for (const auto& [id, edge] : hyperedges_) {
        if (edge.is_self_loop()) {
            to_remove.push_back(id);
        }
    }

    for (const auto& id : to_remove) {
        if (remove_hyperedge(id)) {
            ++removed;
        }
    }

    return removed;
}

std::map<std::string, std::vector<std::string>> Hypergraph::find_duplicate_edges() const {
    std::map<std::string, std::vector<std::string>> duplicates;

    std::vector<std::string> edge_ids;
    for (const auto& [id, edge] : hyperedges_) {
        edge_ids.push_back(id);
    }

    for (size_t i = 0; i < edge_ids.size(); ++i) {
        for (size_t j = i + 1; j < edge_ids.size(); ++j) {
            const auto& edge_i = hyperedges_.at(edge_ids[i]);
            const auto& edge_j = hyperedges_.at(edge_ids[j]);

            if (are_duplicate_edges(edge_i, edge_j)) {
                duplicates[edge_ids[i]].push_back(edge_ids[j]);
            }
        }
    }

    return duplicates;
}

size_t Hypergraph::merge_duplicate_edges() {
    auto duplicates = find_duplicate_edges();
    size_t removed = 0;

    for (const auto& [canonical, dups] : duplicates) {
        for (const auto& dup_id : dups) {
            if (remove_hyperedge(dup_id)) {
                ++removed;
            }
        }
    }

    return removed;
}

int Hypergraph::get_node_degree(const std::string& node_id) const {
    std::string normalized = normalize_node_id(node_id);
    auto it = node_to_edges_.find(normalized);
    return it != node_to_edges_.end() ? static_cast<int>(it->second.size()) : 0;
}

std::map<std::string, int> Hypergraph::compute_node_degrees() const {
    std::map<std::string, int> degrees;

    for (const auto& [node_id, node] : nodes_) {
        degrees[node_id] = get_node_degree(node_id);
    }

    return degrees;
}

std::vector<std::pair<std::string, int>> Hypergraph::get_top_hubs(size_t k) const {
    auto degrees = compute_node_degrees();

    std::vector<std::pair<std::string, int>> hub_list(degrees.begin(), degrees.end());

    std::partial_sort(
        hub_list.begin(),
        hub_list.begin() + std::min(k, hub_list.size()),
        hub_list.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; }
    );

    if (hub_list.size() > k) {
        hub_list.resize(k);
    }

    return hub_list;
}

HypergraphStatistics Hypergraph::compute_statistics() const {
    HypergraphStatistics stats;

    stats.num_nodes = nodes_.size();
    stats.num_edges = hyperedges_.size();

    if (hyperedges_.empty()) {
        return stats;
    }

    // Edge size statistics
    size_t total_edge_size = 0;
    stats.max_edge_size = 0;
    stats.min_edge_size = std::numeric_limits<size_t>::max();

    for (const auto& [id, edge] : hyperedges_) {
        size_t size = edge.size();
        total_edge_size += size;
        stats.max_edge_size = std::max(stats.max_edge_size, size);
        stats.min_edge_size = std::min(stats.min_edge_size, size);
    }

    stats.avg_edge_size = static_cast<double>(total_edge_size) / hyperedges_.size();

    // Node degree statistics
    auto degrees = compute_node_degrees();
    if (!degrees.empty()) {
        size_t total_degree = 0;
        stats.max_node_degree = 0;
        stats.min_node_degree = std::numeric_limits<size_t>::max();

        for (const auto& [node_id, degree] : degrees) {
            total_degree += degree;
            stats.max_node_degree = std::max(stats.max_node_degree, static_cast<size_t>(degree));
            stats.min_node_degree = std::min(stats.min_node_degree, static_cast<size_t>(degree));
        }

        stats.avg_node_degree = static_cast<double>(total_degree) / degrees.size();
    }

    // Compute duplicate edges
    stats.num_duplicate_edges = find_duplicate_edges().size();

    // Compute pairwise overlaps
    std::vector<std::string> edge_ids;
    for (const auto& [id, edge] : hyperedges_) {
        edge_ids.push_back(id);
    }

    for (size_t i = 0; i < edge_ids.size(); ++i) {
        for (size_t j = i + 1; j < edge_ids.size(); ++j) {
            const auto& edge_i = hyperedges_.at(edge_ids[i]);
            const auto& edge_j = hyperedges_.at(edge_ids[j]);

            auto intersection = edge_i.intersection(edge_j);
            size_t overlap_size = intersection.size();

            if (overlap_size >= 1) stats.num_pairs_overlap_1++;
            if (overlap_size >= 2) stats.num_pairs_overlap_2++;
            if (overlap_size >= 3) stats.num_pairs_overlap_3++;

            stats.max_edge_intersection = std::max(
                stats.max_edge_intersection,
                overlap_size
            );
        }
    }

    // Fit power law
    auto [exponent, r_squared] = fit_power_law();
    if (r_squared > 0.5) {  // Only include if reasonable fit
        stats.power_law_exponent = exponent;
        stats.power_law_r_squared = r_squared;
    }

    return stats;
}

// ==========================================
// Path Finding (continued in next part)
// ==========================================

std::vector<HyperEdge> Hypergraph::find_shortest_path(
    const std::string& start,
    const std::string& end,
    int min_intersection_size
) const {
    return bfs_path_search(start, end, min_intersection_size);
}

PathSearchResult Hypergraph::find_k_shortest_paths(
    const std::string& start,
    const std::string& end,
    int k,
    int min_intersection_size
) const {
    PathSearchResult result;

    if (!has_node(start) || !has_node(end)) {
        return result;
    }

    // Yen's algorithm adapted for hypergraphs
    // Step 1: Find shortest path
    auto shortest = find_shortest_path(start, end, min_intersection_size);

    if (shortest.empty()) {
        return result;
    }

    result.paths.push_back(shortest);
    result.found = true;
    result.num_paths_explored = 1;

    // Priority queue of candidate paths
    std::vector<std::pair<size_t, std::vector<HyperEdge>>> candidates;

    // Step 2: Find k-1 additional paths
    for (int k_iter = 1; k_iter < k; ++k_iter) {
        const auto& prev_path = result.paths.back();

        // Generate spur paths by removing edges
        for (size_t i = 0; i < prev_path.size(); ++i) {
            // Build exclusion set
            std::set<std::string> excluded_edges;

            for (const auto& path : result.paths) {
                if (path.size() > i) {
                    excluded_edges.insert(path[i].id);
                }
            }

            // Find spur path
            auto spur_path = bfs_path_search(
                prev_path[i].targets[0],  // Simplified: use first target
                end,
                min_intersection_size,
                excluded_edges
            );

            if (!spur_path.empty()) {
                // Combine root + spur
                std::vector<HyperEdge> candidate;
                candidate.insert(candidate.end(), prev_path.begin(), prev_path.begin() + i);
                candidate.insert(candidate.end(), spur_path.begin(), spur_path.end());

                candidates.push_back({candidate.size(), candidate});
            }
        }

        if (candidates.empty()) {
            break;
        }

        // Sort by path length and take shortest
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        result.paths.push_back(candidates[0].second);
        candidates.erase(candidates.begin());
        result.num_paths_explored++;
    }

    return result;
}

// This file continues in the next part...
// Remaining implementations will be in hypergraph_part2.cpp

// ==========================================
// Helper Methods
// ==========================================

void Hypergraph::update_indices(const HyperEdge& edge) {
    for (const auto& node_id : edge.get_all_nodes()) {
        node_to_edges_[node_id].push_back(edge.id);
        if (nodes_.find(node_id) != nodes_.end()) {
            nodes_[node_id].incident_edges.push_back(edge.id);
            nodes_[node_id].degree = static_cast<int>(node_to_edges_[node_id].size());
        }
    }
}

void Hypergraph::remove_from_indices(const std::string& edge_id) {
    auto it = hyperedges_.find(edge_id);
    if (it == hyperedges_.end()) return;

    for (const auto& node_id : it->second.get_all_nodes()) {
        auto& edges = node_to_edges_[node_id];
        edges.erase(std::remove(edges.begin(), edges.end(), edge_id), edges.end());

        if (nodes_.find(node_id) != nodes_.end()) {
            auto& incident = nodes_[node_id].incident_edges;
            incident.erase(std::remove(incident.begin(), incident.end(), edge_id), incident.end());
            nodes_[node_id].degree = static_cast<int>(edges.size());
        }
    }
}

bool Hypergraph::are_duplicate_edges(const HyperEdge& e1, const HyperEdge& e2) const {
    if (e1.relation != e2.relation) return false;

    std::set<std::string> src1(e1.sources.begin(), e1.sources.end());
    std::set<std::string> src2(e2.sources.begin(), e2.sources.end());
    std::set<std::string> tgt1(e1.targets.begin(), e1.targets.end());
    std::set<std::string> tgt2(e2.targets.begin(), e2.targets.end());

    return src1 == src2 && tgt1 == tgt2;
}

std::vector<HyperEdge> Hypergraph::bfs_path_search(
    const std::string& start,
    const std::string& end,
    int min_intersection_size,
    const std::set<std::string>& excluded_edges
) const {
    // BFS on hyperedges
    std::queue<std::string> edge_queue;
    std::map<std::string, std::string> parent_edge;
    std::set<std::string> visited_edges;

    // Start from edges containing start node
    auto start_edges = get_incident_edges(start);
    for (const auto& edge : start_edges) {
        if (excluded_edges.find(edge.id) == excluded_edges.end()) {
            edge_queue.push(edge.id);
            parent_edge[edge.id] = "";
            visited_edges.insert(edge.id);
        }
    }

    std::string goal_edge_id;

    while (!edge_queue.empty() && goal_edge_id.empty()) {
        std::string current_edge_id = edge_queue.front();
        edge_queue.pop();

        const auto& current_edge = hyperedges_.at(current_edge_id);

        // Check if this edge contains the end node
        if (current_edge.contains_node(end)) {
            goal_edge_id = current_edge_id;
            break;
        }

        // Explore s-connected neighbors
        auto neighbors = get_s_connected_neighbors(current_edge_id, min_intersection_size);

        for (const auto& neighbor_id : neighbors) {
            if (visited_edges.find(neighbor_id) == visited_edges.end() &&
                excluded_edges.find(neighbor_id) == excluded_edges.end()) {

                edge_queue.push(neighbor_id);
                parent_edge[neighbor_id] = current_edge_id;
                visited_edges.insert(neighbor_id);
            }
        }
    }

    // Reconstruct path
    std::vector<HyperEdge> path;
    if (!goal_edge_id.empty()) {
        std::string current = goal_edge_id;
        while (!current.empty()) {
            path.push_back(hyperedges_.at(current));
            current = parent_edge[current];
        }
        std::reverse(path.begin(), path.end());
    }

    return path;
}

bool Hypergraph::are_s_connected(
    const HyperEdge& e1,
    const HyperEdge& e2,
    int min_intersection_size
) const {
    return e1.intersection(e2).size() >= static_cast<size_t>(min_intersection_size);
}

std::vector<std::string> Hypergraph::get_s_connected_neighbors(
    const std::string& edge_id,
    int min_intersection_size
) const {
    std::vector<std::string> neighbors;

    auto it = hyperedges_.find(edge_id);
    if (it == hyperedges_.end()) return neighbors;

    const auto& edge = it->second;

    // Check all edges that share nodes with this edge
    std::set<std::string> candidate_edges;
    for (const auto& node_id : edge.get_all_nodes()) {
        auto incident = get_incident_edges(node_id);
        for (const auto& incident_edge : incident) {
            if (incident_edge.id != edge_id) {
                candidate_edges.insert(incident_edge.id);
            }
        }
    }

    // Filter by intersection size
    for (const auto& candidate_id : candidate_edges) {
        const auto& candidate = hyperedges_.at(candidate_id);
        if (are_s_connected(edge, candidate, min_intersection_size)) {
            neighbors.push_back(candidate_id);
        }
    }

    return neighbors;
}

void Hypergraph::merge_nodes(const std::string& keep_id, const std::string& remove_id) {
    std::string normalized_keep = normalize_node_id(keep_id);
    std::string normalized_remove = normalize_node_id(remove_id);

    if (!has_node(normalized_keep) || !has_node(normalized_remove)) return;

    // Transfer incident edges
    auto incident = get_incident_edges(normalized_remove);
    for (const auto& edge : incident) {
        auto* mutable_edge = get_hyperedge(edge.id);
        if (mutable_edge) {
            // Replace remove_id with keep_id in sources
            for (auto& src : mutable_edge->sources) {
                if (src == normalized_remove) src = normalized_keep;
            }
            // Replace in targets
            for (auto& tgt : mutable_edge->targets) {
                if (tgt == normalized_remove) tgt = normalized_keep;
            }
        }
    }

    // Remove the node
    remove_node(normalized_remove);
}

std::vector<std::vector<std::string>> Hypergraph::find_similarity_components(
    const std::map<std::string, std::vector<std::string>>& similarity_graph
) const {
    std::set<std::string> visited;
    std::vector<std::vector<std::string>> components;

    for (const auto& [node_id, neighbors] : similarity_graph) {
        if (visited.find(node_id) != visited.end()) continue;

        // DFS to find connected component
        std::vector<std::string> component;
        std::stack<std::string> stack;
        stack.push(node_id);

        while (!stack.empty()) {
            std::string current = stack.top();
            stack.pop();

            if (visited.find(current) != visited.end()) continue;

            visited.insert(current);
            component.push_back(current);

            auto it = similarity_graph.find(current);
            if (it != similarity_graph.end()) {
                for (const auto& neighbor : it->second) {
                    if (visited.find(neighbor) == visited.end()) {
                        stack.push(neighbor);
                    }
                }
            }
        }

        if (!component.empty()) {
            components.push_back(component);
        }
    }

    return components;
}

std::string Hypergraph::generate_edge_id() {
    return "edge_" + std::to_string(edge_id_counter_++);
}

double Hypergraph::cosine_similarity(
    const std::vector<float>& vec1,
    const std::vector<float>& vec2
) {
    if (vec1.size() != vec2.size() || vec1.empty()) {
        return 0.0;
    }

    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (size_t i = 0; i < vec1.size(); ++i) {
        dot_product += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dot_product / (std::sqrt(norm1) * std::sqrt(norm2));
}

// Helper: check if string is pure ASCII alphabetic
static bool is_ascii_alpha_word(const std::string& text) {
    if (text.empty()) return false;
    for (unsigned char c : text) {
        if (c >= 128 || !std::isalpha(c)) return false;
    }
    return true;
}

// Helper: singularize a single ASCII word (basic English rules)
static std::string singularize_word(const std::string& word) {
    if (word.size() <= 3) return word;
    if (!is_ascii_alpha_word(word)) return word;

    // "ies" -> "y" (e.g., "studies" -> "study")
    if (word.size() >= 4 && word.compare(word.size() - 3, 3, "ies") == 0) {
        return word.substr(0, word.size() - 3) + "y";
    }
    // "ches", "shes", "xes", "ses", "zes" -> remove "es"
    if (word.size() >= 4 &&
        (word.compare(word.size() - 4, 4, "ches") == 0 ||
         word.compare(word.size() - 4, 4, "shes") == 0 ||
         word.compare(word.size() - 3, 3, "xes") == 0 ||
         word.compare(word.size() - 3, 3, "ses") == 0 ||
         word.compare(word.size() - 3, 3, "zes") == 0)) {
        return word.substr(0, word.size() - 2);
    }
    // Don't singularize words ending in "ss" (e.g., "class", "process")
    if (word.size() >= 2 && word.compare(word.size() - 2, 2, "ss") == 0) {
        return word;
    }
    // Remove trailing "s"
    if (!word.empty() && word.back() == 's') {
        return word.substr(0, word.size() - 1);
    }
    return word;
}

std::string Hypergraph::normalize_node_id(const std::string& label) {
    std::string result;
    result.reserve(label.size());

    // Trim leading whitespace
    size_t start = 0;
    while (start < label.size() && std::isspace(static_cast<unsigned char>(label[start]))) {
        ++start;
    }

    // Trim trailing whitespace
    size_t end = label.size();
    while (end > start && std::isspace(static_cast<unsigned char>(label[end - 1]))) {
        --end;
    }

    // Convert to lowercase
    for (size_t i = start; i < end; ++i) {
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(label[i])));
    }

    // Singularize: for multi-word entities, singularize the last word
    // For single-word entities, singularize the whole word
    // e.g., "knowledge graphs" -> "knowledge graph"
    //       "houses" -> "house"
    size_t last_space = result.rfind(' ');
    if (last_space != std::string::npos) {
        // Multi-word: singularize last word only
        std::string prefix = result.substr(0, last_space + 1);
        std::string last_word = result.substr(last_space + 1);
        result = prefix + singularize_word(last_word);
    } else {
        // Single word: singularize the whole thing
        result = singularize_word(result);
    }

    return result;
}

void Hypergraph::clear() {
    nodes_.clear();
    hyperedges_.clear();
    node_to_edges_.clear();
}

} // namespace kg

#include "graph/hypergraph.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <queue>
#include <iomanip>

namespace kg {

// ==========================================
// Export/Import Methods
// ==========================================

nlohmann::json Hypergraph::to_json(bool include_metadata) const {
    nlohmann::json j;

    // Export nodes
    nlohmann::json nodes_json = nlohmann::json::array();
    for (const auto& [id, node] : nodes_) {
        nodes_json.push_back(node.to_json());
    }
    j["nodes"] = nodes_json;

    // Export hyperedges
    nlohmann::json edges_json = nlohmann::json::array();
    for (const auto& [id, edge] : hyperedges_) {
        auto edge_json = edge.to_json();
        if (!include_metadata) {
            edge_json.erase("source_document");
            edge_json.erase("source_chunk_id");
            edge_json.erase("source_page");
            edge_json.erase("properties");
        }
        edges_json.push_back(edge_json);
    }
    j["hyperedges"] = edges_json;

    // Add statistics
    j["metadata"] = {
        {"num_nodes", nodes_.size()},
        {"num_edges", hyperedges_.size()}
    };

    return j;
}

void Hypergraph::export_to_json(const std::string& filename, bool include_metadata) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    auto j = to_json(include_metadata);
    file << j.dump(2);  // Pretty print with 2-space indentation
    file.close();
}

void Hypergraph::export_to_dot(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    file << "digraph Hypergraph {\n";
    file << "  rankdir=LR;\n";
    file << "  node [shape=ellipse, style=filled, color=lightblue];\n\n";

    // Write nodes
    for (const auto& [id, node] : nodes_) {
        file << "  \"" << id << "\" [label=\"" << node.label << "\"];\n";
    }

    file << "\n";

    // Write hyperedges as relation nodes
    int rel_counter = 0;
    for (const auto& [id, edge] : hyperedges_) {
        std::string rel_node_id = "rel_" + std::to_string(rel_counter++);

        // Create relation node (diamond)
        file << "  " << rel_node_id
             << " [shape=diamond, style=filled, color=orange, label=\""
             << edge.relation << "\"];\n";

        // Edges from sources to relation
        for (const auto& src : edge.sources) {
            file << "  \"" << src << "\" -> " << rel_node_id << ";\n";
        }

        // Edges from relation to targets
        for (const auto& tgt : edge.targets) {
            file << "  " << rel_node_id << " -> \"" << tgt << "\";\n";
        }

        file << "\n";
    }

    file << "}\n";
    file.close();
}

nlohmann::json Hypergraph::to_incidence_matrix() const {
    nlohmann::json j;

    // Create ordered lists
    std::vector<std::string> node_list;
    for (const auto& [id, node] : nodes_) {
        node_list.push_back(id);
    }
    std::sort(node_list.begin(), node_list.end());

    std::vector<std::string> edge_list;
    for (const auto& [id, edge] : hyperedges_) {
        edge_list.push_back(id);
    }
    std::sort(edge_list.begin(), edge_list.end());

    // Build matrix
    std::vector<std::vector<int>> matrix(node_list.size(),
                                         std::vector<int>(edge_list.size(), 0));

    for (size_t j_idx = 0; j_idx < edge_list.size(); ++j_idx) {
        const auto& edge = hyperedges_.at(edge_list[j_idx]);
        auto all_nodes = edge.get_all_nodes();

        for (size_t i_idx = 0; i_idx < node_list.size(); ++i_idx) {
            if (all_nodes.find(node_list[i_idx]) != all_nodes.end()) {
                matrix[i_idx][j_idx] = 1;
            }
        }
    }

    j["nodes"] = node_list;
    j["edges"] = edge_list;
    j["matrix"] = matrix;

    return j;
}

Hypergraph Hypergraph::from_json(const nlohmann::json& j) {
    Hypergraph graph;

    // Load nodes
    if (j.contains("nodes")) {
        for (const auto& node_json : j["nodes"]) {
            auto node = HyperNode::from_json(node_json);
            graph.add_node(node);
        }
    }

    // Load hyperedges
    if (j.contains("hyperedges")) {
        for (const auto& edge_json : j["hyperedges"]) {
            auto edge = HyperEdge::from_json(edge_json);
            graph.add_hyperedge(edge);
        }
    }

    return graph;
}

Hypergraph Hypergraph::load_from_json(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }

    nlohmann::json j;
    file >> j;
    file.close();

    return from_json(j);
}

// ==========================================
// Advanced Graph Operations
// ==========================================

std::vector<std::set<std::string>> Hypergraph::find_s_connected_components(
    int min_intersection_size
) const {
    std::vector<std::set<std::string>> components;
    std::set<std::string> visited;

    for (const auto& [edge_id, edge] : hyperedges_) {
        if (visited.find(edge_id) != visited.end()) continue;

        // BFS/DFS to find component
        std::set<std::string> component;
        std::queue<std::string> queue;
        queue.push(edge_id);

        while (!queue.empty()) {
            std::string current_id = queue.front();
            queue.pop();

            if (visited.find(current_id) != visited.end()) continue;

            visited.insert(current_id);
            component.insert(current_id);

            // Find s-connected neighbors
            auto neighbors = get_s_connected_neighbors(current_id, min_intersection_size);
            for (const auto& neighbor_id : neighbors) {
                if (visited.find(neighbor_id) == visited.end()) {
                    queue.push(neighbor_id);
                }
            }
        }

        if (!component.empty()) {
            components.push_back(component);
        }
    }

    // Sort by size (largest first)
    std::sort(components.begin(), components.end(),
              [](const auto& a, const auto& b) { return a.size() > b.size(); });

    return components;
}

std::set<std::string> Hypergraph::get_neighborhood(
    const std::string& node_id,
    int hops,
    int min_intersection_size
) const {
    if (!has_node(node_id) || hops < 0) {
        return {};
    }

    std::set<std::string> neighborhood;
    std::set<std::string> current_level = {node_id};
    std::set<std::string> visited_nodes = {node_id};

    for (int h = 0; h < hops; ++h) {
        std::set<std::string> next_level;

        for (const auto& current_node : current_level) {
            // Get incident edges
            auto incident = get_incident_edges(current_node);

            for (const auto& edge : incident) {
                // Add all nodes from this edge
                auto nodes = edge.get_all_nodes();
                for (const auto& n : nodes) {
                    if (visited_nodes.find(n) == visited_nodes.end()) {
                        next_level.insert(n);
                        visited_nodes.insert(n);
                        neighborhood.insert(n);
                    }
                }
            }
        }

        current_level = next_level;
    }

    return neighborhood;
}

Hypergraph Hypergraph::extract_subgraph(const std::set<std::string>& node_ids) const {
    Hypergraph subgraph;

    // Add nodes
    for (const auto& node_id : node_ids) {
        auto* node = get_node(node_id);
        if (node) {
            subgraph.add_node(*node);
        }
    }

    // Add hyperedges where all nodes are in the set
    for (const auto& [id, edge] : hyperedges_) {
        auto edge_nodes = edge.get_all_nodes();

        bool all_included = true;
        for (const auto& n : edge_nodes) {
            if (node_ids.find(n) == node_ids.end()) {
                all_included = false;
                break;
            }
        }

        if (all_included) {
            subgraph.add_hyperedge(edge);
        }
    }

    return subgraph;
}

// ==========================================
// Analysis Methods
// ==========================================

std::map<int, int> Hypergraph::compute_degree_distribution() const {
    std::map<int, int> distribution;

    auto degrees = compute_node_degrees();
    for (const auto& [node_id, degree] : degrees) {
        distribution[degree]++;
    }

    return distribution;
}

std::pair<double, double> Hypergraph::fit_power_law() const {
    auto distribution = compute_degree_distribution();

    if (distribution.size() < 3) {
        return {0.0, 0.0};
    }

    // Log-log linear regression
    std::vector<double> log_k;
    std::vector<double> log_freq;

    for (const auto& [degree, frequency] : distribution) {
        if (degree > 0 && frequency > 0) {
            log_k.push_back(std::log(static_cast<double>(degree)));
            log_freq.push_back(std::log(static_cast<double>(frequency)));
        }
    }

    size_t n = log_k.size();
    if (n < 2) {
        return {0.0, 0.0};
    }

    // Calculate means
    double mean_x = std::accumulate(log_k.begin(), log_k.end(), 0.0) / n;
    double mean_y = std::accumulate(log_freq.begin(), log_freq.end(), 0.0) / n;

    // Calculate slope and intercept
    double numerator = 0.0;
    double denominator = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double dx = log_k[i] - mean_x;
        double dy = log_freq[i] - mean_y;
        numerator += dx * dy;
        denominator += dx * dx;
    }

    double slope = numerator / denominator;

    // Calculate R²
    double ss_tot = 0.0;
    double ss_res = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double y_pred = slope * (log_k[i] - mean_x) + mean_y;
        ss_tot += (log_freq[i] - mean_y) * (log_freq[i] - mean_y);
        ss_res += (log_freq[i] - y_pred) * (log_freq[i] - y_pred);
    }

    double r_squared = 1.0 - (ss_res / ss_tot);

    // Return exponent (negative of slope) and R²
    return {-slope, r_squared};
}

double Hypergraph::compute_rich_club_coefficient(int degree_threshold) const {
    // Get nodes with degree >= threshold
    std::set<std::string> rich_nodes;

    for (const auto& [node_id, node] : nodes_) {
        if (get_node_degree(node_id) >= degree_threshold) {
            rich_nodes.insert(node_id);
        }
    }

    if (rich_nodes.size() < 2) {
        return 0.0;
    }

    // Count edges among rich nodes
    size_t edges_among_rich = 0;
    size_t total_edges_with_rich = 0;

    for (const auto& [id, edge] : hyperedges_) {
        auto edge_nodes = edge.get_all_nodes();

        // Count how many rich nodes are in this edge
        size_t rich_count = 0;
        for (const auto& n : edge_nodes) {
            if (rich_nodes.find(n) != rich_nodes.end()) {
                rich_count++;
            }
        }

        if (rich_count >= 2) {
            edges_among_rich++;
        }
        if (rich_count >= 1) {
            total_edges_with_rich++;
        }
    }

    if (total_edges_with_rich == 0) {
        return 0.0;
    }

    return static_cast<double>(edges_among_rich) / total_edges_with_rich;
}

std::map<std::string, int> Hypergraph::compute_hub_integration_scores(int top_k_hubs) const {
    auto hubs = get_top_hubs(top_k_hubs);
    std::set<std::string> hub_ids;

    for (const auto& [id, degree] : hubs) {
        hub_ids.insert(id);
    }

    std::map<std::string, int> integration_scores;

    for (const auto& hub_id : hub_ids) {
        int score = 0;

        // Count co-occurrences with other hubs
        auto incident = get_incident_edges(hub_id);

        for (const auto& edge : incident) {
            auto nodes = edge.get_all_nodes();

            for (const auto& node : nodes) {
                if (node != hub_id && hub_ids.find(node) != hub_ids.end()) {
                    score++;
                }
            }
        }

        integration_scores[hub_id] = score;
    }

    return integration_scores;
}

// ==========================================
// Merge Operations
// ==========================================

void Hypergraph::merge(const Hypergraph& other, bool deduplicate) {
    // Merge nodes
    for (const auto& [id, node] : other.nodes_) {
        if (!has_node(id)) {
            add_node(node);
        } else {
            // Node exists, merge properties
            auto* existing = get_node(id);
            if (existing) {
                // Merge properties (prefer existing)
                for (const auto& [key, value] : node.properties) {
                    if (existing->properties.find(key) == existing->properties.end()) {
                        existing->properties[key] = value;
                    }
                }
            }
        }
    }

    // Merge hyperedges
    for (const auto& [id, edge] : other.hyperedges_) {
        // Check if duplicate
        bool is_duplicate = false;

        if (deduplicate) {
            for (const auto& [existing_id, existing_edge] : hyperedges_) {
                if (are_duplicate_edges(edge, existing_edge)) {
                    is_duplicate = true;
                    break;
                }
            }
        }

        if (!is_duplicate) {
            add_hyperedge(edge);
        }
    }

    if (deduplicate) {
        // Clean up duplicates that might have been introduced
        merge_duplicate_edges();
        remove_self_loops();
    }
}

void Hypergraph::export_to_html(const std::string& filename,
                                 const std::string& title) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Collect statistics for display
    auto stats = compute_statistics();

    // Calculate optimal minimum degree for ~1000 hyperedges max
    const int MAX_INITIAL_EDGES = 1000;
    int optimal_min_degree = 1;
    int max_degree = 1;

    if (static_cast<int>(hyperedges_.size()) > MAX_INITIAL_EDGES) {
        // Build degree distribution
        std::map<int, std::set<std::string>> nodes_by_degree;
        for (const auto& [id, node] : nodes_) {
            nodes_by_degree[node.degree].insert(id);
            if (node.degree > max_degree) max_degree = node.degree;
        }

        // Try increasing min_degree until we get <= MAX_INITIAL_EDGES hyperedges
        for (int test_degree = 1; test_degree <= max_degree; ++test_degree) {
            // Collect nodes with degree >= test_degree
            std::set<std::string> visible_nodes;
            for (int d = test_degree; d <= max_degree; ++d) {
                if (nodes_by_degree.count(d)) {
                    visible_nodes.insert(nodes_by_degree[d].begin(), nodes_by_degree[d].end());
                }
            }

            // Count hyperedges that connect to at least one visible node
            int visible_edges = 0;
            for (const auto& [id, edge] : hyperedges_) {
                bool has_visible = false;
                for (const auto& src : edge.sources) {
                    if (visible_nodes.count(src)) { has_visible = true; break; }
                }
                if (!has_visible) {
                    for (const auto& tgt : edge.targets) {
                        if (visible_nodes.count(tgt)) { has_visible = true; break; }
                    }
                }
                if (has_visible) visible_edges++;
            }

            if (visible_edges <= MAX_INITIAL_EDGES) {
                optimal_min_degree = test_degree;
                break;
            }
            optimal_min_degree = test_degree;
        }
    }

    // Build JSON data for D3.js
    nlohmann::json nodes_json = nlohmann::json::array();
    nlohmann::json edges_json = nlohmann::json::array();
    nlohmann::json links_json = nlohmann::json::array();

    // Add entity nodes
    std::map<std::string, int> node_index;
    int idx = 0;
    for (const auto& [id, node] : nodes_) {
        nlohmann::json n;
        n["id"] = id;
        n["label"] = node.label;
        n["type"] = "entity";
        n["degree"] = node.degree;
        nodes_json.push_back(n);
        node_index[id] = idx++;
    }

    // Add hyperedge nodes (relation nodes) and links
    int edge_idx = 0;
    for (const auto& [id, edge] : hyperedges_) {
        std::string edge_node_id = "edge_" + std::to_string(edge_idx);

        // Add edge as a node
        nlohmann::json en;
        en["id"] = edge_node_id;
        en["label"] = edge.relation;
        en["type"] = "relation";
        en["confidence"] = edge.confidence;
        en["sources"] = edge.sources;
        en["targets"] = edge.targets;
        nodes_json.push_back(en);
        int edge_node_idx = idx++;

        // Add links from sources to relation
        for (const auto& src : edge.sources) {
            if (node_index.find(src) != node_index.end()) {
                nlohmann::json link;
                link["source"] = node_index[src];
                link["target"] = edge_node_idx;
                link["type"] = "source";
                links_json.push_back(link);
            }
        }

        // Add links from relation to targets
        for (const auto& tgt : edge.targets) {
            if (node_index.find(tgt) != node_index.end()) {
                nlohmann::json link;
                link["source"] = edge_node_idx;
                link["target"] = node_index[tgt];
                link["type"] = "target";
                links_json.push_back(link);
            }
        }

        edge_idx++;
    }

    // Write HTML file with optimized Canvas renderer
    file << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << title << R"(</title>
    <script src="https://d3js.org/d3.v7.min.js"></script>
    <script src="https://unpkg.com/three@0.160.0/build/three.min.js"></script>
    <script src="https://unpkg.com/three-spritetext@1.9.3/dist/three-spritetext.min.js"></script>
    <script src="https://unpkg.com/3d-force-graph@1.75.0/dist/3d-force-graph.min.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #eee;
            overflow: hidden;
        }
        #header {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            padding: 15px 25px;
            background: rgba(0, 0, 0, 0.4);
            backdrop-filter: blur(10px);
            z-index: 100;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        #header h1 {
            font-size: 1.5em;
            font-weight: 500;
        }
        #stats {
            font-size: 0.9em;
            opacity: 0.8;
        }
        #graph {
            width: 100vw;
            height: 100vh;
        }
        #controls {
            position: fixed;
            bottom: 20px;
            left: 20px;
            background: rgba(0, 0, 0, 0.6);
            padding: 15px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
        }
        #controls label {
            display: block;
            margin-bottom: 8px;
            font-size: 0.85em;
        }
        #controls input[type="range"] {
            width: 150px;
        }
        #tooltip {
            position: absolute;
            background: rgba(0, 0, 0, 0.85);
            padding: 12px 16px;
            border-radius: 8px;
            pointer-events: none;
            opacity: 0;
            transition: opacity 0.2s;
            max-width: 300px;
            font-size: 0.9em;
            z-index: 1000;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        #tooltip h3 {
            margin-bottom: 6px;
            color: #4fc3f7;
        }
        #tooltip .detail {
            color: #aaa;
            margin: 3px 0;
        }
        #legend {
            position: fixed;
            top: 80px;
            right: 20px;
            background: rgba(0, 0, 0, 0.6);
            padding: 15px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
            font-size: 0.85em;
        }
        #legend h3 {
            margin-bottom: 10px;
            font-size: 1em;
        }
        .legend-item {
            display: flex;
            align-items: center;
            margin: 6px 0;
        }
        .legend-color {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            margin-right: 10px;
        }
        /* --- Performance UI additions (Canvas optimization) --- */
        #graph canvas {
            width: 100%;
            height: 100%;
            display: block;
        }
        .kg-row {
            display: flex;
            gap: 8px;
            align-items: center;
            margin-top: 10px;
        }
        .kg-input {
            width: 220px;
            padding: 8px 10px;
            border-radius: 8px;
            border: 1px solid rgba(255,255,255,0.15);
            background: rgba(255,255,255,0.06);
            color: #eee;
            outline: none;
        }
        .kg-input::placeholder { color: rgba(255,255,255,0.55); }
        .kg-btn {
            padding: 8px 10px;
            border-radius: 8px;
            border: 1px solid rgba(255,255,255,0.18);
            background: rgba(255,255,255,0.08);
            color: #eee;
            cursor: pointer;
            user-select: none;
        }
        .kg-btn:hover { background: rgba(255,255,255,0.12); }
        .kg-btn:active { transform: translateY(1px); }
        #kgHud {
            position: fixed;
            top: 70px;
            left: 20px;
            padding: 10px 12px;
            border-radius: 10px;
            background: rgba(0,0,0,0.45);
            backdrop-filter: blur(10px);
            font-size: 12px;
            color: rgba(255,255,255,0.85);
            z-index: 120;
            pointer-events: none;
        }
        #kgOverlay {
            position: fixed;
            inset: 0;
            display: flex;
            align-items: center;
            justify-content: center;
            background: rgba(0,0,0,0.35);
            backdrop-filter: blur(6px);
            z-index: 2000;
            opacity: 0;
            pointer-events: none;
            transition: opacity 0.2s;
        }
        #kgOverlay.show {
            opacity: 1;
            pointer-events: all;
        }
        #kgOverlay .panel {
            background: rgba(0,0,0,0.75);
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 14px;
            padding: 16px 18px;
            width: min(520px, 92vw);
            color: #eee;
            box-shadow: 0 10px 30px rgba(0,0,0,0.35);
        }
        #kgOverlay .title {
            font-size: 14px;
            color: #4fc3f7;
            margin-bottom: 8px;
        }
        #kgOverlay .sub { font-size: 12px; color: rgba(255,255,255,0.75); }
    </style>
</head>
<body>
    <div id="header">
        <h1>)" << title << R"(</h1>
        <div id="stats">
            Entities: )" << stats.num_nodes << R"( |
            Relations: )" << stats.num_edges << R"( |
            Avg Degree: )" << std::fixed << std::setprecision(2) << stats.avg_node_degree << R"(
        </div>
    </div>

    <div id="graph"></div>

    <div id="controls">
        <label>
            Min Entity Degree: <span id="degreeValue">)" << optimal_min_degree << R"(</span>
            <input type="range" id="minDegree" min="1" max=")" << std::max(20, max_degree) << R"(" step="1" value=")" << optimal_min_degree << R"(">
        </label>
        <label>
            Link Strength:
            <input type="range" id="linkStrength" min="0.1" max="2" step="0.1" value="0.5">
        </label>
        <label>
            Node Repulsion:
            <input type="range" id="repulsion" min="100" max="1000" step="50" value="400">
        </label>
        <label>
            <input type="checkbox" id="showLabels" checked> Show Labels
        </label>
        <div id="filterStats" style="margin-top: 10px; font-size: 0.8em; color: #aaa;">
            Showing: <span id="visibleNodes">0</span> nodes, <span id="visibleLinks">0</span> links
        </div>
    </div>

    <div id="legend">
        <h3>Legend</h3>
        <div class="legend-item">
            <div class="legend-color" style="background: #4fc3f7;"></div>
            <span>Entity Node</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #ff9800;"></div>
            <span>Relation (Hyperedge)</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #4fc3f7; width: 30px; height: 3px;"></div>
            <span>Source Link</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #ff9800; width: 30px; height: 3px;"></div>
            <span>Target Link</span>
        </div>
    </div>

    <div id="tooltip"></div>

    <script>
        const data = {
            nodes: )" << nodes_json.dump() << R"(,
            links: )" << links_json.dump() << R"(
        };

        // --- 3D Renderer (WebGL): ForceGraph3D + filtered subgraph + clustering ---
        // Interaction: left-drag rotates (built-in), right-drag pans, wheel zooms.

        (() => {
          const graphDiv = document.getElementById('graph');
          const controls = document.getElementById('controls');

          // Safety: ensure libs loaded
          if (typeof ForceGraph3D !== 'function' || typeof THREE === 'undefined') {
            const msg = 'Missing WebGL libs. Serve this file via http://localhost and ensure CDN access.';
            console.error(msg);
            alert(msg);
            return;
          }

          // --- small helpers ---
          const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
          const rafThrottle = (fn) => {
            let scheduled = false;
            return (...args) => {
              if (scheduled) return;
              scheduled = true;
              requestAnimationFrame(() => {
                scheduled = false;
                fn(...args);
              });
            };
          };
          const ric = window.requestIdleCallback || ((cb) => setTimeout(() => cb({ timeRemaining: () => 0, didTimeout: true }), 16));

          // --- Overlay + HUD (create if missing) ---
          let overlay = document.getElementById('kgOverlay');
          if (!overlay) {
            overlay = document.createElement('div');
            overlay.id = 'kgOverlay';
            overlay.innerHTML = '<div class="panel"><div class="title" id="kgOverlayTitle">Working...</div><div class="sub" id="kgOverlaySub">Building subgraph</div></div>';
            document.body.appendChild(overlay);
          }
          const overlayTitle = overlay.querySelector('#kgOverlayTitle') || overlay.querySelector('.title');
          const overlaySub = overlay.querySelector('#kgOverlaySub') || overlay.querySelector('.sub');

          function showOverlay(title, sub) {
            if (overlayTitle) overlayTitle.textContent = title || 'Working...';
            if (overlaySub) overlaySub.textContent = sub || '';
            overlay.classList.add('show');
          }
          function hideOverlay() { overlay.classList.remove('show'); }

          let hud = document.getElementById('kgHud');
          if (!hud) {
            hud = document.createElement('div');
            hud.id = 'kgHud';
            hud.textContent = '3D: initializing...';
            document.body.appendChild(hud);
          }

          // --- Inject extra controls ---
          if (!document.getElementById('kgSearch')) {
            controls.insertAdjacentHTML('beforeend', `
              <div class="kg-row">
                <input class="kg-input" id="kgSearch" placeholder="Search node (press /)..." />
                <button class="kg-btn" id="kgGo">Go</button>
              </div>
              <div class="kg-row">
                <button class="kg-btn" id="kgExpand1">+ 1 hop</button>
                <button class="kg-btn" id="kgExpand2">+ 2 hops</button>
                <button class="kg-btn" id="kgReset">Reset</button>
                <button class="kg-btn" id="kgCamReset">Reset camera</button>
              </div>
              <label style="margin-top:10px;">
                <input type="checkbox" id="kgAutoFreeze" checked> Auto-freeze physics
              </label>
              <label>
                <input type="checkbox" id="kgArrows"> Directional arrows
              </label>
              <label>
                Max nodes: <span id="kgMaxNodesVal">2500</span>
                <input type="range" id="kgMaxNodes" min="200" max="8000" step="100" value="2500">
              </label>
              <label>
                Depth spread: <span id="kgDepthVal">120</span>
                <input type="range" id="kgDepth" min="0" max="600" step="10" value="120">
              </label>

              <div style="border-top:1px solid #444; margin-top:12px; padding-top:10px;">
                <label style="font-weight:600; color:#4fc3f7;">Clustering</label>
                <label style="margin-top:6px;">
                  <input type="checkbox" id="kgClusterOn" checked> Cluster coloring
                </label>
                <label>
                  Clustering mode:
                  <select id="kgClusterMode" class="kg-input" style="width: 180px;">
                    <option value="topology" selected>Topology (Louvain)</option>
                    <option value="spatial">Spatial (DBSCAN)</option>
                  </select>
                </label>
                <label id="kgTopoResRow">
                  Resolution: <span id="kgTopoResVal">1.00</span>
                  <input type="range" id="kgTopoRes" min="0.20" max="3.00" step="0.05" value="1.00">
                </label>
                <label id="kgSpatialRadiusRow" style="display:none;">
                  Cluster radius: <span id="kgClusterRadiusVal">40</span>
                  <input type="range" id="kgClusterRadius" min="10" max="200" step="2" value="40">
                </label>
                <label id="kgSpatialMinRow" style="display:none;">
                  Min cluster size: <span id="kgMinClusterVal">12</span>
                  <input type="range" id="kgMinCluster" min="3" max="200" step="1" value="12">
                </label>
                <label>
                  Palette:
                  <select id="kgPalette" class="kg-input" style="width: 180px;">
                    <option value="classic" selected>Classic</option>
                    <option value="pastel">Pastel</option>
                    <option value="neon">Neon</option>
                    <option value="mono">Mono + accent</option>
                  </select>
                </label>
                <div class="kg-row">
                  <button class="kg-btn" id="kgRecluster">Cluster now</button>
                  <label style="margin:0;">
                    <input type="checkbox" id="kgAutoCluster" checked> Auto (on freeze)
                  </label>
                </div>
              </div>

              <div id="kgDetails" style="margin-top:10px; font-size:0.82em; color:#cfcfcf; line-height:1.35;"></div>
            `);
          }

          const minDegreeEl = document.getElementById('minDegree');
          const degreeValueEl = document.getElementById('degreeValue');
          const linkStrengthEl = document.getElementById('linkStrength');
          const repulsionEl = document.getElementById('repulsion');
          const showLabelsEl = document.getElementById('showLabels');

          const searchEl = document.getElementById('kgSearch');
          const goBtn = document.getElementById('kgGo');
          const exp1Btn = document.getElementById('kgExpand1');
          const exp2Btn = document.getElementById('kgExpand2');
          const resetBtn = document.getElementById('kgReset');
          const camResetBtn = document.getElementById('kgCamReset');
          const autoFreezeEl = document.getElementById('kgAutoFreeze');
          const arrowsEl = document.getElementById('kgArrows');
          const maxNodesEl = document.getElementById('kgMaxNodes');
          const maxNodesValEl = document.getElementById('kgMaxNodesVal');
          const depthEl = document.getElementById('kgDepth');
          const depthValEl = document.getElementById('kgDepthVal');
          const detailsEl = document.getElementById('kgDetails');

          // Clustering controls
          const clusterOnEl = document.getElementById('kgClusterOn');
          const clusterModeEl = document.getElementById('kgClusterMode');
          const clusterRadiusEl = document.getElementById('kgClusterRadius');
          const clusterRadiusValEl = document.getElementById('kgClusterRadiusVal');
          const minClusterEl = document.getElementById('kgMinCluster');
          const minClusterValEl = document.getElementById('kgMinClusterVal');
          const topoResEl = document.getElementById('kgTopoRes');
          const topoResValEl = document.getElementById('kgTopoResVal');
          const paletteEl = document.getElementById('kgPalette');
          const reclusterBtn = document.getElementById('kgRecluster');
          const autoClusterEl = document.getElementById('kgAutoCluster');
          const topoResRow = document.getElementById('kgTopoResRow');
          const spatialRadiusRow = document.getElementById('kgSpatialRadiusRow');
          const spatialMinRow = document.getElementById('kgSpatialMinRow');

          // ---- Index nodes/edges ----
          showOverlay('Indexing nodes & edges...', 'Preparing 3D graph');

          const N = data.nodes.length;
          for (let i = 0; i < N; i++) {
            const n = data.nodes[i];
            n.gid = i;
            if (typeof n.x !== 'number') n.x = (window.innerWidth / 2) + (Math.random() - 0.5) * 40;
            if (typeof n.y !== 'number') n.y = (window.innerHeight / 2) + (Math.random() - 0.5) * 40;
          }

          const edges = new Array(data.links.length);
          const incident = Array.from({ length: N }, () => []);
          for (let i = 0; i < data.links.length; i++) {
            const l = data.links[i];
            const s = +((typeof l.source === 'object') ? l.source.index : l.source);
            const t = +((typeof l.target === 'object') ? l.target.index : l.target);
            const e = { s, t, type: l.type };
            edges[i] = e;
            incident[s].push(i);
            incident[t].push(i);
          }

          const entityByDegree = [];
          for (let i = 0; i < N; i++) if (data.nodes[i].type === 'entity') entityByDegree.push(i);
          entityByDegree.sort((a, b) => ((data.nodes[b].degree || 0) - (data.nodes[a].degree || 0)));

          const labelsLower = data.nodes.map(n => (n.label || n.id || '').toLowerCase());

          // ---- State ----
          const state = {
            minDegree: +minDegreeEl.value,
            linkStrength: +linkStrengthEl.value,
            repulsion: +repulsionEl.value,
            showLabels: !!showLabelsEl.checked,
            autoFreeze: true,
            showArrows: false,
            maxNodes: +maxNodesEl.value,
            depthSpread: +depthEl.value,
            selectedGid: null,
            visible: new Set(),
            subNodes: [],
            subLinks: [],
            subNodesByGid: new Map(),
            graph: null,
            fps: { t0: performance.now(), frames: 0, value: 0 },
            // Clustering state
            clusterOn: true,
            clusterMode: 'topology',
            clusterRadius: 40,
            minClusterSize: 12,
            topoResolution: 1.0,
            palette: 'classic',
            autoCluster: true,
            clusters: [],
            clusterLabels: []
          };

          // Apply depth spread (z assignment)
          function applyDepth() {
            const spread = state.depthSpread;
            for (let i = 0; i < N; i++) {
              const n = data.nodes[i];
              const base = (n.type === 'relation') ? 0.45 : 1.0;
              n.z = (Math.random() - 0.5) * spread * base;
            }
          }

          // ---- Visible subgraph ----
          function seedVisibleByDegree(minDegree, maxNodes) {
            const vis = new Set();

            for (let k = 0; k < entityByDegree.length && vis.size < maxNodes; k++) {
              const idx = entityByDegree[k];
              const n = data.nodes[idx];
              if ((n.degree || 0) >= minDegree) vis.add(idx);
              else break;
            }

            if (vis.size < 50) {
              for (let k = 0; k < Math.min(200, entityByDegree.length) && vis.size < Math.min(maxNodes, 200); k++) {
                vis.add(entityByDegree[k]);
              }
            }

            const relQueue = [];
            for (const idx of vis) {
              for (const ei of incident[idx]) {
                const e = edges[ei];
                const other = (e.s === idx) ? e.t : e.s;
                if (data.nodes[other].type === 'relation' && !vis.has(other)) relQueue.push(other);
              }
            }
            for (let i = 0; i < relQueue.length && vis.size < maxNodes; i++) vis.add(relQueue[i]);

            if (state.selectedGid != null) vis.add(state.selectedGid);
            return vis;
          }

          function buildSubgraphFromVisible() {
            const subNodes = [];
            state.visible.forEach(i => subNodes.push(data.nodes[i]));

            const edgeIds = new Set();
            for (const i of state.visible) for (const ei of incident[i]) edgeIds.add(ei);

            const subLinks = [];
            edgeIds.forEach(ei => {
              const e = edges[ei];
              if (state.visible.has(e.s) && state.visible.has(e.t)) subLinks.push(e);
            });

            state.subNodes = subNodes;
            state.subLinks = subLinks;

            const vn = document.getElementById('visibleNodes');
            const vl = document.getElementById('visibleLinks');
            if (vn) vn.textContent = String(subNodes.length);
            if (vl) vl.textContent = String(subLinks.length);
          }

          function rebuildSubgraphIndex() {
            state.subNodesByGid = new Map();
            for (const n of state.subNodes) state.subNodesByGid.set(n.gid, n);
          }

          function updateDetails(node) {
            if (!detailsEl) return;
            if (!node) {
              detailsEl.innerHTML = '<span style="opacity:0.8;">Click a node to see details.</span>';
              return;
            }
            const lines = [];
            lines.push(`<div><b>${escapeHtml(node.label || node.id || 'Node')}</b></div>`);
            lines.push(`<div style="opacity:0.85;">type: <code>${escapeHtml(node.type || '')}</code></div>`);
            if (typeof node.degree === 'number') lines.push(`<div style="opacity:0.85;">degree: <code>${node.degree}</code></div>`);
            if (node.type === 'relation') {
              if (node.label) lines.push(`<div style="opacity:0.85;">relation: <code>${escapeHtml(node.label)}</code></div>`);
              if (node.sources && node.sources.length) lines.push(`<div style="opacity:0.85;">sources: <code>${escapeHtml(node.sources.slice(0, 8).join(', '))}${node.sources.length>8?' ...':''}</code></div>`);
              if (node.targets && node.targets.length) lines.push(`<div style="opacity:0.85;">targets: <code>${escapeHtml(node.targets.slice(0, 8).join(', '))}${node.targets.length>8?' ...':''}</code></div>`);
              if (typeof node.confidence === 'number') lines.push(`<div style="opacity:0.85;">confidence: <code>${node.confidence}</code></div>`);
            }
            detailsEl.innerHTML = lines.join('');
          }

          function escapeHtml(s) {
            return String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
          }

          // ---- Expand hops ----
          function expandSelected(hops) {
            if (state.selectedGid == null) return;

            const maxNodes = state.maxNodes;
            const q = [state.selectedGid];
            const dist = new Map([[state.selectedGid, 0]]);

            while (q.length) {
              const u = q.shift();
              const d = dist.get(u) || 0;
              if (d >= hops) continue;

              for (const ei of incident[u]) {
                const e = edges[ei];
                const v = (e.s === u) ? e.t : e.s;
                if (!dist.has(v)) {
                  dist.set(v, d + 1);
                  q.push(v);
                }
                if (state.visible.size < maxNodes) state.visible.add(v);
              }
              if (state.visible.size >= maxNodes) break;
            }

            const addRel = [];
            for (const idx of state.visible) {
              if (data.nodes[idx].type !== 'entity') continue;
              for (const ei of incident[idx]) {
                const e = edges[ei];
                const other = (e.s === idx) ? e.t : e.s;
                if (data.nodes[other].type === 'relation' && !state.visible.has(other) && state.visible.size < maxNodes) {
                  addRel.push(other);
                }
              }
            }
            for (let i = 0; i < addRel.length && state.visible.size < maxNodes; i++) state.visible.add(addRel[i]);

            rebuildAndRender('Expanding neighborhood...');
          }

          // ---- Graph init ----
          graphDiv.innerHTML = '';

          const Graph = ForceGraph3D()(graphDiv)
            .nodeId('gid')
            .nodeLabel(n => (n.label || n.id || ''))
            .backgroundColor('rgba(0,0,0,0)')
            .showNavInfo(false)
            .enableNodeDrag(true)
            .linkSource('source')
            .linkTarget('target');

          // -------------------- CLUSTERING --------------------

          const PALETTES = {
            classic: ['#4fc3f7','#ff9800','#ab47bc','#66bb6a','#ef5350','#ffa726','#26c6da','#8d6e63','#78909c','#d4e157'],
            pastel:  ['#a3d5ff','#ffd6a5','#bdb2ff','#caffbf','#ffadad','#fdffb6','#9bf6ff','#ffc6ff','#b8f2e6','#f1c0e8'],
            neon:    ['#00e5ff','#ffea00','#ff1744','#76ff03','#e040fb','#ff9100','#1de9b6','#f500ff','#00c853','#2979ff'],
            mono:    ['#4fc3f7','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc']
          };

          function getClusterColor(cid) {
            if (cid == null || cid < 0) return '#4fc3f7';
            const pal = PALETTES[state.palette] || PALETTES.classic;
            return pal[cid % pal.length];
          }

          function applyClusterStyling() {
            const clusteringEnabled = !!state.clusterOn;

            Graph
              .nodeColor(n => {
                if (!clusteringEnabled) return (n.type === 'relation' ? '#ff9800' : '#4fc3f7');
                const cid = n.__cluster ?? -1;
                return getClusterColor(cid);
              })
              .linkColor(l => {
                if (!clusteringEnabled) return (l.type === 'source' ? 'rgba(79,195,247,0.55)' : 'rgba(255,152,0,0.55)');
                const a = state.subNodesByGid.get(l.source) || state.subNodesByGid.get(l.source?.gid) || null;
                const b = state.subNodesByGid.get(l.target) || state.subNodesByGid.get(l.target?.gid) || null;
                const ca = a ? (a.__cluster ?? -1) : -1;
                const cb = b ? (b.__cluster ?? -1) : -1;

                if (ca >= 0 && ca === cb) return getClusterColor(ca);
                return 'rgba(255,255,255,0.10)';
              })
              .linkWidth(l => {
                if (!clusteringEnabled) return 0.6;
                const a = state.subNodesByGid.get(l.source) || state.subNodesByGid.get(l.source?.gid) || null;
                const b = state.subNodesByGid.get(l.target) || state.subNodesByGid.get(l.target?.gid) || null;
                const ca = a ? (a.__cluster ?? -1) : -1;
                const cb = b ? (b.__cluster ?? -1) : -1;
                return (ca >= 0 && ca === cb) ? 0.9 : 0.35;
              });

            Graph.refresh();
          }

          function clearClusterLabels() {
            if (!state.clusterLabels?.length) return;
            const scene = Graph.scene();
            for (const s of state.clusterLabels) scene.remove(s);
            state.clusterLabels = [];
          }

          function drawClusterLabels() {
            clearClusterLabels();
            if (!state.clusterOn || typeof SpriteText === 'undefined') return;

            const scene = Graph.scene();
            for (const cl of state.clusters) {
              const rep = state.subNodesByGid.get(cl.repGid);
              if (!rep) continue;

              const labelTxt = (rep.label || rep.id || `Cluster ${cl.id}`);
              const sprite = new SpriteText(labelTxt);
              sprite.fontFace = 'Arial Black';
              sprite.textHeight = 10;
              sprite.color = 'rgba(255,255,255,0.95)';
              sprite.backgroundColor = 'rgba(0,0,0,0.50)';
              sprite.padding = 6;
              sprite.borderRadius = 10;
              sprite.material.depthWrite = false;

              sprite.position.set(rep.x || 0, (rep.y || 0) + 18, rep.z || 0);
              scene.add(sprite);
              state.clusterLabels.push(sprite);
            }
          }

          function chooseRepresentative(clusterNodeGids) {
            let cx = 0, cy = 0, cz = 0, cnt = 0;
            for (const gid of clusterNodeGids) {
              const n = state.subNodesByGid.get(gid);
              if (!n) continue;
              cx += (n.x || 0); cy += (n.y || 0); cz += (n.z || 0);
              cnt++;
            }
            if (!cnt) return clusterNodeGids[0] ?? null;
            cx /= cnt; cy /= cnt; cz /= cnt;

            let bestGid = null;
            let bestD = Infinity;
            for (const gid of clusterNodeGids) {
              const n = state.subNodesByGid.get(gid);
              if (!n || n.type !== 'entity') continue;
              const dx = (n.x || 0) - cx, dy = (n.y || 0) - cy, dz = (n.z || 0) - cz;
              const d2 = dx*dx + dy*dy + dz*dz;
              if (d2 < bestD) { bestD = d2; bestGid = gid; }
            }
            if (bestGid != null) return bestGid;

            for (const gid of clusterNodeGids) {
              const n = state.subNodesByGid.get(gid);
              if (!n) continue;
              const dx = (n.x || 0) - cx, dy = (n.y || 0) - cy, dz = (n.z || 0) - cz;
              const d2 = dx*dx + dy*dy + dz*dz;
              if (d2 < bestD) { bestD = d2; bestGid = gid; }
            }
            return bestGid;
          }

          // DBSCAN spatial clustering
          function clusterSpatialDBSCAN() {
            const eps = state.clusterRadius;
            const minPts = state.minClusterSize;

            const nodes = state.subNodes;
            const n = nodes.length;
            const labels = new Array(n).fill(0);
            let cid = 0;

            const eps2 = eps * eps;
            function regionQuery(i) {
              const ni = nodes[i];
              const xi = ni.x || 0, yi = ni.y || 0, zi = ni.z || 0;
              const res = [];
              for (let j = 0; j < n; j++) {
                const nj = nodes[j];
                const dx = (nj.x || 0) - xi, dy = (nj.y || 0) - yi, dz = (nj.z || 0) - zi;
                if ((dx*dx + dy*dy + dz*dz) <= eps2) res.push(j);
              }
              return res;
            }

            function expandCluster(i, neighbors, cid) {
              labels[i] = cid;
              for (let k = 0; k < neighbors.length; k++) {
                const j = neighbors[k];
                if (labels[j] === -1) labels[j] = cid;
                if (labels[j] !== 0) continue;
                labels[j] = cid;
                const n2 = regionQuery(j);
                if (n2.length >= minPts) {
                  for (const x of n2) neighbors.push(x);
                }
              }
            }

            for (let i = 0; i < n; i++) {
              if (labels[i] !== 0) continue;
              const neighbors = regionQuery(i);
              if (neighbors.length < minPts) {
                labels[i] = -1;
              } else {
                cid++;
                expandCluster(i, neighbors, cid);
              }
            }

            for (let i = 0; i < n; i++) {
              nodes[i].__cluster = labels[i] > 0 ? (labels[i] - 1) : -1;
            }

            const groups = new Map();
            for (const node of nodes) {
              const c = node.__cluster ?? -1;
              if (c < 0) continue;
              if (!groups.has(c)) groups.set(c, []);
              groups.get(c).push(node.gid);
            }

            state.clusters = [];
            for (const [id, gids] of groups.entries()) {
              const rep = chooseRepresentative(gids);
              state.clusters.push({ id, nodes: gids, repGid: rep });
            }
          }

          // Louvain-style topology clustering
          function clusterTopologyLouvain() {
            const nodes = state.subNodes;
            const links = state.subLinks;
            const idxByGid = new Map();
            for (let i = 0; i < nodes.length; i++) idxByGid.set(nodes[i].gid, i);

            const adj = Array.from({ length: nodes.length }, () => new Map());
            for (const e of links) {
              const a = idxByGid.get(e.s);
              const b = idxByGid.get(e.t);
              if (a == null || b == null || a === b) continue;
              const w = 1;
              adj[a].set(b, (adj[a].get(b) || 0) + w);
              adj[b].set(a, (adj[b].get(a) || 0) + w);
            }

            const resolution = state.topoResolution;
            const n = nodes.length;
            let community = new Array(n);
            for (let i = 0; i < n; i++) community[i] = i;

            let k = new Array(n).fill(0);
            let m2 = 0;
            for (let i = 0; i < n; i++) {
              let sum = 0;
              for (const w of adj[i].values()) sum += w;
              k[i] = sum;
              m2 += sum;
            }
            if (m2 === 0) {
              for (const node of nodes) node.__cluster = -1;
              state.clusters = [];
              return;
            }

            let tot = new Array(n).fill(0);
            for (let i = 0; i < n; i++) tot[community[i]] += k[i];

            function neighCommWeights(i) {
              const map = new Map();
              for (const [j, w] of adj[i].entries()) {
                const cj = community[j];
                map.set(cj, (map.get(cj) || 0) + w);
              }
              return map;
            }

            function modularityGain(i, c, ki_in, totc) {
              return (resolution * (ki_in / m2)) - ((k[i] * totc) / (m2 * m2));
            }

            let improved = true;
            let passes = 0;
            while (improved && passes < 15) {
              improved = false;
              passes++;

              for (let i = 0; i < n; i++) {
                const ci = community[i];
                const neigh = neighCommWeights(i);
                tot[ci] -= k[i];

                let bestC = ci;
                let bestGain = 0;

                for (const [c, ki_in] of neigh.entries()) {
                  const gain = modularityGain(i, c, ki_in, tot[c]);
                  if (gain > bestGain) {
                    bestGain = gain;
                    bestC = c;
                  }
                }

                if (bestC !== ci) {
                  community[i] = bestC;
                  improved = true;
                }

                tot[community[i]] += k[i];
              }
            }

            const remap = new Map();
            let nextId = 0;
            for (let i = 0; i < n; i++) {
              const c = community[i];
              if (!remap.has(c)) remap.set(c, nextId++);
              community[i] = remap.get(c);
            }

            const groups = new Map();
            for (let i = 0; i < n; i++) {
              const cid = community[i];
              if (!groups.has(cid)) groups.set(cid, []);
              groups.get(cid).push(nodes[i].gid);
            }

            for (const node of nodes) node.__cluster = -1;
            state.clusters = [];
            let keptId = 0;
            for (const [cid, gids] of groups.entries()) {
              if (gids.length < state.minClusterSize) continue;
              for (const gid of gids) {
                const nd = state.subNodesByGid.get(gid);
                if (nd) nd.__cluster = keptId;
              }
              const rep = chooseRepresentative(gids);
              state.clusters.push({ id: keptId, nodes: gids, repGid: rep });
              keptId++;
            }
          }

          function recomputeClustering() {
            rebuildSubgraphIndex();

            for (const n of state.subNodes) n.__cluster = -1;
            state.clusters = [];

            if (!state.clusterOn) {
              clearClusterLabels();
              applyClusterStyling();
              return;
            }

            if (state.clusterMode === 'spatial') clusterSpatialDBSCAN();
            else clusterTopologyLouvain();

            applyClusterStyling();
            drawClusterLabels();
          }

          function updateClusterLabelPositions() {
            if (!state.clusterLabels?.length) return;
            for (let i = 0; i < state.clusters.length; i++) {
              const cl = state.clusters[i];
              const rep = state.subNodesByGid.get(cl.repGid);
              const lab = state.clusterLabels[i];
              if (!rep || !lab) continue;
              lab.position.set(rep.x || 0, (rep.y || 0) + 18, rep.z || 0);
            }
          }

          // -------------------- END CLUSTERING --------------------

          // Styling
          Graph
            .nodeRelSize(3)
            .nodeVal(n => (n.type === 'relation' ? 1.5 : clamp(1 + (n.degree || 0) * 0.06, 1.2, 6)))
            .nodeColor(n => (n.type === 'relation' ? '#ff9800' : '#4fc3f7'))
            .linkColor(l => (l.type === 'source' ? 'rgba(79,195,247,0.55)' : 'rgba(255,152,0,0.55)'))
            .linkWidth(l => (l.type === 'source' ? 0.6 : 0.6));

          function applyArrows() {
            const on = !!state.showArrows;
            Graph
              .linkDirectionalArrowLength(on ? 3.5 : 0)
              .linkDirectionalArrowRelPos(1)
              .linkDirectionalArrowColor(() => 'rgba(255,255,255,0.55)');
          }

          function applyLabels() {
            state.showLabels = !!showLabelsEl.checked;
            if (typeof SpriteText === 'undefined') return;

            Graph.nodeThreeObject(node => {
              if (!state.showLabels) return null;

              const isSelected = (state.selectedGid != null && node.gid === state.selectedGid);
              const important = (node.type === 'entity' && (node.degree || 0) >= Math.max(40, state.minDegree));
              if (!isSelected && !important) return null;

              const sprite = new SpriteText(node.label || node.id || '');
              sprite.textHeight = isSelected ? 6 : 4;
              sprite.color = 'rgba(255,255,255,0.9)';
              sprite.backgroundColor = 'rgba(0,0,0,0.35)';
              sprite.padding = 2;
              sprite.borderRadius = 6;
              sprite.material.depthWrite = false;
              sprite.position.y = 8;
              return sprite;
            }).nodeThreeObjectExtend(true);

            Graph.refresh();
          }

          function applyForces() {
            Graph.d3Force('charge').strength(-state.repulsion);
            Graph.d3Force('link').strength(state.linkStrength);
            Graph.d3Force('center', d3.forceCenter(0, 0, 0));
          }

          function applyFreeze() {
            state.autoFreeze = !!autoFreezeEl.checked;
            if (state.autoFreeze) {
              Graph.cooldownTime(1500);
            } else {
              Graph.cooldownTime(3.6e9);
            }
          }

          function resetCamera() {
            Graph.cameraPosition(
              { x: 0, y: 0, z: 420 },
              { x: 0, y: 0, z: 0 },
              700
            );
          }

          function focusNode(node) {
            if (!node) return;
            const dist = 180;
            const ratio = 1 + dist / Math.hypot(node.x || 0, node.y || 0, node.z || 0);
            Graph.cameraPosition(
              { x: (node.x || 0) * ratio, y: (node.y || 0) * ratio, z: (node.z || 0) * ratio },
              { x: node.x || 0, y: node.y || 0, z: node.z || 0 },
              700
            );
          }

          // ---- Render / rebuild orchestration ----
          function rebuildAndRender(message) {
            showOverlay(message || 'Updating...', 'Filtering & rebuilding visible subgraph');

            ric(() => {
              buildSubgraphFromVisible();

              const gData = {
                nodes: state.subNodes,
                links: state.subLinks.map(e => ({ source: e.s, target: e.t, type: e.type }))
              };

              Graph.graphData(gData);
              applyForces();
              applyFreeze();
              applyArrows();
              applyLabels();

              Graph.d3ReheatSimulation();

              // Cluster after a short delay for layout to settle
              if (state.autoCluster) {
                setTimeout(() => recomputeClustering(), 1400);
              } else {
                recomputeClustering();
              }

              hideOverlay();
              updateHud();
            });
          }

          function rebuildFromDegree() {
            showOverlay('Seeding visible subgraph...', 'Using degree threshold + caps');
            ric(() => {
              state.visible = seedVisibleByDegree(state.minDegree, state.maxNodes);
              if (state.selectedGid != null) state.visible.add(state.selectedGid);
              rebuildAndRender('Rendering subgraph...');
            });
          }

          // ---- Events ----
          Graph.onNodeClick((node) => {
            if (!node) return;
            state.selectedGid = node.gid;
            state.visible.add(node.gid);
            updateDetails(node);
            applyLabels();
            focusNode(node);
          });

          Graph.onNodeHover(rafThrottle((node) => {
            const tt = document.getElementById('tooltip');
            if (!tt) return;
            if (!node) {
              tt.style.opacity = 0;
              return;
            }
            tt.style.opacity = 1;
            tt.innerHTML = `<strong>${escapeHtml(node.label || node.id || '')}</strong><br/><span style="opacity:0.85;">${escapeHtml(node.type || '')}${typeof node.degree==='number' ? ` deg ${node.degree}` : ''}</span>`;
          }));

          // Controls wiring
          let degTimer = null;
          minDegreeEl.addEventListener('input', () => {
            state.minDegree = +minDegreeEl.value;
            if (degreeValueEl) degreeValueEl.textContent = String(state.minDegree);
            if (degTimer) clearTimeout(degTimer);
            degTimer = setTimeout(rebuildFromDegree, 150);
          });

          linkStrengthEl.addEventListener('input', () => {
            state.linkStrength = +linkStrengthEl.value;
            applyForces();
            Graph.d3ReheatSimulation();
          });

          repulsionEl.addEventListener('input', () => {
            state.repulsion = +repulsionEl.value;
            applyForces();
            Graph.d3ReheatSimulation();
          });

          showLabelsEl.addEventListener('change', () => {
            state.showLabels = !!showLabelsEl.checked;
            applyLabels();
          });

          autoFreezeEl.addEventListener('change', () => {
            applyFreeze();
            Graph.d3ReheatSimulation();
          });

          arrowsEl.addEventListener('change', () => {
            state.showArrows = !!arrowsEl.checked;
            applyArrows();
            Graph.refresh();
          });

          maxNodesEl.addEventListener('input', () => {
            state.maxNodes = +maxNodesEl.value;
            if (maxNodesValEl) maxNodesValEl.textContent = String(state.maxNodes);
          });
          maxNodesEl.addEventListener('change', () => {
            rebuildFromDegree();
          });

          depthEl.addEventListener('input', () => {
            state.depthSpread = +depthEl.value;
            if (depthValEl) depthValEl.textContent = String(state.depthSpread);
          });
          depthEl.addEventListener('change', () => {
            applyDepth();
            rebuildFromDegree();
          });

          exp1Btn.addEventListener('click', () => expandSelected(1));
          exp2Btn.addEventListener('click', () => expandSelected(2));

          resetBtn.addEventListener('click', () => {
            state.selectedGid = null;
            updateDetails(null);
            rebuildFromDegree();
            resetCamera();
          });

          camResetBtn.addEventListener('click', () => resetCamera());

          function findNodeByQuery(q) {
            q = (q || '').trim().toLowerCase();
            if (!q) return null;
            let idx = labelsLower.indexOf(q);
            if (idx !== -1) return data.nodes[idx];
            for (let i = 0; i < labelsLower.length; i++) {
              if (labelsLower[i] && labelsLower[i].includes(q)) return data.nodes[i];
            }
            return null;
          }

          function goSearch() {
            const node = findNodeByQuery(searchEl.value);
            if (!node) return;
            state.selectedGid = node.gid;
            state.visible.add(node.gid);
            updateDetails(node);
            rebuildAndRender('Jumping to node...');
            setTimeout(() => focusNode(node), 50);
          }

          goBtn.addEventListener('click', goSearch);
          searchEl.addEventListener('keydown', (ev) => {
            if (ev.key === 'Enter') goSearch();
          });

          // Clustering controls
          function updateClusterModeUI() {
            const mode = clusterModeEl.value;
            state.clusterMode = mode;
            if (mode === 'spatial') {
              spatialRadiusRow.style.display = '';
              spatialMinRow.style.display = '';
              topoResRow.style.display = 'none';
            } else {
              spatialRadiusRow.style.display = 'none';
              spatialMinRow.style.display = '';
              topoResRow.style.display = '';
            }
            recomputeClustering();
          }

          clusterOnEl.addEventListener('change', () => {
            state.clusterOn = !!clusterOnEl.checked;
            recomputeClustering();
          });

          clusterModeEl.addEventListener('change', updateClusterModeUI);

          clusterRadiusEl.addEventListener('input', () => {
            state.clusterRadius = +clusterRadiusEl.value;
            clusterRadiusValEl.textContent = String(state.clusterRadius);
          });
          clusterRadiusEl.addEventListener('change', recomputeClustering);

          minClusterEl.addEventListener('input', () => {
            state.minClusterSize = +minClusterEl.value;
            minClusterValEl.textContent = String(state.minClusterSize);
          });
          minClusterEl.addEventListener('change', recomputeClustering);

          topoResEl.addEventListener('input', () => {
            state.topoResolution = +topoResEl.value;
            topoResValEl.textContent = state.topoResolution.toFixed(2);
          });
          topoResEl.addEventListener('change', recomputeClustering);

          paletteEl.addEventListener('change', () => {
            state.palette = paletteEl.value;
            applyClusterStyling();
            drawClusterLabels();
          });

          autoClusterEl.addEventListener('change', () => {
            state.autoCluster = !!autoClusterEl.checked;
          });

          reclusterBtn.addEventListener('click', recomputeClustering);

          // Keyboard shortcuts
          window.addEventListener('keydown', (ev) => {
            if (ev.key === '/') {
              ev.preventDefault();
              searchEl.focus();
            }
          });

          // HUD (FPS + counts)
          function updateHud() {
            const now = performance.now();
            state.fps.frames += 1;
            const dt = now - state.fps.t0;
            if (dt >= 800) {
              state.fps.value = Math.round((state.fps.frames * 1000) / dt);
              state.fps.frames = 0;
              state.fps.t0 = now;
            }
            hud.textContent = `3D | ${state.subNodes.length} nodes | ${state.subLinks.length} links | ${state.fps.value} fps`;
          }
          if (Graph.onRenderFramePost) {
            Graph.onRenderFramePost(() => {
              updateHud();
              updateClusterLabelPositions();
            });
          } else {
            setInterval(() => {
              updateHud();
              updateClusterLabelPositions();
            }, 250);
          }

          window.addEventListener('resize', () => {
            Graph.width(window.innerWidth);
            Graph.height(window.innerHeight);
          });

          // ---- Boot sequence ----
          updateDetails(null);
          if (maxNodesValEl) maxNodesValEl.textContent = String(state.maxNodes);
          if (depthValEl) depthValEl.textContent = String(state.depthSpread);
          clusterRadiusValEl.textContent = String(state.clusterRadius);
          minClusterValEl.textContent = String(state.minClusterSize);
          topoResValEl.textContent = state.topoResolution.toFixed(2);
          updateClusterModeUI();

          applyDepth();
          resetCamera();
          state.visible = seedVisibleByDegree(state.minDegree, state.maxNodes);
          buildSubgraphFromVisible();

          hideOverlay();
          rebuildAndRender('Rendering initial 3D view...');
        })();
    </script>
</body>
</html>
)";

    // Ensure all data is written before closing
    file.flush();
    if (file.fail()) {
        throw std::runtime_error("Failed to write HTML file: " + filename);
    }
    file.close();
}

} // namespace kg

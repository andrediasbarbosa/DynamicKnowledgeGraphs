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
    (void)min_intersection_size;
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

    // Build JSON data for ForceGraph3D.
    // Entity nodes only in nodes_json; hyperedges serialised separately as hyperedges_json,
    // consistent with the graph.json on-disk format (nodes + hyperedges with sources[]/targets[]).
    // JS reifyHyperedges() converts them to relation nodes + binary links at load time.
    nlohmann::json nodes_json = nlohmann::json::array();
    nlohmann::json hyperedges_json = nlohmann::json::array();

    for (const auto& [id, node] : nodes_) {
        nlohmann::json n;
        n["id"] = id;
        n["label"] = node.label;
        n["type"] = "entity";
        n["degree"] = node.degree;
        if (!node.properties.empty()) {
            n["properties"] = node.properties;
        }
        nodes_json.push_back(n);
    }

    for (const auto& [id, edge] : hyperedges_) {
        nlohmann::json he;
        he["id"]         = edge.id;
        he["relation"]   = edge.relation;
        he["sources"]    = edge.sources;
        he["targets"]    = edge.targets;
        he["confidence"] = edge.confidence;
        if (!edge.source_document.empty()) he["source_document"] = edge.source_document;
        if (!edge.source_chunk_id.empty())  he["source_chunk_id"] = edge.source_chunk_id;
        if (!edge.properties.empty())       he["properties"] = edge.properties;
        hyperedges_json.push_back(he);
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
            hyperedges: )" << hyperedges_json.dump() << R"(
        };

        // Reify hyperedges: each entry in data.hyperedges becomes a relation node in data.nodes
        // and two binary links in data.links (entity→relation, relation→entity).
        // Mirrors the graph.json on-disk format (nodes + hyperedges with sources[]/targets[]).
        function reifyHyperedges(d) {
            const idToIdx = new Map();
            d.nodes.forEach((n, i) => idToIdx.set(n.id, i));
            d.links = [];
            let idx = d.nodes.length;
            for (const he of (d.hyperedges || [])) {
                const relNode = {
                    id: he.id, label: he.relation, type: 'relation',
                    confidence: he.confidence,
                    source_document: he.source_document || '',
                    source_chunk_id: he.source_chunk_id || '',
                    sources: he.sources, targets: he.targets
                };
                if (he.properties) relNode.properties = he.properties;
                d.nodes.push(relNode);
                const relIdx = idx++;
                idToIdx.set(he.id, relIdx);
                for (const src of (he.sources || [])) {
                    const si = idToIdx.get(src);
                    if (si !== undefined) d.links.push({ source: si, target: relIdx, type: 'source' });
                }
                for (const tgt of (he.targets || [])) {
                    const ti = idToIdx.get(tgt);
                    if (ti !== undefined) d.links.push({ source: relIdx, target: ti, type: 'target' });
                }
            }
        }
        reifyHyperedges(data);

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
                  Min cluster size: <span id="kgMinClusterVal">5</span>
                  <input type="range" id="kgMinCluster" min="3" max="200" step="1" value="5">
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
          console.log('detailsEl found:', detailsEl);

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
            minClusterSize: 5,
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
            .linkTarget('target')
            .cooldownTime(2000)  // Prevent runaway physics simulation
            .warmupTicks(0);     // Don't pre-calculate - let rebuildAndRender handle it

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
            // Project bipartite entity->relation->entity structure to entity co-occurrence
            // so Louvain can find meaningful entity clusters (not just star micro-communities)
            for (let ri = 0; ri < nodes.length; ri++) {
              if (nodes[ri].type !== 'relation') continue;
              const en = [...adj[ri].keys()].filter(j => nodes[j].type === 'entity');
              for (let p = 0; p < en.length; p++) {
                for (let q = p + 1; q < en.length; q++) {
                  const a = en[p], b = en[q];
                  adj[a].set(b, (adj[a].get(b) || 0) + 1);
                  adj[b].set(a, (adj[b].get(a) || 0) + 1);
                }
              }
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
            // ForceGraph3D has built-in center force, only set charge and link
            if (Graph.d3Force('charge')) Graph.d3Force('charge').strength(-state.repulsion);
            if (Graph.d3Force('link')) Graph.d3Force('link').strength(state.linkStrength);
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
            console.log('Node clicked:', node);
            if (!node) return;
            state.selectedGid = node.gid;
            state.visible.add(node.gid);
            console.log('Calling updateDetails with node:', node.label, node.type, node.degree);
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
              spatialMinRow.style.display = 'none';
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

// ============================================================================
// Graph-RAG HTML Viewer
// ============================================================================

void Hypergraph::export_to_html_rag(  // NOLINT — intentionally large generator
        const std::string& filename,
        const std::string& title,
        const std::string& llm_provider,
        const std::string& llm_api_key,
        const std::string& llm_model,
        const std::string& llm_base_url,
        const nlohmann::json& augmentation_json) const {

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // ----- Statistics & optimal degree (matches augmented renderer) -----
    auto stats = compute_statistics();

    // ----- Optimal min-degree (matches augmented renderer: target ≤500 initial edges) -----
    int max_degree = static_cast<int>(stats.max_node_degree);
    const int MAX_INITIAL_EDGES = 500;
    int optimal_min_degree = 5;
    int num_edges_est = static_cast<int>(stats.num_edges);
    if (num_edges_est > MAX_INITIAL_EDGES) {
        std::map<int,int> nodes_at_degree;
        for (const auto& node : get_all_nodes()) {
            nodes_at_degree[node.degree]++;
            if (node.degree > max_degree) max_degree = node.degree;
        }
        for (int test = 5; test <= max_degree; ++test) {
            int vis = 0;
            for (int d = test; d <= max_degree; ++d)
                if (nodes_at_degree.count(d)) vis += nodes_at_degree.at(d);
            double frac = static_cast<double>(vis) / std::max(1, (int)stats.num_nodes);
            if (static_cast<int>(num_edges_est * frac * frac) <= MAX_INITIAL_EDGES) {
                optimal_min_degree = test; break;
            }
            optimal_min_degree = test;
        }
    }

    // ----- Base graph JSON — entity nodes + hyperedges (consistent with graph.json on disk) -----
    // JS reifyHyperedges() converts hyperedges to relation nodes + binary links at load time.
    nlohmann::json nodes_json = nlohmann::json::array();
    nlohmann::json hyperedges_json = nlohmann::json::array();

    // Build edge provenance index: node_id → {unique source_chunk_ids, unique source_documents}
    struct NodeProv { std::set<std::string> chunks; std::set<std::string> docs; };
    std::map<std::string, NodeProv> node_prov;
    for (const auto& edge : get_all_edges()) {
        for (const auto& nid : edge.sources) {
            if (!edge.source_chunk_id.empty()) node_prov[nid].chunks.insert(edge.source_chunk_id);
            if (!edge.source_document.empty())  node_prov[nid].docs.insert(edge.source_document);
        }
        for (const auto& nid : edge.targets) {
            if (!edge.source_chunk_id.empty()) node_prov[nid].chunks.insert(edge.source_chunk_id);
            if (!edge.source_document.empty())  node_prov[nid].docs.insert(edge.source_document);
        }
    }

    for (const auto& node : get_all_nodes()) {
        nlohmann::json n;
        n["id"]    = node.id;
        n["label"] = node.label;
        n["type"]  = "entity";
        n["degree"]= node.degree;
        // Properties (skip empty)
        if (!node.properties.empty()) {
            nlohmann::json props = nlohmann::json::object();
            for (const auto& [k, v] : node.properties) props[k] = v;
            n["properties"] = props;
        }
        // Provenance: up to 5 chunks and docs referencing this node
        if (node_prov.count(node.id)) {
            const auto& prov = node_prov.at(node.id);
            nlohmann::json cj = nlohmann::json::array();
            int ci = 0;
            for (const auto& c : prov.chunks) { if (ci++ >= 5) break; cj.push_back(c); }
            if (!cj.empty()) n["chunks"] = cj;
            nlohmann::json dj = nlohmann::json::array();
            int di = 0;
            for (const auto& d : prov.docs) { if (di++ >= 5) break; dj.push_back(d); }
            if (!dj.empty()) n["docs"] = dj;
        }
        nodes_json.push_back(n);
    }

    for (const auto& edge : get_all_edges()) {
        nlohmann::json he;
        he["id"]         = edge.id;
        he["relation"]   = edge.relation;
        he["sources"]    = edge.sources;
        he["targets"]    = edge.targets;
        he["confidence"] = edge.confidence;
        if (!edge.source_document.empty()) he["source_document"] = edge.source_document;
        if (!edge.source_chunk_id.empty()) he["source_chunk_id"] = edge.source_chunk_id;
        if (edge.source_page >= 0)         he["source_page"]     = edge.source_page;
        if (!edge.properties.empty()) {
            nlohmann::json eprops = nlohmann::json::object();
            for (const auto& [k, v] : edge.properties) eprops[k] = v;
            he["properties"] = eprops;
        }
        hyperedges_json.push_back(he);
    }

    // ----- Augmentation JSON (passed straight to JS as augData) -----
    nlohmann::json safe_aug = augmentation_json;
    if (safe_aug.is_null() || !safe_aug.is_object())
        safe_aug = {{"nodes", nlohmann::json::array()}, {"links", nlohmann::json::array()}};
    if (!safe_aug.contains("nodes")) safe_aug["nodes"] = nlohmann::json::array();
    if (!safe_aug.contains("links")) safe_aug["links"] = nlohmann::json::array();
    int aug_node_count = (int)safe_aug["nodes"].size();

    file << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)HTML" << title << R"HTML( — Graph-RAG</title>
    <script src="https://d3js.org/d3.v7.min.js"></script>
    <script src="https://unpkg.com/three@0.160.0/build/three.min.js"></script>
    <script src="https://unpkg.com/three-spritetext@1.9.3/dist/three-spritetext.min.js"></script>
<script src="https://unpkg.com/3d-force-graph@1.75.0/dist/3d-force-graph.min.js"></script>
<style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;overflow:hidden;font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);color:#eee}
/* ── header ── */
#header{position:fixed;top:0;left:0;right:0;height:52px;padding:0 20px;background:rgba(0,0,0,.45);backdrop-filter:blur(10px);z-index:200;display:flex;align-items:center;gap:16px}
#header h1{font-size:1.15em;font-weight:500;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1}
#hdrStats{font-size:.82em;opacity:.75;white-space:nowrap}
#chatToggleBtn{padding:7px 14px;border-radius:8px;border:1px solid rgba(255,255,255,.22);background:rgba(79,195,247,.18);color:#4fc3f7;cursor:pointer;font-size:.88em;white-space:nowrap;transition:background .2s}
#chatToggleBtn:hover{background:rgba(79,195,247,.30)}
#chatToggleBtn.active{background:rgba(79,195,247,.38);border-color:#4fc3f7}
/* ── graph canvas ── */
#graph{position:fixed;inset:52px 0 0 0}
/* ── left controls ── */
#controls{position:fixed;bottom:16px;left:16px;max-height:calc(100vh - 80px);overflow-y:auto;background:rgba(0,0,0,.62);padding:14px;border-radius:10px;backdrop-filter:blur(10px);z-index:150;font-size:.83em;width:230px}
#controls label{display:block;margin-bottom:7px}
#controls input[type=range]{width:100%;margin-top:3px}
/* ── legend ── */
#legend{position:fixed;top:64px;right:16px;background:rgba(0,0,0,.62);padding:13px;border-radius:10px;backdrop-filter:blur(10px);z-index:150;font-size:.83em;transition:right .35s ease}
#legend h3{margin-bottom:8px;font-size:.95em}
.legend-item{display:flex;align-items:center;margin:5px 0}
.legend-dot{width:12px;height:12px;border-radius:50%;margin-right:8px;flex-shrink:0}
.legend-line{width:26px;height:3px;margin-right:8px;flex-shrink:0;border-radius:2px}
/* ── shared button/input styles ── */
.kg-btn{padding:7px 11px;border-radius:8px;border:1px solid rgba(255,255,255,.18);background:rgba(255,255,255,.08);color:#eee;cursor:pointer;user-select:none;font-size:.83em}
.kg-btn:hover{background:rgba(255,255,255,.14)}
.kg-btn:active{transform:translateY(1px)}
.kg-row{display:flex;gap:7px;align-items:center;margin-top:8px;flex-wrap:wrap}
.kg-input{padding:7px 9px;border-radius:8px;border:1px solid rgba(255,255,255,.15);background:rgba(255,255,255,.06);color:#eee;outline:none;font-size:.83em}
.kg-input::placeholder{color:rgba(255,255,255,.45)}
/* ── HUD ── */
#kgHud{position:fixed;top:64px;left:16px;padding:8px 11px;border-radius:8px;background:rgba(0,0,0,.45);backdrop-filter:blur(8px);font-size:11px;color:rgba(255,255,255,.8);z-index:120;pointer-events:none}
/* ── overlay ── */
#kgOverlay{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(0,0,0,.35);backdrop-filter:blur(6px);z-index:2000;opacity:0;pointer-events:none;transition:opacity .2s}
#kgOverlay.show{opacity:1;pointer-events:all}
#kgOverlay .panel{background:rgba(0,0,0,.8);border:1px solid rgba(255,255,255,.12);border-radius:14px;padding:18px 22px;min-width:260px;color:#eee}
#kgOverlay .ov-title{font-size:14px;color:#4fc3f7;margin-bottom:7px}
#kgOverlay .ov-sub{font-size:12px;color:rgba(255,255,255,.7)}
/* ── tooltip ── */
#tooltip{position:fixed;background:rgba(0,0,0,.88);padding:11px 15px;border-radius:9px;pointer-events:none;opacity:0;transition:opacity .15s;max-width:280px;font-size:.85em;z-index:1000;border:1px solid rgba(255,255,255,.12)}
#tooltip h3{margin-bottom:5px;color:#4fc3f7;font-size:.95em}
#tooltip .tp-row{color:#aaa;margin:2px 0;font-size:.9em}
/* ── chat panel ── */
#chatPanel{position:fixed;top:52px;right:0;bottom:0;width:380px;background:rgba(10,15,30,.95);backdrop-filter:blur(14px);border-left:1px solid rgba(255,255,255,.1);z-index:160;display:flex;flex-direction:column;transform:translateX(100%);transition:transform .35s ease}
#chatPanel.open{transform:translateX(0)}
#chatPanel.open~#legend{right:396px}
#chatPanelHead{padding:12px 14px;border-bottom:1px solid rgba(255,255,255,.1);display:flex;align-items:center;justify-content:space-between;flex-shrink:0}
#chatPanelHead h2{font-size:1em;font-weight:600;color:#4fc3f7}
#chatCloseBtn{background:none;border:none;color:#aaa;font-size:1.4em;cursor:pointer;line-height:1;padding:0 4px}
#chatCloseBtn:hover{color:#eee}
/* ── settings block inside chat ── */
#chatSettings{padding:10px 12px;border-bottom:1px solid rgba(255,255,255,.08);flex-shrink:0;font-size:.8em}
#settingsToggle{background:none;border:none;color:#4fc3f7;cursor:pointer;font-size:.82em;padding:0;margin-bottom:6px;display:flex;align-items:center;gap:5px}
#settingsBody{display:none}
#settingsBody.open{display:block}
#settingsBody label{display:block;margin-bottom:6px;color:#bbb}
#settingsBody input,#settingsBody select{width:100%;padding:6px 8px;border-radius:7px;border:1px solid rgba(255,255,255,.14);background:rgba(255,255,255,.05);color:#eee;font-size:.9em;outline:none;margin-top:3px}
#settingsBody .warning{font-size:.78em;color:#ff9800;margin-top:5px}
/* ── chat history ── */
#chatHistory{flex:1;overflow-y:auto;padding:12px 12px 6px;display:flex;flex-direction:column;gap:10px}
#chatHistory::-webkit-scrollbar{width:5px}
#chatHistory::-webkit-scrollbar-track{background:transparent}
#chatHistory::-webkit-scrollbar-thumb{background:rgba(255,255,255,.15);border-radius:3px}
.chat-msg{max-width:100%;word-break:break-word}
.chat-msg.user .bubble{background:rgba(79,195,247,.14);border:1px solid rgba(79,195,247,.25);border-radius:10px 10px 4px 10px;padding:9px 12px;color:#eee;font-size:.85em;line-height:1.45;align-self:flex-end}
.chat-msg.assistant .bubble{background:rgba(255,255,255,.06);border:1px solid rgba(255,255,255,.1);border-radius:10px 10px 10px 4px;padding:9px 12px;color:#ddd;font-size:.85em;line-height:1.45}
.chat-msg .meta{font-size:.72em;color:#666;margin-top:4px;display:flex;align-items:center;gap:8px}
.chat-msg.user .meta{justify-content:flex-end}
.hl-btn{background:rgba(255,200,0,.12);border:1px solid rgba(255,200,0,.3);color:#ffd700;border-radius:6px;padding:3px 8px;cursor:pointer;font-size:.78em}
.hl-btn:hover{background:rgba(255,200,0,.22)}
.thinking-dots{display:inline-block;font-size:1.3em;color:#4fc3f7;letter-spacing:3px;animation:dots 1.2s infinite}
@keyframes dots{0%,20%{opacity:.2}50%{opacity:1}80%,100%{opacity:.2}}
/* ── chat input area ── */
#chatInputArea{padding:10px 12px;border-top:1px solid rgba(255,255,255,.1);flex-shrink:0}
#queryInput{width:100%;resize:none;height:70px;padding:9px 11px;border-radius:9px;border:1px solid rgba(255,255,255,.18);background:rgba(255,255,255,.05);color:#eee;font-size:.85em;outline:none;line-height:1.4}
#queryInput::placeholder{color:rgba(255,255,255,.4)}
#queryInput:focus{border-color:rgba(79,195,247,.4)}
#chatBtns{display:flex;gap:7px;margin-top:7px;flex-wrap:wrap}
#sendBtn{flex:1;padding:8px;border-radius:8px;border:none;background:#4fc3f7;color:#111;font-weight:600;cursor:pointer;font-size:.85em;transition:background .2s}
#sendBtn:hover{background:#81d4fa}
#sendBtn:disabled{background:#444;color:#888;cursor:not-allowed}
#clearChatBtn,#clearHlBtn{padding:7px 11px;border-radius:8px;border:1px solid rgba(255,255,255,.18);background:rgba(255,255,255,.07);color:#bbb;cursor:pointer;font-size:.8em}
#clearChatBtn:hover,#clearHlBtn:hover{background:rgba(255,255,255,.13)}
/* ── highlight glow overlay on canvas ── */
#hlBadge{position:fixed;bottom:16px;right:400px;background:rgba(255,215,0,.18);border:1px solid rgba(255,215,0,.45);color:#ffd700;padding:6px 12px;border-radius:8px;font-size:.8em;z-index:155;display:none}
#hlBadge.show{display:block}
/* ── suggested query chips ── */
#suggestedQueries{margin-top:10px;display:flex;flex-wrap:wrap;gap:6px}
#suggestedQueries .sq-label{width:100%;font-size:.72em;color:rgba(255,255,255,.45);margin-bottom:2px}
.sq-chip{padding:5px 10px;border-radius:14px;border:1px solid rgba(79,195,247,.3);background:rgba(79,195,247,.07);color:rgba(79,195,247,.9);font-size:.76em;cursor:pointer;line-height:1.3;transition:background .15s,border-color .15s;text-align:left}
.sq-chip:hover{background:rgba(79,195,247,.18);border-color:rgba(79,195,247,.55)}
/* scrollbar for controls */
#controls::-webkit-scrollbar{width:4px}
#controls::-webkit-scrollbar-thumb{background:rgba(255,255,255,.15);border-radius:2px}
/* ── augmentation section (matches graph_augmented) ── */
.aug-section{border-top:1px solid #e040fb;margin-top:12px;padding-top:10px}
.aug-section>label:first-child{color:#e040fb;font-weight:600}
#augDetails{margin-top:10px;padding:10px;background:rgba(224,64,251,0.1);border-radius:8px;font-size:.8em;display:none}
#augDetails.show{display:block}
</style>
</head>
<body>
<!-- Header -->
<div id="header">
  <h1>)HTML" << title << R"HTML(</h1>
  <span id="hdrStats">Entities: )HTML" << stats.num_nodes << R"HTML( &nbsp;|&nbsp; Relations: )HTML" << stats.num_edges << R"HTML()HTML"
    << (aug_node_count > 0 ? " &nbsp;|&nbsp; Insights: " + std::to_string(aug_node_count) : "")
    << R"HTML( &nbsp;|&nbsp; Avg&nbsp;Deg: )HTML"
    << std::fixed << std::setprecision(2) << stats.avg_node_degree << R"HTML(</span>
  <button id="chatToggleBtn">&#x1F4AC; Graph-RAG Chat</button>
</div>
<!-- 3-D graph canvas -->
<div id="graph"></div>
<!-- Left controls panel -->
<div id="controls">
  <label>Min Entity Degree: <span id="degreeValue">)HTML" << optimal_min_degree << R"HTML(</span>
    <input type="range" id="minDegree" min="1" max=")HTML" << std::max(20, max_degree) << R"HTML(" step="1" value=")HTML" << optimal_min_degree << R"HTML(">
  </label>
  <label>Link Strength:
    <input type="range" id="linkStrength" min="0.1" max="2" step="0.1" value="0.5">
  </label>
  <label>Node Repulsion:
    <input type="range" id="repulsion" min="100" max="1000" step="50" value="400">
  </label>
  <label><input type="checkbox" id="showLabels" checked> Show Labels</label>
  <div id="filterStats" style="margin-top:8px;font-size:.78em;color:#aaa">
    Showing: <span id="visibleNodes">0</span> nodes, <span id="visibleLinks">0</span> links
  </div>
</div>
<!-- HUD / legend / overlay / tooltip -->
<div id="kgHud">3D: initializing...</div>
<div id="legend">
  <h3>Legend</h3>
  <div class="legend-item"><div class="legend-dot" style="background:#4fc3f7"></div>Entity</div>
  <div class="legend-item"><div class="legend-dot" style="background:#ff9800"></div>Relation</div>
  <div class="legend-item"><div class="legend-line" style="background:#4fc3f7"></div>Source link</div>
  <div class="legend-item"><div class="legend-line" style="background:#ff9800"></div>Target link</div>
  <div class="legend-item" id="legendAug" style=")HTML" << (aug_node_count > 0 ? "" : "display:none") << R"HTML("><div class="legend-dot" style="background:#66ffb2;box-shadow:0 0 5px #66ffb2"></div>Insight node</div>
  <div class="legend-item" id="legendHL" style="display:none"><div class="legend-dot" style="background:#ffd700;box-shadow:0 0 6px #ffd700"></div>Highlighted</div>
</div>
<div id="kgOverlay">
  <div class="panel"><div class="ov-title" id="ovTitle">Working…</div><div class="ov-sub" id="ovSub"></div></div>
</div>
<div id="tooltip"></div>
<div id="hlBadge">&#x2728; Highlighted from query</div>
<!-- Chat panel -->
<div id="chatPanel">
  <div id="chatPanelHead">
    <h2>&#x1F9E0; Graph-RAG Chat</h2>
    <button id="chatCloseBtn">&#x00D7;</button>
  </div>
  <!-- Settings -->
  <div id="chatSettings">
    <button id="settingsToggle">&#x2699;&#xFE0F; LLM Settings <span id="settingsArrow">&#x25BC;</span></button>
    <div id="settingsBody">
      <label>Provider
        <select id="cfgProvider">
          <option value="openai">OpenAI</option>
          <option value="gemini">Google Gemini</option>
          <option value="openai-compatible">OpenAI-compatible</option>
        </select>
      </label>
      <label>API Key
        <input type="password" id="cfgApiKey" placeholder="sk-…  or  AIza…">
      </label>
      <label>Model
        <input type="text" id="cfgModel" placeholder="gpt-4o-mini">
      </label>
      <label id="cfgBaseUrlRow" style="display:none">Base URL
        <input type="text" id="cfgBaseUrl" placeholder="https://your-endpoint/v1">
      </label>
      <div class="warning">&#x26A0;&#xFE0F; API key is stored in page memory only and is never sent to any server other than the configured provider.</div>
    </div>
  </div>
  <!-- History -->
  <div id="chatHistory">
    <div class="chat-msg assistant">
      <div class="bubble">Welcome to <strong>Graph-RAG Chat</strong>.<br>Ask any question about the knowledge graph. I will find relevant entities, extract connecting paths, and highlight them directly on the 3D graph.
        <div id="suggestedQueries">
          <span class="sq-label">Suggested queries — click to ask:</span>
        </div>
      </div>
    </div>
  </div>
  <!-- Input -->
  <div id="chatInputArea">
    <textarea id="queryInput" placeholder="Ask about the knowledge graph… (Ctrl+Enter to send)"></textarea>
    <div id="chatBtns">
      <button id="sendBtn">Send</button>
      <button id="clearHlBtn">Clear Highlights</button>
      <button id="clearChatBtn">Clear Chat</button>
    </div>
  </div>
</div>



<script>
    const data = {
        nodes: )HTML" << nodes_json.dump() << R"HTML(,
        hyperedges: )HTML" << hyperedges_json.dump() << R"HTML(
    };

    const augData = )HTML" << safe_aug.dump() << R"HTML(;

    const EMBEDDED_CFG = {
        provider: ")HTML" << llm_provider << R"HTML(",
        api_key:  ")HTML" << llm_api_key << R"HTML(",
        model:    ")HTML" << llm_model << R"HTML(",
        base_url: ")HTML" << llm_base_url << R"HTML("
    };

    // Reify hyperedges: each entry in data.hyperedges becomes a relation node in data.nodes
    // and two binary links in data.links (entity→relation, relation→entity).
    // Mirrors the graph.json on-disk format (nodes + hyperedges with sources[]/targets[]).
    function reifyHyperedges(d) {
        const idToIdx = new Map();
        d.nodes.forEach((n, i) => idToIdx.set(n.id, i));
        d.links = [];
        let idx = d.nodes.length;
        for (const he of (d.hyperedges || [])) {
            const relNode = {
                id: he.id, label: he.relation, type: 'relation',
                confidence: he.confidence,
                source_document: he.source_document || '',
                source_chunk_id: he.source_chunk_id || '',
                sources: he.sources, targets: he.targets
            };
            if (he.properties) relNode.properties = he.properties;
            d.nodes.push(relNode);
            const relIdx = idx++;
            idToIdx.set(he.id, relIdx);
            for (const src of (he.sources || [])) {
                const si = idToIdx.get(src);
                if (si !== undefined) d.links.push({ source: si, target: relIdx, type: 'source' });
            }
            for (const tgt of (he.targets || [])) {
                const ti = idToIdx.get(tgt);
                if (ti !== undefined) d.links.push({ source: relIdx, target: ti, type: 'target' });
            }
        }
    }
    reifyHyperedges(data);

    (() => {
        const graphDiv  = document.getElementById('graph');
        const controls  = document.getElementById('controls');

        if (typeof ForceGraph3D !== 'function' || typeof THREE === 'undefined') {
            alert('Missing WebGL libs. Serve via http://localhost.');
            return;
        }

        const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
        const ric   = window.requestIdleCallback || ((cb) => setTimeout(() => cb({ timeRemaining: () => 0 }), 0));
        const rafThrottle = (fn) => {
            let scheduled = false;
            return (...args) => {
                if (scheduled) return;
                scheduled = true;
                requestAnimationFrame(() => { scheduled = false; fn(...args); });
            };
        };

        // Overlay
        let overlay = document.getElementById('kgOverlay');
        if (!overlay) {
            overlay = document.createElement('div');
            overlay.id = 'kgOverlay';
            overlay.innerHTML = '<div class="panel"><div class="title">Working...</div><div class="sub">Building subgraph</div></div>';
            document.body.appendChild(overlay);
        }
        function showOverlay(t, s) {
            const tEl = overlay.querySelector('.title'); const sEl = overlay.querySelector('.sub');
            if (tEl) tEl.textContent = t||'Working...';
            if (sEl) sEl.textContent = s||'';
            overlay.classList.add('show');
        }
        function hideOverlay() { overlay.classList.remove('show'); }

        let hud = document.getElementById('kgHud');
        if (!hud) {
            hud = document.createElement('div'); hud.id = 'kgHud';
            hud.textContent = '3D: initializing...';
            document.body.appendChild(hud);
        }

        // ── Inject left-panel controls (identical to graph_augmented) ──
        controls.insertAdjacentHTML('beforeend', `
            <div class="kg-row">
                <input class="kg-input" id="kgSearch" placeholder="Search node..." />
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
                    <select id="kgClusterMode" class="kg-input" style="width:180px;">
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
                    Min cluster size: <span id="kgMinClusterVal">5</span>
                    <input type="range" id="kgMinCluster" min="3" max="200" step="1" value="5">
                </label>
                <label>
                    Palette:
                    <select id="kgPalette" class="kg-input" style="width:180px;">
                        <option value="classic" selected>Classic</option>
                        <option value="pastel">Pastel</option>
                        <option value="neon">Neon</option>
                        <option value="mono">Mono + accent</option>
                    </select>
                </label>
                <div class="kg-row">
                    <button class="kg-btn" id="kgRecluster">Cluster now</button>
                    <label style="margin:0;"><input type="checkbox" id="kgAutoCluster" checked> Auto</label>
                </div>
            </div>

            <div class="aug-section">
                <label style="font-weight:600; color:#e040fb;">Augmentation Layer</label>
                <label style="margin-top:8px;"><input type="checkbox" id="showAug"> Show Augmentations</label>
                <label>
                    Aug Opacity: <span id="augOpacityVal">1.0</span>
                    <input type="range" id="augOpacity" min="0.1" max="1" step="0.1" value="1.0">
                </label>
                <label><input type="checkbox" id="augOnlyMode"> Augmentation-only mode</label>
                <div class="kg-row">
                    <button class="kg-btn" id="focusAug">Focus Aug</button>
                </div>
                <div id="augDetails">
                    <strong>Insight Details</strong>
                    <div id="augInfo">Click an augmentation node.</div>
                </div>
            </div>

            <div id="kgDetails" style="margin-top:10px; font-size:0.82em; color:#cfcfcf; line-height:1.35;"></div>
        `);

        // ── DOM refs ──
        const minDegreeEl       = document.getElementById('minDegree');
        const degreeValueEl     = document.getElementById('degreeValue');
        const linkStrengthEl    = document.getElementById('linkStrength');
        const repulsionEl       = document.getElementById('repulsion');
        const showLabelsEl      = document.getElementById('showLabels');
        const searchEl          = document.getElementById('kgSearch');
        const goBtn             = document.getElementById('kgGo');
        const exp1Btn           = document.getElementById('kgExpand1');
        const exp2Btn           = document.getElementById('kgExpand2');
        const resetBtn          = document.getElementById('kgReset');
        const camResetBtn       = document.getElementById('kgCamReset');
        const autoFreezeEl      = document.getElementById('kgAutoFreeze');
        const arrowsEl          = document.getElementById('kgArrows');
        const maxNodesEl        = document.getElementById('kgMaxNodes');
        const maxNodesValEl     = document.getElementById('kgMaxNodesVal');
        const depthEl           = document.getElementById('kgDepth');
        const depthValEl        = document.getElementById('kgDepthVal');
        const detailsEl         = document.getElementById('kgDetails');
        const clusterOnEl       = document.getElementById('kgClusterOn');
        const clusterModeEl     = document.getElementById('kgClusterMode');
        const clusterRadiusEl   = document.getElementById('kgClusterRadius');
        const clusterRadiusValEl= document.getElementById('kgClusterRadiusVal');
        const minClusterEl      = document.getElementById('kgMinCluster');
        const minClusterValEl   = document.getElementById('kgMinClusterVal');
        const topoResEl         = document.getElementById('kgTopoRes');
        const topoResValEl      = document.getElementById('kgTopoResVal');
        const paletteEl         = document.getElementById('kgPalette');
        const reclusterBtn      = document.getElementById('kgRecluster');
        const autoClusterEl     = document.getElementById('kgAutoCluster');
        const topoResRow        = document.getElementById('kgTopoResRow');
        const spatialRadiusRow  = document.getElementById('kgSpatialRadiusRow');
        const spatialMinRow     = document.getElementById('kgSpatialMinRow');
        const showAugEl         = document.getElementById('showAug');
        const augOpacityEl      = document.getElementById('augOpacity');
        const augOpacityValEl   = document.getElementById('augOpacityVal');
        const augOnlyEl         = document.getElementById('augOnlyMode');
        const focusAugBtn       = document.getElementById('focusAug');
        const augDetailsEl      = document.getElementById('augDetails');
        const augInfoEl         = document.getElementById('augInfo');

        // ── Index base nodes ──
        const N = data.nodes.length;
        for (let i = 0; i < N; i++) { data.nodes[i].gid = i; data.nodes[i].isAug = false; }

        let edges   = new Array(data.links.length);
        let incident= Array.from({ length: N }, () => []);
        for (let i = 0; i < data.links.length; i++) {
            const l = data.links[i];
            const s = +((typeof l.source==='object') ? l.source.index : l.source);
            const t = +((typeof l.target==='object') ? l.target.index : l.target);
            edges[i] = { s, t, type: l.type, isAug: false };
            incident[s].push(i); incident[t].push(i);
        }

        const entityByDegree = [];
        for (let i=0;i<N;i++) if (data.nodes[i].type==='entity') entityByDegree.push(i);
        entityByDegree.sort((a,b)=>((data.nodes[b].degree||0)-(data.nodes[a].degree||0)));

        let labelsLower = data.nodes.map(n=>(n.label||n.id||'').toLowerCase());
        let augmentationMerged = false;
        const nodeIdToIdx = new Map();
        for (let i=0;i<N;i++) nodeIdToIdx.set(data.nodes[i].id, i);

        function mergeAugmentation() {
            if (augmentationMerged) return;
            augmentationMerged = true;
            const augNodeIdToIdx = new Map();
            for (const an of augData.nodes||[]) {
                const ni = data.nodes.length;
                data.nodes.push({ gid:ni, id:an.id, label:an.label, type:'augmentation',
                    isAug:true, insightId:an.insight_id, confidence:an.confidence,
                    evidenceChunks:an.evidence_chunk_ids||[], witnessEdges:an.witness_edges||[] });
                augNodeIdToIdx.set(an.id, ni);
            }
            for (const al of augData.links||[]) {
                let s = nodeIdToIdx.get(al.source);
                if (s===undefined) s = augNodeIdToIdx.get(al.source);
                let t = nodeIdToIdx.get(al.target);
                if (t===undefined) t = augNodeIdToIdx.get(al.target);
                if (s!==undefined && t!==undefined)
                    data.links.push({ source:s, target:t, type:al.type, isAug:true });
            }
            edges = new Array(data.links.length);
            incident = Array.from({ length:data.nodes.length }, ()=>[]);
            for (let i=0;i<data.links.length;i++) {
                const l=data.links[i];
                const s=+((typeof l.source==='object')?l.source.index:l.source);
                const t=+((typeof l.target==='object')?l.target.index:l.target);
                edges[i]={s,t,type:l.type,isAug:!!l.isAug};
                incident[s].push(i); incident[t].push(i);
            }
            labelsLower = data.nodes.map(n=>(n.label||n.id||'').toLowerCase());
        }

        // ── State ──
        const state = {
            minDegree: +minDegreeEl.value,
            linkStrength: +linkStrengthEl.value,
            repulsion: +repulsionEl.value,
            showLabels: !!showLabelsEl.checked,
            autoFreeze: true, showArrows: false,
            maxNodes: +maxNodesEl.value,
            depthSpread: +depthEl.value,
            selectedGid: null,
            visible: new Set(),
            subNodes: [], subLinks: [],
            subNodesByGid: new Map(),
            graph: null,
            fps: { t0: performance.now(), frames:0, value:0 },
            clusterOn: true, clusterMode: 'topology',
            clusterRadius: 40, minClusterSize: 5,
            topoResolution: 1.0, palette: 'classic',
            autoCluster: true, clusters: [], clusterLabels: [],
            showAug: true, augOpacity: 1.0, augOnlyMode: true,
            degreeDebounce: null,
            // ── Graph-RAG highlight state ──
            hlNodeGids: new Set(), highlightActive: false
        };

        function applyDepth() {
            const sp = state.depthSpread;
            for (let i=0;i<data.nodes.length;i++) {
                const n=data.nodes[i];
                const base = n.isAug ? 0.6 : (n.type==='relation' ? 0.45 : 1.0);
                n.z = (Math.random()-0.5)*sp*base;
                if (n.isAug) n.z += sp*0.3;
            }
        }

        function seedVisibleByDegree(minDeg, maxN) {
            const vis = new Set();
            if (state.showAug || state.augOnlyMode) {
                for (let i=0;i<data.nodes.length;i++) if (data.nodes[i].isAug) vis.add(i);
            }
            if (state.augOnlyMode) {
                for (let i=0;i<data.nodes.length;i++) {
                    if (data.nodes[i].isAug && incident[i]) {
                        vis.add(i);
                        for (const ei of incident[i]) {
                            const e=edges[ei]; if (!e) continue;
                            vis.add(e.s===i ? e.t : e.s);
                        }
                    }
                }
                return vis;
            }
            for (let k=0;k<entityByDegree.length&&vis.size<maxN;k++) {
                const idx=entityByDegree[k], n=data.nodes[idx];
                if ((n.degree||0)>=minDeg) vis.add(idx); else break;
            }
            if (vis.size<50) {
                for (let k=0;k<Math.min(200,entityByDegree.length)&&vis.size<Math.min(maxN,200);k++)
                    vis.add(entityByDegree[k]);
            }
            const relQ=[];
            for (const idx of vis) {
                if (!incident[idx]) continue;
                for (const ei of incident[idx]) {
                    const e=edges[ei]; if (!e) continue;
                    const other=e.s===idx?e.t:e.s;
                    if (data.nodes[other]&&(data.nodes[other].type==='relation'||data.nodes[other].isAug)&&!vis.has(other))
                        relQ.push(other);
                }
            }
            for (let i=0;i<relQ.length&&vis.size<maxN;i++) vis.add(relQ[i]);
            if (state.selectedGid!=null) vis.add(state.selectedGid);
            return vis;
        }

        function buildSubgraphFromVisible() {
            const sn=[];
            state.visible.forEach(i=>{ if(data.nodes[i]) sn.push(data.nodes[i]); });
            const eids=new Set();
            for (const i of state.visible) if (incident[i]) for (const ei of incident[i]) eids.add(ei);
            const sl=[];
            eids.forEach(ei=>{
                const e=edges[ei]; if(!e) return;
                if (state.visible.has(e.s)&&state.visible.has(e.t)) {
                    if (e.isAug&&!state.showAug) return;
                    sl.push(e);
                }
            });
            state.subNodes=sn; state.subLinks=sl;
            const vn=document.getElementById('visibleNodes');
            const vl=document.getElementById('visibleLinks');
            if(vn) vn.textContent=String(sn.length);
            if(vl) vl.textContent=String(sl.length);
        }

        function rebuildSubgraphIndex() {
            state.subNodesByGid=new Map();
            for (const n of state.subNodes) state.subNodesByGid.set(n.gid,n);
        }

        function escapeHtml(s) {
            return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
        }

        function updateDetails(node) {
            if (!detailsEl) return;
            if (!node) { detailsEl.innerHTML='<span style="opacity:.8">Click a node for details.</span>'; augDetailsEl.classList.remove('show'); return; }
            const lines=[];
            lines.push(`<div><b>${escapeHtml(node.label||node.id||'Node')}</b></div>`);
            lines.push(`<div style="opacity:.85">type: <code>${escapeHtml(node.type||'')}</code></div>`);
            if (node.isAug) {
                augDetailsEl.classList.add('show');
                let h=`<strong>${escapeHtml(node.label)}</strong><br>`;
                if (node.insightId) h+=`Insight: ${escapeHtml(node.insightId)}<br>`;
                if (typeof node.confidence==='number') h+=`Confidence: ${node.confidence.toFixed(3)}<br>`;
                if (node.evidenceChunks&&node.evidenceChunks.length) h+=`Evidence: ${escapeHtml(node.evidenceChunks.slice(0,3).join(', '))}${node.evidenceChunks.length>3?'...':''}`;
                augInfoEl.innerHTML=h;
            } else {
                augDetailsEl.classList.remove('show');
                if (typeof node.degree==='number') lines.push(`<div style="opacity:.85">degree: <code>${node.degree}</code></div>`);
                if (node.type==='relation') {
                    if (node.sources?.length) lines.push(`<div style="opacity:.85">sources: <code>${escapeHtml(node.sources.slice(0,8).join(', '))}${node.sources.length>8?' ...':''}</code></div>`);
                    if (node.targets?.length) lines.push(`<div style="opacity:.85">targets: <code>${escapeHtml(node.targets.slice(0,8).join(', '))}${node.targets.length>8?' ...':''}</code></div>`);
                    if (typeof node.confidence==='number') lines.push(`<div style="opacity:.85">confidence: <code>${node.confidence.toFixed(3)}</code></div>`);
                    if (node.source_document) lines.push(`<div style="opacity:.85">document: <code>${escapeHtml(node.source_document)}</code></div>`);
                    if (node.source_chunk_id) lines.push(`<div style="opacity:.85">chunk: <code>${escapeHtml(node.source_chunk_id)}</code></div>`);
                    if (typeof node.source_page==='number'&&node.source_page>=0) lines.push(`<div style="opacity:.85">page: <code>${node.source_page}</code></div>`);
                    if (node.properties&&Object.keys(node.properties).length) {
                        const ps=Object.entries(node.properties).slice(0,5).map(([k,v])=>`${escapeHtml(k)}: ${escapeHtml(String(v))}`).join('<br>');
                        lines.push(`<div style="opacity:.85;margin-top:4px">properties:<br><code>${ps}</code></div>`);
                    }
                } else {
                    // Entity node
                    if (node.docs&&node.docs.length) lines.push(`<div style="opacity:.85">documents: <code>${escapeHtml(node.docs.slice(0,3).join(', '))}${node.docs.length>3?' ...':''}</code></div>`);
                    if (node.chunks&&node.chunks.length) lines.push(`<div style="opacity:.85">chunks: <code>${escapeHtml(node.chunks.slice(0,3).join(', '))}${node.chunks.length>3?' ...':''}</code></div>`);
                    if (node.properties&&Object.keys(node.properties).length) {
                        const ps=Object.entries(node.properties).slice(0,5).map(([k,v])=>`${escapeHtml(k)}: ${escapeHtml(String(v))}`).join('<br>');
                        lines.push(`<div style="opacity:.85;margin-top:4px">properties:<br><code>${ps}</code></div>`);
                    }
                }
            }
            detailsEl.innerHTML=lines.join('');
        }

        function expandSelected(hops) {
            if (state.selectedGid==null) return;
            const q=[state.selectedGid], dist=new Map([[state.selectedGid,0]]);
            while (q.length) {
                const u=q.shift(), d=dist.get(u)||0;
                if (d>=hops) continue;
                for (const ei of incident[u]||[]) {
                    const e=edges[ei], v=e.s===u?e.t:e.s;
                    if (!dist.has(v)) { dist.set(v,d+1); q.push(v); }
                    if (state.visible.size<state.maxNodes) state.visible.add(v);
                }
                if (state.visible.size>=state.maxNodes) break;
            }
            rebuildAndRender('Expanding neighborhood...');
        }

        // ── Graph init ──
        graphDiv.innerHTML='';
        const Graph = ForceGraph3D()(graphDiv)
            .nodeId('gid')
            .nodeLabel(n=>(n.label||n.id||''))
            .backgroundColor('rgba(0,0,0,0)')
            .showNavInfo(false)
            .enableNodeDrag(true)
            .linkSource('source')
            .linkTarget('target')
            .cooldownTime(2000)
            .warmupTicks(0);
        state.graph = Graph;

        // ── Colour palettes (identical to graph_augmented) ──
        const PALETTES = {
            classic:['#4fc3f7','#ff9800','#ab47bc','#66bb6a','#ef5350','#ffa726','#26c6da','#8d6e63','#78909c','#d4e157'],
            pastel: ['#a3d5ff','#ffd6a5','#bdb2ff','#caffbf','#ffadad','#fdffb6','#9bf6ff','#ffc6ff','#b8f2e6','#f1c0e8'],
            neon:   ['#00e5ff','#ffea00','#ff1744','#76ff03','#e040fb','#ff9100','#1de9b6','#f500ff','#00c853','#2979ff'],
            mono:   ['#4fc3f7','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc']
        };
        function getClusterColor(cid) {
            if (cid==null||cid<0) return '#4fc3f7';
            const pal=PALETTES[state.palette]||PALETTES.classic;
            return pal[cid%pal.length];
        }

        // ── Cluster label sprites ──
        function clearClusterLabels() {
            if (!state.clusterLabels?.length) return;
            const sc=Graph.scene();
            for (const s of state.clusterLabels) sc.remove(s);
            state.clusterLabels=[];
        }
        function drawClusterLabels() {
            clearClusterLabels();
            if (!state.clusterOn||typeof SpriteText==='undefined') return;
            const sc=Graph.scene();
            for (const cl of state.clusters) {
                const rep=state.subNodesByGid.get(cl.repGid); if (!rep) continue;
                const sp=new SpriteText(rep.label||rep.id||`C${cl.id}`);
                sp.fontFace='Arial Black'; sp.textHeight=10;
                sp.color='rgba(255,255,255,0.95)'; sp.backgroundColor='rgba(0,0,0,0.50)';
                sp.padding=6; sp.borderRadius=10; sp.material.depthWrite=false;
                sp.position.set(rep.x||0,(rep.y||0)+18,rep.z||0);
                sc.add(sp); state.clusterLabels.push(sp);
            }
        }
        function chooseRepresentative(gids) {
            let cx=0,cy=0,cz=0,cnt=0;
            for (const gid of gids) {
                const n=state.subNodesByGid.get(gid); if(!n) continue;
                cx+=(n.x||0); cy+=(n.y||0); cz+=(n.z||0); cnt++;
            }
            if(!cnt) return gids[0]??null;
            cx/=cnt; cy/=cnt; cz/=cnt;
            let bestGid=null,bestD=Infinity;
            for (const gid of gids) {
                const n=state.subNodesByGid.get(gid); if(!n||n.type!=='entity') continue;
                const d=(n.x||0-cx)**2+(n.y||0-cy)**2+(n.z||0-cz)**2;
                if(d<bestD){bestD=d;bestGid=gid;}
            }
            if(bestGid!=null) return bestGid;
            for (const gid of gids) {
                const n=state.subNodesByGid.get(gid); if(!n) continue;
                const d=(n.x||0-cx)**2+(n.y||0-cy)**2+(n.z||0-cz)**2;
                if(d<bestD){bestD=d;bestGid=gid;}
            }
            return bestGid;
        }

        // ── DBSCAN spatial clustering ──
        function clusterSpatialDBSCAN() {
            const nodes=state.subNodes.filter(n=>!n.isAug);
            const n=nodes.length;
            const labels=new Array(n).fill(0);
            const eps2=state.clusterRadius*state.clusterRadius;
            let cid=0;
            function rq(i) {
                const ni=nodes[i],xi=ni.x||0,yi=ni.y||0,zi=ni.z||0,res=[];
                for(let j=0;j<n;j++){const nj=nodes[j];const dx=(nj.x||0)-xi,dy=(nj.y||0)-yi,dz=(nj.z||0)-zi;if(dx*dx+dy*dy+dz*dz<=eps2)res.push(j);}
                return res;
            }
            function expand(i,nb,c){
                labels[i]=c;
                for(let k=0;k<nb.length;k++){const j=nb[k];if(labels[j]===-1)labels[j]=c;if(labels[j]!==0)continue;labels[j]=c;const n2=rq(j);if(n2.length>=state.minClusterSize)for(const x of n2)nb.push(x);}
            }
            for(let i=0;i<n;i++){
                if(labels[i]!==0)continue;
                const nb=rq(i);
                if(nb.length<state.minClusterSize){labels[i]=-1;}else{cid++;expand(i,nb,cid);}
            }
            for(let i=0;i<n;i++) nodes[i].__cluster=labels[i]>0?(labels[i]-1):-1;
            for(const node of state.subNodes) if(node.isAug) node.__cluster=-1;
            const groups=new Map();
            for(const node of nodes){const c=node.__cluster??-1;if(c<0)continue;if(!groups.has(c))groups.set(c,[]);groups.get(c).push(node.gid);}
            state.clusters=[];
            for(const[id,gids]of groups.entries()){const rep=chooseRepresentative(gids);state.clusters.push({id,nodes:gids,repGid:rep});}
        }

        // ── Louvain topology clustering ──
        function clusterTopologyLouvain() {
            const nodes=state.subNodes.filter(n=>!n.isAug);
            const links=state.subLinks.filter(l=>!l.isAug);
            const idxByGid=new Map(); for(let i=0;i<nodes.length;i++) idxByGid.set(nodes[i].gid,i);
            const adj=Array.from({length:nodes.length},()=>new Map());
            for(const e of links){const a=idxByGid.get(e.s),b=idxByGid.get(e.t);if(a==null||b==null||a===b)continue;adj[a].set(b,(adj[a].get(b)||0)+1);adj[b].set(a,(adj[b].get(a)||0)+1);}
            // Project bipartite entity->relation->entity to entity co-occurrence for Louvain
            for(let ri=0;ri<nodes.length;ri++){if(nodes[ri].type!=='relation')continue;const en=[...adj[ri].keys()].filter(j=>nodes[j].type==='entity');for(let p=0;p<en.length;p++)for(let q=p+1;q<en.length;q++){const a=en[p],b=en[q];adj[a].set(b,(adj[a].get(b)||0)+1);adj[b].set(a,(adj[b].get(a)||0)+1);}}
            const res=state.topoResolution,nn=nodes.length;
            let community=Array.from({length:nn},(_,i)=>i);
            const k=new Array(nn).fill(0);let m2=0;
            for(let i=0;i<nn;i++){let s=0;for(const w of adj[i].values())s+=w;k[i]=s;m2+=s;}
            if(m2===0){for(const node of state.subNodes)node.__cluster=-1;state.clusters=[];return;}
            let tot=new Array(nn).fill(0);for(let i=0;i<nn;i++)tot[community[i]]+=k[i];
            let changed=true,iter=0;
            while(changed&&iter<30){changed=false;iter++;
                for(let i=0;i<nn;i++){
                    const ci=community[i],ki=k[i];tot[ci]-=ki;
                    const ncw=new Map();for(const[j,w]of adj[i].entries())ncw.set(community[j],(ncw.get(community[j])||0)+w);
                    let bestC=ci,bestG=0;
                    for(const[c,wic]of ncw.entries()){const gain=wic-res*ki*(tot[c])/m2;if(gain>bestG){bestG=gain;bestC=c;}}
                    community[i]=bestC;tot[bestC]+=ki;if(bestC!==ci)changed=true;
                }
            }
            const remap=new Map();let nextId=0;
            for(let i=0;i<nn;i++){const c=community[i];if(!remap.has(c))remap.set(c,nextId++);nodes[i].__cluster=remap.get(c);}
            for(const node of state.subNodes)if(node.isAug)node.__cluster=-1;
            const groups=new Map();
            for(const node of nodes){const c=node.__cluster??-1;if(c<0)continue;if(!groups.has(c))groups.set(c,[]);groups.get(c).push(node.gid);}
            state.clusters=[];
            for(const[id,gids]of groups.entries()){const rep=chooseRepresentative(gids);state.clusters.push({id,nodes:gids,repGid:rep});}
        }

        function runClustering() {
            if(state.clusterMode==='spatial') clusterSpatialDBSCAN();
            else clusterTopologyLouvain();
            applyStyles(); drawClusterLabels();
        }

        // ── applyStyles: augmented renderer colours + RAG highlight overlay ──
        function applyStyles() {
            const showAug=state.showAug, op=state.augOpacity, clOn=!!state.clusterOn;
            const hl=state.highlightActive, hlG=state.hlNodeGids;

            Graph
              .nodeColor(n => {
                if (hl) {
                    if (hlG.has(n.gid)) return '#FFD700';
                    return n.isAug ? `rgba(224,64,251,0.15)` : 'rgba(79,195,247,0.18)';
                }
                if (n.isAug) return showAug ? `rgba(224,64,251,${op})` : 'rgba(224,64,251,0.1)';
                if (clOn) { const c=n.__cluster??-1; return getClusterColor(c); }
                return n.type==='relation' ? '#ff9800' : '#4fc3f7';
              })
              .nodeVal(n => {
                if (hl && hlG.has(n.gid)) return 8;
                if (n.isAug) return showAug ? 4 : 1;
                return n.type==='relation' ? 1.5 : Math.max(1.2, 1+(n.degree||0)*0.06);
              })
              .linkColor(l => {
                if (hl) {
                    const ag=l.source?.gid??l.source, bg=l.target?.gid??l.target;
                    if (hlG.has(ag) && hlG.has(bg)) return '#FF6B35';
                    return 'rgba(80,80,80,0.12)';
                }
                if (l.isAug) return showAug ? `rgba(224,64,251,${op*0.7})` : 'rgba(224,64,251,0.05)';
                if (clOn) {
                    const a=state.subNodesByGid.get(l.source)||state.subNodesByGid.get(l.source?.gid)||null;
                    const b=state.subNodesByGid.get(l.target)||state.subNodesByGid.get(l.target?.gid)||null;
                    const ca=a?(a.__cluster??-1):-1, cb=b?(b.__cluster??-1):-1;
                    if(ca>=0&&ca===cb) return getClusterColor(ca);
                    return 'rgba(255,255,255,0.10)';
                }
                return l.type==='source' ? 'rgba(79,195,247,0.55)' : 'rgba(255,152,0,0.55)';
              })
              .linkWidth(l => {
                if (hl) {
                    const ag=l.source?.gid??l.source, bg=l.target?.gid??l.target;
                    return hlG.has(ag)&&hlG.has(bg) ? 2.5 : 0.2;
                }
                if (l.isAug) return 1.5;
                if (clOn) {
                    const a=state.subNodesByGid.get(l.source)||state.subNodesByGid.get(l.source?.gid)||null;
                    const b=state.subNodesByGid.get(l.target)||state.subNodesByGid.get(l.target?.gid)||null;
                    const ca=a?(a.__cluster??-1):-1, cb=b?(b.__cluster??-1):-1;
                    return (ca>=0&&ca===cb) ? 0.9 : 0.35;
                }
                return 0.6;
              })
              .linkDirectionalArrowLength(state.showArrows?3:0)
              .linkDirectionalArrowRelPos(0.5);
            if (Graph.linkLineDash) Graph.linkLineDash(l=>l.isAug?[2,2]:null);
            Graph.refresh();
        }

        function applyLabels() {
            state.showLabels=!!showLabelsEl.checked;
            if(typeof SpriteText==='undefined') return;
            Graph.nodeThreeObject(node=>{
                if(!state.showLabels) return null;
                const isSel=(state.selectedGid!=null&&node.gid===state.selectedGid);
                const important=(node.type==='entity'&&(node.degree||0)>=Math.max(40,state.minDegree));
                if(!isSel&&!important&&!node.isAug) return null;
                const sp=new SpriteText(node.label||node.id||'');
                sp.textHeight=isSel?6:(node.isAug?5:4);
                sp.color=node.isAug?'rgba(224,64,251,0.95)':'rgba(255,255,255,0.9)';
                sp.backgroundColor='rgba(0,0,0,0.35)'; sp.padding=2; sp.borderRadius=6;
                sp.material.depthWrite=false; sp.position.y=8; return sp;
            }).nodeThreeObjectExtend(true);
            Graph.refresh();
        }

        function applyForces() {
            if(Graph.d3Force('charge')) Graph.d3Force('charge').strength(-state.repulsion);
            if(Graph.d3Force('link'))   Graph.d3Force('link').strength(state.linkStrength);
        }

        function rebuildAndRender(msg) {
            showOverlay(msg||'Updating...','Filtering & rebuilding visible subgraph');
            ric(()=>{
                buildSubgraphFromVisible(); rebuildSubgraphIndex();
                const gdata={nodes:state.subNodes,links:state.subLinks.map(e=>({source:e.s,target:e.t,type:e.type,isAug:e.isAug}))};
                Graph.graphData(gdata); applyForces();
                if(state.autoFreeze) Graph.cooldownTime(2000); else Graph.cooldownTime(Infinity);
                applyStyles(); applyLabels(); Graph.d3ReheatSimulation();
                if(state.autoCluster&&state.clusterOn) setTimeout(()=>runClustering(),1400);
                hideOverlay(); updateHud();
            });
        }

        function rebuildFromDegree() {
            showOverlay('Seeding visible subgraph...','');
            ric(()=>{ state.visible=seedVisibleByDegree(state.minDegree,state.maxNodes); if(state.selectedGid!=null)state.visible.add(state.selectedGid); rebuildAndRender('Rendering subgraph...'); });
        }

        function updateHud() {
            const now=performance.now(); state.fps.frames++;
            const dt=now-state.fps.t0;
            if(dt>=800){state.fps.value=Math.round(state.fps.frames*1000/dt);state.fps.frames=0;state.fps.t0=now;}
            const ac=state.subNodes.filter(n=>n.isAug).length;
            const hlStr=state.highlightActive?' | \u2726 Highlights':'';
            hud.textContent=`3D | ${state.subNodes.length} nodes | ${state.subLinks.length} links | ${ac} aug | ${state.fps.value} fps${hlStr}`;
        }

        function resetCamera() { Graph.cameraPosition({x:0,y:0,z:420},{x:0,y:0,z:0},700); }

        // ── Event wiring (left panel) ──
        Graph.onNodeClick(node=>{ state.selectedGid=node?node.gid:null; updateDetails(node); });

        minDegreeEl.addEventListener('input',()=>{
            state.minDegree=+minDegreeEl.value; degreeValueEl.textContent=String(state.minDegree);
            if(state.degreeDebounce) clearTimeout(state.degreeDebounce);
            state.degreeDebounce=setTimeout(()=>rebuildFromDegree(),150);
        });
        linkStrengthEl.addEventListener('input',()=>{state.linkStrength=+linkStrengthEl.value;applyForces();Graph.d3ReheatSimulation();});
        repulsionEl.addEventListener('input',()=>{state.repulsion=+repulsionEl.value;applyForces();Graph.d3ReheatSimulation();});
        maxNodesEl.addEventListener('input',()=>{ state.maxNodes=+maxNodesEl.value; maxNodesValEl.textContent=String(state.maxNodes); if(state.maxNodesDebounce)clearTimeout(state.maxNodesDebounce); state.maxNodesDebounce=setTimeout(()=>rebuildFromDegree(),200); });
        depthEl.addEventListener('input',()=>{ state.depthSpread=+depthEl.value; depthValEl.textContent=String(state.depthSpread); applyDepth(); Graph.refresh(); });
        goBtn.addEventListener('click',()=>{
            const q=(searchEl.value||'').trim().toLowerCase(); if(!q) return;
            let idx=labelsLower.indexOf(q);
            if(idx===-1) for(let i=0;i<labelsLower.length;i++){if(labelsLower[i].includes(q)){idx=i;break;}}
            if(idx===-1){if(detailsEl)detailsEl.innerHTML=`<div style="color:#ffb74d;">No match for "${escapeHtml(q)}".</div>`;return;}
            state.selectedGid=idx; state.visible.add(idx); rebuildAndRender('Searching...'); updateDetails(data.nodes[idx]);
        });
        searchEl.addEventListener('keydown',e=>{if(e.key==='Enter')goBtn.click();});
        exp1Btn.addEventListener('click',()=>expandSelected(1));
        exp2Btn.addEventListener('click',()=>expandSelected(2));
        resetBtn.addEventListener('click',()=>{state.selectedGid=null;updateDetails(null);rebuildAndRender('Resetting view...');});
        camResetBtn.addEventListener('click',resetCamera);
        showAugEl.addEventListener('change',()=>{state.showAug=showAugEl.checked;if(state.showAug)mergeAugmentation();rebuildAndRender('Toggling augmentation...');});
        augOpacityEl.addEventListener('input',()=>{state.augOpacity=+augOpacityEl.value;augOpacityValEl.textContent=state.augOpacity.toFixed(1);applyStyles();});
        augOnlyEl.addEventListener('change',()=>{
            state.augOnlyMode=augOnlyEl.checked;
            if(state.augOnlyMode){mergeAugmentation();state.showAug=true;showAugEl.checked=true;}
            else{state.showAug=false;if(showAugEl)showAugEl.checked=false;}
            rebuildFromDegree();
        });
        focusAugBtn.addEventListener('click',()=>{
            const an=state.subNodes.filter(n=>n.isAug); if(!an.length) return;
            let cx=0,cy=0,cz=0;for(const n of an){cx+=n.x||0;cy+=n.y||0;cz+=n.z||0;}
            cx/=an.length;cy/=an.length;cz/=an.length;
            Graph.cameraPosition({x:cx,y:cy,z:cz+300},{x:cx,y:cy,z:cz},1000);
        });
        arrowsEl.addEventListener('change',()=>{state.showArrows=!!arrowsEl.checked;applyStyles();});
        showLabelsEl.addEventListener('change',()=>applyLabels());
        clusterOnEl.addEventListener('change',()=>{state.clusterOn=!!clusterOnEl.checked;if(state.clusterOn)runClustering();else{clearClusterLabels();applyStyles();}});
        clusterModeEl.addEventListener('change',()=>{
            state.clusterMode=clusterModeEl.value;
            topoResRow.style.display=state.clusterMode==='topology'?'':'none';
            spatialRadiusRow.style.display=state.clusterMode==='spatial'?'':'none';
            spatialMinRow.style.display=state.clusterMode==='spatial'?'':'none';
            if(state.clusterOn) runClustering();
        });
        clusterRadiusEl.addEventListener('input',()=>{state.clusterRadius=+clusterRadiusEl.value;clusterRadiusValEl.textContent=String(state.clusterRadius);});
        clusterRadiusEl.addEventListener('change',()=>{if(state.clusterOn)runClustering();});
        minClusterEl.addEventListener('input',()=>{state.minClusterSize=+minClusterEl.value;minClusterValEl.textContent=String(state.minClusterSize);});
        minClusterEl.addEventListener('change',()=>{if(state.clusterOn)runClustering();});
        topoResEl.addEventListener('input',()=>{state.topoResolution=+topoResEl.value;topoResValEl.textContent=state.topoResolution.toFixed(2);});
        topoResEl.addEventListener('change',()=>{if(state.clusterOn)runClustering();});
        paletteEl.addEventListener('change',()=>{state.palette=paletteEl.value;applyStyles();drawClusterLabels();});
        reclusterBtn.addEventListener('click',()=>runClustering());
        autoClusterEl.addEventListener('change',()=>{state.autoCluster=!!autoClusterEl.checked;});
        autoFreezeEl.addEventListener('change',()=>{state.autoFreeze=!!autoFreezeEl.checked;Graph.cooldownTime(state.autoFreeze?2000:Infinity);Graph.d3ReheatSimulation();});

        Graph.onNodeHover(rafThrottle(node=>{
            const tt=document.getElementById('tooltip'); if(!tt) return;
            if(!node){tt.style.opacity=0;return;}
            tt.style.opacity=1;
            let info=`<strong>${escapeHtml(node.label||node.id||'')}</strong><br>`;
            info+=`<span style="opacity:.85">${escapeHtml(node.type||'')}`;
            if(typeof node.degree==='number') info+=` deg ${node.degree}`;
            if(node.isAug&&typeof node.confidence==='number') info+=` conf ${node.confidence.toFixed(2)}`;
            info+='</span>';
            tt.innerHTML=info;
            document.onmousemove=e=>{tt.style.left=Math.min(e.clientX+14,window.innerWidth-300)+'px';tt.style.top=Math.min(e.clientY+14,window.innerHeight-200)+'px';};
        }));

        if(Graph.onRenderFramePost) Graph.onRenderFramePost(updateHud);
        else setInterval(updateHud,250);

        window.addEventListener('resize',()=>{Graph.width(window.innerWidth);Graph.height(window.innerHeight-52);});

        // ════════════════════════════════════════════════════════════════════
        // GRAPH-RAG: HIGHLIGHT FUNCTIONS
        // ════════════════════════════════════════════════════════════════════
        const hlBadge  = document.getElementById('hlBadge');
        const legendHL = document.getElementById('legendHL');

        function applyHighlights(entityNames) {
            state.hlNodeGids.clear();
            if (!entityNames.length) { clearHighlights(); return; }
            const lower = entityNames.map(s=>s.toLowerCase());
            // First pass: collect directly matched nodes (entities AND relation nodes)
            const matched = new Set();
            data.nodes.forEach(n=>{
                const lbl=(n.label||n.id||'').toLowerCase();
                if (lower.some(nm=>lbl===nm||lbl.includes(nm)||nm.includes(lbl))) {
                    matched.add(n.gid);
                }
            });
            // Second pass: for matched relation nodes, also highlight their connected entity nodes
            const relGids = new Set([...matched].filter(g=>{const nn=data.nodes[g];return nn&&nn.type==='relation';}));
            if (relGids.size>0) {
                data.links.forEach(lk=>{
                    const s=lk._si??lk.s??(typeof lk.source==='object'?lk.source.gid:lk.source);
                    const t=lk._ti??lk.t??(typeof lk.target==='object'?lk.target.gid:lk.target);
                    if (relGids.has(s)) matched.add(t);
                    if (relGids.has(t)) matched.add(s);
                });
            }
            matched.forEach(g=>{ state.hlNodeGids.add(g); state.visible.add(g); });
            state.highlightActive = state.hlNodeGids.size>0;
            if (hlBadge)  hlBadge.classList.toggle('show', state.highlightActive);
            if (legendHL) legendHL.style.display = state.highlightActive?'':'none';
            buildSubgraphFromVisible(); rebuildSubgraphIndex();
            Graph.graphData({nodes:state.subNodes,links:state.subLinks.map(e=>({source:e.s,target:e.t,type:e.type,isAug:e.isAug}))});
            applyStyles();
        }

        function clearHighlights() {
            state.hlNodeGids.clear(); state.highlightActive=false;
            if (hlBadge)  hlBadge.classList.remove('show');
            if (legendHL) legendHL.style.display='none';
            applyStyles();
        }

        // ════════════════════════════════════════════════════════════════════
        // GRAPH-RAG: QUERY LOGIC
        // ════════════════════════════════════════════════════════════════════
        function findRelevantNodes(query) {
            const stop=new Set(['what','where','when','who','how','why','the','and','for','are','was','that','this','with','from','have','been','will','can','its','but','not','they','about','which','their','into','more','also','than','then','like','some','just','your','has','had']);
            const kws=query.toLowerCase().replace(/[^a-z0-9\s]/g,' ').split(/\s+/).filter(w=>w.length>2&&!stop.has(w));
            if(!kws.length) return [];
            const scores=new Map();
            data.nodes.forEach(n=>{
                const lbl=(n.label||n.id||'').toLowerCase();
                // Search properties values
                const pstr=n.properties?Object.values(n.properties).join(' ').toLowerCase():'';
                // For relation nodes also search source_document and chunk
                const docstr=((n.source_document||'')+' '+(n.source_chunk_id||'')).toLowerCase();
                // Weight multiplier: relation node matches count at 60% to prioritise entities
                const isRel=n.type==='relation';
                const w=isRel?0.6:1.0;
                let sc=0;
                kws.forEach(kw=>{
                    if(lbl===kw)sc+=5*w;
                    else if(lbl.startsWith(kw))sc+=3*w;
                    else if(lbl.includes(kw))sc+=2*w;
                    if(pstr.includes(kw))sc+=1*w;
                    if(isRel&&docstr.includes(kw))sc+=0.5;
                });
                if(sc>0) scores.set(n,sc);
            });
            return [...scores.entries()].sort((a,b)=>b[1]-a[1]).slice(0,20).map(e=>e[0]);
        }

        function findConnectingPaths(seedNodes, maxDepth, maxPaths) {
            if(seedNodes.length<2) return [];
            const seedSet=new Set(seedNodes.map(n=>n.gid));
            const adj=new Map();
            data.links.forEach(lk=>{
                const s=lk._si??lk.s??(typeof lk.source==='object'?lk.source.gid:lk.source);
                const t=lk._ti??lk.t??(typeof lk.target==='object'?lk.target.gid:lk.target);
                if(s==null||t==null) return;
                if(!adj.has(s)) adj.set(s,[]); if(!adj.has(t)) adj.set(t,[]);
                adj.get(s).push(t); adj.get(t).push(s);
            });
            // also use the edges array (post-merge)
            for(let i=0;i<edges.length;i++){const e=edges[i];if(!adj.has(e.s))adj.set(e.s,[]);if(!adj.has(e.t))adj.set(e.t,[]);adj.get(e.s).push(e.t);adj.get(e.t).push(e.s);}
            const paths=[];
            for(const start of seedSet){
                const visited=new Set([start]),queue=[[start,[start]]];
                while(queue.length&&paths.length<maxPaths){
                    const[cur,path]=queue.shift();
                    if(path.length>maxDepth+1) continue;
                    for(const nb of(adj.get(cur)||[])){
                        if(!visited.has(nb)){
                            visited.add(nb);const np=[...path,nb];
                            if(seedSet.has(nb)&&np.length>1){paths.push(np);if(paths.length>=maxPaths)break;}
                            queue.push([nb,np]);
                        }
                    }
                }
            }
            return paths;
        }

        function buildContext(relNodes, paths) {
            let ctx='=== KNOWLEDGE GRAPH CONTEXT ===\n\nRELEVANT ENTITIES:\n';
            relNodes.slice(0,25).forEach(n=>{
                ctx+=`\u2022 ${n.label||n.id}`;
                if(n.type==='relation'){
                    const conf=typeof n.confidence==='number'?`, conf:${n.confidence.toFixed(2)}`:'';
                    const doc=n.source_document?`, doc:${n.source_document}`:'';
                    const chunk=n.source_chunk_id?`, chunk:${n.source_chunk_id}`:'';
                    ctx+=` [RELATION${conf}${doc}${chunk}]`;
                } else {
                    ctx+=` (degree:${n.degree||0}`;
                    if(n.isAug) ctx+=`, insight:${n.insightId}`;
                    if(n.docs&&n.docs.length) ctx+=`, docs:${n.docs.slice(0,2).join('; ')}`;
                    ctx+=')';
                    if(n.properties){const ps=Object.entries(n.properties).slice(0,3).map(([k,v])=>`${k}:${v}`).join(', ');if(ps)ctx+=` [${ps}]`;}
                }
                ctx+='\n';
            });
            if(paths.length){
                ctx+='\nCONNECTING PATHS:\n';
                paths.slice(0,15).forEach(path=>{
                    // Detect reified relation nodes and format as annotated edges
                    const parts=[];
                    let i=0;
                    while(i<path.length){
                        const nn=data.nodes[path[i]];
                        if(!nn){parts.push(String(path[i]));i++;continue;}
                        if(nn.type==='relation'&&parts.length>0){
                            // Replace last separator with annotated edge arrow
                            const conf=typeof nn.confidence==='number'?`, conf:${nn.confidence.toFixed(2)}`:'';
                            const doc=nn.source_document?`, doc:${nn.source_document}`:'';
                            parts[parts.length-1]+=' --['+escapeHtml(nn.label)+conf+doc+']-->';
                        } else {
                            parts.push(nn.label||nn.id);
                        }
                        i++;
                    }
                    ctx+=`\u2022 ${parts.join(' ')}\n`;
                });
            }
            ctx+=`\nGRAPH STATS: ${data.nodes.filter(n=>n.type==='entity').length} entities, ${data.nodes.filter(n=>n.type==='relation').length} relations, ${data.nodes.filter(n=>n.isAug).length} insight nodes\n`;
            return ctx;
        }

        function getLLMConfig() {
            return {
                provider: document.getElementById('cfgProvider').value,
                api_key:  document.getElementById('cfgApiKey').value.trim(),
                model:    document.getElementById('cfgModel').value.trim()||'gpt-4o-mini',
                base_url: document.getElementById('cfgBaseUrl').value.trim()
            };
        }

        const SYSTEM_PROMPT='You are an AI assistant with access to a knowledge graph extracted from academic documents.\n\nGRAPH STRUCTURE:\nThis graph uses reified hyperedges. Each relation is represented as a node (type="relation") connected between entity nodes. Relation nodes carry:\n- label: the semantic relationship type (e.g. "sponsored", "translated", "influenced")\n- confidence: extraction confidence score [0,1]\n- source_document: the document this relation was extracted from\n- source_chunk_id: the specific passage/chunk\nPaths are written as: EntityA --[relation_type, conf:X.XX, doc:filename]--> EntityB\n\nRules:\n1. Reference entities by their exact names as they appear in the graph.\n2. When citing connections, state the relation type and confidence. Cite source_document as evidence.\n3. Distinguish high-confidence (>0.8) from speculative (<0.5) relations.\n4. At the END of your response, always add exactly one line:\n   HIGHLIGHTED_ENTITIES: entity1, entity2, relation_label1\n   (list the most relevant entity names AND relation labels from the graph, comma-separated, exactly as they appear)\n5. Keep your answer focused and under 500 words.';

        async function callLLM(messages) {
            const cfg=getLLMConfig();
            if(!cfg.api_key) throw new Error('No API key configured. Open LLM Settings in the chat panel.');
            if(cfg.provider==='gemini'){
                const model=cfg.model||'gemini-1.5-flash';
                const url=`https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent?key=${cfg.api_key}`;
                const sys=messages.find(m=>m.role==='system');
                const body={contents:messages.filter(m=>m.role!=='system').map(m=>({role:m.role==='assistant'?'model':'user',parts:[{text:m.content}]}))};
                if(sys) body.systemInstruction={parts:[{text:sys.content}]};
                const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
                const j=await r.json(); if(!r.ok) throw new Error(j.error?.message||`Gemini error ${r.status}`);
                return j.candidates[0].content.parts[0].text;
            }
            const base=(cfg.provider==='openai-compatible'&&cfg.base_url)?cfg.base_url.replace(/\/$/,''):'https://api.openai.com/v1';
            const r=await fetch(`${base}/chat/completions`,{method:'POST',headers:{'Content-Type':'application/json','Authorization':`Bearer ${cfg.api_key}`},body:JSON.stringify({model:cfg.model,messages,temperature:0.3,max_tokens:1200})});
            const j=await r.json(); if(!r.ok) throw new Error(j.error?.message||`OpenAI error ${r.status}`);
            return j.choices[0].message.content;
        }

        function parseHighlightedEntities(text) {
            const m=text.match(/HIGHLIGHTED_ENTITIES:\s*(.+?)(?:\n|$)/i);
            return m?m[1].split(',').map(s=>s.trim()).filter(Boolean):[];
        }
        function cleanResponse(text) { return text.replace(/\n?HIGHLIGHTED_ENTITIES:.*$/im,'').trim(); }

        // ════════════════════════════════════════════════════════════════════
        // GRAPH-RAG: CHAT PANEL WIRING
        // ════════════════════════════════════════════════════════════════════
        const chatPanel   = document.getElementById('chatPanel');
        const toggleBtn   = document.getElementById('chatToggleBtn');
        const closeBtn    = document.getElementById('chatCloseBtn');
        const settingsBtn = document.getElementById('settingsToggle');
        const settingsBody= document.getElementById('settingsBody');
        const cfgProvider = document.getElementById('cfgProvider');
        const cfgApiKey   = document.getElementById('cfgApiKey');
        const cfgModel    = document.getElementById('cfgModel');
        const cfgBaseUrl  = document.getElementById('cfgBaseUrl');
        const cfgBaseRow  = document.getElementById('cfgBaseUrlRow');
        const chatHistory = document.getElementById('chatHistory');
        const queryInput  = document.getElementById('queryInput');
        const sendBtn     = document.getElementById('sendBtn');
        const clearChatBtn= document.getElementById('clearChatBtn');
        const clearHlBtn  = document.getElementById('clearHlBtn');

        cfgProvider.value=EMBEDDED_CFG.provider||'openai';
        cfgApiKey.value  =EMBEDDED_CFG.api_key||'';
        cfgModel.value   =EMBEDDED_CFG.model||'gpt-4o-mini';
        cfgBaseUrl.value =EMBEDDED_CFG.base_url||'';
        function updateProviderUI(){cfgBaseRow.style.display=cfgProvider.value==='openai-compatible'?'':'none';}
        updateProviderUI();
        cfgProvider.addEventListener('change',updateProviderUI);

        settingsBtn.addEventListener('click',()=>{
            settingsBody.classList.toggle('open');
            document.getElementById('settingsArrow').textContent=settingsBody.classList.contains('open')?'\u25b2':'\u25bc';
        });
        toggleBtn.addEventListener('click',()=>chatPanel.classList.toggle('open'));
        closeBtn.addEventListener('click',()=>chatPanel.classList.remove('open'));
        queryInput.addEventListener('keydown',e=>{if(e.key==='Enter'&&e.ctrlKey){e.preventDefault();sendBtn.click();}});
        clearChatBtn.addEventListener('click',()=>{chatHistory.innerHTML='<div class="chat-msg assistant"><div class="bubble">Chat cleared.</div></div>';});
        clearHlBtn.addEventListener('click',clearHighlights);
        sendBtn.addEventListener('click',handleQuery);

        const conversationHistory=[];

        function appendMessage(role,text,entities){
            const div=document.createElement('div');
            div.className=`chat-msg ${role}`;
            const safe=text.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\n/g,'<br>');
            let meta=`<div class="meta">${new Date().toLocaleTimeString()}</div>`;
            if(role==='assistant'&&entities&&entities.length) meta=`<div class="meta">${new Date().toLocaleTimeString()} <button class="hl-btn" data-ents='${JSON.stringify(entities)}'>\u2728 Highlight (${entities.length})</button></div>`;
            div.innerHTML=`<div class="bubble">${safe}</div>${meta}`;
            chatHistory.appendChild(div);
            chatHistory.scrollTop=chatHistory.scrollHeight;
            const hb=div.querySelector('.hl-btn');
            if(hb) hb.addEventListener('click',()=>{
                const ents=JSON.parse(hb.dataset.ents);
                applyHighlights(ents);
                for(const n of data.nodes){if(state.hlNodeGids.has(n.gid)){const pos=n.__threeObj?.position;if(pos)Graph.cameraPosition({x:pos.x,y:pos.y,z:pos.z+200},{x:pos.x,y:pos.y,z:pos.z},800);break;}}
            });
            return div;
        }

        function appendThinking(){
            const div=document.createElement('div');
            div.className='chat-msg assistant'; div.id='thinkingMsg';
            div.innerHTML='<div class="bubble"><span class="thinking-dots">\u25cf\u25cf\u25cf</span></div>';
            chatHistory.appendChild(div); chatHistory.scrollTop=chatHistory.scrollHeight;
            return div;
        }

        async function handleQuery(){
            const query=queryInput.value.trim(); if(!query) return;
            queryInput.value=''; sendBtn.disabled=true;
            appendMessage('user',query);
            const rel=findRelevantNodes(query);
            const paths=findConnectingPaths(rel,4,12);
            const ctx=buildContext(rel,paths);
            const thinking=appendThinking();
            const msgs=[{role:'system',content:SYSTEM_PROMPT},{role:'user',content:ctx},{role:'assistant',content:'I have noted the graph context. Please ask your question.'},...conversationHistory,{role:'user',content:query}];
            try{
                const resp=await callLLM(msgs);
                thinking.remove();
                const ents=parseHighlightedEntities(resp);
                const clean=cleanResponse(resp);
                conversationHistory.push({role:'user',content:query},{role:'assistant',content:clean});
                if(conversationHistory.length>12) conversationHistory.splice(0,2);
                appendMessage('assistant',clean,ents);
                if(ents.length) applyHighlights(ents);
            }catch(err){
                thinking.remove();
                appendMessage('assistant',`Error: ${err.message}\n\nCheck your API key and provider settings.`);
            }finally{
                sendBtn.disabled=false; queryInput.focus();
            }
        }

        // ── Boot (identical to graph_augmented) ──
        state.showAug=true; state.augOnlyMode=true;
        if(showAugEl) showAugEl.checked=true;
        if(augOnlyEl) augOnlyEl.checked=true;
        mergeAugmentation();

        function updateClusterModeUI(){
            topoResRow.style.display=state.clusterMode==='topology'?'':'none';
            spatialRadiusRow.style.display=state.clusterMode==='spatial'?'':'none';
            spatialMinRow.style.display=state.clusterMode==='spatial'?'':'none';
        }
        updateDetails(null);
        if(maxNodesValEl)    maxNodesValEl.textContent=String(state.maxNodes);
        if(depthValEl)       depthValEl.textContent=String(state.depthSpread);
        if(clusterRadiusValEl) clusterRadiusValEl.textContent=String(state.clusterRadius);
        if(minClusterValEl)  minClusterValEl.textContent=String(state.minClusterSize);
        if(topoResValEl)     topoResValEl.textContent=state.topoResolution.toFixed(2);
        updateClusterModeUI();
        applyDepth(); resetCamera();
        state.visible=seedVisibleByDegree(state.minDegree,state.maxNodes);
        buildSubgraphFromVisible();
        hideOverlay();
        rebuildAndRender('Rendering initial 3D view...');

        // ── Suggested query chips ──
        (function buildSuggestedQueries(){
            // Pick top hub entities by degree for dynamic suggestions
            const entities=data.nodes.filter(n=>n.type==='entity'&&n.degree>0);
            entities.sort((a,b)=>(b.degree||0)-(a.degree||0));
            const hubs=entities.slice(0,5).map(n=>n.label||n.id);

            // Query templates mixing hub-based and category-based questions
            const queries=[];

            // Hub-node questions (dynamic, based on actual graph content)
            if(hubs[0]) queries.push(`What role does "${hubs[0]}" play in this research landscape?`);
            if(hubs[1]) queries.push(`How is "${hubs[1]}" connected to other major concepts?`);
            if(hubs[0]&&hubs[2]) queries.push(`What is the relationship between "${hubs[0]}" and "${hubs[2]}"?`);

            // Category-based questions (generic but meaningful for KG research)
            queries.push('What are the main methods used for knowledge graph construction?');
            queries.push('Which concepts bridge symbolic reasoning and neural approaches?');
            queries.push('What evidence supports using LLMs for knowledge graph tasks?');
            queries.push('What are the key limitations or open challenges identified in the graph?');
            queries.push('Which communities of research are most interconnected?');

            const container=document.getElementById('suggestedQueries');
            if(!container) return;
            queries.slice(0,7).forEach(q=>{
                const btn=document.createElement('button');
                btn.className='sq-chip';
                btn.textContent=q;
                btn.addEventListener('click',()=>{
                    queryInput.value=q;
                    queryInput.focus();
                    // open chat panel if closed
                    const panel=document.getElementById('chatPanel');
                    if(panel&&!panel.classList.contains('open')) panel.classList.add('open');
                });
                container.appendChild(btn);
            });
        })();

    })();
</script>
</body>
</html>



)HTML";

    file.flush();
    if (file.fail()) {
        throw std::runtime_error("Failed to write Graph-RAG HTML file: " + filename);
    }
    file.close();
}

} // namespace kg

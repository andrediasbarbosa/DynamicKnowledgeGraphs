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

    // Write HTML file
    file << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << title << R"(</title>
    <script src="https://d3js.org/d3.v7.min.js"></script>
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
        .node-entity {
            fill: #4fc3f7;
            stroke: #0288d1;
            stroke-width: 2px;
        }
        .node-relation {
            fill: #ff9800;
            stroke: #f57c00;
            stroke-width: 2px;
        }
        .link-source {
            stroke: #4fc3f7;
            stroke-opacity: 0.6;
        }
        .link-target {
            stroke: #ff9800;
            stroke-opacity: 0.6;
        }
        text {
            fill: #fff;
            font-size: 11px;
            pointer-events: none;
            text-shadow: 0 0 3px rgba(0,0,0,0.8);
        }
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

        const width = window.innerWidth;
        const height = window.innerHeight;

        const svg = d3.select("#graph")
            .append("svg")
            .attr("width", width)
            .attr("height", height);

        // Add zoom behavior
        const g = svg.append("g");

        svg.call(d3.zoom()
            .scaleExtent([0.1, 4])
            .on("zoom", (event) => {
                g.attr("transform", event.transform);
            }));

        // Arrow markers for directed edges
        svg.append("defs").selectAll("marker")
            .data(["source", "target"])
            .join("marker")
            .attr("id", d => `arrow-${d}`)
            .attr("viewBox", "0 -5 10 10")
            .attr("refX", 20)
            .attr("refY", 0)
            .attr("markerWidth", 6)
            .attr("markerHeight", 6)
            .attr("orient", "auto")
            .append("path")
            .attr("fill", d => d === "source" ? "#4fc3f7" : "#ff9800")
            .attr("d", "M0,-5L10,0L0,5");

        // Create force simulation
        const simulation = d3.forceSimulation(data.nodes)
            .force("link", d3.forceLink(data.links).id((d, i) => i).distance(80).strength(0.5))
            .force("charge", d3.forceManyBody().strength(-400))
            .force("center", d3.forceCenter(width / 2, height / 2))
            .force("collision", d3.forceCollide().radius(30));

        // Create links
        const link = g.append("g")
            .selectAll("line")
            .data(data.links)
            .join("line")
            .attr("class", d => `link-${d.type}`)
            .attr("stroke-width", 2)
            .attr("marker-end", d => `url(#arrow-${d.type})`);

        // Create nodes
        const node = g.append("g")
            .selectAll("g")
            .data(data.nodes)
            .join("g")
            .call(d3.drag()
                .on("start", dragstarted)
                .on("drag", dragged)
                .on("end", dragended));

        // Add circles for entities, rectangles for relations
        node.each(function(d) {
            const el = d3.select(this);
            if (d.type === "entity") {
                el.append("circle")
                    .attr("r", Math.max(8, Math.min(20, 6 + d.degree * 2)))
                    .attr("class", "node-entity");
            } else {
                el.append("rect")
                    .attr("width", 24)
                    .attr("height", 16)
                    .attr("x", -12)
                    .attr("y", -8)
                    .attr("rx", 3)
                    .attr("class", "node-relation");
            }
        });

        // Add labels
        const labels = node.append("text")
            .attr("dx", d => d.type === "entity" ? 15 : 18)
            .attr("dy", 4)
            .text(d => d.label.length > 20 ? d.label.substring(0, 17) + "..." : d.label);

        // Tooltip
        const tooltip = d3.select("#tooltip");

        node.on("mouseover", function(event, d) {
            let content = `<h3>${d.label}</h3>`;
            content += `<div class="detail">Type: ${d.type === "entity" ? "Entity" : "Relation"}</div>`;

            if (d.type === "entity") {
                content += `<div class="detail">Degree: ${d.degree}</div>`;
            } else {
                content += `<div class="detail">Confidence: ${(d.confidence * 100).toFixed(0)}%</div>`;
                if (d.sources && d.sources.length > 0) {
                    content += `<div class="detail">Sources: ${d.sources.join(", ")}</div>`;
                }
                if (d.targets && d.targets.length > 0) {
                    content += `<div class="detail">Targets: ${d.targets.join(", ")}</div>`;
                }
            }

            tooltip.html(content)
                .style("left", (event.pageX + 15) + "px")
                .style("top", (event.pageY - 10) + "px")
                .style("opacity", 1);
        })
        .on("mouseout", function() {
            tooltip.style("opacity", 0);
        });

        // Update positions on tick
        simulation.on("tick", () => {
            link
                .attr("x1", d => d.source.x)
                .attr("y1", d => d.source.y)
                .attr("x2", d => d.target.x)
                .attr("y2", d => d.target.y);

            node.attr("transform", d => `translate(${d.x},${d.y})`);
        });

        // Drag functions
        function dragstarted(event) {
            if (!event.active) simulation.alphaTarget(0.3).restart();
            event.subject.fx = event.subject.x;
            event.subject.fy = event.subject.y;
        }

        function dragged(event) {
            event.subject.fx = event.x;
            event.subject.fy = event.y;
        }

        function dragended(event) {
            if (!event.active) simulation.alphaTarget(0);
            event.subject.fx = null;
            event.subject.fy = null;
        }

        // Controls
        d3.select("#linkStrength").on("input", function() {
            simulation.force("link").strength(+this.value);
            simulation.alpha(0.3).restart();
        });

        d3.select("#repulsion").on("input", function() {
            simulation.force("charge").strength(-this.value);
            simulation.alpha(0.3).restart();
        });

        d3.select("#showLabels").on("change", function() {
            labels.style("opacity", this.checked ? 1 : 0);
        });

        // Resize handler
        window.addEventListener("resize", () => {
            svg.attr("width", window.innerWidth)
               .attr("height", window.innerHeight);
            simulation.force("center", d3.forceCenter(window.innerWidth / 2, window.innerHeight / 2));
            simulation.alpha(0.3).restart();
        });
    </script>
</body>
</html>
)";

    file.close();
}

} // namespace kg

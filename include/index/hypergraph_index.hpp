#pragma once

#include "graph/hypergraph.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <set>
#include <iostream>

namespace kg {

struct HypergraphIndex {
    // Metadata
    std::string created_utc;
    std::string source_graph_path;
    size_t node_count = 0;
    size_t edge_count = 0;

    // Inverse index: relation type -> edge IDs
    std::unordered_map<std::string, std::vector<std::string>> relation_to_edges;

    // Inverse index: node label (lowercase) -> node IDs
    std::unordered_map<std::string, std::vector<std::string>> label_to_nodes;

    // S-components cache: s-value -> list of components (each component = set of edge IDs)
    std::map<int, std::vector<std::set<std::string>>> s_components;

    // Node degree rankings (sorted by degree descending)
    std::vector<std::pair<std::string, int>> degree_ranked_nodes;

    // Entity co-occurrence: pair key "min_id|max_id" -> count
    std::unordered_map<std::string, int> entity_cooccurrence;

    // Build index from a hypergraph
    void build(const Hypergraph& graph, const std::vector<int>& s_values = {2, 3, 4}) {
        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        created_utc = ss.str();

        node_count = graph.num_nodes();
        edge_count = graph.num_edges();

        // Clear previous
        relation_to_edges.clear();
        label_to_nodes.clear();
        s_components.clear();
        degree_ranked_nodes.clear();
        entity_cooccurrence.clear();

        // Get all edges and nodes
        auto all_edges = graph.get_all_edges();
        auto all_nodes = graph.get_all_nodes();

        // Build relation index
        for (const auto& edge : all_edges) {
            std::string rel = edge.relation;
            // Normalize relation to lowercase
            std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);
            relation_to_edges[rel].push_back(edge.id);
        }

        // Build label index and degree ranking
        for (const auto& node : all_nodes) {
            std::string label = node.label;
            std::transform(label.begin(), label.end(), label.begin(), ::tolower);
            label_to_nodes[label].push_back(node.id);
            degree_ranked_nodes.emplace_back(node.id, node.degree);
        }

        // Sort by degree descending
        std::sort(degree_ranked_nodes.begin(), degree_ranked_nodes.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Compute s-components
        for (int s : s_values) {
            auto components = graph.find_s_connected_components(s);
            s_components[s] = std::move(components);
        }

        // Build co-occurrence index (for entities only)
        for (const auto& edge : all_edges) {
            std::vector<std::string> entities;
            for (const auto& src : edge.sources) {
                entities.push_back(src);
            }
            for (const auto& tgt : edge.targets) {
                entities.push_back(tgt);
            }

            // Count pairwise co-occurrences
            for (size_t i = 0; i < entities.size(); ++i) {
                for (size_t j = i + 1; j < entities.size(); ++j) {
                    std::string a = entities[i] < entities[j] ? entities[i] : entities[j];
                    std::string b = entities[i] < entities[j] ? entities[j] : entities[i];
                    std::string key = a + "|" + b;
                    entity_cooccurrence[key]++;
                }
            }
        }
    }

    // Get co-occurrence count for a pair (uses normalized IDs for case-insensitive matching)
    int get_cooccurrence(const std::string& a, const std::string& b) const {
        // Normalize IDs to match how the graph stores them
        std::string norm_a = Hypergraph::normalize_node_id(a);
        std::string norm_b = Hypergraph::normalize_node_id(b);
        std::string min_id = norm_a < norm_b ? norm_a : norm_b;
        std::string max_id = norm_a < norm_b ? norm_b : norm_a;
        std::string key = min_id + "|" + max_id;
        auto it = entity_cooccurrence.find(key);
        return it != entity_cooccurrence.end() ? it->second : 0;
    }

    // Get top-k nodes by degree
    std::vector<std::string> get_top_hubs(size_t k) const {
        std::vector<std::string> result;
        for (size_t i = 0; i < std::min(k, degree_ranked_nodes.size()); ++i) {
            result.push_back(degree_ranked_nodes[i].first);
        }
        return result;
    }

    // Find nodes by label prefix
    std::vector<std::string> find_nodes_by_prefix(const std::string& prefix) const {
        std::vector<std::string> result;
        std::string lower_prefix = prefix;
        std::transform(lower_prefix.begin(), lower_prefix.end(), lower_prefix.begin(), ::tolower);

        for (const auto& [label, ids] : label_to_nodes) {
            if (label.rfind(lower_prefix, 0) == 0) {
                result.insert(result.end(), ids.begin(), ids.end());
            }
        }
        return result;
    }

    // Save to JSON
    void save_to_json(const std::string& path) const {
        nlohmann::json j;
        j["meta"] = {
            {"created_utc", created_utc},
            {"source_graph_path", source_graph_path},
            {"node_count", node_count},
            {"edge_count", edge_count}
        };

        // Relation index
        j["relation_to_edges"] = relation_to_edges;

        // Label index
        j["label_to_nodes"] = label_to_nodes;

        // S-components (convert set to vector for JSON)
        nlohmann::json sc;
        for (const auto& [s, comps] : s_components) {
            nlohmann::json comps_arr = nlohmann::json::array();
            for (const auto& comp : comps) {
                comps_arr.push_back(std::vector<std::string>(comp.begin(), comp.end()));
            }
            sc[std::to_string(s)] = comps_arr;
        }
        j["s_components"] = sc;

        // Degree ranking (top 1000 only to save space)
        nlohmann::json dr = nlohmann::json::array();
        for (size_t i = 0; i < std::min(size_t(1000), degree_ranked_nodes.size()); ++i) {
            dr.push_back({degree_ranked_nodes[i].first, degree_ranked_nodes[i].second});
        }
        j["degree_ranked_nodes"] = dr;

        // Co-occurrence (sample for large graphs)
        if (entity_cooccurrence.size() <= 50000) {
            j["entity_cooccurrence"] = entity_cooccurrence;
        } else {
            // Save top co-occurrences only
            std::vector<std::pair<std::string, int>> sorted_cooc(
                entity_cooccurrence.begin(), entity_cooccurrence.end());
            std::sort(sorted_cooc.begin(), sorted_cooc.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            nlohmann::json cooc;
            for (size_t i = 0; i < std::min(size_t(50000), sorted_cooc.size()); ++i) {
                cooc[sorted_cooc[i].first] = sorted_cooc[i].second;
            }
            j["entity_cooccurrence"] = cooc;
        }

        std::ofstream file(path);
        file << j.dump(2);
    }

    // Load from JSON
    static HypergraphIndex load_from_json(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open index file: " + path);
        }

        nlohmann::json j;
        file >> j;

        HypergraphIndex idx;

        // Meta
        if (j.contains("meta")) {
            idx.created_utc = j["meta"].value("created_utc", "");
            idx.source_graph_path = j["meta"].value("source_graph_path", "");
            idx.node_count = j["meta"].value("node_count", 0);
            idx.edge_count = j["meta"].value("edge_count", 0);
        }

        // Relation index
        if (j.contains("relation_to_edges")) {
            idx.relation_to_edges = j["relation_to_edges"]
                .get<std::unordered_map<std::string, std::vector<std::string>>>();
        }

        // Label index
        if (j.contains("label_to_nodes")) {
            idx.label_to_nodes = j["label_to_nodes"]
                .get<std::unordered_map<std::string, std::vector<std::string>>>();
        }

        // S-components (convert vector back to set)
        if (j.contains("s_components")) {
            for (auto& [key, val] : j["s_components"].items()) {
                int s = std::stoi(key);
                std::vector<std::set<std::string>> comps;
                for (const auto& comp_arr : val) {
                    auto vec = comp_arr.get<std::vector<std::string>>();
                    comps.push_back(std::set<std::string>(vec.begin(), vec.end()));
                }
                idx.s_components[s] = comps;
            }
        }

        // Degree ranking
        if (j.contains("degree_ranked_nodes")) {
            for (const auto& item : j["degree_ranked_nodes"]) {
                idx.degree_ranked_nodes.emplace_back(item[0].get<std::string>(), item[1].get<int>());
            }
        }

        // Co-occurrence
        if (j.contains("entity_cooccurrence")) {
            idx.entity_cooccurrence = j["entity_cooccurrence"]
                .get<std::unordered_map<std::string, int>>();
        }

        return idx;
    }

    // Print summary
    void print_summary() const {
        std::cout << "HypergraphIndex Summary:\n";
        std::cout << "  Created: " << created_utc << "\n";
        std::cout << "  Nodes: " << node_count << "\n";
        std::cout << "  Edges: " << edge_count << "\n";
        std::cout << "  Unique relations: " << relation_to_edges.size() << "\n";
        std::cout << "  Unique labels: " << label_to_nodes.size() << "\n";
        std::cout << "  S-components cached: ";
        for (const auto& [s, comps] : s_components) {
            std::cout << "s=" << s << " (" << comps.size() << " components) ";
        }
        std::cout << "\n";
        std::cout << "  Co-occurrence pairs: " << entity_cooccurrence.size() << "\n";
    }
};

} // namespace kg

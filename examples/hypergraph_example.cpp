#include "graph/hypergraph.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

using namespace kg;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

int main() {
    print_separator("Hypergraph Example - Knowledge Graph Construction");

    // Create output directory
    const std::string output_dir = "output_json";
    #ifdef _WIN32
        _mkdir(output_dir.c_str());
    #else
        mkdir(output_dir.c_str(), 0755);
    #endif

    // Create a hypergraph
    Hypergraph graph;

    std::cout << "Creating hypergraph for biocomposite scaffold knowledge...\n\n";

    // Example 1: Simple pairwise relation
    // PCL + chitosan compose nanofibers
    std::cout << "1. Adding simple pairwise relation:\n";
    std::cout << "   [PCL, chitosan] --compose--> [nanofibers]\n";
    graph.add_hyperedge(
        {"PCL", "chitosan"},
        "compose",
        {"nanofibers"}
    );

    // Example 2: Multiple sources, single target
    // Cerium oxide, Chitosan, Hydroxyethylcellulose compose antibacterial films
    std::cout << "\n2. Adding higher-order relation (3 sources -> 1 target):\n";
    std::cout << "   [Cerium oxide, Chitosan, Hydroxyethylcellulose] --compose--> [antibacterial films]\n";
    graph.add_hyperedge(
        {"Cerium oxide", "Chitosan", "Hydroxyethylcellulose", "Polyethylene glycol"},
        "compose",
        {"antibacterial nano composite films"}
    );

    // Example 3: Single source, multiple targets
    // Scaffolds have porosity, biodegradability
    std::cout << "\n3. Adding property relation (1 source -> multiple targets):\n";
    std::cout << "   [scaffolds] --have_properties--> [porosity, biodegradability, biocompatibility]\n";
    graph.add_hyperedge(
        {"scaffolds"},
        "have_properties",
        {"porosity", "biodegradability", "biocompatibility"}
    );

    // Example 4: Complex multi-entity relation
    // PCL, hydroxyapatite, collagen form composite scaffolds for bone tissue engineering
    std::cout << "\n4. Adding complex relation:\n";
    std::cout << "   [PCL, hydroxyapatite, collagen] --form--> [composite scaffolds, bone tissue engineering]\n";
    graph.add_hyperedge(
        {"PCL", "hydroxyapatite", "collagen"},
        "form",
        {"composite scaffolds", "bone tissue engineering"}
    );

    // Example 5: Process relation
    std::cout << "\n5. Adding process relation:\n";
    std::cout << "   [chitosan, gelatin] --processed_via--> [electrospinning]\n";
    graph.add_hyperedge(
        {"chitosan", "gelatin"},
        "processed_via",
        {"electrospinning"}
    );

    // Add more edges to create a richer graph
    graph.add_hyperedge({"PCL", "gelatin"}, "compose", {"hybrid scaffolds"});
    graph.add_hyperedge({"hydroxyapatite", "chitosan"}, "enhance", {"biocompatibility"});
    graph.add_hyperedge({"biocompatibility", "porosity"}, "determine", {"cell adhesion"});
    graph.add_hyperedge({"nanofibers"}, "used_for", {"tissue engineering"});
    graph.add_hyperedge({"composite scaffolds"}, "applied_in", {"bone regeneration"});

    print_separator("Graph Statistics");

    auto stats = graph.compute_statistics();
    std::cout << "Number of nodes: " << stats.num_nodes << "\n";
    std::cout << "Number of hyperedges: " << stats.num_edges << "\n";
    std::cout << "Average edge size: " << std::fixed << std::setprecision(2) << stats.avg_edge_size << "\n";
    std::cout << "Max edge size: " << stats.max_edge_size << "\n";
    std::cout << "Average node degree: " << stats.avg_node_degree << "\n";
    std::cout << "Max node degree: " << stats.max_node_degree << "\n";

    print_separator("Top Hubs (High-Degree Nodes)");

    auto hubs = graph.get_top_hubs(10);
    std::cout << std::left << std::setw(30) << "Node" << std::setw(10) << "Degree" << "\n";
    std::cout << std::string(40, '-') << "\n";

    for (const auto& [node_id, degree] : hubs) {
        std::cout << std::setw(30) << node_id << std::setw(10) << degree << "\n";
    }

    print_separator("Path Finding Example");

    // Find path from PCL to bone regeneration
    std::cout << "Finding shortest path from 'PCL' to 'bone regeneration'...\n\n";

    auto path = graph.find_shortest_path("PCL", "bone regeneration", 1);

    if (!path.empty()) {
        std::cout << "Path found with " << path.size() << " hyperedges:\n\n";

        for (size_t i = 0; i < path.size(); ++i) {
            const auto& edge = path[i];

            std::cout << "Step " << (i + 1) << ":\n";
            std::cout << "  Sources: ";
            for (size_t j = 0; j < edge.sources.size(); ++j) {
                std::cout << edge.sources[j];
                if (j < edge.sources.size() - 1) std::cout << ", ";
            }

            std::cout << "\n  Relation: " << edge.relation << "\n";

            std::cout << "  Targets: ";
            for (size_t j = 0; j < edge.targets.size(); ++j) {
                std::cout << edge.targets[j];
                if (j < edge.targets.size() - 1) std::cout << ", ";
            }
            std::cout << "\n\n";
        }
    } else {
        std::cout << "No path found.\n";
    }

    print_separator("K-Shortest Paths Example");

    // Find multiple paths
    std::cout << "Finding 3 shortest paths from 'chitosan' to 'cell adhesion'...\n\n";

    auto k_paths = graph.find_k_shortest_paths("chitosan", "cell adhesion", 3, 1);

    if (k_paths.found) {
        std::cout << "Found " << k_paths.paths.size() << " paths:\n\n";

        for (size_t p = 0; p < k_paths.paths.size(); ++p) {
            std::cout << "Path " << (p + 1) << " (length: " << k_paths.paths[p].size() << "):\n";

            for (const auto& edge : k_paths.paths[p]) {
                std::cout << "  [";
                for (size_t i = 0; i < edge.sources.size(); ++i) {
                    std::cout << edge.sources[i];
                    if (i < edge.sources.size() - 1) std::cout << ", ";
                }
                std::cout << "] --" << edge.relation << "--> [";
                for (size_t i = 0; i < edge.targets.size(); ++i) {
                    std::cout << edge.targets[i];
                    if (i < edge.targets.size() - 1) std::cout << ", ";
                }
                std::cout << "]\n";
            }
            std::cout << "\n";
        }
    } else {
        std::cout << "No paths found.\n";
    }

    print_separator("Neighborhood Exploration");

    // Get neighborhood
    std::cout << "Finding 2-hop neighborhood of 'chitosan'...\n\n";

    auto neighborhood = graph.get_neighborhood("chitosan", 2, 1);

    std::cout << "Found " << neighborhood.size() << " nodes in 2-hop neighborhood:\n";
    for (const auto& node_id : neighborhood) {
        std::cout << "  - " << node_id << "\n";
    }

    print_separator("Export Examples");

    // Export to JSON
    std::cout << "Exporting to JSON...\n";
    std::string json_path = output_dir + "/hypergraph_example.json";
    graph.export_to_json(json_path);
    std::cout << "  ✓ Saved to: " << json_path << "\n\n";

    // Export to DOT (Graphviz)
    std::cout << "Exporting to DOT format...\n";
    std::string dot_path = output_dir + "/hypergraph_example.dot";
    graph.export_to_dot(dot_path);
    std::cout << "  ✓ Saved to: " << dot_path << "\n";
    std::cout << "  To visualize: dot -Tpng " << dot_path << " -o " << output_dir << "/hypergraph.png\n\n";

    // Export incidence matrix
    std::cout << "Exporting incidence matrix...\n";
    auto incidence = graph.to_incidence_matrix();
    std::string matrix_path = output_dir + "/incidence_matrix.json";
    std::ofstream matrix_file(matrix_path);
    matrix_file << incidence.dump(2);
    matrix_file.close();
    std::cout << "  ✓ Saved to: " << matrix_path << "\n";

    print_separator("Node Deduplication Example");

    // Demonstrate node merging (without embeddings, just showing the API)
    std::cout << "Note: In a real application, you would:\n";
    std::cout << "1. Add embedding vectors to nodes using LLM embeddings\n";
    std::cout << "2. Call merge_similar_nodes() to deduplicate\n";
    std::cout << "3. Example: chitosan vs Chitosan would be merged\n\n";

    std::cout << "API call: graph.merge_similar_nodes(0.95);\n";
    std::cout << "  - Computes cosine similarity between node embeddings\n";
    std::cout << "  - Merges nodes with similarity >= 0.95\n";
    std::cout << "  - Keeps highest-degree node as representative\n";

    print_separator("S-Connected Components");

    // Find strongly connected components
    std::cout << "Finding s-connected components (s=1)...\n\n";

    auto components = graph.find_s_connected_components(1);

    std::cout << "Found " << components.size() << " components:\n";
    for (size_t i = 0; i < std::min(size_t(3), components.size()); ++i) {
        std::cout << "  Component " << (i + 1) << ": " << components[i].size() << " hyperedges\n";
    }

    if (components.size() > 3) {
        std::cout << "  ... and " << (components.size() - 3) << " more\n";
    }

    print_separator("Advanced Analysis");

    // Degree distribution
    auto distribution = graph.compute_degree_distribution();
    std::cout << "Degree Distribution:\n";
    std::cout << std::left << std::setw(15) << "Degree" << std::setw(15) << "Frequency" << "\n";
    std::cout << std::string(30, '-') << "\n";

    int count = 0;
    for (const auto& [degree, freq] : distribution) {
        std::cout << std::setw(15) << degree << std::setw(15) << freq << "\n";
        if (++count >= 5) {
            std::cout << "  ... (showing first 5)\n";
            break;
        }
    }

    // Power law fit
    auto [exponent, r_squared] = graph.fit_power_law();
    std::cout << "\nPower Law Fit:\n";
    std::cout << "  Exponent: " << std::fixed << std::setprecision(3) << exponent << "\n";
    std::cout << "  R²: " << r_squared << "\n";

    if (r_squared > 0.7) {
        std::cout << "  → Graph exhibits scale-free topology\n";
    }

    // Hub integration
    auto integration = graph.compute_hub_integration_scores(5);
    std::cout << "\nHub Integration Scores (co-occurrences with other hubs):\n";
    for (const auto& [hub_id, score] : integration) {
        std::cout << "  " << std::left << std::setw(30) << hub_id
                  << " → " << score << " co-occurrences\n";
    }

    print_separator("Summary");

    std::cout << "This example demonstrated:\n";
    std::cout << "  ✓ Creating higher-order hyperedges\n";
    std::cout << "  ✓ Computing graph statistics\n";
    std::cout << "  ✓ Finding shortest paths (s-connected)\n";
    std::cout << "  ✓ K-shortest paths algorithm\n";
    std::cout << "  ✓ Neighborhood exploration\n";
    std::cout << "  ✓ Export to JSON, DOT, and incidence matrix\n";
    std::cout << "  ✓ Node deduplication (API)\n";
    std::cout << "  ✓ Connected component analysis\n";
    std::cout << "  ✓ Degree distribution and power law fitting\n";
    std::cout << "  ✓ Hub integration analysis\n";

    std::cout << "\nNext steps:\n";
    std::cout << "  1. Integrate with LLM providers for extraction\n";
    std::cout << "  2. Add embedding models for node deduplication\n";
    std::cout << "  3. Build complete pipeline from PDFs\n";
    std::cout << "  4. Implement agentic reasoning on hypergraph\n";

    print_separator("End of Example");

    return 0;
}

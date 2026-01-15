#include "graph/hypergraph.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

using namespace kg;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

int main() {
    print_separator("Historical Knowledge Graph - From tests/1page.pdf");

    // Create output directory
    const std::string output_dir = "output_json";
    #ifdef _WIN32
        _mkdir(output_dir.c_str());
    #else
        mkdir(output_dir.c_str(), 0755);
    #endif

    // Create hypergraph for mathematical and historical knowledge transmission
    Hypergraph graph;

    std::cout << "Building knowledge graph from '1page.pdf'...\n";
    std::cout << "Document: History of mathematical knowledge transmission\n\n";

    // =========================================================================
    // HISTORY THREAD: Movement of Texts and Institutions
    // =========================================================================

    print_separator("History Thread: Movement of Texts and Institutions");

    // 1. House of Wisdom -> Translators -> Arabic versions
    std::cout << "1. Baghdad translation activities:\n";
    graph.add_hyperedge(
        {"House of Wisdom", "Baghdad"},
        "sponsored",
        {"translators"}
    );
    graph.add_hyperedge(
        {"translators"},
        "produced",
        {"Arabic versions", "Greek geometry"}
    );

    // 2. Hunayn ibn Ishaq and standardization
    std::cout << "2. Terminology standardization:\n";
    graph.add_hyperedge(
        {"Hunayn ibn Ishaq"},
        "maintained",
        {"glossary"}
    );
    graph.add_hyperedge(
        {"glossary"},
        "standardized",
        {"technical terms"}
    );

    // 3. Manuscript transmission
    std::cout << "3. Manuscript journey:\n";
    graph.add_hyperedge(
        {"scribe in Damascus"},
        "copied",
        {"Euclid's Elements"}
    );
    graph.add_hyperedge(
        {"Euclid's Elements"},
        "traveled_through",
        {"Mediterranean trade routes"}
    );

    // 4. Sicily multilingual workshops
    std::cout << "4. Sicilian translation workshops:\n";
    graph.add_hyperedge(
        {"Sicily", "Norman administration"},
        "hosted",
        {"multilingual workshops"}
    );
    graph.add_hyperedge(
        {"multilingual workshops"},
        "produced",
        {"Latin copy"}
    );
    graph.add_hyperedge(
        {"Latin copy"},
        "entered",
        {"cathedral library"}
    );

    // 5. Adelard of Bath transmission
    std::cout << "5. Adelard of Bath's role:\n";
    graph.add_hyperedge(
        {"Adelard of Bath"},
        "brought",
        {"the same book", "England"}
    );
    graph.add_hyperedge(
        {"Adelard of Bath"},
        "taught",
        {"geometry", "students"}
    );
    graph.add_hyperedge(
        {"students"},
        "lectured_in",
        {"Paris"}
    );

    // 6. University and clerical networks
    std::cout << "6. Institutional networks:\n";
    graph.add_hyperedge(
        {"Parisian masters"},
        "belonged_to",
        {"university milieu"}
    );
    graph.add_hyperedge(
        {"university milieu"},
        "shaped",
        {"clerical careers"}
    );
    graph.add_hyperedge(
        {"clerks"},
        "staffed",
        {"chanceries"}
    );
    graph.add_hyperedge(
        {"chanceries"},
        "recorded",
        {"land grants", "measured boundaries"}
    );

    // =========================================================================
    // MATHEMATICS THREAD: Methods, Artifacts, and Echoes
    // =========================================================================

    print_separator("Mathematics Thread: Methods, Artifacts, and Echoes");

    // 7. Euclidean geometry
    std::cout << "7. Euclidean propositions:\n";
    graph.add_hyperedge(
        {"Euclid"},
        "organized",
        {"proofs", "propositions"}
    );
    graph.add_hyperedge(
        {"Proposition I.47"},
        "states",
        {"Pythagorean relationship"}
    );
    graph.add_hyperedge(
        {"Pythagorean relationship"},
        "connects",
        {"right triangles", "area"}
    );

    // 8. Greek mathematical tradition
    std::cout << "8. Greek philosophical context:\n";
    graph.add_hyperedge(
        {"Pythagoras"},
        "belongs_to",
        {"Greek mathematical tradition"}
    );
    graph.add_hyperedge(
        {"Greek mathematical tradition"},
        "fed",
        {"Plato's Academy"}
    );
    graph.add_hyperedge(
        {"Plato"},
        "valued",
        {"geometry", "governance"}
    );
    graph.add_hyperedge(
        {"rulers"},
        "funded",
        {"libraries"}
    );
    graph.add_hyperedge(
        {"libraries"},
        "projected",
        {"legitimacy"}
    );

    // 9. Al-Khwarizmi and algebra
    std::cout << "9. Development of algebra:\n";
    graph.add_hyperedge(
        {"Al-Khwarizmi", "Baghdad"},
        "wrote",
        {"treatise on al-jabr"}
    );
    graph.add_hyperedge(
        {"al-jabr"},
        "is_method_for",
        {"balancing equations"}
    );
    graph.add_hyperedge(
        {"treatise on al-jabr"},
        "influenced",
        {"calculation practice"}
    );

    // 10. Fibonacci and Hindu-Arabic numerals
    std::cout << "10. Numeral system transmission:\n";
    graph.add_hyperedge(
        {"Fibonacci"},
        "learned",
        {"arithmetic", "North Africa"}
    );
    graph.add_hyperedge(
        {"Fibonacci"},
        "promoted",
        {"Hindu-Arabic numerals", "Italy"}
    );
    graph.add_hyperedge(
        {"Hindu-Arabic numerals"},
        "simplified",
        {"bookkeeping"}
    );
    graph.add_hyperedge(
        {"bookkeeping"},
        "supported",
        {"long-distance trade", "port cities"}
    );

    // 11. Mechanical applications
    std::cout << "11. Mechanical timekeeping:\n";
    graph.add_hyperedge(
        {"port city"},
        "commissioned",
        {"clock tower"}
    );
    graph.add_hyperedge(
        {"clock tower gearing"},
        "implied",
        {"modular counting"}
    );

    // 12. Newton and calculus
    std::cout << "12. Calculus development:\n";
    graph.add_hyperedge(
        {"Newton"},
        "studied",
        {"algebraic techniques"}
    );
    graph.add_hyperedge(
        {"Newton"},
        "formulated",
        {"calculus", "motion"}
    );

    // 13. French Revolution and metric system
    std::cout << "13. Metric system creation:\n";
    graph.add_hyperedge(
        {"French Revolution", "reformers"},
        "created",
        {"metric system"}
    );
    graph.add_hyperedge(
        {"metric system"},
        "relied_on",
        {"geodetic survey work"}
    );
    graph.add_hyperedge(
        {"geodetic survey work"},
        "used",
        {"triangulation"}
    );
    graph.add_hyperedge(
        {"triangulation"},
        "returns_to",
        {"angle measurement", "tables from Baghdad"}
    );

    // =========================================================================
    // Cross-thread connections (showing interweaving of history and math)
    // =========================================================================

    std::cout << "\n14. Cross-thread connections:\n";
    graph.add_hyperedge(
        {"Euclid's Elements", "Arabic versions"},
        "represents",
        {"knowledge transmission"}
    );
    graph.add_hyperedge(
        {"geometry", "land grants"},
        "applied_in",
        {"practical measurement"}
    );
    graph.add_hyperedge(
        {"Baghdad", "tables from Baghdad"},
        "originated",
        {"mathematical methods"}
    );

    // =========================================================================
    // Graph Analysis
    // =========================================================================

    print_separator("Knowledge Graph Statistics");

    auto stats = graph.compute_statistics();
    std::cout << "Number of entities (nodes): " << stats.num_nodes << "\n";
    std::cout << "Number of relationships (hyperedges): " << stats.num_edges << "\n";
    std::cout << "Average relationship complexity: " << std::fixed << std::setprecision(2)
              << stats.avg_edge_size << " entities per relationship\n";
    std::cout << "Most complex relationship: " << stats.max_edge_size << " entities\n";
    std::cout << "Average entity connectivity: " << stats.avg_node_degree << " relationships\n";
    std::cout << "Highest connectivity: " << stats.max_node_degree << " relationships\n";

    print_separator("Most Connected Entities (Knowledge Hubs)");

    auto hubs = graph.get_top_hubs(15);
    std::cout << std::left << std::setw(35) << "Entity" << std::setw(15) << "Connections" << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (const auto& [entity_id, degree] : hubs) {
        std::cout << std::setw(35) << entity_id << std::setw(15) << degree << "\n";
    }

    print_separator("Knowledge Transmission Path Analysis");

    // Find path from Baghdad to metric system
    std::cout << "Tracing knowledge flow: Baghdad -> metric system\n\n";
    auto path = graph.find_shortest_path("Baghdad", "metric system", 1);

    if (!path.empty()) {
        std::cout << "Found transmission path with " << path.size() << " steps:\n\n";

        for (size_t i = 0; i < path.size(); ++i) {
            const auto& edge = path[i];

            std::cout << "Step " << (i + 1) << ":\n";
            std::cout << "  Entities: [";
            for (size_t j = 0; j < edge.sources.size(); ++j) {
                std::cout << edge.sources[j];
                if (j < edge.sources.size() - 1) std::cout << ", ";
            }

            std::cout << "] --" << edge.relation << "--> [";

            for (size_t j = 0; j < edge.targets.size(); ++j) {
                std::cout << edge.targets[j];
                if (j < edge.targets.size() - 1) std::cout << ", ";
            }
            std::cout << "]\n\n";
        }
    } else {
        std::cout << "No direct path found (paths may exist through more hops).\n";
    }

    // Find alternative paths
    std::cout << "\nFinding alternative transmission routes: Euclid -> Paris\n\n";
    auto k_paths = graph.find_k_shortest_paths("Euclid", "Paris", 3, 1);

    if (k_paths.found && !k_paths.paths.empty()) {
        std::cout << "Found " << k_paths.paths.size() << " alternative routes:\n\n";

        for (size_t p = 0; p < k_paths.paths.size(); ++p) {
            std::cout << "Route " << (p + 1) << " (" << k_paths.paths[p].size() << " steps):\n";

            for (const auto& edge : k_paths.paths[p]) {
                std::cout << "  ";
                for (size_t i = 0; i < edge.sources.size(); ++i) {
                    std::cout << edge.sources[i];
                    if (i < edge.sources.size() - 1) std::cout << ", ";
                }
                std::cout << " --" << edge.relation << "--> ";
                for (size_t i = 0; i < edge.targets.size(); ++i) {
                    std::cout << edge.targets[i];
                    if (i < edge.targets.size() - 1) std::cout << ", ";
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }
    }

    // Knowledge neighborhood
    std::cout << "Exploring knowledge neighborhood of 'geometry' (2-hop radius):\n\n";
    auto neighborhood = graph.get_neighborhood("geometry", 2, 1);

    std::cout << "Connected entities (" << neighborhood.size() << " total):\n";
    int count = 0;
    for (const auto& entity : neighborhood) {
        std::cout << "  - " << entity << "\n";
        if (++count >= 20) {
            std::cout << "  ... and " << (neighborhood.size() - 20) << " more\n";
            break;
        }
    }

    print_separator("Export to Multiple Formats");

    // Export to JSON
    std::cout << "Exporting complete knowledge graph to JSON...\n";
    std::string json_path = output_dir + "/historical_knowledge.json";
    graph.export_to_json(json_path, true);  // Include metadata
    std::cout << "  ✓ Saved to: " << json_path << "\n\n";

    // Export to DOT (Graphviz)
    std::cout << "Exporting to Graphviz DOT format...\n";
    std::string dot_path = output_dir + "/historical_knowledge.dot";
    graph.export_to_dot(dot_path);
    std::cout << "  ✓ Saved to: " << dot_path << "\n";
    std::cout << "  To visualize: dot -Tpng " << dot_path << " -o " << output_dir << "/historical_knowledge.png\n\n";

    // Export incidence matrix
    std::cout << "Exporting incidence matrix...\n";
    auto incidence = graph.to_incidence_matrix();
    std::string matrix_path = output_dir + "/historical_knowledge_matrix.json";
    std::ofstream matrix_file(matrix_path);
    matrix_file << incidence.dump(2);
    matrix_file.close();
    std::cout << "  ✓ Saved to: " << matrix_path << "\n";

    // Export interactive HTML visualization
    std::cout << "\nExporting interactive HTML visualization...\n";
    std::string html_path = output_dir + "/historical_knowledge.html";
    graph.export_to_html(html_path, "Historical Knowledge Hypergraph");
    std::cout << "  ✓ Saved to: " << html_path << "\n";
    std::cout << "  Open this file in a web browser to explore the graph interactively!\n";

    print_separator("Graph Topology Analysis");

    // Degree distribution
    auto distribution = graph.compute_degree_distribution();
    std::cout << "Degree Distribution:\n";
    std::cout << std::left << std::setw(15) << "Connections" << std::setw(15) << "# Entities" << "\n";
    std::cout << std::string(30, '-') << "\n";

    int dist_count = 0;
    for (const auto& [degree, freq] : distribution) {
        std::cout << std::setw(15) << degree << std::setw(15) << freq << "\n";
        if (++dist_count >= 10) {
            std::cout << "  ... (showing first 10)\n";
            break;
        }
    }

    // Power law fit
    auto [exponent, r_squared] = graph.fit_power_law();
    std::cout << "\nNetwork Topology Analysis:\n";
    std::cout << "  Power law exponent: " << std::fixed << std::setprecision(3) << exponent << "\n";
    std::cout << "  R² fit quality: " << r_squared << "\n";

    if (r_squared > 0.7) {
        std::cout << "  → Network exhibits scale-free properties\n";
        std::cout << "  → Characteristic of natural knowledge networks with key hubs\n";
    }

    // Connected components
    auto components = graph.find_s_connected_components(1);
    std::cout << "\nKnowledge Clusters (s-connected components):\n";
    std::cout << "  Found " << components.size() << " distinct clusters:\n";

    for (size_t i = 0; i < std::min(size_t(5), components.size()); ++i) {
        std::cout << "    Cluster " << (i + 1) << ": " << components[i].size()
                  << " interconnected relationships\n";
    }

    print_separator("Summary");

    std::cout << "This example demonstrated:\n";
    std::cout << "  ✓ Extracting entities and relationships from historical text\n";
    std::cout << "  ✓ Representing complex multi-entity relationships (not just pairwise)\n";
    std::cout << "  ✓ Tracking knowledge transmission across time and geography\n";
    std::cout << "  ✓ Finding transmission paths between historical entities\n";
    std::cout << "  ✓ Identifying knowledge hubs (most connected entities)\n";
    std::cout << "  ✓ Analyzing network topology of historical knowledge\n";
    std::cout << "  ✓ Exporting to multiple formats for further analysis\n";

    std::cout << "\nSource document: tests/1page.pdf\n";
    std::cout << "  Thread 1: Movement of texts and institutions\n";
    std::cout << "  Thread 2: Mathematical methods, artifacts, and echoes\n";

    std::cout << "\nKey insight from the document:\n";
    std::cout << "  'In this loop, a place becomes an origin,\n";
    std::cout << "   a manuscript becomes a conduit,\n";
    std::cout << "   and \"the method\" becomes a lineage.'\n";

    print_separator("End of Historical Knowledge Graph Example");

    return 0;
}

#include "graph/hypergraph.hpp"
#include <iostream>

using namespace kg;

int main(int argc, char* argv[]) {
    std::string json_path = "output_json/final_graph.json";
    std::string html_path = "output_json/final_graph.html";
    std::string title = "Knowledge Graph Visualization";

    if (argc > 1) {
        json_path = argv[1];
    }
    if (argc > 2) {
        html_path = argv[2];
    }
    if (argc > 3) {
        title = argv[3];
    }

    std::cout << "Loading hypergraph from: " << json_path << "\n";

    Hypergraph graph = Hypergraph::load_from_json(json_path);

    auto stats = graph.compute_statistics();
    std::cout << "Loaded graph with " << stats.num_nodes << " nodes and "
              << stats.num_edges << " edges\n";

    std::cout << "Exporting to HTML: " << html_path << "\n";
    graph.export_to_html(html_path, title);

    std::cout << "Done! Open " << html_path << " in your browser.\n";

    return 0;
}

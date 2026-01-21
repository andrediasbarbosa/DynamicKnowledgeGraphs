#include "graph/hypergraph.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>

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

    try {
        std::cout << "Loading hypergraph from: " << json_path << "\n";
        std::cout.flush();

        Hypergraph graph = Hypergraph::load_from_json(json_path);

        auto stats = graph.compute_statistics();
        std::cout << "Loaded graph with " << stats.num_nodes << " nodes and "
                  << stats.num_edges << " edges\n";

        if (stats.num_nodes > 5000) {
            std::cout << "Note: Large graph detected. HTML generation may take a moment...\n";
        }
        std::cout.flush();

        std::cout << "Exporting to HTML: " << html_path << "\n";
        std::cout.flush();

        graph.export_to_html(html_path, title);

        // Verify file was created and has content
        struct stat st;
        if (stat(html_path.c_str(), &st) != 0 || st.st_size == 0) {
            std::cerr << "Error: HTML file was not created or is empty!\n";
            return 1;
        }

        std::cout << "Done! Created " << html_path << " (" << st.st_size << " bytes)\n";
        std::cout << "Open in browser to explore the graph.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

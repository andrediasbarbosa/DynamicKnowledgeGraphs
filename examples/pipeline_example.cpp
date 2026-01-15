#include "pipeline/extraction_pipeline.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace kg;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

// Progress callback function
void progress_handler(
    const std::string& stage,
    int current,
    int total,
    const std::string& message
) {
    std::cout << "[" << stage << "] ";
    if (total > 0) {
        std::cout << current << "/" << total << " ";
        int percent = (current * 100) / total;
        std::cout << "(" << percent << "%) ";
    }
    if (!message.empty()) {
        std::cout << "- " << message;
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    print_separator("End-to-End Knowledge Extraction Pipeline");

    std::cout << "This example demonstrates the complete pipeline:\n";
    std::cout << "  PDF → Text Chunks → LLM Extraction → Hypergraph\n\n";

    // =========================================================================
    // Configuration
    // =========================================================================

    print_separator("Step 1: Configuration");

    // Try to load config from file or environment
    PipelineConfig config;

    if (argc > 1 && std::string(argv[1]) == "--config") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " --config <config.json>\n";
            return 1;
        }
        std::cout << "Loading configuration from: " << argv[2] << "\n";
        config = PipelineConfig::from_json_file(argv[2]);
    } else {
        std::cout << "Loading configuration from .llm_config.json or environment...\n";
        config = load_config_with_fallback("");
    }

    // Validate configuration
    std::string error;
    if (!config.validate(error)) {
        std::cerr << "Configuration error: " << error << "\n\n";
        std::cout << "Configuration options (in order of priority):\n\n";
        std::cout << "1. Use .llm_config.json in project root:\n";
        std::cout << "   cp .llm_config.json.example .llm_config.json\n";
        std::cout << "   # Edit .llm_config.json with your API key\n\n";
        std::cout << "2. Or use a custom config file:\n";
        std::cout << "   " << argv[0] << " --config your_config.json\n\n";
        std::cout << "3. Or set environment variables:\n";
        std::cout << "   export OPENAI_API_KEY='your-key'  # For OpenAI\n";
        std::cout << "   OR\n";
        std::cout << "   export GEMINI_API_KEY='your-key'  # For Gemini\n";
        std::cout << "   export KG_LLM_PROVIDER='gemini'\n\n";

        // Save example config
        config.llm_api_key = "your-api-key-here";
        config.llm_provider = "openai";
        config.llm_model = "gpt-4";
        config.chunking_strategy = "sentence";
        config.max_sentences = 5;
        config.rate_limit_delay_ms = 1000;
        config.verbose = true;

        config.to_json_file("example_pipeline_config.json");
        std::cout << "✓ Saved example config to: example_pipeline_config.json\n";
        std::cout << "  Edit this file and run: " << argv[0] << " --config example_pipeline_config.json\n\n";

        return 1;
    }

    std::cout << "✓ Configuration validated\n";
    std::cout << "  Provider: " << config.llm_provider << "\n";
    std::cout << "  Model: " << config.llm_model << "\n";
    std::cout << "  Chunking: " << config.chunking_strategy << "\n";
    std::cout << "  Output: " << config.output_directory << "\n\n";

    // =========================================================================
    // Initialize Pipeline
    // =========================================================================

    print_separator("Step 2: Initialize Pipeline");

    std::cout << "Creating extraction pipeline...\n";

    ExtractionPipeline pipeline(config);
    pipeline.set_progress_callback(progress_handler);

    std::cout << "✓ Pipeline initialized\n\n";

    // =========================================================================
    // Process PDFs
    // =========================================================================

    print_separator("Step 3: Process Documents");

    // Determine what to process
    std::vector<std::string> pdf_paths;

    if (argc > 1 && std::string(argv[1]) != "--config") {
        // Process specific files from command line
        for (int i = 1; i < argc; ++i) {
            pdf_paths.push_back(argv[i]);
        }
        std::cout << "Processing " << pdf_paths.size() << " file(s) from command line\n\n";
    } else {
        // Process test directory (try multiple locations)
        std::cout << "Processing PDFs from tests/ directory...\n\n";

        std::string test_dir = "tests";
        pdf_paths = find_pdf_files(test_dir, false);

        if (pdf_paths.empty()) {
            // Try from build directory
            test_dir = "../tests";
            pdf_paths = find_pdf_files(test_dir, false);
        }

        if (pdf_paths.empty()) {
            // Try from build/bin directory
            test_dir = "../../tests";
            pdf_paths = find_pdf_files(test_dir, false);
        }

        if (pdf_paths.empty()) {
            std::cout << "No PDF files found in tests/ directory.\n";
            std::cout << "Please run from project root directory or provide PDF paths:\n";
            std::cout << "Usage: " << argv[0] << " <pdf1.pdf> [pdf2.pdf ...]\n";
            return 1;
        }

        std::cout << "Found " << pdf_paths.size() << " PDF file(s):\n";
        for (const auto& path : pdf_paths) {
            std::cout << "  - " << path << "\n";
        }
        std::cout << "\n";
    }

    // Process all PDFs
    Hypergraph final_graph;

    try {
        final_graph = pipeline.process_pdfs(pdf_paths);
    } catch (const std::exception& e) {
        std::cerr << "Pipeline error: " << e.what() << "\n";
        return 1;
    }

    // =========================================================================
    // Results and Statistics
    // =========================================================================

    print_separator("Step 4: Results");

    auto stats = pipeline.get_statistics();
    stats.print_summary();

    // Export final graph
    std::string output_path = config.output_directory + "/final_graph.json";
    final_graph.export_to_json(output_path, true);
    std::cout << "✓ Saved final hypergraph to: " << output_path << "\n";

    std::string dot_path = config.output_directory + "/final_graph.dot";
    final_graph.export_to_dot(dot_path);
    std::cout << "✓ Saved visualization to: " << dot_path << "\n";
    std::cout << "  Visualize with: dot -Tpng " << dot_path << " -o final_graph.png\n";

    std::string html_path = config.output_directory + "/final_graph.html";
    final_graph.export_to_html(html_path, "Knowledge Graph - Pipeline Extraction");
    std::cout << "✓ Saved interactive HTML to: " << html_path << "\n";
    std::cout << "  Open in browser to explore the graph!\n\n";

    // Save statistics
    std::string stats_path = config.output_directory + "/pipeline_stats.json";
    std::ofstream stats_file(stats_path);
    stats_file << stats.to_json().dump(2);
    stats_file.close();
    std::cout << "✓ Saved statistics to: " << stats_path << "\n\n";

    // =========================================================================
    // Graph Analysis
    // =========================================================================

    print_separator("Step 5: Graph Analysis");

    auto graph_stats = final_graph.compute_statistics();

    std::cout << "Hypergraph Statistics:\n";
    std::cout << "  Entities (nodes): " << graph_stats.num_nodes << "\n";
    std::cout << "  Relationships (hyperedges): " << graph_stats.num_edges << "\n";
    std::cout << "  Average relation complexity: " << std::fixed << std::setprecision(2)
              << graph_stats.avg_edge_size << " entities/relation\n";
    std::cout << "  Max relation size: " << graph_stats.max_edge_size << " entities\n";
    std::cout << "  Average connectivity: " << graph_stats.avg_node_degree << " relations/entity\n\n";

    // Top hubs
    auto hubs = final_graph.get_top_hubs(10);
    if (!hubs.empty()) {
        std::cout << "Top 10 Knowledge Hubs:\n";
        std::cout << std::left << std::setw(40) << "  Entity" << std::setw(15) << "Connections\n";
        std::cout << "  " << std::string(54, '-') << "\n";

        for (const auto& [entity, degree] : hubs) {
            std::cout << "  " << std::setw(40) << entity << std::setw(15) << degree << "\n";
        }
        std::cout << "\n";
    }

    // Network topology
    auto [exponent, r_squared] = final_graph.fit_power_law();
    if (r_squared > 0.5) {
        std::cout << "Network Topology:\n";
        std::cout << "  Power law exponent: " << std::fixed << std::setprecision(3) << exponent << "\n";
        std::cout << "  R² fit quality: " << r_squared << "\n";
        if (r_squared > 0.7) {
            std::cout << "  → Network exhibits scale-free topology\n";
        }
        std::cout << "\n";
    }

    // Connected components
    auto components = final_graph.find_s_connected_components(1);
    std::cout << "Knowledge Clusters:\n";
    std::cout << "  Found " << components.size() << " connected component(s)\n";
    if (!components.empty()) {
        std::cout << "  Largest cluster: " << components[0].size() << " interconnected relations\n";
    }
    std::cout << "\n";

    // =========================================================================
    // Summary
    // =========================================================================

    print_separator("Pipeline Complete!");

    std::cout << "Successfully processed " << stats.documents_processed << " document(s)\n";
    std::cout << "Extracted " << stats.total_relations_extracted << " relations\n";
    std::cout << "Built hypergraph with " << graph_stats.num_nodes << " entities and "
              << graph_stats.num_edges << " relationships\n\n";

    std::cout << "Output files in " << config.output_directory << "/:\n";
    std::cout << "  - final_graph.json (complete hypergraph)\n";
    std::cout << "  - final_graph.dot (visualization)\n";
    std::cout << "  - final_graph.html (interactive visualization)\n";
    std::cout << "  - pipeline_stats.json (execution statistics)\n";

    if (config.save_intermediate) {
        std::cout << "  - *_graph.json (per-document graphs)\n";
    }
    if (config.save_extractions) {
        std::cout << "  - *_extractions.json (raw LLM outputs)\n";
    }

    std::cout << "\nNext steps:\n";
    std::cout << "  1. Visualize: dot -Tpng " << dot_path << " -o final_graph.png\n";
    std::cout << "  2. Analyze paths: Use hypergraph path-finding APIs\n";
    std::cout << "  3. Add more documents: Process larger corpus\n";
    std::cout << "  4. Deduplicate: Add embedding generation for semantic merging\n\n";

    print_separator("End of Pipeline Example");

    return 0;
}

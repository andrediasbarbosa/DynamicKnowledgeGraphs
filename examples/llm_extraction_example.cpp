#include "llm/llm_provider.hpp"
#include "pdf/pdf_processor.hpp"
#include "graph/hypergraph.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>

using namespace kg;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

void print_extraction_result(const ExtractionResult& result) {
    std::cout << "Chunk ID: " << result.chunk_id << "\n";
    std::cout << "Success: " << (result.success ? "✓" : "✗") << "\n";

    if (!result.success) {
        std::cout << "Error: " << result.error_message << "\n";
        return;
    }

    std::cout << "LLM Response:\n";
    std::cout << "  Model: " << result.llm_response.model << "\n";
    std::cout << "  Tokens: " << result.llm_response.total_tokens << "\n";
    std::cout << "  Latency: " << result.llm_response.latency_ms << " ms\n";

    std::cout << "\nExtracted " << result.relations.size() << " relations:\n";

    for (size_t i = 0; i < result.relations.size(); ++i) {
        const auto& rel = result.relations[i];
        std::cout << "\n  Relation " << (i + 1) << ":\n";

        std::cout << "    Sources: [";
        for (size_t j = 0; j < rel.sources.size(); ++j) {
            std::cout << rel.sources[j];
            if (j < rel.sources.size() - 1) std::cout << ", ";
        }
        std::cout << "]\n";

        std::cout << "    Relation: " << rel.relation << "\n";

        std::cout << "    Targets: [";
        for (size_t j = 0; j < rel.targets.size(); ++j) {
            std::cout << rel.targets[j];
            if (j < rel.targets.size() - 1) std::cout << ", ";
        }
        std::cout << "]\n";

        std::cout << "    Confidence: " << std::fixed << std::setprecision(2)
                  << rel.confidence << "\n";
    }
}

int main(int argc, char* argv[]) {
    print_separator("LLM Knowledge Extraction Example");

    // Create output directory
    const std::string output_dir = "output_json";
    #ifdef _WIN32
        _mkdir(output_dir.c_str());
    #else
        mkdir(output_dir.c_str(), 0755);
    #endif

    // =========================================================================
    // Example 1: Configure LLM Provider
    // =========================================================================

    print_separator("Example 1: LLM Provider Configuration");

    std::cout << "This example demonstrates LLM-based knowledge extraction.\n";
    std::cout << "Configuration options (in order of priority):\n\n";
    std::cout << "1. Create a .llm_config.json file in the project root:\n";
    std::cout << "   {\n";
    std::cout << "     \"provider\": \"openai\",  // or \"gemini\"\n";
    std::cout << "     \"api_key\": \"your-key-here\",\n";
    std::cout << "     \"model\": \"gpt-4\"  // or \"gemini-1.5-flash\"\n";
    std::cout << "   }\n\n";
    std::cout << "2. Or set environment variables:\n";
    std::cout << "   export OPENAI_API_KEY='your-key'  # For OpenAI\n";
    std::cout << "   OR\n";
    std::cout << "   export GEMINI_API_KEY='your-key'  # For Gemini\n";
    std::cout << "   export KG_LLM_PROVIDER='gemini'\n\n";

    // Try to create provider from config file (falls back to environment)
    auto provider = LLMProviderFactory::create_from_config_file();

    if (!provider) {
        std::cout << "⚠ No API key found in config file or environment.\n";
        std::cout << "Running in demo mode with sample data...\n\n";

        // Demo mode - show what would happen
        print_separator("Demo: Sample Extraction Output");

        std::cout << "Sample Text:\n";
        std::cout << "\"The House of Wisdom in Baghdad sponsored translators.\n";
        std::cout << "Those translators produced Arabic versions of Greek geometry.\"\n\n";

        std::cout << "Expected Extraction:\n";
        std::cout << "{\n";
        std::cout << "  \"relations\": [\n";
        std::cout << "    {\n";
        std::cout << "      \"sources\": [\"House of Wisdom\", \"Baghdad\"],\n";
        std::cout << "      \"relation\": \"sponsored\",\n";
        std::cout << "      \"targets\": [\"translators\"],\n";
        std::cout << "      \"confidence\": 0.95\n";
        std::cout << "    },\n";
        std::cout << "    {\n";
        std::cout << "      \"sources\": [\"translators\"],\n";
        std::cout << "      \"relation\": \"produced\",\n";
        std::cout << "      \"targets\": [\"Arabic versions\", \"Greek geometry\"],\n";
        std::cout << "      \"confidence\": 0.90\n";
        std::cout << "    }\n";
        std::cout << "  ]\n";
        std::cout << "}\n\n";

        print_separator("Demo: Integration with Hypergraph");

        std::cout << "After extraction, relations would be added to hypergraph:\n\n";

        Hypergraph demo_graph;

        demo_graph.add_hyperedge(
            {"House of Wisdom", "Baghdad"},
            "sponsored",
            {"translators"}
        );

        demo_graph.add_hyperedge(
            {"translators"},
            "produced",
            {"Arabic versions", "Greek geometry"}
        );

        auto stats = demo_graph.compute_statistics();
        std::cout << "Hypergraph built:\n";
        std::cout << "  Nodes: " << stats.num_nodes << "\n";
        std::cout << "  Hyperedges: " << stats.num_edges << "\n\n";

        demo_graph.export_to_json(output_dir + "/demo_graph.json");
        std::cout << "✓ Saved demo graph to: " << output_dir << "/demo_graph.json\n";

        print_separator("To Run Real Extraction");

        std::cout << "Create .llm_config.json with your API key:\n\n";
        std::cout << "  cp .llm_config.json.example .llm_config.json\n";
        std::cout << "  # Edit .llm_config.json with your API key\n";
        std::cout << "  ./build/bin/llm_extraction_example\n\n";
        std::cout << "Or provide your own text:\n\n";
        std::cout << "  ./build/bin/llm_extraction_example \"Your text to extract from\"\n\n";

        return 0;
    }

    // =========================================================================
    // Real LLM Extraction
    // =========================================================================

    std::cout << "✓ LLM Provider configured:\n";
    std::cout << "  Provider: " << provider->get_provider_name() << "\n";
    std::cout << "  Model: " << provider->get_model() << "\n\n";

    // Get text to extract from
    std::string text_to_extract;

    if (argc > 1) {
        // Use command-line argument
        text_to_extract = argv[1];
        std::cout << "Using text from command line argument\n";
    } else {
        // Use sample text
        text_to_extract = R"(
The House of Wisdom in Baghdad sponsored translators.
Those translators produced Arabic versions of Greek geometry.
Hunayn ibn Ishaq maintained a glossary, and the glossary standardized technical terms.
)";
        std::cout << "Using sample text\n";
    }

    std::cout << "\nText to extract from:\n";
    std::cout << "\"" << text_to_extract << "\"\n";

    // =========================================================================
    // Example 2: Extract Relations
    // =========================================================================

    print_separator("Example 2: Extracting Relations with LLM");

    LLMConfig config = provider->get_config();
    config.verbose = true;
    config.temperature = 0.0;  // Deterministic for extraction
    provider->set_config(config);

    std::cout << "Calling LLM to extract relations...\n\n";

    auto result = provider->extract_relations(text_to_extract, "sample_chunk_0");

    print_extraction_result(result);

    // =========================================================================
    // Example 3: Build Hypergraph from Extraction
    // =========================================================================

    if (result.success && !result.relations.empty()) {
        print_separator("Example 3: Building Hypergraph from Extraction");

        Hypergraph graph;

        for (const auto& rel : result.relations) {
            if (rel.sources.empty() || rel.targets.empty()) {
                continue;
            }

            HyperEdge edge;
            edge.sources = rel.sources;
            edge.relation = rel.relation;
            edge.targets = rel.targets;
            edge.confidence = rel.confidence;
            edge.source_document = "llm_extraction_example";
            edge.source_chunk_id = result.chunk_id;

            graph.add_hyperedge(edge);
        }

        auto stats = graph.compute_statistics();

        std::cout << "Built hypergraph from extracted relations:\n";
        std::cout << "  Entities (nodes): " << stats.num_nodes << "\n";
        std::cout << "  Relationships (hyperedges): " << stats.num_edges << "\n";
        std::cout << "  Average entities per relation: "
                  << std::fixed << std::setprecision(2) << stats.avg_edge_size << "\n\n";

        // Export
        std::string graph_path = output_dir + "/extracted_graph.json";
        graph.export_to_json(graph_path);
        std::cout << "✓ Saved graph to: " << graph_path << "\n";

        std::string dot_path = output_dir + "/extracted_graph.dot";
        graph.export_to_dot(dot_path);
        std::cout << "✓ Saved DOT to: " << dot_path << "\n";
        std::cout << "  Visualize: dot -Tpng " << dot_path
                  << " -o " << output_dir << "/extracted_graph.png\n";

        std::string html_path = output_dir + "/extracted_graph.html";
        graph.export_to_html(html_path, "LLM Extracted Knowledge Graph");
        std::cout << "✓ Saved interactive HTML to: " << html_path << "\n";
        std::cout << "  Open in browser to explore the graph!\n";
    }

    // =========================================================================
    // Example 4: Batch Extraction (if PDF available)
    // =========================================================================

    if (PDFProcessor::is_available()) {
        print_separator("Example 4: Batch Extraction from PDF");

        std::cout << "Loading PDF and extracting in batches...\n\n";

        try {
            PDFProcessor pdf_processor;
            pdf_processor.set_verbose(false);

            // Find PDF path (try multiple locations)
            std::string pdf_path = "../../tests/1page.pdf";
            struct stat buffer;

            if (stat(pdf_path.c_str(), &buffer) != 0) {
                // Try from build directory
                pdf_path = "../tests/1page.pdf";
                if (stat(pdf_path.c_str(), &buffer) != 0) {
                    // Try from build/bin directory
                    pdf_path = "../../tests/1page.pdf";
                    if (stat(pdf_path.c_str(), &buffer) != 0) {
                        throw std::runtime_error(
                            "Could not find tests/1page.pdf. "
                            "Please run from project root directory."
                        );
                    }
                }
            }

            // Load and chunk PDF
            PDFDocument doc = pdf_processor.load_pdf(pdf_path);
            std::cout << "Loaded PDF: " << doc.document_id
                      << " (" << doc.get_total_words() << " words)\n";

            // Use sentence chunking for fine-grained extraction
            SentenceChunking chunker(3, 400);  // 3 sentences or 400 chars
            auto chunks = pdf_processor.chunk_document(doc, chunker);
            std::cout << "Created " << chunks.size() << " chunks\n\n";

            // Extract from first few chunks
            int max_chunks = std::min(2, static_cast<int>(chunks.size()));
            std::cout << "Extracting from first " << max_chunks << " chunks...\n\n";

            Hypergraph full_graph;

            for (int i = 0; i < max_chunks; ++i) {
                const auto& chunk = chunks[i];

                std::cout << "Processing chunk " << (i + 1) << "/"
                          << max_chunks << "...\n";

                auto chunk_result = provider->extract_relations(
                    chunk.text,
                    chunk.chunk_id
                );

                if (chunk_result.success) {
                    std::cout << "  ✓ Extracted " << chunk_result.relations.size()
                              << " relations\n";

                    // Add to graph
                    for (const auto& rel : chunk_result.relations) {
                        if (!rel.sources.empty() && !rel.targets.empty()) {
                            HyperEdge edge;
                            edge.sources = rel.sources;
                            edge.relation = rel.relation;
                            edge.targets = rel.targets;
                            edge.confidence = rel.confidence;
                            edge.source_document = doc.document_id;
                            edge.source_chunk_id = chunk.chunk_id;
                            edge.source_page = chunk.page_number;

                            full_graph.add_hyperedge(edge);
                        }
                    }
                } else {
                    std::cout << "  ✗ Extraction failed: "
                              << chunk_result.error_message << "\n";
                }

                // Rate limiting
                if (i < max_chunks - 1) {
                    std::cout << "  Waiting 2s before next request...\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }

            std::cout << "\nBatch extraction complete!\n\n";

            auto final_stats = full_graph.compute_statistics();
            std::cout << "Final hypergraph:\n";
            std::cout << "  Total entities: " << final_stats.num_nodes << "\n";
            std::cout << "  Total relationships: " << final_stats.num_edges << "\n\n";

            // Export final graph
            std::string batch_graph_path = output_dir + "/batch_extracted_graph.json";
            full_graph.export_to_json(batch_graph_path);
            std::cout << "✓ Saved complete graph to: " << batch_graph_path << "\n";

            std::string batch_html_path = output_dir + "/batch_extracted_graph.html";
            full_graph.export_to_html(batch_html_path, "Knowledge Graph - PDF Extraction");
            std::cout << "✓ Saved interactive visualization to: " << batch_html_path << "\n";

        } catch (const std::exception& e) {
            std::cerr << "Error in batch extraction: " << e.what() << "\n";
        }
    }

    // =========================================================================
    // Summary
    // =========================================================================

    print_separator("Summary");

    std::cout << "This example demonstrated:\n";
    std::cout << "  ✓ Configuring LLM providers (OpenAI/Gemini)\n";
    std::cout << "  ✓ Extracting relations from text using LLMs\n";
    std::cout << "  ✓ Building hypergraphs from extracted relations\n";
    std::cout << "  ✓ Batch processing with rate limiting\n";
    std::cout << "  ✓ Integration with PDF processing\n\n";

    std::cout << "Complete Pipeline:\n";
    std::cout << "  PDF → Text Chunks → LLM Extraction → Relations → Hypergraph\n\n";

    std::cout << "Next steps:\n";
    std::cout << "  1. Process larger document collections\n";
    std::cout << "  2. Use node deduplication (embeddings)\n";
    std::cout << "  3. Implement agentic reasoning on graph\n";

    print_separator("End of LLM Extraction Example");

    return 0;
}

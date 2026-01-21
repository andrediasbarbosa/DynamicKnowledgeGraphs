#include "pipeline/extraction_pipeline.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>

using json = nlohmann::json;

namespace {

bool is_ascii_alpha_word(const std::string& text) {
    if (text.empty()) return false;
    for (unsigned char c : text) {
        if (c >= 128 || !std::isalpha(c)) return false;
    }
    return true;
}

std::string singularize_ascii_word(const std::string& word) {
    if (word.size() <= 3) return word;
    if (!is_ascii_alpha_word(word)) return word;

    if (word.size() >= 4 && word.compare(word.size() - 3, 3, "ies") == 0) {
        return word.substr(0, word.size() - 3) + "y";
    }
    if (word.size() >= 4 &&
        (word.compare(word.size() - 4, 4, "ches") == 0 ||
         word.compare(word.size() - 4, 4, "shes") == 0 ||
         word.compare(word.size() - 3, 3, "xes") == 0 ||
         word.compare(word.size() - 3, 3, "ses") == 0 ||
         word.compare(word.size() - 3, 3, "zes") == 0)) {
        return word.substr(0, word.size() - 2);
    }
    if (word.size() >= 2 && word.compare(word.size() - 2, 2, "ss") == 0) {
        return word;
    }
    if (!word.empty() && word.back() == 's') {
        return word.substr(0, word.size() - 1);
    }
    return word;
}

std::string normalize_entity_label(const std::string& label) {
    if (label.empty()) return label;
    if (label.find(' ') != std::string::npos) return label;
    return singularize_ascii_word(label);
}

}  // namespace

namespace kg {

// ============================================================================
// PipelineConfig
// ============================================================================

PipelineConfig PipelineConfig::from_json_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    json j;
    file >> j;

    PipelineConfig config;

    // LLM config - support both formats:
    // Full format: llm_provider, llm_api_key, llm_model
    // Short format: provider, api_key, model (from .llm_config.json)
    if (j.contains("llm_provider")) {
        config.llm_provider = j["llm_provider"];
    } else if (j.contains("provider")) {
        config.llm_provider = j["provider"];
    }

    if (j.contains("llm_api_key")) {
        config.llm_api_key = j["llm_api_key"];
    } else if (j.contains("api_key")) {
        config.llm_api_key = j["api_key"];
    }

    if (j.contains("llm_model")) {
        config.llm_model = j["llm_model"];
    } else if (j.contains("model")) {
        config.llm_model = j["model"];
    }

    if (j.contains("llm_temperature")) {
        config.llm_temperature = j["llm_temperature"];
    } else if (j.contains("temperature")) {
        config.llm_temperature = j["temperature"];
    }

    if (j.contains("llm_max_tokens")) {
        config.llm_max_tokens = j["llm_max_tokens"];
    } else if (j.contains("max_tokens")) {
        config.llm_max_tokens = j["max_tokens"];
    }

    if (j.contains("llm_max_retries")) {
        config.llm_max_retries = j["llm_max_retries"];
    } else if (j.contains("max_retries")) {
        config.llm_max_retries = j["max_retries"];
    }

    if (j.contains("llm_timeout_seconds")) {
        config.llm_timeout_seconds = j["llm_timeout_seconds"];
    } else if (j.contains("timeout_seconds")) {
        config.llm_timeout_seconds = j["timeout_seconds"];
    }

    // Chunking config
    if (j.contains("chunking_strategy")) config.chunking_strategy = j["chunking_strategy"];
    if (j.contains("chunk_size")) config.chunk_size = j["chunk_size"];
    if (j.contains("chunk_overlap")) config.chunk_overlap = j["chunk_overlap"];
    if (j.contains("max_paragraphs")) config.max_paragraphs = j["max_paragraphs"];
    if (j.contains("max_sentences")) config.max_sentences = j["max_sentences"];
    if (j.contains("max_chars_per_chunk")) config.max_chars_per_chunk = j["max_chars_per_chunk"];

    // Processing config
    if (j.contains("batch_size")) config.batch_size = j["batch_size"];
    if (j.contains("rate_limit_delay_ms")) config.rate_limit_delay_ms = j["rate_limit_delay_ms"];
    if (j.contains("parallel_processing")) config.parallel_processing = j["parallel_processing"];

    // Deduplication config
    if (j.contains("enable_deduplication")) config.enable_deduplication = j["enable_deduplication"];
    if (j.contains("similarity_threshold")) config.similarity_threshold = j["similarity_threshold"];

    // Output config
    if (j.contains("output_directory")) config.output_directory = j["output_directory"];
    if (j.contains("save_intermediate")) config.save_intermediate = j["save_intermediate"];
    if (j.contains("save_extractions")) config.save_extractions = j["save_extractions"];
    if (j.contains("verbose")) config.verbose = j["verbose"];

    // Custom prompt
    if (j.contains("custom_system_prompt")) config.custom_system_prompt = j["custom_system_prompt"];

    return config;
}

void PipelineConfig::to_json_file(const std::string& path) const {
    json j;

    // LLM config
    j["llm_provider"] = llm_provider;
    j["llm_api_key"] = llm_api_key.empty() ? "***REDACTED***" : llm_api_key;
    j["llm_model"] = llm_model;
    j["llm_temperature"] = llm_temperature;
    j["llm_max_tokens"] = llm_max_tokens;
    j["llm_max_retries"] = llm_max_retries;
    j["llm_timeout_seconds"] = llm_timeout_seconds;

    // Chunking config
    j["chunking_strategy"] = chunking_strategy;
    j["chunk_size"] = chunk_size;
    j["chunk_overlap"] = chunk_overlap;
    j["max_paragraphs"] = max_paragraphs;
    j["max_sentences"] = max_sentences;
    j["max_chars_per_chunk"] = max_chars_per_chunk;

    // Processing config
    j["batch_size"] = batch_size;
    j["rate_limit_delay_ms"] = rate_limit_delay_ms;
    j["parallel_processing"] = parallel_processing;

    // Deduplication config
    j["enable_deduplication"] = enable_deduplication;
    j["similarity_threshold"] = similarity_threshold;

    // Output config
    j["output_directory"] = output_directory;
    j["save_intermediate"] = save_intermediate;
    j["save_extractions"] = save_extractions;
    j["verbose"] = verbose;

    // Custom prompt
    if (!custom_system_prompt.empty()) {
        j["custom_system_prompt"] = custom_system_prompt;
    }

    std::ofstream file(path);
    file << j.dump(2);
}

PipelineConfig PipelineConfig::from_environment() {
    PipelineConfig config;

    // LLM configuration from environment
    const char* provider = std::getenv("KG_LLM_PROVIDER");
    if (provider) config.llm_provider = provider;

    const char* api_key = nullptr;
    if (config.llm_provider == "openai") {
        api_key = std::getenv("OPENAI_API_KEY");
        if (!api_key) api_key = std::getenv("KG_OPENAI_API_KEY");
        config.llm_model = "gpt-4";
    } else if (config.llm_provider == "gemini") {
        api_key = std::getenv("GEMINI_API_KEY");
        if (!api_key) api_key = std::getenv("KG_GEMINI_API_KEY");
        config.llm_model = "gemini-1.5-flash";
    }

    if (api_key) config.llm_api_key = api_key;

    const char* model = std::getenv("KG_LLM_MODEL");
    if (model) config.llm_model = model;

    const char* output_dir = std::getenv("KG_OUTPUT_DIR");
    if (output_dir) config.output_directory = output_dir;

    return config;
}

bool PipelineConfig::validate(std::string& error_message) const {
    if (llm_api_key.empty()) {
        error_message = "LLM API key is required";
        return false;
    }

    if (llm_provider != "openai" && llm_provider != "gemini") {
        error_message = "LLM provider must be 'openai' or 'gemini'";
        return false;
    }

    if (chunking_strategy != "fixed" && chunking_strategy != "page" &&
        chunking_strategy != "paragraph" && chunking_strategy != "sentence") {
        error_message = "Invalid chunking strategy: " + chunking_strategy;
        return false;
    }

    if (chunk_size <= chunk_overlap) {
        error_message = "Chunk size must be greater than overlap";
        return false;
    }

    if (similarity_threshold < 0.0 || similarity_threshold > 1.0) {
        error_message = "Similarity threshold must be between 0.0 and 1.0";
        return false;
    }

    return true;
}

// ============================================================================
// PipelineStatistics
// ============================================================================

void PipelineStatistics::print_summary() const {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Pipeline Execution Summary\n";
    std::cout << std::string(70, '=') << "\n\n";

    std::cout << "Document Processing:\n";
    std::cout << "  Processed: " << documents_processed << "\n";
    std::cout << "  Failed: " << documents_failed << "\n";
    std::cout << "  Total pages: " << total_pages << "\n";
    std::cout << "  Total chunks: " << total_chunks << "\n\n";

    std::cout << "LLM Extraction:\n";
    std::cout << "  API calls: " << extraction_calls << "\n";
    std::cout << "  Successes: " << extraction_successes << "\n";
    std::cout << "  Failures: " << extraction_failures << "\n";
    std::cout << "  Relations extracted: " << total_relations_extracted << "\n\n";

    std::cout << "Token Usage:\n";
    std::cout << "  Prompt tokens: " << total_prompt_tokens << "\n";
    std::cout << "  Completion tokens: " << total_completion_tokens << "\n";
    std::cout << "  Total tokens: " << total_tokens << "\n\n";

    std::cout << "Timing:\n";
    std::cout << "  Total time: " << total_time_seconds << " seconds\n";
    std::cout << "  PDF processing: " << pdf_processing_time_seconds << " seconds\n";
    std::cout << "  LLM calls: " << llm_time_seconds << " seconds\n";
    std::cout << "  Graph building: " << graph_building_time_seconds << " seconds\n\n";

    std::cout << "Final Hypergraph:\n";
    std::cout << "  Nodes: " << final_nodes << "\n";
    std::cout << "  Edges: " << final_edges << "\n";
    if (nodes_before_dedup > 0) {
        std::cout << "  Nodes before dedup: " << nodes_before_dedup << "\n";
        std::cout << "  Nodes merged: " << nodes_merged << "\n";
    }

    std::cout << "\n" << std::string(70, '=') << "\n\n";
}

json PipelineStatistics::to_json() const {
    json j;

    j["documents_processed"] = documents_processed;
    j["documents_failed"] = documents_failed;
    j["total_pages"] = total_pages;
    j["total_chunks"] = total_chunks;

    j["extraction_calls"] = extraction_calls;
    j["extraction_successes"] = extraction_successes;
    j["extraction_failures"] = extraction_failures;
    j["total_relations_extracted"] = total_relations_extracted;

    j["total_prompt_tokens"] = total_prompt_tokens;
    j["total_completion_tokens"] = total_completion_tokens;
    j["total_tokens"] = total_tokens;

    j["total_time_seconds"] = total_time_seconds;
    j["pdf_processing_time_seconds"] = pdf_processing_time_seconds;
    j["llm_time_seconds"] = llm_time_seconds;
    j["graph_building_time_seconds"] = graph_building_time_seconds;

    j["final_nodes"] = final_nodes;
    j["final_edges"] = final_edges;
    j["nodes_before_dedup"] = nodes_before_dedup;
    j["nodes_merged"] = nodes_merged;

    return j;
}

// ============================================================================
// ExtractionPipeline
// ============================================================================

ExtractionPipeline::ExtractionPipeline(const PipelineConfig& config)
    : config_(config) {
    std::string error;
    if (!config_.validate(error)) {
        throw std::invalid_argument("Invalid configuration: " + error);
    }

    initialize_components();
}

void ExtractionPipeline::initialize_components() {
    // Initialize PDF processor
    pdf_processor_ = std::make_unique<PDFProcessor>();
    pdf_processor_->set_verbose(config_.verbose);

    // Initialize LLM provider
    LLMConfig llm_config;
    llm_config.api_key = config_.llm_api_key;
    llm_config.model = config_.llm_model;
    llm_config.temperature = config_.llm_temperature;
    llm_config.max_tokens = config_.llm_max_tokens;
    llm_config.max_retries = config_.llm_max_retries;
    llm_config.timeout_seconds = config_.llm_timeout_seconds;
    llm_config.verbose = config_.verbose;

    llm_provider_ = LLMProviderFactory::create(config_.llm_provider, llm_config);

    // Initialize chunking strategy
    chunking_strategy_ = create_chunking_strategy();

    // Create output directory
    #ifdef _WIN32
        _mkdir(config_.output_directory.c_str());
    #else
        mkdir(config_.output_directory.c_str(), 0755);
    #endif
}

std::unique_ptr<ChunkingStrategy> ExtractionPipeline::create_chunking_strategy() {
    if (config_.chunking_strategy == "fixed") {
        return std::make_unique<FixedSizeChunking>(
            config_.chunk_size,
            config_.chunk_overlap
        );
    } else if (config_.chunking_strategy == "page") {
        return std::make_unique<PageBasedChunking>();
    } else if (config_.chunking_strategy == "paragraph") {
        return std::make_unique<ParagraphChunking>(
            config_.max_paragraphs,
            config_.max_chars_per_chunk
        );
    } else if (config_.chunking_strategy == "sentence") {
        return std::make_unique<SentenceChunking>(
            config_.max_sentences,
            config_.max_chars_per_chunk
        );
    }

    // Default to sentence chunking
    return std::make_unique<SentenceChunking>(5, 800);
}

Hypergraph ExtractionPipeline::process_pdf(const std::string& pdf_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    report_progress("Loading PDF", 0, 1, pdf_path);

    // Load PDF
    auto pdf_start = std::chrono::high_resolution_clock::now();
    PDFDocument doc = pdf_processor_->load_pdf(pdf_path);
    auto pdf_end = std::chrono::high_resolution_clock::now();

    stats_.documents_processed++;
    stats_.total_pages += doc.metadata.num_pages;
    stats_.pdf_processing_time_seconds += std::chrono::duration<double>(
        pdf_end - pdf_start
    ).count();

    // Process document
    Hypergraph graph = process_document(doc);

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.total_time_seconds += std::chrono::duration<double>(
        end_time - start_time
    ).count();

    return graph;
}

Hypergraph ExtractionPipeline::process_document(const PDFDocument& doc) {
    report_progress("Chunking", 0, 1, doc.document_id);

    // Chunk document
    auto chunks = pdf_processor_->chunk_document(doc, *chunking_strategy_);
    stats_.total_chunks += chunks.size();

    if (config_.verbose) {
        std::cout << "Created " << chunks.size() << " chunks from "
                  << doc.document_id << "\n";
    }

    // Extract relations from chunks
    auto extraction_results = extract_from_chunks(chunks, doc.document_id);

    // Build graph
    auto graph_start = std::chrono::high_resolution_clock::now();
    Hypergraph graph = build_graph_from_results(extraction_results, doc.document_id);
    auto graph_end = std::chrono::high_resolution_clock::now();

    stats_.graph_building_time_seconds += std::chrono::duration<double>(
        graph_end - graph_start
    ).count();

    // Save intermediate
    if (config_.save_intermediate) {
        save_intermediate_graph(graph, doc.document_id);
    }

    return graph;
}

std::vector<ExtractionResult> ExtractionPipeline::extract_from_chunks(
    const std::vector<TextChunk>& chunks,
    const std::string& document_id
) {
    std::vector<ExtractionResult> results;

    for (size_t i = 0; i < chunks.size(); ++i) {
        report_progress("Extracting", i + 1, chunks.size(), chunks[i].chunk_id);

        auto llm_start = std::chrono::high_resolution_clock::now();

        // Extract relations
        ExtractionResult result = llm_provider_->extract_relations(
            chunks[i].text,
            chunks[i].chunk_id,
            config_.custom_system_prompt
        );

        auto llm_end = std::chrono::high_resolution_clock::now();
        stats_.llm_time_seconds += std::chrono::duration<double>(
            llm_end - llm_start
        ).count();

        // Update statistics
        stats_.extraction_calls++;
        if (result.success) {
            stats_.extraction_successes++;
            stats_.total_relations_extracted += result.relations.size();

            stats_.total_prompt_tokens += result.llm_response.prompt_tokens;
            stats_.total_completion_tokens += result.llm_response.completion_tokens;
            stats_.total_tokens += result.llm_response.total_tokens;
        } else {
            stats_.extraction_failures++;
            if (config_.verbose) {
                std::cerr << "Extraction failed for " << chunks[i].chunk_id
                          << ": " << result.error_message << "\n";
            }
        }

        results.push_back(result);

        // Rate limiting
        if (i < chunks.size() - 1) {
            apply_rate_limit();
        }
    }

    // Save extraction results
    if (config_.save_extractions) {
        save_extraction_results(results, document_id);
    }

    return results;
}

Hypergraph ExtractionPipeline::build_graph_from_results(
    const std::vector<ExtractionResult>& results,
    const std::string& document_id
) {
    Hypergraph graph;

    for (const auto& result : results) {
        if (!result.success) continue;

        for (const auto& rel : result.relations) {
            if (rel.sources.empty() || rel.targets.empty()) continue;

            HyperEdge edge;
            edge.sources.reserve(rel.sources.size());
            for (const auto& src : rel.sources) {
                edge.sources.push_back(normalize_entity_label(src));
            }
            edge.relation = rel.relation;
            edge.targets.reserve(rel.targets.size());
            for (const auto& tgt : rel.targets) {
                edge.targets.push_back(normalize_entity_label(tgt));
            }
            edge.confidence = rel.confidence;
            edge.source_document = document_id;
            edge.source_chunk_id = result.chunk_id;

            // Copy properties
            edge.properties = rel.properties;

            graph.add_hyperedge(edge);
        }
    }

    return graph;
}

Hypergraph ExtractionPipeline::process_pdfs(const std::vector<std::string>& pdf_paths) {
    std::vector<Hypergraph> graphs;

    for (size_t i = 0; i < pdf_paths.size(); ++i) {
        report_progress("Processing PDF", i + 1, pdf_paths.size(), pdf_paths[i]);

        try {
            Hypergraph graph = process_pdf(pdf_paths[i]);
            graphs.push_back(std::move(graph));
        } catch (const std::exception& e) {
            stats_.documents_failed++;
            if (config_.verbose) {
                std::cerr << "Failed to process " << pdf_paths[i]
                          << ": " << e.what() << "\n";
            }
        }
    }

    // Merge all graphs
    report_progress("Merging graphs", 0, 1);
    return merge_graphs(graphs);
}

Hypergraph ExtractionPipeline::process_directory(
    const std::string& directory_path,
    bool recursive
) {
    auto pdf_files = find_pdf_files(directory_path, recursive);

    if (config_.verbose) {
        std::cout << "Found " << pdf_files.size() << " PDF files in "
                  << directory_path << "\n";
    }

    return process_pdfs(pdf_files);
}

Hypergraph ExtractionPipeline::extract_from_text(
    const std::string& text,
    const std::string& document_id
) {
    // Create single chunk
    TextChunk chunk;
    chunk.text = text;
    chunk.document_id = document_id;
    chunk.chunk_id = document_id + "_chunk_0";
    chunk.chunk_index = 0;

    std::vector<TextChunk> chunks = {chunk};

    // Extract and build graph
    auto results = extract_from_chunks(chunks, document_id);
    return build_graph_from_results(results, document_id);
}

Hypergraph ExtractionPipeline::merge_graphs(const std::vector<Hypergraph>& graphs) {
    if (graphs.empty()) {
        return Hypergraph();
    }

    if (graphs.size() == 1) {
        Hypergraph result = graphs[0];

        // Apply deduplication if enabled
        if (config_.enable_deduplication) {
            auto stats = result.compute_statistics();
            stats_.nodes_before_dedup = stats.num_nodes;

            // Note: merge_similar_nodes requires embeddings
            // For now, we skip this step
            // In future: add embedding generation and call:
            // result.merge_similar_nodes(config_.similarity_threshold);

            auto final_stats = result.compute_statistics();
            stats_.nodes_merged = stats_.nodes_before_dedup - final_stats.num_nodes;
        }

        auto final_stats = result.compute_statistics();
        stats_.final_nodes = final_stats.num_nodes;
        stats_.final_edges = final_stats.num_edges;

        return result;
    }

    // Merge multiple graphs by adding all edges
    Hypergraph result;
    for (const auto& g : graphs) {
        // Get all hyperedges from each graph and add to result
        auto edges = g.get_all_edges();
        for (const auto& edge : edges) {
            result.add_hyperedge(edge);
        }
    }

    // Apply deduplication
    if (config_.enable_deduplication) {
        auto stats = result.compute_statistics();
        stats_.nodes_before_dedup = stats.num_nodes;

        // Future: add embedding-based deduplication here

        auto final_stats = result.compute_statistics();
        stats_.nodes_merged = stats_.nodes_before_dedup - final_stats.num_nodes;
    }

    auto final_stats = result.compute_statistics();
    stats_.final_nodes = final_stats.num_nodes;
    stats_.final_edges = final_stats.num_edges;

    return result;
}

void ExtractionPipeline::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = callback;
}

void ExtractionPipeline::reset_statistics() {
    stats_ = PipelineStatistics();
}

void ExtractionPipeline::set_config(const PipelineConfig& config) {
    config_ = config;
    std::string error;
    if (!config_.validate(error)) {
        throw std::invalid_argument("Invalid configuration: " + error);
    }
    initialize_components();
}

void ExtractionPipeline::report_progress(
    const std::string& stage,
    int current,
    int total,
    const std::string& message
) {
    if (progress_callback_) {
        progress_callback_(stage, current, total, message);
    } else if (config_.verbose && total > 0) {
        std::cout << "[" << stage << "] " << current << "/" << total;
        if (!message.empty()) {
            std::cout << " - " << message;
        }
        std::cout << "\n";
    }
}

void ExtractionPipeline::save_intermediate_graph(
    const Hypergraph& graph,
    const std::string& document_id
) {
    std::string path = config_.output_directory + "/" + document_id + "_graph.json";
    graph.export_to_json(path);

    if (config_.verbose) {
        std::cout << "Saved intermediate graph: " << path << "\n";
    }
}

void ExtractionPipeline::save_extraction_results(
    const std::vector<ExtractionResult>& results,
    const std::string& document_id
) {
    json j = json::array();

    for (const auto& result : results) {
        json result_json;
        result_json["chunk_id"] = result.chunk_id;
        result_json["success"] = result.success;

        if (result.success) {
            result_json["relations"] = json::array();
            for (const auto& rel : result.relations) {
                json rel_json;
                rel_json["sources"] = rel.sources;
                rel_json["relation"] = rel.relation;
                rel_json["targets"] = rel.targets;
                rel_json["confidence"] = rel.confidence;
                result_json["relations"].push_back(rel_json);
            }
        } else {
            result_json["error"] = result.error_message;
        }

        j.push_back(result_json);
    }

    std::string path = config_.output_directory + "/" + document_id + "_extractions.json";
    std::ofstream file(path);
    file << j.dump(2);

    if (config_.verbose) {
        std::cout << "Saved extraction results: " << path << "\n";
    }
}

void ExtractionPipeline::apply_rate_limit() {
    if (config_.rate_limit_delay_ms > 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.rate_limit_delay_ms)
        );
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

PipelineConfig create_default_config() {
    PipelineConfig config;
    // Use environment for API key
    config = PipelineConfig::from_environment();
    return config;
}

bool validate_path(const std::string& path, std::string& error_message) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        error_message = "Path does not exist: " + path;
        return false;
    }
    return true;
}

PipelineConfig load_config_with_fallback(const std::string& config_path) {
    std::vector<std::string> paths_to_try;

    // If specific path provided, try it first
    if (!config_path.empty()) {
        paths_to_try.push_back(config_path);
    }

    // Try .llm_config.json in multiple locations
    paths_to_try.push_back(".llm_config.json");
    paths_to_try.push_back("../.llm_config.json");
    paths_to_try.push_back("../../.llm_config.json");

    for (const auto& path : paths_to_try) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            try {
                auto config = PipelineConfig::from_json_file(path);
                // If config loaded successfully and has API key, use it
                if (!config.llm_api_key.empty()) {
                    return config;
                }
            } catch (...) {
                // Continue to next path
            }
        }
    }

    // Fallback to environment
    return PipelineConfig::from_environment();
}

} // namespace kg

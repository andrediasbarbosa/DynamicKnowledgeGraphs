#pragma once

#include "graph/hypergraph.hpp"
#include "pdf/pdf_processor.hpp"
#include "llm/llm_provider.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace kg {

// ============================================================================
// Pipeline Configuration
// ============================================================================

/**
 * @brief Configuration for the extraction pipeline
 */
struct PipelineConfig {
    // LLM Configuration
    std::string llm_provider = "openai";    ///< "openai" or "gemini"
    std::string llm_api_key;                ///< API key
    std::string llm_model = "gpt-4";        ///< Model name
    double llm_temperature = 0.0;           ///< Temperature (0.0 = deterministic)
    int llm_max_tokens = 2000;              ///< Max completion tokens
    int llm_max_retries = 3;                ///< Retry attempts
    int llm_timeout_seconds = 60;           ///< Request timeout

    // Chunking Configuration
    std::string chunking_strategy = "sentence";  ///< "fixed", "page", "paragraph", "sentence"
    int chunk_size = 500;                   ///< For fixed-size chunking
    int chunk_overlap = 100;                ///< For fixed-size chunking
    int max_paragraphs = 3;                 ///< For paragraph chunking
    int max_sentences = 5;                  ///< For sentence chunking
    int max_chars_per_chunk = 800;          ///< Max chars for semantic chunking

    // Processing Configuration
    int batch_size = 10;                    ///< Process N documents at a time
    int rate_limit_delay_ms = 1000;         ///< Delay between LLM calls
    bool parallel_processing = false;       ///< Enable parallel processing (future)

    // Deduplication Configuration
    bool enable_deduplication = true;       ///< Enable node deduplication
    double similarity_threshold = 0.85;     ///< Similarity threshold for merging

    // Output Configuration
    std::string output_directory = "output_json";  ///< Output directory
    bool save_intermediate = true;          ///< Save intermediate graphs
    bool save_extractions = true;           ///< Save raw extraction results
    bool verbose = true;                    ///< Verbose logging

    // Extraction Prompt (optional override)
    std::string custom_system_prompt;       ///< Override default system prompt

    /**
     * @brief Load configuration from JSON file
     */
    static PipelineConfig from_json_file(const std::string& path);

    /**
     * @brief Save configuration to JSON file
     */
    void to_json_file(const std::string& path) const;

    /**
     * @brief Load from environment variables
     */
    static PipelineConfig from_environment();

    /**
     * @brief Validate configuration
     */
    bool validate(std::string& error_message) const;
};

// ============================================================================
// Pipeline Statistics
// ============================================================================

/**
 * @brief Statistics from pipeline execution
 */
struct PipelineStatistics {
    // Document processing
    int documents_processed = 0;
    int documents_failed = 0;
    int total_pages = 0;
    int total_chunks = 0;

    // Extraction stats
    int extraction_calls = 0;
    int extraction_successes = 0;
    int extraction_failures = 0;
    int total_relations_extracted = 0;

    // Token usage
    int total_prompt_tokens = 0;
    int total_completion_tokens = 0;
    int total_tokens = 0;

    // Timing
    double total_time_seconds = 0.0;
    double pdf_processing_time_seconds = 0.0;
    double llm_time_seconds = 0.0;
    double graph_building_time_seconds = 0.0;

    // Graph statistics
    int final_nodes = 0;
    int final_edges = 0;
    int nodes_before_dedup = 0;
    int nodes_merged = 0;

    /**
     * @brief Print summary to stdout
     */
    void print_summary() const;

    /**
     * @brief Export to JSON
     */
    nlohmann::json to_json() const;
};

// ============================================================================
// Progress Callbacks
// ============================================================================

/**
 * @brief Progress callback function type
 */
using ProgressCallback = std::function<void(
    const std::string& stage,
    int current,
    int total,
    const std::string& message
)>;

// ============================================================================
// Extraction Pipeline
// ============================================================================

/**
 * @brief End-to-end knowledge extraction pipeline
 *
 * This class orchestrates the complete pipeline:
 * PDF → Text Chunks → LLM Extraction → Relations → Hypergraph
 */
class ExtractionPipeline {
public:
    /**
     * @brief Constructor
     */
    explicit ExtractionPipeline(const PipelineConfig& config);

    /**
     * @brief Process a single PDF file
     *
     * @param pdf_path Path to PDF file
     * @return Hypergraph with extracted knowledge
     */
    Hypergraph process_pdf(const std::string& pdf_path);

    /**
     * @brief Process multiple PDF files
     *
     * @param pdf_paths Vector of PDF paths
     * @return Merged hypergraph from all documents
     */
    Hypergraph process_pdfs(const std::vector<std::string>& pdf_paths);

    /**
     * @brief Process all PDFs in a directory
     *
     * @param directory_path Path to directory
     * @param recursive Search recursively
     * @return Merged hypergraph
     */
    Hypergraph process_directory(
        const std::string& directory_path,
        bool recursive = false
    );

    /**
     * @brief Extract from text directly (no PDF)
     *
     * @param text Input text
     * @param document_id Document identifier
     * @return Hypergraph
     */
    Hypergraph extract_from_text(
        const std::string& text,
        const std::string& document_id = "text_input"
    );

    /**
     * @brief Set progress callback
     */
    void set_progress_callback(ProgressCallback callback);

    /**
     * @brief Get pipeline statistics
     */
    PipelineStatistics get_statistics() const { return stats_; }

    /**
     * @brief Reset statistics
     */
    void reset_statistics();

    /**
     * @brief Get current configuration
     */
    PipelineConfig get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const PipelineConfig& config);

private:
    PipelineConfig config_;
    PipelineStatistics stats_;
    ProgressCallback progress_callback_;

    std::unique_ptr<PDFProcessor> pdf_processor_;
    std::unique_ptr<LLMProvider> llm_provider_;
    std::unique_ptr<ChunkingStrategy> chunking_strategy_;

    /**
     * @brief Initialize components based on config
     */
    void initialize_components();

    /**
     * @brief Create chunking strategy from config
     */
    std::unique_ptr<ChunkingStrategy> create_chunking_strategy();

    /**
     * @brief Process a single document
     */
    Hypergraph process_document(const PDFDocument& doc);

    /**
     * @brief Extract relations from chunks
     */
    std::vector<ExtractionResult> extract_from_chunks(
        const std::vector<TextChunk>& chunks,
        const std::string& document_id
    );

    /**
     * @brief Build hypergraph from extraction results
     */
    Hypergraph build_graph_from_results(
        const std::vector<ExtractionResult>& results,
        const std::string& document_id
    );

    /**
     * @brief Merge multiple hypergraphs
     */
    Hypergraph merge_graphs(const std::vector<Hypergraph>& graphs);

    /**
     * @brief Report progress
     */
    void report_progress(
        const std::string& stage,
        int current,
        int total,
        const std::string& message = ""
    );

    /**
     * @brief Save intermediate results
     */
    void save_intermediate_graph(
        const Hypergraph& graph,
        const std::string& document_id
    );

    /**
     * @brief Save extraction results
     */
    void save_extraction_results(
        const std::vector<ExtractionResult>& results,
        const std::string& document_id
    );

    /**
     * @brief Apply rate limiting
     */
    void apply_rate_limit();
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Create default pipeline configuration
 */
PipelineConfig create_default_config();

/**
 * @brief Validate PDF path or directory
 */
bool validate_path(const std::string& path, std::string& error_message);

/**
 * @brief Load configuration from file with fallback to environment
 */
PipelineConfig load_config_with_fallback(const std::string& config_path = "");

} // namespace kg

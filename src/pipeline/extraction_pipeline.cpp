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
#include <regex>

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

/**
 * @brief Classify entity as CLASS or INSTANCE using heuristics
 *
 * This is a fallback for entities that weren't classified by the LLM.
 *
 * @param label The entity label
 * @return "class", "instance", or "" if uncertain
 */
std::string classify_entity_heuristic(const std::string& label) {
    if (label.empty()) return "";

    std::string lower_label = label;
    std::transform(lower_label.begin(), lower_label.end(), lower_label.begin(), ::tolower);

    // Heuristic 1: Contains version number, year, or version indicator → INSTANCE
    // Examples: "ResNet-50", "BERT-base", "GPT-4", "ImageNet 2012"
    if (std::regex_search(label, std::regex(R"(\d{4}|\d+\.\d+|v\d+|-\d+|20\d\d)"))) {
        return "instance";
    }

    // Heuristic 2: Well-known model/algorithm names (proper nouns) → INSTANCE
    // These are specific implementations even without version numbers
    std::vector<std::string> known_instances = {
        "adam", "sgd", "bert", "gpt", "resnet", "alexnet", "vgg", "imagenet",
        "coco", "mnist", "cifar", "yolo", "rcnn", "transformer"
    };
    for (const auto& known : known_instances) {
        if (lower_label.find(known) != std::string::npos) {
            return "instance";
        }
    }

    // Heuristic 3: Contains class indicators → CLASS
    // Examples: "algorithm", "method", "technique", "approach", "model", "network"
    std::vector<std::string> class_indicators = {
        "algorithm", "method", "technique", "approach", "process",
        "system", "framework", "architecture", "type", "category"
    };
    for (const auto& indicator : class_indicators) {
        if (lower_label.find(indicator) != std::string::npos) {
            return "class";
        }
    }

    // Heuristic 4: Abstract concepts → CLASS
    // Examples: "optimization", "learning", "training", "inference"
    std::vector<std::string> abstract_concepts = {
        "optimization", "learning", "training", "inference", "classification",
        "regression", "clustering", "detection", "segmentation", "generation"
    };
    for (const auto& concept : abstract_concepts) {
        if (lower_label == concept || lower_label.find(concept + " ") != std::string::npos) {
            return "class";
        }
    }

    // Heuristic 5: Capitalized single word (proper noun) → Likely INSTANCE
    // Examples: "Adam", "BERT" (but not "Neural" which could be part of "neural network")
    if (label.length() > 2 && std::isupper(label[0]) && label.find(' ') == std::string::npos) {
        // Check if it's all caps (acronym) or mixed case
        bool has_lower = false;
        for (char c : label) {
            if (std::islower(c)) {
                has_lower = true;
                break;
            }
        }
        if (!has_lower || (has_lower && label.length() <= 10)) {
            return "instance";
        }
    }

    // Default: CLASS (more common in research papers for general concepts)
    return "class";
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
    if (j.contains("chunk_overlap_percentage")) config.chunk_overlap_percentage = j["chunk_overlap_percentage"];
    if (j.contains("use_percentage_overlap")) config.use_percentage_overlap = j["use_percentage_overlap"];
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

    // V2: Overlap deduplication config
    if (j.contains("enable_overlap_deduplication")) config.enable_overlap_deduplication = j["enable_overlap_deduplication"];
    if (j.contains("overlap_confidence_boost")) config.overlap_confidence_boost = j["overlap_confidence_boost"];
    if (j.contains("overlap_merge_strategy")) config.overlap_merge_strategy = j["overlap_merge_strategy"];

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
    j["chunk_overlap_percentage"] = chunk_overlap_percentage;
    j["use_percentage_overlap"] = use_percentage_overlap;
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

    // V2: Overlap deduplication config
    j["enable_overlap_deduplication"] = enable_overlap_deduplication;
    j["overlap_confidence_boost"] = overlap_confidence_boost;
    j["overlap_merge_strategy"] = overlap_merge_strategy;

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

    // V2: Overlap deduplication statistics
    j["relations_before_dedup"] = relations_before_dedup;
    j["relations_after_dedup"] = relations_after_dedup;
    j["duplicate_relations_merged"] = duplicate_relations_merged;
    j["avg_confidence_boost"] = avg_confidence_boost;
    j["multi_provenance_relations"] = multi_provenance_relations;

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
        if (config_.use_percentage_overlap) {
            // V2: Percentage-based overlap
            return std::make_unique<FixedSizeChunking>(
                config_.chunk_size,
                config_.chunk_overlap_percentage,
                true  // use_percentage marker
            );
        } else {
            // V1: Character-based overlap (backward compatibility)
            return std::make_unique<FixedSizeChunking>(
                config_.chunk_size,
                config_.chunk_overlap
            );
        }
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

    // V2: Collect all relations into flat vector for deduplication
    std::vector<ExtractedRelation> all_relations;
    for (const auto& result : extraction_results) {
        if (result.success) {
            all_relations.insert(all_relations.end(),
                                result.relations.begin(),
                                result.relations.end());
        }
    }

    // V2: Deduplicate overlapping extractions
    if (config_.use_percentage_overlap && config_.enable_overlap_deduplication) {
        all_relations = deduplicate_relations(all_relations);
    }

    // Build graph
    auto graph_start = std::chrono::high_resolution_clock::now();
    Hypergraph graph = build_graph_from_relations(all_relations, doc.document_id);
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

        // Gemini + causal extraction fallback: if causal extraction fails,
        // retry with standard extraction (more reliable)
        if (!result.success &&
            !config_.custom_system_prompt.empty() &&
            config_.llm_provider == "gemini" &&
            config_.custom_system_prompt.find("causal") != std::string::npos) {

            if (config_.verbose && i == 0) {
                std::cout << "\n  Gemini causal extraction failing, falling back to standard extraction...\n";
            }

            // Retry with standard extraction (empty custom prompt)
            result = llm_provider_->extract_relations(
                chunks[i].text,
                chunks[i].chunk_id,
                ""  // Use default standard extraction prompt
            );
        }

        auto llm_end = std::chrono::high_resolution_clock::now();
        stats_.llm_time_seconds += std::chrono::duration<double>(
            llm_end - llm_start
        ).count();

        // V2: Attach provenance to each relation
        if (result.success) {
            for (auto& rel : result.relations) {
                ExtractionProvenance prov;
                prov.chunk_id = chunks[i].chunk_id;
                prov.start_char = chunks[i].start_position;
                prov.end_char = chunks[i].end_position;
                prov.confidence = rel.confidence;
                prov.source_text = chunks[i].text.substr(0, 200);  // Store snippet
                rel.add_provenance(prov);
            }
        }

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
            std::vector<std::string> normalized_sources;
            for (size_t i = 0; i < rel.sources.size(); ++i) {
                std::string normalized = normalize_entity_label(rel.sources[i]);
                edge.sources.push_back(normalized);
                normalized_sources.push_back(normalized);
            }
            edge.relation = rel.relation;
            edge.targets.reserve(rel.targets.size());
            std::vector<std::string> normalized_targets;
            for (size_t i = 0; i < rel.targets.size(); ++i) {
                std::string normalized = normalize_entity_label(rel.targets[i]);
                edge.targets.push_back(normalized);
                normalized_targets.push_back(normalized);
            }
            edge.confidence = rel.confidence;
            edge.source_document = document_id;
            edge.source_chunk_id = result.chunk_id;

            // Copy properties
            edge.properties = rel.properties;

            // Phase 2: Mark hierarchical relations
            if (edge.relation == "instance_of" || edge.relation == "subclass_of" || edge.relation == "is_a") {
                edge.properties["relation_type"] = "hierarchical";
                edge.properties["hierarchy_type"] = edge.relation;
            }

            graph.add_hyperedge(edge);

            // Ontology: Add node_level classification to nodes
            // Set entity levels from extraction if available
            for (size_t i = 0; i < normalized_sources.size(); ++i) {
                if (i < rel.source_levels.size() && !rel.source_levels[i].empty()) {
                    HyperNode* node = graph.get_node(normalized_sources[i]);
                    if (node && node->properties.find("node_level") == node->properties.end()) {
                        node->properties["node_level"] = rel.source_levels[i];
                    }
                }
            }
            for (size_t i = 0; i < normalized_targets.size(); ++i) {
                if (i < rel.target_levels.size() && !rel.target_levels[i].empty()) {
                    HyperNode* node = graph.get_node(normalized_targets[i]);
                    if (node && node->properties.find("node_level") == node->properties.end()) {
                        node->properties["node_level"] = rel.target_levels[i];
                    }
                }
            }
        }
    }

    return graph;
}

// V2: Build graph from flat list of relations (post-deduplication)
Hypergraph ExtractionPipeline::build_graph_from_relations(
    const std::vector<ExtractedRelation>& relations,
    const std::string& document_id
) {
    Hypergraph graph;

    for (const auto& rel : relations) {
        if (rel.sources.empty() || rel.targets.empty()) continue;

        HyperEdge edge;
        edge.sources.reserve(rel.sources.size());
        std::vector<std::string> normalized_sources;
        for (size_t i = 0; i < rel.sources.size(); ++i) {
            std::string normalized = normalize_entity_label(rel.sources[i]);
            edge.sources.push_back(normalized);
            normalized_sources.push_back(normalized);
        }
        edge.relation = rel.relation;
        edge.targets.reserve(rel.targets.size());
        std::vector<std::string> normalized_targets;
        for (size_t i = 0; i < rel.targets.size(); ++i) {
            std::string normalized = normalize_entity_label(rel.targets[i]);
            edge.targets.push_back(normalized);
            normalized_targets.push_back(normalized);
        }
        edge.confidence = rel.confidence;
        edge.source_document = document_id;

        // Use first provenance for chunk ID (relations may have multiple)
        if (!rel.provenances.empty()) {
            edge.source_chunk_id = rel.provenances[0].chunk_id;
        }

        // Copy properties
        edge.properties = rel.properties;

        // Phase 2: Mark hierarchical relations
        if (edge.relation == "instance_of" || edge.relation == "subclass_of" || edge.relation == "is_a") {
            edge.properties["relation_type"] = "hierarchical";
            edge.properties["hierarchy_type"] = edge.relation;
        }

        graph.add_hyperedge(edge);

        // Ontology: Add node_level classification to nodes
        // Set entity levels from extraction if available
        for (size_t i = 0; i < normalized_sources.size(); ++i) {
            if (i < rel.source_levels.size() && !rel.source_levels[i].empty()) {
                HyperNode* node = graph.get_node(normalized_sources[i]);
                if (node && node->properties.find("node_level") == node->properties.end()) {
                    node->properties["node_level"] = rel.source_levels[i];
                }
            }
        }
        for (size_t i = 0; i < normalized_targets.size(); ++i) {
            if (i < rel.target_levels.size() && !rel.target_levels[i].empty()) {
                HyperNode* node = graph.get_node(normalized_targets[i]);
                if (node && node->properties.find("node_level") == node->properties.end()) {
                    node->properties["node_level"] = rel.target_levels[i];
                }
            }
        }
    }

    return graph;
}

int ExtractionPipeline::apply_heuristic_classification(Hypergraph& graph) {
    int classified_count = 0;

    // Get all node IDs, then get mutable access to each
    std::vector<HyperNode> nodes_copy = graph.get_all_nodes();

    for (const auto& node_copy : nodes_copy) {
        // Get mutable access to the actual node in the graph
        HyperNode* node = graph.get_node(node_copy.id);
        if (!node) continue;

        // Skip if already classified
        if (node->properties.find("node_level") != node->properties.end() &&
            !node->properties["node_level"].empty()) {
            continue;
        }

        // Apply heuristic classification
        std::string level = classify_entity_heuristic(node->label);
        if (!level.empty()) {
            node->properties["node_level"] = level;
            classified_count++;
        }
    }

    return classified_count;
}

int ExtractionPipeline::augment_instance_base_classes(Hypergraph& graph) {
    int augmented_count = 0;

    // Set of hierarchical relation types
    std::set<std::string> hierarchical_relations = {"instance_of", "is_a", "subclass_of"};

    // Get all node IDs, then get mutable access to each
    std::vector<HyperNode> nodes_copy = graph.get_all_nodes();

    for (const auto& node_copy : nodes_copy) {
        // Get mutable access to the actual node in the graph
        HyperNode* node = graph.get_node(node_copy.id);
        if (!node) continue;

        // Only process instance nodes
        if (node->properties.find("node_level") == node->properties.end() ||
            node->properties["node_level"] != "instance") {
            continue;
        }

        // Find all edges where this node is a source (pointing to parent classes)
        std::vector<std::string> parent_classes;
        std::vector<HyperEdge> all_edges = graph.get_all_edges();

        for (const auto& edge : all_edges) {
            // Check if this is a hierarchical relation
            if (hierarchical_relations.find(edge.relation) == hierarchical_relations.end()) {
                continue;
            }

            // Check if our node is in the sources
            bool node_is_source = false;
            for (const auto& src : edge.sources) {
                if (src == node->id) {
                    node_is_source = true;
                    break;
                }
            }

            if (node_is_source) {
                // Add all targets as parent classes
                for (const auto& target_id : edge.targets) {
                    HyperNode* target_node = graph.get_node(target_id);
                    if (target_node) {
                        // Use label for readability, fall back to ID if no label
                        std::string class_name = target_node->label.empty() ?
                                                 target_id : target_node->label;

                        // Avoid duplicates
                        if (std::find(parent_classes.begin(), parent_classes.end(), class_name)
                            == parent_classes.end()) {
                            parent_classes.push_back(class_name);
                        }
                    }
                }
            }
        }

        // Store parent classes in node properties if any found
        if (!parent_classes.empty()) {
            // Join with comma
            std::string base_classes_str;
            for (size_t i = 0; i < parent_classes.size(); ++i) {
                if (i > 0) base_classes_str += ", ";
                base_classes_str += parent_classes[i];
            }
            node->properties["base_classes"] = base_classes_str;
            augmented_count++;
        }
    }

    return augmented_count;
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

// ============================================================================
// V2: Overlap Deduplication
// ============================================================================

std::vector<ExtractedRelation> ExtractionPipeline::deduplicate_relations(
    const std::vector<ExtractedRelation>& all_relations
) {
    if (!config_.enable_overlap_deduplication) {
        return all_relations;  // Return as-is if deduplication disabled
    }

    // V2: Track statistics
    stats_.relations_before_dedup = all_relations.size();

    // Map: normalized_key -> merged_relation
    std::map<std::string, ExtractedRelation> relation_map;
    double total_confidence_boost = 0.0;
    int num_merges = 0;

    for (const auto& rel : all_relations) {
        std::string key = rel.get_normalized_key();

        if (relation_map.count(key)) {
            // Duplicate found -> merge
            double old_confidence = relation_map[key].confidence;
            relation_map[key].merge_with(rel, config_.overlap_confidence_boost);
            double new_confidence = relation_map[key].confidence;

            // Track confidence boost
            total_confidence_boost += (new_confidence - old_confidence);
            num_merges++;

            if (config_.verbose) {
                std::cout << "  [Dedup] Merged duplicate: " << key << std::endl;
            }
        } else {
            // New relation
            relation_map[key] = rel;
        }
    }

    // Convert map back to vector
    std::vector<ExtractedRelation> deduplicated;
    deduplicated.reserve(relation_map.size());

    int multi_prov_count = 0;
    for (const auto& [key, rel] : relation_map) {
        if (rel.provenances.size() > 1) {
            multi_prov_count++;
        }
        deduplicated.push_back(rel);
    }

    // V2: Update statistics
    stats_.relations_after_dedup = deduplicated.size();
    stats_.duplicate_relations_merged = all_relations.size() - deduplicated.size();
    stats_.avg_confidence_boost = num_merges > 0 ? total_confidence_boost / num_merges : 0.0;
    stats_.multi_provenance_relations = multi_prov_count;

    // Log statistics
    if (config_.verbose && deduplicated.size() < all_relations.size()) {
        std::cout << "  [Dedup] Merged " << stats_.duplicate_relations_merged << " duplicate relations ("
                  << all_relations.size() << " -> " << deduplicated.size() << ")" << std::endl;
        std::cout << "  [Dedup] Avg confidence boost: " << stats_.avg_confidence_boost << std::endl;
        std::cout << "  [Dedup] Multi-provenance relations: " << multi_prov_count << std::endl;
    }

    return deduplicated;
}

} // namespace kg

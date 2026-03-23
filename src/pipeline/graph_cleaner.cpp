#include "pipeline/graph_cleaner.hpp"
#include "llm/llm_provider.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <regex>
#include <chrono>
#include <iostream>
#include <sstream>

namespace kg {

// ============================================================================
// CleaningReport Implementation
// ============================================================================

void CleaningReport::print_summary() const {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  Graph Quality Control Report\n";
    std::cout << "======================================================================\n";
    std::cout << "\n";
    std::cout << "Initial:   " << initial_nodes << " nodes, " << initial_edges << " edges\n";
    std::cout << "\n";

    if (level1_removed > 0) {
        std::cout << "Level 1: Rule-Based Filtering\n";
        std::cout << "  Removed by length:     " << removed_by_length << "\n";
        std::cout << "  Removed by stopwords:  " << removed_by_stopwords << "\n";
        std::cout << "  Removed by numbers:    " << removed_by_numbers << "\n";
        std::cout << "  Removed by artifacts:  " << removed_by_artifacts << "\n";
        std::cout << "  Level 1 total:         " << level1_removed << " ("
                  << (100.0 * level1_removed / initial_nodes) << "%)\n";
        std::cout << "\n";
    }

    if (level2_removed > 0) {
        std::cout << "Level 2: Statistical Filtering\n";
        std::cout << "  Removed by degree:     " << removed_by_degree << "\n";
        std::cout << "  Removed by importance: " << removed_by_importance << "\n";
        std::cout << "  Removed by outliers:   " << removed_by_outliers << "\n";
        std::cout << "  Level 2 total:         " << level2_removed << " ("
                  << (100.0 * level2_removed / initial_nodes) << "%)\n";
        std::cout << "\n";
    }

    if (validated_by_llm > 0) {
        std::cout << "Level 3: LLM Validation\n";
        std::cout << "  Validated by LLM:      " << validated_by_llm << "\n";
        std::cout << "  Removed by LLM:        " << removed_by_llm << " ("
                  << (100.0 * removed_by_llm / validated_by_llm) << "%)\n";
        std::cout << "  Level 3 total:         " << level3_removed << "\n";
        std::cout << "\n";
    }

    int total_removed = level1_removed + level2_removed + level3_removed;
    std::cout << "Final:     " << final_nodes << " nodes, " << final_edges << " edges\n";
    std::cout << "Removed:   " << total_removed << " nodes ("
              << (100.0 * total_removed / initial_nodes) << "%)\n";
    std::cout << "Time:      " << cleaning_time_ms << " ms\n";
    std::cout << "\n";
    std::cout << "======================================================================\n";
}

nlohmann::json CleaningReport::to_json() const {
    nlohmann::json j;
    j["initial_nodes"] = initial_nodes;
    j["initial_edges"] = initial_edges;
    j["final_nodes"] = final_nodes;
    j["final_edges"] = final_edges;

    j["level1"] = {
        {"removed_by_length", removed_by_length},
        {"removed_by_stopwords", removed_by_stopwords},
        {"removed_by_numbers", removed_by_numbers},
        {"removed_by_artifacts", removed_by_artifacts},
        {"total_removed", level1_removed}
    };

    j["level2"] = {
        {"removed_by_degree", removed_by_degree},
        {"removed_by_importance", removed_by_importance},
        {"removed_by_outliers", removed_by_outliers},
        {"total_removed", level2_removed}
    };

    j["level3"] = {
        {"validated_by_llm", validated_by_llm},
        {"removed_by_llm", removed_by_llm},
        {"total_removed", level3_removed}
    };

    j["total_removed"] = level1_removed + level2_removed + level3_removed;
    j["cleaning_time_ms"] = cleaning_time_ms;

    return j;
}

// ============================================================================
// GraphCleaner Implementation
// ============================================================================

GraphCleaner::GraphCleaner() {
    // Initialize stopwords
    stopwords_ = {
        "a", "an", "the", "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "do", "does", "did", "will", "would", "should",
        "could", "may", "might", "must", "can", "of", "in", "to", "for", "with",
        "on", "at", "from", "by", "about", "as", "into", "through", "during",
        "before", "after", "above", "below", "between", "under", "again",
        "further", "then", "once", "here", "there", "when", "where", "why",
        "how", "all", "each", "every", "both", "few", "more", "most", "other",
        "some", "such", "no", "nor", "not", "only", "own", "same", "so", "than",
        "too", "very", "s", "t", "just", "now"
    };

    // Initialize extraction artifacts
    artifacts_ = {
        "fig", "figure", "table", "page", "section", "chapter", "appendix",
        "ref", "reference", "see", "also", "cf", "et", "al", "i.e", "e.g",
        "etc", "vs", "v", "via", "ibid", "op", "cit", "loc"
    };
}

GraphCleaner::~GraphCleaner() = default;

CleaningReport GraphCleaner::clean(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    std::shared_ptr<LLMProvider> llm
) {
    auto start_time = std::chrono::steady_clock::now();

    CleaningReport report;
    report.initial_nodes = entities.size();
    report.initial_edges = relations.size();

    // Level 1: Rule-based filtering (fast, removes obvious noise)
    if (config.enable_rule_based) {
        level1_rule_based_filtering(entities, relations, config, report);
    }

    // Level 2: Statistical filtering (medium speed, removes outliers)
    if (config.enable_statistical) {
        level2_statistical_filtering(entities, relations, config, report);
    }

    // Level 3: LLM validation (slow but accurate)
    if (config.enable_llm_validation && llm) {
        level3_llm_validation(entities, relations, config, llm, report);
    }

    report.final_nodes = std::count_if(entities.begin(), entities.end(),
                                       [](const auto& e) { return e.is_valid; });
    report.final_edges = std::count_if(relations.begin(), relations.end(),
                                       [](const auto& r) { return r.is_valid; });

    auto end_time = std::chrono::steady_clock::now();
    report.cleaning_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return report;
}

// ============================================================================
// Level 1: Rule-Based Filtering
// ============================================================================

void GraphCleaner::level1_rule_based_filtering(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    CleaningReport& report
) {
    if (config.progress_callback) {
        config.progress_callback("Level 1: Rule-based", 0, entities.size());
    }

    int processed = 0;
    for (auto& entity : entities) {
        if (!entity.is_valid) continue;

        std::string label = entity.label;
        std::string label_lower = to_lowercase(label);

        // Check length
        if (!is_valid_length(label, config.min_node_length, config.max_node_length)) {
            entity.is_valid = false;
            entity.removal_reason = "invalid_length";
            report.removed_by_length++;
            report.level1_removed++;
            continue;
        }

        // Check single character
        if (config.remove_single_chars && is_single_char(label)) {
            entity.is_valid = false;
            entity.removal_reason = "single_char";
            report.removed_by_length++;
            report.level1_removed++;
            continue;
        }

        // Check stopwords
        if (config.remove_stopwords && is_stopword(label_lower)) {
            entity.is_valid = false;
            entity.removal_reason = "stopword";
            report.removed_by_stopwords++;
            report.level1_removed++;
            continue;
        }

        // Check pure numbers
        if (config.remove_numbers && is_pure_number(label)) {
            entity.is_valid = false;
            entity.removal_reason = "pure_number";
            report.removed_by_numbers++;
            report.level1_removed++;
            continue;
        }

        // Check extraction artifacts
        if (config.remove_artifacts && is_extraction_artifact(label_lower)) {
            entity.is_valid = false;
            entity.removal_reason = "artifact";
            report.removed_by_artifacts++;
            report.level1_removed++;
            continue;
        }

        // Check pure punctuation
        if (is_pure_punctuation(label)) {
            entity.is_valid = false;
            entity.removal_reason = "punctuation";
            report.removed_by_artifacts++;
            report.level1_removed++;
            continue;
        }

        processed++;
        if (config.progress_callback && processed % 100 == 0) {
            config.progress_callback("Level 1: Rule-based", processed, entities.size());
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        valid_ids.insert(e.id);
    }
    remove_invalid_relations(relations, valid_ids);
}

// ============================================================================
// Level 2: Statistical Filtering
// ============================================================================

void GraphCleaner::level2_statistical_filtering(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    CleaningReport& report
) {
    if (config.progress_callback) {
        config.progress_callback("Level 2: Statistical", 0, entities.size());
    }

    // Compute node degrees
    std::map<std::string, int> degree_map;
    compute_node_degrees(entities, relations, degree_map);

    // Update entity degrees
    for (auto& entity : entities) {
        entity.degree = degree_map[entity.id];
    }

    // Compute importance scores (simple version: normalized degree)
    compute_importance_scores(entities, relations);

    // Filter by degree
    int processed = 0;
    for (auto& entity : entities) {
        if (!entity.is_valid) continue;

        // Check minimum degree
        if (entity.degree < config.min_degree) {
            entity.is_valid = false;
            entity.removal_reason = "low_degree";
            report.removed_by_degree++;
            report.level2_removed++;
            continue;
        }

        // Check importance
        if (entity.importance < config.min_importance) {
            entity.is_valid = false;
            entity.removal_reason = "low_importance";
            report.removed_by_importance++;
            report.level2_removed++;
            continue;
        }

        // Check for statistical outliers
        if (config.remove_outliers && is_statistical_outlier(entity, 3.0)) {
            entity.is_suspicious = true;  // Don't remove yet, flag for LLM validation
        }

        processed++;
        if (config.progress_callback && processed % 100 == 0) {
            config.progress_callback("Level 2: Statistical", processed, entities.size());
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        valid_ids.insert(e.id);
    }
    remove_invalid_relations(relations, valid_ids);
}

// ============================================================================
// Level 3: LLM Validation
// ============================================================================

void GraphCleaner::level3_llm_validation(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    std::shared_ptr<LLMProvider> llm,
    CleaningReport& report
) {
    if (!llm) return;

    // Collect entities to validate
    std::vector<CleanableEntity*> to_validate;
    for (auto& entity : entities) {
        if (!entity.is_valid) continue;

        bool should_validate = false;
        if (config.validation_mode == "all") {
            should_validate = true;
        } else if (config.validation_mode == "suspicious") {
            should_validate = entity.is_suspicious;
        }

        if (should_validate) {
            to_validate.push_back(&entity);
        }
    }

    if (to_validate.empty()) return;

    if (config.progress_callback) {
        config.progress_callback("Level 3: LLM Validation", 0, to_validate.size());
    }

    // Batch validate
    for (size_t i = 0; i < to_validate.size(); i += config.llm_batch_size) {
        size_t batch_end = std::min(i + config.llm_batch_size, to_validate.size());
        std::vector<std::string> batch_labels;

        for (size_t j = i; j < batch_end; j++) {
            batch_labels.push_back(to_validate[j]->label);
        }

        // Validate batch
        auto results = batch_validate_entities(batch_labels, llm);

        // Apply results
        for (size_t j = 0; j < results.size() && (i + j) < to_validate.size(); j++) {
            report.validated_by_llm++;
            if (!results[j]) {
                to_validate[i + j]->is_valid = false;
                to_validate[i + j]->removal_reason = "llm_validation_failed";
                report.removed_by_llm++;
                report.level3_removed++;
            }
        }

        if (config.progress_callback) {
            config.progress_callback("Level 3: LLM Validation", batch_end, to_validate.size());
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        valid_ids.insert(e.id);
    }
    remove_invalid_relations(relations, valid_ids);
}

// ============================================================================
// Helper Methods: Level 1
// ============================================================================

bool GraphCleaner::is_valid_length(const std::string& s, int min_len, int max_len) const {
    int len = s.length();
    return len >= min_len && len <= max_len;
}

bool GraphCleaner::is_stopword(const std::string& s) const {
    return stopwords_.count(s) > 0;
}

bool GraphCleaner::is_pure_number(const std::string& s) const {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isdigit(c) || c == '.' || c == ',' || c == '-';
    });
}

bool GraphCleaner::is_pure_punctuation(const std::string& s) const {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::ispunct(c) || std::isspace(c);
    });
}

bool GraphCleaner::is_extraction_artifact(const std::string& s) const {
    return artifacts_.count(s) > 0;
}

bool GraphCleaner::is_single_char(const std::string& s) const {
    // Check if string has only one alphanumeric character (ignoring whitespace/punctuation)
    int alpha_count = 0;
    for (char c : s) {
        if (std::isalnum(c)) {
            alpha_count++;
            if (alpha_count > 1) return false;
        }
    }
    return alpha_count == 1;
}

// ============================================================================
// Helper Methods: Level 2
// ============================================================================

void GraphCleaner::compute_node_degrees(
    const std::vector<CleanableEntity>& entities,
    const std::vector<CleanableRelation>& relations,
    std::map<std::string, int>& degree_map
) const {
    // Initialize
    for (const auto& e : entities) {
        degree_map[e.id] = 0;
    }

    // Count degrees
    for (const auto& r : relations) {
        if (r.is_valid) {
            degree_map[r.source]++;
            degree_map[r.target]++;
        }
    }
}

void GraphCleaner::compute_importance_scores(
    std::vector<CleanableEntity>& entities,
    const std::vector<CleanableRelation>& relations
) const {
    // Simple importance: normalized degree * confidence
    int max_degree = 1;
    for (const auto& e : entities) {
        if (e.degree > max_degree) {
            max_degree = e.degree;
        }
    }

    for (auto& e : entities) {
        e.importance = (e.degree / static_cast<double>(max_degree)) * e.confidence;
    }
}

bool GraphCleaner::is_statistical_outlier(const CleanableEntity& entity, double threshold) const {
    // Simple outlier detection: flag very low importance
    return entity.importance < 0.1 && entity.degree < 2;
}

// ============================================================================
// Helper Methods: Level 3
// ============================================================================

std::vector<bool> GraphCleaner::batch_validate_entities(
    const std::vector<std::string>& labels,
    std::shared_ptr<LLMProvider> llm
) const {
    std::string prompt = build_entity_validation_prompt(labels);

    try {
        LLMResponse llm_response = llm->complete(prompt);
        if (!llm_response.success) {
            // If LLM call failed, assume all valid
            return std::vector<bool>(labels.size(), true);
        }
        std::string response = llm_response.content;

        // Parse JSON response
        auto json = nlohmann::json::parse(response);
        std::vector<bool> results;

        if (json.contains("results") && json["results"].is_array()) {
            for (const auto& result : json["results"]) {
                if (result.is_string()) {
                    std::string status = result.get<std::string>();
                    results.push_back(status == "VALID" || status == "valid");
                } else if (result.is_boolean()) {
                    results.push_back(result.get<bool>());
                }
            }
        }

        // Fallback: if parsing failed, assume all valid
        while (results.size() < labels.size()) {
            results.push_back(true);
        }

        return results;

    } catch (const std::exception& e) {
        std::cerr << "LLM validation error: " << e.what() << "\n";
        // Fallback: assume all valid
        return std::vector<bool>(labels.size(), true);
    }
}

std::vector<bool> GraphCleaner::batch_validate_relations(
    const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
    std::shared_ptr<LLMProvider> llm
) const {
    // Similar to batch_validate_entities but for relations
    // TODO: Implement if needed
    return std::vector<bool>(triples.size(), true);
}

std::string GraphCleaner::build_entity_validation_prompt(
    const std::vector<std::string>& labels
) const {
    std::stringstream ss;
    ss << "You are validating entities extracted from scientific documents.\n";
    ss << "Classify each entity as VALID or INVALID.\n\n";
    ss << "VALID: Meaningful concepts, entities, technical terms, or domain-specific terminology\n";
    ss << "INVALID: Single letters, pure numbers, extraction artifacts, nonsense, generic stopwords\n\n";
    ss << "Entities to validate:\n";

    for (size_t i = 0; i < labels.size(); i++) {
        ss << (i + 1) << ". \"" << labels[i] << "\"\n";
    }

    ss << "\nRespond in JSON format:\n";
    ss << "{\"results\": [\"VALID\", \"INVALID\", ...]}\n\n";
    ss << "Response:";

    return ss.str();
}

std::string GraphCleaner::build_relation_validation_prompt(
    const std::vector<std::tuple<std::string, std::string, std::string>>& triples
) const {
    std::stringstream ss;
    ss << "Validate if these relationships make semantic sense:\n\n";

    for (size_t i = 0; i < triples.size(); i++) {
        const auto& [source, relation, target] = triples[i];
        ss << (i + 1) << ". \"" << source << "\" --[" << relation << "]--> \"" << target << "\"\n";
    }

    ss << "\nRespond in JSON format:\n";
    ss << "{\"results\": [true, false, ...]}\n\n";
    ss << "Response:";

    return ss.str();
}

// ============================================================================
// Utility Methods
// ============================================================================

std::string GraphCleaner::to_lowercase(const std::string& s) const {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

void GraphCleaner::remove_invalid_entities(std::vector<CleanableEntity>& entities) {
    entities.erase(
        std::remove_if(entities.begin(), entities.end(),
                      [](const CleanableEntity& e) { return !e.is_valid; }),
        entities.end()
    );
}

void GraphCleaner::remove_invalid_relations(
    std::vector<CleanableRelation>& relations,
    const std::set<std::string>& valid_entity_ids
) {
    for (auto& r : relations) {
        if (valid_entity_ids.count(r.source) == 0 || valid_entity_ids.count(r.target) == 0) {
            r.is_valid = false;
        }
    }

    relations.erase(
        std::remove_if(relations.begin(), relations.end(),
                      [](const CleanableRelation& r) { return !r.is_valid; }),
        relations.end()
    );
}

} // namespace kg

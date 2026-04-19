#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace kg {

// Forward declarations
class LLMProvider;

/**
 * @brief Configuration for graph cleaning operations
 */
struct CleaningConfig {
    // Level 1: Rule-based filtering
    bool enable_rule_based = true;
    int min_node_length = 2;
    int max_node_length = 100;
    bool remove_stopwords = true;
    bool remove_numbers = true;
    bool remove_single_chars = true;
    bool remove_artifacts = true;

    // Level 1.5: Semantic deduplication
    bool enable_semantic_dedup = false;
    int semantic_batch_size = 20;       // Batch size for semantic comparison
    double semantic_threshold = 0.85;    // Similarity threshold for merging

    // Level 2: Statistical filtering
    bool enable_statistical = true;
    int min_degree = 1;           // Remove nodes with degree < this (after graph built)
    double min_importance = 0.0;  // Remove nodes with importance < this
    bool remove_outliers = true;

    // Level 3: LLM validation
    bool enable_llm_validation = false;
    int llm_batch_size = 50;
    double llm_confidence_threshold = 0.7;
    std::string validation_mode = "suspicious"; // "all", "suspicious", "none"

    // Progress reporting
    std::function<void(const std::string&, int, int)> progress_callback;
};

/**
 * @brief Results of a cleaning operation
 */
struct CleaningReport {
    int initial_nodes = 0;
    int initial_edges = 0;

    // Level 1 results
    int removed_by_length = 0;
    int removed_by_stopwords = 0;
    int removed_by_numbers = 0;
    int removed_by_artifacts = 0;
    int level1_removed = 0;

    // Level 1.5 results (semantic dedup)
    int semantic_duplicates_found = 0;
    int semantic_duplicates_merged = 0;

    // Level 1.2: Label simplification
    int labels_simplified = 0;

    // Level 2 results
    int removed_by_degree = 0;
    int removed_by_importance = 0;
    int removed_by_outliers = 0;
    int level2_removed = 0;

    // Level 3 results
    int validated_by_llm = 0;
    int removed_by_llm = 0;
    int level3_removed = 0;

    int final_nodes = 0;
    int final_edges = 0;

    double cleaning_time_ms = 0.0;

    // Graph connectivity analysis
    int num_connected_components = 0;
    int largest_component_size = 0;
    int num_isolated_nodes = 0;
    double graph_density = 0.0;
    double average_degree = 0.0;
    double clustering_coefficient = 0.0;
    std::string hub_node_label = "";  // Highest degree node in largest component
    int hub_node_degree = 0;

    void print_summary() const;
    nlohmann::json to_json() const;
    std::string generate_html_report() const;
};

/**
 * @brief Entity with metadata for cleaning
 */
struct CleanableEntity {
    std::string id;
    std::string label;
    std::string type;
    double confidence = 1.0;
    int degree = 0;
    double importance = 0.0;
    bool is_suspicious = false;
    bool is_valid = true;
    std::string removal_reason;
};

/**
 * @brief Relation with metadata for cleaning
 */
struct CleanableRelation {
    std::string id;
    std::string source;
    std::string relation;
    std::string target;
    double confidence = 1.0;
    bool is_valid = true;
    std::string removal_reason;
};

/**
 * @brief Hyperedge with metadata for cleaning
 */
struct CleanableHyperedge {
    std::string id;
    std::vector<std::string> sources;
    std::string relation;
    std::vector<std::string> targets;
    double confidence = 1.0;
    bool is_valid = true;
    std::string removal_reason;

    std::set<std::string> get_all_nodes() const {
        std::set<std::string> nodes;
        nodes.insert(sources.begin(), sources.end());
        nodes.insert(targets.begin(), targets.end());
        return nodes;
    }
};

/**
 * @brief Main graph cleaning engine with 3-level validation
 */
class GraphCleaner {
public:
    GraphCleaner();
    ~GraphCleaner();

    /**
     * @brief Clean entities and relations using configured levels
     * @param entities Entities to clean (modified in place)
     * @param relations Relations to clean (modified in place)
     * @param config Cleaning configuration
     * @param llm Optional LLM provider for Level 3 validation
     * @return Cleaning report with statistics
     */
    CleaningReport clean(
        std::vector<CleanableEntity>& entities,
        std::vector<CleanableRelation>& relations,
        const CleaningConfig& config,
        std::shared_ptr<LLMProvider> llm = nullptr
    );

    /**
     * @brief Clean entities and hyperedges while preserving higher-order structure
     */
    CleaningReport clean(
        std::vector<CleanableEntity>& entities,
        std::vector<CleanableHyperedge>& hyperedges,
        const CleaningConfig& config,
        std::shared_ptr<LLMProvider> llm = nullptr
    );

    // Individual level operations
    void level1_rule_based_filtering(
        std::vector<CleanableEntity>& entities,
        std::vector<CleanableHyperedge>& hyperedges,
        const CleaningConfig& config,
        CleaningReport& report
    );

    void level12_simplify_labels(
        std::vector<CleanableEntity>& entities,
        const CleaningConfig& config,
        CleaningReport& report
    );

    void level15_semantic_deduplication(
        std::vector<CleanableEntity>& entities,
        std::vector<CleanableHyperedge>& hyperedges,
        const CleaningConfig& config,
        std::shared_ptr<LLMProvider> llm,
        CleaningReport& report
    );

    void level2_statistical_filtering(
        std::vector<CleanableEntity>& entities,
        std::vector<CleanableHyperedge>& hyperedges,
        const CleaningConfig& config,
        CleaningReport& report
    );

    void level3_llm_validation(
        std::vector<CleanableEntity>& entities,
        std::vector<CleanableHyperedge>& hyperedges,
        const CleaningConfig& config,
        std::shared_ptr<LLMProvider> llm,
        CleaningReport& report
    );

private:
    // Level 1: Rule-based helpers
    bool is_valid_length(const std::string& s, int min_len, int max_len) const;
    bool is_stopword(const std::string& s) const;
    bool is_pure_number(const std::string& s) const;
    bool is_pure_punctuation(const std::string& s) const;
    bool is_extraction_artifact(const std::string& s) const;
    bool is_single_char(const std::string& s) const;

    // Level 2: Statistical helpers
    void compute_node_degrees(
        const std::vector<CleanableEntity>& entities,
        const std::vector<CleanableHyperedge>& hyperedges,
        std::map<std::string, int>& degree_map
    ) const;

    void compute_importance_scores(
        std::vector<CleanableEntity>& entities,
        const std::vector<CleanableHyperedge>& hyperedges
    ) const;

    bool is_statistical_outlier(const CleanableEntity& entity, double threshold) const;

    // Level 1.5: Semantic deduplication helpers
    struct SemanticDuplicateGroup {
        std::string canonical;
        std::vector<std::string> variants;
    };

    std::vector<SemanticDuplicateGroup> identify_semantic_duplicates(
        const std::vector<std::string>& labels,
        std::shared_ptr<LLMProvider> llm
    ) const;

    // Normalize singular/plural variations (kg -> kg, kgs -> kg, knowledge graph -> knowledge graph, knowledge graphs -> knowledge graph)
    static std::string normalize_plural(const std::string& label);

    // Level 3: LLM validation helpers
    std::vector<bool> batch_validate_entities(
        const std::vector<std::string>& labels,
        std::shared_ptr<LLMProvider> llm
    ) const;

    std::vector<bool> batch_validate_relations(
        const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
        std::shared_ptr<LLMProvider> llm
    ) const;

    std::string build_entity_validation_prompt(
        const std::vector<std::string>& labels
    ) const;

    std::string build_relation_validation_prompt(
        const std::vector<std::tuple<std::string, std::string, std::string>>& triples
    ) const;

    // Graph connectivity analysis
    void analyze_connectivity(
        const std::vector<CleanableEntity>& entities,
        const std::vector<CleanableHyperedge>& hyperedges,
        CleaningReport& report
    ) const;

    // Utility
    std::string to_lowercase(const std::string& s) const;
    std::string trim(const std::string& s) const;
    void remove_invalid_entities(std::vector<CleanableEntity>& entities);
    void remove_invalid_hyperedges(
        std::vector<CleanableHyperedge>& hyperedges,
        const std::set<std::string>& valid_entity_ids
    );

    // Stopwords and artifacts
    std::set<std::string> stopwords_;
    std::set<std::string> artifacts_;
};

} // namespace kg

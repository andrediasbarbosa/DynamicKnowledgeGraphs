#pragma once

#include "discovery/insight.hpp"
#include "graph/hypergraph.hpp"
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <memory>

namespace kg {
class LLMProvider;
}

namespace kg {

// Report format
enum class ReportFormat {
    MARKDOWN,
    HTML,
    PLAIN_TEXT
};

// Report configuration
struct ReportConfig {
    std::string title = "Knowledge Discovery Report";
    std::string source_document;
    int max_examples_per_type = 5;
    bool include_evidence = true;
    bool include_statistics = true;
    bool include_llm_examples = false;
    int llm_examples_per_type = 1;
    int llm_max_witness_nodes = 6;
    bool markdown_format = true;  // deprecated, use format instead
    ReportFormat format = ReportFormat::MARKDOWN;

    // LLM narrative generation for all findings
    bool use_llm_narratives = true;  ///< Use LLM to generate contextual descriptions for all findings

    // HTML coalescing options
    bool coalesce_similar_findings = true;  ///< Group similar findings together in HTML to reduce repetition
    double similarity_threshold = 0.7;       ///< Threshold for considering findings similar (0-1)
    int max_items_per_cluster = 10;          ///< Maximum items to show per cluster before summarizing
};

/**
 * @brief A cluster of similar insights grouped together
 */
struct InsightCluster {
    std::vector<Insight> insights;           ///< Insights in this cluster
    std::string cluster_summary;              ///< LLM-generated summary for the cluster
    std::string representative_description;  ///< Description based on representative insight
    double avg_score = 0.0;                   ///< Average score of insights in cluster
    std::string common_theme;                 ///< Identified common theme/pattern
};

// Report generator - creates natural language reports from insights
class ReportGenerator {
public:
    ReportGenerator(const Hypergraph& graph);

    // Generate a full report from insights
    std::string generate(const InsightCollection& insights, const ReportConfig& config = {});

    // Generate HTML report with styling
    std::string generate_html(const InsightCollection& insights, const ReportConfig& config = {});

    // Save report to file
    void save_to_file(const std::string& path, const std::string& content);
    void set_llm_provider(const std::shared_ptr<LLMProvider>& provider);

private:
    const Hypergraph& graph_;
    std::shared_ptr<LLMProvider> llm_provider_;
    std::map<std::string, std::string> llm_example_cache_;

    // Section generators
    std::string generate_header(const InsightCollection& insights, const ReportConfig& config);
    std::string generate_executive_summary(const InsightCollection& insights, const ReportConfig& config);
    std::string generate_statistics_section(const InsightCollection& insights, const ReportConfig& config);
    std::string generate_augmentation_overview(const ReportConfig& config);
    std::string generate_llm_examples_section(
        const std::map<InsightType, std::vector<Insight>>& by_type,
        const ReportConfig& config
    );
    std::string generate_llm_examples_section_html(
        const std::map<InsightType, std::vector<Insight>>& by_type,
        const ReportConfig& config
    );
    std::string generate_bridges_section(const std::vector<Insight>& bridges, const ReportConfig& config);
    std::string generate_completions_section(const std::vector<Insight>& completions, const ReportConfig& config);
    std::string generate_motifs_section(const std::vector<Insight>& motifs, const ReportConfig& config);
    std::string generate_substitutions_section(const std::vector<Insight>& substitutions, const ReportConfig& config);
    std::string generate_surprise_section(const std::vector<Insight>& surprises, const ReportConfig& config);
    std::string generate_diffusion_section(const std::vector<Insight>& diffusions, const ReportConfig& config);
    std::string generate_path_rank_section(const std::vector<Insight>& paths, const ReportConfig& config);
    std::string generate_community_links_section(const std::vector<Insight>& links, const ReportConfig& config);
    std::string generate_hypotheses_section(const std::vector<Insight>& hypotheses, const ReportConfig& config);
    std::string generate_rules_section(const std::vector<Insight>& rules, const ReportConfig& config);
    std::string generate_embedding_links_section(const std::vector<Insight>& links, const ReportConfig& config);
    std::string generate_author_chains_section(const std::vector<Insight>& chains, const ReportConfig& config);
    std::string generate_conclusions(const InsightCollection& insights, const ReportConfig& config);

    // Helpers
    std::string get_node_label(const std::string& node_id) const;
    std::string format_entity_list(const std::vector<std::string>& entities, int max_items = 5) const;
    std::string format_confidence(double score) const;
    std::string get_insight_type_name(InsightType type) const;
    std::string get_current_timestamp() const;
    std::string build_llm_prompt(const Insight& insight, int max_witness_nodes) const;
    std::string get_llm_example(const Insight& insight, const ReportConfig& config);
    std::string get_graph_context_summary(const Insight& insight, bool markdown) const;

    /**
     * @brief Generate a contextual narrative for an insight using LLM
     *
     * This method invokes the LLM to produce a rich, natural language
     * description of the finding, providing domain context and explaining
     * the significance of the discovery.
     *
     * @param insight The insight to describe
     * @param config Report configuration
     * @return LLM-generated narrative, or fallback template text if LLM unavailable
     */
    std::string generate_llm_narrative(const Insight& insight, const ReportConfig& config);

    // Natural language generators
    std::string describe_bridge(const Insight& insight) const;
    std::string describe_completion(const Insight& insight) const;
    std::string describe_motif(const Insight& insight) const;
    std::string describe_substitution(const Insight& insight) const;
    std::string describe_surprise(const Insight& insight) const;
    std::string describe_diffusion(const Insight& insight) const;
    std::string describe_path_rank(const Insight& insight) const;
    std::string describe_community_link(const Insight& insight) const;
    std::string describe_hypothesis(const Insight& insight) const;
    std::string describe_rule(const Insight& insight) const;
    std::string describe_embedding_link(const Insight& insight) const;
    std::string describe_author_chain(const Insight& insight) const;

    // Clustering helpers for HTML coalescing
    /**
     * @brief Cluster similar insights together to reduce repetition in reports
     * @param insights Vector of insights to cluster
     * @param config Report configuration with similarity threshold
     * @return Vector of insight clusters
     */
    std::vector<InsightCluster> cluster_insights(
        const std::vector<Insight>& insights,
        const ReportConfig& config
    ) const;

    /**
     * @brief Calculate similarity between two insights based on entities and scores
     * @param a First insight
     * @param b Second insight
     * @return Similarity score between 0 and 1
     */
    double calculate_insight_similarity(const Insight& a, const Insight& b) const;

    /**
     * @brief Generate a summary description for a cluster of insights
     * @param cluster The cluster to summarize
     * @param config Report configuration
     * @return Summary text for the cluster
     */
    std::string generate_cluster_summary(InsightCluster& cluster, const ReportConfig& config);

    /**
     * @brief Identify common theme among clustered insights
     * @param cluster The cluster to analyze
     * @return String describing the common theme
     */
    std::string identify_cluster_theme(const InsightCluster& cluster) const;
};

} // namespace kg

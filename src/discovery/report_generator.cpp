#include "discovery/report_generator.hpp"
#include "llm/llm_provider.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <set>

namespace kg {

ReportGenerator::ReportGenerator(const Hypergraph& graph)
    : graph_(graph) {}

std::string ReportGenerator::get_node_label(const std::string& node_id) const {
    const auto* node = graph_.get_node(node_id);
    return node ? node->label : node_id;
}

std::string ReportGenerator::format_entity_list(const std::vector<std::string>& entities, int max_items) const {
    if (entities.empty()) return "(none)";

    std::stringstream ss;
    int count = 0;
    for (const auto& e : entities) {
        if (count > 0) {
            if (count == static_cast<int>(entities.size()) - 1 && count < max_items) {
                ss << " and ";
            } else {
                ss << ", ";
            }
        }
        if (count >= max_items) {
            ss << "and " << (entities.size() - max_items) << " others";
            break;
        }
        ss << "**" << get_node_label(e) << "**";
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::format_confidence(double score) const {
    if (score >= 0.9) return "very high";
    if (score >= 0.7) return "high";
    if (score >= 0.5) return "moderate";
    if (score >= 0.3) return "low";
    return "very low";
}

std::string ReportGenerator::get_insight_type_name(InsightType type) const {
    switch (type) {
        case InsightType::BRIDGE: return "Bridge";
        case InsightType::COMPLETION: return "Completion";
        case InsightType::MOTIF: return "Motif";
        case InsightType::SUBSTITUTION: return "Substitution";
        case InsightType::DIFFUSION: return "Diffusion";
        case InsightType::SURPRISE: return "Surprise";
        case InsightType::COMMUNITY_LINK: return "Community Link";
        case InsightType::PATH_RANK: return "Path Rank";
        case InsightType::HYPOTHESIS: return "Hypothesis";
        case InsightType::RULE: return "Rule";
        case InsightType::EMBEDDING_LINK: return "Embedding Link";
        case InsightType::AUTHOR_CHAIN: return "Author Chain";
        default: return "Unknown";
    }
}

std::string ReportGenerator::get_current_timestamp() const {
    auto now = std::time(nullptr);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&now), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

void ReportGenerator::set_llm_provider(const std::shared_ptr<LLMProvider>& provider) {
    llm_provider_ = provider;
}

std::string ReportGenerator::build_llm_prompt(const Insight& insight, int max_witness_nodes) const {
    std::vector<std::string> seed_labels = insight.seed_labels;
    if (seed_labels.empty()) {
        for (const auto& id : insight.seed_nodes) {
            std::string label = get_node_label(id);
            seed_labels.push_back(label.empty() ? id : label);
        }
    }

    std::vector<std::string> witness_labels;
    for (size_t i = 0; i < insight.witness_nodes.size() && static_cast<int>(i) < max_witness_nodes; ++i) {
        const auto& id = insight.witness_nodes[i];
        std::string label = get_node_label(id);
        witness_labels.push_back(label.empty() ? id : label);
    }

    std::stringstream prompt;
    const std::string context_summary = get_graph_context_summary(insight, false);
    prompt << "You are a knowledge graph analyst. "
           << "Write 2-3 concise sentences about the relationship highlighted here. "
           << "The explanation must stay within the graph context and avoid speculative analysis.\n\n";
    prompt << "Method: " << get_insight_type_name(insight.type) << "\n";
    if (!seed_labels.empty()) {
        prompt << "Seed entities: " << format_entity_list(seed_labels, 6) << "\n";
    }
    if (!witness_labels.empty()) {
        prompt << "Witness entities: " << format_entity_list(witness_labels, 8) << "\n";
    }
    if (!insight.description.empty()) {
        prompt << "Insight description: " << insight.description << "\n";
    }
    if (!context_summary.empty()) {
        prompt << "Graph context summary: " << context_summary << "\n";
    }
    prompt << "Task: First sentence restates the relationship in plain language. "
           << "Then add 1-2 sentences with more context or data based only on the entities listed above. "
           << "Do not introduce new facts. Return plain text only.";
    return prompt.str();
}

std::string ReportGenerator::get_llm_example(const Insight& insight, const ReportConfig& config) {
    if (!llm_provider_) return "";
    auto it = llm_example_cache_.find(insight.insight_id);
    if (it != llm_example_cache_.end()) return it->second;

    const std::string prompt = build_llm_prompt(insight, config.llm_max_witness_nodes);
    std::vector<Message> messages = {
        Message(Message::Role::System, "You are a careful, concise analyst."),
        Message(Message::Role::User, prompt)
    };
    LLMResponse response = llm_provider_->chat(messages);
    if (!response.success) {
        return "";
    }

    std::string content = response.content;
    llm_example_cache_[insight.insight_id] = content;
    return content;
}

std::string ReportGenerator::generate_llm_narrative(const Insight& insight, const ReportConfig& config) {
    // Check cache first using a narrative-specific key
    std::string cache_key = "narrative:" + insight.insight_id;
    auto it = llm_example_cache_.find(cache_key);
    if (it != llm_example_cache_.end()) return it->second;

    // If no LLM provider, fall back to template-based description
    if (!llm_provider_) {
        return get_graph_context_summary(insight, config.markdown_format);
    }

    // Build entity labels
    std::vector<std::string> seed_labels = insight.seed_labels;
    if (seed_labels.empty()) {
        for (const auto& id : insight.seed_nodes) {
            std::string label = get_node_label(id);
            seed_labels.push_back(label.empty() ? id : label);
        }
    }

    std::vector<std::string> witness_labels;
    for (size_t i = 0; i < insight.witness_nodes.size() && static_cast<int>(i) < config.llm_max_witness_nodes; ++i) {
        const auto& id = insight.witness_nodes[i];
        std::string label = get_node_label(id);
        witness_labels.push_back(label.empty() ? id : label);
    }

    // Build a richer prompt for narrative generation
    std::stringstream prompt;
    prompt << "You are a knowledge discovery analyst examining findings from an automated knowledge graph analysis. "
           << "Your task is to explain the following discovery in clear, accessible language that provides context "
           << "and helps the reader understand its significance.\n\n";

    prompt << "## Discovery Type: " << get_insight_type_name(insight.type) << "\n\n";

    // Add type-specific context
    switch (insight.type) {
        case InsightType::BRIDGE:
            prompt << "A bridge entity connects otherwise separate clusters of information in the knowledge graph, "
                   << "serving as a critical junction for information flow.\n\n";
            break;
        case InsightType::COMPLETION:
            prompt << "A completion suggests a missing relationship that should likely exist based on patterns "
                   << "observed elsewhere in the knowledge graph.\n\n";
            break;
        case InsightType::MOTIF:
            prompt << "A motif is a recurring pattern of entities that frequently co-occur, suggesting a thematic "
                   << "or conceptual relationship.\n\n";
            break;
        case InsightType::SUBSTITUTION:
            prompt << "A substitution identifies entities that appear interchangeable in context, potentially "
                   << "indicating synonyms or closely related concepts.\n\n";
            break;
        case InsightType::DIFFUSION:
            prompt << "Diffusion analysis reveals influence pathways showing how concepts relate through the "
                   << "network structure, even when not directly connected.\n\n";
            break;
        case InsightType::SURPRISE:
            prompt << "A surprising discovery is a connection that is statistically unusual given the overall "
                   << "structure of the knowledge graph, potentially representing a novel finding.\n\n";
            break;
        case InsightType::PATH_RANK:
            prompt << "Path-ranked links are entity pairs connected by multiple short paths in the graph, "
                   << "suggesting a relationship supported by structure but not directly observed.\n\n";
            break;
        case InsightType::RULE:
            prompt << "An association rule reveals predictable patterns between relation types that can be "
                   << "used for knowledge inference or validation.\n\n";
            break;
        case InsightType::EMBEDDING_LINK:
            prompt << "An embedding-based link prediction uses TransE neural embeddings to predict missing "
                   << "relationships. TransE learns vector representations where valid relations satisfy h + r ≈ t. "
                   << "This prediction suggests a plausible but unobserved relationship.\n\n";
            break;
        case InsightType::AUTHOR_CHAIN:
            prompt << "An author reference chain traces a sequence of citations between authors, highlighting "
                   << "how scholarly influence or references propagate through the literature.\n\n";
            break;
        default:
            break;
    }

    prompt << "## Entities Involved\n";
    if (!seed_labels.empty()) {
        prompt << "Primary entities: ";
        for (size_t i = 0; i < seed_labels.size(); ++i) {
            if (i > 0) prompt << ", ";
            prompt << seed_labels[i];
        }
        prompt << "\n";
    }
    if (!witness_labels.empty()) {
        prompt << "Related entities: ";
        for (size_t i = 0; i < witness_labels.size(); ++i) {
            if (i > 0) prompt << ", ";
            prompt << witness_labels[i];
        }
        prompt << "\n";
    }
    prompt << "\n";

    prompt << "## Confidence Score: " << std::fixed << std::setprecision(2) << insight.score << " (" << format_confidence(insight.score) << ")\n\n";

    if (!insight.description.empty()) {
        prompt << "## Additional Context\n" << insight.description << "\n\n";
    }

    // Get the template description for additional context
    std::string template_desc = get_graph_context_summary(insight, false);
    if (!template_desc.empty()) {
        prompt << "## Graph Structure Analysis\n" << template_desc << "\n\n";
    }

    prompt << "## Your Task\n"
           << "Write a 3-5 sentence explanation of this finding that:\n"
           << "1. Explains what was discovered in plain language\n"
           << "2. Provides context about why this might be significant\n"
           << "3. Suggests what this could mean for understanding the domain\n\n"
           << "Focus on being informative and accessible. Do not introduce facts not present in the data above. "
           << "Return plain text only, no markdown formatting.";

    std::vector<Message> messages = {
        Message(Message::Role::System,
            "You are an expert knowledge graph analyst. Your explanations are clear, insightful, and grounded "
            "in the data provided. You help readers understand complex graph-based discoveries in accessible terms."),
        Message(Message::Role::User, prompt.str())
    };

    LLMResponse response = llm_provider_->chat(messages);
    if (!response.success) {
        // Fall back to template
        return get_graph_context_summary(insight, config.markdown_format);
    }

    std::string narrative = response.content;
    llm_example_cache_[cache_key] = narrative;
    return narrative;
}

std::string ReportGenerator::describe_bridge(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.empty()) return "";

    std::string entity = insight.seed_labels[0];
    int components = static_cast<int>(insight.score);

    ss << "The entity **" << entity << "** serves as a critical bridge in the knowledge graph. ";
    if (components >= 2) {
        ss << "It connects " << components << " otherwise separate clusters of information. ";
    }

    if (!insight.witness_nodes.empty()) {
        ss << "Through this entity, concepts like ";
        std::vector<std::string> examples;
        for (size_t i = 0; i < std::min(size_t(3), insight.witness_nodes.size()); ++i) {
            examples.push_back(get_node_label(insight.witness_nodes[i]));
        }
        for (size_t i = 0; i < examples.size(); ++i) {
            if (i > 0) ss << (i == examples.size() - 1 ? " and " : ", ");
            ss << "**" << examples[i] << "**";
        }
        ss << " become interconnected. ";
    }

    ss << "This suggests that **" << entity << "** plays a pivotal role in bridging different domains of knowledge.";

    return ss.str();
}

std::string ReportGenerator::describe_completion(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    ss << "A potential knowledge gap was identified: the relationship between **"
       << insight.seed_labels[0] << "** and **" << insight.seed_labels[1]
       << "** appears incomplete. ";

    if (!insight.witness_nodes.empty() && insight.witness_nodes.size() > 2) {
        ss << "Based on similar patterns in the graph, entities like **"
           << get_node_label(insight.witness_nodes[2]) << "** might also be relevant to this relationship. ";
    }

    ss << "This represents an opportunity to enrich the knowledge graph with additional connections "
       << "(confidence: " << format_confidence(insight.score) << ").";

    return ss.str();
}

std::string ReportGenerator::describe_motif(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.empty()) return "";

    ss << "A recurring pattern was discovered involving ";

    for (size_t i = 0; i < std::min(size_t(4), insight.seed_labels.size()); ++i) {
        if (i > 0) ss << (i == std::min(size_t(4), insight.seed_labels.size()) - 1 ? " and " : ", ");
        ss << "**" << insight.seed_labels[i] << "**";
    }

    if (insight.seed_labels.size() > 4) {
        ss << " (and " << (insight.seed_labels.size() - 4) << " others)";
    }

    ss << ". These entities frequently co-occur across " << static_cast<int>(insight.score)
       << " different contexts, suggesting a strong thematic or conceptual relationship. "
       << "This motif may represent a fundamental concept cluster in the domain.";

    return ss.str();
}

std::string ReportGenerator::describe_substitution(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    ss << "The entities **" << insight.seed_labels[0] << "** and **" << insight.seed_labels[1]
       << "** appear to be interchangeable in certain contexts. ";

    ss << "They share " << std::fixed << std::setprecision(0) << (insight.score * 100)
       << "% of their relational patterns, appearing in similar types of relationships. ";

    ss << "This could indicate that they are synonyms, closely related concepts, "
       << "or represent the same entity under different names.";

    return ss.str();
}

std::string ReportGenerator::describe_surprise(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.empty()) return "";

    ss << "An unexpected connection was found: ";

    if (insight.seed_labels.size() == 1) {
        ss << "**" << insight.seed_labels[0] << "** appears in an unusual context. ";
    } else {
        ss << "the combination of ";
        for (size_t i = 0; i < std::min(size_t(3), insight.seed_labels.size()); ++i) {
            if (i > 0) ss << (i == std::min(size_t(3), insight.seed_labels.size()) - 1 ? " and " : ", ");
            ss << "**" << insight.seed_labels[i] << "**";
        }
        ss << " is statistically rare in this knowledge domain. ";
    }

    ss << "With a surprise score of " << std::fixed << std::setprecision(2) << insight.score
       << ", this represents a potentially novel or overlooked relationship that warrants further investigation.";

    return ss.str();
}

std::string ReportGenerator::describe_diffusion(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    ss << "Through network analysis, **" << insight.seed_labels[0]
       << "** was found to have strong influence pathways to **" << insight.seed_labels[1]
       << "** (relevance score: " << std::fixed << std::setprecision(3) << insight.score << "). ";

    ss << "This suggests that information or concepts flow naturally between these entities, "
       << "even if they are not directly connected.";

    return ss.str();
}

std::string ReportGenerator::describe_rule(const Insight& insight) const {
    std::stringstream ss;

    if (insight.description.empty()) return "";

    // Parse the description to extract rule components
    ss << "An association rule was discovered: " << insight.description << ". ";

    if (!insight.seed_labels.empty()) {
        ss << "Example entities that satisfy this rule include ";
        for (size_t i = 0; i < std::min(size_t(3), insight.seed_labels.size()); ++i) {
            if (i > 0) ss << (i == std::min(size_t(3), insight.seed_labels.size()) - 1 ? " and " : ", ");
            ss << "**" << insight.seed_labels[i] << "**";
        }
        if (insight.seed_labels.size() > 3) {
            ss << " (and " << (insight.seed_labels.size() - 3) << " others)";
        }
        ss << ". ";
    }

    ss << "This pattern suggests a predictable relationship structure that could be used "
       << "for knowledge inference or validation.";

    return ss.str();
}

std::string ReportGenerator::describe_path_rank(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    ss << "A path-ranked signal links **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1]
       << "** through multiple short paths in the graph. ";

    if (!insight.description.empty()) {
        ss << insight.description << ". ";
    }

    if (!insight.witness_edges.empty()) {
        ss << "This candidate link is supported by " << insight.witness_edges.size()
           << " evidence edges ";
    }

    ss << "(confidence: " << format_confidence(insight.score) << ").";

    return ss.str();
}

std::string ReportGenerator::describe_embedding_link(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    ss << "Embedding-based link prediction (TransE) suggests a relationship between **"
       << insight.seed_labels[0] << "** and **" << insight.seed_labels[1] << "**. ";

    // Extract relation type from novelty tags
    std::string relation_type = "related_to";
    for (const auto& tag : insight.novelty_tags) {
        if (tag.find("relation=") == 0) {
            relation_type = tag.substr(9);
            break;
        }
    }

    ss << "The predicted relation type is **" << relation_type << "**. ";

    // Get plausibility from score breakdown
    auto it = insight.score_breakdown.find("novelty");
    if (it != insight.score_breakdown.end()) {
        ss << "The model assigns a plausibility score of "
           << std::fixed << std::setprecision(3) << it->second << ", ";
        if (it->second > 0.7) {
            ss << "indicating high confidence in this prediction. ";
        } else if (it->second > 0.5) {
            ss << "indicating moderate confidence. ";
        } else {
            ss << "suggesting this link warrants further validation. ";
        }
    }

    if (!insight.witness_edges.empty()) {
        ss << "This prediction is grounded in " << insight.witness_edges.size()
           << " existing edges involving these entities.";
    }

    return ss.str();
}

std::string ReportGenerator::describe_community_link(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    ss << "A cross-cluster signal links **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1]
       << "** based on shared relation patterns across separate communities. ";

    if (!insight.description.empty()) {
        ss << insight.description << ". ";
    }

    if (!insight.witness_edges.empty()) {
        ss << "This candidate link is supported by " << insight.witness_edges.size()
           << " evidence edges ";
    }

    ss << "(confidence: " << format_confidence(insight.score) << ").";

    return ss.str();
}

std::string ReportGenerator::describe_hypothesis(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    ss << "Hypothesis: ";
    if (insight.seed_labels.size() >= 2) {
        ss << "**" << insight.seed_labels[0] << "** is meaningfully related to **"
           << insight.seed_labels[1] << "** based on patterns in the augmented graph. ";
    } else {
        ss << "**" << insight.seed_labels[0] << "** is a key driver in the observed graph structure. ";
    }

    if (!insight.description.empty()) {
        ss << insight.description << ". ";
    }

    if (!insight.evidence_chunk_ids.empty()) {
        ss << "Evidence sources include " << insight.evidence_chunk_ids[0];
        if (insight.evidence_chunk_ids.size() > 1) {
            ss << " and " << (insight.evidence_chunk_ids.size() - 1) << " others";
        }
        ss << ".";
    }

    return ss.str();
}

std::string ReportGenerator::describe_author_chain(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 3) return "";

    ss << "A reference trail connects **" << insight.seed_labels[0] << "** to **"
       << insight.seed_labels[1] << "** and onward to **" << insight.seed_labels[2]
       << "**, suggesting a chain of citations across authors. ";

    if (!insight.witness_edges.empty()) {
        ss << "The chain is supported by " << insight.witness_edges.size()
           << " citation-related edges ";
    }

    ss << "(confidence: " << format_confidence(insight.score) << ").";
    return ss.str();
}

std::string ReportGenerator::generate_header(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream ss;

    if (config.markdown_format) {
        ss << "# " << config.title << "\n\n";
        ss << "**Generated:** " << get_current_timestamp() << "  \n";
        if (!config.source_document.empty()) {
            ss << "**Source:** " << config.source_document << "  \n";
        }
        ss << "**Run ID:** " << insights.run_id << "  \n";
        ss << "\n---\n\n";
    } else {
        ss << config.title << "\n";
        ss << std::string(config.title.length(), '=') << "\n\n";
        ss << "Generated: " << get_current_timestamp() << "\n";
        if (!config.source_document.empty()) {
            ss << "Source: " << config.source_document << "\n";
        }
        ss << "\n";
    }

    return ss.str();
}

std::string ReportGenerator::generate_executive_summary(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream ss;

    // Count insights by type
    std::map<InsightType, int> counts;
    for (const auto& insight : insights.insights) {
        counts[insight.type]++;
    }

    if (config.markdown_format) {
        ss << "## Executive Summary\n\n";
    } else {
        ss << "EXECUTIVE SUMMARY\n";
        ss << "-----------------\n\n";
    }

    ss << "This report presents the results of automated knowledge discovery analysis on the extracted knowledge graph. ";
    ss << "A total of **" << insights.insights.size() << " insights** were identified across "
       << counts.size() << " different categories.\n\n";

    // Key findings
    if (config.markdown_format) {
        ss << "### Key Findings\n\n";
    }

    if (counts[InsightType::BRIDGE] > 0) {
        ss << "- **" << counts[InsightType::BRIDGE] << " bridge entities** were identified that connect separate knowledge clusters, "
           << "revealing critical nodes for information flow.\n";
    }

    if (counts[InsightType::COMPLETION] > 0) {
        ss << "- **" << counts[InsightType::COMPLETION] << " potential completions** suggest relationships that could be extended "
           << "with additional entities.\n";
    }

    if (counts[InsightType::MOTIF] > 0) {
        ss << "- **" << counts[InsightType::MOTIF] << " recurring patterns (motifs)** were detected, indicating frequently "
           << "co-occurring concept clusters.\n";
    }

    if (counts[InsightType::SUBSTITUTION] > 0) {
        ss << "- **" << counts[InsightType::SUBSTITUTION] << " potential substitutions** were found where entities appear "
           << "interchangeable in context.\n";
    }

    if (counts[InsightType::SURPRISE] > 0) {
        ss << "- **" << counts[InsightType::SURPRISE] << " surprising connections** were discovered that represent "
           << "statistically unusual or novel relationships.\n";
    }

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        ss << "- **" << counts[InsightType::COMMUNITY_LINK] << " community links** highlight cross-cluster candidates "
           << "with similar relation signatures.\n";
    }

    if (counts[InsightType::HYPOTHESIS] > 0) {
        ss << "- **" << counts[InsightType::HYPOTHESIS] << " hypotheses** synthesize testable claims from the "
           << "combined discovery findings.\n";
    }

    if (counts[InsightType::DIFFUSION] > 0) {
        ss << "- **" << counts[InsightType::DIFFUSION] << " influence pathways** were mapped showing how concepts "
           << "relate through network diffusion.\n";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        ss << "- **" << counts[InsightType::PATH_RANK] << " path-ranked links** highlight pairs connected by multiple short paths.\n";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        ss << "- **" << counts[InsightType::AUTHOR_CHAIN] << " author reference chains** trace citation pathways across authors.\n";
    }

    if (counts[InsightType::RULE] > 0) {
        ss << "- **" << counts[InsightType::RULE] << " association rules** were mined, revealing predictable "
           << "relationship patterns between different relation types.\n";
    }

    if (counts[InsightType::EMBEDDING_LINK] > 0) {
        ss << "- **" << counts[InsightType::EMBEDDING_LINK] << " embedding-based predictions** were generated using TransE "
           << "link prediction, suggesting plausible missing relationships.\n";
    }

    ss << "\n";
    return ss.str();
}

std::string ReportGenerator::generate_statistics_section(const InsightCollection& insights, const ReportConfig& config) {
    if (!config.include_statistics) return "";

    std::stringstream ss;

    auto stats = graph_.compute_statistics();

    if (config.markdown_format) {
        ss << "## Knowledge Graph Statistics\n\n";
        ss << "| Metric | Value |\n";
        ss << "|--------|-------|\n";
        ss << "| Total Entities | " << stats.num_nodes << " |\n";
        ss << "| Total Relationships | " << stats.num_edges << " |\n";
        ss << "| Average Degree | " << std::fixed << std::setprecision(2) << stats.avg_node_degree << " |\n";
        ss << "| Max Degree | " << stats.max_node_degree << " |\n";
        ss << "| Insights Discovered | " << insights.insights.size() << " |\n";
        ss << "\n";
    } else {
        ss << "KNOWLEDGE GRAPH STATISTICS\n";
        ss << "--------------------------\n";
        ss << "Total Entities: " << stats.num_nodes << "\n";
        ss << "Total Relationships: " << stats.num_edges << "\n";
        ss << "Average Degree: " << std::fixed << std::setprecision(2) << stats.avg_node_degree << "\n";
        ss << "Insights Discovered: " << insights.insights.size() << "\n\n";
    }

    return ss.str();
}

std::string ReportGenerator::generate_augmentation_overview(const ReportConfig& config) {
    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Augmentation Methods\n\n";
        ss << "This report uses twelve augmentation signals derived from the graph structure and local patterns:\n";
        ss << "- **Bridge**: identifies entities that connect otherwise separate regions of the graph.\n";
        ss << "- **Completion**: suggests missing relations based on similar neighborhood patterns.\n";
        ss << "- **Motif**: highlights recurring subgraph structures that indicate repeated concepts.\n";
        ss << "- **Substitution**: proposes interchangeable entities in similar contexts.\n";
        ss << "- **Diffusion**: maps likely influence pathways over the graph.\n";
        ss << "- **Surprise**: flags statistically unusual or novel connections.\n";
        ss << "- **Community Link**: proposes cross-cluster links with similar relation signatures.\n";
        ss << "- **Path Rank**: proposes links supported by multiple short graph paths.\n";
        ss << "- **Author Chain**: traces citation chains across authors.\n";
        ss << "- **Hypothesis**: synthesizes testable claims from combined discovery findings.\n";
        ss << "- **Rule**: mines association rules between relation types (e.g., if R1(X,Y) then R2(X,Z)).\n\n";
        ss << "- **Embedding Link**: predicts missing relations using TransE embeddings.\n\n";
        ss << "Alternatives to consider:\n";
        ss << "- Embedding-based link prediction (TransE, RotatE, ComplEx).\n";
        ss << "- GNN-based link prediction (GraphSAGE, GAT, RGCN).\n";
        ss << "- Path-based ranking or meta-path inference for schema-aware graphs.\n";
        ss << "- Community-aware augmentation that prioritizes cross-cluster links.\n\n";
    } else {
        ss << "AUGMENTATION METHODS\n";
        ss << "--------------------\n";
        ss << "This report uses twelve augmentation signals derived from the graph structure and local patterns:\n";
        ss << "1) Bridge: identifies entities that connect otherwise separate regions of the graph.\n";
        ss << "2) Completion: suggests missing relations based on similar neighborhood patterns.\n";
        ss << "3) Motif: highlights recurring subgraph structures that indicate repeated concepts.\n";
        ss << "4) Substitution: proposes interchangeable entities in similar contexts.\n";
        ss << "5) Diffusion: maps likely influence pathways over the graph.\n";
        ss << "6) Surprise: flags statistically unusual or novel connections.\n";
        ss << "7) Community Link: proposes cross-cluster links with similar relation signatures.\n";
        ss << "8) Path Rank: proposes links supported by multiple short graph paths.\n";
        ss << "9) Author Chain: traces citation chains across authors.\n";
        ss << "10) Hypothesis: synthesizes testable claims from combined findings.\n";
        ss << "11) Rule: mines association rules between relation types.\n";
        ss << "12) Embedding Link: predicts missing relations using TransE embeddings.\n\n";
        ss << "Alternatives to consider:\n";
        ss << "- Embedding-based link prediction (TransE, RotatE, ComplEx).\n";
        ss << "- GNN-based link prediction (GraphSAGE, GAT, RGCN).\n";
        ss << "- Path-based ranking or meta-path inference for schema-aware graphs.\n";
        ss << "- Community-aware augmentation that prioritizes cross-cluster links.\n\n";
    }

    return ss.str();
}

namespace {
std::string strip_markdown_bold(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '*') {
            ++i;
            continue;
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::string escape_html(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}
} // namespace

std::string ReportGenerator::get_graph_context_summary(const Insight& insight, bool markdown) const {
    std::string summary;
    switch (insight.type) {
        case InsightType::BRIDGE:
            summary = describe_bridge(insight);
            break;
        case InsightType::COMPLETION:
            summary = describe_completion(insight);
            break;
        case InsightType::MOTIF:
            summary = describe_motif(insight);
            break;
        case InsightType::SUBSTITUTION:
            summary = describe_substitution(insight);
            break;
        case InsightType::DIFFUSION:
            summary = describe_diffusion(insight);
            break;
        case InsightType::SURPRISE:
            summary = describe_surprise(insight);
            break;
        case InsightType::COMMUNITY_LINK:
            summary = describe_community_link(insight);
            break;
        case InsightType::PATH_RANK:
            summary = describe_path_rank(insight);
            break;
        case InsightType::HYPOTHESIS:
            summary = describe_hypothesis(insight);
            break;
        case InsightType::RULE:
            summary = describe_rule(insight);
            break;
        case InsightType::EMBEDDING_LINK:
            summary = describe_embedding_link(insight);
            break;
        case InsightType::AUTHOR_CHAIN:
            summary = describe_author_chain(insight);
            break;
        default:
            break;
    }

    if (!markdown) {
        summary = strip_markdown_bold(summary);
    }
    return summary;
}

std::string ReportGenerator::generate_llm_examples_section(
    const std::map<InsightType, std::vector<Insight>>& by_type,
    const ReportConfig& config) {
    if (!config.include_llm_examples || !llm_provider_) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## LLM-Synthesized Examples\n\n";
        ss << "Top example per discovery method, with graph-context summaries and LLM expansions.\n\n";
    } else {
        ss << "LLM-SYNTHESIZED EXAMPLES\n";
        ss << "------------------------\n";
        ss << "Top example per discovery method, with graph-context summaries and LLM expansions.\n\n";
    }

    const std::vector<InsightType> order = {
        InsightType::BRIDGE,
        InsightType::COMPLETION,
        InsightType::MOTIF,
        InsightType::SUBSTITUTION,
        InsightType::DIFFUSION,
        InsightType::SURPRISE,
        InsightType::COMMUNITY_LINK,
        InsightType::PATH_RANK,
        InsightType::AUTHOR_CHAIN,
        InsightType::HYPOTHESIS,
        InsightType::RULE,
        InsightType::EMBEDDING_LINK
    };

    int added = 0;
    for (const auto& type : order) {
        auto it = by_type.find(type);
        if (it == by_type.end() || it->second.empty()) continue;

        std::vector<Insight> sorted = it->second;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        int examples = std::min(config.llm_examples_per_type, static_cast<int>(sorted.size()));
        for (int i = 0; i < examples; ++i) {
            const Insight& insight = sorted[i];
            std::string example = get_llm_example(insight, config);
            if (example.empty()) continue;

            if (config.markdown_format) {
                if (i == 0) {
                    ss << "### " << get_insight_type_name(type) << "\n\n";
                }
                const std::string context_summary = get_graph_context_summary(insight, true);
                if (!context_summary.empty()) {
                    ss << "**Graph context:** " << context_summary << "\n\n";
                }
                ss << "**LLM expansion:** " << example << "\n\n";
            } else {
                if (i == 0) {
                    ss << get_insight_type_name(type) << "\n";
                }
                const std::string context_summary = get_graph_context_summary(insight, false);
                if (!context_summary.empty()) {
                    ss << "Graph context: " << context_summary << "\n";
                }
                ss << "LLM expansion: " << example << "\n\n";
            }
            added++;
        }
    }

    if (added == 0) return "";
    return ss.str();
}

std::string ReportGenerator::generate_llm_examples_section_html(
    const std::map<InsightType, std::vector<Insight>>& by_type,
    const ReportConfig& config) {
    if (!config.include_llm_examples || !llm_provider_) return "";

    std::stringstream html;
    html << R"(
        <section id="llm-examples">
            <h2>LLM-Synthesized Examples</h2>
            <p>Top example per discovery method, with graph-context summaries and LLM expansions.</p>
)";

    const std::vector<InsightType> order = {
        InsightType::BRIDGE,
        InsightType::COMPLETION,
        InsightType::MOTIF,
        InsightType::SUBSTITUTION,
        InsightType::DIFFUSION,
        InsightType::SURPRISE,
        InsightType::COMMUNITY_LINK,
        InsightType::PATH_RANK,
        InsightType::AUTHOR_CHAIN,
        InsightType::HYPOTHESIS,
        InsightType::RULE,
        InsightType::EMBEDDING_LINK
    };

    int added = 0;
    for (const auto& type : order) {
        auto it = by_type.find(type);
        if (it == by_type.end() || it->second.empty()) continue;

        std::vector<Insight> sorted = it->second;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        int examples = std::min(config.llm_examples_per_type, static_cast<int>(sorted.size()));
        for (int i = 0; i < examples; ++i) {
            const Insight& insight = sorted[i];
            std::string example = get_llm_example(insight, config);
            if (example.empty()) continue;

            const std::string context_summary = get_graph_context_summary(insight, false);
            html << R"(
            <div class="insight">
                <h4>)" << get_insight_type_name(type) << R"(</h4>
)";
            if (!context_summary.empty()) {
                html << R"(                <p><strong>Graph context:</strong> )"
                     << escape_html(context_summary) << R"(</p>
)";
            }
            html << R"(                <p><strong>LLM expansion:</strong> )"
                 << escape_html(example) << R"(</p>
            </div>
)";
            added++;
        }
    }

    if (added == 0) return "";
    html << R"(        </section>
)";
    return html.str();
}

std::string ReportGenerator::generate_bridges_section(const std::vector<Insight>& bridges, const ReportConfig& config) {
    if (bridges.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Bridge Entities\n\n";
        ss << "Bridge entities are nodes that connect otherwise disconnected parts of the knowledge graph. "
           << "They serve as critical junctions for information flow and often represent interdisciplinary concepts.\n\n";
    } else {
        ss << "BRIDGE ENTITIES\n";
        ss << "---------------\n\n";
    }

    // Sort by score (number of components connected)
    std::vector<Insight> sorted = bridges;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". " << (insight.seed_labels.empty() ? "Unknown" : insight.seed_labels[0]) << "\n\n";
        } else {
            ss << (count + 1) << ". " << (insight.seed_labels.empty() ? "Unknown" : insight.seed_labels[0]) << "\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_bridge(insight);
        }
        ss << narrative << "\n\n";

        if (config.include_evidence && !insight.evidence_chunk_ids.empty()) {
            ss << "*Evidence from: " << insight.evidence_chunk_ids[0];
            if (insight.evidence_chunk_ids.size() > 1) {
                ss << " and " << (insight.evidence_chunk_ids.size() - 1) << " other sources";
            }
            ss << "*\n\n";
        }

        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_completions_section(const std::vector<Insight>& completions, const ReportConfig& config) {
    if (completions.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Knowledge Gaps (Potential Completions)\n\n";
        ss << "These insights identify relationships that appear incomplete based on patterns in the graph. "
           << "They represent opportunities to enrich the knowledge base.\n\n";
    }

    std::vector<Insight> sorted = completions;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". ";
            if (insight.seed_labels.size() >= 2) {
                ss << insight.seed_labels[0] << " + " << insight.seed_labels[1];
            }
            ss << "\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_completion(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_motifs_section(const std::vector<Insight>& motifs, const ReportConfig& config) {
    if (motifs.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Recurring Patterns (Motifs)\n\n";
        ss << "Motifs are frequently co-occurring sets of entities that form recognizable patterns. "
           << "They often represent core concepts or themes in the domain.\n\n";
    }

    std::vector<Insight> sorted = motifs;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Pattern Cluster\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_motif(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_substitutions_section(const std::vector<Insight>& substitutions, const ReportConfig& config) {
    if (substitutions.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Interchangeable Entities (Substitutions)\n\n";
        ss << "These pairs of entities appear in similar contexts and may represent synonyms, "
           << "aliases, or closely related concepts that could be merged or linked.\n\n";
    }

    std::vector<Insight> sorted = substitutions;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ↔ " << insight.seed_labels[1] << "\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_substitution(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_surprise_section(const std::vector<Insight>& surprises, const ReportConfig& config) {
    if (surprises.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Surprising Discoveries\n\n";
        ss << "These connections are statistically unusual given the overall structure of the knowledge graph. "
           << "They may represent novel findings, errors, or genuinely unexpected relationships worth investigating.\n\n";
    }

    std::vector<Insight> sorted = surprises;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Unexpected Connection\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_surprise(insight);
        }
        ss << narrative << "\n\n";

        if (config.include_evidence && !insight.evidence_chunk_ids.empty()) {
            ss << "*Found in: " << insight.evidence_chunk_ids[0] << "*\n\n";
        }

        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_diffusion_section(const std::vector<Insight>& diffusions, const ReportConfig& config) {
    if (diffusions.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Influence Pathways (Diffusion Analysis)\n\n";
        ss << "These insights show how concepts relate through network paths, even when not directly connected.\n\n";
    }

    std::vector<Insight> sorted = diffusions;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_diffusion(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_path_rank_section(const std::vector<Insight>& paths, const ReportConfig& config) {
    if (paths.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Path-Ranked Links\n\n";
        ss << "These candidates are inferred from multiple short paths between entity pairs, "
           << "highlighting relationships that are structurally supported but not directly observed.\n\n";
    } else {
        ss << "PATH-RANKED LINKS\n";
        ss << "-----------------\n\n";
    }

    std::vector<Insight> sorted = paths;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " ↔ " << insight.seed_labels[1] << "\n\n";
        } else if (!config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << (count + 1) << ". " << insight.seed_labels[0]
               << " <-> " << insight.seed_labels[1] << "\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_path_rank(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_author_chains_section(const std::vector<Insight>& chains, const ReportConfig& config) {
    if (chains.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Author Reference Chains\n\n";
        ss << "These chains trace citation pathways across authors, highlighting how references propagate through the literature.\n\n";
    } else {
        ss << "AUTHOR REFERENCE CHAINS\n";
        ss << "------------------------\n\n";
    }

    std::vector<Insight> sorted = chains;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (insight.seed_labels.size() >= 3) {
            if (config.markdown_format) {
                ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
                   << " -> " << insight.seed_labels[1] << " -> " << insight.seed_labels[2] << "\n\n";
            } else {
                ss << (count + 1) << ". " << insight.seed_labels[0]
                   << " -> " << insight.seed_labels[1] << " -> " << insight.seed_labels[2] << "\n";
            }
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_author_chain(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_community_links_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Community Links\n\n";
        ss << "These candidates connect entities from different graph communities based on shared relation signatures.\n\n";
    } else {
        ss << "COMMUNITY LINKS\n";
        ss << "---------------\n\n";
    }

    std::vector<Insight> sorted = links;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (const auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " <-> " << insight.seed_labels[1] << "\n\n";
        } else if (!config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << (count + 1) << ". " << insight.seed_labels[0]
               << " <-> " << insight.seed_labels[1] << "\n";
        }

        std::string narrative = get_llm_example(insight, config);
        if (narrative.empty()) {
            narrative = describe_community_link(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_hypotheses_section(const std::vector<Insight>& hypotheses, const ReportConfig& config) {
    if (hypotheses.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Testable Hypotheses\n\n";
        ss << "These hypotheses synthesize findings across discovery methods into testable claims.\n\n";
    } else {
        ss << "TESTABLE HYPOTHESES\n";
        ss << "-------------------\n\n";
    }

    std::vector<Insight> sorted = hypotheses;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (const auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Hypothesis\n\n";
        } else {
            ss << (count + 1) << ". Hypothesis\n";
        }

        std::string narrative = get_llm_example(insight, config);
        if (narrative.empty()) {
            narrative = describe_hypothesis(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_rules_section(const std::vector<Insight>& rules, const ReportConfig& config) {
    if (rules.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Association Rules\n\n";
        ss << "Association rules reveal predictable patterns between different relation types. "
           << "These rules can be used for knowledge inference, validation, or identifying structural regularities.\n\n";
    } else {
        ss << "ASSOCIATION RULES\n";
        ss << "-----------------\n\n";
    }

    std::vector<Insight> sorted = rules;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". " << insight.description << "\n\n";
        } else {
            ss << (count + 1) << ". " << insight.description << "\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_rule(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_embedding_links_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Embedding-Based Link Predictions\n\n";
        ss << "These predictions are generated using TransE embeddings, which learn vector representations "
           << "of entities and relations such that valid relationships satisfy h + r ≈ t. "
           << "Higher plausibility scores indicate more confident predictions.\n\n";
    } else {
        ss << "EMBEDDING-BASED LINK PREDICTIONS\n";
        ss << "---------------------------------\n\n";
    }

    std::vector<Insight> sorted = links;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= config.max_examples_per_type) break;

        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            // Extract relation type
            std::string relation_type = "related_to";
            for (const auto& tag : insight.novelty_tags) {
                if (tag.find("relation=") == 0) {
                    relation_type = tag.substr(9);
                    break;
                }
            }
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " —[" << relation_type << "]→ " << insight.seed_labels[1] << "\n\n";
        } else if (!config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << (count + 1) << ". " << insight.seed_labels[0]
               << " -> " << insight.seed_labels[1] << "\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_embedding_link(insight);
        }
        ss << narrative << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_conclusions(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Conclusions and Recommendations\n\n";
    } else {
        ss << "CONCLUSIONS AND RECOMMENDATIONS\n";
        ss << "-------------------------------\n\n";
    }

    std::map<InsightType, int> counts;
    for (const auto& insight : insights.insights) {
        counts[insight.type]++;
    }

    ss << "Based on the analysis of " << insights.insights.size() << " discovered insights, "
       << "the following recommendations are made:\n\n";

    if (counts[InsightType::BRIDGE] > 0) {
        ss << "1. **Protect Bridge Entities**: The " << counts[InsightType::BRIDGE]
           << " identified bridge entities are critical for knowledge connectivity. "
           << "Consider documenting these thoroughly and ensuring they are well-maintained.\n\n";
    }

    if (counts[InsightType::COMPLETION] > 0) {
        ss << "2. **Address Knowledge Gaps**: Review the " << counts[InsightType::COMPLETION]
           << " potential completions to determine if additional relationships should be added "
           << "to create a more complete knowledge representation.\n\n";
    }

    if (counts[InsightType::SUBSTITUTION] > 0) {
        ss << "3. **Resolve Duplicates**: The " << counts[InsightType::SUBSTITUTION]
           << " potential substitutions may indicate duplicate or synonymous entities "
           << "that could be merged for consistency.\n\n";
    }

    if (counts[InsightType::SURPRISE] > 0) {
        ss << "4. **Investigate Surprises**: The " << counts[InsightType::SURPRISE]
           << " surprising connections warrant manual review to determine if they represent "
           << "genuine discoveries or potential data quality issues.\n\n";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        ss << "5. **Validate Path-Ranked Links**: The " << counts[InsightType::PATH_RANK]
           << " path-ranked links are supported by multiple short graph paths. "
           << "Prioritize high-confidence candidates for validation or targeted data collection.\n\n";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        ss << "6. **Track Citation Trails**: The " << counts[InsightType::AUTHOR_CHAIN]
           << " author reference chains reveal how scholarship propagates through citations. "
           << "Use these chains to map influence or identify key scholarly bridges.\n\n";
    }

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        ss << "7. **Review Community Links**: The " << counts[InsightType::COMMUNITY_LINK]
           << " cross-cluster links highlight structurally similar entities across communities. "
           << "Validate candidates that bridge distinct topic areas.\n\n";
    }

    if (counts[InsightType::HYPOTHESIS] > 0) {
        ss << "8. **Test Hypotheses**: The " << counts[InsightType::HYPOTHESIS]
           << " synthesized hypotheses translate graph discoveries into testable claims. "
           << "Prioritize those with strong supporting evidence.\n\n";
    }

    if (counts[InsightType::RULE] > 0) {
        ss << "9. **Leverage Association Rules**: The " << counts[InsightType::RULE]
           << " discovered rules can be used for automated knowledge inference, "
           << "consistency checking, or to guide further data collection.\n\n";
    }

    if (counts[InsightType::EMBEDDING_LINK] > 0) {
        ss << "10. **Review Embedding Predictions**: The " << counts[InsightType::EMBEDDING_LINK]
           << " TransE-based link predictions suggest plausible missing relationships. "
           << "Higher plausibility scores indicate stronger evidence for the predicted link.\n\n";
    }

    ss << "---\n\n";
    ss << "*This report was automatically generated by the Knowledge Discovery Engine. "
       << "All insights should be validated by domain experts before taking action.*\n";

    return ss.str();
}

std::string ReportGenerator::generate(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream report;

    // Group insights by type
    std::map<InsightType, std::vector<Insight>> by_type;
    for (const auto& insight : insights.insights) {
        by_type[insight.type].push_back(insight);
    }

    // Generate sections
    report << generate_header(insights, config);
    report << generate_executive_summary(insights, config);
    report << generate_statistics_section(insights, config);
    report << generate_augmentation_overview(config);
    report << generate_llm_examples_section(by_type, config);

    // Add sections for each insight type that has results
    report << generate_bridges_section(by_type[InsightType::BRIDGE], config);
    report << generate_completions_section(by_type[InsightType::COMPLETION], config);
    report << generate_motifs_section(by_type[InsightType::MOTIF], config);
    report << generate_substitutions_section(by_type[InsightType::SUBSTITUTION], config);
    report << generate_surprise_section(by_type[InsightType::SURPRISE], config);
    report << generate_diffusion_section(by_type[InsightType::DIFFUSION], config);
    report << generate_community_links_section(by_type[InsightType::COMMUNITY_LINK], config);
    report << generate_path_rank_section(by_type[InsightType::PATH_RANK], config);
    report << generate_author_chains_section(by_type[InsightType::AUTHOR_CHAIN], config);
    report << generate_hypotheses_section(by_type[InsightType::HYPOTHESIS], config);
    report << generate_rules_section(by_type[InsightType::RULE], config);
    report << generate_embedding_links_section(by_type[InsightType::EMBEDDING_LINK], config);

    // Conclusions
    report << generate_conclusions(insights, config);

    return report.str();
}

void ReportGenerator::save_to_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + path);
    }
    file << content;
    file.close();
}

std::string ReportGenerator::generate_html(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream html;

    // Group insights by type
    std::map<InsightType, std::vector<Insight>> by_type;
    for (const auto& insight : insights.insights) {
        by_type[insight.type].push_back(insight);
    }

    // Count by type
    std::map<InsightType, int> counts;
    for (const auto& insight : insights.insights) {
        counts[insight.type]++;
    }

    auto stats = graph_.compute_statistics();

    // HTML Header with styling
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << config.title << R"(</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --primary: #4fc3f7;
            --secondary: #fbbf24;
            --accent: #e879f9;
            --bg-dark: #0f172a;
            --bg-card: #1e293b;
            --text: #f8fafc;
            --text-muted: #94a3b8;
            --border: rgba(148, 163, 184, 0.2);
            --theme-surprise: #e879f9;
            --theme-gap: #fbbf24;
            --theme-rule: #818cf8;
            --theme-motif: #4ade80;
            --theme-path: #38bdf8;
            --theme-community: #f97316;
            --theme-hypothesis: #22d3ee;
            --theme-author: #facc15;
        }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Inter', system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            background: linear-gradient(135deg, var(--bg-dark) 0%, var(--bg-card) 100%);
            color: var(--text);
            line-height: 1.6;
            min-height: 100vh;
        }
        .container {
            max-width: 1000px;
            margin: 0 auto;
            padding: 40px 20px;
        }
        header {
            text-align: center;
            margin-bottom: 40px;
            padding-bottom: 30px;
            border-bottom: 1px solid var(--border);
        }
        header h1 {
            font-size: 2.5em;
            color: var(--primary);
            margin-bottom: 15px;
        }
        .meta {
            color: var(--text-muted);
            font-size: 0.9em;
        }
        .meta span { margin: 0 10px; }
        .summary-cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }
        .card {
            background: rgba(0,0,0,0.3);
            border-radius: 12px;
            padding: 20px;
            border: 1px solid var(--border);
        }
        .card-link {
            display: block;
            text-decoration: none;
            color: inherit;
            transition: transform 0.15s ease, border-color 0.15s ease;
        }
        .card-link:hover .card {
            border-color: rgba(79, 195, 247, 0.5);
            transform: translateY(-2px);
        }
        .card h3 {
            color: var(--primary);
            font-size: 0.9em;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }
        .card .value {
            font-size: 2.5em;
            font-weight: 600;
        }
        .card.bridges .value { color: var(--primary); }
        .card.surprises .value { color: var(--theme-surprise); }
        .card.completions .value { color: var(--theme-gap); }
        .card.motifs .value { color: var(--theme-motif); }
        .card.community .value { color: var(--theme-community); }
        .card.pathrank .value { color: var(--theme-path); }
        .card.rules .value { color: var(--theme-rule); }
        .card.embedding .value { color: var(--secondary); }
        .card.hypothesis .value { color: var(--theme-hypothesis); }
        .card.authorchain .value { color: var(--theme-author); }
        section {
            margin: 40px 0;
        }
        section h2 {
            color: var(--primary);
            font-size: 1.8em;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 2px solid var(--primary);
        }
        section > p {
            color: var(--text-muted);
            margin-bottom: 20px;
        }
        .insight {
            background: rgba(0,0,0,0.2);
            border-radius: 10px;
            padding: 20px;
            margin: 15px 0;
            border-left: 4px solid var(--primary);
        }
        .insight.surprise { border-left-color: var(--accent); }
        .insight.completion { border-left-color: var(--secondary); }
        .insight.motif { border-left-color: #66bb6a; }
        .insight.rule { border-left-color: #ab47bc; }
        .insight h4 {
            color: var(--primary);
            margin-bottom: 10px;
            font-size: 1.1em;
        }
        .insight.surprise h4 { color: var(--accent); }
        .insight p { margin-bottom: 10px; }
        .evidence {
            font-size: 0.85em;
            color: var(--text-muted);
            font-style: italic;
        }
        .entity {
            background: rgba(79, 195, 247, 0.2);
            color: var(--primary);
            padding: 2px 8px;
            border-radius: 4px;
            font-weight: 500;
        }
        .module {
            background: rgba(15, 23, 42, 0.65);
            border-radius: 14px;
            padding: 24px;
            margin: 30px 0;
            border: 1px solid var(--border);
            border-left-width: 4px;
        }
        .module-header h2 {
            margin-bottom: 8px;
        }
        .module-header .definition {
            color: var(--text-muted);
            margin-bottom: 6px;
        }
        .module-header .count {
            font-size: 0.9em;
            color: var(--text-muted);
        }
        .spotlight {
            background: rgba(15, 23, 42, 0.7);
            border-radius: 10px;
            padding: 16px;
            margin: 18px 0;
        }
        .spotlight h3 {
            margin-bottom: 8px;
            font-size: 1.05em;
            color: var(--text);
        }
        .spotlight .narrative {
            font-size: 1.05em;
            color: var(--text);
        }
        .stats-bar {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
            gap: 12px;
            background: rgba(15, 23, 42, 0.7);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 16px;
            margin: 10px 0 30px;
        }
        .stat {
            display: flex;
            flex-direction: column;
            gap: 4px;
        }
        .stat .label {
            font-size: 0.78em;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--text-muted);
        }
        .stat .value {
            font-size: 1.4em;
            font-weight: 600;
            color: var(--text);
        }
        .data-table {
            width: 100%;
            border-collapse: collapse;
            margin: 16px 0 8px;
        }
        .data-table th, .data-table td {
            padding: 12px 14px;
            text-align: left;
            border-bottom: 1px solid var(--border);
        }
        .data-table th {
            background: rgba(15, 23, 42, 0.6);
            color: var(--text-muted);
            font-weight: 600;
            font-size: 0.85em;
            text-transform: uppercase;
            letter-spacing: 0.6px;
        }
        .data-table td:last-child {
            text-align: right;
            font-weight: 500;
        }
        .recommendations {
            background: rgba(79, 195, 247, 0.1);
            border-radius: 12px;
            padding: 25px;
            margin: 30px 0;
        }
        .recommendations h3 {
            color: var(--primary);
            margin-bottom: 15px;
        }
        .recommendations ol {
            margin-left: 20px;
        }
        .recommendations li {
            margin: 10px 0;
        }
        .recommendations strong {
            color: var(--primary);
        }
        footer {
            text-align: center;
            padding: 30px;
            color: var(--text-muted);
            font-size: 0.85em;
            border-top: 1px solid var(--border);
            margin-top: 40px;
        }
        .toc {
            background: rgba(0,0,0,0.2);
            border-radius: 10px;
            padding: 20px;
            margin: 20px 0;
        }
        .toc h3 {
            color: var(--primary);
            margin-bottom: 15px;
        }
        .toc ul {
            list-style: none;
        }
        .toc li {
            margin: 8px 0;
        }
        .toc a {
            color: var(--text);
            text-decoration: none;
            transition: color 0.2s;
        }
        .toc a:hover {
            color: var(--primary);
        }
        .toc .count {
            color: var(--text-muted);
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>)" << config.title << R"(</h1>
            <div class="meta">
                <span>Generated: )" << get_current_timestamp() << R"(</span>
                <span>|</span>
                <span>Source: )" << (config.source_document.empty() ? "Unknown" : config.source_document) << R"(</span>
                <span>|</span>
                <span>Run ID: )" << insights.run_id << R"(</span>
            </div>
        </header>

        <section id="statistics" class="stats-bar">
            <div class="stat">
                <div class="label">Total Entities</div>
                <div class="value">)" << stats.num_nodes << R"(</div>
            </div>
            <div class="stat">
                <div class="label">Total Relationships</div>
                <div class="value">)" << stats.num_edges << R"(</div>
            </div>
            <div class="stat">
                <div class="label">Average Degree</div>
                <div class="value">)" << std::fixed << std::setprecision(2) << stats.avg_node_degree << R"(</div>
            </div>
            <div class="stat">
                <div class="label">Insights Discovered</div>
                <div class="value">)" << insights.insights.size() << R"(</div>
            </div>
        </section>

        <section>
            <h2>Executive Summary</h2>
            <p>This report presents the results of automated knowledge discovery analysis.
               A total of <strong>)" << insights.insights.size() << R"( insights</strong> were identified.</p>

            <div class="summary-cards">
)";

    // Summary cards
    if (counts[InsightType::BRIDGE] > 0) {
        html << R"(                <a class="card-link" href="#bridges">
                    <div class="card bridges">
                        <h3>Bridge Entities</h3>
                        <div class="value">)" << counts[InsightType::BRIDGE] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::SURPRISE] > 0) {
        html << R"(                <a class="card-link" href="#module-surprises">
                    <div class="card surprises">
                        <h3>Surprising Discoveries</h3>
                        <div class="value">)" << counts[InsightType::SURPRISE] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::COMPLETION] > 0) {
        html << R"(                <a class="card-link" href="#module-completions">
                    <div class="card completions">
                        <h3>Potential Completions</h3>
                        <div class="value">)" << counts[InsightType::COMPLETION] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::MOTIF] > 0) {
        html << R"(                <a class="card-link" href="#module-motifs">
                    <div class="card motifs">
                        <h3>Recurring Patterns</h3>
                        <div class="value">)" << counts[InsightType::MOTIF] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        html << R"(                <a class="card-link" href="#module-community">
                    <div class="card community">
                        <h3>Community Links</h3>
                        <div class="value">)" << counts[InsightType::COMMUNITY_LINK] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::HYPOTHESIS] > 0) {
        html << R"(                <a class="card-link" href="#module-hypotheses">
                    <div class="card hypothesis">
                        <h3>Hypotheses</h3>
                        <div class="value">)" << counts[InsightType::HYPOTHESIS] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        html << R"(                <a class="card-link" href="#path-rank">
                    <div class="card pathrank">
                        <h3>Path-Ranked Links</h3>
                        <div class="value">)" << counts[InsightType::PATH_RANK] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        html << R"(                <a class="card-link" href="#module-author-chains">
                    <div class="card authorchain">
                        <h3>Author Chains</h3>
                        <div class="value">)" << counts[InsightType::AUTHOR_CHAIN] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::RULE] > 0) {
        html << R"(                <a class="card-link" href="#module-rules">
                    <div class="card rules">
                        <h3>Association Rules</h3>
                        <div class="value">)" << counts[InsightType::RULE] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::EMBEDDING_LINK] > 0) {
        html << R"(                <a class="card-link" href="#module-embedding">
                    <div class="card embedding">
                        <h3>Embedding Predictions</h3>
                        <div class="value">)" << counts[InsightType::EMBEDDING_LINK] << R"(</div>
                    </div>
                </a>
)";
    }

    html << R"(            </div>
        </section>
)";

    // Table of Contents
    html << R"(
        <nav class="toc">
            <h3>Contents</h3>
            <ul>
                <li><a href="#statistics">Knowledge Graph Statistics</a></li>
)";

    if (counts[InsightType::BRIDGE] > 0)
        html << R"(                <li><a href="#bridges">Bridge Entities</a> <span class="count">()" << counts[InsightType::BRIDGE] << R"()</span></li>
)";
    if (counts[InsightType::SURPRISE] > 0)
        html << R"(                <li><a href="#module-surprises">Surprising Discoveries</a> <span class="count">()" << counts[InsightType::SURPRISE] << R"()</span></li>
)";
    if (counts[InsightType::COMPLETION] > 0)
        html << R"(                <li><a href="#module-completions">Knowledge Gaps</a> <span class="count">()" << counts[InsightType::COMPLETION] << R"()</span></li>
)";
    if (counts[InsightType::MOTIF] > 0)
        html << R"(                <li><a href="#module-motifs">Recurring Patterns</a> <span class="count">()" << counts[InsightType::MOTIF] << R"()</span></li>
)";
    if (counts[InsightType::COMMUNITY_LINK] > 0)
        html << R"(                <li><a href="#module-community">Community Links</a> <span class="count">()" << counts[InsightType::COMMUNITY_LINK] << R"()</span></li>
)";
    if (counts[InsightType::HYPOTHESIS] > 0)
        html << R"(                <li><a href="#module-hypotheses">Hypotheses</a> <span class="count">()" << counts[InsightType::HYPOTHESIS] << R"()</span></li>
)";
    if (counts[InsightType::RULE] > 0)
        html << R"(                <li><a href="#module-rules">Association Rules</a> <span class="count">()" << counts[InsightType::RULE] << R"()</span></li>
)";
    if (counts[InsightType::PATH_RANK] > 0)
        html << R"(                <li><a href="#path-rank">Path-Ranked Links</a> <span class="count">()" << counts[InsightType::PATH_RANK] << R"()</span></li>
)";
    if (counts[InsightType::AUTHOR_CHAIN] > 0)
        html << R"(                <li><a href="#module-author-chains">Author Reference Chains</a> <span class="count">()" << counts[InsightType::AUTHOR_CHAIN] << R"()</span></li>
)";
    if (counts[InsightType::EMBEDDING_LINK] > 0)
        html << R"(                <li><a href="#module-embedding">Embedding Predictions</a> <span class="count">()" << counts[InsightType::EMBEDDING_LINK] << R"()</span></li>
)";

    html << R"(                <li><a href="#recommendations">Conclusions &amp; Recommendations</a></li>
            </ul>
        </nav>
)";

    // No standalone statistics/augmentation sections in modular layout.

    [[maybe_unused]] auto spotlight_text = [&](std::vector<Insight>& list) -> std::string {
        if (list.empty()) return "";
        std::sort(list.begin(), list.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });
        Insight& top = list.front();
        std::string narrative;
        // Use LLM narrative if enabled, otherwise fall back to template + optional LLM example
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(top, config);
        } else {
            narrative = get_graph_context_summary(top, false);
            if (config.include_llm_examples && llm_provider_) {
                std::string llm = get_llm_example(top, config);
                if (!llm.empty()) {
                    if (!narrative.empty()) narrative += " ";
                    narrative += llm;
                }
            }
        }
        if (narrative.empty()) {
            narrative = "No spotlight insight available.";
        }
        return escape_html(strip_markdown_bold(narrative));
    };

    [[maybe_unused]] auto format_entities = [&](const std::vector<std::string>& labels, size_t max_items) -> std::string {
        std::stringstream ss;
        for (size_t i = 0; i < std::min(max_items, labels.size()); ++i) {
            if (i > 0) ss << " + ";
            ss << "<span class=\"entity\">" << escape_html(labels[i]) << "</span>";
        }
        if (labels.size() > max_items) {
            ss << " and " << (labels.size() - max_items) << " others";
        }
        return ss.str();
    };

    [[maybe_unused]] auto parse_rule = [&](const std::string& desc) {
        std::string text = desc;
        auto rule_pos = text.find("Rule:");
        if (rule_pos != std::string::npos) {
            text = text.substr(rule_pos + 5);
        }
        text = trim_copy(text);
        std::string core = text;
        std::string metrics;
        auto bracket = text.find("[");
        if (bracket != std::string::npos) {
            core = trim_copy(text.substr(0, bracket));
            metrics = text.substr(bracket + 1);
            auto end = metrics.find("]");
            if (end != std::string::npos) metrics = metrics.substr(0, end);
            metrics = trim_copy(metrics);
        }
        std::string antecedent = core;
        std::string consequent;
        auto arrow = core.find("=>");
        if (arrow != std::string::npos) {
            antecedent = trim_copy(core.substr(0, arrow));
            consequent = trim_copy(core.substr(arrow + 2));
        }
        return std::make_tuple(antecedent, consequent, metrics);
    };

    // Discovery modules
    auto& surprises = by_type[InsightType::SURPRISE];
    if (!surprises.empty()) {
        std::vector<Insight> sorted = surprises;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-surprises" class="module" style="border-left-color: var(--theme-surprise)">
            <div class="module-header">
                <h2 style="color: var(--theme-surprise)">Surprising Discoveries</h2>
                <p class="definition">Flags statistically unusual or novel connections in the graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-surprise);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Surprise Detection</strong> algorithm identifies entity pairs that co-occur in the knowledge graph more frequently than would be expected by chance. It computes an expected co-occurrence rate based on each entity's individual frequency, then flags pairs where actual co-occurrence significantly exceeds this expectation.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> These unexpected connections often reveal hidden relationships, novel insights, or potential data quality issues that merit human review. A high surprise score indicates the relationship is statistically anomalous given the overall graph structure.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
)HTML";

        // Use clustering to coalesce similar findings
        if (config.coalesce_similar_findings && sorted.size() > 3) {
            auto clusters = cluster_insights(sorted, config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= config.max_examples_per_type) break;

                std::string summary = generate_cluster_summary(cluster, config);
                cluster_num++;

                html << R"HTML(            <div class="cluster" style="margin: 15px 0; padding: 15px; background: rgba(0,0,0,0.2); border-radius: 8px;">
                <h4 style="color: var(--theme-surprise); margin-bottom: 10px;">)HTML";

                if (cluster.insights.size() > 1) {
                    html << "Discovery Group " << cluster_num << " (" << cluster.insights.size() << " similar findings)";
                } else {
                    html << "Discovery: " << escape_html(cluster.common_theme);
                }
                html << R"HTML(</h4>
                <p style="margin-bottom: 15px; color: var(--text-muted);">)HTML" << escape_html(summary) << R"HTML(</p>
                <table class="data-table" style="font-size: 0.9em;">
                    <thead>
                        <tr><th>Entities</th><th>Score</th><th>Source</th></tr>
                    </thead>
                    <tbody>
)HTML";

                int items_in_cluster = 0;
                for (const auto& insight : cluster.insights) {
                    if (items_in_cluster >= config.max_items_per_cluster) {
                        html << R"(                        <tr><td colspan="3" style="text-align: center; font-style: italic;">... and )"
                             << (cluster.insights.size() - items_in_cluster) << R"( more similar items</td></tr>
)";
                        break;
                    }
                    std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];

                    html << R"(                        <tr>
                            <td>)" << format_entities(insight.seed_labels, 3) << R"(</td>
                            <td>)" << std::fixed << std::setprecision(2) << insight.score << R"(</td>
                            <td>)" << escape_html(source) << R"(</td>
                        </tr>
)";
                    items_in_cluster++;
                    total_shown++;
                }

                html << R"HTML(                    </tbody>
                </table>
            </div>
)HTML";
            }
        } else {
            // Non-clustered view (original behavior)
            bool include_explanation = (config.use_llm_narratives || config.include_llm_examples) && llm_provider_;
            html << R"HTML(            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Score</th><th>Source</th>)HTML";
            if (include_explanation) {
                html << R"HTML(<th>Explanation</th>)HTML";
            }
            html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
            int count = 0;
            for (auto& insight : sorted) {
                if (count >= config.max_examples_per_type) break;
                std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];
                std::string explanation;
                if (include_explanation) {
                    if (config.use_llm_narratives) {
                        explanation = generate_llm_narrative(insight, config);
                    } else {
                        explanation = get_llm_example(insight, config);
                        if (explanation.empty()) {
                            explanation = get_graph_context_summary(insight, false);
                        }
                    }
                }
                html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 3) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << insight.score << R"(</td>
                        <td>)" << escape_html(source) << R"(</td>)";
                if (include_explanation) {
                    html << R"(
                        <td>)" << escape_html(explanation) << R"(</td>)";
                }
                html << R"(
                    </tr>
)";
                count++;
            }
            html << R"HTML(                </tbody>
            </table>
)HTML";
        }
        html << R"HTML(        </section>
)HTML";
    }

    auto& completions = by_type[InsightType::COMPLETION];
    if (!completions.empty()) {
        std::vector<Insight> sorted = completions;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-completions" class="module" style="border-left-color: var(--theme-gap)">
            <div class="module-header">
                <h2 style="color: var(--theme-gap)">Knowledge Gaps (Completions)</h2>
                <p class="definition">Suggests missing relations based on similar neighborhood patterns.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-gap);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Completion Detection</strong> algorithm identifies potential missing relationships by analyzing neighborhood patterns. When two entities share multiple common neighbors or appear in similar hyperedge contexts but lack a direct connection, the algorithm suggests they may be related. This is based on the principle that entities with similar structural positions in a knowledge graph often share semantic relationships.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> These suggestions highlight gaps where relationships likely exist but were not explicitly captured during extraction. Reviewing completions can improve knowledge graph coverage and reveal implicit connections in the source documents.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
)HTML";

        // Use clustering to coalesce similar findings
        if (config.coalesce_similar_findings && sorted.size() > 3) {
            auto clusters = cluster_insights(sorted, config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= config.max_examples_per_type) break;

                std::string summary = generate_cluster_summary(cluster, config);
                cluster_num++;

                html << R"HTML(            <div class="cluster" style="margin: 15px 0; padding: 15px; background: rgba(0,0,0,0.2); border-radius: 8px;">
                <h4 style="color: var(--theme-gap); margin-bottom: 10px;">)HTML";

                if (cluster.insights.size() > 1) {
                    html << "Gap Group " << cluster_num << " (" << cluster.insights.size() << " similar findings)";
                } else {
                    html << "Gap: " << escape_html(cluster.common_theme);
                }
                html << R"HTML(</h4>
                <p style="margin-bottom: 15px; color: var(--text-muted);">)HTML" << escape_html(summary) << R"HTML(</p>
                <table class="data-table" style="font-size: 0.9em;">
                    <thead>
                        <tr><th>Gap (X + Y)</th><th>Confidence</th></tr>
                    </thead>
                    <tbody>
)HTML";

                int items_in_cluster = 0;
                for (const auto& insight : cluster.insights) {
                    if (items_in_cluster >= config.max_items_per_cluster) {
                        html << R"(                        <tr><td colspan="2" style="text-align: center; font-style: italic;">... and )"
                             << (cluster.insights.size() - items_in_cluster) << R"( more similar items</td></tr>
)";
                        break;
                    }
                    if (insight.seed_labels.size() < 2) continue;

                    html << R"(                        <tr>
                            <td>)" << format_entities({insight.seed_labels[0], insight.seed_labels[1]}, 2) << R"(</td>
                            <td>)" << escape_html(format_confidence(insight.score)) << R"(</td>
                        </tr>
)";
                    items_in_cluster++;
                    total_shown++;
                }

                html << R"HTML(                    </tbody>
                </table>
            </div>
)HTML";
            }
        } else {
            // Non-clustered view (original behavior)
            bool include_explanation = (config.use_llm_narratives || config.include_llm_examples) && llm_provider_;
            html << R"HTML(            <table class="data-table">
                <thead>
                    <tr><th>Gap (X + Y)</th><th>Confidence</th>)HTML";
            if (include_explanation) {
                html << R"HTML(<th>Explanation</th>)HTML";
            }
            html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
            int count = 0;
            for (auto& insight : sorted) {
                if (count >= config.max_examples_per_type) break;
                if (insight.seed_labels.size() < 2) continue;
                std::string explanation;
                if (include_explanation) {
                    if (config.use_llm_narratives) {
                        explanation = generate_llm_narrative(insight, config);
                    } else {
                        explanation = get_llm_example(insight, config);
                        if (explanation.empty()) {
                            explanation = get_graph_context_summary(insight, false);
                        }
                    }
                }
                html << R"(                    <tr>
                        <td>)" << format_entities({insight.seed_labels[0], insight.seed_labels[1]}, 2) << R"(</td>
                        <td>)" << escape_html(format_confidence(insight.score)) << R"(</td>)";
                if (include_explanation) {
                    html << R"(
                        <td>)" << escape_html(explanation) << R"(</td>)";
                }
                html << R"(
                    </tr>
)";
                count++;
            }
            html << R"HTML(                </tbody>
            </table>
)HTML";
        }
        html << R"HTML(        </section>
)HTML";
    }

    auto& motifs = by_type[InsightType::MOTIF];
    if (!motifs.empty()) {
        std::vector<Insight> sorted = motifs;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-motifs" class="module" style="border-left-color: var(--theme-motif)">
            <div class="module-header">
                <h2 style="color: var(--theme-motif)">Recurring Patterns (Motifs)</h2>
                <p class="definition">Highlights recurring subgraph structures that indicate repeated concepts.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-motif);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Motif Mining</strong> algorithm discovers sets of entities that frequently co-occur together across multiple hyperedges. Using association mining principles, it computes a <em>lift</em> score that measures how much more often entities appear together than would be expected if they were independent. High-lift motifs represent genuine recurring patterns rather than coincidental co-occurrences.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Motifs reveal the conceptual building blocks of your knowledge domain. Recurring entity groups often represent established concepts, common workflows, or domain-specific patterns that provide structural insight into the underlying subject matter.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
)HTML";

        // Use clustering to coalesce similar findings
        if (config.coalesce_similar_findings && sorted.size() > 3) {
            auto clusters = cluster_insights(sorted, config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= config.max_examples_per_type) break;

                // Generate cluster summary
                std::string summary = generate_cluster_summary(cluster, config);
                cluster_num++;

                html << R"HTML(            <div class="cluster" style="margin: 15px 0; padding: 15px; background: rgba(0,0,0,0.2); border-radius: 8px;">
                <h4 style="color: var(--theme-motif); margin-bottom: 10px;">)HTML";

                if (cluster.insights.size() > 1) {
                    html << "Pattern Group " << cluster_num << " (" << cluster.insights.size() << " similar findings)";
                } else {
                    html << "Pattern: " << escape_html(cluster.common_theme);
                }
                html << R"HTML(</h4>
                <p style="margin-bottom: 15px; color: var(--text-muted);">)HTML" << escape_html(summary) << R"HTML(</p>
                <table class="data-table" style="font-size: 0.9em;">
                    <thead>
                        <tr><th>Entities</th><th>Support</th><th>Lift</th></tr>
                    </thead>
                    <tbody>
)HTML";

                int items_in_cluster = 0;
                for (const auto& insight : cluster.insights) {
                    if (items_in_cluster >= config.max_items_per_cluster) {
                        html << R"(                        <tr><td colspan="3" style="text-align: center; font-style: italic;">... and )"
                             << (cluster.insights.size() - items_in_cluster) << R"( more similar items</td></tr>
)";
                        break;
                    }
                    double support = 0.0;
                    double lift = 0.0;
                    auto sit = insight.score_breakdown.find("support");
                    if (sit != insight.score_breakdown.end()) support = sit->second;
                    auto lit = insight.score_breakdown.find("novelty");
                    if (lit != insight.score_breakdown.end()) lift = lit->second;

                    html << R"(                        <tr>
                            <td>)" << format_entities(insight.seed_labels, 4) << R"(</td>
                            <td>)" << std::fixed << std::setprecision(0) << support << R"(</td>
                            <td>)" << std::fixed << std::setprecision(2) << lift << R"(</td>
                        </tr>
)";
                    items_in_cluster++;
                    total_shown++;
                }

                html << R"HTML(                    </tbody>
                </table>
            </div>
)HTML";
            }
        } else {
            // Non-clustered view (original behavior)
            bool include_explanation = (config.use_llm_narratives || config.include_llm_examples) && llm_provider_;
            html << R"HTML(            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Support</th><th>Lift</th>)HTML";
            if (include_explanation) {
                html << R"HTML(<th>Explanation</th>)HTML";
            }
            html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
            int count = 0;
            for (auto& insight : sorted) {
                if (count >= config.max_examples_per_type) break;
                double support = 0.0;
                double lift = 0.0;
                auto sit = insight.score_breakdown.find("support");
                if (sit != insight.score_breakdown.end()) support = sit->second;
                auto lit = insight.score_breakdown.find("novelty");
                if (lit != insight.score_breakdown.end()) lift = lit->second;
                std::string explanation;
                if (include_explanation) {
                    if (config.use_llm_narratives) {
                        explanation = generate_llm_narrative(insight, config);
                    } else {
                        explanation = get_llm_example(insight, config);
                        if (explanation.empty()) {
                            explanation = get_graph_context_summary(insight, false);
                        }
                    }
                }
                html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 4) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(0) << support << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << lift << R"(</td>)";
                if (include_explanation) {
                    html << R"(
                        <td>)" << escape_html(explanation) << R"(</td>)";
                }
                html << R"(
                    </tr>
)";
                count++;
            }
            html << R"HTML(                </tbody>
            </table>
)HTML";
        }
        html << R"HTML(        </section>
)HTML";
    }

    auto& community_links = by_type[InsightType::COMMUNITY_LINK];
    if (!community_links.empty()) {
        std::vector<Insight> sorted = community_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-community" class="module" style="border-left-color: var(--theme-community)">
            <div class="module-header">
                <h2 style="color: var(--theme-community)">Community Links</h2>
                <p class="definition">Proposes cross-cluster links based on shared relation signatures.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-community);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Community Link Detection</strong> algorithm first identifies densely connected communities (clusters) in the hypergraph using s-connectivity analysis. It then finds candidate pairs of entities from <em>different</em> communities that share similar "relation signatures" - meaning they participate in similar types of relationships. The Jaccard overlap of their relation types determines similarity.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> These cross-cluster candidates suggest potential bridges between distinct topic areas or knowledge domains. They often represent interdisciplinary connections, analogies across fields, or integration points that could unify separate parts of the knowledge graph.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Overlap</th><th>Evidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= config.max_examples_per_type) break;
            double overlap = 0.0;
            auto oit = insight.score_breakdown.find("novelty");
            if (oit != insight.score_breakdown.end()) overlap = oit->second;
            std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];
            std::string explanation;
            if (include_llm) {
                explanation = get_llm_example(insight, config);
                if (explanation.empty()) {
                    explanation = get_graph_context_summary(insight, false);
                }
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 2) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << overlap << R"HTML(</td>
                        <td>)HTML" << escape_html(source) << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(explanation) << R"HTML(</td>)HTML";
            }
            html << R"HTML(
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& hypotheses = by_type[InsightType::HYPOTHESIS];
    if (!hypotheses.empty()) {
        std::vector<Insight> sorted = hypotheses;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-hypotheses" class="module" style="border-left-color: var(--theme-hypothesis)">
            <div class="module-header">
                <h2 style="color: var(--theme-hypothesis)">Testable Hypotheses</h2>
                <p class="definition">Synthesizes findings into testable claims grounded in the augmented graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="spotlight">
                <h3>Top Hypothesis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Hypothesis</th><th>Evidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= config.max_examples_per_type) break;
            std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];
            std::string explanation;
            if (include_llm) {
                explanation = get_llm_example(insight, config);
                if (explanation.empty()) {
                    explanation = get_graph_context_summary(insight, false);
                }
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_hypothesis(insight))) << R"HTML(</td>
                        <td>)HTML" << escape_html(source) << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(explanation) << R"HTML(</td>)HTML";
            }
            html << R"HTML(
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& rules = by_type[InsightType::RULE];
    if (!rules.empty()) {
        std::vector<Insight> sorted = rules;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-rules" class="module" style="border-left-color: var(--theme-rule)">
            <div class="module-header">
                <h2 style="color: var(--theme-rule)">Association Rules</h2>
                <p class="definition">Mines association rules between relation types.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-rule);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Association Rule Mining</strong> algorithm discovers "if-then" patterns in the knowledge graph. It identifies cases where the presence of certain relation types (the antecedent) reliably predicts other relation types (the consequent). Rules are scored by <em>support</em> (how often the pattern occurs), <em>confidence</em> (P(consequent|antecedent)), and <em>lift</em> (how much better than random chance).</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> These rules encode predictable patterns that can be used for automated inference, data validation, or identifying incomplete records. High-lift rules reveal genuine domain regularities rather than coincidental correlations.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
)HTML";

        // Use clustering to coalesce similar findings
        if (config.coalesce_similar_findings && sorted.size() > 3) {
            auto clusters = cluster_insights(sorted, config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= config.max_examples_per_type) break;

                std::string summary = generate_cluster_summary(cluster, config);
                cluster_num++;

                html << R"HTML(            <div class="cluster" style="margin: 15px 0; padding: 15px; background: rgba(0,0,0,0.2); border-radius: 8px;">
                <h4 style="color: var(--theme-rule); margin-bottom: 10px;">)HTML";

                if (cluster.insights.size() > 1) {
                    html << "Rule Group " << cluster_num << " (" << cluster.insights.size() << " similar rules)";
                } else {
                    html << "Rule: " << escape_html(cluster.common_theme);
                }
                html << R"HTML(</h4>
                <p style="margin-bottom: 15px; color: var(--text-muted);">)HTML" << escape_html(summary) << R"HTML(</p>
                <table class="data-table" style="font-size: 0.9em;">
                    <thead>
                        <tr><th>Antecedent</th><th>Consequent</th><th>Lift / Confidence</th><th>Examples</th></tr>
                    </thead>
                    <tbody>
)HTML";

                int items_in_cluster = 0;
                for (const auto& insight : cluster.insights) {
                    if (items_in_cluster >= config.max_items_per_cluster) {
                        html << R"(                        <tr><td colspan="4" style="text-align: center; font-style: italic;">... and )"
                             << (cluster.insights.size() - items_in_cluster) << R"( more similar rules</td></tr>
)";
                        break;
                    }
                    auto [antecedent, consequent, metrics] = parse_rule(insight.description);
                    double confidence = 0.0;
                    double lift = 0.0;
                    auto cit = insight.score_breakdown.find("specificity");
                    if (cit != insight.score_breakdown.end()) confidence = cit->second;
                    auto lit = insight.score_breakdown.find("novelty");
                    if (lit != insight.score_breakdown.end()) lift = lit->second;
                    std::stringstream metric_ss;
                    if (confidence > 0.0 || lift > 0.0) {
                        metric_ss << "lift " << std::fixed << std::setprecision(2) << lift
                                  << ", conf " << std::fixed << std::setprecision(2) << confidence;
                    } else if (!metrics.empty()) {
                        metric_ss << metrics;
                    } else {
                        metric_ss << "-";
                    }

                    html << R"(                        <tr>
                            <td>)" << escape_html(antecedent) << R"(</td>
                            <td>)" << escape_html(consequent) << R"(</td>
                            <td>)" << escape_html(metric_ss.str()) << R"(</td>
                            <td>)" << format_entities(insight.seed_labels, 3) << R"(</td>
                        </tr>
)";
                    items_in_cluster++;
                    total_shown++;
                }

                html << R"HTML(                    </tbody>
                </table>
            </div>
)HTML";
            }
        } else {
            // Non-clustered view (original behavior)
            bool include_explanation = (config.use_llm_narratives || config.include_llm_examples) && llm_provider_;
            html << R"HTML(            <table class="data-table">
                <thead>
                    <tr><th>Antecedent</th><th>Consequent</th><th>Lift / Confidence</th><th>Examples</th>)HTML";
            if (include_explanation) {
                html << R"HTML(<th>Explanation</th>)HTML";
            }
            html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
            int count = 0;
            for (auto& insight : sorted) {
                if (count >= config.max_examples_per_type) break;
                auto [antecedent, consequent, metrics] = parse_rule(insight.description);
                double confidence = 0.0;
                double lift = 0.0;
                auto cit = insight.score_breakdown.find("specificity");
                if (cit != insight.score_breakdown.end()) confidence = cit->second;
                auto lit = insight.score_breakdown.find("novelty");
                if (lit != insight.score_breakdown.end()) lift = lit->second;
                std::string explanation;
                if (include_explanation) {
                    if (config.use_llm_narratives) {
                        explanation = generate_llm_narrative(insight, config);
                    } else {
                        explanation = get_llm_example(insight, config);
                        if (explanation.empty()) {
                            explanation = get_graph_context_summary(insight, false);
                        }
                    }
                }
                std::stringstream metric_ss;
                if (confidence > 0.0 || lift > 0.0) {
                    metric_ss << "lift " << std::fixed << std::setprecision(2) << lift
                              << ", conf " << std::fixed << std::setprecision(2) << confidence;
                } else if (!metrics.empty()) {
                    metric_ss << metrics;
                } else {
                    metric_ss << "-";
                }

                html << R"(                    <tr>
                        <td>)" << escape_html(antecedent) << R"(</td>
                        <td>)" << escape_html(consequent) << R"(</td>
                        <td>)" << escape_html(metric_ss.str()) << R"(</td>
                        <td>)" << format_entities(insight.seed_labels, 3) << R"(</td>)";
                if (include_explanation) {
                    html << R"(
                        <td>)" << escape_html(explanation) << R"(</td>)";
                }
                html << R"(
                    </tr>
)";
                count++;
            }
            html << R"HTML(                </tbody>
            </table>
)HTML";
        }
        html << R"HTML(        </section>
)HTML";
    }

    // Bridge entities section
    auto& bridges = by_type[InsightType::BRIDGE];
    if (!bridges.empty()) {
        std::sort(bridges.begin(), bridges.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"(
        <section id="bridges">
            <h2>Bridge Entities</h2>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-bridge);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Bridge Detection</strong> algorithm identifies entities whose removal would disconnect or significantly fragment the knowledge graph. Using s-connectivity analysis on the hypergraph structure, it finds nodes that serve as critical links between otherwise separate clusters or communities. The bridge score reflects how essential the entity is for maintaining graph connectivity.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Bridge entities are often key concepts, pivotal figures, or central ideas that tie together different areas of knowledge. They represent critical junctions for information flow and should be documented thoroughly. Losing or misrepresenting a bridge entity could fragment understanding of the domain.</p>
            </div>
)";

        int count = 0;
        for (auto& insight : bridges) {
            if (count >= config.max_examples_per_type) break;

            std::string entity = insight.seed_labels.empty() ? "Unknown" : insight.seed_labels[0];

            std::string narrative;
            if (config.use_llm_narratives && llm_provider_) {
                narrative = generate_llm_narrative(insight, config);
            } else {
                narrative = describe_bridge(insight);
            }
            html << R"(
            <div class="insight">
                <h4>)" << (count + 1) << ". " << entity << R"(</h4>
                <p>)" << escape_html(strip_markdown_bold(narrative)) << R"(</p>
)";

            if (config.include_evidence && !insight.evidence_chunk_ids.empty()) {
                html << R"(                <p class="evidence">Evidence from: )" << insight.evidence_chunk_ids[0];
                if (insight.evidence_chunk_ids.size() > 1) {
                    html << " and " << (insight.evidence_chunk_ids.size() - 1) << " other sources";
                }
                html << R"(</p>
)";
            }

            html << R"(            </div>
)";
            count++;
        }

        html << R"(        </section>
)";
    }

    // Path rank section
    auto& path_ranks = by_type[InsightType::PATH_RANK];
    if (!path_ranks.empty()) {
        std::sort(path_ranks.begin(), path_ranks.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"(
        <section id="path-rank">
            <h2>Path-Ranked Links</h2>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-pathrank);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Path Ranking</strong> algorithm identifies entity pairs that are connected by multiple short paths through the graph but lack a direct relationship. It computes path scores based on the number and length of connecting paths, with shorter and more numerous paths indicating stronger potential relationships. The algorithm uses k-shortest-path search with configurable maximum hop distance.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Path-ranked links represent relationships strongly implied by the graph structure. When two entities share many neighbors or are consistently a few hops apart, they likely have a meaningful connection that wasn't explicitly captured. These are high-confidence candidates for knowledge graph completion.</p>
            </div>
)";

        int count = 0;
        for (auto& insight : path_ranks) {
            if (count >= config.max_examples_per_type) break;
            if (insight.seed_labels.size() < 2) continue;

            std::string narrative;
            if (config.use_llm_narratives && llm_provider_) {
                narrative = generate_llm_narrative(insight, config);
            } else {
                narrative = describe_path_rank(insight);
            }
            html << R"(
            <div class="insight pathrank">
                <h4>)" << (count + 1) << ". " << insight.seed_labels[0] << " <-> " << insight.seed_labels[1] << R"(</h4>
                <p>)" << escape_html(strip_markdown_bold(narrative)) << R"(</p>
            </div>
)";
            count++;
        }

        html << R"(        </section>
)";
    }

    // Author reference chains section
    auto& author_chains = by_type[InsightType::AUTHOR_CHAIN];
    if (!author_chains.empty()) {
        std::sort(author_chains.begin(), author_chains.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-author-chains" class="module" style="border-left-color: var(--theme-author)">
            <div class="module-header">
                <h2 style="color: var(--theme-author)">Author Reference Chains</h2>
                <p class="definition">Tracks citation chains across authors using reference relations in the graph.</p>
                <div class="count">Total: )HTML" << author_chains.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-author);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Author Chain</strong> operator scans citation-like relations (e.g., cites, references) and builds directed chains that link authors through references. Chains of length two (A -> B -> C) indicate a sequence where one author cites another who cites a third.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> These chains reveal how influence and scholarship propagate across authors, highlighting citation paths that may indicate schools of thought or intellectual lineage.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : author_chains) {
            if (count >= config.max_examples_per_type) break;
            if (insight.seed_labels.size() < 3) continue;

            std::string narrative;
            if (config.use_llm_narratives && llm_provider_) {
                narrative = generate_llm_narrative(insight, config);
            } else {
                narrative = describe_author_chain(insight);
            }
            html << R"HTML(
            <div class="insight authorchain">
                <h4>)HTML" << (count + 1) << ". " << insight.seed_labels[0] << " -> " << insight.seed_labels[1]
                 << " -> " << insight.seed_labels[2] << R"HTML(</h4>
                <p>)HTML" << escape_html(strip_markdown_bold(narrative)) << R"HTML(</p>
            </div>
)HTML";
            count++;
        }

        html << R"HTML(        </section>
)HTML";
    }

    // Embedding link predictions section
    auto& embedding_links = by_type[InsightType::EMBEDDING_LINK];
    if (!embedding_links.empty()) {
        std::vector<Insight> sorted = embedding_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-embedding" class="module" style="border-left-color: var(--theme-pathrank)">
            <div class="module-header">
                <h2 style="color: var(--theme-pathrank)">Embedding-Based Link Predictions</h2>
                <p class="definition">Uses TransE neural embeddings to predict plausible missing relationships based on learned vector representations.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-pathrank);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>TransE Embedding</strong> algorithm learns dense vector representations (embeddings) for all entities and relations in the knowledge graph. The model is trained on the principle that <code>head + relation ≈ tail</code> for valid triples. After training, the algorithm scores candidate triples by measuring how well they satisfy this translation property - lower distance means higher plausibility.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Embedding-based predictions leverage the entire graph structure to identify plausible missing links. Unlike path-based methods, embeddings can capture latent semantic patterns and long-range dependencies. High plausibility scores indicate relationships that fit naturally with learned patterns in the knowledge graph.</p>
            </div>
            <div class="spotlight">
                <h3>Top Prediction Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
)HTML";

        // Use clustering to coalesce similar findings
        if (config.coalesce_similar_findings && sorted.size() > 3) {
            auto clusters = cluster_insights(sorted, config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= config.max_examples_per_type) break;

                std::string summary = generate_cluster_summary(cluster, config);
                cluster_num++;

                html << R"HTML(            <div class="cluster" style="margin: 15px 0; padding: 15px; background: rgba(0,0,0,0.2); border-radius: 8px;">
                <h4 style="color: var(--theme-pathrank); margin-bottom: 10px;">)HTML";

                if (cluster.insights.size() > 1) {
                    html << "Prediction Group " << cluster_num << " (" << cluster.insights.size() << " similar predictions)";
                } else {
                    html << "Prediction: " << escape_html(cluster.common_theme);
                }
                html << R"HTML(</h4>
                <p style="margin-bottom: 15px; color: var(--text-muted);">)HTML" << escape_html(summary) << R"HTML(</p>
                <table class="data-table" style="font-size: 0.9em;">
                    <thead>
                        <tr><th>Head Entity</th><th>Relation</th><th>Tail Entity</th><th>Plausibility</th></tr>
                    </thead>
                    <tbody>
)HTML";

                int items_in_cluster = 0;
                for (const auto& insight : cluster.insights) {
                    if (items_in_cluster >= config.max_items_per_cluster) {
                        html << R"(                        <tr><td colspan="4" style="text-align: center; font-style: italic;">... and )"
                             << (cluster.insights.size() - items_in_cluster) << R"( more similar predictions</td></tr>
)";
                        break;
                    }
                    if (insight.seed_labels.size() < 2) continue;

                    // Extract relation type
                    std::string relation = "related_to";
                    for (const auto& tag : insight.novelty_tags) {
                        if (tag.find("relation=") == 0) {
                            relation = tag.substr(9);
                            break;
                        }
                    }

                    double plausibility = 0.0;
                    auto it = insight.score_breakdown.find("novelty");
                    if (it != insight.score_breakdown.end()) plausibility = it->second;

                    html << R"(                        <tr>
                            <td>)" << escape_html(insight.seed_labels[0]) << R"(</td>
                            <td>)" << escape_html(relation) << R"(</td>
                            <td>)" << escape_html(insight.seed_labels[1]) << R"(</td>
                            <td>)" << std::fixed << std::setprecision(3) << plausibility << R"(</td>
                        </tr>
)";
                    items_in_cluster++;
                    total_shown++;
                }

                html << R"HTML(                    </tbody>
                </table>
            </div>
)HTML";
            }
        } else {
            // Non-clustered view
            html << R"HTML(            <table class="data-table">
                <thead>
                    <tr><th>Head Entity</th><th>Relation</th><th>Tail Entity</th><th>Plausibility</th><th>Explanation</th></tr>
                </thead>
                <tbody>
)HTML";
            int count = 0;
            for (auto& insight : sorted) {
                if (count >= config.max_examples_per_type) break;
                if (insight.seed_labels.size() < 2) continue;

                std::string relation = "related_to";
                for (const auto& tag : insight.novelty_tags) {
                    if (tag.find("relation=") == 0) {
                        relation = tag.substr(9);
                        break;
                    }
                }

                double plausibility = 0.0;
                auto it = insight.score_breakdown.find("novelty");
                if (it != insight.score_breakdown.end()) plausibility = it->second;

                std::string explanation;
                if (config.use_llm_narratives && llm_provider_) {
                    explanation = generate_llm_narrative(insight, config);
                } else {
                    explanation = describe_embedding_link(insight);
                }

                html << R"(                    <tr>
                        <td>)" << escape_html(insight.seed_labels[0]) << R"(</td>
                        <td>)" << escape_html(relation) << R"(</td>
                        <td>)" << escape_html(insight.seed_labels[1]) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(3) << plausibility << R"(</td>
                        <td>)" << escape_html(strip_markdown_bold(explanation)) << R"(</td>
                    </tr>
)";
                count++;
            }
            html << R"HTML(                </tbody>
            </table>
)HTML";
        }
        html << R"HTML(        </section>
)HTML";
    }

    // Recommendations
    html << R"(
        <section id="recommendations">
            <h2>Conclusions &amp; Recommendations</h2>
            <div class="recommendations">
                <h3>Based on the analysis of )" << insights.insights.size() << R"( discovered insights:</h3>
                <ol>
)";

    if (counts[InsightType::BRIDGE] > 0) {
        html << R"(                    <li><strong>Protect Bridge Entities:</strong> The )" << counts[InsightType::BRIDGE] << R"( identified bridge entities are critical for knowledge connectivity. Consider documenting these thoroughly.</li>
)";
    }

    if (counts[InsightType::COMPLETION] > 0) {
        html << R"(                    <li><strong>Address Knowledge Gaps:</strong> Review the )" << counts[InsightType::COMPLETION] << R"( potential completions to determine if additional relationships should be added.</li>
)";
    }

    if (counts[InsightType::SUBSTITUTION] > 0) {
        html << R"(                    <li><strong>Resolve Duplicates:</strong> The )" << counts[InsightType::SUBSTITUTION] << R"( potential substitutions may indicate duplicate or synonymous entities that could be merged.</li>
)";
    }

    if (counts[InsightType::SURPRISE] > 0) {
        html << R"(                    <li><strong>Investigate Surprises:</strong> The )" << counts[InsightType::SURPRISE] << R"( surprising connections warrant manual review to determine if they represent genuine discoveries.</li>
)";
    }

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        html << R"(                    <li><strong>Review Community Links:</strong> The )" << counts[InsightType::COMMUNITY_LINK] << R"( cross-cluster links highlight structurally similar entities across communities. Validate candidates that bridge distinct topic areas.</li>
)";
    }

    if (counts[InsightType::HYPOTHESIS] > 0) {
        html << R"(                    <li><strong>Test Hypotheses:</strong> The )" << counts[InsightType::HYPOTHESIS] << R"( synthesized hypotheses translate graph discoveries into testable claims. Prioritize those with strong supporting evidence.</li>
)";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        html << R"(                    <li><strong>Validate Path-Ranked Links:</strong> The )" << counts[InsightType::PATH_RANK] << R"( path-ranked links are supported by multiple short graph paths. Prioritize high-confidence candidates for validation.</li>
)";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        html << R"(                    <li><strong>Track Citation Trails:</strong> The )" << counts[InsightType::AUTHOR_CHAIN] << R"( author reference chains reveal citation pathways across authors. Use them to map scholarly influence or identify key bridges.</li>
)";
    }

    if (counts[InsightType::RULE] > 0) {
        html << R"(                    <li><strong>Leverage Association Rules:</strong> The )" << counts[InsightType::RULE] << R"( discovered rules can be used for automated knowledge inference, consistency checking, or guiding further data collection.</li>
)";
    }

    if (counts[InsightType::EMBEDDING_LINK] > 0) {
        html << R"(                    <li><strong>Review Embedding Predictions:</strong> The )" << counts[InsightType::EMBEDDING_LINK] << R"( TransE-based link predictions suggest plausible missing relationships. Higher plausibility scores indicate stronger evidence for the predicted link.</li>
)";
    }

    html << R"(                </ol>
            </div>
        </section>

        <footer>
            <p>This report was automatically generated by the Knowledge Discovery Engine.<br>
            All insights should be validated by domain experts before taking action.</p>
        </footer>
    </div>
</body>
</html>
)";

    return html.str();
}

// ============================================================================
// Clustering Implementation for Coalescing Similar Findings
// ============================================================================

double ReportGenerator::calculate_insight_similarity(const Insight& a, const Insight& b) const {
    // Similarity based on:
    // 1. Shared seed entities (Jaccard similarity)
    // 2. Score similarity (closer scores = more similar)
    // 3. Shared witness entities

    // Calculate Jaccard similarity for seed labels
    std::set<std::string> a_seeds(a.seed_labels.begin(), a.seed_labels.end());
    std::set<std::string> b_seeds(b.seed_labels.begin(), b.seed_labels.end());

    std::set<std::string> intersection;
    std::set_intersection(a_seeds.begin(), a_seeds.end(),
                          b_seeds.begin(), b_seeds.end(),
                          std::inserter(intersection, intersection.begin()));

    std::set<std::string> union_set;
    std::set_union(a_seeds.begin(), a_seeds.end(),
                   b_seeds.begin(), b_seeds.end(),
                   std::inserter(union_set, union_set.begin()));

    double seed_jaccard = union_set.empty() ? 0.0 : static_cast<double>(intersection.size()) / union_set.size();

    // Score similarity (inverse of absolute difference, normalized)
    double score_diff = std::abs(a.score - b.score);
    double score_sim = 1.0 - std::min(score_diff, 1.0);

    // Check for similar score breakdowns (e.g., both have similar lift/support)
    double breakdown_sim = 0.0;
    int common_keys = 0;
    for (const auto& [key, val_a] : a.score_breakdown) {
        auto it = b.score_breakdown.find(key);
        if (it != b.score_breakdown.end()) {
            double diff = std::abs(val_a - it->second);
            double max_val = std::max(std::abs(val_a), std::abs(it->second));
            if (max_val > 0) {
                breakdown_sim += 1.0 - std::min(diff / max_val, 1.0);
            } else {
                breakdown_sim += 1.0;
            }
            common_keys++;
        }
    }
    if (common_keys > 0) {
        breakdown_sim /= common_keys;
    }

    // Weighted combination
    // If entities are shared, that's most important
    // Otherwise, score similarity matters more
    double similarity = 0.0;
    if (seed_jaccard > 0) {
        similarity = 0.5 * seed_jaccard + 0.3 * score_sim + 0.2 * breakdown_sim;
    } else {
        // No shared entities - rely more on score patterns
        similarity = 0.6 * score_sim + 0.4 * breakdown_sim;
    }

    return similarity;
}

std::vector<InsightCluster> ReportGenerator::cluster_insights(
    const std::vector<Insight>& insights,
    const ReportConfig& config
) const {
    std::vector<InsightCluster> clusters;
    if (insights.empty()) return clusters;

    // Simple greedy clustering
    std::vector<bool> assigned(insights.size(), false);

    for (size_t i = 0; i < insights.size(); ++i) {
        if (assigned[i]) continue;

        InsightCluster cluster;
        cluster.insights.push_back(insights[i]);
        assigned[i] = true;

        // Find all similar insights
        for (size_t j = i + 1; j < insights.size(); ++j) {
            if (assigned[j]) continue;

            double sim = calculate_insight_similarity(insights[i], insights[j]);
            if (sim >= config.similarity_threshold) {
                cluster.insights.push_back(insights[j]);
                assigned[j] = true;
            }
        }

        // Calculate average score for the cluster
        double total_score = 0.0;
        for (const auto& ins : cluster.insights) {
            total_score += ins.score;
        }
        cluster.avg_score = total_score / cluster.insights.size();

        clusters.push_back(std::move(cluster));
    }

    // Sort clusters by size (largest first) then by average score
    std::sort(clusters.begin(), clusters.end(), [](const InsightCluster& a, const InsightCluster& b) {
        if (a.insights.size() != b.insights.size()) {
            return a.insights.size() > b.insights.size();
        }
        return a.avg_score > b.avg_score;
    });

    return clusters;
}

std::string ReportGenerator::identify_cluster_theme(const InsightCluster& cluster) const {
    if (cluster.insights.empty()) return "Unknown theme";

    // Collect all entity labels from the cluster
    std::map<std::string, int> entity_freq;
    for (const auto& insight : cluster.insights) {
        for (const auto& label : insight.seed_labels) {
            entity_freq[label]++;
        }
    }

    // Find the most common entities
    std::vector<std::pair<std::string, int>> sorted_entities(entity_freq.begin(), entity_freq.end());
    std::sort(sorted_entities.begin(), sorted_entities.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Build theme description
    std::stringstream theme;
    if (cluster.insights.size() == 1) {
        // Single item cluster - use its labels
        const auto& labels = cluster.insights[0].seed_labels;
        if (!labels.empty()) {
            theme << labels[0];
            if (labels.size() > 1) {
                theme << " and " << labels[1];
            }
        }
    } else if (!sorted_entities.empty()) {
        // Multi-item cluster - use common entities
        theme << sorted_entities[0].first;
        if (sorted_entities.size() > 1 && sorted_entities[1].second > 1) {
            theme << " and related entities";
        }

        // Check for score pattern commonality
        bool all_low_confidence = true;
        bool all_high_confidence = true;
        for (const auto& insight : cluster.insights) {
            if (insight.score > 0.5) all_low_confidence = false;
            if (insight.score < 0.7) all_high_confidence = false;
        }

        if (all_low_confidence) {
            theme << " (low confidence patterns)";
        } else if (all_high_confidence) {
            theme << " (high confidence patterns)";
        }
    }

    return theme.str();
}

std::string ReportGenerator::generate_cluster_summary(InsightCluster& cluster, const ReportConfig& config) {
    if (cluster.insights.empty()) return "";

    // Identify the theme first
    cluster.common_theme = identify_cluster_theme(cluster);

    // If only one item, just describe it
    if (cluster.insights.size() == 1) {
        if (config.use_llm_narratives && llm_provider_) {
            return generate_llm_narrative(cluster.insights[0], config);
        }
        return get_graph_context_summary(cluster.insights[0], false);
    }

    // For multi-item clusters, generate a summary
    std::string cache_key = "cluster_summary:";
    for (const auto& ins : cluster.insights) {
        cache_key += ins.insight_id + ";";
    }

    auto it = llm_example_cache_.find(cache_key);
    if (it != llm_example_cache_.end()) {
        return it->second;
    }

    // If LLM is available, generate a summary for the cluster
    if (config.use_llm_narratives && llm_provider_) {
        std::stringstream prompt;
        prompt << "You are summarizing a group of " << cluster.insights.size()
               << " similar findings from a knowledge graph analysis. "
               << "Instead of describing each one individually, provide a concise summary "
               << "that captures the common pattern.\n\n";

        prompt << "## Common Theme: " << cluster.common_theme << "\n\n";
        prompt << "## Average Confidence: " << std::fixed << std::setprecision(2)
               << cluster.avg_score << " (" << format_confidence(cluster.avg_score) << ")\n\n";

        prompt << "## Entity Pairs in this Group:\n";
        int shown = 0;
        for (const auto& ins : cluster.insights) {
            if (shown >= 5) {
                prompt << "... and " << (cluster.insights.size() - shown) << " more similar findings\n";
                break;
            }
            if (!ins.seed_labels.empty()) {
                prompt << "- ";
                for (size_t i = 0; i < ins.seed_labels.size() && i < 3; ++i) {
                    if (i > 0) prompt << " + ";
                    prompt << ins.seed_labels[i];
                }
                prompt << " (score: " << std::fixed << std::setprecision(2) << ins.score << ")\n";
            }
            shown++;
        }

        prompt << "\n## Your Task\n"
               << "Write 2-3 sentences that:\n"
               << "1. Summarize what this group of findings represents\n"
               << "2. Explain what they have in common\n"
               << "3. Note the overall confidence level\n\n"
               << "Be concise. Return plain text only, no markdown.";

        std::vector<Message> messages = {
            Message(Message::Role::System,
                "You are summarizing grouped knowledge graph findings. Be concise and informative."),
            Message(Message::Role::User, prompt.str())
        };

        LLMResponse response = llm_provider_->chat(messages);
        if (response.success) {
            llm_example_cache_[cache_key] = response.content;
            return response.content;
        }
    }

    // Fallback: generate a template-based summary
    std::stringstream summary;
    summary << "This group contains " << cluster.insights.size() << " related findings";

    if (!cluster.common_theme.empty()) {
        summary << " involving " << cluster.common_theme;
    }

    summary << ". The average confidence is " << format_confidence(cluster.avg_score)
            << " (" << std::fixed << std::setprecision(2) << cluster.avg_score << ").";

    // List representative entities
    std::set<std::string> all_entities;
    for (const auto& ins : cluster.insights) {
        for (const auto& label : ins.seed_labels) {
            all_entities.insert(label);
        }
    }

    if (all_entities.size() <= 6) {
        summary << " Entities involved: ";
        bool first = true;
        for (const auto& e : all_entities) {
            if (!first) summary << ", ";
            summary << e;
            first = false;
        }
        summary << ".";
    } else {
        summary << " Involves " << all_entities.size() << " distinct entities.";
    }

    return summary.str();
}

} // namespace kg

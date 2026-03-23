#include "discovery/report_generator.hpp"
#include "llm/llm_provider.hpp"
#include "llm/causal_metadata.hpp"  // Phase 2: Causal metadata support
#include <fstream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <set>

namespace kg {

ReportGenerator::ReportGenerator(const Hypergraph& graph)
    : graph_(graph) {}

namespace {
int capped_max_examples(const ReportConfig& config) {
    return std::max(1, std::min(10, config.max_examples_per_type));
}
}  // namespace

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
        case InsightType::CONTRADICTION: return "Contradiction";
        case InsightType::ENTITY_RESOLUTION: return "Entity Resolution";
        case InsightType::CORE_PERIPHERY: return "Core-Periphery";
        case InsightType::TEXT_SIMILARITY: return "Text Similarity";
        case InsightType::ARGUMENT_SUPPORT: return "Argument Support";
        case InsightType::ACTIVE_LEARNING: return "Active Learning";
        case InsightType::METHOD_OUTCOME: return "Method/Outcome";
        case InsightType::CENTRALITY: return "Centrality";
        case InsightType::COMMUNITY_DETECTION: return "Community Detection";
        case InsightType::K_CORE: return "k-Core";
        case InsightType::K_TRUSS: return "k-Truss";
        case InsightType::CLAIM_STANCE: return "Claim Stance";
        case InsightType::RELATION_INDUCTION: return "Relation Induction";
        case InsightType::ANALOGICAL_TRANSFER: return "Analogical Transfer";
        case InsightType::UNCERTAINTY_SAMPLING: return "Uncertainty Sampling";
        case InsightType::COUNTERFACTUAL: return "Counterfactual";
        case InsightType::HYPEREDGE_PREDICTION: return "Hyperedge Prediction";
        case InsightType::DIFFUSION: return "Diffusion";
        case InsightType::SURPRISE: return "Surprise";
        case InsightType::COMMUNITY_LINK: return "Community Link";
        case InsightType::PATH_RANK: return "Path Rank";
        case InsightType::LONG_CHAIN: return "Long Chain";
        case InsightType::META_PATH_PATTERN: return "Meta-Path Pattern";
        case InsightType::HYPOTHESES_1: return "Hypothesis (H1)";
        case InsightType::HYPOTHESES_2: return "Mechanistic Chain (H2)";
        case InsightType::HYPOTHESES_3: return "Causal Hypothesis (H3)";
        case InsightType::MECHANISM_CONSOLIDATION: return "Mechanism Consolidation";
        case InsightType::EVIDENCE_FUSION_LINK: return "Evidence Fusion Link";
        case InsightType::META_PATH_ANOMALY: return "Meta-Path Anomaly";
        case InsightType::INTERVENTION_BOTTLENECK: return "Intervention Bottleneck";
        case InsightType::COMPETING_MECHANISM: return "Competing Mechanism";
        case InsightType::SCHEMA_REPAIR: return "Schema Repair";
        case InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE: return "Cross-Community Mechanism Bridge";
        case InsightType::RULE: return "Rule";
        case InsightType::EMBEDDING_LINK: return "Embedding Link";
        case InsightType::AUTHOR_CHAIN: return "Author Chain";
        case InsightType::CO_AUTHORSHIP: return "Co-Authorship";
        case InsightType::CITATION_IMPACT: return "Citation Impact";
        case InsightType::MULTI_RESOLUTION_COMMUNITY: return "Multi-Resolution Community";
        case InsightType::CROSS_COMMUNITY_BRIDGE_MAP: return "Cross-Community Bridge Map";
        case InsightType::META_PATTERN: return "Meta-Pattern";
        case InsightType::META_PATH: return "Meta-Path Link";
        case InsightType::CAUSAL_CHAIN: return "Causal Chain";
        case InsightType::INTERVENTION_POINT: return "Intervention Point";
        case InsightType::FEEDBACK_LOOP: return "Feedback Loop";
        case InsightType::CONFOUNDER: return "Confounder";
        case InsightType::TAXONOMY: return "Taxonomy";
        case InsightType::DOMAIN_BRIDGE: return "Domain Bridge";
        case InsightType::LOGICAL_ENTAILMENT: return "Logical Entailment";
        case InsightType::COMPOSITIONAL_REASONING: return "Compositional Reasoning";
        case InsightType::EXPLANATORY_CHAIN: return "Explanatory Chain";
        case InsightType::SCHEMA_VIOLATION: return "Schema Violation";
        case InsightType::TRANSITIVE_CLOSURE: return "Transitive Closure";
        case InsightType::BIAS_AUDIT: return "Bias Audit";
        case InsightType::COMMUNITY_RECOMMENDATION: return "Community Recommendation";
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
        case InsightType::CONTRADICTION:
            prompt << "A contradiction highlights a conflict where the graph contains both affirmed and negated "
                   << "claims about the same relationship, warranting manual review.\n\n";
            break;
        case InsightType::ENTITY_RESOLUTION:
            prompt << "Entity resolution findings suggest that two entities are likely duplicates or aliases "
                   << "based on similar labels and graph context.\n\n";
            break;
        case InsightType::CORE_PERIPHERY:
            prompt << "Core-periphery analysis highlights entities that form the structural backbone of the graph "
                   << "versus those that sit on the periphery. Hub/authority scores indicate how often a node "
                   << "acts as a source or target within relations.\n\n";
            break;
        case InsightType::TEXT_SIMILARITY:
            prompt << "Text similarity links connect entities with highly similar labels or descriptions, "
                   << "suggesting related concepts or near-duplicate naming.\n\n";
            break;
        case InsightType::ARGUMENT_SUPPORT:
            prompt << "Argument-supported relations are candidate links grounded in evidence paths through the graph, "
                   << "indicating a plausible relationship supported by intermediate entities and relations.\n\n";
            break;
        case InsightType::ACTIVE_LEARNING:
            prompt << "Active learning queries highlight uncertain or high-impact relations that should be verified "
                   << "to improve graph quality.\n\n";
            break;
        case InsightType::METHOD_OUTCOME:
            prompt << "Method/outcome findings classify entities as methods or outcomes based on their labels and "
                   << "context in the graph.\n\n";
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
        case InsightType::LONG_CHAIN:
            prompt << "Long-chain reasoning links trace multi-hop pathways that connect distant entities. "
                   << "They highlight extended mechanisms or thematic connections supported by sequences of relations.\n\n";
            break;
        case InsightType::EVIDENCE_FUSION_LINK:
            prompt << "An evidence fusion link combines independent signals from multiple discovery operators "
                   << "to propose a high-confidence relationship with explicit provenance.\n\n";
            break;
        case InsightType::META_PATH_ANOMALY:
            prompt << "A meta-path anomaly is a rare but plausible relation-sequence pattern. "
                   << "It highlights structurally unusual pathways that still have strong local support.\n\n";
            break;
        case InsightType::INTERVENTION_BOTTLENECK:
            prompt << "An intervention bottleneck identifies nodes that appear in many causal pathways, "
                   << "making them strategic leverage points for changing downstream outcomes.\n\n";
            break;
        case InsightType::COMPETING_MECHANISM:
            prompt << "A competing mechanism finding surfaces alternative explanatory pathways for the same outcome, "
                   << "helping distinguish rival hypotheses that need targeted tests.\n\n";
            break;
        case InsightType::SCHEMA_REPAIR:
            prompt << "A schema repair is an actionable graph edit proposal derived from closure gaps, "
                   << "schema violations, or completion evidence to improve consistency.\n\n";
            break;
        case InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE:
            prompt << "A cross-community mechanism bridge links distant communities/domains via mechanistic paths, "
                   << "revealing interdisciplinary pathways in the graph.\n\n";
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
        case InsightType::CAUSAL_CHAIN:
            prompt << "A causal chain represents a directed sequence of cause-effect relationships, "
                   << "showing how influence propagates through the knowledge graph.\n\n";
            break;
        case InsightType::INTERVENTION_POINT:
            prompt << "An intervention point is a critical node whose removal would break causal pathways, "
                   << "making it a strategic point for influencing outcomes.\n\n";
            break;
        case InsightType::FEEDBACK_LOOP:
            prompt << "A feedback loop represents a cycle in causal relationships, creating self-reinforcing "
                   << "or self-regulating dynamics in the system.\n\n";
            break;
        case InsightType::CONFOUNDER:
            prompt << "A confounder is a node connected to both ends of a suspected causal relationship, "
                   << "suggesting it may be a common cause that needs to be controlled for.\n\n";
            break;
        case InsightType::TAXONOMY:
            prompt << "A taxonomy insight reveals hierarchical relationships through is-a or part-of relations, "
                   << "organizing concepts into a structured classification.\n\n";
            break;
        case InsightType::DOMAIN_BRIDGE:
            prompt << "A domain bridge is a node that connects multiple separate domains or disciplines, "
                   << "enabling cross-domain knowledge transfer and interdisciplinary insights.\n\n";
            break;
        case InsightType::LOGICAL_ENTAILMENT:
            prompt << "A logical entailment represents an inferred implication (if X and Y, then Z), "
                   << "derived from logical reasoning over the knowledge graph.\n\n";
            break;
        case InsightType::COMPOSITIONAL_REASONING:
            prompt << "Compositional reasoning combines multiple relations through relation algebra, "
                   << "discovering new relationships by composing existing ones.\n\n";
            break;
        case InsightType::EXPLANATORY_CHAIN:
            prompt << "An explanatory chain provides a multi-hop path connecting distant concepts, "
                   << "offering an explanation for how seemingly unrelated ideas are connected.\n\n";
            break;
        case InsightType::SCHEMA_VIOLATION:
            prompt << "A schema violation occurs when entities of a particular type are missing expected relations "
                   << "that other entities of the same type typically have, suggesting incomplete knowledge.\n\n";
            break;
        case InsightType::TRANSITIVE_CLOSURE:
            prompt << "A transitive closure gap identifies a missing link implied by transitivity: "
                   << "if A relates to B and B relates to C, then A should relate to C.\n\n";
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

std::string ReportGenerator::describe_contradiction(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.empty()) return "";

    ss << "A contradiction was detected involving ";
    for (size_t i = 0; i < std::min(size_t(3), insight.seed_labels.size()); ++i) {
        if (i > 0) ss << (i == std::min(size_t(3), insight.seed_labels.size()) - 1 ? " and " : ", ");
        ss << "**" << insight.seed_labels[i] << "**";
    }
    ss << ". ";

    if (!insight.description.empty()) {
        ss << insight.description << " ";
    }

    ss << "These conflicting claims should be reviewed to resolve ambiguity or extraction errors.";

    return ss.str();
}

std::string ReportGenerator::describe_entity_resolution(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    ss << "The entities **" << insight.seed_labels[0] << "** and **" << insight.seed_labels[1]
       << "** appear to be duplicate or alias entries. ";

    auto it_label = insight.score_breakdown.find("label_similarity");
    auto it_neighbor = insight.score_breakdown.find("neighbor_overlap");
    if (it_label != insight.score_breakdown.end()) {
        ss << "Label similarity is " << std::fixed << std::setprecision(2) << it_label->second << ", ";
    }
    if (it_neighbor != insight.score_breakdown.end()) {
        ss << "neighbor overlap is " << std::fixed << std::setprecision(2) << it_neighbor->second << ". ";
    }

    ss << "Merging or linking these entities could reduce duplication and improve graph consistency.";

    return ss.str();
}

std::string ReportGenerator::describe_core_periphery(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.empty()) return "";

    std::string role = "core";
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "periphery") {
            role = "periphery";
            break;
        }
    }

    ss << "The entity **" << insight.seed_labels[0] << "** sits in the " << role
       << " of the graph structure. ";

    auto it_deg = insight.score_breakdown.find("degree_norm");
    auto it_hub = insight.score_breakdown.find("hub_score");
    auto it_auth = insight.score_breakdown.find("authority_score");
    auto it_int = insight.score_breakdown.find("integration_score");
    if (it_deg != insight.score_breakdown.end()) {
        ss << "Degree centrality: " << std::fixed << std::setprecision(2) << it_deg->second << ". ";
    }
    if (it_hub != insight.score_breakdown.end()) {
        ss << "Hub score: " << std::fixed << std::setprecision(2) << it_hub->second << ". ";
    }
    if (it_auth != insight.score_breakdown.end()) {
        ss << "Authority score: " << std::fixed << std::setprecision(2) << it_auth->second << ". ";
    }
    if (it_int != insight.score_breakdown.end()) {
        ss << "Core integration: " << std::fixed << std::setprecision(2) << it_int->second << ". ";
    }

    ss << "These signals summarize how central this entity is to the knowledge graph.";

    return ss.str();
}

std::string ReportGenerator::describe_text_similarity(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    ss << "The labels **" << insight.seed_labels[0] << "** and **" << insight.seed_labels[1]
       << "** are textually similar based on token overlap and TF-IDF weighting. ";

    auto it = insight.score_breakdown.find("text_similarity");
    if (it != insight.score_breakdown.end()) {
        ss << "Cosine similarity: " << std::fixed << std::setprecision(2) << it->second << ". ";
    }

    ss << "These entities may represent closely related concepts, variants, or aliases.";
    return ss.str();
}

std::string ReportGenerator::describe_argument_support(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    ss << "A candidate relation is supported by an evidence path between **"
       << insight.seed_labels[0] << "** and **" << insight.seed_labels[1] << "**. ";
    if (!insight.description.empty()) {
        ss << insight.description;
    }
    return ss.str();
}

std::string ReportGenerator::describe_active_learning(const Insight& insight) const {
    std::stringstream ss;
    if (!insight.description.empty()) {
        ss << "Validation query: " << insight.description;
    } else if (!insight.seed_labels.empty()) {
        ss << "Validate relation involving " << format_entity_list(insight.seed_labels, 4) << ".";
    }
    return ss.str();
}

std::string ReportGenerator::describe_method_outcome(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    std::string role = "method/outcome";
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "method") role = "method";
        if (tag == "outcome") role = "outcome";
    }

    ss << "The entity **" << insight.seed_labels[0] << "** is classified as a " << role << ". ";
    if (!insight.description.empty()) {
        ss << insight.description;
    }
    return ss.str();
}

std::string ReportGenerator::describe_centrality(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";
    ss << "The entity **" << insight.seed_labels[0] << "** is structurally central in the bipartite projection. ";
    auto it = insight.score_breakdown.find("centrality");
    if (it != insight.score_breakdown.end()) {
        ss << "Centrality score: " << std::fixed << std::setprecision(2) << it->second << ". ";
    }
    ss << "Highly central entities often connect many relationship contexts.";
    return ss.str();
}

std::string ReportGenerator::describe_community_detection(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";
    ss << "A dense community is centered around " << format_entity_list(insight.seed_labels, 3) << ". ";
    auto it = insight.score_breakdown.find("size");
    if (it != insight.score_breakdown.end()) {
        ss << "Relative size score: " << std::fixed << std::setprecision(2) << it->second << ". ";
    }
    ss << "These nodes likely share thematic or structural overlap.";
    return ss.str();
}

std::string ReportGenerator::describe_k_core(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";
    ss << "The entity **" << insight.seed_labels[0] << "** appears in a dense k-core. ";
    auto it = insight.score_breakdown.find("core");
    if (it != insight.score_breakdown.end()) {
        ss << "Core number: " << std::fixed << std::setprecision(0) << it->second << ". ";
    }
    ss << "High k-core nodes participate in tightly connected regions.";
    return ss.str();
}

std::string ReportGenerator::describe_k_truss(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "The pair **" << insight.seed_labels[0] << "** – **" << insight.seed_labels[1]
       << "** lies in a dense k-truss subgraph. ";
    auto it = insight.score_breakdown.find("support");
    if (it != insight.score_breakdown.end()) {
        ss << "Triangle support: " << std::fixed << std::setprecision(0) << it->second << ". ";
    }
    ss << "Truss edges are reinforced by shared neighbors.";
    return ss.str();
}

std::string ReportGenerator::describe_claim_stance(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    std::string stance = "neutral";
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "supports" || tag == "opposes" || tag == "neutral") {
            stance = tag;
            break;
        }
    }
    ss << "Claim stance is **" << stance << "** for **" << insight.seed_labels[0]
       << "** → **" << insight.seed_labels[1] << "**. ";
    if (!insight.description.empty()) {
        ss << insight.description;
    }
    return ss.str();
}

std::string ReportGenerator::describe_relation_induction(const Insight& insight) const {
    std::stringstream ss;
    ss << "Relation type induction suggests: " << insight.description;
    return ss.str();
}

std::string ReportGenerator::describe_analogical_transfer(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "Analogical transfer proposes a relation between **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1] << "** based on similar relational patterns. ";
    if (!insight.description.empty()) {
        ss << insight.description;
    }
    return ss.str();
}

std::string ReportGenerator::describe_uncertainty_sampling(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    double uncertainty = 0.0;
    auto it = insight.score_breakdown.find("uncertainty");
    if (it != insight.score_breakdown.end()) uncertainty = it->second;
    ss << "Low-confidence relation between **" << insight.seed_labels[0] << "** and **"
       << insight.seed_labels[1] << "**. ";
    ss << "Uncertainty score: " << std::fixed << std::setprecision(2) << uncertainty << ".";
    return ss.str();
}

std::string ReportGenerator::describe_counterfactual(const Insight& insight) const {
    std::stringstream ss;
    if (!insight.description.empty()) {
        ss << insight.description;
    } else {
        ss << "Counterfactual probe for relation involving " << format_entity_list(insight.seed_labels, 2) << ".";
    }
    return ss.str();
}

std::string ReportGenerator::describe_hyperedge_prediction(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "Predicted relation between **" << insight.seed_labels[0] << "** and **"
       << insight.seed_labels[1] << "** based on similar target sets. ";
    auto it = insight.score_breakdown.find("overlap");
    if (it != insight.score_breakdown.end()) {
        ss << "Overlap score: " << std::fixed << std::setprecision(2) << it->second << ".";
    }
    return ss.str();
}

std::string ReportGenerator::describe_constrained_rule(const Insight& insight) const {
    std::stringstream ss;
    ss << insight.description;
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

std::string ReportGenerator::describe_long_chain(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    int hops = 0;
    auto hop_it = insight.score_breakdown.find("hops");
    if (hop_it != insight.score_breakdown.end()) {
        hops = static_cast<int>(std::round(hop_it->second));
    }
    if (hops <= 0) {
        for (const auto& tag : insight.novelty_tags) {
            if (tag.rfind("hops=", 0) == 0) {
                try {
                    hops = std::stoi(tag.substr(5));
                } catch (...) {
                    hops = 0;
                }
                break;
            }
        }
    }

    ss << "A long-chain reasoning path links **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1] << "**";
    if (hops > 0) {
        ss << " across " << hops << " hops";
    }
    ss << ", revealing a distant but structurally coherent connection. ";

    if (!insight.description.empty()) {
        ss << insight.description << ". ";
    }

    if (!insight.witness_edges.empty()) {
        ss << "This chain is supported by " << insight.witness_edges.size()
           << " evidence edges ";
    }

    ss << "(confidence: " << format_confidence(insight.score) << ").";

    return ss.str();
}

std::string ReportGenerator::describe_meta_path(const Insight& insight) const {
    std::stringstream ss;

    if (insight.seed_labels.size() < 2) return "";

    std::string pattern;
    std::string path_count;
    for (const auto& tag : insight.novelty_tags) {
        if (tag.find("pattern=") == 0) {
            pattern = tag.substr(std::string("pattern=").size());
        } else if (tag.find("paths=") == 0) {
            path_count = tag.substr(std::string("paths=").size());
        }
    }

    ss << "Meta-path reasoning links **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1] << "**";
    if (!pattern.empty()) {
        ss << " via the relation pattern **" << pattern << "**";
    }
    ss << ". ";

    if (!path_count.empty()) {
        ss << "This pattern is supported by " << path_count << " path instances. ";
    }

    if (!insight.description.empty()) {
        ss << insight.description << ". ";
    }

    if (!insight.witness_edges.empty()) {
        ss << "Supporting evidence from " << insight.witness_edges.size() << " graph edges";
    }

    return ss.str();
}

std::string ReportGenerator::describe_meta_path_pattern(const Insight& insight) const {
    std::stringstream ss;

    std::string pattern;
    for (const auto& tag : insight.novelty_tags) {
        if (tag.rfind("pattern=", 0) == 0) {
            pattern = tag.substr(std::string("pattern=").size());
            break;
        }
    }

    double support = 0.0;
    auto sup_it = insight.score_breakdown.find("support_count");
    if (sup_it != insight.score_breakdown.end()) support = sup_it->second;

    double avg_conf = 0.0;
    auto conf_it = insight.score_breakdown.find("avg_confidence");
    if (conf_it != insight.score_breakdown.end()) avg_conf = conf_it->second;

    if (!insight.description.empty()) {
        ss << insight.description << " ";
    } else {
        ss << "A frequent meta-path pattern";
        if (!pattern.empty()) {
            ss << " **" << pattern << "**";
        }
        if (support > 0) {
            ss << " appears across " << static_cast<int>(support) << " distinct entity pairs";
        }
        ss << ". ";
        if (avg_conf > 0.0) {
            ss << "Average path confidence is " << std::fixed << std::setprecision(2) << avg_conf << ". ";
        }
    }

    if (!insight.witness_nodes.empty()) {
        ss << "Example chain: ";
        for (size_t i = 0; i < std::min<size_t>(insight.witness_nodes.size(), 4); ++i) {
            if (i > 0) ss << " → ";
            ss << get_node_label(insight.witness_nodes[i]);
        }
        ss << ".";
    }

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

std::string ReportGenerator::describe_mechanism_consolidation(const Insight& insight) const {
    std::stringstream ss;

    ss << "Mechanism consolidation groups related mechanistic findings into a shared explanation cluster. ";
    if (!insight.description.empty()) {
        ss << insight.description << " ";
    }

    auto size_it = insight.score_breakdown.find("cluster_size");
    if (size_it != insight.score_breakdown.end()) {
        ss << "Cluster size: " << static_cast<int>(size_it->second) << " insights. ";
    }

    auto cohesion_it = insight.score_breakdown.find("cohesion");
    if (cohesion_it != insight.score_breakdown.end()) {
        ss << "Cohesion score: " << std::fixed << std::setprecision(2) << cohesion_it->second << ". ";
    }

    if (!insight.witness_nodes.empty()) {
        ss << "Core nodes include ";
        for (size_t i = 0; i < std::min<size_t>(insight.witness_nodes.size(), 4); ++i) {
            if (i > 0) ss << ", ";
            ss << get_node_label(insight.witness_nodes[i]);
        }
        ss << ".";
    }

    return ss.str();
}

std::string ReportGenerator::describe_evidence_fusion(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    ss << "Evidence fusion proposes a link between **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1]
       << "** by combining multiple independent operators. ";

    auto src_it = insight.score_breakdown.find("source_count");
    if (src_it != insight.score_breakdown.end()) {
        ss << "It is supported by " << static_cast<int>(std::round(src_it->second))
           << " operator signals. ";
    }

    auto cons_it = insight.score_breakdown.find("consensus");
    if (cons_it != insight.score_breakdown.end()) {
        ss << "Consensus strength is " << std::fixed << std::setprecision(2)
           << cons_it->second << ". ";
    }

    ss << "This is a high-priority candidate for validation due to convergent evidence.";
    return ss.str();
}

std::string ReportGenerator::describe_meta_path_anomaly(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    ss << "A rare relation-sequence pattern links **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1] << "**. ";

    auto an_it = insight.score_breakdown.find("anomaly");
    auto pl_it = insight.score_breakdown.find("plausibility");
    if (an_it != insight.score_breakdown.end()) {
        ss << "Anomaly score: " << std::fixed << std::setprecision(2) << an_it->second << ". ";
    }
    if (pl_it != insight.score_breakdown.end()) {
        ss << "Plausibility: " << std::fixed << std::setprecision(2) << pl_it->second << ". ";
    }
    ss << "These anomalies are useful for novel but interpretable discovery.";
    return ss.str();
}

std::string ReportGenerator::describe_intervention_bottleneck(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    ss << "**" << insight.seed_labels[0] << "** is identified as an intervention bottleneck ";
    auto cov_it = insight.score_breakdown.find("coverage");
    auto ch_it = insight.score_breakdown.find("affected_chains");
    if (ch_it != insight.score_breakdown.end()) {
        ss << "appearing in " << static_cast<int>(std::round(ch_it->second)) << " causal chains";
    } else {
        ss << "across causal pathways";
    }
    if (cov_it != insight.score_breakdown.end()) {
        ss << " (coverage " << std::fixed << std::setprecision(2) << cov_it->second << ")";
    }
    ss << ". Targeting this node can affect multiple downstream trajectories.";
    return ss.str();
}

std::string ReportGenerator::describe_competing_mechanism(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 3) return "";

    ss << "Competing mechanisms suggest **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1]
       << "** as alternative explanations for outcome **" << insight.seed_labels[2] << "**. ";

    auto div_it = insight.score_breakdown.find("divergence");
    auto sup_it = insight.score_breakdown.find("support");
    if (div_it != insight.score_breakdown.end()) {
        ss << "Mechanism divergence is " << std::fixed << std::setprecision(2)
           << div_it->second << ". ";
    }
    if (sup_it != insight.score_breakdown.end()) {
        ss << "Average support is " << std::fixed << std::setprecision(2)
           << sup_it->second << ". ";
    }
    ss << "Use this to prioritize discriminative tests.";
    return ss.str();
}

std::string ReportGenerator::describe_schema_repair(const Insight& insight) const {
    std::stringstream ss;
    if (!insight.description.empty()) {
        ss << insight.description;
        return ss.str();
    }
    if (insight.seed_labels.size() >= 2) {
        ss << "Schema repair suggests adding or validating a relation between **"
           << insight.seed_labels[0] << "** and **" << insight.seed_labels[1] << "**.";
    }
    return ss.str();
}

std::string ReportGenerator::describe_cross_community_bridge(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    ss << "A cross-community mechanism bridge connects **" << insight.seed_labels[0]
       << "** and **" << insight.seed_labels[1]
       << "** through intermediate bridge nodes, linking distant graph communities. ";

    auto nov_it = insight.score_breakdown.find("novelty");
    auto coh_it = insight.score_breakdown.find("coherence");
    if (nov_it != insight.score_breakdown.end()) {
        ss << "Novelty: " << std::fixed << std::setprecision(2) << nov_it->second << ". ";
    }
    if (coh_it != insight.score_breakdown.end()) {
        ss << "Coherence: " << std::fixed << std::setprecision(2) << coh_it->second << ". ";
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

std::string ReportGenerator::describe_causal_chain(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    // Phase 2: Check if witness edges have causal metadata
    bool has_causal_metadata = false;
    const HyperEdge* causal_edge = nullptr;

    if (!insight.witness_edges.empty()) {
        // Try to find an edge with causal metadata
        for (const auto& edge_id : insight.witness_edges) {
            const auto* edge = graph_.get_hyperedge(edge_id);
            if (edge && edge->is_causal()) {
                causal_edge = edge;
                has_causal_metadata = true;
                break;
            }
        }
    }

    if (has_causal_metadata && causal_edge) {
        const auto& causal = *causal_edge->causal_metadata;

        // Build enhanced description with causal metadata
        ss << "A **" << causal.get_strength_string() << "** causal chain: ";
        ss << "**" << insight.seed_labels[0] << "**";

        // Show causal type
        std::string type_str = causal.get_type_string();
        if (type_str == "direct_cause") {
            ss << " directly causes";
        } else if (type_str == "necessary") {
            ss << " is necessary for";
        } else if (type_str == "sufficient") {
            ss << " is sufficient for";
        } else if (type_str == "contributing") {
            ss << " contributes to";
        } else if (type_str == "preventing") {
            ss << " prevents";
        } else if (type_str == "enabling") {
            ss << " enables";
        } else {
            ss << " causes";
        }

        if (insight.seed_labels.size() > 1) {
            ss << " **" << insight.seed_labels[1] << "**";
        }

        // Show mechanism if available
        if (!causal.mechanism_description.empty()) {
            ss << " through " << causal.mechanism_description;
            if (!causal.mechanism_type.empty()) {
                ss << " (" << causal.mechanism_type << " mechanism)";
            }
        }

        // Show temporality
        ss << ". This operates over the **" << causal.get_temporality_string() << "**";

        // Show temporal context if available
        if (!causal.temporal_context.empty()) {
            ss << " (" << causal.temporal_context << ")";
        }

        // Show evidence sources if available
        if (!causal.evidence_sources.empty()) {
            ss << ", with evidence from " << causal.evidence_sources[0];
            if (causal.evidence_sources.size() > 1) {
                ss << " and " << (causal.evidence_sources.size() - 1) << " other source(s)";
            }
        }

        // Show confidence
        ss << " (confidence: " << std::fixed << std::setprecision(2) << causal.confidence << ").";

        // Show mechanism chain if available
        if (!causal.mechanism_chain.empty() && causal.mechanism_chain.size() > 1) {
            ss << " Mechanism chain: ";
            for (size_t i = 0; i < causal.mechanism_chain.size(); ++i) {
                if (i > 0) ss << " → ";
                ss << causal.mechanism_chain[i];
            }
            ss << ".";
        }

        return ss.str();
    }

    // Fallback to generic description if no causal metadata
    ss << "A causal chain involving **" << insight.seed_labels[0] << "**";
    if (insight.seed_labels.size() > 1) ss << " and **" << insight.seed_labels[1] << "**";
    ss << " suggests a directional flow of influence through the graph.";
    return ss.str();
}

std::string ReportGenerator::describe_intervention_point(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    // Phase 2: Check for causal metadata in witness edges
    int strong_links = 0;
    int necessary_links = 0;
    bool has_mechanism = false;

    for (const auto& edge_id : insight.witness_edges) {
        const auto* edge = graph_.get_hyperedge(edge_id);
        if (edge && edge->is_causal()) {
            const auto& causal = *edge->causal_metadata;
            if (causal.strength >= CausalStrength::STRONG) {
                strong_links++;
            }
            if (causal.type == CausalRelationType::NECESSARY) {
                necessary_links++;
            }
            if (!causal.mechanism_description.empty()) {
                has_mechanism = true;
            }
        }
    }

    ss << "Node **" << insight.seed_labels[0] << "** acts as a critical intervention point";

    // Phase 2: Add detail about causal links if available
    if (necessary_links > 0) {
        ss << " with **" << necessary_links << " necessary causal link(s)**";
    }
    if (strong_links > 0) {
        ss << " (" << strong_links << " strong/deterministic link" << (strong_links > 1 ? "s" : "") << ")";
    }

    ss << ", whose removal would disconnect causal pathways";

    if (has_mechanism) {
        ss << " and disrupt mechanistic processes";
    }

    ss << ".";

    return ss.str();
}

std::string ReportGenerator::describe_feedback_loop(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    // Phase 2: Analyze causal metadata in the loop
    double avg_strength = 0.0;
    int edge_count = 0;
    bool has_immediate = false;
    bool has_delayed = false;
    std::string dominant_mechanism_type;

    std::map<std::string, int> mechanism_types;
    for (const auto& edge_id : insight.witness_edges) {
        const auto* edge = graph_.get_hyperedge(edge_id);
        if (edge && edge->is_causal()) {
            const auto& causal = *edge->causal_metadata;
            avg_strength += causal.get_strength_score();
            edge_count++;

            if (causal.temporality == Temporality::IMMEDIATE) has_immediate = true;
            if (causal.temporality == Temporality::DELAYED || causal.temporality == Temporality::LONG_TERM) has_delayed = true;

            if (!causal.mechanism_type.empty()) {
                mechanism_types[causal.mechanism_type]++;
            }
        }
    }

    if (edge_count > 0) {
        avg_strength /= edge_count;
    }

    // Find dominant mechanism type
    int max_count = 0;
    for (const auto& [type, count] : mechanism_types) {
        if (count > max_count) {
            max_count = count;
            dominant_mechanism_type = type;
        }
    }

    ss << "A ";

    // Phase 2: Classify by strength
    if (avg_strength >= 0.75) {
        ss << "**strong** ";
    } else if (avg_strength >= 0.5) {
        ss << "**moderate** ";
    } else if (avg_strength > 0) {
        ss << "**weak** ";
    }

    ss << "feedback loop involving **" << insight.seed_labels[0] << "** ";

    // Show temporality characteristics
    if (has_immediate && has_delayed) {
        ss << "with both immediate and delayed feedback ";
    } else if (has_immediate) {
        ss << "with immediate feedback ";
    } else if (has_delayed) {
        ss << "with delayed feedback ";
    }

    ss << "creates a self-reinforcing cycle";

    // Show mechanism type if available
    if (!dominant_mechanism_type.empty()) {
        ss << " through " << dominant_mechanism_type << " mechanisms";
    }

    ss << " in the causal structure.";

    return ss.str();
}

std::string ReportGenerator::describe_confounder(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";

    // Phase 2: Check if the confounding links have causal metadata
    int causal_links = 0;
    int strong_links = 0;
    std::vector<std::string> affected_outcomes;

    for (const auto& edge_id : insight.witness_edges) {
        const auto* edge = graph_.get_hyperedge(edge_id);
        if (edge && edge->is_causal()) {
            causal_links++;
            if (edge->causal_metadata->strength >= CausalStrength::STRONG) {
                strong_links++;
            }
            // Collect targets as affected outcomes
            for (const auto& target : edge->targets) {
                if (std::find(affected_outcomes.begin(), affected_outcomes.end(), target) == affected_outcomes.end()) {
                    affected_outcomes.push_back(target);
                }
            }
        }
    }

    ss << "Node **" << insight.seed_labels[0] << "** may confound the relationship";

    if (affected_outcomes.size() > 1) {
        ss << " affecting **" << affected_outcomes.size() << " outcome(s)**";
    }

    // Phase 2: Add strength information
    if (strong_links > 0) {
        ss << " through **" << strong_links << " strong causal link(s)**";
    } else if (causal_links > 0) {
        ss << " through **" << causal_links << " causal link(s)**";
    }

    ss << ", acting as a common cause that influences multiple outcomes.";

    return ss.str();
}

std::string ReportGenerator::describe_taxonomy(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "A hierarchical relationship connects **" << insight.seed_labels[0] << "** ";
    ss << "and **" << insight.seed_labels[1] << "** through is-a or part-of relations.";
    return ss.str();
}

std::string ReportGenerator::describe_domain_bridge(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";
    ss << "Node **" << insight.seed_labels[0] << "** bridges multiple domains or disciplines, ";
    ss << "connecting otherwise separate areas of knowledge.";
    return ss.str();
}

std::string ReportGenerator::describe_logical_entailment(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "Logical relationships suggest that **" << insight.seed_labels[0] << "** ";
    ss << "may entail or imply **" << insight.seed_labels[1] << "**.";
    return ss.str();
}

std::string ReportGenerator::describe_compositional_reasoning(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "Composed relations connect **" << insight.seed_labels[0] << "** ";
    ss << "and **" << insight.seed_labels[1] << "** through relation algebra.";
    return ss.str();
}

std::string ReportGenerator::describe_explanatory_chain(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.size() < 2) return "";
    ss << "An explanatory path connects **" << insight.seed_labels[0] << "** to **";
    ss << insight.seed_labels[1] << "**, providing a multi-hop explanation across distant concepts.";
    return ss.str();
}

std::string ReportGenerator::describe_schema_violation(const Insight& insight) const {
    std::stringstream ss;

    // Parse structured description: SCHEMA_VIOLATION|entity_type:X|missing_relation:Y|expected_coverage:Z%|affected:N
    std::map<std::string, std::string> metadata;
    std::istringstream desc_stream(insight.description);
    std::string token;

    while (std::getline(desc_stream, token, '|')) {
        size_t colon_pos = token.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = token.substr(0, colon_pos);
            std::string value = token.substr(colon_pos + 1);
            metadata[key] = value;
        }
    }

    if (metadata.count("entity_type") && metadata.count("missing_relation")) {
        ss << "Entities of type **" << metadata["entity_type"] << "** ";
        ss << "are missing expected **" << metadata["missing_relation"] << "** relations. ";

        if (metadata.count("expected_coverage")) {
            ss << "Expected coverage: " << metadata["expected_coverage"] << "%. ";
        }

        if (metadata.count("affected")) {
            ss << "Affected entities: " << metadata["affected"] << ".";
        }
    }

    return ss.str();
}

std::string ReportGenerator::describe_transitive_closure(const Insight& insight) const {
    std::stringstream ss;

    // Parse structured description: TRANSITIVE_GAP|relation:X|intermediate:Y|chains:N
    std::map<std::string, std::string> metadata;
    std::istringstream desc_stream(insight.description);
    std::string token;

    while (std::getline(desc_stream, token, '|')) {
        size_t colon_pos = token.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = token.substr(0, colon_pos);
            std::string value = token.substr(colon_pos + 1);
            metadata[key] = value;
        }
    }

    if (insight.seed_labels.size() >= 2 && metadata.count("relation")) {
        ss << "Missing **" << metadata["relation"] << "** link between **";
        ss << insight.seed_labels[0] << "** and **" << insight.seed_labels[1] << "**";

        if (metadata.count("intermediate")) {
            ss << " (via **" << metadata["intermediate"] << "**)";
        }

        if (metadata.count("chains")) {
            ss << " — supported by " << metadata["chains"] << " transitive chain";
            if (metadata["chains"] != "1") ss << "s";
        }

        ss << ".";
    }

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

    if (counts[InsightType::CONTRADICTION] > 0) {
        ss << "- **" << counts[InsightType::CONTRADICTION] << " contradictions** flag conflicts where both affirmed "
           << "and negated claims appear for the same relationship.\n";
    }

    if (counts[InsightType::ENTITY_RESOLUTION] > 0) {
        ss << "- **" << counts[InsightType::ENTITY_RESOLUTION] << " entity resolution candidates** suggest likely "
           << "duplicate or alias entities.\n";
    }

    if (counts[InsightType::CORE_PERIPHERY] > 0) {
        ss << "- **" << counts[InsightType::CORE_PERIPHERY] << " core-periphery roles** highlight which entities sit "
           << "at the structural core versus the periphery.\n";
    }

    if (counts[InsightType::TEXT_SIMILARITY] > 0) {
        ss << "- **" << counts[InsightType::TEXT_SIMILARITY] << " text similarity links** connect entities with "
           << "highly similar labels.\n";
    }

    if (counts[InsightType::ARGUMENT_SUPPORT] > 0) {
        ss << "- **" << counts[InsightType::ARGUMENT_SUPPORT] << " argument-supported relations** propose links "
           << "grounded in evidence paths through the graph.\n";
    }

    if (counts[InsightType::ACTIVE_LEARNING] > 0) {
        ss << "- **" << counts[InsightType::ACTIVE_LEARNING] << " active learning queries** prioritize uncertain or "
           << "high-impact relations for verification.\n";
    }

    if (counts[InsightType::METHOD_OUTCOME] > 0) {
        ss << "- **" << counts[InsightType::METHOD_OUTCOME] << " method/outcome classifications** identify methods "
           << "and outcomes in the graph.\n";
    }
    if (counts[InsightType::CENTRALITY] > 0) {
        ss << "- **" << counts[InsightType::CENTRALITY] << " centrality findings** rank entities by structural importance.\n";
    }
    if (counts[InsightType::COMMUNITY_DETECTION] > 0) {
        ss << "- **" << counts[InsightType::COMMUNITY_DETECTION] << " community clusters** summarize dense regions of the graph.\n";
    }
    if (counts[InsightType::K_CORE] > 0) {
        ss << "- **" << counts[InsightType::K_CORE] << " k-core nodes** identify entities in tightly connected cores.\n";
    }
    if (counts[InsightType::K_TRUSS] > 0) {
        ss << "- **" << counts[InsightType::K_TRUSS] << " k-truss edges** highlight triangle-reinforced relationships.\n";
    }
    if (counts[InsightType::CLAIM_STANCE] > 0) {
        ss << "- **" << counts[InsightType::CLAIM_STANCE] << " claim stance classifications** label extracted claims as supportive, opposing, or neutral.\n";
    }
    if (counts[InsightType::RELATION_INDUCTION] > 0) {
        ss << "- **" << counts[InsightType::RELATION_INDUCTION] << " relation inductions** suggest canonical relation types.\n";
    }
    if (counts[InsightType::ANALOGICAL_TRANSFER] > 0) {
        ss << "- **" << counts[InsightType::ANALOGICAL_TRANSFER] << " analogical transfers** propose new links by analogy.\n";
    }
    if (counts[InsightType::UNCERTAINTY_SAMPLING] > 0) {
        ss << "- **" << counts[InsightType::UNCERTAINTY_SAMPLING] << " uncertainty samples** highlight low-confidence relations.\n";
    }
    if (counts[InsightType::COUNTERFACTUAL] > 0) {
        ss << "- **" << counts[InsightType::COUNTERFACTUAL] << " counterfactual probes** generate falsification questions.\n";
    }
    if (counts[InsightType::HYPEREDGE_PREDICTION] > 0) {
        ss << "- **" << counts[InsightType::HYPEREDGE_PREDICTION] << " hyperedge predictions** propose new relations from overlap.\n";
    }
    // Constrained rules removed from pipeline

    if (counts[InsightType::SURPRISE] > 0) {
        ss << "- **" << counts[InsightType::SURPRISE] << " surprising connections** were discovered that represent "
           << "statistically unusual or novel relationships.\n";
    }

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        ss << "- **" << counts[InsightType::COMMUNITY_LINK] << " community links** highlight cross-cluster candidates "
           << "with similar relation signatures.\n";
    }

    if (counts[InsightType::HYPOTHESES_1] > 0) {
        ss << "- **" << counts[InsightType::HYPOTHESES_1] << " hypotheses** synthesize testable claims from the "
           << "combined discovery findings.\n";
    }
    if (counts[InsightType::HYPOTHESES_2] > 0) {
        ss << "- **" << counts[InsightType::HYPOTHESES_2] << " mechanistic hypotheses (H2)** generate graph-native "
           << "mechanistic chains with structured test plans.\n";
    }
    if (counts[InsightType::HYPOTHESES_3] > 0) {
        ss << "- **" << counts[InsightType::HYPOTHESES_3] << " counterfactual causal hypotheses (H3)** propose "
           << "causal interventions of the form 'If X then Y because Z'.\n";
    }
    if (counts[InsightType::MECHANISM_CONSOLIDATION] > 0) {
        ss << "- **" << counts[InsightType::MECHANISM_CONSOLIDATION] << " mechanism consolidations** cluster related hypotheses into shared explanatory themes.\n";
    }
    if (counts[InsightType::EVIDENCE_FUSION_LINK] > 0) {
        ss << "- **" << counts[InsightType::EVIDENCE_FUSION_LINK] << " evidence-fusion links** combine multiple operator signals into high-confidence candidates.\n";
    }
    if (counts[InsightType::META_PATH_ANOMALY] > 0) {
        ss << "- **" << counts[InsightType::META_PATH_ANOMALY] << " meta-path anomalies** capture rare but plausible relation-sequence templates.\n";
    }
    if (counts[InsightType::INTERVENTION_BOTTLENECK] > 0) {
        ss << "- **" << counts[InsightType::INTERVENTION_BOTTLENECK] << " intervention bottlenecks** identify high-leverage nodes across causal pathways.\n";
    }
    if (counts[InsightType::COMPETING_MECHANISM] > 0) {
        ss << "- **" << counts[InsightType::COMPETING_MECHANISM] << " competing-mechanism findings** expose alternative explanations for shared outcomes.\n";
    }
    if (counts[InsightType::SCHEMA_REPAIR] > 0) {
        ss << "- **" << counts[InsightType::SCHEMA_REPAIR] << " schema repairs** provide executable graph consistency improvements.\n";
    }
    if (counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] > 0) {
        ss << "- **" << counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] << " cross-community mechanism bridges** connect distant domains through interpretable pathways.\n";
    }

    if (counts[InsightType::DIFFUSION] > 0) {
        ss << "- **" << counts[InsightType::DIFFUSION] << " influence pathways** were mapped showing how concepts "
           << "relate through network diffusion.\n";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        ss << "- **" << counts[InsightType::PATH_RANK] << " path-ranked links** highlight pairs connected by multiple short paths.\n";
    }
    if (counts[InsightType::LONG_CHAIN] > 0) {
        ss << "- **" << counts[InsightType::LONG_CHAIN] << " long-chain links** surface distant multi-hop reasoning paths.\n";
    }
    if (counts[InsightType::META_PATH] > 0) {
        ss << "- **" << counts[InsightType::META_PATH] << " meta-path links** connect entities through typed relation sequences.\n";
    }
    if (counts[InsightType::META_PATH_PATTERN] > 0) {
        ss << "- **" << counts[InsightType::META_PATH_PATTERN] << " meta-path patterns** capture frequent relation templates across the graph.\n";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        ss << "- **" << counts[InsightType::AUTHOR_CHAIN] << " author reference chains** trace citation pathways across authors.\n";
    }

    if (counts[InsightType::CO_AUTHORSHIP] > 0) {
        ss << "- **" << counts[InsightType::CO_AUTHORSHIP] << " co-authorship networks** identify frequent collaborators and research clusters.\n";
    }

    if (counts[InsightType::CITATION_IMPACT] > 0) {
        ss << "- **" << counts[InsightType::CITATION_IMPACT] << " citation impact** rankings reveal influential authors and seminal works.\n";
    }

    if (counts[InsightType::MULTI_RESOLUTION_COMMUNITY] > 0) {
        ss << "- **" << counts[InsightType::MULTI_RESOLUTION_COMMUNITY] << " multi-resolution communities** reveal hierarchical clustering at multiple scales.\n";
    }

    if (counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] > 0) {
        ss << "- **" << counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] << " cross-community bridge maps** link bridge entities to the communities they connect.\n";
    }

    if (counts[InsightType::META_PATTERN] > 0) {
        ss << "- **" << counts[InsightType::META_PATTERN] << " meta-patterns** identify recurring structural templates across pattern insights.\n";
    }

    if (counts[InsightType::RULE] > 0) {
        ss << "- **" << counts[InsightType::RULE] << " association rules** were mined, revealing predictable "
           << "relationship patterns between different relation types.\n";
    }

    if (counts[InsightType::EMBEDDING_LINK] > 0) {
        ss << "- **" << counts[InsightType::EMBEDDING_LINK] << " embedding-based predictions** were generated using TransE "
           << "link prediction, suggesting plausible missing relationships.\n";
    }

    // Causal & Mechanistic Discovery
    if (counts[InsightType::CAUSAL_CHAIN] > 0) {
        ss << "- **" << counts[InsightType::CAUSAL_CHAIN] << " causal chains** trace directed paths along causal relation types.\n";
    }
    if (counts[InsightType::INTERVENTION_POINT] > 0) {
        ss << "- **" << counts[InsightType::INTERVENTION_POINT] << " intervention points** identify nodes whose removal disconnects causal chains.\n";
    }
    if (counts[InsightType::FEEDBACK_LOOP] > 0) {
        ss << "- **" << counts[InsightType::FEEDBACK_LOOP] << " feedback loops** expose cycles in causal or directional relations.\n";
    }
    if (counts[InsightType::CONFOUNDER] > 0) {
        ss << "- **" << counts[InsightType::CONFOUNDER] << " confounders** flag nodes connected to both ends of suspected causal relations.\n";
    }

    // Structural & Logical Reasoning
    if (counts[InsightType::TAXONOMY] > 0) {
        ss << "- **" << counts[InsightType::TAXONOMY] << " taxonomic relations** surface hierarchical is-a and part-of structures.\n";
    }
    if (counts[InsightType::DOMAIN_BRIDGE] > 0) {
        ss << "- **" << counts[InsightType::DOMAIN_BRIDGE] << " domain bridges** identify cross-domain or interdisciplinary connector nodes.\n";
    }
    if (counts[InsightType::LOGICAL_ENTAILMENT] > 0) {
        ss << "- **" << counts[InsightType::LOGICAL_ENTAILMENT] << " logical entailments** derive implicit implications from existing relations.\n";
    }
    if (counts[InsightType::COMPOSITIONAL_REASONING] > 0) {
        ss << "- **" << counts[InsightType::COMPOSITIONAL_REASONING] << " compositional relations** compose relation algebra to infer new links.\n";
    }
    if (counts[InsightType::EXPLANATORY_CHAIN] > 0) {
        ss << "- **" << counts[InsightType::EXPLANATORY_CHAIN] << " explanatory chains** connect distant concepts through multi-hop explanation paths.\n";
    }
    if (counts[InsightType::SCHEMA_VIOLATION] > 0) {
        ss << "- **" << counts[InsightType::SCHEMA_VIOLATION] << " schema violations** highlight entities or relations that break expected graph patterns.\n";
    }
    if (counts[InsightType::TRANSITIVE_CLOSURE] > 0) {
        ss << "- **" << counts[InsightType::TRANSITIVE_CLOSURE] << " transitive closure gaps** surface missing links implied by transitive relations.\n";
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

        // Add QC stats if available
        if (config.pipeline_stats.contains("quality_control")) {
            auto qc = config.pipeline_stats["quality_control"];
            ss << "### Quality Control Statistics\n\n";
            ss << "| Metric | Value |\n";
            ss << "|--------|-------|\n";
            ss << "| Initial Entities | " << qc.value("initial_nodes", 0) << " |\n";
            ss << "| Entities Removed | " << qc.value("total_removed", 0) << " |\n";
            ss << "| Removal Rate | " << std::fixed << std::setprecision(1)
               << (qc.value("initial_nodes", 0) > 0 ? 100.0 * qc.value("total_removed", 0) / qc.value("initial_nodes", 1) : 0.0) << "% |\n";

            if (qc.contains("connectivity")) {
                auto conn = qc["connectivity"];
                ss << "| Connected Components | " << conn.value("num_connected_components", 0) << " |\n";
                ss << "| Largest Component | " << conn.value("largest_component_size", 0)
                   << " (" << std::fixed << std::setprecision(1)
                   << (qc.value("final_nodes", 0) > 0 ? 100.0 * conn.value("largest_component_size", 0) / qc.value("final_nodes", 1) : 0.0) << "%) |\n";
                ss << "| Graph Density | " << std::fixed << std::setprecision(4) << conn.value("graph_density", 0.0) << " |\n";
                ss << "| Clustering Coefficient | " << std::fixed << std::setprecision(3) << conn.value("clustering_coefficient", 0.0) << " |\n";
            }
            ss << "\n";
        }
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

std::string ReportGenerator::generate_category_overview(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream ss;

    // Count insights by category
    std::map<InsightCategory, int> category_counts;
    std::map<InsightCategory, double> category_avg_scores;

    for (const auto& insight : insights.insights) {
        category_counts[insight.category]++;
        category_avg_scores[insight.category] += insight.score;
    }

    // Calculate averages
    for (auto& [cat, total_score] : category_avg_scores) {
        if (category_counts[cat] > 0) {
            total_score /= category_counts[cat];
        }
    }

    if (config.markdown_format) {
        ss << "## Knowledge Discovery Categories\n\n";
        ss << "Insights are classified into three categories based on how they generate new knowledge:\n\n";

        // Combinatorial
        ss << "### 🧩 Combinatorial Insights (" << category_counts[InsightCategory::COMBINATORIAL] << ")\n";
        ss << "**Pattern Detection & Structural Combinations**\n\n";
        ss << "These insights emerge from combining existing knowledge in new ways, identifying patterns, ";
        ss << "structures, and motifs that reveal the underlying organization of the knowledge graph.\n";
        if (category_counts[InsightCategory::COMBINATORIAL] > 0) {
            ss << "- Average Score: " << std::fixed << std::setprecision(3)
               << category_avg_scores[InsightCategory::COMBINATORIAL] << "\n";
        }
        ss << "\n";

        // Exploratory
        ss << "### 🔍 Exploratory Insights (" << category_counts[InsightCategory::EXPLORATORY] << ")\n";
        ss << "**Path Finding & Connection Discovery**\n\n";
        ss << "These insights discover new connections, paths, and bridges between concepts, ";
        ss << "exploring previously unexplored areas of the knowledge space.\n";
        if (category_counts[InsightCategory::EXPLORATORY] > 0) {
            ss << "- Average Score: " << std::fixed << std::setprecision(3)
               << category_avg_scores[InsightCategory::EXPLORATORY] << "\n";
        }
        ss << "\n";

        // Transformational
        ss << "### 🔄 Transformational Insights (" << category_counts[InsightCategory::TRANSFORMATIONAL] << ")\n";
        ss << "**Reframing & Perspective Shifts**\n\n";
        ss << "These insights transform understanding by reframing concepts, generating hypotheses, ";
        ss << "identifying contradictions, and suggesting alternative causal mechanisms.\n";
        if (category_counts[InsightCategory::TRANSFORMATIONAL] > 0) {
            ss << "- Average Score: " << std::fixed << std::setprecision(3)
               << category_avg_scores[InsightCategory::TRANSFORMATIONAL] << "\n";
        }
        ss << "\n";

    } else {
        ss << "KNOWLEDGE DISCOVERY CATEGORIES\n";
        ss << "------------------------------\n\n";
        ss << "Combinatorial (Pattern Detection): " << category_counts[InsightCategory::COMBINATORIAL] << "\n";
        ss << "Exploratory (Connection Discovery): " << category_counts[InsightCategory::EXPLORATORY] << "\n";
        ss << "Transformational (Reframing): " << category_counts[InsightCategory::TRANSFORMATIONAL] << "\n\n";
    }

    return ss.str();
}

std::string ReportGenerator::generate_augmentation_overview(const ReportConfig& config) {
    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Augmentation Methods\n\n";
        ss << "This report uses a broad set of augmentation signals derived from the graph structure and local patterns:\n";
        ss << "- **Bridge**: identifies entities that connect otherwise separate regions of the graph.\n";
        ss << "- **Completion**: suggests missing relations based on similar neighborhood patterns.\n";
        ss << "- **Motif**: highlights recurring subgraph structures that indicate repeated concepts.\n";
        ss << "- **Substitution**: proposes interchangeable entities in similar contexts.\n";
        ss << "- **Contradiction**: flags conflicts where affirmed and negated claims both appear.\n";
        ss << "- **Entity Resolution**: detects likely duplicate or alias entities.\n";
        ss << "- **Core-Periphery**: maps central hubs versus peripheral entities.\n";
        ss << "- **Text Similarity**: links entities with highly similar labels.\n";
        ss << "- **Argument Support**: proposes relations grounded in evidence paths.\n";
        ss << "- **Active Learning**: surfaces high-value verification queries.\n";
        ss << "- **Method/Outcome**: identifies method or outcome entities.\n";
        ss << "- **Centrality**: ranks entities by structural importance in the bipartite graph.\n";
        ss << "- **Community Detection**: groups entities into dense clusters.\n";
        ss << "- **k-Core**: highlights nodes in tightly connected cores.\n";
        ss << "- **k-Truss**: identifies edges reinforced by shared triangles.\n";
        ss << "- **Claim Stance**: classifies claims as supporting/opposing/neutral.\n";
        ss << "- **Relation Induction**: proposes canonical relation type names.\n";
        ss << "- **Analogical Transfer**: suggests new links by analogy.\n";
        ss << "- **Uncertainty Sampling**: highlights low-confidence relations for verification.\n";
        ss << "- **Counterfactual**: asks falsification questions for key claims.\n";
        ss << "- **Hyperedge Prediction**: predicts relations using overlap patterns.\n";
        // Constrained rules removed from pipeline
        ss << "- **Diffusion**: maps likely influence pathways over the graph.\n";
        ss << "- **Surprise**: flags statistically unusual or novel connections.\n";
        ss << "- **Community Link**: proposes cross-cluster links with similar relation signatures.\n";
        ss << "- **Path Rank**: proposes links supported by multiple short graph paths.\n";
        ss << "- **Author Chain**: traces citation chains across authors.\n";
        ss << "- **Co-Authorship**: identifies frequent collaborators and research clusters.\n";
        ss << "- **Citation Impact**: ranks influential authors and works by citations/authorships.\n";
        ss << "- **Multi-Resolution Community**: reveals hierarchical community structure at multiple scales.\n";
        ss << "- **Cross-Community Bridge Map**: maps bridges to the communities they connect.\n";
        ss << "- **Meta-Pattern**: identifies recurring structural templates across pattern insights.\n";
        ss << "- **Hypothesis**: synthesizes testable claims from combined discovery findings.\n";
        ss << "- **Mechanism Consolidation**: clusters overlapping hypotheses into shared mechanisms.\n";
        ss << "- **Evidence Fusion Link**: combines multiple operator signals into consensus links.\n";
        ss << "- **Meta-Path Anomaly**: captures rare but plausible relation-sequence templates.\n";
        ss << "- **Intervention Bottleneck**: identifies high-leverage nodes across causal paths.\n";
        ss << "- **Competing Mechanism**: surfaces alternative explanations for the same outcome.\n";
        ss << "- **Schema Repair**: proposes executable graph consistency fixes.\n";
        ss << "- **Cross-Community Mechanism Bridge**: connects distant communities via mechanism paths.\n";
        ss << "- **Rule**: mines association rules between relation types (e.g., if R1(X,Y) then R2(X,Z)).\n";
        ss << "- **Embedding Link**: predicts missing relations using TransE embeddings.\n\n";
        ss << "Alternatives to consider:\n";
        ss << "- Embedding-based link prediction (TransE, RotatE, ComplEx).\n";
        ss << "- GNN-based link prediction (GraphSAGE, GAT, RGCN).\n";
        ss << "- Path-based ranking or meta-path inference for schema-aware graphs.\n";
        ss << "- Community-aware augmentation that prioritizes cross-cluster links.\n\n";
    } else {
        ss << "AUGMENTATION METHODS\n";
        ss << "--------------------\n";
        ss << "This report uses a broad set of augmentation signals derived from the graph structure and local patterns:\n";
        ss << "1) Bridge: identifies entities that connect otherwise separate regions of the graph.\n";
        ss << "2) Completion: suggests missing relations based on similar neighborhood patterns.\n";
        ss << "3) Motif: highlights recurring subgraph structures that indicate repeated concepts.\n";
        ss << "4) Substitution: proposes interchangeable entities in similar contexts.\n";
        ss << "5) Contradiction: flags conflicts where the graph includes both affirmed and negated claims.\n";
        ss << "6) Entity Resolution: detects likely duplicate or alias entities.\n";
        ss << "7) Core-Periphery: maps central hubs versus peripheral entities.\n";
        ss << "8) Text Similarity: links entities with highly similar labels.\n";
        ss << "9) Argument Support: proposes relations grounded in evidence paths.\n";
        ss << "10) Active Learning: surfaces high-value verification queries.\n";
        ss << "11) Method/Outcome: identifies method or outcome entities.\n";
        ss << "12) Centrality: ranks entities by structural importance.\n";
        ss << "13) Community Detection: groups entities into dense clusters.\n";
        ss << "14) k-Core: highlights nodes in tightly connected cores.\n";
        ss << "15) k-Truss: identifies edges reinforced by triangles.\n";
        ss << "16) Claim Stance: classifies claims as supporting/opposing/neutral.\n";
        ss << "17) Relation Induction: proposes canonical relation type names.\n";
        ss << "18) Analogical Transfer: suggests new links by analogy.\n";
        ss << "19) Uncertainty Sampling: highlights low-confidence relations.\n";
        ss << "20) Counterfactual: generates falsification questions.\n";
        ss << "21) Hyperedge Prediction: predicts relations using overlap patterns.\n";
        ss << "22) Diffusion: maps likely influence pathways over the graph.\n";
        ss << "23) Surprise: flags statistically unusual or novel connections.\n";
        ss << "24) Community Link: proposes cross-cluster links with similar relation signatures.\n";
        ss << "25) Path Rank: proposes links supported by multiple short graph paths.\n";
        ss << "26) Author Chain: traces citation chains across authors.\n";
        ss << "27) Co-Authorship: identifies frequent collaborators and research clusters.\n";
        ss << "28) Citation Impact: ranks influential authors and works by citations/authorships.\n";
        ss << "29) Multi-Resolution Community: reveals hierarchical community structure at multiple scales.\n";
        ss << "30) Cross-Community Bridge Map: maps bridges to the communities they connect.\n";
        ss << "31) Meta-Pattern: identifies recurring structural templates across pattern insights.\n";
        ss << "32) Hypothesis: synthesizes testable claims from combined findings.\n";
        ss << "33) Mechanism Consolidation: clusters overlapping hypotheses into shared mechanisms.\n";
        ss << "34) Evidence Fusion Link: combines multiple operator signals into consensus links.\n";
        ss << "35) Competing Mechanism: surfaces alternative explanations for shared outcomes.\n";
        ss << "36) Schema Repair: proposes executable graph consistency fixes.\n";
        ss << "37) Cross-Community Mechanism Bridge: links distant communities via mechanism paths.\n";
        ss << "38) Rule: mines association rules between relation types.\n";
        ss << "39) Embedding Link: predicts missing relations using TransE embeddings.\n\n";
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

std::string ReportGenerator::format_source_documents_html(const Insight& insight) const {
    if (insight.source_documents.empty()) {
        return "";
    }

    std::stringstream ss;
    ss << R"HTML(<div class="provenance" style="margin-top: 10px; padding: 8px; background: rgba(100,100,100,0.1); border-left: 3px solid #4fc3f7; border-radius: 4px;">
        <span style="font-weight: 600; color: #4fc3f7;">📄 Sources:</span> )HTML";

    for (size_t i = 0; i < insight.source_documents.size(); ++i) {
        const auto& doc = insight.source_documents[i];
        // Extract filename from path
        size_t last_slash = doc.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? doc.substr(last_slash + 1) : doc;

        if (i > 0) ss << ", ";
        ss << R"HTML(<a href=")HTML" << escape_html(doc) << R"HTML(" style="color: #4fc3f7; text-decoration: none;">)HTML"
           << escape_html(filename) << "</a>";
    }

    ss << R"HTML(</div>)HTML";
    return ss.str();
}

std::string ReportGenerator::format_source_documents_markdown(const Insight& insight) const {
    if (insight.source_documents.empty()) {
        return "";
    }

    std::stringstream ss;
    ss << "\n\n**Sources:** ";

    for (size_t i = 0; i < insight.source_documents.size(); ++i) {
        const auto& doc = insight.source_documents[i];
        // Extract filename from path
        size_t last_slash = doc.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? doc.substr(last_slash + 1) : doc;

        if (i > 0) ss << ", ";
        ss << "`" << filename << "`";
    }

    return ss.str();
}

// Phase 2: Generate HTML badges for causal metadata
std::string ReportGenerator::generate_causal_badges_html(const Insight& insight) const {
    std::stringstream badges;

    // Only generate badges for causal insight types
    if (insight.type != InsightType::CAUSAL_CHAIN &&
        insight.type != InsightType::INTERVENTION_POINT &&
        insight.type != InsightType::FEEDBACK_LOOP &&
        insight.type != InsightType::CONFOUNDER) {
        return "";
    }

    // Check if any witness edges have causal metadata
    bool has_causal_metadata = false;
    const HyperEdge* causal_edge = nullptr;

    for (const auto& edge_id : insight.witness_edges) {
        const auto* edge = graph_.get_hyperedge(edge_id);
        if (edge && edge->is_causal()) {
            has_causal_metadata = true;
            causal_edge = edge;
            break;  // Use first causal edge for badge generation
        }
    }

    if (!has_causal_metadata || !causal_edge) {
        return "";
    }

    const auto& causal = *causal_edge->causal_metadata;
    badges << R"HTML(<div class="causal-badges">)HTML";

    // Strength badge
    std::string strength_str = causal.get_strength_string();
    std::string strength_class = "badge badge-strength-" + strength_str;
    badges << R"HTML(<span class=")HTML" << strength_class << R"HTML(">)HTML"
           << strength_str << R"HTML(</span>)HTML";

    // Type badge (highlight NECESSARY and SUFFICIENT)
    std::string type_str = causal.get_type_string();
    std::string type_class = "badge badge-type";
    if (causal.type == CausalRelationType::NECESSARY) {
        type_class = "badge badge-type-necessary";
    } else if (causal.type == CausalRelationType::SUFFICIENT) {
        type_class = "badge badge-type-sufficient";
    }
    badges << R"HTML(<span class=")HTML" << type_class << R"HTML(">)HTML"
           << type_str << R"HTML(</span>)HTML";

    // Temporality badge
    std::string temp_str = causal.get_temporality_string();
    std::string temp_class = "badge badge-temporality";
    if (causal.temporality == Temporality::IMMEDIATE) {
        temp_class = "badge badge-immediate";
    } else if (causal.temporality == Temporality::DELAYED) {
        temp_class = "badge badge-delayed";
    }
    badges << R"HTML(<span class=")HTML" << temp_class << R"HTML(">)HTML"
           << temp_str << R"HTML(</span>)HTML";

    // Mechanism badge (if available)
    if (!causal.mechanism_type.empty()) {
        badges << R"HTML(<span class="badge badge-mechanism">)HTML"
               << escape_html(causal.mechanism_type) << R"HTML(</span>)HTML";
    }

    badges << R"HTML(</div>)HTML";
    return badges.str();
}

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
        case InsightType::CONTRADICTION:
            summary = describe_contradiction(insight);
            break;
        case InsightType::ENTITY_RESOLUTION:
            summary = describe_entity_resolution(insight);
            break;
        case InsightType::CORE_PERIPHERY:
            summary = describe_core_periphery(insight);
            break;
        case InsightType::TEXT_SIMILARITY:
            summary = describe_text_similarity(insight);
            break;
        case InsightType::ARGUMENT_SUPPORT:
            summary = describe_argument_support(insight);
            break;
        case InsightType::ACTIVE_LEARNING:
            summary = describe_active_learning(insight);
            break;
        case InsightType::METHOD_OUTCOME:
            summary = describe_method_outcome(insight);
            break;
        case InsightType::CENTRALITY:
            summary = describe_centrality(insight);
            break;
        case InsightType::COMMUNITY_DETECTION:
            summary = describe_community_detection(insight);
            break;
        case InsightType::K_CORE:
            summary = describe_k_core(insight);
            break;
        case InsightType::K_TRUSS:
            summary = describe_k_truss(insight);
            break;
        case InsightType::CLAIM_STANCE:
            summary = describe_claim_stance(insight);
            break;
        case InsightType::RELATION_INDUCTION:
            summary = describe_relation_induction(insight);
            break;
        case InsightType::ANALOGICAL_TRANSFER:
            summary = describe_analogical_transfer(insight);
            break;
        case InsightType::UNCERTAINTY_SAMPLING:
            summary = describe_uncertainty_sampling(insight);
            break;
        case InsightType::COUNTERFACTUAL:
            summary = describe_counterfactual(insight);
            break;
        case InsightType::HYPEREDGE_PREDICTION:
            summary = describe_hyperedge_prediction(insight);
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
        case InsightType::LONG_CHAIN:
            summary = describe_long_chain(insight);
            break;
        case InsightType::META_PATH_PATTERN:
            summary = describe_meta_path_pattern(insight);
            break;
        case InsightType::META_PATH:
            summary = describe_meta_path(insight);
            break;
        case InsightType::HYPOTHESES_1:
        case InsightType::HYPOTHESES_2:
        case InsightType::HYPOTHESES_3:
            summary = describe_hypothesis(insight);
            break;
        case InsightType::MECHANISM_CONSOLIDATION:
            summary = describe_mechanism_consolidation(insight);
            break;
        case InsightType::EVIDENCE_FUSION_LINK:
            summary = describe_evidence_fusion(insight);
            break;
        case InsightType::META_PATH_ANOMALY:
            summary = describe_meta_path_anomaly(insight);
            break;
        case InsightType::INTERVENTION_BOTTLENECK:
            summary = describe_intervention_bottleneck(insight);
            break;
        case InsightType::COMPETING_MECHANISM:
            summary = describe_competing_mechanism(insight);
            break;
        case InsightType::SCHEMA_REPAIR:
            summary = describe_schema_repair(insight);
            break;
        case InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE:
            summary = describe_cross_community_bridge(insight);
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
        case InsightType::CAUSAL_CHAIN:
            summary = describe_causal_chain(insight);
            break;
        case InsightType::INTERVENTION_POINT:
            summary = describe_intervention_point(insight);
            break;
        case InsightType::FEEDBACK_LOOP:
            summary = describe_feedback_loop(insight);
            break;
        case InsightType::CONFOUNDER:
            summary = describe_confounder(insight);
            break;
        case InsightType::TAXONOMY:
            summary = describe_taxonomy(insight);
            break;
        case InsightType::DOMAIN_BRIDGE:
            summary = describe_domain_bridge(insight);
            break;
        case InsightType::LOGICAL_ENTAILMENT:
            summary = describe_logical_entailment(insight);
            break;
        case InsightType::COMPOSITIONAL_REASONING:
            summary = describe_compositional_reasoning(insight);
            break;
        case InsightType::EXPLANATORY_CHAIN:
            summary = describe_explanatory_chain(insight);
            break;
        case InsightType::SCHEMA_VIOLATION:
            summary = describe_schema_violation(insight);
            break;
        case InsightType::TRANSITIVE_CLOSURE:
            summary = describe_transitive_closure(insight);
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
        InsightType::CONTRADICTION,
        InsightType::ENTITY_RESOLUTION,
        InsightType::CORE_PERIPHERY,
        InsightType::TEXT_SIMILARITY,
        InsightType::ARGUMENT_SUPPORT,
        InsightType::ACTIVE_LEARNING,
        InsightType::METHOD_OUTCOME,
        InsightType::CENTRALITY,
        InsightType::COMMUNITY_DETECTION,
        InsightType::K_CORE,
        InsightType::K_TRUSS,
        InsightType::CLAIM_STANCE,
        InsightType::RELATION_INDUCTION,
        InsightType::ANALOGICAL_TRANSFER,
        InsightType::UNCERTAINTY_SAMPLING,
        InsightType::COUNTERFACTUAL,
        InsightType::HYPEREDGE_PREDICTION,
        InsightType::DIFFUSION,
        InsightType::SURPRISE,
        InsightType::COMMUNITY_LINK,
        InsightType::PATH_RANK,
        InsightType::LONG_CHAIN,
        InsightType::META_PATH_PATTERN,
        InsightType::AUTHOR_CHAIN,
        InsightType::HYPOTHESES_1,
        InsightType::MECHANISM_CONSOLIDATION,
        InsightType::EVIDENCE_FUSION_LINK,
        InsightType::META_PATH_ANOMALY,
        InsightType::INTERVENTION_BOTTLENECK,
        InsightType::COMPETING_MECHANISM,
        InsightType::SCHEMA_REPAIR,
        InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE,
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
        InsightType::CONTRADICTION,
        InsightType::ENTITY_RESOLUTION,
        InsightType::CORE_PERIPHERY,
        InsightType::TEXT_SIMILARITY,
        InsightType::ARGUMENT_SUPPORT,
        InsightType::ACTIVE_LEARNING,
        InsightType::METHOD_OUTCOME,
        InsightType::CENTRALITY,
        InsightType::COMMUNITY_DETECTION,
        InsightType::K_CORE,
        InsightType::K_TRUSS,
        InsightType::CLAIM_STANCE,
        InsightType::RELATION_INDUCTION,
        InsightType::ANALOGICAL_TRANSFER,
        InsightType::UNCERTAINTY_SAMPLING,
        InsightType::COUNTERFACTUAL,
        InsightType::HYPEREDGE_PREDICTION,
        InsightType::DIFFUSION,
        InsightType::SURPRISE,
        InsightType::COMMUNITY_LINK,
        InsightType::PATH_RANK,
        InsightType::LONG_CHAIN,
        InsightType::META_PATH_PATTERN,
        InsightType::AUTHOR_CHAIN,
        InsightType::HYPOTHESES_1,
        InsightType::MECHANISM_CONSOLIDATION,
        InsightType::EVIDENCE_FUSION_LINK,
        InsightType::META_PATH_ANOMALY,
        InsightType::INTERVENTION_BOTTLENECK,
        InsightType::COMPETING_MECHANISM,
        InsightType::SCHEMA_REPAIR,
        InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE,
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);

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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_contradictions_section(const std::vector<Insight>& contradictions, const ReportConfig& config) {
    if (contradictions.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Contradictions\n\n";
        ss << "These findings highlight conflicting claims where both affirmed and negated relationships appear. "
           << "They often indicate extraction inconsistencies or ambiguous evidence that warrants review.\n\n";
    }

    std::vector<Insight> sorted = contradictions;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Conflict\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_contradiction(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_entity_resolutions_section(const std::vector<Insight>& resolutions, const ReportConfig& config) {
    if (resolutions.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Entity Resolution Candidates\n\n";
        ss << "These pairs of entities are likely duplicates or aliases based on label similarity and shared context. "
           << "Merging or linking them can reduce redundancy in the graph.\n\n";
    }

    std::vector<Insight> sorted = resolutions;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " ≈ " << insight.seed_labels[1] << "\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_entity_resolution(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_core_periphery_section(const std::vector<Insight>& cores, const ReportConfig& config) {
    if (cores.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Core–Periphery / Hub–Authority Analysis\n\n";
        ss << "These findings highlight entities that form the structural core of the graph "
           << "as well as those in the periphery. Hub/authority scores reflect whether an entity "
           << "tends to act as a source (hub) or target (authority) in relations.\n\n";
    }

    std::vector<Insight> sorted = cores;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_core_periphery(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_text_similarity_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Text-Semantic Similarity Links\n\n";
        ss << "These links connect entities whose labels are highly similar based on TF-IDF token similarity. "
           << "They can surface related concepts or naming variants.\n\n";
    }

    std::vector<Insight> sorted = links;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " ~ " << insight.seed_labels[1] << "\n\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_text_similarity(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_argument_support_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Argument-Supported Relations\n\n";
        ss << "These candidate links are grounded in evidence paths through the hypergraph.\n\n";
    }

    std::vector<Insight> sorted = links;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " ↔ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = config.use_llm_narratives && llm_provider_
            ? generate_llm_narrative(insight, config)
            : describe_argument_support(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_active_learning_section(const std::vector<Insight>& queries, const ReportConfig& config) {
    if (queries.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Active Learning Queries\n\n";
        ss << "These questions prioritize high-impact or low-confidence relations for verification.\n\n";
    }

    int count = 0;
    for (const auto& insight : queries) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Verification Question\n\n";
        }
        ss << describe_active_learning(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_method_outcome_section(const std::vector<Insight>& nodes, const ReportConfig& config) {
    if (nodes.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Method/Outcome Entities\n\n";
        ss << "These entities are classified as methods or outcomes based on label and context.\n\n";
    }

    std::vector<Insight> sorted = nodes;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        ss << describe_method_outcome(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_centrality_section(const std::vector<Insight>& nodes, const ReportConfig& config) {
    if (nodes.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Centrality Rankings\n\n";
        ss << "These entities are structurally central in the bipartite projection of the hypergraph.\n\n";
    }

    std::vector<Insight> sorted = nodes;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        ss << describe_centrality(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_community_detection_section(const std::vector<Insight>& communities, const ReportConfig& config) {
    if (communities.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Community Detection\n\n";
        ss << "These clusters are dense groups discovered via Louvain on the projected graph.\n\n";
    }

    std::vector<Insight> sorted = communities;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Community Cluster\n\n";
        }
        ss << describe_community_detection(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_k_core_section(const std::vector<Insight>& nodes, const ReportConfig& config) {
    if (nodes.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## k-Core Nodes\n\n";
        ss << "These nodes reside in dense k-core regions of the projected graph.\n\n";
    }

    std::vector<Insight> sorted = nodes;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        ss << describe_k_core(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_k_truss_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## k-Truss Edges\n\n";
        ss << "These edges lie inside dense triangle-reinforced subgraphs.\n\n";
    }

    std::vector<Insight> sorted = links;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " — " << insight.seed_labels[1] << "\n\n";
        }
        ss << describe_k_truss(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_claim_stance_section(const std::vector<Insight>& claims, const ReportConfig& config) {
    if (claims.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Claim Stance\n\n";
        ss << "These findings classify the stance (supports/opposes/neutral) of extracted claims.\n\n";
    }

    int count = 0;
    for (const auto& insight : claims) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " → "
               << insight.seed_labels[1] << "\n\n";
        }
        ss << describe_claim_stance(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_relation_induction_section(const std::vector<Insight>& relations, const ReportConfig& config) {
    if (relations.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Relation Type Induction\n\n";
        ss << "These suggestions propose canonical relation types based on observed usage.\n\n";
    }

    int count = 0;
    for (const auto& insight : relations) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Suggested Type\n\n";
        }
        ss << describe_relation_induction(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_analogical_transfer_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Analogical Transfer\n\n";
        ss << "These candidates are inferred by analogy between similar relation patterns.\n\n";
    }

    int count = 0;
    for (const auto& insight : links) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " → " << insight.seed_labels[1] << "\n\n";
        }
        ss << describe_analogical_transfer(insight) << "\n\n";
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_uncertainty_sampling_section(const std::vector<Insight>& samples, const ReportConfig& config) {
    if (samples.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Uncertainty Sampling\n\n";
        ss << "These low-confidence relations are prioritized for verification.\n\n";
    }
    int count = 0;
    for (const auto& insight : samples) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " → "
               << insight.seed_labels[1] << "\n\n";
        }
        ss << describe_uncertainty_sampling(insight) << "\n\n";
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_counterfactual_section(const std::vector<Insight>& probes, const ReportConfig& config) {
    if (probes.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Counterfactual Probing\n\n";
        ss << "These questions probe how claims could be falsified or reversed.\n\n";
    }
    int count = 0;
    for (const auto& insight : probes) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Counterfactual Question\n\n";
        }
        ss << describe_counterfactual(insight) << "\n\n";
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_hyperedge_prediction_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Hyperedge Prediction\n\n";
        ss << "These candidates are predicted by overlapping target sets under the same relation.\n\n";
    }
    int count = 0;
    for (const auto& insight : links) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " → "
               << insight.seed_labels[1] << "\n\n";
        }
        ss << describe_hyperedge_prediction(insight) << "\n\n";
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_constrained_rule_section(const std::vector<Insight>& rules, const ReportConfig& config) {
    if (rules.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Constrained Rules\n\n";
        ss << "These rules satisfy stricter support, confidence, and lift constraints.\n\n";
    }
    int count = 0;
    for (const auto& insight : rules) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Rule\n\n";
        }
        ss << describe_constrained_rule(insight) << "\n\n";
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);

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
        if (count >= capped_max_examples(config)) break;

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_diffusion(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_long_chain_section(const std::vector<Insight>& chains, const ReportConfig& config) {
    if (chains.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Long-Chain Reasoning Links\n\n";
        ss << "These findings highlight distant, multi-hop pathways that connect entities across the graph. "
           << "They surface interpretive chains that span multiple relations rather than direct links.\n\n";
    } else {
        ss << "LONG-CHAIN REASONING LINKS\n";
        ss << "---------------------------\n\n";
    }

    std::vector<Insight> sorted = chains;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

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
            narrative = describe_long_chain(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_meta_path_patterns_section(const std::vector<Insight>& patterns, const ReportConfig& config) {
    if (patterns.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Meta-Path Patterns\n\n";
        ss << "These patterns capture frequently occurring relation sequences across the graph, "
           << "revealing reusable structural templates for reasoning or link prediction.\n\n";
    } else {
        ss << "META-PATH PATTERNS\n";
        ss << "------------------\n\n";
    }

    std::vector<Insight> sorted = patterns;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

        std::string pattern;
        for (const auto& tag : insight.novelty_tags) {
            if (tag.rfind("pattern=", 0) == 0) {
                pattern = tag.substr(std::string("pattern=").size());
                break;
            }
        }

        if (config.markdown_format) {
            if (!pattern.empty()) {
                ss << "### " << (count + 1) << ". " << pattern << "\n\n";
            } else if (insight.seed_labels.size() >= 2) {
                ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
                   << " ↔ " << insight.seed_labels[1] << "\n\n";
            } else {
                ss << "### " << (count + 1) << ". Pattern\n\n";
            }
        } else {
            if (!pattern.empty()) {
                ss << (count + 1) << ". " << pattern << "\n";
            } else {
                ss << (count + 1) << ". Pattern\n";
            }
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_meta_path_pattern(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_meta_path_section(const std::vector<Insight>& paths, const ReportConfig& config) {
    if (paths.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Meta-Path Links\n\n";
        ss << "These candidates are inferred from typed relation sequences (meta-paths) that repeatedly connect pairs of entities. "
           << "They provide interpretable, schema-aware evidence for missing links.\n\n";
    } else {
        ss << "META-PATH LINKS\n";
        ss << "--------------\n\n";
    }

    std::vector<Insight> sorted = paths;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

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
            narrative = describe_meta_path(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_hypotheses_section(const std::vector<Insight>& hypotheses, const ReportConfig& config,
                                                          const std::string& title,
                                                          const std::string& description) {
    if (hypotheses.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## " << title << "\n\n";
        ss << description << "\n\n";
    } else {
        ss << title << "\n";
        for (size_t i = 0; i < title.length(); ++i) ss << "-";
        ss << "\n\n";
        ss << description << "\n\n";
    }

    std::vector<Insight> sorted = hypotheses;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (const auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_mechanism_consolidation_section(const std::vector<Insight>& mechanisms, const ReportConfig& config) {
    if (mechanisms.empty()) return "";

    std::stringstream ss;

    if (config.markdown_format) {
        ss << "## Mechanism Consolidation\n\n";
        ss << "These clusters consolidate overlapping hypotheses and chains into higher-level mechanisms.\n\n";
    } else {
        ss << "MECHANISM CONSOLIDATION\n";
        ss << "-----------------------\n\n";
    }

    std::vector<Insight> sorted = mechanisms;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
        return a.score > b.score;
    });

    int count = 0;
    for (const auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;

        if (config.markdown_format) {
            ss << "### " << (count + 1) << ". Mechanism Cluster\n\n";
        } else {
            ss << (count + 1) << ". Mechanism Cluster\n";
        }

        std::string narrative;
        if (config.use_llm_narratives && llm_provider_) {
            narrative = generate_llm_narrative(insight, config);
        } else {
            narrative = describe_mechanism_consolidation(insight);
        }
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_evidence_fusion_section(const std::vector<Insight>& links, const ReportConfig& config) {
    if (links.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Evidence Fusion Links\n\n";
        ss << "These links are supported by multiple independent operators, increasing confidence through convergent evidence.\n\n";
    } else {
        ss << "EVIDENCE FUSION LINKS\n";
        ss << "---------------------\n\n";
    }

    std::vector<Insight> sorted = links;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ↔ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_evidence_fusion(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_meta_path_anomaly_section(const std::vector<Insight>& anomalies, const ReportConfig& config) {
    if (anomalies.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Meta-Path Anomalies\n\n";
        ss << "Rare but plausible relation-sequence templates that may indicate novel mechanisms or underexplored structures.\n\n";
    } else {
        ss << "META-PATH ANOMALIES\n";
        ss << "-------------------\n\n";
    }

    std::vector<Insight> sorted = anomalies;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ↔ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_meta_path_anomaly(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_intervention_bottleneck_section(const std::vector<Insight>& bottlenecks, const ReportConfig& config) {
    if (bottlenecks.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Intervention Bottlenecks\n\n";
        ss << "Nodes that sit on many causal pathways and therefore offer high leverage for intervention.\n\n";
    } else {
        ss << "INTERVENTION BOTTLENECKS\n";
        ss << "------------------------\n\n";
    }

    std::vector<Insight> sorted = bottlenecks;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_intervention_bottleneck(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_competing_mechanism_section(const std::vector<Insight>& alternatives, const ReportConfig& config) {
    if (alternatives.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Competing Mechanisms\n\n";
        ss << "Alternative mechanism sets for the same outcome, useful for discriminative testing and falsification.\n\n";
    } else {
        ss << "COMPETING MECHANISMS\n";
        ss << "--------------------\n\n";
    }

    std::vector<Insight> sorted = alternatives;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 3) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0]
               << " vs " << insight.seed_labels[1] << " ⇒ " << insight.seed_labels[2] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_competing_mechanism(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_schema_repair_section(const std::vector<Insight>& repairs, const ReportConfig& config) {
    if (repairs.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Schema Repair Suggestions\n\n";
        ss << "Executable repair actions derived from schema violations, transitive closure gaps, and completion evidence.\n\n";
    } else {
        ss << "SCHEMA REPAIR SUGGESTIONS\n";
        ss << "-------------------------\n\n";
    }

    std::vector<Insight> sorted = repairs;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " → " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_schema_repair(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_cross_community_bridge_section(const std::vector<Insight>& bridges, const ReportConfig& config) {
    if (bridges.empty()) return "";

    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Cross-Community Mechanism Bridges\n\n";
        ss << "Mechanistic links that connect distant communities or domains through bridge nodes.\n\n";
    } else {
        ss << "CROSS-COMMUNITY MECHANISM BRIDGES\n";
        ss << "---------------------------------\n\n";
    }

    std::vector<Insight> sorted = bridges;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });

    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ↔ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_cross_community_bridge(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
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
        if (count >= capped_max_examples(config)) break;

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
        ss << format_source_documents_markdown(insight);
        count++;
    }

    return ss.str();
}

std::string ReportGenerator::generate_causal_chains_section(const std::vector<Insight>& chains, const ReportConfig& config) {
    if (chains.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Causal Chains\n\n";
        ss << "These chains represent directed sequences of cause-effect relationships.\n\n";
    } else {
        ss << "CAUSAL CHAINS\n--------------\n\n";
    }
    std::vector<Insight> sorted = chains;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0];
            if (insight.seed_labels.size() > 1) ss << " → " << insight.seed_labels[1];
            ss << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_causal_chain(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_intervention_points_section(const std::vector<Insight>& points, const ReportConfig& config) {
    if (points.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Intervention Points\n\n";
        ss << "Critical nodes whose removal would disconnect causal pathways.\n\n";
    } else {
        ss << "INTERVENTION POINTS\n-------------------\n\n";
    }
    std::vector<Insight> sorted = points;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_intervention_point(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_feedback_loops_section(const std::vector<Insight>& loops, const ReportConfig& config) {
    if (loops.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Feedback Loops\n\n";
        ss << "Cycles in causal relationships creating self-reinforcing dynamics.\n\n";
    } else {
        ss << "FEEDBACK LOOPS\n--------------\n\n";
    }
    std::vector<Insight> sorted = loops;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_feedback_loop(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_confounders_section(const std::vector<Insight>& confounders, const ReportConfig& config) {
    if (confounders.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Confounders\n\n";
        ss << "Nodes that may confound causal relationships as common causes.\n\n";
    } else {
        ss << "CONFOUNDERS\n-----------\n\n";
    }
    std::vector<Insight> sorted = confounders;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_confounder(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_taxonomy_section(const std::vector<Insight>& taxonomies, const ReportConfig& config) {
    if (taxonomies.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Taxonomy Relationships\n\n";
        ss << "Hierarchical is-a and part-of relationships organizing concepts.\n\n";
    } else {
        ss << "TAXONOMY RELATIONSHIPS\n----------------------\n\n";
    }
    std::vector<Insight> sorted = taxonomies;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ↔ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_taxonomy(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_domain_bridges_section(const std::vector<Insight>& bridges, const ReportConfig& config) {
    if (bridges.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Domain Bridges\n\n";
        ss << "Cross-domain connectors enabling interdisciplinary knowledge transfer.\n\n";
    } else {
        ss << "DOMAIN BRIDGES\n--------------\n\n";
    }
    std::vector<Insight> sorted = bridges;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && !insight.seed_labels.empty()) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_domain_bridge(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_logical_entailments_section(const std::vector<Insight>& entailments, const ReportConfig& config) {
    if (entailments.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Logical Entailments\n\n";
        ss << "Inferred implications derived from logical reasoning.\n\n";
    } else {
        ss << "LOGICAL ENTAILMENTS\n-------------------\n\n";
    }
    std::vector<Insight> sorted = entailments;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ⇒ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_logical_entailment(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_compositional_reasoning_section(const std::vector<Insight>& compositions, const ReportConfig& config) {
    if (compositions.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Compositional Reasoning\n\n";
        ss << "Composed relations discovered through relation algebra.\n\n";
    } else {
        ss << "COMPOSITIONAL REASONING\n-----------------------\n\n";
    }
    std::vector<Insight> sorted = compositions;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ◦ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_compositional_reasoning(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_explanatory_chains_section(const std::vector<Insight>& chains, const ReportConfig& config) {
    if (chains.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Explanatory Chains\n\n";
        ss << "Multi-hop paths explaining connections between distant concepts.\n\n";
    } else {
        ss << "EXPLANATORY CHAINS\n------------------\n\n";
    }
    std::vector<Insight> sorted = chains;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ⟿ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_explanatory_chain(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_schema_violations_section(const std::vector<Insight>& violations, const ReportConfig& config) {
    if (violations.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Schema Violations\n\n";
        ss << "Entities missing expected relations based on learned schema patterns.\n\n";
    } else {
        ss << "SCHEMA VIOLATIONS\n-----------------\n\n";
    }
    std::vector<Insight> sorted = violations;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_schema_violation(insight);
        ss << "**" << (count + 1) << ".** " << narrative << "\n\n";
        count++;
    }
    return ss.str();
}

std::string ReportGenerator::generate_transitive_closure_section(const std::vector<Insight>& gaps, const ReportConfig& config) {
    if (gaps.empty()) return "";
    std::stringstream ss;
    if (config.markdown_format) {
        ss << "## Transitive Closure Gaps\n\n";
        ss << "Missing links implied by transitive relations (if A→B and B→C, expect A→C).\n\n";
    } else {
        ss << "TRANSITIVE CLOSURE GAPS\n-----------------------\n\n";
    }
    std::vector<Insight> sorted = gaps;
    std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) { return a.score > b.score; });
    int count = 0;
    for (auto& insight : sorted) {
        if (count >= capped_max_examples(config)) break;
        if (config.markdown_format && insight.seed_labels.size() >= 2) {
            ss << "### " << (count + 1) << ". " << insight.seed_labels[0] << " ⇝ " << insight.seed_labels[1] << "\n\n";
        }
        std::string narrative = (config.use_llm_narratives && llm_provider_)
            ? generate_llm_narrative(insight, config) : describe_transitive_closure(insight);
        ss << narrative << "\n\n";
        ss << format_source_documents_markdown(insight);
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
        ss << "3. **Review Substitutions**: The " << counts[InsightType::SUBSTITUTION]
           << " potential substitutions highlight entities with interchangeable context. "
           << "Evaluate whether they represent synonyms or closely related concepts.\n\n";
    }

    if (counts[InsightType::ENTITY_RESOLUTION] > 0) {
        ss << "4. **Merge Likely Duplicates**: The " << counts[InsightType::ENTITY_RESOLUTION]
           << " entity resolution candidates suggest duplicate or alias entities "
           << "that could be linked or merged to improve consistency.\n\n";
    }

    if (counts[InsightType::CORE_PERIPHERY] > 0) {
        ss << "5. **Review Core–Periphery Roles**: The " << counts[InsightType::CORE_PERIPHERY]
           << " core-periphery insights highlight which entities anchor the graph "
           << "versus those on the periphery.\n\n";
    }

    if (counts[InsightType::TEXT_SIMILARITY] > 0) {
        ss << "6. **Review Text Similarity Links**: The " << counts[InsightType::TEXT_SIMILARITY]
           << " text similarity links surface entities with near-duplicate or closely related labels.\n\n";
    }

    if (counts[InsightType::ARGUMENT_SUPPORT] > 0) {
        ss << "7. **Validate Argument-Supported Relations**: The " << counts[InsightType::ARGUMENT_SUPPORT]
           << " proposed relations are backed by evidence paths and should be reviewed for correctness.\n\n";
    }

    if (counts[InsightType::ACTIVE_LEARNING] > 0) {
        ss << "8. **Answer Active Learning Queries**: The " << counts[InsightType::ACTIVE_LEARNING]
           << " validation questions target the most uncertain or high-impact relations.\n\n";
    }

    if (counts[InsightType::METHOD_OUTCOME] > 0) {
        ss << "9. **Confirm Method/Outcome Roles**: The " << counts[InsightType::METHOD_OUTCOME]
           << " classifications can clarify the graph’s conceptual structure.\n\n";
    }

    if (counts[InsightType::CENTRALITY] > 0) {
        ss << "10. **Review Central Entities**: The " << counts[InsightType::CENTRALITY]
           << " centrality findings highlight influential entities to prioritize for curation.\n\n";
    }

    if (counts[InsightType::COMMUNITY_DETECTION] > 0) {
        ss << "11. **Inspect Community Clusters**: The " << counts[InsightType::COMMUNITY_DETECTION]
           << " detected communities can guide topic segmentation or subgraph analysis.\n\n";
    }

    if (counts[InsightType::K_CORE] > 0) {
        ss << "12. **Assess k-Core Nodes**: The " << counts[InsightType::K_CORE]
           << " k-core entities represent dense cores worth validating or expanding.\n\n";
    }

    if (counts[InsightType::K_TRUSS] > 0) {
        ss << "13. **Validate k-Truss Links**: The " << counts[InsightType::K_TRUSS]
           << " k-truss edges reflect strong local cohesion and should be verified.\n\n";
    }

    if (counts[InsightType::CLAIM_STANCE] > 0) {
        ss << "14. **Review Claim Stance**: The " << counts[InsightType::CLAIM_STANCE]
           << " stance classifications help identify supporting vs. opposing claims.\n\n";
    }

    if (counts[InsightType::RELATION_INDUCTION] > 0) {
        ss << "15. **Normalize Relation Types**: The " << counts[InsightType::RELATION_INDUCTION]
           << " induced relation types can guide ontology cleanup.\n\n";
    }

    if (counts[InsightType::ANALOGICAL_TRANSFER] > 0) {
        ss << "16. **Validate Analogical Links**: The " << counts[InsightType::ANALOGICAL_TRANSFER]
           << " analogical transfers suggest new links worth verification.\n\n";
    }

    if (counts[InsightType::CONTRADICTION] > 0) {
        ss << "17. **Resolve Contradictions**: The " << counts[InsightType::CONTRADICTION]
           << " contradictions indicate conflicting claims that require manual review "
           << "to determine the correct relationship.\n\n";
    }

    if (counts[InsightType::SURPRISE] > 0) {
        ss << "18. **Investigate Surprises**: The " << counts[InsightType::SURPRISE]
           << " surprising connections warrant manual review to determine if they represent "
           << "genuine discoveries or potential data quality issues.\n\n";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        ss << "19. **Validate Path-Ranked Links**: The " << counts[InsightType::PATH_RANK]
           << " path-ranked links are supported by multiple short graph paths. "
           << "Prioritize high-confidence candidates for validation or targeted data collection.\n\n";
    }

    if (counts[InsightType::LONG_CHAIN] > 0) {
        ss << "20. **Review Long-Chain Links**: The " << counts[InsightType::LONG_CHAIN]
           << " long-chain reasoning paths highlight distant connections across multiple hops. "
           << "Use them to trace extended mechanisms or thematic linkages.\n\n";
    }

    if (counts[InsightType::META_PATH_PATTERN] > 0) {
        ss << "21. **Interpret Meta-Path Patterns**: The " << counts[InsightType::META_PATH_PATTERN]
           << " frequent relation templates summarize recurring multi-hop structures. "
           << "Use them to guide querying and validation strategies.\n\n";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        ss << "22. **Track Citation Trails**: The " << counts[InsightType::AUTHOR_CHAIN]
           << " author reference chains reveal how scholarship propagates through citations. "
           << "Use these chains to map influence or identify key scholarly bridges.\n\n";
    }

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        ss << "23. **Review Community Links**: The " << counts[InsightType::COMMUNITY_LINK]
           << " cross-cluster links highlight structurally similar entities across communities. "
           << "Validate candidates that bridge distinct topic areas.\n\n";
    }

    if (counts[InsightType::HYPOTHESES_1] > 0) {
        ss << "24. **Test Hypotheses**: The " << counts[InsightType::HYPOTHESES_1]
           << " synthesized hypotheses translate graph discoveries into testable claims. "
           << "Prioritize those with strong supporting evidence.\n\n";
    }

    if (counts[InsightType::MECHANISM_CONSOLIDATION] > 0) {
        ss << "25. **Review Consolidated Mechanisms**: The " << counts[InsightType::MECHANISM_CONSOLIDATION]
           << " mechanism clusters unify overlapping hypotheses and chains. "
           << "Use them to focus validation on the strongest shared explanations.\n\n";
    }

    if (counts[InsightType::EVIDENCE_FUSION_LINK] > 0) {
        ss << "26. **Prioritize Evidence-Fusion Links**: The " << counts[InsightType::EVIDENCE_FUSION_LINK]
           << " fused candidates are supported by multiple independent operators. "
           << "Use them as top-tier targets for validation and curation.\n\n";
    }

    if (counts[InsightType::META_PATH_ANOMALY] > 0) {
        ss << "27. **Investigate Meta-Path Anomalies**: The " << counts[InsightType::META_PATH_ANOMALY]
           << " rare relation templates may indicate novel mechanisms or underexplored structures.\n\n";
    }

    if (counts[InsightType::INTERVENTION_BOTTLENECK] > 0) {
        ss << "28. **Stress-Test Bottleneck Nodes**: The " << counts[InsightType::INTERVENTION_BOTTLENECK]
           << " intervention bottlenecks offer high leverage over downstream causal pathways.\n\n";
    }

    if (counts[InsightType::COMPETING_MECHANISM] > 0) {
        ss << "29. **Design Discriminative Tests**: The " << counts[InsightType::COMPETING_MECHANISM]
           << " competing-mechanism findings provide alternative explanations for shared outcomes.\n\n";
    }

    if (counts[InsightType::SCHEMA_REPAIR] > 0) {
        ss << "30. **Apply Schema Repairs Carefully**: The " << counts[InsightType::SCHEMA_REPAIR]
           << " repair suggestions provide executable consistency improvements. "
           << "Validate high-scoring actions before insertion.\n\n";
    }

    if (counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] > 0) {
        ss << "31. **Explore Cross-Community Bridges**: The " << counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE]
           << " bridge mechanisms connect distant communities and may support interdisciplinary hypotheses.\n\n";
    }

    if (counts[InsightType::RULE] > 0) {
        ss << "32. **Leverage Association Rules**: The " << counts[InsightType::RULE]
           << " discovered rules can be used for automated knowledge inference, "
           << "consistency checking, or to guide further data collection.\n\n";
    }

    if (counts[InsightType::EMBEDDING_LINK] > 0) {
        ss << "33. **Review Embedding Predictions**: The " << counts[InsightType::EMBEDDING_LINK]
           << " TransE-based link predictions suggest plausible missing relationships. "
           << "Higher plausibility scores indicate stronger evidence for the predicted link.\n\n";
    }
    if (counts[InsightType::MOTIF] > 0) {
        ss << "34. **Investigate Recurring Motifs**: The " << counts[InsightType::MOTIF]
           << " detected motifs represent frequently co-occurring concept clusters. "
           << "Use them to identify recurring structural patterns worth further study.\n\n";
    }
    if (counts[InsightType::DIFFUSION] > 0) {
        ss << "35. **Follow Influence Pathways**: The " << counts[InsightType::DIFFUSION]
           << " diffusion pathways map how concepts propagate through the graph. "
           << "Use them to trace knowledge transmission routes and influence dynamics.\n\n";
    }
    if (counts[InsightType::UNCERTAINTY_SAMPLING] > 0) {
        ss << "36. **Verify Uncertain Relations**: The " << counts[InsightType::UNCERTAINTY_SAMPLING]
           << " low-confidence relations are prime candidates for targeted validation or expert review.\n\n";
    }
    if (counts[InsightType::COUNTERFACTUAL] > 0) {
        ss << "37. **Answer Counterfactual Probes**: The " << counts[InsightType::COUNTERFACTUAL]
           << " falsification questions test the robustness of key claims. "
           << "Prioritize probes that target high-confidence findings.\n\n";
    }
    if (counts[InsightType::HYPEREDGE_PREDICTION] > 0) {
        ss << "38. **Review Hyperedge Predictions**: The " << counts[InsightType::HYPEREDGE_PREDICTION]
           << " predicted relations are derived from entity overlap patterns. "
           << "Validate before insertion into the knowledge graph.\n\n";
    }
    if (counts[InsightType::HYPOTHESES_2] > 0) {
        ss << "39. **Evaluate Mechanistic Chains (H2)**: The " << counts[InsightType::HYPOTHESES_2]
           << " typed mechanistic hypotheses provide role-labeled causal pathways with structured test plans. "
           << "Use them to prioritize interpretable mechanisms for experimental follow-up.\n\n";
    }
    if (counts[InsightType::HYPOTHESES_3] > 0) {
        ss << "40. **Test Counterfactual Hypotheses (H3)**: The " << counts[InsightType::HYPOTHESES_3]
           << " counterfactual causal hypotheses propose specific interventions with predicted outcomes. "
           << "Design experiments to validate or falsify the proposed causal claims.\n\n";
    }
    if (counts[InsightType::META_PATH] > 0) {
        ss << "41. **Validate Meta-Path Links**: The " << counts[InsightType::META_PATH]
           << " meta-path links connect entities through typed relation sequences. "
           << "Validate higher-confidence candidates for knowledge graph augmentation.\n\n";
    }
    if (counts[InsightType::CAUSAL_CHAIN] > 0) {
        ss << "42. **Verify Causal Chains**: The " << counts[InsightType::CAUSAL_CHAIN]
           << " causal chains trace directed paths via causal relation types. "
           << "Prioritize chains with strong supporting evidence for mechanistic validation.\n\n";
    }
    if (counts[InsightType::INTERVENTION_POINT] > 0) {
        ss << "43. **Target Intervention Points**: The " << counts[InsightType::INTERVENTION_POINT]
           << " intervention points are nodes whose removal disconnects causal chains. "
           << "These are high-value targets for experimental design.\n\n";
    }
    if (counts[InsightType::FEEDBACK_LOOP] > 0) {
        ss << "44. **Examine Feedback Loops**: The " << counts[InsightType::FEEDBACK_LOOP]
           << " feedback loops expose cycles in causal or directional relations. "
           << "These may indicate self-reinforcing mechanisms worth modelling explicitly.\n\n";
    }
    if (counts[InsightType::CONFOUNDER] > 0) {
        ss << "45. **Control for Confounders**: The " << counts[InsightType::CONFOUNDER]
           << " identified confounders connect to both ends of suspected causal relations. "
           << "Account for these in experimental or statistical analyses.\n\n";
    }
    if (counts[InsightType::TAXONOMY] > 0) {
        ss << "46. **Refine Taxonomic Structure**: The " << counts[InsightType::TAXONOMY]
           << " taxonomic relations surface is-a and part-of hierarchies. "
           << "Use them to improve ontological consistency and support classification tasks.\n\n";
    }
    if (counts[InsightType::DOMAIN_BRIDGE] > 0) {
        ss << "47. **Leverage Domain Bridges**: The " << counts[InsightType::DOMAIN_BRIDGE]
           << " domain bridge nodes connect distinct disciplines. "
           << "These are prime candidates for interdisciplinary hypotheses.\n\n";
    }
    if (counts[InsightType::LOGICAL_ENTAILMENT] > 0) {
        ss << "48. **Expand via Logical Entailments**: The " << counts[InsightType::LOGICAL_ENTAILMENT]
           << " entailments derive implicit implications from existing relations. "
           << "Validate and add confirmed entailments to enrich the knowledge graph.\n\n";
    }
    if (counts[InsightType::COMPOSITIONAL_REASONING] > 0) {
        ss << "49. **Test Compositional Relations**: The " << counts[InsightType::COMPOSITIONAL_REASONING]
           << " compositional inferences combine relation algebra to propose new links. "
           << "Verify whether composed paths reflect genuine domain knowledge.\n\n";
    }
    if (counts[InsightType::EXPLANATORY_CHAIN] > 0) {
        ss << "50. **Trace Explanatory Chains**: The " << counts[InsightType::EXPLANATORY_CHAIN]
           << " explanatory chains connect distant concepts through multi-hop paths. "
           << "Use them to build narrative explanations linking remote but related findings.\n\n";
    }
    if (counts[InsightType::SCHEMA_VIOLATION] > 0) {
        ss << "51. **Address Schema Violations**: The " << counts[InsightType::SCHEMA_VIOLATION]
           << " violations highlight entities or relations that break expected graph patterns. "
           << "Resolve these to improve data quality and consistency.\n\n";
    }
    if (counts[InsightType::TRANSITIVE_CLOSURE] > 0) {
        ss << "52. **Close Transitive Gaps**: The " << counts[InsightType::TRANSITIVE_CLOSURE]
           << " transitive closure gaps reveal links implied by transitivity but not yet recorded. "
           << "Validate and add confirmed closures to increase graph completeness.\n\n";
    }

    if (counts[InsightType::CO_AUTHORSHIP] > 0) {
        ss << "53. **Map Collaboration Networks**: The " << counts[InsightType::CO_AUTHORSHIP]
           << " co-authorship networks identify frequent collaborators and research clusters. "
           << "Use them to understand research team dynamics and identify collaboration opportunities.\n\n";
    }

    if (counts[InsightType::CITATION_IMPACT] > 0) {
        ss << "54. **Identify Influential Works**: The " << counts[InsightType::CITATION_IMPACT]
           << " citation impact rankings reveal influential authors and seminal works. "
           << "Prioritize these for deeper analysis or as entry points for domain exploration.\n\n";
    }

    if (counts[InsightType::MULTI_RESOLUTION_COMMUNITY] > 0) {
        ss << "55. **Explore Hierarchical Structure**: The " << counts[InsightType::MULTI_RESOLUTION_COMMUNITY]
           << " multi-resolution communities reveal organization at different granularities. "
           << "Use fine-grained scales for detailed analysis and coarse scales for high-level overview.\n\n";
    }

    if (counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] > 0) {
        ss << "56. **Leverage Inter-Community Bridges**: The " << counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP]
           << " bridge mappings identify entities that connect distinct communities. "
           << "Focus on high-scoring bridges to understand cross-domain knowledge transfer.\n\n";
    }

    if (counts[InsightType::META_PATTERN] > 0) {
        ss << "57. **Recognize Structural Templates**: The " << counts[InsightType::META_PATTERN]
           << " meta-patterns identify frequently recurring structural configurations. "
           << "Use them to guide targeted searches for similar patterns elsewhere in the graph.\n\n";
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
    report << generate_category_overview(insights, config);
    report << generate_augmentation_overview(config);
    report << generate_llm_examples_section(by_type, config);

    // Add sections for each insight type that has results
    report << generate_bridges_section(by_type[InsightType::BRIDGE], config);
    report << generate_completions_section(by_type[InsightType::COMPLETION], config);
    report << generate_motifs_section(by_type[InsightType::MOTIF], config);
    report << generate_substitutions_section(by_type[InsightType::SUBSTITUTION], config);
    report << generate_contradictions_section(by_type[InsightType::CONTRADICTION], config);
    report << generate_entity_resolutions_section(by_type[InsightType::ENTITY_RESOLUTION], config);
    report << generate_core_periphery_section(by_type[InsightType::CORE_PERIPHERY], config);
    report << generate_text_similarity_section(by_type[InsightType::TEXT_SIMILARITY], config);
    report << generate_argument_support_section(by_type[InsightType::ARGUMENT_SUPPORT], config);
    report << generate_active_learning_section(by_type[InsightType::ACTIVE_LEARNING], config);
    report << generate_method_outcome_section(by_type[InsightType::METHOD_OUTCOME], config);
    report << generate_centrality_section(by_type[InsightType::CENTRALITY], config);
    report << generate_community_detection_section(by_type[InsightType::COMMUNITY_DETECTION], config);
    report << generate_k_core_section(by_type[InsightType::K_CORE], config);
    report << generate_k_truss_section(by_type[InsightType::K_TRUSS], config);
    report << generate_claim_stance_section(by_type[InsightType::CLAIM_STANCE], config);
    report << generate_relation_induction_section(by_type[InsightType::RELATION_INDUCTION], config);
    report << generate_analogical_transfer_section(by_type[InsightType::ANALOGICAL_TRANSFER], config);
    report << generate_uncertainty_sampling_section(by_type[InsightType::UNCERTAINTY_SAMPLING], config);
    report << generate_counterfactual_section(by_type[InsightType::COUNTERFACTUAL], config);
    report << generate_hyperedge_prediction_section(by_type[InsightType::HYPEREDGE_PREDICTION], config);
    // Constrained rules removed from pipeline
    report << generate_surprise_section(by_type[InsightType::SURPRISE], config);
    report << generate_diffusion_section(by_type[InsightType::DIFFUSION], config);
    report << generate_community_links_section(by_type[InsightType::COMMUNITY_LINK], config);
    report << generate_path_rank_section(by_type[InsightType::PATH_RANK], config);
    report << generate_long_chain_section(by_type[InsightType::LONG_CHAIN], config);
    report << generate_meta_path_patterns_section(by_type[InsightType::META_PATH_PATTERN], config);
    report << generate_meta_path_section(by_type[InsightType::META_PATH], config);
    report << generate_author_chains_section(by_type[InsightType::AUTHOR_CHAIN], config);

    // Hypothesis Generation (3 types)
    report << generate_hypotheses_section(
        by_type[InsightType::HYPOTHESES_1],
        config,
        "Bayesian Hypothesis Network (Hypotheses_1)",
        "Hypotheses_1 uses probabilistic belief propagation to infer likely missing relationships. "
        "It constructs a Bayesian network from insights, estimates conditional probabilities, propagates beliefs iteratively, "
        "and identifies high-posterior relationships with quantified uncertainty and confidence intervals.");
    report << generate_hypotheses_section(
        by_type[InsightType::HYPOTHESES_2],
        config,
        "Typed Mechanistic Chains (Hypotheses_2)",
        "Hypotheses_2 discovers typed mechanistic chains that match domain templates (e.g., Driver→Hazard→Impact) "
        "and augments them with compact explanation subgraphs. It emphasizes role-labeled pathways and evidence-aligned "
        "supporting structure rather than timestamp ordering.");
    report << generate_hypotheses_section(
        by_type[InsightType::HYPOTHESES_3],
        config,
        "Counterfactual Causal Hypotheses (Hypotheses_3)",
        "Hypotheses_3 generates testable counterfactual hypotheses using causal infrastructure. "
        "Each hypothesis proposes an intervention, predicts downstream effects, explains the mechanism, and suggests test plans.");

    report << generate_mechanism_consolidation_section(by_type[InsightType::MECHANISM_CONSOLIDATION], config);
    report << generate_evidence_fusion_section(by_type[InsightType::EVIDENCE_FUSION_LINK], config);
    report << generate_meta_path_anomaly_section(by_type[InsightType::META_PATH_ANOMALY], config);
    report << generate_intervention_bottleneck_section(by_type[InsightType::INTERVENTION_BOTTLENECK], config);
    report << generate_competing_mechanism_section(by_type[InsightType::COMPETING_MECHANISM], config);
    report << generate_schema_repair_section(by_type[InsightType::SCHEMA_REPAIR], config);
    report << generate_cross_community_bridge_section(by_type[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE], config);
    report << generate_rules_section(by_type[InsightType::RULE], config);
    report << generate_embedding_links_section(by_type[InsightType::EMBEDDING_LINK], config);

    // Causal & Mechanistic Discovery
    report << generate_causal_chains_section(by_type[InsightType::CAUSAL_CHAIN], config);
    report << generate_intervention_points_section(by_type[InsightType::INTERVENTION_POINT], config);
    report << generate_feedback_loops_section(by_type[InsightType::FEEDBACK_LOOP], config);
    report << generate_confounders_section(by_type[InsightType::CONFOUNDER], config);

    // Advanced Reasoning & Structural Discovery
    report << generate_taxonomy_section(by_type[InsightType::TAXONOMY], config);
    report << generate_domain_bridges_section(by_type[InsightType::DOMAIN_BRIDGE], config);
    report << generate_logical_entailments_section(by_type[InsightType::LOGICAL_ENTAILMENT], config);
    report << generate_compositional_reasoning_section(by_type[InsightType::COMPOSITIONAL_REASONING], config);
    report << generate_explanatory_chains_section(by_type[InsightType::EXPLANATORY_CHAIN], config);
    report << generate_schema_violations_section(by_type[InsightType::SCHEMA_VIOLATION], config);
    report << generate_transitive_closure_section(by_type[InsightType::TRANSITIVE_CLOSURE], config);

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

// ============================================================================
// Dynamic Threshold Helper for HTML Report Generation
// ============================================================================

// Helper to calculate dynamic limits based on actual data
struct DynamicLimits {
    int max_clusters_to_show;
    int max_items_per_cluster;
    bool should_cluster;
    double similarity_threshold;
};

static DynamicLimits calculate_dynamic_limits(size_t insight_count, const ReportConfig& config) {
    DynamicLimits limits;
    const int max_examples = capped_max_examples(config);

    // Respect per-operator display cap (hard-capped at 10 in capped_max_examples).
    limits.max_clusters_to_show = std::min(max_examples, static_cast<int>(insight_count));
    limits.max_items_per_cluster = std::min(config.max_items_per_cluster, max_examples);

    // Only enable clustering if:
    // 1. User hasn't disabled it
    // 2. There are enough insights to benefit from clustering (> 15)
    // 3. Not too many (< 200, where we want to see everything)
    limits.should_cluster = config.coalesce_similar_findings
                          && insight_count > 15
                          && insight_count < 200;

    // Dynamic similarity threshold: more aggressive only with many insights
    if (insight_count < 20) {
        limits.similarity_threshold = 0.95;  // Very strict - almost no clustering
    } else if (insight_count < 50) {
        limits.similarity_threshold = 0.85;  // Moderate
    } else {
        limits.similarity_threshold = config.similarity_threshold;  // Use configured (0.7)
    }

    // If clustering is disabled, show all insights individually
    if (!limits.should_cluster) {
        limits.max_clusters_to_show = std::min(max_examples, static_cast<int>(insight_count));
        limits.max_items_per_cluster = 1;  // Each insight gets its own display
    }

    return limits;
}

std::string ReportGenerator::generate_html(const InsightCollection& insights, const ReportConfig& config) {
    std::stringstream html;

    auto has_novelty_tag = [](const Insight& insight, const std::string& tag) {
        return std::find(insight.novelty_tags.begin(), insight.novelty_tags.end(), tag) != insight.novelty_tags.end();
    };

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

    // Split out intersection-constrained hypotheses from generic H2 chains.
    std::vector<Insight> intersection_bridge_hypotheses;
    std::vector<Insight> regular_h2_hypotheses;
    for (const auto& insight : by_type[InsightType::HYPOTHESES_2]) {
        if (has_novelty_tag(insight, "intersection_bridge")) {
            intersection_bridge_hypotheses.push_back(insight);
        } else {
            regular_h2_hypotheses.push_back(insight);
        }
    }
    const int intersection_bridge_count = static_cast<int>(intersection_bridge_hypotheses.size());
    const int regular_h2_count = static_cast<int>(regular_h2_hypotheses.size());

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
            --theme-longchain: #a3e635;
            --theme-metapath: #38bdf8;
            --theme-community: #f97316;
            --theme-hypothesis: #22d3ee;
            --theme-mechanism: #22d3ee;
            --theme-intersection-bridge: #06b6d4;
            --theme-fusion: #06b6d4;
            --theme-anomaly: #fb7185;
            --theme-bottleneck: #f97316;
            --theme-competing: #8b5cf6;
            --theme-repair: #10b981;
            --theme-crosscommunity: #0ea5e9;
            --theme-author: #facc15;
            --theme-contradiction: #f87171;
            --theme-resolution: #34d399;
            --theme-core: #60a5fa;
            --theme-text: #a78bfa;
            --theme-argument: #38bdf8;
            --theme-active: #f59e0b;
            --theme-method: #10b981;
            --theme-centrality: #14b8a6;
            --theme-community-detect: #3b82f6;
            --theme-k-core: #f59e0b;
            --theme-k-truss: #84cc16;
            --theme-claim: #ec4899;
            --theme-relation: #0ea5e9;
            --theme-analogy: #a855f7;
            --theme-uncertainty: #f97316;
            --theme-counterfactual: #64748b;
            --theme-hyperedge: #22c55e;
            --theme-constrained: #eab308;
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
        .card.longchain .value { color: var(--theme-longchain); }
        .card.metapathpattern .value { color: var(--theme-metapath); }
        .card.rules .value { color: var(--theme-rule); }
        .card.embedding .value { color: var(--secondary); }
        .card.hypothesis .value { color: var(--theme-hypothesis); }
        .card.mechanism .value { color: var(--theme-mechanism); }
        .card.intersectionbridge .value { color: var(--theme-intersection-bridge); }
        .card.fusion .value { color: var(--theme-fusion); }
        .card.anomaly .value { color: var(--theme-anomaly); }
        .card.bottleneck .value { color: var(--theme-bottleneck); }
        .card.competing .value { color: var(--theme-competing); }
        .card.repair .value { color: var(--theme-repair); }
        .card.crosscommunity .value { color: var(--theme-crosscommunity); }
        .card.authorchain .value { color: var(--theme-author); }
        .card.coauthorship .value { color: var(--theme-author); }
        .card.citations .value { color: var(--theme-author); }
        .card.contradiction .value { color: var(--theme-contradiction); }
        .card.resolution .value { color: var(--theme-resolution); }
        .card.coreperiphery .value { color: var(--theme-core); }
        .card.textsimilarity .value { color: var(--theme-text); }
        .card.argumentsupport .value { color: var(--theme-argument); }
        .card.activelearning .value { color: var(--theme-active); }
        .card.methodoutcome .value { color: var(--theme-method); }
        .card.centrality .value { color: var(--theme-centrality); }
        .card.communitydetect .value { color: var(--theme-community-detect); }
        .card.kcore .value { color: var(--theme-k-core); }
        .card.ktruss .value { color: var(--theme-k-truss); }
        .card.claimstance .value { color: var(--theme-claim); }
        .card.relationinduction .value { color: var(--theme-relation); }
        .card.analogical .value { color: var(--theme-analogy); }
        .card.uncertainty .value { color: var(--theme-uncertainty); }
        .card.counterfactual .value { color: var(--theme-counterfactual); }
        .card.hyperedge .value { color: var(--theme-hyperedge); }
        .card.constrainedrule .value { color: var(--theme-constrained); }

        /* Category overview styles */
        .categories-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 24px;
            margin: 30px 0;
        }
        .category-card {
            background: rgba(15, 23, 42, 0.6);
            border-radius: 12px;
            padding: 24px;
            border: 2px solid var(--border);
            transition: all 0.3s ease;
        }
        .category-card:hover {
            border-color: rgba(79, 195, 247, 0.4);
            transform: translateY(-4px);
        }
        .category-card.combinatorial {
            border-left: 4px solid #4ade80;
        }
        .category-card.exploratory {
            border-left: 4px solid #4fc3f7;
        }
        .category-card.transformational {
            border-left: 4px solid #e879f9;
        }
        .category-card .icon {
            font-size: 2.5em;
            margin-bottom: 12px;
        }
        .category-card h3 {
            color: var(--primary);
            font-size: 1.3em;
            margin-bottom: 8px;
        }
        .category-card.combinatorial h3 { color: #4ade80; }
        .category-card.exploratory h3 { color: #4fc3f7; }
        .category-card.transformational h3 { color: #e879f9; }
        .category-card .subtitle {
            color: var(--text-muted);
            font-size: 0.9em;
            margin-bottom: 16px;
            font-weight: 500;
        }
        .category-card .description {
            color: var(--text-muted);
            font-size: 0.9em;
            line-height: 1.6;
            margin-bottom: 20px;
        }
        .category-card .stats {
            display: flex;
            justify-content: space-between;
            padding-top: 16px;
            border-top: 1px solid var(--border);
        }
        .category-card .stat-item {
            text-align: center;
        }
        .category-card .stat-value {
            font-size: 2em;
            font-weight: 700;
            margin-bottom: 4px;
        }
        .category-card.combinatorial .stat-value { color: #4ade80; }
        .category-card.exploratory .stat-value { color: #4fc3f7; }
        .category-card.transformational .stat-value { color: #e879f9; }
        .category-card .stat-label {
            font-size: 0.75em;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 1px;
        }

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
        .insight.contradiction { border-left-color: var(--theme-contradiction); }
        .insight.entity-resolution { border-left-color: var(--theme-resolution); }
        .insight.core-periphery { border-left-color: var(--theme-core); }
        .insight.text-similarity { border-left-color: var(--theme-text); }
        .insight.argument-support { border-left-color: var(--theme-argument); }
        .insight.active-learning { border-left-color: var(--theme-active); }
        .insight.method-outcome { border-left-color: var(--theme-method); }
        .insight h4 {
            color: var(--primary);
            margin-bottom: 10px;
            font-size: 1.1em;
        }
        .insight.surprise h4 { color: var(--accent); }
        .insight.contradiction h4 { color: var(--theme-contradiction); }
        .insight.entity-resolution h4 { color: var(--theme-resolution); }
        .insight.core-periphery h4 { color: var(--theme-core); }
        .insight.text-similarity h4 { color: var(--theme-text); }
        .insight.argument-support h4 { color: var(--theme-argument); }
        .insight.active-learning h4 { color: var(--theme-active); }
        .insight.method-outcome h4 { color: var(--theme-method); }
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
        .toc .category-toc > a {
            font-weight: 600;
            font-size: 1.05em;
            display: block;
            margin: 12px 0 8px 0;
            color: var(--primary);
        }
        .toc .sub-toc {
            margin-left: 20px;
            margin-top: 8px;
        }
        .toc .sub-toc li {
            margin: 6px 0;
        }

        /* Category section headers */
        .category-section-header {
            margin: 50px 0 30px 0;
            padding: 30px;
            background: linear-gradient(135deg, rgba(99, 102, 241, 0.1) 0%, rgba(168, 85, 247, 0.1) 100%);
            border-radius: 16px;
            border-left: 5px solid var(--primary);
        }
        .category-section-header h2 {
            margin: 0 0 10px 0;
            font-size: 2em;
            color: var(--primary);
        }
        .category-section-header p {
            margin: 0;
            color: var(--text-muted);
            font-size: 1.1em;
        }

        /* Interactive features CSS */
        .search-controls {
            margin: 25px 0;
            padding: 20px;
            background: rgba(0,0,0,0.3);
            border-radius: 12px;
            border: 1px solid var(--border);
        }
        .search-controls input[type="text"] {
            width: 100%;
            padding: 12px 16px;
            background: rgba(0,0,0,0.4);
            border: 1px solid var(--border);
            border-radius: 8px;
            color: var(--text);
            font-size: 1em;
            font-family: inherit;
            transition: border-color 0.2s;
        }
        .search-controls input[type="text"]:focus {
            outline: none;
            border-color: var(--primary);
        }
        .filter-group {
            margin-top: 15px;
            display: flex;
            gap: 15px;
            flex-wrap: wrap;
            align-items: center;
        }
        .filter-group label {
            cursor: pointer;
            padding: 6px 12px;
            background: rgba(79, 195, 247, 0.1);
            border-radius: 6px;
            transition: background 0.2s;
            display: flex;
            align-items: center;
            gap: 6px;
        }
        .filter-group label:hover {
            background: rgba(79, 195, 247, 0.2);
        }
        .filter-group input[type="checkbox"] {
            cursor: pointer;
        }
        .highlighted {
            background: rgba(251, 191, 36, 0.3) !important;
            box-shadow: 0 0 0 2px var(--theme-gap);
            animation: pulse 0.5s ease-in-out;
        }
        @keyframes pulse {
            0%, 100% { transform: scale(1); }
            50% { transform: scale(1.02); }
        }
        .entity, .data-table td:first-child {
            cursor: pointer;
            transition: background 0.15s;
        }
        .entity:hover, .data-table td:first-child:hover {
            background: rgba(79, 195, 247, 0.15);
        }
        section h2 {
            cursor: pointer;
            user-select: none;
            position: relative;
            padding-left: 30px;
        }
        section h2::before {
            content: '▼';
            position: absolute;
            left: 0;
            transition: transform 0.2s;
        }
        section.collapsed h2::before {
            transform: rotate(-90deg);
        }
        .export-btn {
            position: fixed;
            bottom: 20px;
            right: 20px;
            padding: 12px 24px;
            background: var(--primary);
            color: var(--bg-dark);
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 600;
            font-size: 0.95em;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 1000;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        .export-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 16px rgba(0,0,0,0.4);
        }
        .load-more-btn {
            margin: 15px auto;
            display: block;
            padding: 10px 20px;
            background: var(--primary);
            color: var(--bg-dark);
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-weight: 600;
            transition: background 0.2s;
        }
        .load-more-btn:hover {
            background: rgba(79, 195, 247, 0.8);
        }
        .chart-container {
            max-width: 700px;
            min-height: 250px;
            margin: 30px auto;
            padding: 25px;
            background: rgba(0,0,0,0.2);
            border-radius: 12px;
            border: 1px solid var(--border);
        }
        @media print {
            .search-controls, .export-btn, .load-more-btn, .chart-container {
                display: none !important;
            }
            section.collapsed > *:not(h2) {
                display: block !important;
            }
            section h2::before {
                content: '' !important;
            }
        }

        /* Phase 2: Causal Metadata Badges */
        .causal-badges {
            display: inline-flex;
            gap: 6px;
            flex-wrap: wrap;
            align-items: center;
            margin-left: 8px;
        }
        .badge {
            display: inline-block;
            padding: 3px 8px;
            border-radius: 4px;
            font-size: 0.75em;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            white-space: nowrap;
        }
        /* Strength badges */
        .badge-strength-deterministic {
            background: rgba(34, 197, 94, 0.25);
            color: #22c55e;
            border: 1px solid rgba(34, 197, 94, 0.4);
        }
        .badge-strength-strong {
            background: rgba(132, 204, 22, 0.25);
            color: #84cc16;
            border: 1px solid rgba(132, 204, 22, 0.4);
        }
        .badge-strength-moderate {
            background: rgba(251, 191, 36, 0.25);
            color: #fbbf24;
            border: 1px solid rgba(251, 191, 36, 0.4);
        }
        .badge-strength-weak {
            background: rgba(248, 113, 113, 0.25);
            color: #f87171;
            border: 1px solid rgba(248, 113, 113, 0.4);
        }
        /* Type badges */
        .badge-type {
            background: rgba(79, 195, 247, 0.25);
            color: #4fc3f7;
            border: 1px solid rgba(79, 195, 247, 0.4);
        }
        .badge-type-necessary {
            background: rgba(220, 38, 38, 0.25);
            color: #ff6b6b;
            border: 1px solid rgba(220, 38, 38, 0.4);
        }
        .badge-type-sufficient {
            background: rgba(168, 85, 247, 0.25);
            color: #a855f7;
            border: 1px solid rgba(168, 85, 247, 0.4);
        }
        /* Temporality badges */
        .badge-temporality {
            background: rgba(148, 163, 184, 0.25);
            color: #94a3b8;
            border: 1px solid rgba(148, 163, 184, 0.4);
        }
        .badge-immediate {
            background: rgba(34, 211, 238, 0.25);
            color: #22d3ee;
            border: 1px solid rgba(34, 211, 238, 0.4);
        }
        .badge-delayed {
            background: rgba(251, 146, 60, 0.25);
            color: #fb923c;
            border: 1px solid rgba(251, 146, 60, 0.4);
        }
        /* Mechanism badge */
        .badge-mechanism {
            background: rgba(139, 92, 246, 0.25);
            color: #8b5cf6;
            border: 1px solid rgba(139, 92, 246, 0.4);
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
)";

    // V2: Add deduplication statistics if available
    if (!config.pipeline_stats.empty() && config.pipeline_stats.contains("relations_before_dedup")) {
        html << R"(
        <section id="v2-dedup-stats" style="margin-top: 30px;">
            <h2 style="margin-bottom: 15px;">V2 Extraction Statistics</h2>
            <p style="color: var(--text-muted); margin-bottom: 20px; font-size: 0.95em;">
                This pipeline uses overlapping chunk extraction with automatic deduplication to improve boundary recall.
            </p>
            <div class="stats-bar">
                <div class="stat">
                    <div class="label">Relations Before Dedup</div>
                    <div class="value">)" << config.pipeline_stats.value("relations_before_dedup", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Relations After Dedup</div>
                    <div class="value">)" << config.pipeline_stats.value("relations_after_dedup", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Duplicates Merged</div>
                    <div class="value">)" << config.pipeline_stats.value("duplicate_relations_merged", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Multi-Provenance</div>
                    <div class="value">)" << config.pipeline_stats.value("multi_provenance_relations", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Avg Confidence Boost</div>
                    <div class="value">)" << std::fixed << std::setprecision(3) << config.pipeline_stats.value("avg_confidence_boost", 0.0) << R"(</div>
                </div>
            </div>
        </section>
)";
    }

    // Add QC stats if available
    if (!config.pipeline_stats.empty() && config.pipeline_stats.contains("quality_control")) {
        auto qc = config.pipeline_stats["quality_control"];
        html << R"(
        <section id="qc-stats" style="margin-top: 30px;">
            <h2 style="margin-bottom: 15px;">Quality Control Statistics</h2>
            <p style="color: var(--text-muted); margin-bottom: 20px; font-size: 0.95em;">
                3-level validation system removes noise, artifacts, and invalid entities to improve graph quality.
            </p>
            <div class="stats-bar">
                <div class="stat">
                    <div class="label">Initial Entities</div>
                    <div class="value">)" << qc.value("initial_nodes", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Final Entities</div>
                    <div class="value">)" << qc.value("final_nodes", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Removed</div>
                    <div class="value">)" << qc.value("total_removed", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Removal Rate</div>
                    <div class="value">)" << std::fixed << std::setprecision(1)
            << (qc.value("initial_nodes", 0) > 0 ? 100.0 * qc.value("total_removed", 0) / qc.value("initial_nodes", 1) : 0.0) << R"(%</div>
                </div>
            </div>
)";

        if (qc.contains("connectivity")) {
            auto conn = qc["connectivity"];
            html << R"(
            <h3 style="margin-top: 25px; margin-bottom: 15px;">Graph Connectivity</h3>
            <div class="stats-bar">
                <div class="stat">
                    <div class="label">Connected Components</div>
                    <div class="value">)" << conn.value("num_connected_components", 0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Largest Component</div>
                    <div class="value">)" << conn.value("largest_component_size", 0)
                << " (" << std::fixed << std::setprecision(1)
                << (qc.value("final_nodes", 0) > 0 ? 100.0 * conn.value("largest_component_size", 0) / qc.value("final_nodes", 1) : 0.0) << R"(%)</div>
                </div>
                <div class="stat">
                    <div class="label">Graph Density</div>
                    <div class="value">)" << std::fixed << std::setprecision(4) << conn.value("graph_density", 0.0) << R"(</div>
                </div>
                <div class="stat">
                    <div class="label">Clustering Coeff</div>
                    <div class="value">)" << std::fixed << std::setprecision(3) << conn.value("clustering_coefficient", 0.0) << R"(</div>
                </div>
            </div>
)";
        }

        html << R"(
        </section>
)";
    }

    html << R"(
        <section id="categories">
            <h2>Knowledge Discovery Categories</h2>
            <p style="color: var(--text-muted); margin-bottom: 15px;">
                Insights are classified into three categories based on how they generate new knowledge from the graph structure.
            </p>
            <p style="color: var(--text-muted); margin-bottom: 30px; font-size: 0.95em;">
                <strong>Average Score</strong> indicates the mean confidence/quality score across all insights in each category,
                ranging from 0.0 (low confidence) to 1.0 (high confidence). Higher scores suggest more reliable or significant discoveries.
            </p>

            <div class="categories-grid">
)";

    // Count by category
    std::map<InsightCategory, int> category_counts;
    std::map<InsightCategory, double> category_avg_scores;

    for (const auto& insight : insights.insights) {
        category_counts[insight.category]++;
        category_avg_scores[insight.category] += insight.score;
    }

    // Calculate averages
    for (auto& [cat, total_score] : category_avg_scores) {
        if (category_counts[cat] > 0) {
            total_score /= category_counts[cat];
        }
    }

    // Combinatorial card
    html << R"(
                <div class="category-card combinatorial">
                    <div class="icon">🧩</div>
                    <h3>Combinatorial</h3>
                    <div class="subtitle">Pattern Detection & Structural Combinations</div>
                    <div class="description">
                        These insights emerge from combining existing knowledge in new ways,
                        identifying patterns, structures, and motifs that reveal the underlying
                        organization of the knowledge graph.
                    </div>
                    <div class="stats">
                        <div class="stat-item">
                            <div class="stat-value">)" << category_counts[InsightCategory::COMBINATORIAL] << R"(</div>
                            <div class="stat-label">Insights</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">)" << std::fixed << std::setprecision(2) << category_avg_scores[InsightCategory::COMBINATORIAL] << R"(</div>
                            <div class="stat-label">Avg Score</div>
                        </div>
                    </div>
                </div>
)";

    // Exploratory card
    html << R"(
                <div class="category-card exploratory">
                    <div class="icon">🔍</div>
                    <h3>Exploratory</h3>
                    <div class="subtitle">Path Finding & Connection Discovery</div>
                    <div class="description">
                        These insights discover new connections, paths, and bridges between concepts,
                        exploring previously unexplored areas of the knowledge space and revealing
                        hidden relationships.
                    </div>
                    <div class="stats">
                        <div class="stat-item">
                            <div class="stat-value">)" << category_counts[InsightCategory::EXPLORATORY] << R"(</div>
                            <div class="stat-label">Insights</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">)" << std::fixed << std::setprecision(2) << category_avg_scores[InsightCategory::EXPLORATORY] << R"(</div>
                            <div class="stat-label">Avg Score</div>
                        </div>
                    </div>
                </div>
)";

    // Transformational card
    html << R"(
                <div class="category-card transformational">
                    <div class="icon">🔄</div>
                    <h3>Transformational</h3>
                    <div class="subtitle">Reframing & Perspective Shifts</div>
                    <div class="description">
                        These insights transform understanding by reframing concepts, generating
                        hypotheses, identifying contradictions, and suggesting alternative causal
                        mechanisms or paradigms.
                    </div>
                    <div class="stats">
                        <div class="stat-item">
                            <div class="stat-value">)" << category_counts[InsightCategory::TRANSFORMATIONAL] << R"(</div>
                            <div class="stat-label">Insights</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">)" << std::fixed << std::setprecision(2) << category_avg_scores[InsightCategory::TRANSFORMATIONAL] << R"(</div>
                            <div class="stat-label">Avg Score</div>
                        </div>
                    </div>
                </div>
)";

    html << R"(
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

    if (counts[InsightType::CONTRADICTION] > 0) {
        html << R"(                <a class="card-link" href="#module-contradictions">
                    <div class="card contradiction">
                        <h3>Contradictions</h3>
                        <div class="value">)" << counts[InsightType::CONTRADICTION] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::ENTITY_RESOLUTION] > 0) {
        html << R"(                <a class="card-link" href="#module-entity-resolution">
                    <div class="card resolution">
                        <h3>Entity Resolutions</h3>
                        <div class="value">)" << counts[InsightType::ENTITY_RESOLUTION] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::CORE_PERIPHERY] > 0) {
        html << R"(                <a class="card-link" href="#module-core-periphery">
                    <div class="card coreperiphery">
                        <h3>Core–Periphery</h3>
                        <div class="value">)" << counts[InsightType::CORE_PERIPHERY] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::TEXT_SIMILARITY] > 0) {
        html << R"(                <a class="card-link" href="#module-text-similarity">
                    <div class="card textsimilarity">
                        <h3>Text Similarity</h3>
                        <div class="value">)" << counts[InsightType::TEXT_SIMILARITY] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::ARGUMENT_SUPPORT] > 0) {
        html << R"(                <a class="card-link" href="#module-argument-support">
                    <div class="card argumentsupport">
                        <h3>Argument Support</h3>
                        <div class="value">)" << counts[InsightType::ARGUMENT_SUPPORT] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::ACTIVE_LEARNING] > 0) {
        html << R"(                <a class="card-link" href="#module-active-learning">
                    <div class="card activelearning">
                        <h3>Active Learning</h3>
                        <div class="value">)" << counts[InsightType::ACTIVE_LEARNING] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::METHOD_OUTCOME] > 0) {
        html << R"(                <a class="card-link" href="#module-method-outcome">
                    <div class="card methodoutcome">
                        <h3>Method/Outcome</h3>
                        <div class="value">)" << counts[InsightType::METHOD_OUTCOME] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::CENTRALITY] > 0) {
        html << R"(                <a class="card-link" href="#module-centrality">
                    <div class="card centrality">
                        <h3>Centrality</h3>
                        <div class="value">)" << counts[InsightType::CENTRALITY] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::COMMUNITY_DETECTION] > 0) {
        html << R"(                <a class="card-link" href="#module-community-detection">
                    <div class="card communitydetect">
                        <h3>Communities</h3>
                        <div class="value">)" << counts[InsightType::COMMUNITY_DETECTION] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::K_CORE] > 0) {
        html << R"(                <a class="card-link" href="#module-k-core">
                    <div class="card kcore">
                        <h3>k-Core</h3>
                        <div class="value">)" << counts[InsightType::K_CORE] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::K_TRUSS] > 0) {
        html << R"(                <a class="card-link" href="#module-k-truss">
                    <div class="card ktruss">
                        <h3>k-Truss</h3>
                        <div class="value">)" << counts[InsightType::K_TRUSS] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::CLAIM_STANCE] > 0) {
        html << R"(                <a class="card-link" href="#module-claim-stance">
                    <div class="card claimstance">
                        <h3>Claim Stance</h3>
                        <div class="value">)" << counts[InsightType::CLAIM_STANCE] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::RELATION_INDUCTION] > 0) {
        html << R"(                <a class="card-link" href="#module-relation-induction">
                    <div class="card relationinduction">
                        <h3>Relation Induction</h3>
                        <div class="value">)" << counts[InsightType::RELATION_INDUCTION] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::ANALOGICAL_TRANSFER] > 0) {
        html << R"(                <a class="card-link" href="#module-analogical-transfer">
                    <div class="card analogical">
                        <h3>Analogical Transfer</h3>
                        <div class="value">)" << counts[InsightType::ANALOGICAL_TRANSFER] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::UNCERTAINTY_SAMPLING] > 0) {
        html << R"(                <a class="card-link" href="#module-uncertainty-sampling">
                    <div class="card uncertainty">
                        <h3>Uncertainty</h3>
                        <div class="value">)" << counts[InsightType::UNCERTAINTY_SAMPLING] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::COUNTERFACTUAL] > 0) {
        html << R"(                <a class="card-link" href="#module-counterfactual">
                    <div class="card counterfactual">
                        <h3>Counterfactual</h3>
                        <div class="value">)" << counts[InsightType::COUNTERFACTUAL] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::HYPEREDGE_PREDICTION] > 0) {
        html << R"(                <a class="card-link" href="#module-hyperedge-prediction">
                    <div class="card hyperedge">
                        <h3>Hyperedge Prediction</h3>
                        <div class="value">)" << counts[InsightType::HYPEREDGE_PREDICTION] << R"(</div>
                    </div>
                </a>
)";
    }

    // Constrained rules removed from pipeline

    if (counts[InsightType::COMMUNITY_LINK] > 0) {
        html << R"(                <a class="card-link" href="#module-community">
                    <div class="card community">
                        <h3>Community Links</h3>
                        <div class="value">)" << counts[InsightType::COMMUNITY_LINK] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::HYPOTHESES_1] > 0) {
        html << R"(                <a class="card-link" href="#module-hypotheses">
                    <div class="card hypothesis">
                        <h3>Hypotheses (H1)</h3>
                        <div class="value">)" << counts[InsightType::HYPOTHESES_1] << R"(</div>
                    </div>
                </a>
)";
    }

    if (intersection_bridge_count > 0) {
        html << R"(                <a class="card-link" href="#module-intersection-bridge">
                    <div class="card intersectionbridge">
                        <h3>Intersection Bridges</h3>
                        <div class="value">)" << intersection_bridge_count << R"(</div>
                    </div>
                </a>
)";
    }

    if (regular_h2_count > 0) {
        html << R"(                <a class="card-link" href="#module-hypotheses-2">
                    <div class="card mechanism">
                        <h3>Mechanistic (H2)</h3>
                        <div class="value">)" << regular_h2_count << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::HYPOTHESES_3] > 0) {
        html << R"(                <a class="card-link" href="#module-hypotheses-3">
                    <div class="card counterfactual">
                        <h3>Causal (H3)</h3>
                        <div class="value">)" << counts[InsightType::HYPOTHESES_3] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::MECHANISM_CONSOLIDATION] > 0) {
        html << R"(                <a class="card-link" href="#module-mechanism-consolidation">
                    <div class="card mechanism">
                        <h3>Mechanism Consolidation</h3>
                        <div class="value">)" << counts[InsightType::MECHANISM_CONSOLIDATION] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::EVIDENCE_FUSION_LINK] > 0) {
        html << R"(                <a class="card-link" href="#module-evidence-fusion">
                    <div class="card fusion">
                        <h3>Evidence Fusion</h3>
                        <div class="value">)" << counts[InsightType::EVIDENCE_FUSION_LINK] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::META_PATH_ANOMALY] > 0) {
        html << R"(                <a class="card-link" href="#module-meta-path-anomaly">
                    <div class="card anomaly">
                        <h3>Meta-Path Anomaly</h3>
                        <div class="value">)" << counts[InsightType::META_PATH_ANOMALY] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::INTERVENTION_BOTTLENECK] > 0) {
        html << R"(                <a class="card-link" href="#module-intervention-bottleneck">
                    <div class="card bottleneck">
                        <h3>Bottlenecks</h3>
                        <div class="value">)" << counts[InsightType::INTERVENTION_BOTTLENECK] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::COMPETING_MECHANISM] > 0) {
        html << R"(                <a class="card-link" href="#module-competing-mechanism">
                    <div class="card competing">
                        <h3>Competing Mechanisms</h3>
                        <div class="value">)" << counts[InsightType::COMPETING_MECHANISM] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::SCHEMA_REPAIR] > 0) {
        html << R"(                <a class="card-link" href="#module-schema-repair">
                    <div class="card repair">
                        <h3>Schema Repairs</h3>
                        <div class="value">)" << counts[InsightType::SCHEMA_REPAIR] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] > 0) {
        html << R"(                <a class="card-link" href="#module-cross-community-bridge">
                    <div class="card crosscommunity">
                        <h3>Cross-Community Bridges</h3>
                        <div class="value">)" << counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] << R"(</div>
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

    if (counts[InsightType::LONG_CHAIN] > 0) {
        html << R"(                <a class="card-link" href="#long-chain">
                    <div class="card longchain">
                        <h3>Long-Chain Links</h3>
                        <div class="value">)" << counts[InsightType::LONG_CHAIN] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::META_PATH_PATTERN] > 0) {
        html << R"(                <a class="card-link" href="#meta-path-patterns">
                    <div class="card metapathpattern">
                        <h3>Meta-Path Patterns</h3>
                        <div class="value">)" << counts[InsightType::META_PATH_PATTERN] << R"(</div>
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

    if (counts[InsightType::CO_AUTHORSHIP] > 0) {
        html << R"(                <a class="card-link" href="#module-co-authorship">
                    <div class="card coauthorship">
                        <h3>Co-Authorships</h3>
                        <div class="value">)" << counts[InsightType::CO_AUTHORSHIP] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::CITATION_IMPACT] > 0) {
        html << R"(                <a class="card-link" href="#module-citation-impact">
                    <div class="card citations">
                        <h3>Citation Impact</h3>
                        <div class="value">)" << counts[InsightType::CITATION_IMPACT] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::MULTI_RESOLUTION_COMMUNITY] > 0) {
        html << R"(                <a class="card-link" href="#module-multi-resolution-community">
                    <div class="card communitydetect">
                        <h3>Multi-Res Communities</h3>
                        <div class="value">)" << counts[InsightType::MULTI_RESOLUTION_COMMUNITY] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] > 0) {
        html << R"(                <a class="card-link" href="#module-cross-community-bridge-map">
                    <div class="card crosscommunity">
                        <h3>Community Bridge Maps</h3>
                        <div class="value">)" << counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] << R"(</div>
                    </div>
                </a>
)";
    }

    if (counts[InsightType::META_PATTERN] > 0) {
        html << R"(                <a class="card-link" href="#module-meta-pattern">
                    <div class="card metapathpattern">
                        <h3>Meta-Patterns</h3>
                        <div class="value">)" << counts[InsightType::META_PATTERN] << R"(</div>
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

    if (counts[InsightType::META_PATH] > 0) {
        html << R"(                <a class="card-link" href="#module-meta-path">
                    <div class="card meta-path">
                        <h3>Meta-Path Links</h3>
                        <div class="value">)" << counts[InsightType::META_PATH] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::CAUSAL_CHAIN] > 0) {
        html << R"(                <a class="card-link" href="#module-causal-chain">
                    <div class="card causal-chain">
                        <h3>Causal Chains</h3>
                        <div class="value">)" << counts[InsightType::CAUSAL_CHAIN] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::INTERVENTION_POINT] > 0) {
        html << R"(                <a class="card-link" href="#module-intervention-point">
                    <div class="card intervention-point">
                        <h3>Intervention Points</h3>
                        <div class="value">)" << counts[InsightType::INTERVENTION_POINT] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::FEEDBACK_LOOP] > 0) {
        html << R"(                <a class="card-link" href="#module-feedback-loop">
                    <div class="card feedback-loop">
                        <h3>Feedback Loops</h3>
                        <div class="value">)" << counts[InsightType::FEEDBACK_LOOP] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::CONFOUNDER] > 0) {
        html << R"(                <a class="card-link" href="#module-confounder">
                    <div class="card confounder">
                        <h3>Confounders</h3>
                        <div class="value">)" << counts[InsightType::CONFOUNDER] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::TAXONOMY] > 0) {
        html << R"(                <a class="card-link" href="#module-taxonomy">
                    <div class="card taxonomy">
                        <h3>Taxonomic Relations</h3>
                        <div class="value">)" << counts[InsightType::TAXONOMY] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::DOMAIN_BRIDGE] > 0) {
        html << R"(                <a class="card-link" href="#module-domain-bridge">
                    <div class="card domain-bridge">
                        <h3>Domain Bridges</h3>
                        <div class="value">)" << counts[InsightType::DOMAIN_BRIDGE] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::LOGICAL_ENTAILMENT] > 0) {
        html << R"(                <a class="card-link" href="#module-logical-entailment">
                    <div class="card logical-entailment">
                        <h3>Logical Entailments</h3>
                        <div class="value">)" << counts[InsightType::LOGICAL_ENTAILMENT] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::COMPOSITIONAL_REASONING] > 0) {
        html << R"(                <a class="card-link" href="#module-compositional">
                    <div class="card compositional">
                        <h3>Compositional Reasoning</h3>
                        <div class="value">)" << counts[InsightType::COMPOSITIONAL_REASONING] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::EXPLANATORY_CHAIN] > 0) {
        html << R"(                <a class="card-link" href="#module-explanatory-chain">
                    <div class="card explanatory-chain">
                        <h3>Explanatory Chains</h3>
                        <div class="value">)" << counts[InsightType::EXPLANATORY_CHAIN] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::SCHEMA_VIOLATION] > 0) {
        html << R"(                <a class="card-link" href="#module-schema-violation">
                    <div class="card schema-violation">
                        <h3>Schema Violations</h3>
                        <div class="value">)" << counts[InsightType::SCHEMA_VIOLATION] << R"(</div>
                    </div>
                </a>
)";
    }
    if (counts[InsightType::TRANSITIVE_CLOSURE] > 0) {
        html << R"(                <a class="card-link" href="#module-transitive-closure">
                    <div class="card transitive-closure">
                        <h3>Transitive Closure Gaps</h3>
                        <div class="value">)" << counts[InsightType::TRANSITIVE_CLOSURE] << R"(</div>
                    </div>
                </a>
)";
    }

    html << R"(            </div>

            <!-- Featured Insights with Subgraph Visualizations -->
            <div style="margin: 40px 0;">
                <h3 style="color: var(--primary); margin-bottom: 10px;">Featured High-Impact Insights</h3>
                <p style="color: var(--text-muted); margin-bottom: 25px; font-size: 0.95em;">
                    Diverse examples showcasing different types of discoveries with their underlying graph structures.
                    Arrows indicate directionality of relationships.
                </p>
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 25px;">
)";

    // Select diverse insights from different categories
    std::map<InsightCategory, std::vector<Insight>> by_category;
    for (const auto& insight : insights.insights) {
        if (!insight.witness_edges.empty()) {
            by_category[insight.category].push_back(insight);
        }
    }

    // Sort within each category by score
    for (auto& [cat, insights_vec] : by_category) {
        std::sort(insights_vec.begin(), insights_vec.end(),
                  [](const Insight& a, const Insight& b) { return a.score > b.score; });
    }

    // Select top insight from each category (max 4 - ensure diversity)
    std::vector<std::pair<InsightCategory, Insight>> featured_insights;
    for (const auto& cat : {InsightCategory::EXPLORATORY, InsightCategory::TRANSFORMATIONAL, InsightCategory::COMBINATORIAL}) {
        if (by_category.count(cat) && !by_category[cat].empty()) {
            featured_insights.push_back({cat, by_category[cat][0]});
        }
    }

    // If we have less than 4, add more from different types within categories
    if (featured_insights.size() < 4) {
        std::vector<Insight> remaining;
        for (const auto& insight : insights.insights) {
            if (insight.witness_edges.empty()) continue;
            bool already_featured = false;
            for (const auto& [cat, fi] : featured_insights) {
                if (fi.seed_labels == insight.seed_labels && fi.type == insight.type) {
                    already_featured = true;
                    break;
                }
            }
            if (!already_featured) {
                remaining.push_back(insight);
            }
        }
        std::sort(remaining.begin(), remaining.end(),
                  [](const Insight& a, const Insight& b) { return a.score > b.score; });

        for (const auto& insight : remaining) {
            if (featured_insights.size() >= 4) break;
            featured_insights.push_back({insight.category, insight});
        }
    }

    // Render featured insights
    for (const auto& [category, insight] : featured_insights) {
        std::string svg = generate_mini_subgraph_svg(insight, 6);
        if (svg.empty()) continue;

        std::string type_name = get_insight_type_name(insight.type);
        std::string category_name = category_to_string(category);
        std::string entity_label = insight.seed_labels.empty() ? "N/A" : insight.seed_labels[0];

        // Get brief description - contextual, not generic
        std::string description;
        if (insight.type == InsightType::CAUSAL_CHAIN) {
            description = "Directed causal pathway showing cause-effect relationships";
        } else if (insight.type == InsightType::BRIDGE) {
            description = "Connects separate knowledge clusters, enabling information flow";
        } else if (insight.type == InsightType::INTERVENTION_POINT) {
            description = "Critical node whose removal would disconnect causal pathways";
        } else if (insight.type == InsightType::MOTIF) {
            description = "Recurring structural pattern across the knowledge graph";
        } else if (insight.type == InsightType::FEEDBACK_LOOP) {
            description = "Cyclic structure indicating self-reinforcing dynamics";
        } else if (insight.type == InsightType::DOMAIN_BRIDGE) {
            description = "Cross-domain connector linking distinct research areas";
        } else if (insight.type == InsightType::COMMUNITY_DETECTION) {
            description = "Dense cluster of related concepts forming a community";
        } else {
            // Build contextual description from entities
            if (insight.seed_labels.size() >= 2) {
                description = "Relationship between " + insight.seed_labels[0] + " and " + insight.seed_labels[1];
            } else if (!insight.seed_labels.empty()) {
                description = "Network structure centered on " + insight.seed_labels[0];
            } else {
                description = "Graph pattern with " + std::to_string(insight.witness_edges.size()) + " supporting edges";
            }
        }

        // Generate contextual "Why High-Impact" explanation
        std::stringstream why_impact;
        why_impact << "<strong>Reveals:</strong> ";

        // Extract context from insight
        std::vector<std::string> entities;
        for (size_t i = 0; i < std::min(insight.seed_labels.size(), size_t(3)); i++) {
            entities.push_back(insight.seed_labels[i]);
        }

        // Get relationships from witness edges
        std::vector<std::string> relations;
        for (size_t i = 0; i < std::min(insight.witness_edges.size(), size_t(2)); i++) {
            const auto* edge = graph_.get_hyperedge(insight.witness_edges[i]);
            if (edge && !edge->relation.empty()) {
                relations.push_back(edge->relation);
            }
        }

        // Build contextual explanation based on type
        if (insight.type == InsightType::CAUSAL_CHAIN) {
            if (entities.size() >= 2) {
                why_impact << "How <em>" << escape_html(entities[0]) << "</em>";
                if (entities.size() >= 3) {
                    why_impact << " causally influences <em>" << escape_html(entities[1])
                              << "</em> leading to <em>" << escape_html(entities[2]) << "</em>";
                } else {
                    why_impact << " directly causes <em>" << escape_html(entities[1]) << "</em>";
                }
                why_impact << ", with " << insight.witness_edges.size()
                          << " evidence paths showing " << std::fixed << std::setprecision(0)
                          << (insight.score * 100) << "% confidence in this causal mechanism";
            } else {
                why_impact << "A causal pathway with " << insight.witness_edges.size()
                          << " supporting evidence links demonstrating " << std::fixed << std::setprecision(0)
                          << (insight.score * 100) << "% confidence in the cause-effect relationship";
            }

        } else if (insight.type == InsightType::BRIDGE) {
            if (!entities.empty()) {
                why_impact << "That <em>" << escape_html(entities[0])
                          << "</em> connects otherwise disconnected knowledge domains";
                if (insight.witness_edges.size() >= 5) {
                    why_impact << ", serving as a critical junction in "
                              << insight.witness_edges.size() << " cross-cluster pathways";
                } else {
                    why_impact << " across " << insight.witness_edges.size() << " boundary-spanning relationships";
                }
                why_impact << " (" << std::fixed << std::setprecision(0)
                          << (insight.score * 100) << "% structural importance)";
            } else {
                why_impact << "A critical bridge connecting " << insight.witness_edges.size()
                          << " cross-cluster relationships with " << std::fixed << std::setprecision(0)
                          << (insight.score * 100) << "% centrality score";
            }

        } else if (insight.type == InsightType::INTERVENTION_POINT) {
            if (!entities.empty()) {
                why_impact << "That removing or modifying <em>" << escape_html(entities[0])
                          << "</em> would disrupt " << insight.witness_edges.size()
                          << " causal pathways, making it a leverage point for intervention";
                why_impact << " (criticality: " << std::fixed << std::setprecision(0)
                          << (insight.score * 100) << "%)";
            } else {
                why_impact << "A critical control point affecting " << insight.witness_edges.size()
                          << " downstream pathways (" << std::fixed << std::setprecision(0)
                          << (insight.score * 100) << "% bottleneck score)";
            }

        } else if (insight.type == InsightType::MOTIF) {
            if (entities.size() >= 2) {
                why_impact << "A recurring pattern involving <em>" << escape_html(entities[0])
                          << "</em>, <em>" << escape_html(entities[1]) << "</em>";
                if (entities.size() >= 3) {
                    why_impact << ", and <em>" << escape_html(entities[2]) << "</em>";
                }
                why_impact << " appearing " << insight.witness_edges.size()
                          << " times across the graph";
            } else {
                why_impact << "A structural pattern recurring " << insight.witness_edges.size()
                          << " times, suggesting systematic organization";
            }
            why_impact << " (" << std::fixed << std::setprecision(0)
                      << (insight.score * 100) << "% pattern consistency)";

        } else if (insight.type == InsightType::FEEDBACK_LOOP) {
            if (entities.size() >= 2) {
                why_impact << "A self-reinforcing cycle where <em>" << escape_html(entities[0])
                          << "</em> influences <em>" << escape_html(entities[1]) << "</em>";
                if (entities.size() >= 3) {
                    why_impact << " which affects <em>" << escape_html(entities[2]) << "</em>";
                }
                why_impact << " and loops back, creating amplifying dynamics";
            } else {
                why_impact << "A cyclic structure with " << insight.witness_edges.size()
                          << " feedback connections creating self-reinforcing behavior";
            }
            why_impact << " (" << std::fixed << std::setprecision(0)
                      << (insight.score * 100) << "% loop strength)";

        } else if (insight.type == InsightType::DOMAIN_BRIDGE) {
            if (!entities.empty()) {
                why_impact << "That <em>" << escape_html(entities[0])
                          << "</em> spans multiple research domains, connecting "
                          << insight.witness_edges.size()
                          << " cross-disciplinary relationships and enabling knowledge transfer";
            } else {
                why_impact << "A cross-domain connector linking " << insight.witness_edges.size()
                          << " relationships across distinct research fields";
            }
            why_impact << " (" << std::fixed << std::setprecision(0)
                      << (insight.score * 100) << "% interdisciplinary score)";

        } else if (insight.type == InsightType::COMMUNITY_DETECTION) {
            if (entities.size() >= 2) {
                why_impact << "A cohesive cluster containing <em>" << escape_html(entities[0])
                          << "</em>, <em>" << escape_html(entities[1]) << "</em>";
                if (entities.size() >= 3) {
                    why_impact << ", <em>" << escape_html(entities[2]) << "</em>";
                }
                why_impact << " and " << (insight.witness_edges.size() - std::min(size_t(3), entities.size()))
                          << " other densely connected concepts";
            } else {
                why_impact << "A dense community of " << insight.witness_edges.size()
                          << " tightly interconnected concepts forming a distinct knowledge cluster";
            }
            why_impact << " (" << std::fixed << std::setprecision(0)
                      << (insight.score * 100) << "% clustering coefficient)";

        } else {
            // Build truly contextual explanation from available data
            if (entities.size() >= 2) {
                // Multiple entities - describe their relationship
                why_impact << "A network connection linking <em>" << escape_html(entities[0])
                          << "</em> with <em>" << escape_html(entities[1]) << "</em>";
                if (entities.size() >= 3) {
                    why_impact << " and <em>" << escape_html(entities[2]) << "</em>";
                }

                // Add relation context if available
                if (!relations.empty()) {
                    why_impact << " through <em>" << escape_html(relations[0]) << "</em> relationships";
                }

                why_impact << ", supported by " << insight.witness_edges.size() << " evidence paths";

                // Determine significance from score
                if (insight.score >= 0.8) {
                    why_impact << ". This high-confidence pattern (" << std::fixed << std::setprecision(0)
                              << (insight.score * 100) << "%) suggests a fundamental structural connection";
                } else {
                    why_impact << " with " << std::fixed << std::setprecision(0)
                              << (insight.score * 100) << "% confidence in the association";
                }

            } else if (!entities.empty()) {
                // Single entity - describe its role
                why_impact << "The central role of <em>" << escape_html(entities[0])
                          << "</em> in connecting " << insight.witness_edges.size()
                          << " related concepts";

                if (!relations.empty()) {
                    why_impact << " via <em>" << escape_html(relations[0]) << "</em> relationships";
                }

                if (insight.score >= 0.8) {
                    why_impact << ". High structural importance (" << std::fixed << std::setprecision(0)
                              << (insight.score * 100) << "%) indicates this is a key organizational node";
                } else {
                    why_impact << " (" << std::fixed << std::setprecision(0)
                              << (insight.score * 100) << "% centrality score)";
                }

            } else {
                // No entities - describe structure
                why_impact << "A structural pattern involving " << insight.witness_edges.size()
                          << " interconnected relationships";

                if (!relations.empty()) {
                    why_impact << " of type <em>" << escape_html(relations[0]) << "</em>";
                }

                if (insight.score >= 0.8) {
                    why_impact << ", with exceptional pattern strength (" << std::fixed << std::setprecision(0)
                              << (insight.score * 100) << "%) indicating systematic organization";
                } else {
                    why_impact << " showing " << std::fixed << std::setprecision(0)
                              << (insight.score * 100) << "% pattern consistency";
                }
            }
        }

        html << R"(                    <div class="card" style="padding: 20px;">
                        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
                            <div style="font-size: 0.8em; color: var(--text-muted); text-transform: uppercase; letter-spacing: 1px;">)"
             << type_name << R"(</div>
                            <div style="font-size: 0.75em; color: var(--primary); background: rgba(79, 195, 247, 0.15); padding: 3px 8px; border-radius: 4px;">)"
             << category_name << R"(</div>
                        </div>
                        <div style="font-weight: 600; margin-bottom: 8px; color: var(--text); font-size: 1.05em;">)"
             << escape_html(entity_label) << R"(</div>
                        <div style="font-size: 0.85em; color: var(--text-muted); margin-bottom: 12px; line-height: 1.4;">)"
             << description << R"(</div>
                        <div style="margin: 15px 0;">)" << svg << R"(</div>
                        <div style="background: rgba(79, 195, 247, 0.08); padding: 12px; border-radius: 6px; margin: 12px 0; border-left: 3px solid var(--primary);">
                            <div style="font-size: 0.75em; color: var(--primary); font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 6px;">Why High-Impact</div>
                            <div style="font-size: 0.85em; color: var(--text); line-height: 1.5;">)"
             << why_impact.str() << R"(</div>
                        </div>
                        <div style="font-size: 0.9em; color: var(--text-muted); display: flex; justify-content: space-between; align-items: center; padding-top: 10px; border-top: 1px solid rgba(148, 163, 184, 0.2);">
                            <span>Confidence: )" << std::fixed << std::setprecision(2) << insight.score << R"(</span>
                            <span style="font-size: 0.8em;">)" << insight.witness_edges.size() << R"( witness edges</span>
                        </div>
                    </div>
)";
    }

    html << R"(                </div>
            </div>

            <!-- Interactive Chart -->
            <div class="chart-container">
                <canvas id="insightChart"></canvas>
            </div>
        </section>
)";

    // Table of Contents - Organized by Category
    html << R"(
        <nav class="toc">
            <h3>Contents</h3>
            <ul>
                <li><a href="#statistics">Knowledge Graph Statistics</a></li>
                <li><a href="#categories">Knowledge Discovery Categories</a></li>
                <li class="category-toc"><a href="#combinatorial-section">🧩 Combinatorial Insights</a>
                    <ul class="sub-toc">
)";

    if (counts[InsightType::MOTIF] > 0)
        html << R"(                        <li><a href="#module-motifs">Recurring Patterns</a> <span class="count">()" << counts[InsightType::MOTIF] << R"()</span></li>
)";
    if (counts[InsightType::K_CORE] > 0)
        html << R"(                        <li><a href="#module-k-core">K-Core Subgraphs</a> <span class="count">()" << counts[InsightType::K_CORE] << R"()</span></li>
)";
    if (counts[InsightType::K_TRUSS] > 0)
        html << R"(                        <li><a href="#module-k-truss">K-Truss Subgraphs</a> <span class="count">()" << counts[InsightType::K_TRUSS] << R"()</span></li>
)";
    if (counts[InsightType::COMMUNITY_DETECTION] > 0)
        html << R"(                        <li><a href="#module-community-detection">Community Detection</a> <span class="count">()" << counts[InsightType::COMMUNITY_DETECTION] << R"()</span></li>
)";
    if (counts[InsightType::CORE_PERIPHERY] > 0)
        html << R"(                        <li><a href="#module-core-periphery">Core–Periphery</a> <span class="count">()" << counts[InsightType::CORE_PERIPHERY] << R"()</span></li>
)";
    if (counts[InsightType::META_PATH_PATTERN] > 0)
        html << R"(                        <li><a href="#meta-path-patterns">Meta-Path Patterns</a> <span class="count">()" << counts[InsightType::META_PATH_PATTERN] << R"()</span></li>
)";
    if (counts[InsightType::RULE] > 0)
        html << R"(                        <li><a href="#module-rules">Association Rules</a> <span class="count">()" << counts[InsightType::RULE] << R"()</span></li>
)";
    if (counts[InsightType::TAXONOMY] > 0)
        html << R"(                        <li><a href="#module-taxonomy">Taxonomic Relations</a> <span class="count">()" << counts[InsightType::TAXONOMY] << R"()</span></li>
)";
    if (counts[InsightType::COMPOSITIONAL_REASONING] > 0)
        html << R"(                        <li><a href="#module-compositional">Compositional Reasoning</a> <span class="count">()" << counts[InsightType::COMPOSITIONAL_REASONING] << R"()</span></li>
)";
    if (counts[InsightType::TRANSITIVE_CLOSURE] > 0)
        html << R"(                        <li><a href="#module-transitive-closure">Transitive Closure Gaps</a> <span class="count">()" << counts[InsightType::TRANSITIVE_CLOSURE] << R"()</span></li>
)";
    if (counts[InsightType::MECHANISM_CONSOLIDATION] > 0)
        html << R"(                        <li><a href="#module-mechanism-consolidation">Mechanism Consolidation</a> <span class="count">()" << counts[InsightType::MECHANISM_CONSOLIDATION] << R"()</span></li>
)";
    if (counts[InsightType::META_PATH] > 0)
        html << R"(                        <li><a href="#module-meta-path">Meta-Path Links</a> <span class="count">()" << counts[InsightType::META_PATH] << R"()</span></li>
)";
    if (counts[InsightType::CENTRALITY] > 0)
        html << R"(                        <li><a href="#module-centrality">Centrality Analysis</a> <span class="count">()" << counts[InsightType::CENTRALITY] << R"()</span></li>
)";

    html << R"(                    </ul>
                </li>
                <li class="category-toc"><a href="#exploratory-section">🔍 Exploratory Insights</a>
                    <ul class="sub-toc">
)";

    if (counts[InsightType::BRIDGE] > 0)
        html << R"(                        <li><a href="#bridges">Bridge Entities</a> <span class="count">()" << counts[InsightType::BRIDGE] << R"()</span></li>
)";
    if (counts[InsightType::COMPLETION] > 0)
        html << R"(                        <li><a href="#module-completions">Knowledge Gaps</a> <span class="count">()" << counts[InsightType::COMPLETION] << R"()</span></li>
)";
    if (counts[InsightType::COMMUNITY_LINK] > 0)
        html << R"(                        <li><a href="#module-community">Cross-Community Links</a> <span class="count">()" << counts[InsightType::COMMUNITY_LINK] << R"()</span></li>
)";
    if (counts[InsightType::EMBEDDING_LINK] > 0)
        html << R"(                        <li><a href="#module-embedding">Embedding Predictions</a> <span class="count">()" << counts[InsightType::EMBEDDING_LINK] << R"()</span></li>
)";
    if (counts[InsightType::HYPEREDGE_PREDICTION] > 0)
        html << R"(                        <li><a href="#module-hyperedge-prediction">Hyperedge Predictions</a> <span class="count">()" << counts[InsightType::HYPEREDGE_PREDICTION] << R"()</span></li>
)";
    if (counts[InsightType::DOMAIN_BRIDGE] > 0)
        html << R"(                        <li><a href="#module-domain-bridge">Domain Bridges</a> <span class="count">()" << counts[InsightType::DOMAIN_BRIDGE] << R"()</span></li>
)";
    if (counts[InsightType::EXPLANATORY_CHAIN] > 0)
        html << R"(                        <li><a href="#module-explanatory-chain">Explanatory Chains</a> <span class="count">()" << counts[InsightType::EXPLANATORY_CHAIN] << R"()</span></li>
)";
    if (counts[InsightType::AUTHOR_CHAIN] > 0)
        html << R"(                        <li><a href="#module-author-chains">Author Chains</a> <span class="count">()" << counts[InsightType::AUTHOR_CHAIN] << R"()</span></li>
)";
    if (counts[InsightType::CO_AUTHORSHIP] > 0)
        html << R"(                        <li><a href="#module-co-authorship">Co-Authorship Networks</a> <span class="count">()" << counts[InsightType::CO_AUTHORSHIP] << R"()</span></li>
)";
    if (counts[InsightType::CITATION_IMPACT] > 0)
        html << R"(                        <li><a href="#module-citation-impact">Citation Impact</a> <span class="count">()" << counts[InsightType::CITATION_IMPACT] << R"()</span></li>
)";
    if (counts[InsightType::MULTI_RESOLUTION_COMMUNITY] > 0)
        html << R"(                        <li><a href="#module-multi-resolution-community">Multi-Resolution Communities</a> <span class="count">()" << counts[InsightType::MULTI_RESOLUTION_COMMUNITY] << R"()</span></li>
)";
    if (counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] > 0)
        html << R"(                        <li><a href="#module-cross-community-bridge-map">Cross-Community Bridge Maps</a> <span class="count">()" << counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] << R"()</span></li>
)";
    if (counts[InsightType::META_PATTERN] > 0)
        html << R"(                        <li><a href="#module-meta-pattern">Meta-Patterns</a> <span class="count">()" << counts[InsightType::META_PATTERN] << R"()</span></li>
)";
    if (counts[InsightType::CAUSAL_CHAIN] > 0)
        html << R"(                        <li><a href="#module-causal-chain">Causal Chains</a> <span class="count">()" << counts[InsightType::CAUSAL_CHAIN] << R"()</span></li>
)";
    if (counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] > 0)
        html << R"(                        <li><a href="#module-cross-community-bridge">Cross-Community Bridges</a> <span class="count">()" << counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] << R"()</span></li>
)";
    if (counts[InsightType::DIFFUSION] > 0)
        html << R"(                        <li><a href="#module-diffusion">Diffusion Predictions</a> <span class="count">()" << counts[InsightType::DIFFUSION] << R"()</span></li>
)";
    if (counts[InsightType::RELATION_INDUCTION] > 0)
        html << R"(                        <li><a href="#module-relation-induction">Relation Induction</a> <span class="count">()" << counts[InsightType::RELATION_INDUCTION] << R"()</span></li>
)";
    if (counts[InsightType::EVIDENCE_FUSION_LINK] > 0)
        html << R"(                        <li><a href="#module-evidence-fusion">Evidence Fusion Links</a> <span class="count">()" << counts[InsightType::EVIDENCE_FUSION_LINK] << R"()</span></li>
)";
    if (counts[InsightType::TEXT_SIMILARITY] > 0)
        html << R"(                        <li><a href="#module-text-similarity">Text Similarity</a> <span class="count">()" << counts[InsightType::TEXT_SIMILARITY] << R"()</span></li>
)";
    if (counts[InsightType::ARGUMENT_SUPPORT] > 0)
        html << R"(                        <li><a href="#module-argument-support">Argument Support</a> <span class="count">()" << counts[InsightType::ARGUMENT_SUPPORT] << R"()</span></li>
)";
    if (counts[InsightType::METHOD_OUTCOME] > 0)
        html << R"(                        <li><a href="#module-method-outcome">Method/Outcome</a> <span class="count">()" << counts[InsightType::METHOD_OUTCOME] << R"()</span></li>
)";
    if (counts[InsightType::LONG_CHAIN] > 0)
        html << R"(                        <li><a href="#long-chain">Long-Chain Links</a> <span class="count">()" << counts[InsightType::LONG_CHAIN] << R"()</span></li>
)";
    if (counts[InsightType::PATH_RANK] > 0)
        html << R"(                        <li><a href="#path-rank">Path-Ranked Links</a> <span class="count">()" << counts[InsightType::PATH_RANK] << R"()</span></li>
)";

    html << R"(                    </ul>
                </li>
                <li class="category-toc"><a href="#transformational-section">🔄 Transformational Insights</a>
                    <ul class="sub-toc">
)";

    if (counts[InsightType::SUBSTITUTION] > 0)
        html << R"(                        <li><a href="#module-substitution">Substitutions</a> <span class="count">()" << counts[InsightType::SUBSTITUTION] << R"()</span></li>
)";
    if (counts[InsightType::CONTRADICTION] > 0)
        html << R"(                        <li><a href="#module-contradictions">Contradictions</a> <span class="count">()" << counts[InsightType::CONTRADICTION] << R"()</span></li>
)";
    if (counts[InsightType::ENTITY_RESOLUTION] > 0)
        html << R"(                        <li><a href="#module-entity-resolution">Entity Resolution</a> <span class="count">()" << counts[InsightType::ENTITY_RESOLUTION] << R"()</span></li>
)";
    if (counts[InsightType::ANALOGICAL_TRANSFER] > 0)
        html << R"(                        <li><a href="#module-analogical-transfer">Analogical Transfer</a> <span class="count">()" << counts[InsightType::ANALOGICAL_TRANSFER] << R"()</span></li>
)";
    if (counts[InsightType::COUNTERFACTUAL] > 0)
        html << R"(                        <li><a href="#module-counterfactual">Counterfactual Queries</a> <span class="count">()" << counts[InsightType::COUNTERFACTUAL] << R"()</span></li>
)";
    if (counts[InsightType::SURPRISE] > 0)
        html << R"(                        <li><a href="#module-surprises">Surprising Discoveries</a> <span class="count">()" << counts[InsightType::SURPRISE] << R"()</span></li>
)";
    if (counts[InsightType::HYPOTHESES_1] > 0)
        html << R"(                        <li><a href="#module-hypotheses">Testable Hypotheses</a> <span class="count">()" << counts[InsightType::HYPOTHESES_1] << R"()</span></li>
)";
    if (counts[InsightType::HYPOTHESES_3] > 0)
        html << R"(                        <li><a href="#module-hypotheses-3">Counterfactual Hypotheses</a> <span class="count">()" << counts[InsightType::HYPOTHESES_3] << R"()</span></li>
)";
    if (counts[InsightType::META_PATH_ANOMALY] > 0)
        html << R"(                        <li><a href="#module-meta-path-anomaly">Meta-Path Anomalies</a> <span class="count">()" << counts[InsightType::META_PATH_ANOMALY] << R"()</span></li>
)";
    if (counts[InsightType::INTERVENTION_BOTTLENECK] > 0)
        html << R"(                        <li><a href="#module-intervention-bottleneck">Intervention Bottlenecks</a> <span class="count">()" << counts[InsightType::INTERVENTION_BOTTLENECK] << R"()</span></li>
)";
    if (counts[InsightType::COMPETING_MECHANISM] > 0)
        html << R"(                        <li><a href="#module-competing-mechanism">Competing Mechanisms</a> <span class="count">()" << counts[InsightType::COMPETING_MECHANISM] << R"()</span></li>
)";
    if (counts[InsightType::SCHEMA_REPAIR] > 0)
        html << R"(                        <li><a href="#module-schema-repair">Schema Repairs</a> <span class="count">()" << counts[InsightType::SCHEMA_REPAIR] << R"()</span></li>
)";
    if (counts[InsightType::INTERVENTION_POINT] > 0)
        html << R"(                        <li><a href="#module-intervention-point">Intervention Points</a> <span class="count">()" << counts[InsightType::INTERVENTION_POINT] << R"()</span></li>
)";
    if (counts[InsightType::FEEDBACK_LOOP] > 0)
        html << R"(                        <li><a href="#module-feedback-loop">Feedback Loops</a> <span class="count">()" << counts[InsightType::FEEDBACK_LOOP] << R"()</span></li>
)";
    if (counts[InsightType::CONFOUNDER] > 0)
        html << R"(                        <li><a href="#module-confounder">Confounders</a> <span class="count">()" << counts[InsightType::CONFOUNDER] << R"()</span></li>
)";
    if (counts[InsightType::LOGICAL_ENTAILMENT] > 0)
        html << R"(                        <li><a href="#module-logical-entailment">Logical Entailments</a> <span class="count">()" << counts[InsightType::LOGICAL_ENTAILMENT] << R"()</span></li>
)";
    if (counts[InsightType::SCHEMA_VIOLATION] > 0)
        html << R"(                        <li><a href="#module-schema-violation">Schema Violations</a> <span class="count">()" << counts[InsightType::SCHEMA_VIOLATION] << R"()</span></li>
)";
    if (counts[InsightType::CLAIM_STANCE] > 0)
        html << R"(                        <li><a href="#module-claim-stance">Claim Stance Analysis</a> <span class="count">()" << counts[InsightType::CLAIM_STANCE] << R"()</span></li>
)";
    if (counts[InsightType::UNCERTAINTY_SAMPLING] > 0)
        html << R"(                        <li><a href="#module-uncertainty-sampling">Uncertainty Sampling</a> <span class="count">()" << counts[InsightType::UNCERTAINTY_SAMPLING] << R"()</span></li>
)";
    if (counts[InsightType::ACTIVE_LEARNING] > 0)
        html << R"(                        <li><a href="#module-active-learning">Active Learning</a> <span class="count">()" << counts[InsightType::ACTIVE_LEARNING] << R"()</span></li>
)";
    if (regular_h2_count > 0)
        html << R"(                        <li><a href="#module-hypotheses-2">Mechanistic Hypotheses</a> <span class="count">()" << regular_h2_count << R"()</span></li>
)";
    if (intersection_bridge_count > 0)
        html << R"(                        <li><a href="#module-intersection-bridge">Intersection Bridges</a> <span class="count">()" << intersection_bridge_count << R"()</span></li>
)";

    html << R"(                    </ul>
                </li>
                <li><a href="#recommendations">Conclusions &amp; Recommendations</a></li>
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

    // Discovery modules - organized by category

    // COMBINATORIAL INSIGHTS SECTION
    html << R"(
        <div class="category-section-header" id="combinatorial-section">
            <h2>🧩 Combinatorial Insights</h2>
            <p>Pattern Detection & Structural Combinations - insights that emerge from combining existing knowledge in new ways and identifying patterns</p>
        </div>
)";

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
                )HTML" << (!sorted.empty() ? format_source_documents_html(sorted[0]) : "") << R"HTML(
            </div>
)HTML";

        // Use dynamic limits based on data size
        auto limits = calculate_dynamic_limits(sorted.size(), config);

        // Adjust config for clustering with dynamic threshold
        ReportConfig dynamic_config = config;
        dynamic_config.similarity_threshold = limits.similarity_threshold;

        // Use clustering to coalesce similar findings
        if (limits.should_cluster) {
            auto clusters = cluster_insights(sorted, dynamic_config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= limits.max_clusters_to_show) break;

                // Generate cluster summary
                std::string summary = generate_cluster_summary(cluster, dynamic_config);
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
                    if (items_in_cluster >= limits.max_items_per_cluster) {
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
                if (count >= limits.max_clusters_to_show) break;
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

    auto& core_periphery = by_type[InsightType::CORE_PERIPHERY];
    if (!core_periphery.empty()) {
        std::vector<Insight> sorted = core_periphery;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-core-periphery" class="module" style="border-left-color: var(--theme-core)">
            <div class="module-header">
                <h2 style="color: var(--theme-core)">Core–Periphery / Hub–Authority</h2>
                <p class="definition">Maps central hubs versus peripheral entities using degree, hub/authority, and integration signals.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-core);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Core–Periphery</strong> operator combines degree centrality, hub/authority balance (source vs. target roles), and hub integration scores to identify structurally central entities. Low-scoring nodes are treated as peripheral.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Core nodes anchor the graph’s structure, while peripheral nodes may represent niche concepts or sparse evidence. Understanding both helps prioritize validation and curation.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <!-- Core Nodes -->
            <h4 style="color: var(--theme-core); margin-top: 20px; margin-bottom: 10px;">🔵 Core Nodes (Highly Connected Hubs)</h4>
            <table class="data-table">
                <thead>
                    <tr><th>Entity</th><th>Degree</th><th>Hub</th><th>Authority</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            bool is_core = true;
            for (const auto& tag : insight.novelty_tags) {
                if (tag == "periphery") {
                    is_core = false;
                    break;
                }
            }
            if (!is_core) continue; // Skip periphery for now

            double degree = 0.0;
            double hub = 0.0;
            double auth = 0.0;
            auto dit = insight.score_breakdown.find("degree_norm");
            if (dit != insight.score_breakdown.end()) degree = dit->second;
            auto hit = insight.score_breakdown.find("hub_score");
            if (hit != insight.score_breakdown.end()) hub = hit->second;
            auto ait = insight.score_breakdown.find("authority_score");
            if (ait != insight.score_breakdown.end()) auth = ait->second;

            html << R"(                    <tr style="background: rgba(74, 222, 128, 0.05);">
                        <td><strong>)" << format_entities(insight.seed_labels, 1) << R"(</strong></td>
                        <td>)" << std::fixed << std::setprecision(2) << degree << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << hub << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << auth << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>

            <!-- Periphery Nodes -->
            <h4 style="color: var(--text-muted); margin-top: 30px; margin-bottom: 10px;">⚪ Periphery Nodes (Low Connectivity)</h4>
            <table class="data-table">
                <thead>
                    <tr><th>Entity</th><th>Degree</th><th>Hub</th><th>Authority</th></tr>
                </thead>
                <tbody>
)HTML";
        count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            bool is_periphery = false;
            for (const auto& tag : insight.novelty_tags) {
                if (tag == "periphery") {
                    is_periphery = true;
                    break;
                }
            }
            if (!is_periphery) continue; // Skip core for this table

            double degree = 0.0;
            double hub = 0.0;
            double auth = 0.0;
            auto dit = insight.score_breakdown.find("degree_norm");
            if (dit != insight.score_breakdown.end()) degree = dit->second;
            auto hit = insight.score_breakdown.find("hub_score");
            if (hit != insight.score_breakdown.end()) hub = hit->second;
            auto ait = insight.score_breakdown.find("authority_score");
            if (ait != insight.score_breakdown.end()) auth = ait->second;

            html << R"(                    <tr style="background: rgba(148, 163, 184, 0.03);">
                        <td>)" << format_entities(insight.seed_labels, 1) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << degree << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << hub << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << auth << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& centrality_nodes = by_type[InsightType::CENTRALITY];
    if (!centrality_nodes.empty()) {
        std::vector<Insight> sorted = centrality_nodes;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-centrality" class="module" style="border-left-color: var(--theme-centrality)">
            <div class="module-header">
                <h2 style="color: var(--theme-centrality)">Centrality</h2>
                <p class="definition">Ranks entities by structural importance in the bipartite projection.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-centrality);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Centrality</strong> operator runs PageRank on the bipartite graph formed by entities and hyperedges, highlighting structurally influential nodes.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Central entities are often critical for connecting multiple relationship contexts.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entity</th><th>Centrality</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double score = insight.score;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 1) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << score << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& communities = by_type[InsightType::COMMUNITY_DETECTION];
    if (!communities.empty()) {
        std::vector<Insight> sorted = communities;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-community-detection" class="module" style="border-left-color: var(--theme-community-detect)">
            <div class="module-header">
                <h2 style="color: var(--theme-community-detect)">Community Detection</h2>
                <p class="definition">Finds dense clusters via Louvain on the projected graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-community-detect);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Community Detection</strong> operator clusters entities using a Louvain-style modularity optimization on the projected graph.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Communities represent topic or subdomain structure in the knowledge graph.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Representative Entities</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 3) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << insight.score << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& k_core_nodes = by_type[InsightType::K_CORE];
    if (!k_core_nodes.empty()) {
        std::vector<Insight> sorted = k_core_nodes;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-k-core" class="module" style="border-left-color: var(--theme-k-core)">
            <div class="module-header">
                <h2 style="color: var(--theme-k-core)">k-Core Nodes</h2>
                <p class="definition">Highlights entities in tightly connected cores.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-k-core);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>k-Core</strong> operator computes the core number of each node on the projected graph, identifying the largest k for which the node remains connected to at least k neighbors. Higher core numbers indicate denser local connectivity.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> k-cores reveal robust, tightly knit regions that often represent stable conceptual hubs or well-supported areas of the knowledge graph.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entity</th><th>Core Number</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double core = 0.0;
            auto it = insight.score_breakdown.find("core");
            if (it != insight.score_breakdown.end()) core = it->second;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 1) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(0) << core << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& k_truss_links = by_type[InsightType::K_TRUSS];
    if (!k_truss_links.empty()) {
        std::vector<Insight> sorted = k_truss_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-k-truss" class="module" style="border-left-color: var(--theme-k-truss)">
            <div class="module-header">
                <h2 style="color: var(--theme-k-truss)">k-Truss Edges</h2>
                <p class="definition">Finds triangle-reinforced edges in the projected graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-k-truss);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>k-Truss</strong> operator scores edges by the number of shared neighbors (triangles) in the projected graph. Edges supported by many triangles are retained in higher-order trusses.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Triangle-reinforced edges often indicate cohesive substructures and more reliable associations.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Triangle Support</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double support_val = 0.0;
            auto it = insight.score_breakdown.find("support");
            if (it != insight.score_breakdown.end()) support_val = it->second;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(0) << support_val << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& mechanisms = by_type[InsightType::MECHANISM_CONSOLIDATION];
    if (!mechanisms.empty()) {
        std::vector<Insight> sorted = mechanisms;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-mechanism-consolidation" class="module" style="border-left-color: var(--theme-mechanism)">
            <div class="module-header">
                <h2 style="color: var(--theme-mechanism)">Mechanism Consolidation</h2>
                <p class="definition">Clusters overlapping hypotheses and chains into higher-level mechanisms.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-mechanism);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Mechanism Consolidation</strong> operator clusters mechanistic hypotheses, long-chain links, and explanatory chains based on overlapping node sets. Each cluster highlights a shared explanatory theme supported by multiple independent signals.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Consolidated mechanisms reduce redundancy and emphasize the most persistent explanatory structures in the graph.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Cluster Summary</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_mechanism_consolidation(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>
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
                )HTML" << (!sorted.empty() ? format_source_documents_html(sorted[0]) : "") << R"HTML(
            </div>
)HTML";

        // Use dynamic limits based on data size
        auto limits = calculate_dynamic_limits(sorted.size(), config);

        // Adjust config for clustering with dynamic threshold
        ReportConfig dynamic_config = config;
        dynamic_config.similarity_threshold = limits.similarity_threshold;

        // Use clustering to coalesce similar findings
        if (limits.should_cluster) {
            auto clusters = cluster_insights(sorted, dynamic_config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= limits.max_clusters_to_show) break;

                std::string summary = generate_cluster_summary(cluster, dynamic_config);
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
                    if (items_in_cluster >= limits.max_items_per_cluster) {
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
                if (count >= limits.max_clusters_to_show) break;
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

    auto& meta_patterns = by_type[InsightType::META_PATH_PATTERN];
    if (!meta_patterns.empty()) {
        std::vector<Insight> sorted = meta_patterns;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"(
        <section id="meta-path-patterns">
            <h2>Meta-Path Patterns</h2>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-metapath);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Meta-Path Pattern</strong> operator mines frequently occurring relation sequences across the graph. Patterns are scored by distinct (start, end) support, average confidence, and path length.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> These templates reveal reusable structural motifs that can guide validation, querying, and hypothesis generation.</p>
            </div>
)";

        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;

            std::string narrative;
            if (config.use_llm_narratives && llm_provider_) {
                narrative = generate_llm_narrative(insight, config);
            } else {
                narrative = describe_meta_path_pattern(insight);
            }
            html << R"HTML(
            <div class="insight metapathpattern">
                <h4>)HTML" << (count + 1) << R"HTML(. Pattern</h4>
                <p>)HTML" << escape_html(strip_markdown_bold(narrative)) << R"HTML(</p>
            </div>
)HTML";
            count++;
        }

        html << R"(        </section>
)";
    }

    auto& meta_path_links = by_type[InsightType::META_PATH];
    if (!meta_path_links.empty()) {
        std::vector<Insight> sorted = meta_path_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-meta-path" class="module" style="border-left-color: var(--theme-metapath)">
            <div class="module-header">
                <h2 style="color: var(--theme-metapath)">Meta-Path Links</h2>
                <p class="definition">Inferred from typed relation sequences that repeatedly connect entity pairs.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-metapath);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Meta-Path Link</strong> detector searches for entity pairs connected by recurring typed relation sequences (e.g., A &rarr;causes&rarr; B &rarr;regulates&rarr; C). It scores pairs by template frequency and entity-type alignment, surfacing connections that are non-obvious from direct or single-hop links alone.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Meta-path links capture structural relationships that only emerge when considering the full typed context of a multi-hop path. They enable predictions that span knowledge boundaries, revealing how entities are related through shared relational patterns even when no direct connection exists.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entity Pair</th><th>Pattern</th><th>Score</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            if (insight.seed_labels.size() < 2) continue;

            std::string pattern = "";
            for (const auto& tag : insight.novelty_tags) {
                if (tag.find("pattern=") == 0) {
                    pattern = tag.substr(8);
                    break;
                }
            }

            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_meta_path(insight);
            }

            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(insight.seed_labels[0]) << " ↔ " << escape_html(insight.seed_labels[1]) << R"HTML(</td>
                        <td>)HTML" << escape_html(pattern) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& taxonomies = by_type[InsightType::TAXONOMY];
    if (!taxonomies.empty()) {
        std::vector<Insight> sorted = taxonomies;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-taxonomy" class="module" style="border-left-color: var(--theme-motif)">
            <div class="module-header">
                <h2 style="color: var(--theme-motif)">Taxonomy</h2>
                <p class="definition">Hierarchical is-a and part-of relationships.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-motif);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Taxonomy</strong> detector identifies hierarchical edges (is-a, instance-of, type-of, part-of, subclass-of) in the graph and assembles them into consistent hierarchical trees. It resolves cross-edges, detects inconsistencies (e.g., circular inheritance), and scores relations by consistency and evidence support.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Explicit taxonomic structure enables class-level inference, improves ontological consistency, and supports classification algorithms that exploit the hierarchy. It is foundational for knowledge graph reasoning: an entity that is-a X inherits all properties of X without requiring them to be explicitly stated for each instance.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Hierarchy</th><th>Confidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_taxonomy(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 3) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& compositional = by_type[InsightType::COMPOSITIONAL_REASONING];
    if (!compositional.empty()) {
        std::vector<Insight> sorted = compositional;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-compositional" class="module" style="border-left-color: var(--theme-analogy)">
            <div class="module-header">
                <h2 style="color: var(--theme-analogy)">Compositional Reasoning</h2>
                <p class="definition">Composed relations via relation algebra.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-analogy);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Compositional Reasoning</strong> operator applies relation algebra to compose pairs of existing edges. Given edges A &rarr;<em>r1</em>&rarr; B and B &rarr;<em>r2</em>&rarr; C, it infers A &rarr;compose(<em>r1</em>,<em>r2</em>)&rarr; C using a learned or rule-based composition table, then scores the inferred link by the confidence of both component edges and the specificity of the composed relation.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Compositional reasoning enables multi-hop inference without requiring an explicit path to already exist in the graph. It extends knowledge coverage through principled link prediction grounded in known relation semantics, and can surface implied connections that are definitionally entailed but not yet recorded.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Composition</th><th>Confidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Derivation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_compositional_reasoning(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 3) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& transitive_closures = by_type[InsightType::TRANSITIVE_CLOSURE];
    if (!transitive_closures.empty()) {
        std::vector<Insight> sorted = transitive_closures;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-transitive-closure" class="module" style="border-left-color: var(--theme-completion)">
            <div class="module-header">
                <h2 style="color: var(--theme-completion)">Transitive Closure Gaps</h2>
                <p class="definition">Missing links implied by transitive relations (if A&rarr;B and B&rarr;C, expect A&rarr;C).</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-completion);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Transitive Closure</strong> detector applies transitivity rules to specific relation types known to be transitive (is-a, part-of, implies, precedes, etc.). For each transitive chain A &rarr; B &rarr; C, it checks whether the implied direct link A &rarr; C exists. Missing links are scored by the strength of the transitive chain that implies them.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Transitive closure gaps represent definitively missing knowledge — links that are logically implied but not yet recorded. Adding confirmed closures directly increases graph completeness and reduces the need to traverse multi-hop paths during inference. High-confidence gaps are among the safest additions to a knowledge graph since they are deductively entailed by existing data.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Source</th><th>Target</th><th>Confidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Reasoning</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string source = insight.seed_labels.size() > 0 ? insight.seed_labels[0] : "-";
            std::string target = insight.seed_labels.size() > 1 ? insight.seed_labels[1] : "-";
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_transitive_closure(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(source) << R"HTML(</td>
                        <td>)HTML" << escape_html(target) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    // EXPLORATORY INSIGHTS SECTION
    html << R"(
        <div class="category-section-header" id="exploratory-section">
            <h2>🔍 Exploratory Insights</h2>
            <p>Path Finding & Connection Discovery - insights that discover new connections, paths, and bridges between concepts</p>
        </div>
)";

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
                )HTML" << (!sorted.empty() ? format_source_documents_html(sorted[0]) : "") << R"HTML(
            </div>
)HTML";

        // Use dynamic limits based on data size
        auto limits = calculate_dynamic_limits(sorted.size(), config);

        // Adjust config for clustering with dynamic threshold
        ReportConfig dynamic_config = config;
        dynamic_config.similarity_threshold = limits.similarity_threshold;

        // Use clustering to coalesce similar findings
        if (limits.should_cluster) {
            auto clusters = cluster_insights(sorted, dynamic_config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= limits.max_clusters_to_show) break;

                std::string summary = generate_cluster_summary(cluster, dynamic_config);
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
                    if (items_in_cluster >= limits.max_items_per_cluster) {
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
                if (count >= limits.max_clusters_to_show) break;
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

    auto& text_links = by_type[InsightType::TEXT_SIMILARITY];
    if (!text_links.empty()) {
        std::vector<Insight> sorted = text_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-text-similarity" class="module" style="border-left-color: var(--theme-text)">
            <div class="module-header">
                <h2 style="color: var(--theme-text)">Text-Semantic Similarity Links</h2>
                <p class="definition">Links entities with highly similar labels based on TF-IDF cosine similarity.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-text);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Text Similarity</strong> operator tokenizes entity labels, builds TF-IDF vectors, and computes cosine similarity. Pairs above a threshold are suggested as semantically related.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Similar labels often indicate related concepts, synonyms, or naming variants that can inform curation.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Similarity</th><th>Source</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];
            double sim = 0.0;
            auto it = insight.score_breakdown.find("text_similarity");
            if (it != insight.score_breakdown.end()) sim = it->second;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << sim << R"(</td>
                        <td>)" << escape_html(source) << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& argument_links = by_type[InsightType::ARGUMENT_SUPPORT];
    if (!argument_links.empty()) {
        std::vector<Insight> sorted = argument_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-argument-support" class="module" style="border-left-color: var(--theme-argument)">
            <div class="module-header">
                <h2 style="color: var(--theme-argument)">Argument-Supported Relations</h2>
                <p class="definition">Proposes links grounded in evidence paths through the graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-argument);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Argument Support</strong> operator finds short evidence paths between entity pairs and uses them to justify a plausible relation. Longer or weaker paths reduce confidence.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Path-grounded suggestions provide interpretable support for candidate edges.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Support</th><th>Source</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << insight.score << R"(</td>
                        <td>)" << escape_html(source) << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& method_nodes = by_type[InsightType::METHOD_OUTCOME];
    if (!method_nodes.empty()) {
        std::vector<Insight> sorted = method_nodes;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-method-outcome" class="module" style="border-left-color: var(--theme-method)">
            <div class="module-header">
                <h2 style="color: var(--theme-method)">Method/Outcome Entities</h2>
                <p class="definition">Identifies entities likely representing methods or outcomes.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-method);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Method/Outcome</strong> operator classifies nodes using label cues and local relation context. LLM assistance refines ambiguous cases.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Explicit method/outcome roles support clearer reasoning and downstream analysis.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entity</th><th>Role</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string role = "method";
            for (const auto& tag : insight.novelty_tags) {
                if (tag == "outcome") role = "outcome";
            }
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 1) << R"(</td>
                        <td>)" << escape_html(role) << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& relation_inductions = by_type[InsightType::RELATION_INDUCTION];
    if (!relation_inductions.empty()) {
        std::vector<Insight> sorted = relation_inductions;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-relation-induction" class="module" style="border-left-color: var(--theme-relation)">
            <div class="module-header">
                <h2 style="color: var(--theme-relation)">Relation Induction</h2>
                <p class="definition">Suggests canonical relation type names and descriptions.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-relation);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Relation Induction</strong> operator clusters examples of a relation label and uses an LLM to propose a canonical type name plus a short description.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Normalized relation types improve consistency and enable cleaner downstream analytics.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Suggestion</th><th>Confidence</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double conf = 0.0;
            auto it = insight.score_breakdown.find("confidence");
            if (it != insight.score_breakdown.end()) conf = it->second;
            html << R"(                    <tr>
                        <td>)" << escape_html(insight.description) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << conf << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& hyperedge_links = by_type[InsightType::HYPEREDGE_PREDICTION];
    if (!hyperedge_links.empty()) {
        std::vector<Insight> sorted = hyperedge_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-hyperedge-prediction" class="module" style="border-left-color: var(--theme-hyperedge)">
            <div class="module-header">
                <h2 style="color: var(--theme-hyperedge)">Hyperedge Prediction</h2>
                <p class="definition">Predicts relations using overlap patterns under the same relation type.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-hyperedge);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Hyperedge Prediction</strong> operator compares source entities that share similar target sets under the same relation and proposes missing links based on overlap.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Overlap-based predictions capture consistent relational patterns that often indicate missing knowledge.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Proposed Link</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << insight.score << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
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
            if (count >= capped_max_examples(config)) break;
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

    auto& evidence_fusion = by_type[InsightType::EVIDENCE_FUSION_LINK];
    if (!evidence_fusion.empty()) {
        std::vector<Insight> sorted = evidence_fusion;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-evidence-fusion" class="module" style="border-left-color: var(--theme-fusion)">
            <div class="module-header">
                <h2 style="color: var(--theme-fusion)">Evidence Fusion Links</h2>
                <p class="definition">Combines multiple operator signals into consensus link candidates.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-fusion);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Evidence Fusion</strong> operator aggregates confidence scores from multiple independent discovery operators (completions, analogies, path ranks, embedding links, etc.) using a weighted consensus model. Candidate links that appear in several independent pipelines receive a multiplicative score boost, making them high-priority discoveries.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Multi-operator agreement is a strong signal of genuine knowledge. Fused links are substantially more reliable than single-method findings — independent lines of evidence converging on the same candidate link provide the strongest grounds for knowledge graph augmentation.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Candidate Link</th><th>Sources</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double source_count = 0.0;
            auto it = insight.score_breakdown.find("source_count");
            if (it != insight.score_breakdown.end()) source_count = it->second;
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_evidence_fusion(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(0) << source_count << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& cross_bridges = by_type[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE];
    if (!cross_bridges.empty()) {
        std::vector<Insight> sorted = cross_bridges;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-cross-community-bridge" class="module" style="border-left-color: var(--theme-crosscommunity)">
            <div class="module-header">
                <h2 style="color: var(--theme-crosscommunity)">Cross-Community Mechanism Bridges</h2>
                <p class="definition">Mechanistic paths that connect distant communities/domains.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-crosscommunity);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Cross-Community Mechanism Bridge</strong> detector extracts the shortest mechanistic path between every pair of detected graph communities, scores bridges by path strength, entity-type diversity, and cross-domain explanatory power. It selects bridges where the connecting path uses causal or mechanistic relations, not just structural proximity.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Cross-community bridges represent the most intellectually fertile discovery zone — they connect knowledge communities through interpretable causal pathways, seeding interdisciplinary hypotheses. These findings are prime candidates for research that cuts across disciplinary boundaries.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Bridge</th><th>Novelty</th><th>Support</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double novelty = 0.0;
            double support = 0.0;
            auto n_it = insight.score_breakdown.find("novelty");
            if (n_it != insight.score_breakdown.end()) novelty = n_it->second;
            auto s_it = insight.score_breakdown.find("support");
            if (s_it != insight.score_breakdown.end()) support = s_it->second;
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_cross_community_bridge(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << novelty << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << support << R"HTML(</td>
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& bridges = by_type[InsightType::BRIDGE];
    if (!bridges.empty()) {
        std::sort(bridges.begin(), bridges.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        // Calculate dynamic limits based on data size
        auto bridge_limits = calculate_dynamic_limits(bridges.size(), config);

        html << R"HTML(
        <section id="bridges" class="module" style="border-left-color: var(--theme-bridge)">
            <div class="module-header">
                <h2 style="color: var(--theme-bridge)">Bridge Entities</h2>
                <p class="definition">Identifies critical entities that connect otherwise separate parts of the graph.</p>
                <div class="count">Total: )HTML" << bridges.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-bridge);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Bridge Detection</strong> algorithm identifies entities whose removal would disconnect or significantly fragment the knowledge graph. Using s-connectivity analysis on the hypergraph structure, it finds nodes that serve as critical links between otherwise separate clusters or communities. The bridge score reflects how essential the entity is for maintaining graph connectivity.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Bridge entities are often key concepts, pivotal figures, or central ideas that tie together different areas of knowledge. They represent critical junctions for information flow and should be documented thoroughly. Losing or misrepresenting a bridge entity could fragment understanding of the domain.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : bridges) {
            if (count >= bridge_limits.max_clusters_to_show) break;

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
            if (count >= capped_max_examples(config)) break;
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

    auto& long_chains = by_type[InsightType::LONG_CHAIN];
    if (!long_chains.empty()) {
        std::sort(long_chains.begin(), long_chains.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"(
        <section id="long-chain">
            <h2>Long-Chain Reasoning Links</h2>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-longchain);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Long-Chain Reasoning</strong> algorithm searches for multi-hop paths (4–7 edges by default) that connect distant entities. It favors chains with strong average edge confidence, coherent relation patterns, and novel path signatures. These chains are not direct links, but they can reveal extended mechanisms or thematic connections across the graph.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Long chains can surface deeper explanatory pathways that short-hop methods miss, helping researchers trace multi-step dependencies or causal narratives.</p>
            </div>
)";

        int count = 0;
        for (auto& insight : long_chains) {
            if (count >= capped_max_examples(config)) break;
            if (insight.seed_labels.size() < 2) continue;

            std::string narrative;
            if (config.use_llm_narratives && llm_provider_) {
                narrative = generate_llm_narrative(insight, config);
            } else {
                narrative = describe_long_chain(insight);
            }
            html << R"(
            <div class="insight longchain">
                <h4>)" << (count + 1) << ". " << insight.seed_labels[0] << " <-> " << insight.seed_labels[1] << R"(</h4>
                <p>)" << escape_html(strip_markdown_bold(narrative)) << R"(</p>
            </div>
)";
            count++;
        }

        html << R"(        </section>
)";
    }

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
            if (count >= capped_max_examples(config)) break;
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

    // ========== CO-AUTHORSHIP NETWORKS ==========
    auto& co_authorships = by_type[InsightType::CO_AUTHORSHIP];
    if (!co_authorships.empty()) {
        std::sort(co_authorships.begin(), co_authorships.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-co-authorship" class="module" style="border-left-color: var(--theme-author)">
            <div class="module-header">
                <h2 style="color: var(--theme-author)">Co-Authorship Networks</h2>
                <p class="definition">Identifies frequent collaborators and co-authorship patterns based on joint document appearances.</p>
                <div class="count">Total: )HTML" << co_authorships.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-author);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Co-Authorship</strong> operator detects authors who frequently appear together in the same documents. It builds a collaboration network by identifying person entities that co-occur across multiple texts, indicating joint authorship or collaboration.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Co-authorship networks reveal research clusters, institutional collaborations, and intellectual communities. High collaboration counts suggest strong working relationships or shared research agendas.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : co_authorships) {
            if (count >= capped_max_examples(config)) break;
            if (insight.seed_labels.size() < 2) continue;

            int collab_count = static_cast<int>(insight.score_breakdown["collaborations"]);

            html << R"HTML(
            <div class="insight co-authorship">
                <h4>)HTML" << (count + 1) << ". " << escape_html(insight.seed_labels[0]) << " & "
                 << escape_html(insight.seed_labels[1]) << R"HTML(</h4>
                <p><strong>Collaborations:</strong> )HTML" << collab_count << R"HTML( joint works</p>
                <p>)HTML" << escape_html(strip_markdown_bold(insight.description)) << R"HTML(</p>
            </div>
)HTML";
            count++;
        }

        html << R"HTML(        </section>
)HTML";
    }

    // ========== CITATION IMPACT ANALYSIS ==========
    auto& citation_impacts = by_type[InsightType::CITATION_IMPACT];
    if (!citation_impacts.empty()) {
        std::sort(citation_impacts.begin(), citation_impacts.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-citation-impact" class="module" style="border-left-color: var(--theme-author)">
            <div class="module-header">
                <h2 style="color: var(--theme-author)">Citation & Authorship Impact Analysis</h2>
                <p class="definition">Ranks authors and works by incoming citations or authorship counts to identify influential scholars and seminal works.</p>
                <div class="count">Total: )HTML" << citation_impacts.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-author);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Citation Impact</strong> operator counts incoming citations for each author or work in the graph using reference relations (cites, cited by, references). When citation data is unavailable, it counts authorship relations (authored, co-authored) to identify frequently authored works.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Citation/authorship counts are proxies for scholarly influence and impact. Highly cited authors and frequently authored works often represent foundational contributions, seminal papers, or influential thought leaders in their domains.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : citation_impacts) {
            if (count >= capped_max_examples(config)) break;
            if (insight.seed_labels.empty()) continue;

            // Determine metric type (citations or authorships)
            int impact_count = 0;
            std::string metric_type = "Citations";
            std::string contributor_label = "Cited by";

            if (insight.score_breakdown.find("citations") != insight.score_breakdown.end()) {
                impact_count = static_cast<int>(insight.score_breakdown["citations"]);
                metric_type = "Citations";
                contributor_label = "Cited by";
            } else if (insight.score_breakdown.find("authorships") != insight.score_breakdown.end()) {
                impact_count = static_cast<int>(insight.score_breakdown["authorships"]);
                metric_type = "Authorships";
                contributor_label = "Authored by";
            } else if (insight.score_breakdown.find("citation_count") != insight.score_breakdown.end()) {
                // Legacy support
                impact_count = static_cast<int>(insight.score_breakdown["citation_count"]);
                metric_type = "Citations";
                contributor_label = "Cited by";
            }

            std::string entity_name = insight.seed_labels[0];

            html << R"HTML(
            <div class="insight citation-impact">
                <h4>)HTML" << (count + 1) << ". " << escape_html(entity_name) << R"HTML(</h4>
                <p><strong>)HTML" << metric_type << R"HTML(:</strong> )HTML" << impact_count << R"HTML(</p>
)HTML";

            // Show top contributors if available
            if (insight.seed_labels.size() > 1) {
                html << R"HTML(                <p><strong>)HTML" << contributor_label << R"HTML(:</strong> )HTML";
                for (size_t i = 1; i < std::min(insight.seed_labels.size(), size_t(6)); ++i) {
                    if (i > 1) html << ", ";
                    html << escape_html(insight.seed_labels[i]);
                }
                if (insight.seed_labels.size() > 6) {
                    html << ", and " << (insight.seed_labels.size() - 6) << " others";
                }
                html << R"HTML(</p>
)HTML";
            }

            html << R"HTML(            </div>
)HTML";
            count++;
        }

        html << R"HTML(        </section>
)HTML";
    }

    // ========== MULTI-RESOLUTION COMMUNITY DETECTION ==========
    auto& multi_res_communities = by_type[InsightType::MULTI_RESOLUTION_COMMUNITY];
    if (!multi_res_communities.empty()) {
        std::sort(multi_res_communities.begin(), multi_res_communities.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-multi-resolution-community" class="module" style="border-left-color: var(--theme-community)">
            <div class="module-header">
                <h2 style="color: var(--theme-community)">Multi-Resolution Community Detection</h2>
                <p class="definition">Reveals hierarchical community structure by running detection at multiple resolution scales.</p>
                <div class="count">Total: )HTML" << multi_res_communities.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-community);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Multi-Resolution Community</strong> operator runs Louvain community detection at multiple resolution parameters (0.5, 1.0, 2.0, 4.0). This reveals hierarchical organization—communities merge and split across scales, showing both fine-grained clusters and broader groupings.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Different resolution scales reveal different organizational levels. Low resolution shows major divisions; high resolution reveals sub-communities. This hierarchical view helps understand nested knowledge structures.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : multi_res_communities) {
            if (count >= capped_max_examples(config)) break;
            if (insight.seed_labels.empty()) continue;

            double resolution = insight.score_breakdown["resolution"];
            int size = static_cast<int>(insight.score_breakdown["size"]);

            html << R"HTML(
            <div class="insight multi-resolution">
                <h4>)HTML" << (count + 1) << ". Resolution " << std::fixed << std::setprecision(1) << resolution
                 << " - " << size << " members" << R"HTML(</h4>
                <p><strong>Members (sample):</strong> )HTML";

            for (size_t i = 0; i < std::min(insight.seed_labels.size(), size_t(10)); ++i) {
                if (i > 0) html << ", ";
                html << escape_html(insight.seed_labels[i]);
            }
            if (insight.seed_labels.size() > 10) {
                html << ", and " << (size - 10) << " more";
            }

            html << R"HTML(</p>
            </div>
)HTML";
            count++;
        }

        html << R"HTML(        </section>
)HTML";
    }

    // ========== CROSS-COMMUNITY BRIDGE MAPPING ==========
    auto& bridge_maps = by_type[InsightType::CROSS_COMMUNITY_BRIDGE_MAP];
    if (!bridge_maps.empty()) {
        std::sort(bridge_maps.begin(), bridge_maps.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-cross-community-bridge-map" class="module" style="border-left-color: var(--theme-bridge)">
            <div class="module-header">
                <h2 style="color: var(--theme-bridge)">Cross-Community Bridge Mapping</h2>
                <p class="definition">Maps bridge entities to the communities they connect, revealing inter-cluster knowledge flow.</p>
                <div class="count">Total: )HTML" << bridge_maps.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-bridge);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Bridge Mapping</strong> operator takes previously identified bridges and determines which communities each bridge connects. It scores bridges by the number of distinct communities they link.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Bridges that connect many communities are critical for cross-domain knowledge transfer. High-connectivity bridges enable interdisciplinary synthesis and represent boundary-spanning concepts.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : bridge_maps) {
            if (count >= capped_max_examples(config)) break;
            if (insight.seed_labels.empty()) continue;

            int communities_connected = static_cast<int>(insight.score_breakdown["communities_connected"]);
            std::string bridge_name = insight.seed_labels[0];

            html << R"HTML(
            <div class="insight bridge-map">
                <h4>)HTML" << (count + 1) << ". " << escape_html(bridge_name) << R"HTML(</h4>
                <p><strong>Communities Connected:</strong> )HTML" << communities_connected << R"HTML(</p>
                <p>)HTML" << escape_html(insight.description) << R"HTML(</p>
            </div>
)HTML";
            count++;
        }

        html << R"HTML(        </section>
)HTML";
    }

    // ========== META-PATTERN DISCOVERY ==========
    auto& meta_pattern_insights = by_type[InsightType::META_PATTERN];
    if (!meta_pattern_insights.empty()) {
        std::sort(meta_pattern_insights.begin(), meta_pattern_insights.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-meta-pattern" class="module" style="border-left-color: var(--theme-motif)">
            <div class="module-header">
                <h2 style="color: var(--theme-motif)">Meta-Pattern Discovery</h2>
                <p class="definition">Identifies patterns across patterns—recurring structural templates that appear multiple times.</p>
                <div class="count">Total: )HTML" << meta_pattern_insights.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-motif);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Meta-Pattern</strong> operator analyzes existing patterns (motifs, k-trusses) to find structural similarities. It groups patterns by size and identifies frequently recurring configurations.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Meta-patterns reveal the "grammar" of the knowledge graph—fundamental structural building blocks that recur across domains. They enable pattern-based querying and structural abstraction.</p>
            </div>
)HTML";

        int count = 0;
        for (auto& insight : meta_pattern_insights) {
            if (count >= capped_max_examples(config)) break;

            int occurrences = static_cast<int>(insight.score_breakdown["occurrences"]);
            int pattern_size = static_cast<int>(insight.score_breakdown["pattern_size"]);

            html << R"HTML(
            <div class="insight meta-pattern">
                <h4>)HTML" << (count + 1) << ". " << pattern_size << "-node pattern (" << occurrences << " occurrences)" << R"HTML(</h4>
                <p><strong>Sample instances:</strong> )HTML";

            size_t max_show = std::min(insight.seed_labels.size(), size_t(15));
            for (size_t i = 0; i < max_show; ++i) {
                if (i > 0 && i % pattern_size == 0) html << " | ";
                else if (i > 0) html << ", ";
                html << escape_html(insight.seed_labels[i]);
            }
            if (insight.seed_labels.size() > max_show) {
                html << " ...";
            }

            html << R"HTML(</p>
            </div>
)HTML";
            count++;
        }

        html << R"HTML(        </section>
)HTML";
    }

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

        // Use dynamic limits based on data size
        auto limits = calculate_dynamic_limits(sorted.size(), config);

        // Adjust config for clustering with dynamic threshold
        ReportConfig dynamic_config = config;
        dynamic_config.similarity_threshold = limits.similarity_threshold;

        // Use clustering to coalesce similar findings
        if (limits.should_cluster) {
            auto clusters = cluster_insights(sorted, dynamic_config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= limits.max_clusters_to_show) break;

                std::string summary = generate_cluster_summary(cluster, dynamic_config);
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
                    if (items_in_cluster >= limits.max_items_per_cluster) {
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
                if (count >= limits.max_clusters_to_show) break;
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

    auto& diffusions = by_type[InsightType::DIFFUSION];
    if (!diffusions.empty()) {
        std::vector<Insight> sorted = diffusions;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-diffusion" class="module" style="border-left-color: var(--theme-path)">
            <div class="module-header">
                <h2 style="color: var(--theme-path)">Diffusion Patterns</h2>
                <p class="definition">Information or influence spread patterns through the graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-path);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Diffusion</strong> operator simulates random-walk or label-propagation over graph edges to estimate how influence, information, or contagion would spread from a seed node. The spread score reflects how broadly and rapidly the seed's influence would reach other nodes given the graph's edge structure.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Diffusion patterns reveal knowledge transmission routes — which concepts would most rapidly propagate an idea through the domain. High spread-score entities are influential information hubs whose connections should be validated and maintained, as errors there would propagate widely.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Source</th><th>Spread Score</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_diffusion(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 2) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& causal_chains = by_type[InsightType::CAUSAL_CHAIN];
    if (!causal_chains.empty()) {
        std::vector<Insight> sorted = causal_chains;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-causal-chain" class="module" style="border-left-color: var(--theme-longchain)">
            <div class="module-header">
                <h2 style="color: var(--theme-longchain)">Causal Chains</h2>
                <p class="definition">Directed causal pathways through the knowledge graph.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-longchain);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Causal Chain</strong> detector traverses the graph searching for directed paths that use only relation types labeled as causal (causes, leads-to, results-in, triggers, inhibits, etc.). Chains are scored by the specificity of their causal relation types, the length of the chain, and the confidence of individual edges.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Explicit causal chains provide mechanistic explanations for why two concepts are related, moving beyond correlation to directed dependency. They are essential for building causal models of the domain and identifying intervention points.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Chain</th><th>Confidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Mechanism</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_causal_chain(insight);
            }
            // Phase 2: Generate causal metadata badges
            std::string badges = generate_causal_badges_html(insight);
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 4) << badges << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& domain_bridges = by_type[InsightType::DOMAIN_BRIDGE];
    if (!domain_bridges.empty()) {
        std::vector<Insight> sorted = domain_bridges;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-domain-bridge" class="module" style="border-left-color: var(--theme-crosscommunity)">
            <div class="module-header">
                <h2 style="color: var(--theme-crosscommunity)">Domain Bridges</h2>
                <p class="definition">Cross-domain or interdisciplinary connector nodes.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-crosscommunity);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Domain Bridge</strong> detector computes cross-community edge ratios for each node. Nodes with a high proportion of edges crossing community boundaries — connecting to entities in multiple distinct clusters with diverse relation types — are flagged as domain bridges. The score reflects both the number of domains bridged and the semantic diversity of relations.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Domain bridges sit at the intellectual intersection of multiple fields. They often represent the most productive areas for interdisciplinary research, as they connect concepts that are independently studied but rarely linked. These nodes are prime starting points for cross-domain hypothesis generation.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Bridge</th><th>Domains</th><th>Strength</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Significance</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string domains = insight.seed_labels.size() > 1 ? (insight.seed_labels.size() > 2 ? std::to_string(insight.seed_labels.size()) + " domains" : "2 domains") : "-";
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_domain_bridge(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << (insight.seed_labels.empty() ? "-" : escape_html(insight.seed_labels[0])) << R"HTML(</td>
                        <td>)HTML" << escape_html(domains) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& explanatory_chains = by_type[InsightType::EXPLANATORY_CHAIN];
    if (!explanatory_chains.empty()) {
        std::vector<Insight> sorted = explanatory_chains;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-explanatory-chain" class="module" style="border-left-color: var(--theme-longchain)">
            <div class="module-header">
                <h2 style="color: var(--theme-longchain)">Explanatory Chains</h2>
                <p class="definition">Multi-hop explanation paths connecting distant concepts.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-longchain);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Explanatory Chain</strong> detector searches for multi-hop paths between distantly related entities that maximise a combination of path coherence (consistent relation type sequences), entity relevance, and narrative interpretability. Chains are scored by path quality — how clearly the sequence of relations forms a logical explanation.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Explanatory chains provide narrative connections between remote concepts, helping researchers understand <em>why</em> distant entities are related and how knowledge flows between them. They are essential for building explanatory narratives from graph data — transforming structured links into human-readable reasoning chains.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Chain</th><th>Quality</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_explanatory_chain(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 4) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    // TRANSFORMATIONAL INSIGHTS SECTION
    html << R"(
        <div class="category-section-header" id="transformational-section">
            <h2>🔄 Transformational Insights</h2>
            <p>Reframing & Perspective Shifts - insights that transform understanding by reframing concepts, generating hypotheses, and suggesting alternative explanations</p>
        </div>
)";

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
                )HTML" << (!sorted.empty() ? format_source_documents_html(sorted[0]) : "") << R"HTML(
            </div>
)HTML";

        // Use dynamic limits based on data size
        auto limits = calculate_dynamic_limits(sorted.size(), config);

        // Adjust config for clustering with dynamic threshold
        ReportConfig dynamic_config = config;
        dynamic_config.similarity_threshold = limits.similarity_threshold;

        // Use clustering to coalesce similar findings
        if (limits.should_cluster) {
            auto clusters = cluster_insights(sorted, dynamic_config);
            int cluster_num = 0;
            int total_shown = 0;

            for (auto& cluster : clusters) {
                if (total_shown >= limits.max_clusters_to_show) break;

                std::string summary = generate_cluster_summary(cluster, dynamic_config);
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
                    if (items_in_cluster >= limits.max_items_per_cluster) {
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
                if (count >= limits.max_clusters_to_show) break;
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

    auto& contradictions = by_type[InsightType::CONTRADICTION];
    if (!contradictions.empty()) {
        std::vector<Insight> sorted = contradictions;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-contradictions" class="module" style="border-left-color: var(--theme-contradiction)">
            <div class="module-header">
                <h2 style="color: var(--theme-contradiction)">Contradictions</h2>
                <p class="definition">Flags conflicts where affirmed and negated claims both appear.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-contradiction);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Contradiction Detector</strong> scans relation labels for negation cues and checks whether the same entity pairs appear with both negated and affirmed relations. It groups evidence by relation base and entity sets to surface conflicts.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Contradictions often indicate ambiguous evidence, conflicting sources, or extraction errors. Resolving them improves graph reliability.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Score</th><th>Source</th>)HTML";
        bool include_explanation = (config.use_llm_narratives || config.include_llm_examples) && llm_provider_;
        if (include_explanation) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string source = insight.evidence_chunk_ids.empty() ? "-" : insight.evidence_chunk_ids[0];
            std::string explanation;
            if (include_explanation) {
                if (config.use_llm_narratives) {
                    explanation = generate_llm_narrative(insight, config);
                } else {
                    explanation = get_graph_context_summary(insight, false);
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
        </section>
)HTML";
    }

    auto& resolutions = by_type[InsightType::ENTITY_RESOLUTION];
    if (!resolutions.empty()) {
        std::vector<Insight> sorted = resolutions;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-entity-resolution" class="module" style="border-left-color: var(--theme-resolution)">
            <div class="module-header">
                <h2 style="color: var(--theme-resolution)">Entity Resolution</h2>
                <p class="definition">Detects likely duplicate or alias entities based on label and context similarity.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-resolution);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Entity Resolution</strong> operator compares node labels and neighborhood overlap to detect likely duplicates. High token similarity combined with shared neighbors suggests two nodes represent the same underlying entity.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Resolving duplicates reduces redundancy and improves graph consistency, which can improve downstream discovery quality.</p>
            </div>
            <div class="spotlight">
                <h3>Top Insight Analysis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Similarity</th><th>Neighbor Overlap</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double label_sim = 0.0;
            double neighbor_overlap = 0.0;
            auto lit = insight.score_breakdown.find("label_similarity");
            if (lit != insight.score_breakdown.end()) label_sim = lit->second;
            auto nit = insight.score_breakdown.find("neighbor_overlap");
            if (nit != insight.score_breakdown.end()) neighbor_overlap = nit->second;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << label_sim << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << neighbor_overlap << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& active_queries = by_type[InsightType::ACTIVE_LEARNING];
    if (!active_queries.empty()) {
        std::vector<Insight> sorted = active_queries;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-active-learning" class="module" style="border-left-color: var(--theme-active)">
            <div class="module-header">
                <h2 style="color: var(--theme-active)">Active Learning Queries</h2>
                <p class="definition">Questions to verify high-impact or uncertain relations.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-active);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Active Learning</strong> operator prioritizes low-confidence edges attached to high-degree nodes, yielding the most informative human validation questions.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Targeted validation improves graph quality with minimal expert effort.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Query</th><th>Entities</th><th>Confidence</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double conf = 0.0;
            auto it = insight.score_breakdown.find("confidence");
            if (it != insight.score_breakdown.end()) conf = it->second;
            html << R"(                    <tr>
                        <td>)" << escape_html(insight.description) << R"(</td>
                        <td>)" << format_entities(insight.seed_labels, 3) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << conf << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& claim_stances = by_type[InsightType::CLAIM_STANCE];
    if (!claim_stances.empty()) {
        std::vector<Insight> sorted = claim_stances;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-claim-stance" class="module" style="border-left-color: var(--theme-claim)">
            <div class="module-header">
                <h2 style="color: var(--theme-claim)">Claim Stance</h2>
                <p class="definition">Classifies extracted claims as supporting, opposing, or neutral.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-claim);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Claim Stance</strong> operator summarizes each relation as a claim and uses LLM-assisted classification to label it as supporting, opposing, or neutral. Confidence scores reflect model certainty.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Stance labels help disambiguate contradictory statements and prioritize evidence review.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Claim</th><th>Stance</th><th>Confidence</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string stance = "neutral";
            for (const auto& tag : insight.novelty_tags) {
                if (tag == "supports" || tag == "opposes" || tag == "neutral") stance = tag;
            }
            double conf = 0.0;
            auto it = insight.score_breakdown.find("confidence");
            if (it != insight.score_breakdown.end()) conf = it->second;
            std::string claim = insight.description.empty() ? format_entities(insight.seed_labels, 2) : insight.description;
            html << R"(                    <tr>
                        <td>)" << escape_html(claim) << R"(</td>
                        <td>)" << escape_html(stance) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << conf << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& analogical_links = by_type[InsightType::ANALOGICAL_TRANSFER];
    if (!analogical_links.empty()) {
        std::vector<Insight> sorted = analogical_links;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-analogical-transfer" class="module" style="border-left-color: var(--theme-analogy)">
            <div class="module-header">
                <h2 style="color: var(--theme-analogy)">Analogical Transfer</h2>
                <p class="definition">Proposes new links by analogy between relation patterns.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-analogy);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Analogical Transfer</strong> operator finds pairs of relations with similar source and target labels, then proposes a cross-combination (A relates to D) by analogy.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Analogies can surface plausible missing links that are structurally supported but not yet recorded.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Proposed Link</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << insight.score << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& uncertainty_samples = by_type[InsightType::UNCERTAINTY_SAMPLING];
    if (!uncertainty_samples.empty()) {
        std::vector<Insight> sorted = uncertainty_samples;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-uncertainty-sampling" class="module" style="border-left-color: var(--theme-uncertainty)">
            <div class="module-header">
                <h2 style="color: var(--theme-uncertainty)">Uncertainty Sampling</h2>
                <p class="definition">Highlights low-confidence relations for verification.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-uncertainty);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Uncertainty Sampling</strong> operator ranks relations by low extraction confidence and surfaces the most uncertain items for review.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Focusing on high-uncertainty edges improves graph quality with minimal expert effort.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Relation</th><th>Uncertainty</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double uncertainty = 0.0;
            auto it = insight.score_breakdown.find("uncertainty");
            if (it != insight.score_breakdown.end()) uncertainty = it->second;
            html << R"(                    <tr>
                        <td>)" << format_entities(insight.seed_labels, 2) << R"(</td>
                        <td>)" << std::fixed << std::setprecision(2) << uncertainty << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& counterfactuals = by_type[InsightType::COUNTERFACTUAL];
    if (!counterfactuals.empty()) {
        std::vector<Insight> sorted = counterfactuals;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-counterfactual" class="module" style="border-left-color: var(--theme-counterfactual)">
            <div class="module-header">
                <h2 style="color: var(--theme-counterfactual)">Counterfactual Probing</h2>
                <p class="definition">Generates falsification questions for extracted claims.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-counterfactual);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Counterfactual</strong> operator converts claims into targeted questions asking what evidence would refute or reverse them.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Counterfactual queries make validation explicit and help identify missing evidence.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Question</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            html << R"(                    <tr>
                        <td>)" << escape_html(describe_counterfactual(insight)) << R"(</td>
                    </tr>
)";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& hypotheses = by_type[InsightType::HYPOTHESES_1];
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
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-hypothesis);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Bayesian Hypothesis Generator (H1)</strong> combines signals from bridge detection, completion, contradiction, community, and structural scoring into posterior probability estimates for candidate relational hypotheses. Each hypothesis is ranked by posterior probability, prior surprise, and evidence breadth.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Bayesian hypotheses quantify uncertainty explicitly, surfacing high-impact uncertain relations as the best targets for empirical validation. They provide a probabilistic ranking rather than a binary pass/fail, enabling prioritization under limited experimental budget.</p>
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
            if (count >= capped_max_examples(config)) break;
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

    auto& hypotheses3 = by_type[InsightType::HYPOTHESES_3];
    if (!hypotheses3.empty()) {
        std::vector<Insight> sorted = hypotheses3;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-hypotheses-3" class="module" style="border-left-color: var(--theme-counterfactual)">
            <div class="module-header">
                <h2 style="color: var(--theme-counterfactual)">Counterfactual Causal Hypotheses (Hypotheses_3)</h2>
                <p class="definition">Generates testable counterfactual hypotheses with predicted interventions and mechanisms.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-counterfactual);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Counterfactual Causal Hypothesis Generator (H3)</strong> applies graph-based counterfactual reasoning to identify causal interventions. It estimates effect size from local structural changes (node/edge removal) and proposes 'If X then Y because Z' hypotheses with quantified predicted outcomes and underlying graph-derived mechanisms.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Counterfactual hypotheses directly support experimental design by specifying the exact intervention, the expected outcome, and the mechanistic chain that would be disrupted. High effect-size hypotheses with clear mechanisms are the highest-priority candidates for empirical testing.</p>
            </div>
            <div class="spotlight">
                <h3>Top Causal Hypothesis</h3>
                <p class="narrative">)HTML" << spotlight_text(sorted) << R"HTML(</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Hypothesis</th><th>Effect Size</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Mechanism</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            auto eit = insight.score_breakdown.find("effect_size");
            double effect_size = (eit != insight.score_breakdown.end()) ? eit->second : insight.score;
            std::string explanation;
            if (include_llm) {
                explanation = get_llm_example(insight, config);
                if (explanation.empty()) {
                    explanation = get_graph_context_summary(insight, false);
                }
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_hypothesis(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << effect_size << R"HTML(</td>)HTML";
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

    auto& meta_path_anomalies = by_type[InsightType::META_PATH_ANOMALY];
    if (!meta_path_anomalies.empty()) {
        std::vector<Insight> sorted = meta_path_anomalies;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-meta-path-anomaly" class="module" style="border-left-color: var(--theme-anomaly)">
            <div class="module-header">
                <h2 style="color: var(--theme-anomaly)">Meta-Path Anomalies</h2>
                <p class="definition">Rare but plausible relation-sequence templates.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-anomaly);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Meta-Path Anomaly Detector</strong> mines all observed relation-sequence templates (meta-paths) in the graph and computes their frequency. Templates that occur rarely but still satisfy entity-type compatibility and plausibility constraints are flagged as anomalies. The anomaly score measures statistical rarity; the plausibility score measures structural compatibility.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Rare relation templates that remain plausible may expose novel mechanisms that frequency-based mining systematically misses. These anomalies are high-value discovery candidates — they represent unique structural patterns that could reflect genuine but underrepresented domain knowledge.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Anomaly</th><th>Anomaly Score</th><th>Plausibility</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double anomaly = 0.0;
            double plausibility = 0.0;
            auto a_it = insight.score_breakdown.find("anomaly");
            if (a_it != insight.score_breakdown.end()) anomaly = a_it->second;
            auto p_it = insight.score_breakdown.find("plausibility");
            if (p_it != insight.score_breakdown.end()) plausibility = p_it->second;
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_meta_path_anomaly(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << anomaly << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << plausibility << R"HTML(</td>
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& intervention_bottlenecks = by_type[InsightType::INTERVENTION_BOTTLENECK];
    if (!intervention_bottlenecks.empty()) {
        std::vector<Insight> sorted = intervention_bottlenecks;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-intervention-bottleneck" class="module" style="border-left-color: var(--theme-bottleneck)">
            <div class="module-header">
                <h2 style="color: var(--theme-bottleneck)">Intervention Bottlenecks</h2>
                <p class="definition">High-leverage nodes that appear across many causal pathways.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-bottleneck);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Intervention Bottleneck</strong> detector computes causal path centrality — it counts how many distinct causal chains pass through each node and scores nodes by their coverage (fraction of all chains) and the raw number of affected chains. Nodes are ranked from most to least pivotal.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Bottleneck nodes are the highest-leverage intervention points in a causal network. Modifying, blocking, or amplifying a bottleneck node affects the maximum number of downstream causal effects simultaneously, making them the most strategically valuable targets for experimental or therapeutic intervention.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Node</th><th>Coverage</th><th>Affected Chains</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double coverage = 0.0;
            double chains = 0.0;
            auto c_it = insight.score_breakdown.find("coverage");
            if (c_it != insight.score_breakdown.end()) coverage = c_it->second;
            auto a_it = insight.score_breakdown.find("affected_chains");
            if (a_it != insight.score_breakdown.end()) chains = a_it->second;
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 1) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << coverage << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(0) << chains << R"HTML(</td>
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& competing_mechanisms = by_type[InsightType::COMPETING_MECHANISM];
    if (!competing_mechanisms.empty()) {
        std::vector<Insight> sorted = competing_mechanisms;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-competing-mechanism" class="module" style="border-left-color: var(--theme-competing)">
            <div class="module-header">
                <h2 style="color: var(--theme-competing)">Competing Mechanisms</h2>
                <p class="definition">Alternative mechanism sets for the same outcome.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-competing);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Competing Mechanism</strong> detector finds all minimal causal chains that converge on the same outcome node, then clusters chains by structural divergence — measuring how different their intermediate entities and relation types are. High-divergence clusters identify genuinely competing explanatory routes to the same effect.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Competing mechanisms are essential for scientific rigor: they prevent over-commitment to a single explanation. Identifying alternative pathways to the same outcome enables researchers to design discriminative experiments that pit the hypotheses against each other, ultimately converging on the most accurate causal model.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Alternative Mechanisms</th><th>Divergence</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            double divergence = 0.0;
            auto d_it = insight.score_breakdown.find("divergence");
            if (d_it != insight.score_breakdown.end()) divergence = d_it->second;
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_competing_mechanism(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << divergence << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& schema_repairs = by_type[InsightType::SCHEMA_REPAIR];
    if (!schema_repairs.empty()) {
        std::vector<Insight> sorted = schema_repairs;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        html << R"HTML(
        <section id="module-schema-repair" class="module" style="border-left-color: var(--theme-repair)">
            <div class="module-header">
                <h2 style="color: var(--theme-repair)">Schema Repair Suggestions</h2>
                <p class="definition">Actionable graph repair proposals inferred from structural gaps.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-repair);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Schema Repair</strong> detector identifies constraint violations — missing type annotations, broken is-a hierarchies, isolated nodes, domain/range mismatches — and generates concrete, executable repair operations. Each repair is scored by impact (how many downstream inferences it restores) and confidence (how certain the repair is correct).</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Schema violations silently corrupt downstream reasoning. Repair suggestions are executable actions that improve graph consistency and integrity without requiring manual inspection of every entity. Applying high-scoring repairs directly improves the reliability of all other discovery results.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Repair Action</th><th>Score</th></tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            html << R"HTML(                    <tr>
                        <td>)HTML" << escape_html(strip_markdown_bold(describe_schema_repair(insight))) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>
                    </tr>
)HTML";
            count++;
        }
        html << R"HTML(                </tbody>
            </table>
        </section>
)HTML";
    }

    auto& substitutions = by_type[InsightType::SUBSTITUTION];
    if (!substitutions.empty()) {
        std::vector<Insight> sorted = substitutions;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-substitution" class="module" style="border-left-color: var(--theme-resolution)">
            <div class="module-header">
                <h2 style="color: var(--theme-resolution)">Substitutions</h2>
                <p class="definition">Interchangeable entities that may be synonyms or closely related concepts.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-resolution);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Substitution Detector</strong> compares entity neighborhoods using Jaccard similarity over shared hyperedges. Two entities that appear in highly overlapping relation contexts — connected to the same set of other entities through the same types of relations — are flagged as contextually interchangeable substitution candidates.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Substitution reveals synonym entities or context-interchangeable concepts that may represent the same real-world object expressed with different terminology. Identifying these candidates helps consolidate redundant nodes, reduce vocabulary inconsistencies, and improve retrieval and reasoning quality.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entities</th><th>Similarity</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Explanation</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_substitution(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 3) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& intervention_pts = by_type[InsightType::INTERVENTION_POINT];
    if (!intervention_pts.empty()) {
        std::vector<Insight> sorted = intervention_pts;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-intervention-point" class="module" style="border-left-color: var(--theme-bottleneck)">
            <div class="module-header">
                <h2 style="color: var(--theme-bottleneck)">Intervention Points</h2>
                <p class="definition">Critical nodes whose removal would disconnect causal pathways.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-bottleneck);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Intervention Point</strong> detector applies a directed variant of betweenness centrality restricted to causal edges. It identifies nodes that, if removed, would disconnect the maximum number of causal pathways. Criticality scores reflect the fraction of all causal chains disrupted by removing that node.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Intervention points are the most strategically valuable nodes in a causal network — modifying, blocking, or amplifying them has the maximum downstream effect. These nodes are prime candidates for experimental manipulation, therapeutic targeting, or policy intervention.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Node</th><th>Criticality</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Impact</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_intervention_point(insight);
            }
            // Phase 2: Generate causal metadata badges
            std::string badges = generate_causal_badges_html(insight);
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 1) << badges << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& feedback_loops = by_type[InsightType::FEEDBACK_LOOP];
    if (!feedback_loops.empty()) {
        std::vector<Insight> sorted = feedback_loops;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-feedback-loop" class="module" style="border-left-color: var(--theme-surprise)">
            <div class="module-header">
                <h2 style="color: var(--theme-surprise)">Feedback Loops</h2>
                <p class="definition">Cycles in causal or directional relationships.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-surprise);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Feedback Loop</strong> detector finds strongly connected components (SCCs) in the directed subgraph of causal and directional relations. Cycles within SCCs are enumerated, and each loop is scored by the minimum edge strength in the cycle — a loop is only as strong as its weakest causal link.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Feedback loops indicate self-reinforcing or self-regulating dynamics in the domain — amplification cycles or homeostatic control mechanisms. They are critical for system dynamics modelling, as positive feedback loops lead to exponential growth and negative loops to equilibrium. Identifying them early prevents incorrect linear modelling of inherently cyclic phenomena.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Loop</th><th>Strength</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Dynamics</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_feedback_loop(insight);
            }
            // Phase 2: Generate causal metadata badges
            std::string badges = generate_causal_badges_html(insight);
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 3) << badges << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& confounders = by_type[InsightType::CONFOUNDER];
    if (!confounders.empty()) {
        std::vector<Insight> sorted = confounders;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-confounder" class="module" style="border-left-color: var(--theme-anomaly)">
            <div class="module-header">
                <h2 style="color: var(--theme-anomaly)">Confounders</h2>
                <p class="definition">Potential confounding variables in causal relationships.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-anomaly);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Confounder Detector</strong> searches for third variables connected to both ends of a suspected causal relation. For each candidate causal pair A &rarr; C, it identifies entities B that share edges with both A and C, scoring B by how strongly it is associated with each endpoint and how likely it is to be a genuine shared cause rather than a coincidental neighbor.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Confounders can make spurious correlations appear causal. Identifying them early guides researchers to control for these variables in statistical or experimental analyses, preventing false causal conclusions and improving the credibility of derived hypotheses.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Confounder</th><th>Relation</th><th>Association</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Impact</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string relation = insight.seed_labels.size() >= 3 ? (insight.seed_labels[0] + " → " + insight.seed_labels[2]) : "-";
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_confounder(insight);
            }
            // Phase 2: Generate causal metadata badges
            std::string badges = generate_causal_badges_html(insight);
            html << R"HTML(                    <tr>
                        <td>)HTML" << (insight.seed_labels.size() > 1 ? escape_html(insight.seed_labels[1]) : "-") << badges << R"HTML(</td>
                        <td>)HTML" << escape_html(relation) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& logical_entailments = by_type[InsightType::LOGICAL_ENTAILMENT];
    if (!logical_entailments.empty()) {
        std::vector<Insight> sorted = logical_entailments;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-logical-entailment" class="module" style="border-left-color: var(--theme-rule)">
            <div class="module-header">
                <h2 style="color: var(--theme-rule)">Logical Entailments</h2>
                <p class="definition">Inferred implications (if X and Y, then Z).</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-rule);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Logical Entailment</strong> detector applies a set of first-order inference rules to derive implicit facts from the graph. For example: if A is-a B and B has-property P, then A has-property P. Rules are applied iteratively until a fixed point, and each derived fact is scored by the confidence of the rule chain that produced it.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Logical entailments can significantly densify the knowledge graph with reliable implicit knowledge — facts that are not explicitly stated but necessarily follow from what is. They reduce redundant explicit assertions needed and reveal implied relations that improve downstream reasoning quality without requiring additional data collection.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Entailment</th><th>Confidence</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Reasoning</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_logical_entailment(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 3) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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

    auto& schema_violations = by_type[InsightType::SCHEMA_VIOLATION];
    if (!schema_violations.empty()) {
        std::vector<Insight> sorted = schema_violations;
        std::sort(sorted.begin(), sorted.end(), [](const Insight& a, const Insight& b) {
            return a.score > b.score;
        });

        bool include_llm = config.include_llm_examples && llm_provider_;
        html << R"HTML(
        <section id="module-schema-violation" class="module" style="border-left-color: var(--theme-contradiction)">
            <div class="module-header">
                <h2 style="color: var(--theme-contradiction)">Schema Violations</h2>
                <p class="definition">Entities or relations that violate expected patterns.</p>
                <div class="count">Total: )HTML" << sorted.size() << R"HTML(</div>
            </div>
            <div class="method-explanation" style="background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <h4 style="margin-top: 0; color: var(--theme-contradiction);">How This Method Works</h4>
                <p style="margin-bottom: 10px;">The <strong>Schema Violation</strong> detector checks each entity and relation against expected schema constraints: type compatibility, arity requirements, domain and range restrictions, mandatory property rules, and structural invariants. Each violation is scored by severity — how many downstream inferences are invalidated by it.</p>
                <p style="margin-bottom: 0;"><em>Why it matters:</em> Schema violations silently corrupt downstream reasoning and can propagate errors across the graph. High-severity violations indicate data quality issues, ingestion errors, or genuinely novel findings that break assumed patterns — all of which deserve immediate attention before the graph is used for inference or augmentation.</p>
            </div>
            <table class="data-table">
                <thead>
                    <tr><th>Violation</th><th>Severity</th>)HTML";
        if (include_llm) {
            html << R"HTML(<th>Details</th>)HTML";
        }
        html << R"HTML(</tr>
                </thead>
                <tbody>
)HTML";
        int count = 0;
        for (const auto& insight : sorted) {
            if (count >= capped_max_examples(config)) break;
            std::string explanation;
            if (include_llm) {
                explanation = config.use_llm_narratives ? generate_llm_narrative(insight, config) : describe_schema_violation(insight);
            }
            html << R"HTML(                    <tr>
                        <td>)HTML" << format_entities(insight.seed_labels, 2) << R"HTML(</td>
                        <td>)HTML" << std::fixed << std::setprecision(2) << insight.score << R"HTML(</td>)HTML";
            if (include_llm) {
                html << R"HTML(
                        <td>)HTML" << escape_html(strip_markdown_bold(explanation)) << R"HTML(</td>)HTML";
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
        html << R"(                    <li><strong>Review Substitutions:</strong> The )" << counts[InsightType::SUBSTITUTION] << R"( potential substitutions highlight interchangeable entities that may be synonyms or closely related concepts.</li>
)";
    }

    if (counts[InsightType::ENTITY_RESOLUTION] > 0) {
        html << R"(                    <li><strong>Merge Likely Duplicates:</strong> The )" << counts[InsightType::ENTITY_RESOLUTION] << R"( entity resolution candidates suggest duplicate or alias entities that could be linked or merged.</li>
)";
    }

    if (counts[InsightType::CORE_PERIPHERY] > 0) {
        html << R"(                    <li><strong>Review Core–Periphery Roles:</strong> The )" << counts[InsightType::CORE_PERIPHERY] << R"( core-periphery insights highlight which entities anchor the graph versus those on the periphery.</li>
)";
    }

    if (counts[InsightType::TEXT_SIMILARITY] > 0) {
        html << R"(                    <li><strong>Review Text Similarity Links:</strong> The )" << counts[InsightType::TEXT_SIMILARITY] << R"( text similarity links surface entities with near-duplicate or closely related labels.</li>
)";
    }

    if (counts[InsightType::ARGUMENT_SUPPORT] > 0) {
        html << R"(                    <li><strong>Validate Argument-Supported Relations:</strong> The )" << counts[InsightType::ARGUMENT_SUPPORT] << R"( proposed relations are backed by evidence paths and should be reviewed.</li>
)";
    }

    if (counts[InsightType::ACTIVE_LEARNING] > 0) {
        html << R"(                    <li><strong>Answer Active Learning Queries:</strong> The )" << counts[InsightType::ACTIVE_LEARNING] << R"( validation questions target the most uncertain or high-impact relations.</li>
)";
    }

    if (counts[InsightType::METHOD_OUTCOME] > 0) {
        html << R"(                    <li><strong>Confirm Method/Outcome Roles:</strong> The )" << counts[InsightType::METHOD_OUTCOME] << R"( classifications clarify which entities are methods or outcomes.</li>
)";
    }
    if (counts[InsightType::CENTRALITY] > 0) {
        html << R"(                    <li><strong>Review Central Entities:</strong> The )" << counts[InsightType::CENTRALITY] << R"( centrality findings highlight influential entities to prioritize for curation.</li>
)";
    }
    if (counts[InsightType::COMMUNITY_DETECTION] > 0) {
        html << R"(                    <li><strong>Inspect Community Clusters:</strong> The )" << counts[InsightType::COMMUNITY_DETECTION] << R"( detected communities can guide topic segmentation or subgraph analysis.</li>
)";
    }
    if (counts[InsightType::K_CORE] > 0) {
        html << R"(                    <li><strong>Assess k-Core Nodes:</strong> The )" << counts[InsightType::K_CORE] << R"( k-core entities represent dense cores worth validating or expanding.</li>
)";
    }
    if (counts[InsightType::K_TRUSS] > 0) {
        html << R"(                    <li><strong>Validate k-Truss Links:</strong> The )" << counts[InsightType::K_TRUSS] << R"( k-truss edges reflect strong local cohesion and should be verified.</li>
)";
    }
    if (counts[InsightType::CLAIM_STANCE] > 0) {
        html << R"(                    <li><strong>Review Claim Stance:</strong> The )" << counts[InsightType::CLAIM_STANCE] << R"( stance classifications help identify supporting vs. opposing claims.</li>
)";
    }
    if (counts[InsightType::RELATION_INDUCTION] > 0) {
        html << R"(                    <li><strong>Normalize Relation Types:</strong> The )" << counts[InsightType::RELATION_INDUCTION] << R"( induced relation types can guide ontology cleanup.</li>
)";
    }
    if (counts[InsightType::ANALOGICAL_TRANSFER] > 0) {
        html << R"(                    <li><strong>Validate Analogical Links:</strong> The )" << counts[InsightType::ANALOGICAL_TRANSFER] << R"( analogical transfers suggest new links worth verification.</li>
)";
    }
    if (counts[InsightType::UNCERTAINTY_SAMPLING] > 0) {
        html << R"(                    <li><strong>Verify Uncertain Relations:</strong> The )" << counts[InsightType::UNCERTAINTY_SAMPLING] << R"( low-confidence relations are prime candidates for validation.</li>
)";
    }
    if (counts[InsightType::COUNTERFACTUAL] > 0) {
        html << R"(                    <li><strong>Answer Counterfactual Probes:</strong> The )" << counts[InsightType::COUNTERFACTUAL] << R"( questions help test claim robustness.</li>
)";
    }
    if (counts[InsightType::HYPEREDGE_PREDICTION] > 0) {
        html << R"(                    <li><strong>Review Hyperedge Predictions:</strong> The )" << counts[InsightType::HYPEREDGE_PREDICTION] << R"( predicted links should be validated before insertion.</li>
)";
    }
    // Constrained rules removed from pipeline

    if (counts[InsightType::CONTRADICTION] > 0) {
        html << R"(                    <li><strong>Resolve Contradictions:</strong> The )" << counts[InsightType::CONTRADICTION] << R"( contradictions indicate conflicting claims that require manual review.</li>
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

    if (counts[InsightType::HYPOTHESES_1] > 0) {
        html << R"(                    <li><strong>Test Bayesian Hypotheses (H1):</strong> The )" << counts[InsightType::HYPOTHESES_1] << R"( probabilistic hypotheses quantify relationship likelihood with posterior probabilities and uncertainty metrics. Prioritize high-posterior, high-surprise candidates.</li>
)";
    }

    if (intersection_bridge_count > 0) {
        html << R"(                    <li><strong>Prioritize Intersection-Constrained Bridges:</strong> The )" << intersection_bridge_count << R"( intersection-based mechanistic bridge hypotheses connect semantically distant concepts through higher-order pathways. Validate top bridge nodes and pathway evidence first.</li>
)";
    }

    if (regular_h2_count > 0) {
        html << R"(                    <li><strong>Evaluate Typed Mechanistic Chains (H2):</strong> The )" << regular_h2_count << R"( typed mechanistic chains highlight role-labeled pathways and compact explanation subgraphs. Use them to prioritize interpretable causal pathways and supporting evidence.</li>
)";
    }

    if (counts[InsightType::HYPOTHESES_3] > 0) {
        html << R"(                    <li><strong>Test Counterfactual Hypotheses (H3):</strong> The )" << counts[InsightType::HYPOTHESES_3] << R"( counterfactual causal hypotheses propose specific interventions with predicted outcomes. Use the mechanistic explanations and test plans to design empirical validations.</li>
)";
    }

    if (counts[InsightType::MECHANISM_CONSOLIDATION] > 0) {
        html << R"(                    <li><strong>Review Consolidated Mechanisms:</strong> The )" << counts[InsightType::MECHANISM_CONSOLIDATION] << R"( mechanism clusters unify overlapping hypotheses and chains. Focus validation on the strongest shared explanations.</li>
)";
    }

    if (counts[InsightType::EVIDENCE_FUSION_LINK] > 0) {
        html << R"(                    <li><strong>Prioritize Evidence-Fusion Links:</strong> The )" << counts[InsightType::EVIDENCE_FUSION_LINK] << R"( fused candidates aggregate multiple independent operator signals. Validate these first for high-yield KG expansion.</li>
)";
    }

    if (counts[InsightType::META_PATH_ANOMALY] > 0) {
        html << R"(                    <li><strong>Inspect Meta-Path Anomalies:</strong> The )" << counts[InsightType::META_PATH_ANOMALY] << R"( rare templates may expose novel mechanisms that standard pattern mining misses.</li>
)";
    }

    if (counts[InsightType::INTERVENTION_BOTTLENECK] > 0) {
        html << R"(                    <li><strong>Stress-Test Bottleneck Nodes:</strong> The )" << counts[InsightType::INTERVENTION_BOTTLENECK] << R"( bottlenecks sit on many causal pathways and are high-leverage intervention points.</li>
)";
    }

    if (counts[InsightType::COMPETING_MECHANISM] > 0) {
        html << R"(                    <li><strong>Design Discriminative Experiments:</strong> The )" << counts[InsightType::COMPETING_MECHANISM] << R"( competing mechanisms represent alternative explanations for shared outcomes.</li>
)";
    }

    if (counts[InsightType::SCHEMA_REPAIR] > 0) {
        html << R"(                    <li><strong>Apply Schema Repairs Carefully:</strong> The )" << counts[InsightType::SCHEMA_REPAIR] << R"( repair suggestions provide executable consistency improvements. Validate top actions before insertion.</li>
)";
    }

    if (counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] > 0) {
        html << R"(                    <li><strong>Explore Cross-Community Bridges:</strong> The )" << counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] << R"( bridge mechanisms connect distant domains and can seed interdisciplinary hypotheses.</li>
)";
    }

    if (counts[InsightType::PATH_RANK] > 0) {
        html << R"(                    <li><strong>Validate Path-Ranked Links:</strong> The )" << counts[InsightType::PATH_RANK] << R"( path-ranked links are supported by multiple short graph paths. Prioritize high-confidence candidates for validation.</li>
)";
    }

    if (counts[InsightType::LONG_CHAIN] > 0) {
        html << R"(                    <li><strong>Review Long-Chain Links:</strong> The )" << counts[InsightType::LONG_CHAIN] << R"( long-chain reasoning paths highlight distant multi-hop connections. Use them to trace extended mechanisms or thematic linkages.</li>
)";
    }

    if (counts[InsightType::META_PATH_PATTERN] > 0) {
        html << R"(                    <li><strong>Interpret Meta-Path Patterns:</strong> The )" << counts[InsightType::META_PATH_PATTERN] << R"( frequent relation templates summarize recurring multi-hop structures. Use them to guide querying and validation strategies.</li>
)";
    }

    if (counts[InsightType::AUTHOR_CHAIN] > 0) {
        html << R"(                    <li><strong>Track Citation Trails:</strong> The )" << counts[InsightType::AUTHOR_CHAIN] << R"( author reference chains reveal citation pathways across authors. Use them to map scholarly influence or identify key bridges.</li>
)";
    }

    if (counts[InsightType::CO_AUTHORSHIP] > 0) {
        html << R"(                    <li><strong>Explore Collaboration Networks:</strong> The )" << counts[InsightType::CO_AUTHORSHIP] << R"( co-authorship patterns reveal research clusters and institutional partnerships. Use them to identify active collaborators and potential interdisciplinary teams.</li>
)";
    }

    if (counts[InsightType::CITATION_IMPACT] > 0) {
        html << R"(                    <li><strong>Identify Influential Works:</strong> The )" << counts[InsightType::CITATION_IMPACT] << R"( highly cited/authored entities represent seminal contributions. Prioritize these for literature review and theoretical grounding.</li>
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
    if (counts[InsightType::MOTIF] > 0) {
        html << R"(                    <li><strong>Investigate Recurring Motifs:</strong> The )" << counts[InsightType::MOTIF] << R"( detected motifs represent frequently co-occurring concept clusters. Use them to identify recurring structural patterns worth documenting.</li>
)";
    }
    if (counts[InsightType::DIFFUSION] > 0) {
        html << R"(                    <li><strong>Follow Influence Pathways:</strong> The )" << counts[InsightType::DIFFUSION] << R"( diffusion pathways reveal how concepts propagate through the graph. Use them to map likely transmission routes for knowledge or influence.</li>
)";
    }
    if (counts[InsightType::META_PATH] > 0) {
        html << R"(                    <li><strong>Validate Meta-Path Links:</strong> The )" << counts[InsightType::META_PATH] << R"( meta-path links connect entities through typed relation sequences. Validate higher-confidence candidates for knowledge graph augmentation.</li>
)";
    }
    if (counts[InsightType::CAUSAL_CHAIN] > 0) {
        html << R"(                    <li><strong>Verify Causal Chains:</strong> The )" << counts[InsightType::CAUSAL_CHAIN] << R"( causal chains trace directed paths via causal relation types. Prioritize chains with strong supporting evidence for mechanistic validation.</li>
)";
    }
    if (counts[InsightType::INTERVENTION_POINT] > 0) {
        html << R"(                    <li><strong>Target Intervention Points:</strong> The )" << counts[InsightType::INTERVENTION_POINT] << R"( intervention points are nodes whose removal would disconnect causal chains. These are high-value targets for experimental design.</li>
)";
    }
    if (counts[InsightType::FEEDBACK_LOOP] > 0) {
        html << R"(                    <li><strong>Examine Feedback Loops:</strong> The )" << counts[InsightType::FEEDBACK_LOOP] << R"( feedback loops expose cycles in causal or directional relations. These may indicate self-reinforcing mechanisms or oscillatory dynamics.</li>
)";
    }
    if (counts[InsightType::CONFOUNDER] > 0) {
        html << R"(                    <li><strong>Control for Confounders:</strong> The )" << counts[InsightType::CONFOUNDER] << R"( identified confounders connect to both ends of suspected causal relations. Account for these in experimental or statistical analyses.</li>
)";
    }
    if (counts[InsightType::TAXONOMY] > 0) {
        html << R"(                    <li><strong>Refine Taxonomic Structure:</strong> The )" << counts[InsightType::TAXONOMY] << R"( taxonomic relations surface is-a and part-of hierarchies. Use them to improve ontological consistency and support classification tasks.</li>
)";
    }
    if (counts[InsightType::DOMAIN_BRIDGE] > 0) {
        html << R"(                    <li><strong>Leverage Domain Bridges:</strong> The )" << counts[InsightType::DOMAIN_BRIDGE] << R"( domain bridge nodes connect distinct disciplines. These are prime candidates for interdisciplinary hypotheses and cross-domain knowledge transfer.</li>
)";
    }
    if (counts[InsightType::LOGICAL_ENTAILMENT] > 0) {
        html << R"(                    <li><strong>Expand via Logical Entailments:</strong> The )" << counts[InsightType::LOGICAL_ENTAILMENT] << R"( entailments derive implicit implications from existing relations. Validate and add confirmed entailments to enrich the knowledge graph.</li>
)";
    }
    if (counts[InsightType::COMPOSITIONAL_REASONING] > 0) {
        html << R"(                    <li><strong>Test Compositional Relations:</strong> The )" << counts[InsightType::COMPOSITIONAL_REASONING] << R"( compositional inferences combine relation algebra to propose new links. Verify whether composed paths reflect genuine domain knowledge.</li>
)";
    }
    if (counts[InsightType::EXPLANATORY_CHAIN] > 0) {
        html << R"(                    <li><strong>Trace Explanatory Chains:</strong> The )" << counts[InsightType::EXPLANATORY_CHAIN] << R"( explanatory chains connect distant concepts through multi-hop paths. Use them to build narrative explanations linking remote but related findings.</li>
)";
    }
    if (counts[InsightType::SCHEMA_VIOLATION] > 0) {
        html << R"(                    <li><strong>Address Schema Violations:</strong> The )" << counts[InsightType::SCHEMA_VIOLATION] << R"( violations highlight entities or relations that break expected graph patterns. Resolve these to improve data quality and consistency.</li>
)";
    }
    if (counts[InsightType::TRANSITIVE_CLOSURE] > 0) {
        html << R"(                    <li><strong>Close Transitive Gaps:</strong> The )" << counts[InsightType::TRANSITIVE_CLOSURE] << R"( transitive closure gaps reveal links implied by transitivity but not yet recorded. Validate and add confirmed closures to increase graph completeness.</li>
)";
    }

    html << R"HTML(                </ol>
            </div>
        </section>
)HTML";

    // ========================================================================
    // SUGGESTED FUTURE WORK - Dynamic based on actual findings
    // ========================================================================

    html << R"HTML(
        <!-- Suggested Future Work Section -->
        <section id="future-work" class="module" style="border-left-color: var(--theme-misc)">
            <div class="module-header">
                <h2 style="color: var(--theme-misc)">Suggested Future Work</h2>
                <p class="definition">Tailored research directions based on the epistemological distribution and gaps in this analysis</p>
            </div>
            <div class="module-content">
)HTML";

    // Calculate total insights and percentages
    int total_insights = static_cast<int>(insights.insights.size());
    int comb_count = category_counts[InsightCategory::COMBINATORIAL];
    int expl_count = category_counts[InsightCategory::EXPLORATORY];
    int trans_count = category_counts[InsightCategory::TRANSFORMATIONAL];

    double comb_pct = (total_insights > 0) ? (100.0 * comb_count / total_insights) : 0.0;
    double expl_pct = (total_insights > 0) ? (100.0 * expl_count / total_insights) : 0.0;
    double trans_pct = (total_insights > 0) ? (100.0 * trans_count / total_insights) : 0.0;

    // Introduction based on category distribution
    html << R"HTML(                <h3>Epistemological Analysis of This Report</h3>
                <p>This analysis generated <strong>)HTML" << total_insights << R"HTML( insights</strong> distributed across Margaret Boden's creativity framework as follows:</p>
                <ul>
                    <li>🧩 <strong>Combinatorial</strong> (Pattern Detection): )HTML" << comb_count << " (" << std::fixed << std::setprecision(1) << comb_pct << R"HTML(%)</li>
                    <li>🔍 <strong>Exploratory</strong> (Connection Discovery): )HTML" << expl_count << " (" << expl_pct << R"HTML(%)</li>
                    <li>🔄 <strong>Transformational</strong> (Conceptual Reframing): )HTML" << trans_count << " (" << trans_pct << R"HTML(%)</li>
                </ul>
)HTML";

    // Category-based analysis and recommendations
    html << R"HTML(
                <h3>1. Category Distribution Insights</h3>
)HTML";

    // Determine dominant category
    if (comb_count > expl_count && comb_count > trans_count) {
        html << R"HTML(                <p><strong>Dominant Mode: Combinatorial (Pattern-Based)</strong></p>
                <p>This report is <em>pattern-heavy</em>, with )HTML" << comb_pct << R"HTML(% of insights emerging from structural combinations and motif detection. This suggests:</p>
                <ul>
                    <li><strong>Strength:</strong> The knowledge graph contains rich, detectable structural patterns suitable for automated discovery.</li>
                    <li><strong>Opportunity:</strong> To advance toward exploratory and transformational modes, implement deeper path-traversal operators and hypothesis generation mechanisms.</li>
                    <li><strong>Next Steps:</strong> Develop meta-pattern operators that identify patterns <em>across patterns</em>, enabling cross-domain structural mappings.</li>
                </ul>
)HTML";
    } else if (expl_count > comb_count && expl_count > trans_count) {
        html << R"HTML(                <p><strong>Dominant Mode: Exploratory (Connection-Based)</strong></p>
                <p>This report is <em>connection-heavy</em>, with )HTML" << expl_pct << R"HTML(% of insights arising from path traversal and link prediction. This suggests:</p>
                <ul>
                    <li><strong>Strength:</strong> The system effectively navigates the graph to discover non-obvious relationships and multi-hop reasoning chains.</li>
                    <li><strong>Opportunity:</strong> To advance toward transformational creativity, add operators that propose ontology revisions or challenge existing categorizations.</li>
                    <li><strong>Next Steps:</strong> Implement causal inference mechanisms that distinguish correlation from causation in discovered paths.</li>
                </ul>
)HTML";
    } else if (trans_count > 0 && trans_count >= comb_count && trans_count >= expl_count) {
        html << R"HTML(                <p><strong>Dominant Mode: Transformational (Reframing)</strong></p>
                <p>This report demonstrates significant <em>transformational capacity</em>, with )HTML" << trans_pct << R"HTML(% of insights challenging existing structures. This suggests:</p>
                <ul>
                    <li><strong>Strength:</strong> The system is successfully identifying schema violations, confounders, and structural anomalies that warrant conceptual revision.</li>
                    <li><strong>Opportunity:</strong> Leverage these findings to implement semi-automated ontology refinement workflows.</li>
                    <li><strong>Next Steps:</strong> Develop interactive tools allowing domain experts to approve, reject, or modify proposed schema changes.</li>
                </ul>
)HTML";
    } else {
        html << R"HTML(                <p><strong>Balanced Distribution</strong></p>
                <p>This report shows relatively balanced distribution across all three modes, suggesting a mature analytical capability. Continue expanding all operator types while focusing on:</p>
                <ul>
                    <li>Deepening transformational mechanisms for ontology revision</li>
                    <li>Enhancing causal reasoning beyond pattern detection</li>
                    <li>Implementing cross-corpus comparative analysis</li>
                </ul>
)HTML";
    }

    // Transformational gap analysis
    if (trans_pct < 15.0) {
        html << R"HTML(
                <h3>2. Transformational Creativity Gap</h3>
                <p><strong>⚠️ Low Transformational Output:</strong> Only )HTML" << trans_count << R"HTML( transformational insights ()HTML" << trans_pct << R"HTML(%) were generated. To advance toward transformational creativity:</p>
                <ul>
                    <li><strong>Implement Ontology Revision Operators:</strong> Develop algorithms that propose schema modifications when recurring anomalies suggest structural inadequacies.</li>
)HTML";
        if (counts[InsightType::SCHEMA_VIOLATION] > 0) {
            html << R"HTML(                    <li><strong>Leverage Schema Violations:</strong> The )HTML" << counts[InsightType::SCHEMA_VIOLATION] << R"HTML( schema violations identified are prime candidates for ontology refinement—investigate whether these represent data errors or genuine conceptual mismatches.</li>
)HTML";
        }
        if (counts[InsightType::CONFOUNDER] > 0) {
            html << R"HTML(                    <li><strong>Investigate Confounders:</strong> The )HTML" << counts[InsightType::CONFOUNDER] << R"HTML( confounders reveal hidden variables affecting apparent relationships—use these to refine causal models.</li>
)HTML";
        }
        if (counts[InsightType::HYPOTHESES_1] == 0 && counts[InsightType::HYPOTHESES_2] == 0 && counts[InsightType::HYPOTHESES_3] == 0) {
            html << R"HTML(                    <li><strong>Hypothesis Generation Available:</strong> Run <code>hypotheses_1</code> (Bayesian), <code>hypotheses_2</code> (mechanistic), or <code>hypotheses_3</code> (counterfactual) operators to generate testable hypotheses about missing relationships and alternative classification schemes.</li>
)HTML";
        }
        html << R"HTML(                </ul>
)HTML";
    }

    // Missing insight type analysis
    html << R"HTML(
                <h3>3. Operator Coverage Analysis</h3>
                <p>The following capabilities were not observed in this analysis:</p>
                <ul>
)HTML";

    // Check for missing causal insights
    if (counts[InsightType::CAUSAL_CHAIN] == 0 && counts[InsightType::INTERVENTION_POINT] == 0 &&
        counts[InsightType::FEEDBACK_LOOP] == 0 && counts[InsightType::CONFOUNDER] == 0) {
        html << R"HTML(                    <li><strong>Causal Reasoning (✓ Implemented, Not Run):</strong> Operators for causal chains, intervention points, feedback loops, and confounders are available but were not activated in this run.
                        <ul>
                            <li><strong>System Status:</strong> <code>causal_chains</code>, <code>intervention_points</code>, <code>feedback_loops</code>, <code>confounders</code> are fully implemented</li>
                            <li><strong>Why Not Observed:</strong> Either these operators were not included in the run parameters, or the graph lacks causal relation types (e.g., "causes", "leads to", "results in")</li>
                            <li><em>Action:</em> Run with <code>-p "causal_chains,intervention_points,feedback_loops,confounders"</code> or ensure extraction captures causal language</li>
                        </ul>
                    </li>
)HTML";
    }

    // Check for missing taxonomic insights
    if (counts[InsightType::TAXONOMY] == 0) {
        html << R"HTML(                    <li><strong>Taxonomic Structure (✓ Implemented, Not Run):</strong> Taxonomy operator is available but was not activated or found no hierarchical relations.
                        <ul>
                            <li><strong>System Status:</strong> <code>taxonomy</code> operator is fully implemented</li>
                            <li><strong>Why Not Observed:</strong> Either the operator was not run, or the graph lacks hierarchical relation types (e.g., "is-a", "part-of", "subclass-of")</li>
                            <li><em>Action:</em> Run with <code>-p taxonomy</code> and ensure extraction captures hierarchical relations for ontological organization</li>
                        </ul>
                    </li>
)HTML";
    }

    // Check for missing logical entailments
    if (counts[InsightType::LOGICAL_ENTAILMENT] == 0 && counts[InsightType::COMPOSITIONAL_REASONING] == 0) {
        html << R"HTML(                    <li><strong>Logical Inference (✓ Implemented, Not Run):</strong> Operators for logical entailment and compositional reasoning are available but were not activated.
                        <ul>
                            <li><strong>System Status:</strong> <code>logical_entailment</code>, <code>compositional_reasoning</code>, <code>transitive_closure</code> are fully implemented</li>
                            <li><strong>Why Not Observed:</strong> These operators were not included in the run parameters</li>
                            <li><em>Action:</em> Run with <code>-p "logical_entailment,compositional_reasoning,transitive_closure"</code> to derive implicit facts from explicit relations and dramatically increase knowledge graph completeness</li>
                        </ul>
                    </li>
)HTML";
    }

    // Check for missing diffusion/author chains
    if (counts[InsightType::DIFFUSION] == 0 && counts[InsightType::AUTHOR_CHAIN] == 0 &&
        counts[InsightType::CO_AUTHORSHIP] == 0 && counts[InsightType::CITATION_IMPACT] == 0) {
        html << R"HTML(                    <li><strong>Citation/Influence Networks (✓ Implemented, Not Run):</strong> Bibliometric operators are available but were not activated or found no citation data.
                        <ul>
                            <li><strong>System Status:</strong> <code>author_chain</code>, <code>co_authorship</code>, <code>citation_impact</code>, <code>diffusion</code> are fully implemented</li>
                            <li><strong>Why Not Observed:</strong> Either these operators were not run, or the graph lacks author/citation metadata (e.g., "cites", "authored-by", "co-authored" relations)</li>
                            <li><em>Action:</em> Run with <code>-p "author_chain,co_authorship,citation_impact,diffusion"</code> and ensure extraction captures bibliometric data to reveal knowledge transmission pathways</li>
                        </ul>
                    </li>
)HTML";
    }

    html << R"HTML(                </ul>

                <h3>4. Leveraging Strong Discovery Areas</h3>
                <p>Based on the insight types that <em>were</em> successfully generated, prioritize the following extensions:</p>
                <ul>
)HTML";

    // Highlight successful areas and suggest extensions
    if (counts[InsightType::COMMUNITY_DETECTION] > 0 || counts[InsightType::COMMUNITY_LINK] > 0 ||
        counts[InsightType::MULTI_RESOLUTION_COMMUNITY] > 0 || counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] > 0) {
        html << R"HTML(                    <li><strong>Community Structure (✓ Active):</strong> With )HTML" << (counts[InsightType::COMMUNITY_DETECTION] + counts[InsightType::COMMUNITY_LINK] + counts[InsightType::MULTI_RESOLUTION_COMMUNITY] + counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP]) << R"HTML( community-related insights:
                        <ul>
)HTML";
        if (counts[InsightType::MULTI_RESOLUTION_COMMUNITY] == 0) {
            html << R"HTML(                            <li><strong>Available:</strong> Run <code>multi_resolution_community</code> operator to reveal hierarchical structure at multiple scales</li>
)HTML";
        }
        if (counts[InsightType::CROSS_COMMUNITY_BRIDGE_MAP] == 0) {
            html << R"HTML(                            <li><strong>Available:</strong> Run <code>cross_community_bridge_map</code> operator to identify interdisciplinary synthesis opportunities</li>
)HTML";
        }
        html << R"HTML(                            <li>Use community labels as features for supervised learning tasks (e.g., entity classification)</li>
                            <li>Develop community-aware recommendation systems</li>
                        </ul>
                    </li>
)HTML";
    }

    if (counts[InsightType::BRIDGE] > 0 || counts[InsightType::DOMAIN_BRIDGE] > 0) {
        html << R"HTML(                    <li><strong>Bridge Entities (✓ Active):</strong> The )HTML" << (counts[InsightType::BRIDGE] + counts[InsightType::DOMAIN_BRIDGE]) << R"HTML( bridge nodes identified are prime candidates for:
                        <ul>
                            <li>Cross-domain knowledge transfer and analogy generation</li>
                            <li>Interdisciplinary hypothesis formulation</li>
                            <li>Targeted expert interviews (bridges often represent boundary-spanning concepts)</li>
                        </ul>
                    </li>
)HTML";
    }

    if (counts[InsightType::MOTIF] > 0 || counts[InsightType::META_PATTERN] > 0) {
        html << R"HTML(                    <li><strong>Recurring Motifs (✓ Active):</strong> The )HTML" << (counts[InsightType::MOTIF] + counts[InsightType::META_PATTERN]) << R"HTML( detected patterns suggest:
                        <ul>
)HTML";
        if (counts[InsightType::META_PATTERN] == 0 && counts[InsightType::MOTIF] > 0) {
            html << R"HTML(                            <li><strong>Available:</strong> Run <code>meta_pattern</code> operator to identify patterns-of-patterns (structural templates across motifs)</li>
)HTML";
        }
        html << R"HTML(                            <li>Develop a motif/pattern library as reusable knowledge templates</li>
                            <li>Implement pattern-based querying (find similar structural patterns elsewhere)</li>
                            <li>Use pattern frequency as a measure of conceptual importance or centrality</li>
                        </ul>
                    </li>
)HTML";
    }

    if (counts[InsightType::HYPOTHESES_1] > 0 || counts[InsightType::HYPOTHESES_2] > 0) {
        html << R"HTML(                    <li><strong>Link Prediction (✓ Active):</strong> With )HTML" << (counts[InsightType::HYPOTHESES_1] + counts[InsightType::HYPOTHESES_2]) << R"HTML( predicted links:
                        <ul>
                            <li>Validate high-confidence predictions through expert review or literature search</li>
                            <li>Add confirmed predictions back to the graph to increase density and enable deeper multi-hop reasoning</li>
                            <li>Use prediction confidence scores as uncertainty estimates for downstream tasks</li>
                        </ul>
                    </li>
)HTML";
    }

    html << R"HTML(                </ul>

                <h3>5. Advancing Toward Epistemic Reliability</h3>
                <p>From an epistemological perspective (justification, grounding, calibration), the system includes:</p>
                <ul>
                    <li><strong>Provenance Tracking (✓ Implemented):</strong> Each insight now includes source document links in its JSON output, enabling one-click verification. Spotlight sections in HTML reports display these with the 📄 Sources indicator.</li>
                    <li><strong>Confidence Calibration (Recommendation):</strong> Current scores reflect heuristic-based confidence; implement Bayesian or statistical methods for well-calibrated uncertainty estimates.</li>
                    <li><strong>External Validation:</strong> Link discovered entities to authoritative knowledge bases (Wikidata, domain ontologies) to validate extraction accuracy.</li>
                    <li><strong>Human-in-the-Loop Workflows:</strong> Design interfaces for domain experts to approve/reject/refine insights, creating feedback loops for continuous improvement.</li>
                    <li><strong>Bias Audits:</strong> Analyze whether discovered patterns systematically privilege certain perspectives or domains—implement fairness metrics for knowledge representation.</li>
                </ul>

                <h3>6. Long-Term Research Directions</h3>
                <p>To truly achieve transformational machine creativity—the ability to restructure conceptual spaces rather than merely navigate them—this project should pursue:</p>
                <ul>
                    <li><strong>Autonomous Ontology Evolution:</strong> Systems that propose, test, and adopt schema revisions based on recurring anomalies or improved explanatory power.</li>
                    <li><strong>Causal World Models:</strong> Move beyond correlation to build structural causal models enabling counterfactual reasoning ("what would happen if...?").</li>
                    <li><strong>Meta-Learning Over Operators:</strong> Analyze which discovery operators produce the most valuable insights for specific domains, then adaptively prioritize them.</li>
                    <li><strong>Collaborative Human-AI Knowledge Construction:</strong> Interactive systems where experts and AI co-construct knowledge through dialogue, challenging each other's assumptions.</li>
                    <li><strong>Multimodal Integration:</strong> Extend beyond text to incorporate visual, spatial, and temporal data, grounding abstract concepts in perceptual evidence.</li>
                </ul>

                <div style="margin-top: 30px; padding: 20px; background: rgba(79, 195, 247, 0.1); border-left: 4px solid var(--theme-misc); border-radius: 8px;">
                    <p style="margin: 0; font-style: italic;">
                        <strong>Philosophical Positioning:</strong> This project deliberately occupies a middle ground between neural generation (fluent but opaque) and symbolic reasoning (transparent but brittle). The )HTML" << comb_pct << R"HTML(% combinatorial, )HTML" << expl_pct << R"HTML(% exploratory, and )HTML" << trans_pct << R"HTML(% transformational distribution reveals the current epistemological profile. The path forward lies in deepening hybrid neuro-symbolic integration—leveraging neural pattern recognition to generate candidate structures, and symbolic reasoning to validate, explain, and refine them. The ultimate goal is <strong>epistemic reliability over fluent generation</strong>: systems that produce not just plausible claims, but <em>justified, traceable, and contestable</em> knowledge.
                    </p>
                </div>
            </div>
        </section>

        <footer>
            <p>This report was automatically generated by the Knowledge Discovery Engine.<br>
            All insights should be validated by domain experts before taking action.</p>
        </footer>
    </div>

    <!-- Export Button -->
    <button class="export-btn" id="exportBtn">Export PDF</button>

    <!-- Chart.js Library -->
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>

    <!-- Interactive Features JavaScript -->
    <script>
    (function() {
        'use strict';

        // ====================================================================
        // 0. BUTTON EVENT LISTENERS
        // ====================================================================
        const expandAllBtn = document.getElementById('expandAllBtn');
        const collapseAllBtn = document.getElementById('collapseAllBtn');
        const exportBtn = document.getElementById('exportBtn');

        if (expandAllBtn) {
            expandAllBtn.addEventListener('click', function() {
                document.querySelectorAll('section:not(#statistics)').forEach(s => {
                    s.classList.remove('collapsed');
                    Array.from(s.children).forEach((child, idx) => {
                        if (idx > 0) child.style.display = '';
                    });
                });
            });
        }

        if (collapseAllBtn) {
            collapseAllBtn.addEventListener('click', function() {
                document.querySelectorAll('section:not(#statistics)').forEach(s => {
                    s.classList.add('collapsed');
                    Array.from(s.children).forEach((child, idx) => {
                        if (idx > 0) child.style.display = 'none';
                    });
                });
            });
        }

        if (exportBtn) {
            exportBtn.addEventListener('click', function() {
                window.print();
            });
        }

        // ====================================================================
        // 1. COLLAPSIBLE SECTIONS
        // ====================================================================
        const allSections = Array.from(document.querySelectorAll('section:not(#statistics)'));
        allSections.forEach((section, index) => {
            const header = section.querySelector('h2');
            if (!header) return;

            // Start all sections collapsed
            section.classList.add('collapsed');
            const children = Array.from(section.children);
            children.forEach((child, idx) => {
                if (idx > 0) { // Skip the h2
                    child.style.display = 'none';
                }
            });

            // Expand the first two sections (categories overview and executive summary)
            if (index < 2) {
                section.classList.remove('collapsed');
                children.forEach((child, idx) => {
                    if (idx > 0) {
                        child.style.display = '';
                    }
                });
            }

            header.addEventListener('click', function() {
                section.classList.toggle('collapsed');
                const children = Array.from(section.children);
                children.forEach((child, idx) => {
                    if (idx > 0) { // Skip the h2
                        child.style.display = section.classList.contains('collapsed') ? 'none' : '';
                    }
                });
            });
        });

        // ====================================================================
        // 2. SEARCH FUNCTIONALITY
        // ====================================================================
        const searchBox = document.getElementById('searchBox');
        if (searchBox) {
            searchBox.addEventListener('input', function(e) {
                const query = e.target.value.toLowerCase();
                let visibleCount = 0;

                // Search through table rows
                document.querySelectorAll('.data-table tbody tr').forEach(row => {
                    const text = row.textContent.toLowerCase();
                    const matches = text.includes(query);
                    row.style.display = matches ? '' : 'none';
                    if (matches) visibleCount++;
                });

                // Search through clusters
                document.querySelectorAll('.cluster').forEach(cluster => {
                    const text = cluster.textContent.toLowerCase();
                    cluster.style.display = text.includes(query) ? '' : 'none';
                });

                // Search through module sections - show/hide entire sections
                document.querySelectorAll('section[id^="module-"]').forEach(section => {
                    const text = section.textContent.toLowerCase();
                    if (query === '') {
                        section.style.display = '';
                    } else {
                        section.style.display = text.includes(query) ? '' : 'none';
                    }
                });

                updateVisibleCounts();
            });
        }

        // ====================================================================
        // 3. CONFIDENCE FILTERING
        // ====================================================================
        document.querySelectorAll('.filter-score').forEach(checkbox => {
            checkbox.addEventListener('change', function() {
                const activeFilters = Array.from(document.querySelectorAll('.filter-score:checked'))
                    .map(cb => cb.value);

                document.querySelectorAll('.data-table tbody tr').forEach(row => {
                    const scoreCell = row.querySelector('td:nth-child(2)');
                    if (!scoreCell) return;

                    const scoreText = scoreCell.textContent.trim();
                    const score = parseFloat(scoreText);

                    if (isNaN(score)) {
                        row.style.display = '';
                        return;
                    }

                    let show = false;
                    if (activeFilters.includes('high') && score >= 0.7) show = true;
                    if (activeFilters.includes('med') && score >= 0.4 && score < 0.7) show = true;
                    if (activeFilters.includes('low') && score < 0.4) show = true;

                    row.style.display = show ? '' : 'none';
                });

                updateVisibleCounts();
            });
        });

        function updateVisibleCounts() {
            document.querySelectorAll('section[id^="module-"]').forEach(section => {
                const table = section.querySelector('.data-table tbody');
                if (!table) return;

                const total = table.querySelectorAll('tr').length;
                const visible = table.querySelectorAll('tr:not([style*="display: none"])').length;

                const countEl = section.querySelector('.module-header .count');
                if (countEl && total > 0) {
                    const originalText = countEl.textContent;
                    if (visible < total) {
                        countEl.textContent = originalText.replace(/Total: \d+/, `Showing: ${visible} / ${total}`);
                    } else {
                        countEl.textContent = originalText.replace(/Showing: \d+ \/ /, 'Total: ');
                    }
                }
            });
        }

        // ====================================================================
        // 4. ENTITY HIGHLIGHTING (Cross-Reference)
        // ====================================================================
        function highlightEntity(entityName) {
            // Clear previous highlights
            document.querySelectorAll('.highlighted').forEach(el => {
                el.classList.remove('highlighted');
            });

            if (!entityName) return;

            const normalizedSearch = entityName.toLowerCase().trim();
            let firstMatch = null;

            // Highlight in tables
            document.querySelectorAll('.data-table td').forEach(cell => {
                if (cell.textContent.toLowerCase().includes(normalizedSearch)) {
                    cell.classList.add('highlighted');
                    if (!firstMatch) firstMatch = cell;
                }
            });

            // Highlight entity badges
            document.querySelectorAll('.entity').forEach(badge => {
                if (badge.textContent.toLowerCase().includes(normalizedSearch)) {
                    badge.classList.add('highlighted');
                    if (!firstMatch) firstMatch = badge;
                }
            });

            // Scroll to first match
            if (firstMatch) {
                firstMatch.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
        }

        // Add click handlers to entities
        document.querySelectorAll('.entity, .data-table td:first-child').forEach(el => {
            el.addEventListener('click', function(e) {
                e.stopPropagation();
                const entityName = this.textContent.trim();
                highlightEntity(entityName);
            });
        });

        // ====================================================================
        // 5. LAZY LOADING FOR LARGE TABLES
        // ====================================================================
        document.querySelectorAll('.data-table tbody').forEach(tbody => {
            const rows = Array.from(tbody.querySelectorAll('tr'));
            const INITIAL_ROWS = 20;

            if (rows.length <= INITIAL_ROWS) return; // Skip small sections

            // Hide rows after INITIAL_ROWS
            rows.slice(INITIAL_ROWS).forEach(row => {
                row.style.display = 'none';
                row.dataset.lazyHidden = 'true';
            });

            // Create "Load More" button
            const loadMoreBtn = document.createElement('button');
            loadMoreBtn.className = 'load-more-btn';
            loadMoreBtn.textContent = `Load ${rows.length - INITIAL_ROWS} more...`;

            loadMoreBtn.addEventListener('click', function() {
                rows.forEach(row => {
                    if (row.dataset.lazyHidden === 'true') {
                        row.style.display = '';
                        delete row.dataset.lazyHidden;
                    }
                });
                this.remove();
            });

            tbody.parentElement.insertAdjacentElement('afterend', loadMoreBtn);
        });

        // ====================================================================
        // 6. INTERACTIVE CHART
        // ====================================================================
        const chartCanvas = document.getElementById('insightChart');
        if (chartCanvas && typeof Chart !== 'undefined') {
            // Collect category counts from category cards
            const categoryData = {
                'Combinatorial': 0,
                'Exploratory': 0,
                'Transformational': 0
            };

            // Get data from the category overview cards
            document.querySelectorAll('.category-card').forEach(card => {
                const title = card.querySelector('h3');
                const statItems = card.querySelectorAll('.stat-item');
                if (title && statItems.length > 0) {
                    const categoryName = title.textContent.trim();
                    // First stat-item contains the count
                    const countValue = statItems[0].querySelector('.stat-value');
                    const count = countValue ? parseInt(countValue.textContent.trim()) || 0 : 0;
                    if (categoryData.hasOwnProperty(categoryName)) {
                        categoryData[categoryName] = count;
                    }
                }
            });

            const totalInsights = Object.values(categoryData).reduce((a, b) => a + b, 0);

            if (totalInsights > 0) {
                new Chart(chartCanvas, {
                    type: 'bar',
                    data: {
                        labels: ['🧩 Combinatorial', '🔍 Exploratory', '🔄 Transformational'],
                        datasets: [{
                            label: 'Number of Insights',
                            data: [
                                categoryData['Combinatorial'],
                                categoryData['Exploratory'],
                                categoryData['Transformational']
                            ],
                            backgroundColor: [
                                'rgba(74, 222, 128, 0.7)',   // Green for Combinatorial
                                'rgba(79, 195, 247, 0.7)',   // Blue for Exploratory
                                'rgba(232, 121, 249, 0.7)'   // Purple for Transformational
                            ],
                            borderColor: [
                                'rgba(74, 222, 128, 1)',
                                'rgba(79, 195, 247, 1)',
                                'rgba(232, 121, 249, 1)'
                            ],
                            borderWidth: 2,
                            borderRadius: 8
                        }]
                    },
                    options: {
                        indexAxis: 'y',
                        responsive: true,
                        maintainAspectRatio: true,
                        plugins: {
                            legend: {
                                display: false
                            },
                            title: {
                                display: true,
                                text: 'Insight Distribution by Category',
                                color: '#f8fafc',
                                font: { size: 16, weight: 'bold' },
                                padding: { bottom: 20 }
                            },
                            tooltip: {
                                backgroundColor: 'rgba(15, 23, 42, 0.95)',
                                padding: 12,
                                titleColor: '#f8fafc',
                                bodyColor: '#f8fafc',
                                borderColor: 'rgba(148, 163, 184, 0.3)',
                                borderWidth: 1,
                                callbacks: {
                                    label: function(context) {
                                        const value = context.parsed.x || 0;
                                        const percentage = ((value / totalInsights) * 100).toFixed(1);
                                        return `${value} insights (${percentage}%)`;
                                    },
                                    afterLabel: function(context) {
                                        const descriptions = [
                                            'Pattern Detection & Structural Combinations',
                                            'Path Finding & Connection Discovery',
                                            'Reframing & Perspective Shifts'
                                        ];
                                        return descriptions[context.dataIndex];
                                    }
                                }
                            }
                        },
                        scales: {
                            x: {
                                beginAtZero: true,
                                ticks: {
                                    color: '#94a3b8',
                                    font: { size: 11 }
                                },
                                grid: {
                                    color: 'rgba(148, 163, 184, 0.1)'
                                }
                            },
                            y: {
                                ticks: {
                                    color: '#f8fafc',
                                    font: { size: 13, weight: '600' },
                                    padding: 10
                                },
                                grid: {
                                    display: false
                                }
                            }
                        },
                        onClick: (event, activeElements) => {
                            if (activeElements.length > 0) {
                                const index = activeElements[0].index;
                                const sectionIds = [
                                    '#combinatorial-section',
                                    '#exploratory-section',
                                    '#transformational-section'
                                ];
                                const section = document.querySelector(sectionIds[index]);
                                if (section) {
                                    section.scrollIntoView({ behavior: 'smooth', block: 'start' });
                                }
                            }
                        }
                    }
                });
            }
        }

        // ====================================================================
        // 7. KEYBOARD SHORTCUTS
        // ====================================================================
        document.addEventListener('keydown', function(e) {
            // Ctrl/Cmd + K: Focus search
            if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
                e.preventDefault();
                if (searchBox) searchBox.focus();
            }

            // Escape: Clear search and highlights
            if (e.key === 'Escape') {
                if (searchBox) {
                    searchBox.value = '';
                    searchBox.dispatchEvent(new Event('input'));
                }
                highlightEntity(null);
            }
        });

        console.log('Interactive report features loaded successfully');
    })();
    </script>
)";

    // Add Chart.js visualization
    html << generate_chart_js_data(counts, insights);

    html << R"(
</body>
</html>
)HTML";

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

// ============== PATTERN LIBRARY EXPORT ==============
void ReportGenerator::export_pattern_library(const InsightCollection& insights, const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open pattern library file: " + output_path);
    }

    out << "{\n";
    out << "  \"meta\": {\n";
    out << "    \"generated_at\": \"" << get_current_timestamp() << "\",\n";
    out << "    \"run_id\": \"" << insights.run_id << "\",\n";
    out << "    \"total_patterns\": " << 0 << "\n";  // Will update below
    out << "  },\n";
    out << "  \"patterns\": [\n";

    int pattern_count = 0;
    bool first_pattern = true;

    // Extract patterns from motifs, k-truss, and meta-patterns
    for (const auto& insight : insights.insights) {
        if (insight.type != InsightType::MOTIF &&
            insight.type != InsightType::K_TRUSS &&
            insight.type != InsightType::META_PATTERN) {
            continue;
        }

        if (!first_pattern) {
            out << ",\n";
        }
        first_pattern = false;
        pattern_count++;

        out << "    {\n";
        out << "      \"id\": \"" << insight.insight_id << "\",\n";
        out << "      \"type\": \"" << get_insight_type_name(insight.type) << "\",\n";
        out << "      \"size\": " << insight.seed_nodes.size() << ",\n";
        out << "      \"confidence\": " << std::fixed << std::setprecision(3) << insight.score << ",\n";

        // Pattern structure (nodes)
        out << "      \"nodes\": [";
        for (size_t i = 0; i < insight.seed_labels.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << insight.seed_labels[i] << "\"";
        }
        out << "],\n";

        // Node IDs (for graph queries)
        out << "      \"node_ids\": [";
        for (size_t i = 0; i < insight.seed_nodes.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << insight.seed_nodes[i] << "\"";
        }
        out << "],\n";

        // Metadata from score_breakdown
        out << "      \"metadata\": {\n";
        bool first_meta = true;
        for (const auto& [key, value] : insight.score_breakdown) {
            if (!first_meta) out << ",\n";
            first_meta = false;
            out << "        \"" << key << "\": " << std::fixed << std::setprecision(3) << value;
        }
        out << "\n      },\n";

        // Description
        out << "      \"description\": \"";
        // Escape quotes in description
        std::string desc = insight.description;
        for (size_t i = 0; i < desc.size(); ++i) {
            if (desc[i] == '"') out << "\\\"";
            else if (desc[i] == '\\') out << "\\\\";
            else if (desc[i] == '\n') out << "\\n";
            else if (desc[i] == '\r') {} // Skip
            else out << desc[i];
        }
        out << "\",\n";

        // Source documents
        out << "      \"sources\": [";
        for (size_t i = 0; i < insight.source_documents.size() && i < 10; ++i) {
            if (i > 0) out << ", ";
            out << "\"" << insight.source_documents[i] << "\"";
        }
        out << "]\n";

        out << "    }";
    }

    out << "\n  ],\n";

    // Statistics by pattern type
    std::map<std::string, int> type_counts;
    std::map<std::string, int> size_distribution;

    for (const auto& insight : insights.insights) {
        if (insight.type == InsightType::MOTIF ||
            insight.type == InsightType::K_TRUSS ||
            insight.type == InsightType::META_PATTERN) {

            std::string type_name = get_insight_type_name(insight.type);
            type_counts[type_name]++;

            int size = static_cast<int>(insight.seed_nodes.size());
            std::string size_key = std::to_string(size) + "-node";
            size_distribution[size_key]++;
        }
    }

    out << "  \"statistics\": {\n";
    out << "    \"total_patterns\": " << pattern_count << ",\n";

    out << "    \"by_type\": {\n";
    bool first_type = true;
    for (const auto& [type, count] : type_counts) {
        if (!first_type) out << ",\n";
        first_type = false;
        out << "      \"" << type << "\": " << count;
    }
    out << "\n    },\n";

    out << "    \"by_size\": {\n";
    bool first_size = true;
    for (const auto& [size, count] : size_distribution) {
        if (!first_size) out << ",\n";
        first_size = false;
        out << "      \"" << size << "\": " << count;
    }
    out << "\n    }\n";

    out << "  },\n";

    // Usage guide
    out << "  \"usage\": {\n";
    out << "    \"description\": \"Pattern library for reuse and querying\",\n";
    out << "    \"query_by_id\": \"Use 'id' field to retrieve specific patterns\",\n";
    out << "    \"query_by_size\": \"Use 'size' field to find patterns of specific dimensions\",\n";
    out << "    \"query_by_nodes\": \"Use 'node_ids' to find patterns containing specific entities\",\n";
    out << "    \"similarity_search\": \"Compare 'nodes' arrays to find structurally similar patterns\"\n";
    out << "  }\n";

    out << "}\n";
    out.close();
}

// ============================================================================
// Visualization Helpers (Executive Summary Enhancements)
// ============================================================================

std::string ReportGenerator::generate_mini_subgraph_svg(const Insight& insight, int max_nodes) const {
    std::stringstream svg;

    // Extract unique nodes from witness edges with their labels
    std::set<std::string> node_set;
    std::vector<std::pair<std::string, std::string>> edge_list;
    std::map<std::string, std::string> node_labels;

    for (const auto& edge_id : insight.witness_edges) {
        const auto* edge = graph_.get_hyperedge(edge_id);
        if (!edge) continue;

        // Add sources and targets
        for (const auto& src : edge->sources) {
            node_set.insert(src);
            if (node_labels.count(src) == 0) {
                node_labels[src] = get_node_label(src);
            }
        }
        for (const auto& tgt : edge->targets) {
            node_set.insert(tgt);
            if (node_labels.count(tgt) == 0) {
                node_labels[tgt] = get_node_label(tgt);
            }
        }

        // Create edge pairs (source -> target)
        if (!edge->sources.empty() && !edge->targets.empty()) {
            edge_list.push_back(std::make_pair(edge->sources[0], edge->targets[0]));
        }

        if (node_set.size() >= static_cast<size_t>(max_nodes)) break;
    }

    // Convert to vector for indexing
    std::vector<std::string> nodes(node_set.begin(), node_set.end());
    if (nodes.size() > static_cast<size_t>(max_nodes)) {
        nodes.resize(max_nodes);
    }

    if (nodes.empty()) return "";

    // SVG dimensions - larger to accommodate labels
    const int width = 300;
    const int height = 280;
    const int padding = 40;
    const int node_radius = 8;
    const int legend_height = 60;

    svg << R"(<svg width=")" << width << R"(" height=")" << height << R"(" viewBox="0 0 )" << width << " " << height << R"(" xmlns="http://www.w3.org/2000/svg">)";
    svg << R"(<rect width=")" << width << R"(" height=")" << height << R"(" fill="#1e293b" rx="8"/>)";

    // Layout nodes in a circle
    std::map<std::string, std::pair<double, double>> positions;
    double center_x = width / 2.0;
    double center_y = (height - legend_height) / 2.0;
    double radius = std::min(width, height - legend_height) / 2.0 - padding;

    for (size_t i = 0; i < nodes.size(); i++) {
        double angle = 2.0 * M_PI * i / nodes.size() - M_PI / 2.0;
        double x = center_x + radius * std::cos(angle);
        double y = center_y + radius * std::sin(angle);
        positions[nodes[i]] = {x, y};
    }

    // Draw edges with arrows
    svg << "<defs><marker id=\"arrowhead\" markerWidth=\"6\" markerHeight=\"6\" refX=\"5\" refY=\"3\" orient=\"auto\">"
        << "<polygon points=\"0 0, 6 3, 0 6\" fill=\"#4fc3f7\"/></marker></defs>";
    svg << "<g opacity=\"0.7\">";
    for (const auto& [from, to] : edge_list) {
        if (positions.count(from) && positions.count(to)) {
            auto [x1, y1] = positions[from];
            auto [x2, y2] = positions[to];

            // Calculate shortened line to stop at node edge
            double dx = x2 - x1;
            double dy = y2 - y1;
            double len = std::sqrt(dx*dx + dy*dy);
            double ratio = (len - node_radius - 2) / len;
            double new_x2 = x1 + dx * ratio;
            double new_y2 = y1 + dy * ratio;

            svg << "<line x1=\"" << x1 << "\" y1=\"" << y1
                << "\" x2=\"" << new_x2 << "\" y2=\"" << new_y2
                << "\" stroke=\"#4fc3f7\" stroke-width=\"2\" marker-end=\"url(#arrowhead)\"/>";
        }
    }
    svg << "</g>";

    // Draw nodes with labels
    for (const auto& [node_id, pos] : positions) {
        auto [x, y] = pos;

        // Node circle
        svg << R"(<circle cx=")" << x << R"(" cy=")" << y << R"(" r=")" << node_radius
            << R"(" fill="#fbbf24" stroke="#0f172a" stroke-width="2"/>)";

        // Node label - truncate if too long
        std::string label = node_labels[node_id];
        if (label.length() > 15) {
            label = label.substr(0, 12) + "...";
        }

        // Position label below node
        svg << R"(<text x=")" << x << R"(" y=")" << (y + node_radius + 12)
            << R"(" font-size="10" fill="#94a3b8" text-anchor="middle" font-family="Inter, sans-serif">)"
            << label << R"(</text>)";
    }

    // Add legend at bottom
    int legend_y = height - legend_height + 10;
    svg << R"(<text x=")" << (width / 2) << R"(" y=")" << legend_y
        << R"(" font-size="11" font-weight="600" fill="#f8fafc" text-anchor="middle">Subgraph Structure</text>)";

    // Legend items
    int item_y = legend_y + 18;
    svg << R"(<circle cx="20" cy=")" << item_y << R"(" r="5" fill="#fbbf24"/>)";
    svg << R"(<text x="30" y=")" << (item_y + 4) << R"(" font-size="9" fill="#94a3b8">Entity</text>)";

    svg << "<line x1=\"100\" y1=\"" << item_y << "\" x2=\"130\" y2=\"" << item_y
        << "\" stroke=\"#4fc3f7\" stroke-width=\"2\" marker-end=\"url(#arrowhead)\"/>";
    svg << R"(<text x="135" y=")" << (item_y + 4) << R"(" font-size="9" fill="#94a3b8">Relation</text>)";

    svg << R"(<text x=")" << (width / 2) << R"(" y=")" << (item_y + 20)
        << R"(" font-size="8" fill="#64748b" text-anchor="middle">)"
        << nodes.size() << " nodes, " << edge_list.size() << " edges</text>)";

    svg << R"(</svg>)";
    return svg.str();
}

std::string ReportGenerator::generate_chart_js_data(const std::map<InsightType, int>& counts, const InsightCollection& insights) const {
    std::stringstream js;

    // Collect top 10 insight types by count
    std::vector<std::pair<std::string, int>> top_types;
    for (const auto& [type, count] : counts) {
        if (count > 0) {
            top_types.push_back({get_insight_type_name(type), count});
        }
    }
    std::sort(top_types.begin(), top_types.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (top_types.size() > 10) {
        top_types.resize(10);
    }

    // Count by category
    std::map<std::string, int> category_counts;
    for (const auto& insight : insights.insights) {
        category_counts[category_to_string(insight.category)]++;
    }

    js << R"(
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.js"></script>
    <script>
    (function() {
        const ctx = document.getElementById('insightChart');
        if (!ctx) return;

        new Chart(ctx, {
            type: 'bar',
            data: {
                labels: [)";

    // Add labels
    for (size_t i = 0; i < top_types.size(); i++) {
        js << "'" << top_types[i].first << "'";
        if (i < top_types.size() - 1) js << ", ";
    }

    js << R"(],
                datasets: [{
                    label: 'Number of Insights',
                    data: [)";

    // Add data
    for (size_t i = 0; i < top_types.size(); i++) {
        js << top_types[i].second;
        if (i < top_types.size() - 1) js << ", ";
    }

    js << R"(],
                    backgroundColor: 'rgba(79, 195, 247, 0.6)',
                    borderColor: 'rgba(79, 195, 247, 1)',
                    borderWidth: 2
                }]
            },
            options: {
                indexAxis: 'y',
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: false
                    },
                    title: {
                        display: true,
                        text: 'Top 10 Insight Types',
                        color: '#f8fafc',
                        font: { size: 16, weight: 'bold' }
                    }
                },
                scales: {
                    x: {
                        ticks: { color: '#94a3b8' },
                        grid: { color: 'rgba(148, 163, 184, 0.1)' }
                    },
                    y: {
                        ticks: { color: '#94a3b8' },
                        grid: { display: false }
                    }
                }
            }
        });
    })();
    </script>
    )";

    return js.str();
}

} // namespace kg

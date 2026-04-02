#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <stdexcept>

namespace kg {

// Knowledge Discovery Categories
// High-level classification of how insights generate new knowledge
enum class InsightCategory {
    COMBINATORIAL,    // Combining existing knowledge in new ways (patterns, structures)
    EXPLORATORY,      // Discovering new connections, paths, and unexplored areas
    TRANSFORMATIONAL  // Reframing understanding, suggesting new perspectives
};

inline std::string category_to_string(InsightCategory cat) {
    switch (cat) {
        case InsightCategory::COMBINATORIAL: return "combinatorial";
        case InsightCategory::EXPLORATORY: return "exploratory";
        case InsightCategory::TRANSFORMATIONAL: return "transformational";
        default: return "unknown";
    }
}

inline InsightCategory string_to_category(const std::string& s) {
    if (s == "combinatorial") return InsightCategory::COMBINATORIAL;
    if (s == "exploratory") return InsightCategory::EXPLORATORY;
    if (s == "transformational") return InsightCategory::TRANSFORMATIONAL;
    return InsightCategory::EXPLORATORY; // default
}

// Insight types
enum class InsightType {
    BRIDGE,
    COMPLETION,
    MOTIF,
    SUBSTITUTION,
    CONTRADICTION,
    ENTITY_RESOLUTION,
    CORE_PERIPHERY,
    TEXT_SIMILARITY,
    ARGUMENT_SUPPORT,
    ACTIVE_LEARNING,
    METHOD_OUTCOME,
    CENTRALITY,
    COMMUNITY_DETECTION,
    K_CORE,
    K_TRUSS,
    CLAIM_STANCE,
    RELATION_INDUCTION,
    ANALOGICAL_TRANSFER,
    UNCERTAINTY_SAMPLING,
    COUNTERFACTUAL,
    HYPEREDGE_PREDICTION,
    DIFFUSION,
    SURPRISE,
    COMMUNITY_LINK,
    PATH_RANK,
    LONG_CHAIN,
    META_PATH,
    META_PATH_PATTERN, // Global meta-path pattern mining (relation sequence templates)
    HYPOTHESES_1,
    HYPOTHESES_2,    // Graph-native mechanistic hypothesis generation with test plans
    HYPOTHESES_3,    // Counterfactual causal hypothesis generation (If X then Y because Z)
    MECHANISM_CONSOLIDATION, // Clustered mechanistic hypotheses/paths
    EVIDENCE_FUSION_LINK, // Multi-operator consensus link with provenance fusion
    META_PATH_ANOMALY, // Rare but plausible relation-sequence anomaly
    INTERVENTION_BOTTLENECK, // Nodes that bottleneck many causal pathways
    COMPETING_MECHANISM, // Alternative mechanisms for a shared outcome
    SCHEMA_REPAIR, // Suggested executable schema/closure/completion repairs
    CROSS_COMMUNITY_MECHANISM_BRIDGE, // Mechanistic chains spanning communities/domains
    RULE,
    EMBEDDING_LINK,  // TransE/RotatE/ComplEx embedding-based link prediction
    AUTHOR_CHAIN,
    CO_AUTHORSHIP,   // Co-authorship networks and collaboration patterns
    CITATION_IMPACT, // Citation counts and influential authors/works
    MULTI_RESOLUTION_COMMUNITY, // Hierarchical community structure at multiple scales
    CROSS_COMMUNITY_BRIDGE_MAP, // Bridges mapped to communities they connect
    META_PATTERN,    // Patterns across patterns - structural templates
    // Category 3: Causal & Mechanistic Discovery
    CAUSAL_CHAIN,        // Directed paths with causal relation types
    INTERVENTION_POINT,  // Nodes whose removal disconnects causal chains
    FEEDBACK_LOOP,       // Cycles in causal/directional relations
    CONFOUNDER,          // Nodes connected to both ends of suspected causal relations
    // Category 4: Advanced Reasoning & Structural Discovery
    TAXONOMY,            // Hierarchical is-a and part-of relationships
    DOMAIN_BRIDGE,       // Cross-domain or interdisciplinary connector nodes
    LOGICAL_ENTAILMENT,  // Inferred implications (if X and Y, then Z)
    COMPOSITIONAL_REASONING,  // Composed relations via relation algebra
    EXPLANATORY_CHAIN,   // Multi-hop explanation paths connecting distant concepts
    SCHEMA_VIOLATION,    // Entities or relations that violate expected patterns
    TRANSITIVE_CLOSURE,  // Missing links implied by transitive relations
    BIAS_AUDIT,         // Fairness and representation analysis of insights
    COMMUNITY_RECOMMENDATION,  // Entity recommendations based on community structure
    // Epistemic Discovery
    EVIDENCE_DEBT,        // High-impact nodes with narrow evidence provenance
    CONSENSUS_FRONTIER,   // Claim clusters with convergence or active disagreement
    BOUNDARY_CONDITION_MAP, // Conditions under which methods/claims hold
    FAILURE_MODE_TOPOLOGY,  // Recurring failure patterns across the graph
    BENCHMARK_DEPENDENCE,   // Claims concentrated on narrow evaluation regimes
    CONCEPT_DRIFT,          // Terms with divergent meanings across communities
    PREMISE_BOTTLENECK,     // Hidden premises that many downstream claims rely on
    TRANSLATION_GAP         // Theory-rich but practice-weak concept nodes
};

inline std::string insight_type_to_string(InsightType type) {
    switch (type) {
        case InsightType::BRIDGE: return "bridge";
        case InsightType::COMPLETION: return "completion";
        case InsightType::MOTIF: return "motif";
        case InsightType::SUBSTITUTION: return "substitution";
        case InsightType::CONTRADICTION: return "contradiction";
        case InsightType::ENTITY_RESOLUTION: return "entity_resolution";
        case InsightType::CORE_PERIPHERY: return "core_periphery";
        case InsightType::TEXT_SIMILARITY: return "text_similarity";
        case InsightType::ARGUMENT_SUPPORT: return "argument_support";
        case InsightType::ACTIVE_LEARNING: return "active_learning";
        case InsightType::METHOD_OUTCOME: return "method_outcome";
        case InsightType::CENTRALITY: return "centrality";
        case InsightType::COMMUNITY_DETECTION: return "community_detection";
        case InsightType::K_CORE: return "k_core";
        case InsightType::K_TRUSS: return "k_truss";
        case InsightType::CLAIM_STANCE: return "claim_stance";
        case InsightType::RELATION_INDUCTION: return "relation_induction";
        case InsightType::ANALOGICAL_TRANSFER: return "analogical_transfer";
        case InsightType::UNCERTAINTY_SAMPLING: return "uncertainty_sampling";
        case InsightType::COUNTERFACTUAL: return "counterfactual";
        case InsightType::HYPEREDGE_PREDICTION: return "hyperedge_prediction";
        case InsightType::DIFFUSION: return "diffusion";
        case InsightType::SURPRISE: return "surprise";
        case InsightType::COMMUNITY_LINK: return "community_link";
        case InsightType::PATH_RANK: return "path_rank";
        case InsightType::LONG_CHAIN: return "long_chain";
        case InsightType::META_PATH: return "meta_path";
        case InsightType::META_PATH_PATTERN: return "meta_path_pattern";
        case InsightType::HYPOTHESES_1: return "hypotheses_1";
        case InsightType::HYPOTHESES_2: return "hypotheses_2";
        case InsightType::HYPOTHESES_3: return "hypotheses_3";
        case InsightType::MECHANISM_CONSOLIDATION: return "mechanism_consolidation";
        case InsightType::EVIDENCE_FUSION_LINK: return "evidence_fusion_link";
        case InsightType::META_PATH_ANOMALY: return "meta_path_anomaly";
        case InsightType::INTERVENTION_BOTTLENECK: return "intervention_bottleneck";
        case InsightType::COMPETING_MECHANISM: return "competing_mechanism";
        case InsightType::SCHEMA_REPAIR: return "schema_repair";
        case InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE: return "cross_community_mechanism_bridge";
        case InsightType::RULE: return "rule";
        case InsightType::EMBEDDING_LINK: return "embedding_link";
        case InsightType::AUTHOR_CHAIN: return "author_chain";
        case InsightType::CO_AUTHORSHIP: return "co_authorship";
        case InsightType::CITATION_IMPACT: return "citation_impact";
        case InsightType::MULTI_RESOLUTION_COMMUNITY: return "multi_resolution_community";
        case InsightType::CROSS_COMMUNITY_BRIDGE_MAP: return "cross_community_bridge_map";
        case InsightType::META_PATTERN: return "meta_pattern";
        case InsightType::CAUSAL_CHAIN: return "causal_chain";
        case InsightType::INTERVENTION_POINT: return "intervention_point";
        case InsightType::FEEDBACK_LOOP: return "feedback_loop";
        case InsightType::CONFOUNDER: return "confounder";
        case InsightType::TAXONOMY: return "taxonomy";
        case InsightType::DOMAIN_BRIDGE: return "domain_bridge";
        case InsightType::LOGICAL_ENTAILMENT: return "logical_entailment";
        case InsightType::COMPOSITIONAL_REASONING: return "compositional_reasoning";
        case InsightType::EXPLANATORY_CHAIN: return "explanatory_chain";
        case InsightType::SCHEMA_VIOLATION: return "schema_violation";
        case InsightType::TRANSITIVE_CLOSURE: return "transitive_closure";
        case InsightType::BIAS_AUDIT: return "bias_audit";
        case InsightType::COMMUNITY_RECOMMENDATION: return "community_recommendation";
        case InsightType::EVIDENCE_DEBT: return "evidence_debt";
        case InsightType::CONSENSUS_FRONTIER: return "consensus_frontier";
        case InsightType::BOUNDARY_CONDITION_MAP: return "boundary_condition_map";
        case InsightType::FAILURE_MODE_TOPOLOGY: return "failure_mode_topology";
        case InsightType::BENCHMARK_DEPENDENCE: return "benchmark_dependence";
        case InsightType::CONCEPT_DRIFT: return "concept_drift";
        case InsightType::PREMISE_BOTTLENECK: return "premise_bottleneck";
        case InsightType::TRANSLATION_GAP: return "translation_gap";
        default: return "unknown";
    }
}

inline InsightType string_to_insight_type(const std::string& s) {
    if (s == "bridge") return InsightType::BRIDGE;
    if (s == "completion") return InsightType::COMPLETION;
    if (s == "motif") return InsightType::MOTIF;
    if (s == "substitution") return InsightType::SUBSTITUTION;
    if (s == "contradiction" || s == "contradictions") return InsightType::CONTRADICTION;
    if (s == "entity_resolution" || s == "entity-resolution" || s == "entityresolution" || s == "dedup") return InsightType::ENTITY_RESOLUTION;
    if (s == "core_periphery" || s == "core-periphery" || s == "coreperiphery" || s == "hub_authority" || s == "hub-authority") return InsightType::CORE_PERIPHERY;
    if (s == "text_similarity" || s == "text-similarity" || s == "textsimilarity" || s == "semantic" || s == "semantic_similarity") return InsightType::TEXT_SIMILARITY;
    if (s == "argument_support" || s == "argument-support" || s == "argument") return InsightType::ARGUMENT_SUPPORT;
    if (s == "active_learning" || s == "active-learning" || s == "active") return InsightType::ACTIVE_LEARNING;
    if (s == "method_outcome" || s == "method-outcome" || s == "method" || s == "outcome") return InsightType::METHOD_OUTCOME;
    if (s == "centrality" || s == "centrality_rank" || s == "centrality_rankings") return InsightType::CENTRALITY;
    if (s == "community_detection" || s == "community-detection" || s == "communities") return InsightType::COMMUNITY_DETECTION;
    if (s == "k_core" || s == "k-core" || s == "core") return InsightType::K_CORE;
    if (s == "k_truss" || s == "k-truss" || s == "truss") return InsightType::K_TRUSS;
    if (s == "claim_stance" || s == "claim-stance" || s == "stance") return InsightType::CLAIM_STANCE;
    if (s == "relation_induction" || s == "relation-induction" || s == "relation_type") return InsightType::RELATION_INDUCTION;
    if (s == "analogical_transfer" || s == "analogical-transfer" || s == "analogy") return InsightType::ANALOGICAL_TRANSFER;
    if (s == "uncertainty_sampling" || s == "uncertainty-sampling" || s == "uncertainty") return InsightType::UNCERTAINTY_SAMPLING;
    if (s == "counterfactual" || s == "counterfactual-probing") return InsightType::COUNTERFACTUAL;
    if (s == "hyperedge_prediction" || s == "hyperedge-prediction" || s == "hyperedge") return InsightType::HYPEREDGE_PREDICTION;
    if (s == "diffusion") return InsightType::DIFFUSION;
    if (s == "surprise") return InsightType::SURPRISE;
    if (s == "community_link" || s == "community") return InsightType::COMMUNITY_LINK;
    if (s == "path_rank" || s == "pathrank") return InsightType::PATH_RANK;
    if (s == "long_chain" || s == "long-chain" || s == "longchain") return InsightType::LONG_CHAIN;
    if (s == "meta_path" || s == "meta-path" || s == "metapath") return InsightType::META_PATH;
    if (s == "meta_path_pattern" || s == "meta-path-pattern" || s == "meta_path_patterns" || s == "meta-path-patterns") return InsightType::META_PATH_PATTERN;
    if (s == "hypothesis" || s == "hypotheses" || s == "hypotheses_1") return InsightType::HYPOTHESES_1;
    if (s == "hypotheses_2" || s == "hypothesis_2" || s == "mechanistic") return InsightType::HYPOTHESES_2;
    if (s == "hypotheses_3" || s == "hypothesis_3" || s == "counterfactual_causal" || s == "counterfactual-causal") return InsightType::HYPOTHESES_3;
    if (s == "mechanism_consolidation" || s == "mechanism-consolidation" || s == "mechanism_cluster" || s == "mechanism-cluster") return InsightType::MECHANISM_CONSOLIDATION;
    if (s == "evidence_fusion_link" || s == "evidence-fusion-link" || s == "evidence_fusion" || s == "evidence-fusion") return InsightType::EVIDENCE_FUSION_LINK;
    if (s == "meta_path_anomaly" || s == "meta-path-anomaly" || s == "metapath_anomaly" || s == "metapath-anomaly") return InsightType::META_PATH_ANOMALY;
    if (s == "intervention_bottleneck" || s == "intervention-bottleneck") return InsightType::INTERVENTION_BOTTLENECK;
    if (s == "competing_mechanism" || s == "competing-mechanism" || s == "competing_mechanisms" || s == "competing-mechanisms") return InsightType::COMPETING_MECHANISM;
    if (s == "schema_repair" || s == "schema-repair" || s == "schema_repairs" || s == "schema-repairs") return InsightType::SCHEMA_REPAIR;
    if (s == "cross_community_mechanism_bridge" || s == "cross-community-mechanism-bridge" ||
        s == "cross_community_bridge" || s == "cross-community-bridge") return InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE;
    if (s == "rule") return InsightType::RULE;
    if (s == "embedding_link" || s == "embedding" || s == "transe") return InsightType::EMBEDDING_LINK;
    if (s == "author_chain" || s == "authorchain" || s == "author-chains") return InsightType::AUTHOR_CHAIN;
    if (s == "co_authorship" || s == "co-authorship" || s == "coauthorship" || s == "collaboration") return InsightType::CO_AUTHORSHIP;
    if (s == "citation_impact" || s == "citation-impact" || s == "citations" || s == "impact") return InsightType::CITATION_IMPACT;
    if (s == "multi_resolution_community" || s == "multi-resolution-community" || s == "multiresolution" || s == "hierarchical_community") return InsightType::MULTI_RESOLUTION_COMMUNITY;
    if (s == "cross_community_bridge_map" || s == "cross-community-bridge-map" || s == "community_bridge_map" || s == "bridge_map") return InsightType::CROSS_COMMUNITY_BRIDGE_MAP;
    if (s == "meta_pattern" || s == "meta-pattern" || s == "metapattern" || s == "pattern_of_patterns") return InsightType::META_PATTERN;
    if (s == "causal_chain" || s == "causal-chain" || s == "causal_chains" || s == "causal") return InsightType::CAUSAL_CHAIN;
    if (s == "intervention_point" || s == "intervention-point" || s == "intervention_points" || s == "intervention") return InsightType::INTERVENTION_POINT;
    if (s == "feedback_loop" || s == "feedback-loop" || s == "feedback_loops" || s == "feedback") return InsightType::FEEDBACK_LOOP;
    if (s == "confounder" || s == "confounders" || s == "confounding") return InsightType::CONFOUNDER;
    if (s == "taxonomy" || s == "hierarchical" || s == "hierarchy") return InsightType::TAXONOMY;
    if (s == "domain_bridge" || s == "domain-bridge" || s == "interdisciplinary") return InsightType::DOMAIN_BRIDGE;
    if (s == "logical_entailment" || s == "logical-entailment" || s == "entailment") return InsightType::LOGICAL_ENTAILMENT;
    if (s == "compositional_reasoning" || s == "compositional-reasoning" || s == "composition") return InsightType::COMPOSITIONAL_REASONING;
    if (s == "explanatory_chain" || s == "explanatory-chain" || s == "explanation" || s == "explanatory_chains") return InsightType::EXPLANATORY_CHAIN;
    if (s == "schema_violation" || s == "schema-violation" || s == "schema_violations" || s == "schema") return InsightType::SCHEMA_VIOLATION;
    if (s == "transitive_closure" || s == "transitive-closure" || s == "transitive" || s == "closure") return InsightType::TRANSITIVE_CLOSURE;
    if (s == "evidence_debt" || s == "evidence-debt") return InsightType::EVIDENCE_DEBT;
    if (s == "consensus_frontier" || s == "consensus-frontier") return InsightType::CONSENSUS_FRONTIER;
    if (s == "boundary_condition_map" || s == "boundary-condition-map" || s == "boundary_condition") return InsightType::BOUNDARY_CONDITION_MAP;
    if (s == "failure_mode_topology" || s == "failure-mode-topology" || s == "failure_mode") return InsightType::FAILURE_MODE_TOPOLOGY;
    if (s == "benchmark_dependence" || s == "benchmark-dependence") return InsightType::BENCHMARK_DEPENDENCE;
    if (s == "concept_drift" || s == "concept-drift") return InsightType::CONCEPT_DRIFT;
    if (s == "premise_bottleneck" || s == "premise-bottleneck") return InsightType::PREMISE_BOTTLENECK;
    if (s == "translation_gap" || s == "translation-gap") return InsightType::TRANSLATION_GAP;
    return InsightType::BRIDGE; // default
}

// Map each insight type to its knowledge discovery category
inline InsightCategory get_insight_category(InsightType type) {
    switch (type) {
        // COMBINATORIAL: Pattern detection, structural combinations, motifs
        case InsightType::MOTIF:
        case InsightType::K_CORE:
        case InsightType::K_TRUSS:
        case InsightType::COMMUNITY_DETECTION:
        case InsightType::CORE_PERIPHERY:
        case InsightType::META_PATH_PATTERN:
        case InsightType::RULE:
        case InsightType::TAXONOMY:
        case InsightType::COMPOSITIONAL_REASONING:
        case InsightType::TRANSITIVE_CLOSURE:
        case InsightType::MECHANISM_CONSOLIDATION:
        case InsightType::META_PATH:
        case InsightType::CENTRALITY:
        case InsightType::CO_AUTHORSHIP:
        case InsightType::CITATION_IMPACT:
        case InsightType::MULTI_RESOLUTION_COMMUNITY:
        case InsightType::CROSS_COMMUNITY_BRIDGE_MAP:
        case InsightType::META_PATTERN:
        case InsightType::BIAS_AUDIT:
            return InsightCategory::COMBINATORIAL;

        // EXPLORATORY: Path finding, bridge discovery, link prediction
        case InsightType::BRIDGE:
        case InsightType::COMPLETION:
        case InsightType::LONG_CHAIN:
        case InsightType::PATH_RANK:
        case InsightType::COMMUNITY_LINK:
        case InsightType::EMBEDDING_LINK:
        case InsightType::HYPEREDGE_PREDICTION:
        case InsightType::DOMAIN_BRIDGE:
        case InsightType::EXPLANATORY_CHAIN:
        case InsightType::AUTHOR_CHAIN:
        case InsightType::CAUSAL_CHAIN:
        case InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE:
        case InsightType::DIFFUSION:
        case InsightType::RELATION_INDUCTION:
        case InsightType::EVIDENCE_FUSION_LINK:
        case InsightType::COMMUNITY_RECOMMENDATION:
            return InsightCategory::EXPLORATORY;

        // TRANSFORMATIONAL: Reframing, hypothesis generation, causal reasoning
        case InsightType::SUBSTITUTION:
        case InsightType::CONTRADICTION:
        case InsightType::ENTITY_RESOLUTION:
        case InsightType::ANALOGICAL_TRANSFER:
        case InsightType::COUNTERFACTUAL:
        case InsightType::SURPRISE:
        case InsightType::HYPOTHESES_1:
        case InsightType::HYPOTHESES_2:
        case InsightType::HYPOTHESES_3:
        case InsightType::META_PATH_ANOMALY:
        case InsightType::INTERVENTION_BOTTLENECK:
        case InsightType::COMPETING_MECHANISM:
        case InsightType::SCHEMA_REPAIR:
        case InsightType::INTERVENTION_POINT:
        case InsightType::FEEDBACK_LOOP:
        case InsightType::CONFOUNDER:
        case InsightType::LOGICAL_ENTAILMENT:
        case InsightType::SCHEMA_VIOLATION:
        case InsightType::CLAIM_STANCE:
        case InsightType::UNCERTAINTY_SAMPLING:
        case InsightType::ACTIVE_LEARNING:
            return InsightCategory::TRANSFORMATIONAL;

        // Domain-specific (can be categorized based on use)
        case InsightType::TEXT_SIMILARITY:
        case InsightType::ARGUMENT_SUPPORT:
        case InsightType::METHOD_OUTCOME:
            return InsightCategory::EXPLORATORY;

        // Epistemic Discovery
        case InsightType::EVIDENCE_DEBT:
        case InsightType::CONSENSUS_FRONTIER:
        case InsightType::BOUNDARY_CONDITION_MAP:
        case InsightType::FAILURE_MODE_TOPOLOGY:
        case InsightType::BENCHMARK_DEPENDENCE:
        case InsightType::CONCEPT_DRIFT:
        case InsightType::PREMISE_BOTTLENECK:
            return InsightCategory::TRANSFORMATIONAL;

        case InsightType::TRANSLATION_GAP:
            return InsightCategory::EXPLORATORY;

        default:
            return InsightCategory::EXPLORATORY;
    }
}

struct Insight {
    std::string insight_id;             // "run_001:bridge:000187"
    InsightType type;
    InsightCategory category;           // High-level knowledge discovery category
    std::vector<std::string> seed_nodes;   // Primary node IDs involved
    std::vector<std::string> witness_edges; // Edge IDs that support this insight
    std::vector<std::string> witness_nodes; // All node IDs in the witness subgraph
    std::vector<std::string> evidence_chunk_ids; // Source chunk IDs for provenance
    std::vector<std::string> source_documents;   // Source document paths for one-click verification
    double score = 0.0;
    std::map<std::string, double> score_breakdown; // "support", "novelty", "specificity"
    std::vector<std::string> novelty_tags;
    nlohmann::json llm = nullptr;       // LLM synthesis output (null until synthesized)

    // Additional fields for specific insight types
    std::string description;            // Human-readable description
    std::vector<std::string> seed_labels; // Labels for seed nodes (for display)

    // Constructor to automatically set category from type
    Insight() : type(InsightType::BRIDGE), category(InsightCategory::EXPLORATORY) {}

    void set_type(InsightType t) {
        type = t;
        category = get_insight_category(t);
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["insight_id"] = insight_id;
        j["type"] = insight_type_to_string(type);
        j["category"] = category_to_string(category);
        j["seed_nodes"] = seed_nodes;
        j["witness_edges"] = witness_edges;
        j["witness_nodes"] = witness_nodes;
        j["evidence_chunk_ids"] = evidence_chunk_ids;
        j["source_documents"] = source_documents;
        j["score"] = score;
        j["score_breakdown"] = score_breakdown;
        j["novelty_tags"] = novelty_tags;
        j["llm"] = llm;
        j["description"] = description;
        j["seed_labels"] = seed_labels;
        return j;
    }

    static Insight from_json(const nlohmann::json& j) {
        Insight ins;
        ins.insight_id = j.value("insight_id", "");
        ins.type = string_to_insight_type(j.value("type", "bridge"));

        // Set category - either from JSON or auto-derive from type
        if (j.contains("category")) {
            ins.category = string_to_category(j["category"].get<std::string>());
        } else {
            ins.category = get_insight_category(ins.type);
        }

        ins.seed_nodes = j.value("seed_nodes", std::vector<std::string>{});
        ins.witness_edges = j.value("witness_edges", std::vector<std::string>{});
        ins.witness_nodes = j.value("witness_nodes", std::vector<std::string>{});
        ins.evidence_chunk_ids = j.value("evidence_chunk_ids", std::vector<std::string>{});
        ins.source_documents = j.value("source_documents", std::vector<std::string>{});
        ins.score = j.value("score", 0.0);
        if (j.contains("score_breakdown")) {
            ins.score_breakdown = j["score_breakdown"].get<std::map<std::string, double>>();
        }
        ins.novelty_tags = j.value("novelty_tags", std::vector<std::string>{});
        ins.llm = j.value("llm", nlohmann::json(nullptr));
        ins.description = j.value("description", "");
        ins.seed_labels = j.value("seed_labels", std::vector<std::string>{});
        return ins;
    }
};

// Collection of insights with metadata
struct InsightCollection {
    std::string run_id;
    std::string created_utc;
    std::string source_graph;
    std::vector<Insight> insights;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["meta"] = {
            {"run_id", run_id},
            {"created_utc", created_utc},
            {"source_graph", source_graph},
            {"total_insights", insights.size()}
        };

        nlohmann::json insights_arr = nlohmann::json::array();
        for (const auto& ins : insights) {
            insights_arr.push_back(ins.to_json());
        }
        j["insights"] = insights_arr;

        // Summary by type
        std::map<std::string, int> by_type;
        for (const auto& ins : insights) {
            by_type[insight_type_to_string(ins.type)]++;
        }
        j["summary_by_type"] = by_type;

        // Summary by category (NEW)
        std::map<std::string, int> by_category;
        for (const auto& ins : insights) {
            by_category[category_to_string(ins.category)]++;
        }
        j["summary_by_category"] = by_category;

        return j;
    }

    static InsightCollection from_json(const nlohmann::json& j) {
        InsightCollection col;

        if (j.contains("meta")) {
            col.run_id = j["meta"].value("run_id", "");
            col.created_utc = j["meta"].value("created_utc", "");
            col.source_graph = j["meta"].value("source_graph", "");
        }

        if (j.contains("insights")) {
            for (const auto& ins_json : j["insights"]) {
                col.insights.push_back(Insight::from_json(ins_json));
            }
        }

        return col;
    }

    void save_to_json(const std::string& path) const {
        std::ofstream file(path);
        file << to_json().dump(2);
    }

    static InsightCollection load_from_json(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open insights file: " + path);
        }
        nlohmann::json j;
        file >> j;
        return from_json(j);
    }
};

} // namespace kg

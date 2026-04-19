#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kg {

// Single source of truth for discovery operator ordering and inclusion.
//
// OPERATOR DEPENDENCIES:
// ----------------------
// Some operators depend on insights from other operators. The ordering below
// ensures dependencies run before dependent operators:
//
//   hypotheses_1: Requires insights with 2+ seed_nodes from ANY other operators
//   hypotheses_2: Requires graph edges (always available)
//   hypotheses_3: Requires causal_chains AND intervention_points insights
//   mechanism_consolidation: Benefits from hypotheses_2, long_chain, explanatory_chain
//   competing_mechanisms: Benefits from hypotheses_2/3, mechanism_consolidation, contradictions
//   schema_repair: Benefits from completions, schema_violation, transitive_closure
//   cross_community_mechanism_bridge: Benefits from community/domain/long-chain operators
//   meta_path_anomaly: Benefits from meta_path_pattern/long_chain
//   evidence_fusion: Aggregates link evidence from multiple operators and should run last
//
// When using -p "all", operators execute in this order to satisfy dependencies.
//
inline const std::vector<std::string>& all_discovery_operators() {
    static const std::vector<std::string> ops = {
        // Structural (11)
        "bridges", "completions", "motifs", "substitutions", "surprise",
        "core_periphery", "centrality", "community_detection", "k_core", "k_truss", "diffusion",
        // Advanced Reasoning & Structural Discovery (2) - Early
        "taxonomy", "domain_bridge",
        // Causal & Mechanistic Discovery (4) - MUST run before hypotheses_3
        "causal_chains", "intervention_points", "feedback_loops", "confounders",
        // Semantic/LLM (10) - removed analogical_transfer (deprecated 2026-03-29)
        "contradictions", "entity_resolution", "text_similarity", "argument_support",
        "method_outcome", "claim_stance", "relation_induction",
        "hypotheses_1", "hypotheses_2", "hypotheses_3",
        // Advanced Reasoning (continued, 5)
        "logical_entailment", "compositional_reasoning", "explanatory_chain",
        "schema_violation", "transitive_closure",
        // Active Learning (4)
        "active_learning", "uncertainty_sampling", "counterfactual", "hyperedge_prediction",
        // Link Prediction & Path Reasoning (13) - removed co_authorship (deprecated 2026-03-29)
        "rules", "community", "pathrank", "intersection_bridge",
        "long_chain", "meta_path", "meta_path_pattern", "embedding",
        "author_chain", "citation_impact",
        "multi_resolution_community", "cross_community_bridge_map", "meta_pattern",
        // Synthesis & Consolidation (0) - removed mechanism_consolidation (deprecated 2026-03-29)
        // Advanced cross-operator synthesis (5) - removed intervention_bottleneck (deprecated 2026-03-29)
        "competing_mechanisms", "schema_repair",
        "cross_community_mechanism_bridge", "meta_path_anomaly", "evidence_fusion",
        // Epistemic Discovery (8)
        "evidence_debt", "consensus_frontier", "boundary_condition_map",
        "failure_mode_topology", "benchmark_dependence", "concept_drift",
        "premise_bottleneck", "translation_gap"
    };
    return ops;
}

inline std::string normalize_operator_name(const std::string& name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            normalized.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '-' || c == ' ' || c == '/') {
            normalized.push_back('_');
        } else if (c == '_') {
            normalized.push_back('_');
        }
    }

    normalized.erase(
        std::unique(normalized.begin(), normalized.end(),
                    [](char a, char b) { return a == '_' && b == '_'; }),
        normalized.end()
    );

    if (!normalized.empty() && normalized.front() == '_') {
        normalized.erase(normalized.begin());
    }
    if (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }

    return normalized;
}

inline const std::unordered_map<std::string, std::string>& discovery_operator_aliases() {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"bridge", "bridges"},
        {"bridges", "bridges"},
        {"completion", "completions"},
        {"completions", "completions"},
        {"motif", "motifs"},
        {"motifs", "motifs"},
        {"substitution", "substitutions"},
        {"substitutions", "substitutions"},
        {"contradiction", "contradictions"},
        {"contradictions", "contradictions"},
        {"entity_resolution", "entity_resolution"},
        {"entityresolution", "entity_resolution"},
        {"dedup", "entity_resolution"},
        {"core_periphery", "core_periphery"},
        {"coreperiphery", "core_periphery"},
        {"hub_authority", "core_periphery"},
        {"text_similarity", "text_similarity"},
        {"textsimilarity", "text_similarity"},
        {"semantic", "text_similarity"},
        {"semantic_similarity", "text_similarity"},
        {"argument_support", "argument_support"},
        {"argument", "argument_support"},
        {"active_learning", "active_learning"},
        {"active", "active_learning"},
        {"method_outcome", "method_outcome"},
        {"method", "method_outcome"},
        {"outcome", "method_outcome"},
        {"centrality", "centrality"},
        {"centrality_rank", "centrality"},
        {"centrality_rankings", "centrality"},
        {"community_detection", "community_detection"},
        {"communities", "community_detection"},
        {"k_core", "k_core"},
        {"core", "k_core"},
        {"k_truss", "k_truss"},
        {"truss", "k_truss"},
        {"claim_stance", "claim_stance"},
        {"stance", "claim_stance"},
        {"relation_induction", "relation_induction"},
        {"relation_type", "relation_induction"},
        {"analogical_transfer", "analogical_transfer"},
        {"analogy", "analogical_transfer"},
        {"uncertainty_sampling", "uncertainty_sampling"},
        {"uncertainty", "uncertainty_sampling"},
        {"counterfactual", "counterfactual"},
        {"counterfactual_probing", "counterfactual"},
        {"hyperedge_prediction", "hyperedge_prediction"},
        {"hyperedge", "hyperedge_prediction"},
        {"diffusion", "diffusion"},
        {"diffusions", "diffusion"},
        {"surprise", "surprise"},
        {"surprises", "surprise"},
        {"rule", "rules"},
        {"rules", "rules"},
        {"community", "community"},
        {"community_link", "community"},
        {"community_links", "community"},
        {"hypotheses_1", "hypotheses_1"},
        {"bayesian_hypotheses", "hypotheses_1"},
        {"hypotheses_2", "hypotheses_2"},
        {"mechanistic_hypotheses", "hypotheses_2"},
        {"hypotheses_3", "hypotheses_3"},
        {"counterfactual_hypotheses", "hypotheses_3"},
        {"causal_chain", "causal_chains"},
        {"causal_chains", "causal_chains"},
        {"intervention_point", "intervention_points"},
        {"intervention_points", "intervention_points"},
        {"feedback_loop", "feedback_loops"},
        {"feedback_loops", "feedback_loops"},
        {"confounder", "confounders"},
        {"confounders", "confounders"},
        {"taxonomy", "taxonomy"},
        {"taxonomy_induction", "taxonomy"},
        {"domain_bridge", "domain_bridge"},
        {"domain_bridges", "domain_bridge"},
        {"logical_entailment", "logical_entailment"},
        {"logical_entailments", "logical_entailment"},
        {"compositional_reasoning", "compositional_reasoning"},
        {"composition", "compositional_reasoning"},
        {"explanatory_chain", "explanatory_chain"},
        {"explanatory_chains", "explanatory_chain"},
        {"explanation", "explanatory_chain"},
        {"schema_violation", "schema_violation"},
        {"schema_violations", "schema_violation"},
        {"schema", "schema_violation"},
        {"transitive_closure", "transitive_closure"},
        {"transitive", "transitive_closure"},
        {"closure", "transitive_closure"},
        {"meta_path", "meta_path"},
        {"meta_paths", "meta_path"},
        {"metapath", "meta_path"},
        {"meta_path_pattern", "meta_path_pattern"},
        {"meta_path_patterns", "meta_path_pattern"},
        {"hypothesis", "hypotheses"},
        {"hypotheses", "hypotheses"},
        {"pathrank", "pathrank"},
        {"path_rank", "pathrank"},
        {"path_ranking", "pathrank"},
        {"intersection_bridge", "intersection_bridge"},
        {"intersection_hypothesis_bridge", "intersection_bridge"},
        {"higher_order_bridge", "intersection_bridge"},
        {"hypergraph_traversal", "intersection_bridge"},
        {"long_chain", "long_chain"},
        {"longchain", "long_chain"},
        {"embedding", "embedding"},
        {"embedding_link", "embedding"},
        {"embeddings", "embedding"},
        {"transe", "embedding"},
        {"author_chain", "author_chain"},
        {"authorchain", "author_chain"},
        {"author_chains", "author_chain"},
        {"citation_impact", "citation_impact"},
        {"citations", "citation_impact"},
        {"impact", "citation_impact"},
        {"multi_resolution_community", "multi_resolution_community"},
        {"multiresolution", "multi_resolution_community"},
        {"hierarchical_community", "multi_resolution_community"},
        {"cross_community_bridge_map", "cross_community_bridge_map"},
        {"community_bridge_map", "cross_community_bridge_map"},
        {"bridge_map", "cross_community_bridge_map"},
        {"meta_pattern", "meta_pattern"},
        {"metapattern", "meta_pattern"},
        {"pattern_of_patterns", "meta_pattern"},
        {"bridge_analogies", "bridge_analogies"},
        {"bridge_analogy", "bridge_analogies"},
        {"analogies", "bridge_analogies"},
        {"bias_audit", "bias_audit"},
        {"bias", "bias_audit"},
        {"fairness", "bias_audit"},
        {"representation", "bias_audit"},
        {"community_recommendation", "community_recommendation"},
        {"recommend", "community_recommendation"},
        {"recommendations", "community_recommendation"},
        {"mechanism_consolidation", "mechanism_consolidation"},
        {"mechanism_cluster", "mechanism_consolidation"},
        {"evidence_fusion", "evidence_fusion"},
        {"evidence_fusion_link", "evidence_fusion"},
        {"meta_path_anomaly", "meta_path_anomaly"},
        {"metapath_anomaly", "meta_path_anomaly"},
        {"intervention_bottleneck", "intervention_bottleneck"},
        {"competing_mechanism", "competing_mechanisms"},
        {"competing_mechanisms", "competing_mechanisms"},
        {"schema_repair", "schema_repair"},
        {"schema_repairs", "schema_repair"},
        {"cross_community_mechanism_bridge", "cross_community_mechanism_bridge"},
        {"cross_community_bridge", "cross_community_mechanism_bridge"},
        {"evidence_debt", "evidence_debt"},
        {"consensus_frontier", "consensus_frontier"},
        {"boundary_condition_map", "boundary_condition_map"},
        {"boundary_condition", "boundary_condition_map"},
        {"failure_mode_topology", "failure_mode_topology"},
        {"failure_mode", "failure_mode_topology"},
        {"benchmark_dependence", "benchmark_dependence"},
        {"concept_drift", "concept_drift"},
        {"premise_bottleneck", "premise_bottleneck"},
        {"translation_gap", "translation_gap"}
    };
    return aliases;
}

inline std::optional<std::string> resolve_discovery_operator(const std::string& name) {
    const std::string normalized = normalize_operator_name(name);
    const auto& aliases = discovery_operator_aliases();
    const auto alias_it = aliases.find(normalized);
    if (alias_it != aliases.end()) {
        return alias_it->second;
    }
    return std::nullopt;
}

}  // namespace kg

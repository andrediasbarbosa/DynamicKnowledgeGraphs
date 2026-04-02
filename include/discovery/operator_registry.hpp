#pragma once

#include <string>
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

}  // namespace kg

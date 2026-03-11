#pragma once

#include "discovery/insight.hpp"
#include "index/hypergraph_index.hpp"
#include "graph/hypergraph.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <random>
#include <cmath>
#include <memory>

namespace kg {
class LLMProvider;

// Discovery configuration
struct DiscoveryConfig {
    // Bridge detection
    int bridge_s_threshold = 2;          // Minimum s-value for component separation
    int bridge_max_path_length = 4;      // Maximum path length for bridges
    size_t bridge_max_candidates = 200;  // Max bridge candidates to evaluate

    // Completion detection
    double completion_min_confidence = 0.3;
    size_t completion_max_candidates = 200;
    int completion_min_shared_edges = 1; // Min shared hyperedges for completion pattern

    // Motif mining
    int motif_min_support = 2;           // Minimum co-occurrence count
    double motif_min_lift = 1.5;         // Minimum lift (observed / expected)
    size_t motif_max_size = 5;           // Maximum motif size
    size_t motif_max_candidates = 200;

    // Substitution detection
    double substitution_similarity_threshold = 0.6;
    size_t substitution_max_candidates = 200;

    // Contradiction detection
    size_t contradiction_max_candidates = 200;
    size_t contradiction_max_evidence_edges = 8;

    // Entity resolution (duplicate/alias detection)
    double entity_resolution_min_label_similarity = 0.82;
    double entity_resolution_min_neighbor_overlap = 0.1;
    size_t entity_resolution_max_candidates = 200;
    size_t entity_resolution_neighbor_limit = 60;

    // Core-periphery / hub-authority analysis
    size_t core_periphery_top_k = 20;
    size_t core_periphery_bottom_k = 20;
    size_t core_periphery_max_evidence_edges = 6;
    int core_periphery_hub_set_size = 25;

    // Text semantic similarity links
    double text_similarity_min_score = 0.6;
    size_t text_similarity_top_k_per_node = 10;
    size_t text_similarity_max_candidates = 200;

    // Argument-supported relations
    int argument_support_max_path_length = 3;
    size_t argument_support_max_candidates = 100;
    size_t argument_support_max_evidence_edges = 6;

    // Active learning for graphs
    double active_learning_confidence_threshold = 0.6;
    size_t active_learning_max_queries = 40;

    // Method/outcome node discovery
    size_t method_outcome_max_candidates = 50;

    // Centrality analysis (bipartite PageRank)
    int centrality_iterations = 30;
    double centrality_damping = 0.85;
    size_t centrality_top_k = 30;
    size_t centrality_max_evidence_edges = 6;

    // Community detection (Louvain on projected graph)
    double community_detection_resolution = 1.0;
    size_t community_detection_min_size = 4;
    size_t community_detection_max_communities = 30;
    size_t community_detection_max_evidence_edges = 8;

    // Multi-resolution community detection
    std::vector<double> multi_resolution_scales = {0.5, 1.0, 2.0, 4.0}; // Resolution parameters
    size_t multi_resolution_max_levels = 10; // Max hierarchy levels to report
    size_t multi_resolution_min_community_size = 3; // Min nodes per community

    // Cross-community bridge mapping
    size_t cross_community_bridge_map_min_communities = 2; // Min communities a bridge must connect
    size_t cross_community_bridge_map_max_candidates = 100; // Max bridge mappings to report

    // Meta-pattern discovery
    size_t meta_pattern_min_occurrences = 3; // Min times a pattern must appear
    size_t meta_pattern_max_patterns = 50; // Max meta-patterns to report
    size_t meta_pattern_max_size = 8; // Max nodes in a meta-pattern

    // Bridge analogy generation
    size_t bridge_analogy_min_shared_patterns = 2; // Min shared relation patterns
    size_t bridge_analogy_max_candidates = 100; // Max analogies to report
    double bridge_analogy_min_confidence = 0.5; // Min confidence score

    // Bias audit
    double bias_audit_overrepresentation_threshold = 2.0; // Factor above mean for flagging
    size_t bias_audit_min_insights = 10; // Min insights needed for meaningful audit

    // Community recommendations
    size_t community_recommendation_max_per_entity = 10; // Max recommendations per entity
    double community_recommendation_min_similarity = 0.3; // Min similarity threshold
    double community_recommendation_novelty_weight = 0.3; // Weight for cross-community novelty

    // k-core / k-truss on projected graph
    int k_core_min_k = 3;
    size_t k_core_max_candidates = 50;
    int k_truss_min_k = 3;
    size_t k_truss_max_candidates = 80;

    // Claim decomposition + stance
    size_t claim_stance_max_candidates = 80;
    double claim_stance_min_confidence = 0.5;

    // Relation type induction
    size_t relation_induction_max_relations = 25;
    size_t relation_induction_examples_per_relation = 5;
    double relation_induction_min_confidence = 0.6;

    // Analogical transfer
    double analogical_transfer_min_score = 0.6;
    size_t analogical_transfer_max_candidates = 120;
    size_t analogical_transfer_pairs_per_relation = 60;

    // Uncertainty sampling
    size_t uncertainty_sampling_max_candidates = 80;
    double uncertainty_sampling_min_uncertainty = 0.3;

    // Counterfactual probing
    size_t counterfactual_max_candidates = 60;

    // Hyperedge prediction
    size_t hyperedge_prediction_max_candidates = 120;
    double hyperedge_prediction_min_score = 0.4;

    // Constrained rule mining
    int constrained_rule_min_support = 2;
    double constrained_rule_min_confidence = 0.5;
    double constrained_rule_min_lift = 1.1;
    size_t constrained_rule_max_candidates = 120;

    // Diffusion (PageRank-style)
    int diffusion_iterations = 20;
    double diffusion_damping = 0.85;
    size_t diffusion_top_k = 50;

    // Surprise detection
    double surprise_max_expected_cooccurrence = 0.2;
    size_t surprise_max_candidates = 200;

    // Rule mining (association rules)
    int rule_min_support = 2;            // Minimum occurrences of rule body
    double rule_min_confidence = 0.4;    // Minimum P(head|body)
    double rule_min_lift = 1.1;          // Minimum lift (confidence / P(head))
    size_t rule_max_candidates = 200;    // Max rules to evaluate

    // Path ranking (path-based link prediction)
    int path_rank_k = 5;                 // K shortest paths to consider
    int path_rank_max_hops = 3;          // Maximum path length (edges)
    int path_rank_min_intersection = 1;  // Minimum s-connectivity for path search
    size_t path_rank_max_seed_nodes = 200; // Top-degree nodes to evaluate
    size_t path_rank_max_pairs = 2000;   // Max candidate pairs to test
    size_t path_rank_max_candidates = 200; // Max insights to evaluate
    size_t path_rank_max_witness_edges = 50; // Cap evidence edges stored
    double path_rank_min_score = 0.6;    // Minimum path score to keep
    size_t path_rank_min_evidence_edges = 2; // Minimum evidence edges to keep

    // Intersection-constrained higher-order traversal (semantic bridge hypotheses)
    size_t intersection_bridge_count = 15;         // Number of bridge hypotheses to keep
    size_t intersection_bridge_max_seed_nodes = 180; // Top-degree nodes considered
    size_t intersection_bridge_max_pairs = 1600;   // Max candidate pairs to evaluate
    int intersection_bridge_k_paths = 5;           // K shortest paths per pair
    int intersection_bridge_max_hops = 5;          // Max hyperedges per path
    int intersection_bridge_min_intersection = 2;  // IS: minimum shared nodes between adjacent hyperedges
    double intersection_bridge_min_semantic_distance = 0.65; // Require semantically distant endpoint pairs
    size_t intersection_bridge_max_witness_edges = 40; // Cap evidence edges per hypothesis
    size_t intersection_bridge_max_witness_nodes = 60; // Cap evidence nodes per hypothesis
    double intersection_bridge_min_score = 0.45;   // Minimum final score to keep

    // Long-chain reasoning (multi-hop)
    int long_chain_min_hops = 4;          // Minimum edges in chain
    int long_chain_max_hops = 7;          // Maximum edges in chain
    size_t long_chain_max_candidates = 200; // Max chain candidates to evaluate
    size_t long_chain_max_seed_nodes = 150; // Seed nodes to explore (top-degree)
    size_t long_chain_degree_cap = 30;     // Max outgoing edges per expansion
    double long_chain_min_avg_confidence = 0.2; // Minimum avg confidence per chain
    double long_chain_weight_confidence = 0.55;
    double long_chain_weight_length = 0.25;
    double long_chain_weight_novelty = 0.20;

    // Meta-path reasoning (typed path patterns)
    int meta_path_min_len = 2;           // Minimum path length (edges)
    int meta_path_max_len = 3;           // Maximum path length (edges)
    int meta_path_min_support = 5;       // Minimum distinct pairs per pattern
    size_t meta_path_top_k_patterns = 50; // Keep top patterns by support
    size_t meta_path_top_k_paths = 3;    // Max path instances stored per candidate
    size_t meta_path_max_candidates = 200; // Max insights to evaluate
    size_t meta_path_max_seed_nodes = 200; // Seed nodes to explore
    size_t meta_path_degree_cap = 200;   // Cap outgoing edges per expansion
    double meta_path_min_path_confidence = 0.0; // Minimum avg confidence per path
    size_t meta_path_max_witness_edges = 60; // Cap evidence edges stored

    // Meta-path pattern mining (global templates)
    int meta_path_pattern_min_len = 2;           // Minimum relations in pattern
    int meta_path_pattern_max_len = 4;           // Maximum relations in pattern
    int meta_path_pattern_min_support = 5;       // Minimum distinct (start,end) pairs
    size_t meta_path_pattern_max_patterns = 30;  // Maximum patterns to keep
    size_t meta_path_pattern_max_seed_nodes = 200; // Seed nodes to explore
    size_t meta_path_pattern_degree_cap = 50;    // Max outgoing edges per expansion
    size_t meta_path_pattern_max_instances = 6;  // Example instances per pattern
    double meta_path_pattern_min_avg_confidence = 0.2; // Minimum avg confidence
    double meta_path_pattern_weight_support = 0.50;
    double meta_path_pattern_weight_confidence = 0.30;
    double meta_path_pattern_weight_length = 0.20;

    // Community-aware cross-cluster links
    int community_s_threshold = 2;       // s-value for component separation
    size_t community_top_nodes_per_component = 15; // Candidate nodes per component
    double community_min_relation_overlap = 0.1; // Min Jaccard overlap of relation signatures
    size_t community_max_candidates = 200;  // Max insights to evaluate

    // Hypotheses_1: Bayesian hypothesis network (probabilistic belief propagation)
    size_t hypothesis_count = 15;                    // Number of hypotheses to generate (increased from 5)
    size_t bayesian_max_iterations = 20;             // Belief propagation iterations
    double bayesian_min_posterior = 0.4;             // Minimum posterior probability (lowered from 0.6)
    double bayesian_convergence_threshold = 0.01;    // Convergence threshold for BP
    size_t bayesian_top_evidence_per_hypothesis = 10; // Max evidence items per hypothesis
    size_t bayesian_max_candidates = 300;            // Max candidate pairs to evaluate (increased from 200)
    // Weights for Bayesian scoring (sum to 1.0)
    double bayesian_weight_posterior = 0.35;         // Posterior probability weight
    double bayesian_weight_evidence_diversity = 0.25; // Evidence diversity weight
    double bayesian_weight_prior_surprise = 0.20;    // Prior surprise weight (low prior + high posterior)
    double bayesian_weight_uncertainty_reduction = 0.20; // Uncertainty reduction weight

    // Hypotheses_2: Typed mechanistic chains + explanation subgraphs
    size_t hypothesis_2_count = 15;                   // Number of hypotheses to generate
    size_t hypothesis_2_max_candidates = 300;         // Max chain candidates to evaluate
    size_t hypothesis_2_max_seed_nodes = 200;         // Seed nodes to explore (top-degree)
    int hypothesis_2_chain_min_length = 3;            // Minimum nodes in chain
    int hypothesis_2_chain_max_length = 5;            // Maximum nodes in chain
    double hypothesis_2_min_chain_score = 0.25;       // Minimum chain score to keep
    double hypothesis_2_min_explanation_score = 0.15; // Minimum explanation score to keep
    double hypothesis_2_min_final_score = 0.3;        // Minimum final score to keep
    bool hypothesis_2_allow_unknown_types = true;     // Allow nodes without explicit types
    bool hypothesis_2_enable_untyped_fallback = true; // Add generic template if no typed matches
    size_t hypothesis_2_explanation_max_nodes = 12;   // Cap explanation subgraph size (nodes)
    size_t hypothesis_2_explanation_max_edges = 20;   // Cap explanation subgraph size (edges)
    size_t hypothesis_2_explanation_degree_cap = 30;  // Max edges per chain node to consider
    double hypothesis_2_explanation_min_confidence = 0.2; // Min confidence for evidence edges
    // Weights for chain scoring (sum to 1.0)
    double hypothesis_2_weight_support = 0.30;
    double hypothesis_2_weight_relation_strength = 0.25;
    double hypothesis_2_weight_type_fidelity = 0.20;
    double hypothesis_2_weight_novelty = 0.15;
    double hypothesis_2_weight_coherence = 0.10;
    // Combine chain and explanation scores
    double hypothesis_2_weight_chain = 0.6;
    double hypothesis_2_weight_explanation = 0.4;

    // Hypotheses_3: Counterfactual causal hypothesis generation
    size_t hypothesis_3_count = 15;                   // Number of hypotheses to generate (increased from 5)
    size_t hypothesis_3_intervention_candidates = 250; // Max intervention points to consider (increased from 150)
    int hypothesis_3_max_chain_length = 6;            // Max causal chain length to trace (increased from 5)
    double hypothesis_3_min_effect_size = 0.05;       // Minimum predicted effect size (lowered from 0.1)
    bool hypothesis_3_require_mechanism = true;       // Require LLM mechanistic explanation
    size_t hypothesis_3_max_evidence_edges = 10;      // Max evidence edges per hypothesis

    // Mechanism consolidation (cluster related hypotheses/paths)
    size_t mechanism_consolidation_max_sources = 200; // Max candidate insights to cluster
    size_t mechanism_consolidation_max_clusters = 25; // Max consolidated mechanisms
    size_t mechanism_consolidation_min_cluster_size = 2; // Minimum members per cluster
    double mechanism_consolidation_min_similarity = 0.3; // Jaccard overlap threshold
    size_t mechanism_consolidation_max_nodes = 20;    // Max nodes in consolidated witness set
    size_t mechanism_consolidation_max_edges = 40;    // Max edges in consolidated witness set
    double mechanism_consolidation_weight_size = 0.35;
    double mechanism_consolidation_weight_cohesion = 0.35;
    double mechanism_consolidation_weight_score = 0.30;

    // Evidence fusion links (cross-operator consensus)
    size_t evidence_fusion_max_candidates = 120;
    size_t evidence_fusion_min_sources = 2;
    size_t evidence_fusion_max_witness_edges = 30;
    double evidence_fusion_min_score = 0.45;
    double evidence_fusion_weight_consensus = 0.45;
    double evidence_fusion_weight_strength = 0.35;
    double evidence_fusion_weight_diversity = 0.20;

    // Meta-path anomaly discovery (rare-but-plausible path templates)
    size_t meta_path_anomaly_max_candidates = 120;
    size_t meta_path_anomaly_max_seed_nodes = 180;
    int meta_path_anomaly_min_len = 2;
    int meta_path_anomaly_max_len = 4;
    size_t meta_path_anomaly_degree_cap = 40;
    int meta_path_anomaly_min_support = 2;
    double meta_path_anomaly_max_support_ratio = 0.20;
    double meta_path_anomaly_min_plausibility = 0.20;
    double meta_path_anomaly_min_anomaly = 0.35;
    double meta_path_anomaly_weight_anomaly = 0.50;
    double meta_path_anomaly_weight_plausibility = 0.35;
    double meta_path_anomaly_weight_length = 0.15;

    // Intervention bottlenecks (critical nodes across causal chains)
    size_t intervention_bottleneck_max_candidates = 80;
    double intervention_bottleneck_min_coverage = 0.08;
    size_t intervention_bottleneck_max_evidence_edges = 20;
    double intervention_bottleneck_weight_coverage = 0.45;
    double intervention_bottleneck_weight_chain_strength = 0.35;
    double intervention_bottleneck_weight_span = 0.20;

    // Competing mechanisms (alternative explanations for same outcome)
    size_t competing_mechanism_max_candidates = 80;
    size_t competing_mechanism_min_alternatives = 2;
    double competing_mechanism_max_overlap = 0.55;
    double competing_mechanism_min_divergence = 0.35;
    double competing_mechanism_weight_divergence = 0.45;
    double competing_mechanism_weight_support = 0.40;
    double competing_mechanism_weight_contradiction_penalty = 0.15;

    // Schema repair suggester (actionable graph repair proposals)
    size_t schema_repair_max_candidates = 120;
    double schema_repair_min_gain = 0.35;
    size_t schema_repair_max_witness_edges = 20;
    double schema_repair_weight_gain = 0.45;
    double schema_repair_weight_support = 0.35;
    double schema_repair_weight_consistency = 0.20;

    // Cross-community mechanism bridges
    size_t cross_community_bridge_max_candidates = 80;
    double cross_community_bridge_min_novelty = 0.30;
    double cross_community_bridge_min_support = 0.20;
    size_t cross_community_bridge_max_witness_edges = 30;
    double cross_community_bridge_weight_novelty = 0.40;
    double cross_community_bridge_weight_support = 0.35;
    double cross_community_bridge_weight_coherence = 0.25;
    double hypothesis_3_min_causal_confidence = 0.1;  // Min confidence for causal chains (lowered from 0.15)
    // Weights for soft scoring (sum to 1.0)
    double hypothesis_3_weight_centrality = 0.25;     // Intervention point criticality
    double hypothesis_3_weight_effect_size = 0.25;    // Predicted downstream impact
    double hypothesis_3_weight_mechanism = 0.20;      // LLM mechanistic coherence
    double hypothesis_3_weight_testability = 0.15;    // How testable is the intervention?
    double hypothesis_3_weight_confounding = 0.15;    // Lower if confounders detected

    // Author reference chains
    size_t author_chain_max_candidates = 200; // Max author chains to evaluate

    // Co-authorship networks
    int co_authorship_min_collaborations = 2; // Minimum joint works to report
    size_t co_authorship_max_candidates = 200; // Max co-authorship pairs to report

    // Citation impact analysis
    int citation_impact_min_citations = 3; // Minimum incoming citations to report
    size_t citation_impact_top_k = 50; // Top K most cited authors/works

    // ========== Category 3: Causal & Mechanistic Discovery ==========

    // Causal chain detection
    size_t causal_chain_max_candidates = 250;      // Max causal chains to report (increased for hypotheses_3)
    int causal_chain_min_length = 2;               // Minimum chain length (edges)
    int causal_chain_max_length = 6;               // Maximum chain length (edges)
    double causal_chain_min_confidence = 0.2;      // Minimum chain confidence score (lowered for hypotheses_3)
    size_t causal_chain_max_evidence_edges = 10;   // Max evidence edges per chain

    // Intervention point detection
    size_t intervention_max_candidates = 200;      // Max intervention points to report (increased for hypotheses_3)
    double intervention_min_criticality = 0.15;    // Min criticality score (0-1) (lowered for hypotheses_3)
    size_t intervention_max_evidence_edges = 8;    // Max evidence edges per point

    // Feedback loop detection
    size_t feedback_loop_max_candidates = 150;     // Max feedback loops to report (increased for hypotheses_3)
    int feedback_loop_min_length = 2;              // Minimum loop length (edges)
    int feedback_loop_max_length = 5;              // Maximum loop length (edges)
    double feedback_loop_min_confidence = 0.15;    // Min loop confidence score (lowered for hypotheses_3)
    size_t feedback_loop_max_evidence_edges = 10;  // Max evidence edges per loop

    // Confounder detection
    size_t confounder_max_candidates = 200;        // Max confounders to report (increased for hypotheses_3)
    double confounder_min_association = 0.1;       // Min association strength (lowered for hypotheses_3)
    size_t confounder_max_evidence_edges = 8;      // Max evidence edges per confounder

    // ========== Category 4: Advanced Reasoning & Structural Discovery ==========

    // Taxonomy induction (hierarchical is-a and part-of relationships)
    size_t taxonomy_max_candidates = 150;          // Max taxonomy relations to report
    double taxonomy_min_confidence = 0.4;          // Min confidence for hierarchical relation
    size_t taxonomy_max_depth = 5;                 // Max hierarchy depth to explore
    size_t taxonomy_max_evidence_edges = 8;        // Max evidence edges per relation
    bool taxonomy_use_llm = true;                  // Use LLM to validate hierarchies

    // Domain bridge detection (interdisciplinary connectors)
    size_t domain_bridge_max_candidates = 100;     // Max domain bridges to report
    double domain_bridge_min_diversity = 0.3;      // Min diversity score (cluster variance)
    size_t domain_bridge_min_domains = 2;          // Min number of domains connected
    size_t domain_bridge_max_evidence_edges = 10;  // Max evidence edges per bridge
    bool domain_bridge_use_taxonomy = true;        // Leverage taxonomy if available

    // Logical entailment discovery (if X and Y, then Z)
    size_t entailment_max_candidates = 120;        // Max entailments to report
    double entailment_min_confidence = 0.5;        // Min confidence for entailment rule
    size_t entailment_max_premise_size = 3;        // Max premises in rule (X and Y and ...)
    size_t entailment_max_evidence_edges = 8;      // Max evidence edges per entailment
    bool entailment_use_llm = true;                // Use LLM to validate entailments

    // Compositional reasoning (relation algebra)
    size_t composition_max_candidates = 150;       // Max composed relations to report
    double composition_min_confidence = 0.4;       // Min confidence for composed relation
    size_t composition_max_path_length = 3;        // Max composition chain length
    size_t composition_max_evidence_edges = 10;    // Max evidence edges per composition
    bool composition_use_llm = false;              // Use LLM to validate compositions (expensive)

    // Embedding-based link prediction (TransE/RotatE/ComplEx)
    size_t embedding_dim = 50;           // Embedding dimension
    size_t embedding_epochs = 100;       // Training epochs
    double embedding_learning_rate = 0.01; // Learning rate for SGD
    double embedding_margin = 1.0;       // Margin for ranking loss (TransE)
    size_t embedding_neg_samples = 5;    // Negative samples per positive triple
    size_t embedding_max_candidates = 200;  // Max link predictions to evaluate
    double embedding_min_score = 0.7;    // Minimum plausibility score threshold
    size_t embedding_min_evidence_edges = 1; // Minimum witness edges required
    size_t embedding_allowed_relations_top_k = 30; // Allow only top-k frequent relations
    double embedding_min_neighbor_overlap = 0.05; // Minimum Jaccard overlap
    size_t embedding_batch_size = 128;   // Mini-batch size for training
    std::string embedding_model = "transe"; // Model type: "transe", "rotate", "complex"

    // Global
    size_t max_total_insights = 2000;    // Hard cap total insights

    // Dynamic calibration targets
    size_t target_insights_per_operator = 20; // Soft target per operator
    size_t target_total_insights = 100;       // Soft global target
    bool adaptive_thresholds = true;          // Enable adaptive pruning
};

// Progress callback
using DiscoveryProgressCallback = std::function<void(const std::string& stage, int current, int total)>;

class DiscoveryEngine {
public:
    DiscoveryEngine(const Hypergraph& graph, const HypergraphIndex& index);

    // Set configuration
    void set_config(const DiscoveryConfig& config) { config_ = config; }
    void set_run_id(const std::string& run_id) { run_id_ = run_id; }
    void set_progress_callback(DiscoveryProgressCallback cb) { progress_cb_ = std::move(cb); }
    void set_llm_provider(const std::shared_ptr<LLMProvider>& provider) { llm_provider_ = provider; }

    // Individual operators
    std::vector<Insight> find_bridges();
    std::vector<Insight> find_completions();
    std::vector<Insight> find_motifs();
    std::vector<Insight> find_substitutions();
    std::vector<Insight> find_contradictions();
    std::vector<Insight> find_entity_resolutions();
    std::vector<Insight> find_core_periphery();
    std::vector<Insight> find_text_similarity_links();
    std::vector<Insight> find_argument_support_relations();
    std::vector<Insight> find_active_learning_queries();
    std::vector<Insight> find_method_outcome_nodes();
    std::vector<Insight> find_centrality_nodes();
    std::vector<Insight> find_community_structures();
    std::vector<Insight> find_k_core_nodes();
    std::vector<Insight> find_k_truss_edges();
    std::vector<Insight> find_claim_stances();
    std::vector<Insight> find_relation_induction();
    std::vector<Insight> find_analogical_transfers();
    std::vector<Insight> find_uncertainty_samples();
    std::vector<Insight> find_counterfactual_probes();
    std::vector<Insight> find_hyperedge_predictions();
    std::vector<Insight> compute_diffusion_relevance(const std::string& seed_node);
    std::vector<Insight> find_diffusions();
    std::vector<Insight> find_surprise_edges();
    std::vector<Insight> find_rules();
    std::vector<Insight> find_path_rankings();
    std::vector<Insight> find_intersection_hypothesis_bridges();
    std::vector<Insight> find_long_chains();
    std::vector<Insight> find_meta_path_links();
    std::vector<Insight> find_meta_path_patterns();
    std::vector<Insight> find_community_links();
    std::vector<Insight> find_hypotheses_1(const InsightCollection& collection);
    std::vector<Insight> find_hypotheses_2(const InsightCollection& collection);  // Typed mechanistic chains + explanation subgraphs
    std::vector<Insight> find_hypotheses_3(const InsightCollection& collection);  // Counterfactual causal hypotheses
    std::vector<Insight> find_mechanism_consolidations(const InsightCollection& collection);
    std::vector<Insight> find_evidence_fusion_links(const InsightCollection& collection);
    std::vector<Insight> find_meta_path_anomalies(const InsightCollection& collection);
    std::vector<Insight> find_intervention_bottlenecks(const InsightCollection& collection);
    std::vector<Insight> find_competing_mechanisms(const InsightCollection& collection);
    std::vector<Insight> find_schema_repairs(const InsightCollection& collection);
    std::vector<Insight> find_cross_community_mechanism_bridges(const InsightCollection& collection);
    std::vector<Insight> find_embedding_links();  // TransE/RotatE/ComplEx link prediction
    std::vector<Insight> find_author_reference_chains();
    std::vector<Insight> find_co_authorship_networks();
    std::vector<Insight> find_citation_impact();
    std::vector<Insight> find_multi_resolution_communities();
    std::vector<Insight> find_cross_community_bridge_maps(const InsightCollection& collection);
    std::vector<Insight> find_meta_patterns(const InsightCollection& collection);
    std::vector<Insight> find_bridge_analogies(const InsightCollection& collection);
    std::vector<Insight> compute_bias_audit(const InsightCollection& collection);
    std::vector<Insight> generate_community_recommendations(const InsightCollection& collection);

    // Category 3: Causal & Mechanistic Discovery
    std::vector<Insight> find_causal_chains();       // Directed paths with causal relations
    std::vector<Insight> find_intervention_points(); // Critical nodes in causal pathways
    std::vector<Insight> find_feedback_loops();      // Cycles in causal/directional relations
    std::vector<Insight> find_confounders();         // Potential confounding variables

    // Category 4: Advanced Reasoning & Structural Discovery
    std::vector<Insight> find_taxonomy_induction();  // Hierarchical is-a and part-of relationships
    std::vector<Insight> find_domain_bridges();      // Cross-domain/interdisciplinary connectors
    std::vector<Insight> find_logical_entailments(); // Inferred implications (if X and Y, then Z)
    std::vector<Insight> find_compositional_reasoning(); // Composed relations via relation algebra
    std::vector<Insight> find_explanatory_chains();      // Multi-hop explanation paths
    std::vector<Insight> find_schema_violations();       // Entities/relations violating expected patterns
    std::vector<Insight> find_transitive_closure_gaps(); // Missing links implied by transitive relations

    // Run multiple operators
    InsightCollection run_operators(const std::vector<std::string>& operators);

    // Run all operators
    InsightCollection run_all();

private:
    const Hypergraph& graph_;
    const HypergraphIndex& index_;
    DiscoveryConfig config_;
    std::string run_id_;
    DiscoveryProgressCallback progress_cb_;
    std::shared_ptr<LLMProvider> llm_provider_;
    int insight_counter_ = 0;

    // Helper: generate insight ID
    std::string make_insight_id(InsightType type);

    // Helper: compute score
    double compute_score(const Insight& insight);

    // Helper: get node label
    std::string get_node_label(const std::string& node_id) const;

    // Helper: get chunk IDs from edges
    std::vector<std::string> get_chunk_ids(const std::vector<std::string>& edge_ids) const;

    // Helper: get source document paths from edges (for provenance tracking)
    std::vector<std::string> get_source_documents(const std::vector<std::string>& edge_ids) const;

    // Helper: report progress
    void report_progress(const std::string& stage, int current, int total);

    // Helper: filter author reference findings
    bool is_author_reference_insight(const Insight& insight) const;

    // ========== Embedding Link Prediction Helpers ==========

    // Triple structure for knowledge graph triples
    struct Triple {
        size_t head;      // Entity index
        size_t relation;  // Relation index
        size_t tail;      // Entity index
    };

    // Embedding model state
    struct EmbeddingModel {
        std::vector<std::vector<double>> entity_embeddings;   // [num_entities][dim]
        std::vector<std::vector<double>> relation_embeddings; // [num_relations][dim]
        std::unordered_map<std::string, size_t> entity_to_idx;
        std::unordered_map<std::string, size_t> relation_to_idx;
        std::vector<std::string> idx_to_entity;
        std::vector<std::string> idx_to_relation;
    };

    // Extract triples from hypergraph (convert hyperedges to binary relations)
    std::vector<Triple> extract_triples(EmbeddingModel& model) const;

    // Initialize embeddings randomly
    void init_embeddings(EmbeddingModel& model, size_t num_entities, size_t num_relations) const;

    // TransE scoring: ||h + r - t||
    double transe_score(const EmbeddingModel& model, const Triple& triple) const;

    // Train TransE model
    void train_transe(EmbeddingModel& model, const std::vector<Triple>& triples);

    // Generate negative sample (corrupt head or tail)
    Triple corrupt_triple(const Triple& triple, size_t num_entities) const;

    // Predict missing links
    std::vector<std::pair<Triple, double>> predict_links(
        const EmbeddingModel& model,
        const std::vector<Triple>& existing_triples) const;

    // L2 normalize a vector
    void normalize_vector(std::vector<double>& vec) const;

    // ========== Causal Discovery Helpers ==========

    // Check if a relation type suggests causality
    bool is_causal_relation(const std::string& relation) const;

    // Check if a relation type suggests directionality (broader than causal)
    bool is_directional_relation(const std::string& relation) const;

    // Structure for representing a causal path
    struct CausalPath {
        std::vector<std::string> nodes;       // Ordered sequence of nodes
        std::vector<std::string> edges;       // Edge IDs connecting the nodes
        std::vector<std::string> relations;   // Relation types along the path
        double confidence = 0.0;              // Path confidence score
    };

    // Find all causal paths from a source node using DFS
    std::vector<CausalPath> find_causal_paths_from(
        const std::string& start_node,
        int max_length,
        std::set<std::string>& visited_nodes) const;

    // Detect cycles in the causal subgraph using DFS
    std::vector<CausalPath> detect_cycles_from(
        const std::string& start_node,
        int max_length) const;

    // Compute criticality score for a node in causal pathways
    double compute_intervention_criticality(
        const std::string& node_id,
        const std::vector<CausalPath>& all_paths) const;

    // Build the causal subgraph (edges with causal relations only)
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>
        build_causal_adjacency() const;
};

} // namespace kg

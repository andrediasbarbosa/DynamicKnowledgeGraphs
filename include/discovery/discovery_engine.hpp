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

namespace kg {

// Discovery configuration
struct DiscoveryConfig {
    // Bridge detection
    int bridge_s_threshold = 2;          // Minimum s-value for component separation
    int bridge_max_path_length = 4;      // Maximum path length for bridges
    size_t bridge_max_candidates = 200;  // Max bridge candidates to evaluate

    // Completion detection
    double completion_min_confidence = 0.5;
    size_t completion_max_candidates = 200;
    int completion_min_shared_edges = 2; // Min shared hyperedges for completion pattern

    // Motif mining
    int motif_min_support = 4;           // Minimum co-occurrence count
    double motif_min_lift = 2.0;         // Minimum lift (observed / expected)
    size_t motif_max_size = 5;           // Maximum motif size
    size_t motif_max_candidates = 200;

    // Substitution detection
    double substitution_similarity_threshold = 0.8;
    size_t substitution_max_candidates = 200;

    // Diffusion (PageRank-style)
    int diffusion_iterations = 20;
    double diffusion_damping = 0.85;
    size_t diffusion_top_k = 50;

    // Surprise detection
    double surprise_max_expected_cooccurrence = 0.1;
    size_t surprise_max_candidates = 200;

    // Rule mining (association rules)
    int rule_min_support = 3;            // Minimum occurrences of rule body
    double rule_min_confidence = 0.5;    // Minimum P(head|body)
    double rule_min_lift = 1.2;          // Minimum lift (confidence / P(head))
    size_t rule_max_candidates = 200;    // Max rules to evaluate

    // Path ranking (path-based link prediction)
    int path_rank_k = 5;                 // K shortest paths to consider
    int path_rank_max_hops = 3;          // Maximum path length (edges)
    int path_rank_min_intersection = 1;  // Minimum s-connectivity for path search
    size_t path_rank_max_seed_nodes = 200; // Top-degree nodes to evaluate
    size_t path_rank_max_pairs = 2000;   // Max candidate pairs to test
    size_t path_rank_max_candidates = 200; // Max insights to evaluate
    size_t path_rank_max_witness_edges = 50; // Cap evidence edges stored
    double path_rank_min_score = 0.85;   // Minimum path score to keep
    size_t path_rank_min_evidence_edges = 2; // Minimum evidence edges to keep

    // Community-aware cross-cluster links
    int community_s_threshold = 2;       // s-value for component separation
    size_t community_top_nodes_per_component = 15; // Candidate nodes per component
    double community_min_relation_overlap = 0.2; // Min Jaccard overlap of relation signatures
    size_t community_max_candidates = 200;  // Max insights to evaluate

    // Hypotheses synthesis
    size_t hypothesis_count = 3;         // Number of hypotheses to generate

    // Author reference chains
    size_t author_chain_max_candidates = 200; // Max author chains to evaluate

    // Embedding-based link prediction (TransE/RotatE/ComplEx)
    size_t embedding_dim = 50;           // Embedding dimension
    size_t embedding_epochs = 100;       // Training epochs
    double embedding_learning_rate = 0.01; // Learning rate for SGD
    double embedding_margin = 1.0;       // Margin for ranking loss (TransE)
    size_t embedding_neg_samples = 5;    // Negative samples per positive triple
    size_t embedding_max_candidates = 200;  // Max link predictions to evaluate
    double embedding_min_score = 0.8;    // Minimum plausibility score threshold
    size_t embedding_min_evidence_edges = 2; // Minimum witness edges required
    size_t embedding_allowed_relations_top_k = 20; // Allow only top-k frequent relations
    double embedding_min_neighbor_overlap = 0.1; // Minimum Jaccard overlap
    size_t embedding_batch_size = 128;   // Mini-batch size for training
    std::string embedding_model = "transe"; // Model type: "transe", "rotate", "complex"

    // Global
    size_t max_total_insights = 2000;    // Hard cap total insights

    // Dynamic calibration targets
    size_t target_insights_per_operator = 10; // Soft target per operator
    size_t target_total_insights = 10;        // Soft global target
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

    // Individual operators
    std::vector<Insight> find_bridges();
    std::vector<Insight> find_completions();
    std::vector<Insight> find_motifs();
    std::vector<Insight> find_substitutions();
    std::vector<Insight> compute_diffusion_relevance(const std::string& seed_node);
    std::vector<Insight> find_diffusions();
    std::vector<Insight> find_surprise_edges();
    std::vector<Insight> find_rules();
    std::vector<Insight> find_path_rankings();
    std::vector<Insight> find_community_links();
    std::vector<Insight> find_hypotheses(const InsightCollection& collection);
    std::vector<Insight> find_embedding_links();  // TransE/RotatE/ComplEx link prediction
    std::vector<Insight> find_author_reference_chains();

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
    int insight_counter_ = 0;

    // Helper: generate insight ID
    std::string make_insight_id(InsightType type);

    // Helper: compute score
    double compute_score(const Insight& insight);

    // Helper: get node label
    std::string get_node_label(const std::string& node_id) const;

    // Helper: get chunk IDs from edges
    std::vector<std::string> get_chunk_ids(const std::vector<std::string>& edge_ids) const;

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
};

} // namespace kg

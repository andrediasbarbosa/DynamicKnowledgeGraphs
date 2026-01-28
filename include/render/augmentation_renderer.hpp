#pragma once

#include "discovery/insight.hpp"
#include "graph/hypergraph.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

namespace kg {

// Augmentation node (new nodes to add to the graph)
struct AugmentationNode {
    std::string id;             // "aug:n:000001"
    std::string label;
    std::string type;           // "entity" or "relation"
    bool is_new = true;
    std::string insight_id;
    double confidence = 0.0;
    std::vector<std::string> evidence_chunk_ids;
    std::vector<std::string> witness_edges;  // Changed from uint32_t to string

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["id"] = id;
        j["label"] = label;
        j["type"] = type;
        j["is_new"] = is_new;
        j["insight_id"] = insight_id;
        j["confidence"] = confidence;
        j["evidence_chunk_ids"] = evidence_chunk_ids;
        j["witness_edges"] = witness_edges;
        return j;
    }
};

// Augmentation link (new links to add)
struct AugmentationLink {
    std::string source;         // Can be numeric ID (for baseline) or string (for new)
    std::string target;
    std::string type;           // "source" or "target"
    bool is_new = true;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["source"] = source;
        j["target"] = target;
        j["type"] = type;
        j["is_new"] = is_new;
        return j;
    }
};

// Full augmentation data
struct AugmentationData {
    std::string run_id;
    std::string created_utc;
    std::string source;
    std::string notes;
    std::vector<AugmentationNode> nodes;
    std::vector<AugmentationLink> links;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["meta"] = {
            {"run_id", run_id},
            {"created_utc", created_utc},
            {"source", source},
            {"notes", notes}
        };

        nlohmann::json nodes_arr = nlohmann::json::array();
        for (const auto& n : nodes) {
            nodes_arr.push_back(n.to_json());
        }
        j["nodes"] = nodes_arr;

        nlohmann::json links_arr = nlohmann::json::array();
        for (const auto& l : links) {
            links_arr.push_back(l.to_json());
        }
        j["links"] = links_arr;

        return j;
    }

    void save_to_json(const std::string& path) const {
        std::ofstream file(path);
        file << to_json().dump(2);
    }
};

// Augmentation renderer - converts insights to overlay format
class AugmentationRenderer {
public:
    AugmentationRenderer(const Hypergraph& graph);

    // Convert insights to augmentation data
    AugmentationData convert(const InsightCollection& insights);

    // Export augmented HTML
    void export_augmented_html(const std::string& filename,
                               const std::string& title,
                               const AugmentationData& augmentation);

private:
    const Hypergraph& graph_;
    int aug_node_counter_ = 0;

    std::string make_aug_node_id();
    std::string get_node_label(const std::string& node_id) const;

    // Normalize a node ID to match the graph's normalized storage
    // This ensures augmentation links correctly reference existing nodes
    std::string normalize_graph_ref(const std::string& node_id) const;

    // Check if two nodes are already connected in the base graph
    // Used to filter out discoveries that duplicate existing relationships
    bool nodes_already_connected(const std::string& node_a, const std::string& node_b) const;

    // Check if a node ID refers to an existing graph node (not an augmentation node)
    bool is_existing_node(const std::string& node_id) const;

    // Conversion helpers for each insight type
    void convert_bridge(const Insight& insight, AugmentationData& data);
    void convert_completion(const Insight& insight, AugmentationData& data);
    void convert_motif(const Insight& insight, AugmentationData& data);
    void convert_substitution(const Insight& insight, AugmentationData& data);
    void convert_contradiction(const Insight& insight, AugmentationData& data);
    void convert_entity_resolution(const Insight& insight, AugmentationData& data);
    void convert_core_periphery(const Insight& insight, AugmentationData& data);
    void convert_text_similarity(const Insight& insight, AugmentationData& data);
    void convert_argument_support(const Insight& insight, AugmentationData& data);
    void convert_active_learning(const Insight& insight, AugmentationData& data);
    void convert_method_outcome(const Insight& insight, AugmentationData& data);
    void convert_centrality(const Insight& insight, AugmentationData& data);
    void convert_community_detection(const Insight& insight, AugmentationData& data);
    void convert_k_core(const Insight& insight, AugmentationData& data);
    void convert_k_truss(const Insight& insight, AugmentationData& data);
    void convert_claim_stance(const Insight& insight, AugmentationData& data);
    void convert_relation_induction(const Insight& insight, AugmentationData& data);
    void convert_analogical_transfer(const Insight& insight, AugmentationData& data);
    void convert_uncertainty_sampling(const Insight& insight, AugmentationData& data);
    void convert_counterfactual(const Insight& insight, AugmentationData& data);
    void convert_hyperedge_prediction(const Insight& insight, AugmentationData& data);
    void convert_constrained_rule(const Insight& insight, AugmentationData& data);
    void convert_diffusion(const Insight& insight, AugmentationData& data);
    void convert_surprise(const Insight& insight, AugmentationData& data);
    void convert_community_link(const Insight& insight, AugmentationData& data);
    void convert_path_rank(const Insight& insight, AugmentationData& data);
    void convert_hypothesis(const Insight& insight, AugmentationData& data);
    void convert_rule(const Insight& insight, AugmentationData& data);
    void convert_embedding_link(const Insight& insight, AugmentationData& data);
    void convert_author_chain(const Insight& insight, AugmentationData& data);
};

} // namespace kg

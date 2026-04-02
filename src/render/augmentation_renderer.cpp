#include "render/augmentation_renderer.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace kg {

AugmentationRenderer::AugmentationRenderer(const Hypergraph& graph)
    : graph_(graph) {}

std::string AugmentationRenderer::make_aug_node_id() {
    std::stringstream ss;
    ss << "aug:n:" << std::setfill('0') << std::setw(6) << (++aug_node_counter_);
    return ss.str();
}

std::string AugmentationRenderer::get_node_label(const std::string& node_id) const {
    const auto* node = graph_.get_node(node_id);
    return node ? node->label : "";
}

std::string AugmentationRenderer::normalize_graph_ref(const std::string& node_id) const {
    // Normalize node ID to match how the graph stores nodes
    // This ensures augmentation links correctly connect to existing graph nodes
    return Hypergraph::normalize_node_id(node_id);
}

bool AugmentationRenderer::is_existing_node(const std::string& node_id) const {
    // Augmentation nodes have IDs starting with "aug:"
    if (node_id.find("aug:") == 0) return false;
    return graph_.has_node(node_id);
}

bool AugmentationRenderer::nodes_already_connected(const std::string& node_a, const std::string& node_b) const {
    // Check if two nodes are already connected via any edge in the base graph
    std::string norm_a = normalize_graph_ref(node_a);
    std::string norm_b = normalize_graph_ref(node_b);

    if (norm_a == norm_b) return true;  // Same node after normalization

    // Get edges incident to node_a
    auto edges = graph_.get_incident_edges(norm_a);
    for (const auto& edge : edges) {
        // Check if node_b is in this edge's sources or targets
        for (const auto& src : edge.sources) {
            if (src == norm_b) return true;
        }
        for (const auto& tgt : edge.targets) {
            if (tgt == norm_b) return true;
        }
    }
    return false;
}

AugmentationData AugmentationRenderer::convert(const InsightCollection& insights) {
    AugmentationData data;
    data.run_id = insights.run_id;
    data.source = "kg render";
    data.notes = "Overlay candidates only; not ground truth.";

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    data.created_utc = ss.str();

    for (const auto& insight : insights.insights) {
        // Skip insights where the primary seed nodes are already connected
        // (only for link-suggestion operators)
        bool skip_insight = false;
        bool check_existing = (insight.type == InsightType::COMPLETION ||
                               insight.type == InsightType::SUBSTITUTION ||
                               insight.type == InsightType::TEXT_SIMILARITY ||
                               insight.type == InsightType::ARGUMENT_SUPPORT ||
                               insight.type == InsightType::COMMUNITY_LINK ||
                               insight.type == InsightType::PATH_RANK ||
                               insight.type == InsightType::META_PATH ||
                               insight.type == InsightType::EMBEDDING_LINK ||
                               insight.type == InsightType::ANALOGICAL_TRANSFER ||
                               insight.type == InsightType::HYPEREDGE_PREDICTION);
        if (check_existing && insight.seed_nodes.size() >= 2) {
            std::string norm_a = normalize_graph_ref(insight.seed_nodes[0]);
            std::string norm_b = normalize_graph_ref(insight.seed_nodes[1]);

            if (norm_a == norm_b) {
                skip_insight = true;
            } else if (nodes_already_connected(norm_a, norm_b)) {
                skip_insight = true;
            }
        }

        if (skip_insight) continue;

        switch (insight.type) {
            case InsightType::BRIDGE:
                convert_bridge(insight, data);
                break;
            case InsightType::COMPLETION:
                convert_completion(insight, data);
                break;
            case InsightType::MOTIF:
                convert_motif(insight, data);
                break;
            case InsightType::SUBSTITUTION:
                convert_substitution(insight, data);
                break;
            case InsightType::CONTRADICTION:
                convert_contradiction(insight, data);
                break;
            case InsightType::ENTITY_RESOLUTION:
                convert_entity_resolution(insight, data);
                break;
            case InsightType::CORE_PERIPHERY:
                convert_core_periphery(insight, data);
                break;
            case InsightType::TEXT_SIMILARITY:
                convert_text_similarity(insight, data);
                break;
            case InsightType::ARGUMENT_SUPPORT:
                convert_argument_support(insight, data);
                break;
            case InsightType::ACTIVE_LEARNING:
                convert_active_learning(insight, data);
                break;
            case InsightType::METHOD_OUTCOME:
                convert_method_outcome(insight, data);
                break;
            case InsightType::CENTRALITY:
                convert_centrality(insight, data);
                break;
            case InsightType::COMMUNITY_DETECTION:
                convert_community_detection(insight, data);
                break;
            case InsightType::K_CORE:
                convert_k_core(insight, data);
                break;
            case InsightType::K_TRUSS:
                convert_k_truss(insight, data);
                break;
            case InsightType::CLAIM_STANCE:
                convert_claim_stance(insight, data);
                break;
            case InsightType::RELATION_INDUCTION:
                convert_relation_induction(insight, data);
                break;
            case InsightType::ANALOGICAL_TRANSFER:
                convert_analogical_transfer(insight, data);
                break;
            case InsightType::UNCERTAINTY_SAMPLING:
                convert_uncertainty_sampling(insight, data);
                break;
            case InsightType::COUNTERFACTUAL:
                convert_counterfactual(insight, data);
                break;
            case InsightType::HYPEREDGE_PREDICTION:
                convert_hyperedge_prediction(insight, data);
                break;
            case InsightType::DIFFUSION:
                convert_diffusion(insight, data);
                break;
            case InsightType::SURPRISE:
                convert_surprise(insight, data);
                break;
            case InsightType::COMMUNITY_LINK:
                convert_community_link(insight, data);
                break;
            case InsightType::PATH_RANK:
                convert_path_rank(insight, data);
                break;
            case InsightType::LONG_CHAIN:
                convert_long_chain(insight, data);
                break;
            case InsightType::META_PATH:
                convert_meta_path(insight, data);
                break;
            case InsightType::META_PATH_PATTERN:
                convert_meta_path(insight, data);
                break;
            case InsightType::HYPOTHESES_1:
            case InsightType::HYPOTHESES_2:
            case InsightType::HYPOTHESES_3:
                convert_hypothesis(insight, data);
                break;
            case InsightType::MECHANISM_CONSOLIDATION:
                convert_hypothesis(insight, data);
                break;
            case InsightType::EVIDENCE_FUSION_LINK:
                convert_argument_support(insight, data);
                break;
            case InsightType::META_PATH_ANOMALY:
                convert_meta_path(insight, data);
                break;
            case InsightType::INTERVENTION_BOTTLENECK:
                convert_intervention_point(insight, data);
                break;
            case InsightType::COMPETING_MECHANISM:
                convert_hypothesis(insight, data);
                break;
            case InsightType::SCHEMA_REPAIR:
                convert_rule(insight, data);
                break;
            case InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE:
                convert_community_link(insight, data);
                break;
            case InsightType::TAXONOMY:
            case InsightType::DOMAIN_BRIDGE:
            case InsightType::LOGICAL_ENTAILMENT:
            case InsightType::COMPOSITIONAL_REASONING:
            case InsightType::EXPLANATORY_CHAIN:
            case InsightType::SCHEMA_VIOLATION:
            case InsightType::TRANSITIVE_CLOSURE:
                convert_rule(insight, data);
                break;
            case InsightType::RULE:
                convert_rule(insight, data);
                break;
            case InsightType::EMBEDDING_LINK:
                convert_embedding_link(insight, data);
                break;
            case InsightType::AUTHOR_CHAIN:
                convert_author_chain(insight, data);
                break;
            // Category 3: Causal & Mechanistic Discovery
            case InsightType::CAUSAL_CHAIN:
                convert_causal_chain(insight, data);
                break;
            case InsightType::INTERVENTION_POINT:
                convert_intervention_point(insight, data);
                break;
            case InsightType::FEEDBACK_LOOP:
                convert_feedback_loop(insight, data);
                break;
            case InsightType::CONFOUNDER:
                convert_confounder(insight, data);
                break;
            case InsightType::CROSS_COMMUNITY_BRIDGE_MAP:
                convert_cross_community_bridge_map(insight, data);
                break;
            case InsightType::MULTI_RESOLUTION_COMMUNITY:
                convert_multi_resolution_community(insight, data);
                break;
            case InsightType::META_PATTERN:
                convert_meta_pattern(insight, data);
                break;
            // Epistemic Discovery
            case InsightType::EVIDENCE_DEBT:
                convert_evidence_debt(insight, data);
                break;
            case InsightType::CONSENSUS_FRONTIER:
                convert_consensus_frontier(insight, data);
                break;
            case InsightType::BOUNDARY_CONDITION_MAP:
                convert_boundary_condition_map(insight, data);
                break;
            case InsightType::FAILURE_MODE_TOPOLOGY:
                convert_failure_mode_topology(insight, data);
                break;
            case InsightType::BENCHMARK_DEPENDENCE:
                convert_benchmark_dependence(insight, data);
                break;
            case InsightType::CONCEPT_DRIFT:
                convert_concept_drift(insight, data);
                break;
            case InsightType::PREMISE_BOTTLENECK:
                convert_premise_bottleneck(insight, data);
                break;
            case InsightType::TRANSLATION_GAP:
                convert_translation_gap(insight, data);
                break;
        }
    }

    // V2: Apply semantic simplification to all augmented node labels
    for (auto& node : data.nodes) {
        if (!node.label.empty()) {
            std::string original_label = node.label;
            node.label = simplify_label(node.label);

            // Store original if it changed and not already in properties
            if (node.label != original_label &&
                node.properties.find("original_label") == node.properties.end() &&
                node.properties.find("full_description") == node.properties.end()) {
                node.properties["original_label"] = original_label;
            }
        }
    }

    return data;
}

void AugmentationRenderer::convert_bridge(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (just the bridge entity)
    rel_node.label = insight.seed_labels.empty() ? "bridge" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (!insight.seed_labels.empty()) {
        rel_node.properties["full_description"] = "Bridge: " + insight.seed_labels[0];
        rel_node.properties["insight_type"] = "bridge";
    }

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (const auto& seed : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(seed);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int link_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        bool is_seed = false;
        for (const auto& seed : insight.seed_nodes) {
            if (normalize_graph_ref(seed) == norm_wn) {
                is_seed = true;
                break;
            }
        }
        if (!is_seed && link_count < 3) {
            AugmentationLink link;
            link.source = rel_node.id;
            link.target = norm_wn;
            link.type = "target";
            link.is_new = true;
            data.links.push_back(link);
            link_count++;
        }
    }
}

void AugmentationRenderer::convert_completion(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity)
    rel_node.label = insight.seed_labels.empty() ? "completion" : insight.seed_labels[0];

    // V2: Store full description in properties
    std::string full_desc = "Completion: ";
    for (size_t i = 0; i < insight.seed_labels.size() && i < 2; ++i) {
        if (i > 0) full_desc += " + ";
        full_desc += insight.seed_labels[i];
    }
    rel_node.properties["full_description"] = full_desc;
    rel_node.properties["insight_type"] = "completion";
    if (insight.seed_labels.size() >= 2) {
        rel_node.properties["completion_nodes"] = insight.seed_labels[0] + ", " + insight.seed_labels[1];
    }

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int filler_count = 0;
    for (size_t i = 2; i < insight.witness_nodes.size() && filler_count < 3; ++i) {
        AugmentationLink link;
        link.source = rel_node.id;
        link.target = normalize_graph_ref(insight.witness_nodes[i]);
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        filler_count++;
    }
}

void AugmentationRenderer::convert_motif(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity or motif count)
    if (!insight.seed_labels.empty()) {
        rel_node.label = insight.seed_labels[0];
    } else {
        rel_node.label = "motif-" + std::to_string(insight.seed_nodes.size());
    }

    // V2: Store full motif description in properties
    std::string full_desc = "Motif: {";
    for (size_t i = 0; i < insight.seed_labels.size(); ++i) {
        if (i > 0) full_desc += ", ";
        full_desc += insight.seed_labels[i];
    }
    full_desc += "}";
    rel_node.properties["full_description"] = full_desc;
    rel_node.properties["insight_type"] = "motif";
    rel_node.properties["motif_size"] = std::to_string(insight.seed_nodes.size());

    // Store all motif members
    std::string members;
    for (size_t i = 0; i < insight.seed_labels.size(); ++i) {
        if (i > 0) members += ", ";
        members += insight.seed_labels[i];
    }
    rel_node.properties["motif_members"] = members;

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;

    data.nodes.push_back(rel_node);

    for (const auto& member : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(member);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_substitution(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity)
    rel_node.label = insight.seed_labels.empty() ? "substitute" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (insight.seed_labels.size() >= 2) {
        rel_node.properties["full_description"] = "Substitute: " + insight.seed_labels[0] + " <-> " + insight.seed_labels[1];
        rel_node.properties["substitute_nodes"] = insight.seed_labels[0] + ", " + insight.seed_labels[1];
    }
    rel_node.properties["insight_type"] = "substitution";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2 && i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int ctx_count = 0;
    for (size_t i = 2; i < insight.witness_nodes.size() && ctx_count < 2; ++i) {
        AugmentationLink link;
        link.source = rel_node.id;
        link.target = normalize_graph_ref(insight.witness_nodes[i]);
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        ctx_count++;
    }
}

void AugmentationRenderer::convert_contradiction(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity)
    rel_node.label = insight.seed_labels.empty() ? "contradiction" : insight.seed_labels[0];

    // V2: Store full description in properties
    std::string full_desc = "Contradiction: ";
    for (size_t i = 0; i < insight.seed_labels.size() && i < 2; ++i) {
        if (i > 0) full_desc += " + ";
        full_desc += insight.seed_labels[i];
    }
    rel_node.properties["full_description"] = full_desc;
    rel_node.properties["insight_type"] = "contradiction";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 3; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_entity_resolution(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity)
    rel_node.label = insight.seed_labels.empty() ? "resolve" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (insight.seed_labels.size() >= 2) {
        rel_node.properties["full_description"] = "Resolve: " + insight.seed_labels[0] + " ≈ " + insight.seed_labels[1];
        rel_node.properties["resolved_entities"] = insight.seed_labels[0] + ", " + insight.seed_labels[1];
    }
    rel_node.properties["insight_type"] = "entity_resolution";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2 && i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_core_periphery(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string role = "Core";
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "periphery") {
            role = "Periphery";
            break;
        }
    }

    std::string label = role + ": ";
    label += insight.seed_labels.empty() ? "" : insight.seed_labels[0];
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    AugmentationLink link;
    link.source = normalize_graph_ref(insight.seed_nodes[0]);
    link.target = rel_node.id;
    link.type = "source";
    link.is_new = true;
    data.links.push_back(link);
}

void AugmentationRenderer::convert_text_similarity(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Text Similar: ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " ~ " + insight.seed_labels[1];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_argument_support(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    std::string label = "Argument: ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " -> " + insight.seed_labels[1];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_active_learning(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity or "query")
    rel_node.label = insight.seed_labels.empty() ? "query" : insight.seed_labels[0];

    // V2: Store full description in properties
    std::string full_desc = "Query: " + (insight.description.empty() ? "Validate relation" : insight.description);
    rel_node.properties["full_description"] = full_desc;
    rel_node.properties["insight_type"] = "active_learning";
    if (!insight.description.empty()) {
        rel_node.properties["query_text"] = insight.description;
    }

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    int linked = 0;
    for (const auto& node_id : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(node_id);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
        if (++linked >= 3) break;
    }
}

void AugmentationRenderer::convert_method_outcome(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    std::string role = "Method";
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "outcome") {
            role = "Outcome";
            break;
        }
    }

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = role + ": " + (insight.seed_labels.empty() ? "" : insight.seed_labels[0]);
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    AugmentationLink link;
    link.source = normalize_graph_ref(insight.seed_nodes[0]);
    link.target = rel_node.id;
    link.type = "source";
    link.is_new = true;
    data.links.push_back(link);
}

void AugmentationRenderer::convert_centrality(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (just the entity)
    rel_node.label = insight.seed_labels.empty() ? "central" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (!insight.seed_labels.empty()) {
        rel_node.properties["full_description"] = "Centrality: " + insight.seed_labels[0];
        rel_node.properties["insight_type"] = "centrality";
    }

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    AugmentationLink link;
    link.source = normalize_graph_ref(insight.seed_nodes[0]);
    link.target = rel_node.id;
    link.type = "source";
    link.is_new = true;
    data.links.push_back(link);
}

void AugmentationRenderer::convert_community_detection(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (just the entity)
    rel_node.label = insight.seed_labels.empty() ? "community" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (!insight.seed_labels.empty()) {
        rel_node.properties["full_description"] = "Community: " + insight.seed_labels[0];
        rel_node.properties["insight_type"] = "community";
    }

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 3; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_k_core(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (just the entity)
    rel_node.label = insight.seed_labels.empty() ? "k-core" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (!insight.seed_labels.empty()) {
        rel_node.properties["full_description"] = "k-Core: " + insight.seed_labels[0];
    }
    rel_node.properties["insight_type"] = "k_core";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    AugmentationLink link;
    link.source = normalize_graph_ref(insight.seed_nodes[0]);
    link.target = rel_node.id;
    link.type = "source";
    link.is_new = true;
    data.links.push_back(link);
}

void AugmentationRenderer::convert_k_truss(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    std::string label = "k-Truss: ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " — " + insight.seed_labels[1];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_claim_stance(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    std::string stance = "Neutral";
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "supports") stance = "Supports";
        if (tag == "opposes") stance = "Opposes";
        if (tag == "neutral") stance = "Neutral";
    }

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    std::string label = "Claim (" + stance + "): ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " → " + insight.seed_labels[1];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_relation_induction(const Insight& insight, AugmentationData& data) {
    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity or type name)
    rel_node.label = insight.seed_labels.empty() ? "relation" : insight.seed_labels[0];

    // V2: Store full description in properties
    rel_node.properties["full_description"] = "Relation Induction";
    rel_node.properties["insight_type"] = "relation_induction";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    if (!insight.seed_nodes.empty()) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[0]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_analogical_transfer(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity)
    rel_node.label = insight.seed_labels.empty() ? "analogy" : insight.seed_labels[0];

    // V2: Store full description in properties
    if (insight.seed_labels.size() >= 2) {
        rel_node.properties["full_description"] = "Analogy: " + insight.seed_labels[0] + " → " + insight.seed_labels[1];
        rel_node.properties["analogy_nodes"] = insight.seed_labels[0] + ", " + insight.seed_labels[1];
    }
    rel_node.properties["insight_type"] = "analogical_transfer";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_uncertainty_sampling(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    std::string label = "Uncertain: ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " → " + insight.seed_labels[1];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_counterfactual(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (first entity or type name)
    rel_node.label = insight.seed_labels.empty() ? "counterfactual" : insight.seed_labels[0];

    // V2: Store full description in properties
    rel_node.properties["full_description"] = "Counterfactual";
    rel_node.properties["insight_type"] = "counterfactual";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (const auto& node_id : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(node_id);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_hyperedge_prediction(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    std::string label = "Predicted: ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " → " + insight.seed_labels[1];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_diffusion(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Relevance: ";
    if (insight.seed_labels.size() >= 2) {
        label += insight.seed_labels[0] + " -> " + insight.seed_labels[1];
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;

    data.nodes.push_back(rel_node);

    if (insight.seed_nodes.size() >= 2) {
        AugmentationLink link1;
        link1.source = normalize_graph_ref(insight.seed_nodes[0]);
        link1.target = rel_node.id;
        link1.type = "source";
        link1.is_new = true;
        data.links.push_back(link1);

        AugmentationLink link2;
        link2.source = rel_node.id;
        link2.target = normalize_graph_ref(insight.seed_nodes[1]);
        link2.type = "target";
        link2.is_new = true;
        data.links.push_back(link2);
    }
}

void AugmentationRenderer::convert_surprise(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Surprising: {";
    for (size_t i = 0; i < insight.seed_labels.size() && i < 3; ++i) {
        if (i > 0) label += ", ";
        label += insight.seed_labels[i];
    }
    if (insight.seed_labels.size() > 3) label += ", ...";
    label += "}";

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (const auto& member : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(member);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_community_link(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Community: ";
    for (size_t i = 0; i < 2 && i < insight.seed_labels.size(); ++i) {
        if (i > 0) label += " <-> ";
        label += insight.seed_labels[i];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2 && i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int ctx_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        bool is_seed = false;
        for (const auto& seed : insight.seed_nodes) {
            if (normalize_graph_ref(seed) == norm_wn) {
                is_seed = true;
                break;
            }
        }
        if (is_seed) continue;

        AugmentationLink link;
        link.source = rel_node.id;
        link.target = norm_wn;
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        if (++ctx_count >= 2) break;
    }
}

void AugmentationRenderer::convert_path_rank(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "PathRank: ";
    for (size_t i = 0; i < 2 && i < insight.seed_labels.size(); ++i) {
        if (i > 0) label += " <-> ";
        label += insight.seed_labels[i];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2 && i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int ctx_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        bool is_seed = false;
        for (const auto& seed : insight.seed_nodes) {
            if (normalize_graph_ref(seed) == norm_wn) {
                is_seed = true;
                break;
            }
        }
        if (is_seed) continue;

        AugmentationLink link;
        link.source = rel_node.id;
        link.target = norm_wn;
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        if (++ctx_count >= 2) break;
    }
}

void AugmentationRenderer::convert_long_chain(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    int hops = 0;
    auto hop_it = insight.score_breakdown.find("hops");
    if (hop_it != insight.score_breakdown.end()) {
        hops = static_cast<int>(std::round(hop_it->second));
    } else {
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

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "LongChain";
    if (hops > 0) label += " (" + std::to_string(hops) + "h)";
    label += ": ";
    for (size_t i = 0; i < 2 && i < insight.seed_labels.size(); ++i) {
        if (i > 0) label += " <-> ";
        label += insight.seed_labels[i];
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2 && i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int ctx_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        bool is_seed = false;
        for (const auto& seed : insight.seed_nodes) {
            if (normalize_graph_ref(seed) == norm_wn) {
                is_seed = true;
                break;
            }
        }
        if (is_seed) continue;

        AugmentationLink link;
        link.source = rel_node.id;
        link.target = norm_wn;
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        if (++ctx_count >= 2) break;
    }
}

void AugmentationRenderer::convert_meta_path(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string pattern;
    for (const auto& tag : insight.novelty_tags) {
        if (tag.find("pattern=") == 0) {
            pattern = tag.substr(std::string("pattern=").size());
            break;
        }
    }

    std::string label = "MetaPath: ";
    for (size_t i = 0; i < 2 && i < insight.seed_labels.size(); ++i) {
        if (i > 0) label += " <-> ";
        label += insight.seed_labels[i];
    }
    if (!pattern.empty()) {
        label += " [" + pattern + "]";
    }

    if (label.length() > 80) {
        label = label.substr(0, 77) + "...";
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < 2 && i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int ctx_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        bool is_seed = false;
        for (const auto& seed : insight.seed_nodes) {
            if (normalize_graph_ref(seed) == norm_wn) {
                is_seed = true;
                break;
            }
        }
        if (is_seed) continue;

        AugmentationLink link;
        link.source = rel_node.id;
        link.target = norm_wn;
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        if (++ctx_count >= 2) break;
    }
}

void AugmentationRenderer::convert_hypothesis(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Hypothesis: ";
    if (!insight.seed_labels.empty()) {
        label += insight.seed_labels[0];
        if (insight.seed_labels.size() >= 2) {
            label += " <-> " + insight.seed_labels[1];
        }
    }
    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 2; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }

    int ctx_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        bool is_seed = false;
        for (const auto& seed : insight.seed_nodes) {
            if (normalize_graph_ref(seed) == norm_wn) {
                is_seed = true;
                break;
            }
        }
        if (is_seed) continue;

        AugmentationLink link;
        link.source = rel_node.id;
        link.target = norm_wn;
        link.type = "target";
        link.is_new = true;
        data.links.push_back(link);
        if (++ctx_count >= 2) break;
    }
}

void AugmentationRenderer::convert_rule(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // Use description for label (it contains the rule)
    std::string label = insight.description;
    if (label.length() > 60) {
        label = label.substr(0, 57) + "...";
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    // Link example entities to the rule node
    for (const auto& member : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(member);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_embedding_link(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // Extract relation type from novelty tags
    std::string relation = "predicted_link";
    for (const auto& tag : insight.novelty_tags) {
        if (tag.find("relation=") == 0) {
            relation = tag.substr(9);
            break;
        }
    }

    std::string label = "TransE: " + relation;
    if (insight.seed_labels.size() >= 2) {
        label = insight.seed_labels[0] + " --[" + relation + "]--> " + insight.seed_labels[1];
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    // Link the two predicted entities through the relation node
    if (insight.seed_nodes.size() >= 2) {
        AugmentationLink link1;
        link1.source = normalize_graph_ref(insight.seed_nodes[0]);
        link1.target = rel_node.id;
        link1.type = "source";
        link1.is_new = true;
        data.links.push_back(link1);

        AugmentationLink link2;
        link2.source = rel_node.id;
        link2.target = normalize_graph_ref(insight.seed_nodes[1]);
        link2.type = "target";
        link2.is_new = true;
        data.links.push_back(link2);
    }
}

void AugmentationRenderer::convert_author_chain(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 3) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Author chain";
    if (insight.seed_labels.size() >= 3) {
        label = "Author chain: " + insight.seed_labels[0] + " -> " +
                insight.seed_labels[1] + " -> " + insight.seed_labels[2];
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    for (const auto& seed : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(seed);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

// ============== Category 3: Causal & Mechanistic Discovery ==============

void AugmentationRenderer::convert_causal_chain(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Causal chain: ";
    for (size_t i = 0; i < std::min(insight.seed_labels.size(), size_t(4)); ++i) {
        if (i > 0) label += " → ";
        label += insight.seed_labels[i];
    }
    if (insight.seed_labels.size() > 4) {
        label += " → ...";
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    // Link all chain nodes sequentially
    for (size_t i = 0; i < insight.seed_nodes.size(); ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = (i == 0) ? "source" : "member";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_intervention_point(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Intervention: " + (insight.seed_labels.empty() ? "?" : insight.seed_labels[0]);
    if (insight.score_breakdown.count("criticality")) {
        label += " (" + std::to_string(static_cast<int>(insight.score_breakdown.at("criticality") * 100)) + "% critical)";
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    // Link the intervention point
    AugmentationLink link;
    link.source = normalize_graph_ref(insight.seed_nodes[0]);
    link.target = rel_node.id;
    link.type = "source";
    link.is_new = true;
    data.links.push_back(link);

    // Link a few witness nodes
    int link_count = 0;
    for (const auto& wn : insight.witness_nodes) {
        std::string norm_wn = normalize_graph_ref(wn);
        if (norm_wn != normalize_graph_ref(insight.seed_nodes[0]) && link_count < 3) {
            AugmentationLink wlink;
            wlink.source = rel_node.id;
            wlink.target = norm_wn;
            wlink.type = "target";
            wlink.is_new = true;
            data.links.push_back(wlink);
            link_count++;
        }
    }
}

void AugmentationRenderer::convert_feedback_loop(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 2) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    std::string label = "Feedback loop: ";
    for (size_t i = 0; i < std::min(insight.seed_labels.size(), size_t(3)); ++i) {
        if (i > 0) label += " → ";
        label += insight.seed_labels[i];
    }
    label += " → ...";

    // Check if it's a reinforcing or balancing loop
    for (const auto& tag : insight.novelty_tags) {
        if (tag == "reinforcing_loop") {
            label += " (reinforcing)";
            break;
        } else if (tag == "balancing_loop") {
            label += " (balancing)";
            break;
        }
    }

    rel_node.label = label;
    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    // Link all loop nodes
    for (const auto& seed : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(seed);
        link.target = rel_node.id;
        link.type = "member";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_confounder(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.size() < 3) return;  // confounder, cause, effect

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Use short label (just the confounder entity)
    rel_node.label = insight.seed_labels.size() >= 1 ? insight.seed_labels[0] : "confounder";

    // V2: Store full description in properties
    if (insight.seed_labels.size() >= 3) {
        std::string full_desc = "Confounder: " + insight.seed_labels[0] + " confounds " +
                                insight.seed_labels[1] + " → " + insight.seed_labels[2];
        rel_node.properties["full_description"] = full_desc;
        rel_node.properties["insight_type"] = "confounder";
        rel_node.properties["confounded_cause"] = insight.seed_labels[1];
        rel_node.properties["confounded_effect"] = insight.seed_labels[2];
    }

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;

    data.nodes.push_back(rel_node);

    // Link confounder node (first seed)
    AugmentationLink link1;
    link1.source = normalize_graph_ref(insight.seed_nodes[0]);
    link1.target = rel_node.id;
    link1.type = "source";
    link1.is_new = true;
    data.links.push_back(link1);

    // Link cause and effect nodes
    if (insight.seed_nodes.size() >= 2) {
        AugmentationLink link2;
        link2.source = rel_node.id;
        link2.target = normalize_graph_ref(insight.seed_nodes[1]);
        link2.type = "confounded";
        link2.is_new = true;
        data.links.push_back(link2);
    }

    if (insight.seed_nodes.size() >= 3) {
        AugmentationLink link3;
        link3.source = rel_node.id;
        link3.target = normalize_graph_ref(insight.seed_nodes[2]);
        link3.type = "confounded";
        link3.is_new = true;
        data.links.push_back(link3);
    }
}

void AugmentationRenderer::export_augmented_html(
    const std::string& filename,
    const std::string& title,
    const AugmentationData& augmentation) {

    // Build JSON for augmentation
    nlohmann::json aug_json = augmentation.to_json();

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    // Compute statistics
    auto stats = graph_.compute_statistics();
    int max_degree = static_cast<int>(stats.max_node_degree);

    // Calculate optimal minimum degree for initial display
    // For smaller graphs, start with degree=1 to show everything
    // For larger graphs, find the lowest degree threshold that keeps edges manageable
    // Use 500 edges max for smooth initial rendering at 30+ FPS
    const int MAX_INITIAL_EDGES = 500;
    // Start with degree 5 as minimum for better initial performance
    int optimal_min_degree = 5;
    int num_edges = static_cast<int>(stats.num_edges);

    if (num_edges > MAX_INITIAL_EDGES) {
        // Build degree distribution (nodes grouped by degree)
        std::map<int, int> nodes_at_degree;
        for (const auto& node : graph_.get_all_nodes()) {
            nodes_at_degree[node.degree]++;
            if (node.degree > max_degree) max_degree = node.degree;
        }

        // Try increasing min_degree from 5 upward until we get <= MAX_INITIAL_EDGES
        // This finds a threshold that keeps the graph manageable with good FPS
        for (int test_degree = 5; test_degree <= max_degree; ++test_degree) {
            // Count nodes with degree >= test_degree
            int visible_nodes = 0;
            for (int d = test_degree; d <= max_degree; ++d) {
                if (nodes_at_degree.count(d)) {
                    visible_nodes += nodes_at_degree[d];
                }
            }

            // Estimate visible edges based on node fraction
            double node_fraction = static_cast<double>(visible_nodes) / stats.num_nodes;
            int estimated_edges = static_cast<int>(num_edges * node_fraction * node_fraction);

            if (estimated_edges <= MAX_INITIAL_EDGES) {
                optimal_min_degree = test_degree;
                break;
            }
            // Keep track of last tested degree in case we never go under threshold
            optimal_min_degree = test_degree;
        }
    }

    // Build graph data JSON.
    // Entity nodes only in nodes_json; hyperedges serialised separately as hyperedges_json.
    // JS reifyHyperedges() converts them to relation nodes + binary links at load time,
    // keeping the format consistent with the graph.json on-disk representation.
    nlohmann::json nodes_json = nlohmann::json::array();
    nlohmann::json hyperedges_json = nlohmann::json::array();

    for (const auto& node : graph_.get_all_nodes()) {
        nlohmann::json n;
        n["id"] = node.id;
        n["label"] = node.label;
        n["type"] = "entity";
        n["degree"] = node.degree;
        if (!node.properties.empty()) {
            n["properties"] = node.properties;
        }
        nodes_json.push_back(n);
    }

    for (const auto& edge : graph_.get_all_edges()) {
        nlohmann::json he;
        he["id"]        = edge.id;
        he["relation"]  = edge.relation;
        he["sources"]   = edge.sources;
        he["targets"]   = edge.targets;
        he["confidence"]= edge.confidence;
        if (!edge.source_document.empty()) he["source_document"] = edge.source_document;
        if (!edge.source_chunk_id.empty())  he["source_chunk_id"] = edge.source_chunk_id;
        if (!edge.properties.empty())       he["properties"] = edge.properties;
        hyperedges_json.push_back(he);
    }

    // Write HTML with all features from base viewer plus augmentation support
    file << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << title << R"( (Augmented)</title>
    <script src="https://d3js.org/d3.v7.min.js"></script>
    <script src="https://unpkg.com/three@0.160.0/build/three.min.js"></script>
    <script src="https://unpkg.com/three-spritetext@1.9.3/dist/three-spritetext.min.js"></script>
    <script src="https://unpkg.com/3d-force-graph@1.75.0/dist/3d-force-graph.min.js"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #eee;
            overflow: hidden;
        }
        #header {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            padding: 15px 25px;
            background: rgba(0, 0, 0, 0.4);
            backdrop-filter: blur(10px);
            z-index: 100;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        #header h1 { font-size: 1.5em; font-weight: 500; }
        #stats { font-size: 0.9em; opacity: 0.8; }
        #graph { width: 100vw; height: 100vh; }
        #controls {
            position: fixed;
            bottom: 20px;
            left: 20px;
            background: rgba(0, 0, 0, 0.6);
            padding: 15px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
            max-height: calc(100vh - 120px);
            overflow-y: auto;
        }
        #controls label { display: block; margin-bottom: 8px; font-size: 0.85em; }
        #controls input[type="range"] { width: 150px; }
        #tooltip {
            position: absolute;
            background: rgba(0, 0, 0, 0.85);
            padding: 12px 16px;
            border-radius: 8px;
            pointer-events: none;
            opacity: 0;
            transition: opacity 0.2s;
            max-width: 300px;
            font-size: 0.9em;
            z-index: 1000;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        #tooltip h3 { margin-bottom: 6px; color: #4fc3f7; }
        #tooltip .detail { color: #aaa; margin: 3px 0; }
        #legend {
            position: fixed;
            top: 80px;
            right: 20px;
            background: rgba(0, 0, 0, 0.6);
            padding: 15px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
            font-size: 0.85em;
        }
        #legend h3 { margin-bottom: 10px; font-size: 1em; }
        .legend-item { display: flex; align-items: center; margin: 6px 0; }
        .legend-color { width: 16px; height: 16px; border-radius: 3px; margin-right: 10px; }
        .kg-row { display: flex; gap: 8px; align-items: center; margin-top: 10px; }
        .kg-input {
            width: 220px;
            padding: 8px 10px;
            border-radius: 8px;
            border: 1px solid rgba(255,255,255,0.15);
            background: rgba(255,255,255,0.06);
            color: #eee;
            outline: none;
        }
        .kg-input::placeholder { color: rgba(255,255,255,0.55); }
        .kg-btn {
            padding: 8px 10px;
            border-radius: 8px;
            border: 1px solid rgba(255,255,255,0.18);
            background: rgba(255,255,255,0.08);
            color: #eee;
            cursor: pointer;
            user-select: none;
        }
        .kg-btn:hover { background: rgba(255,255,255,0.12); }
        .kg-btn:active { transform: translateY(1px); }
        #kgHud {
            position: fixed;
            top: 70px;
            left: 20px;
            padding: 10px 12px;
            border-radius: 10px;
            background: rgba(0,0,0,0.45);
            backdrop-filter: blur(10px);
            font-size: 12px;
            color: rgba(255,255,255,0.85);
            z-index: 120;
            pointer-events: none;
        }
        #kgOverlay {
            position: fixed;
            inset: 0;
            display: flex;
            align-items: center;
            justify-content: center;
            background: rgba(0,0,0,0.35);
            backdrop-filter: blur(6px);
            z-index: 2000;
            opacity: 0;
            pointer-events: none;
            transition: opacity 0.2s;
        }
        #kgOverlay.show { opacity: 1; pointer-events: all; }
        #kgOverlay .panel {
            background: rgba(0,0,0,0.75);
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 14px;
            padding: 16px 18px;
            width: min(520px, 92vw);
            color: #eee;
            box-shadow: 0 10px 30px rgba(0,0,0,0.35);
        }
        #kgOverlay .title { font-size: 14px; color: #4fc3f7; margin-bottom: 8px; }
        #kgOverlay .sub { font-size: 12px; color: rgba(255,255,255,0.75); }
        .aug-section {
            border-top: 1px solid #e040fb;
            margin-top: 12px;
            padding-top: 10px;
        }
        .aug-section > label:first-child { color: #e040fb; font-weight: 600; }
        #augDetails {
            margin-top: 10px;
            padding: 10px;
            background: rgba(224, 64, 251, 0.1);
            border-radius: 8px;
            font-size: 0.8em;
            display: none;
        }
        #augDetails.show { display: block; }
    </style>
</head>
<body>
    <div id="header">
        <h1>)" << title << R"( (Augmented)</h1>
        <div id="stats">
            Entities: )" << stats.num_nodes << R"( |
            Relations: )" << stats.num_edges << R"( |
            Augmentations: )" << augmentation.nodes.size() << R"(
        </div>
    </div>

    <div id="graph"></div>

    <div id="controls">
        <label>
            Min Entity Degree: <span id="degreeValue">)" << optimal_min_degree << R"(</span>
            <input type="range" id="minDegree" min="1" max=")" << std::max(20, max_degree) << R"(" step="1" value=")" << std::min(5, optimal_min_degree) << R"(">
        </label>
        <label>
            Link Strength:
            <input type="range" id="linkStrength" min="0.1" max="2" step="0.1" value="0.5">
        </label>
        <label>
            Node Repulsion:
            <input type="range" id="repulsion" min="100" max="1000" step="50" value="400">
        </label>
        <label>
            <input type="checkbox" id="showLabels"> Show Labels (high-degree only)
        </label>
        <div id="filterStats" style="margin-top: 10px; font-size: 0.8em; color: #aaa;">
            Showing: <span id="visibleNodes">0</span> nodes, <span id="visibleLinks">0</span> links
        </div>
    </div>

    <div id="legend">
        <h3>Legend</h3>
        <div class="legend-item">
            <div class="legend-color" style="background: #4fc3f7;"></div>
            <span>Entity Node</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #ff9800;"></div>
            <span>Relation (Hyperedge)</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #e040fb;"></div>
            <span>Augmentation (Insight)</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #4A90E2;"></div>
            <span>Class (General Concept)</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #7ED321;"></div>
            <span>Instance (Specific Example)</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #4fc3f7; width: 30px; height: 3px;"></div>
            <span>Source Link</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #ff9800; width: 30px; height: 3px;"></div>
            <span>Target Link</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #e040fb; width: 30px; height: 3px; border-style: dashed;"></div>
            <span>Augmentation Link</span>
        </div>
        <div class="legend-item">
            <div class="legend-color" style="background: #9E9E9E; width: 30px; height: 3px; border-style: dashed; border-width: 2px;"></div>
            <span>Hierarchy (instance_of, subclass_of, is_a)</span>
        </div>
    </div>

    <div id="tooltip"></div>

    <script>
        const data = {
            nodes: )" << nodes_json.dump() << R"(,
            hyperedges: )" << hyperedges_json.dump() << R"(
        };

        const augData = )" << aug_json.dump() << R"(;

        // Reify hyperedges: each entry in data.hyperedges becomes a relation node in data.nodes
        // and two binary links in data.links (entity→relation, relation→entity).
        // This mirrors the graph.json on-disk format (nodes + hyperedges with sources[]/targets[]).
        // augData.nodes/links are merged separately by mergeAugmentation() after this step.
        function reifyHyperedges(d) {
            const idToIdx = new Map();
            d.nodes.forEach((n, i) => idToIdx.set(n.id, i));
            d.links = [];
            let idx = d.nodes.length;
            for (const he of (d.hyperedges || [])) {
                const relNode = {
                    id: he.id, label: he.relation, type: 'relation',
                    confidence: he.confidence,
                    source_document: he.source_document || '',
                    source_chunk_id: he.source_chunk_id || '',
                    sources: he.sources, targets: he.targets
                };
                if (he.properties) relNode.properties = he.properties;
                d.nodes.push(relNode);
                const relIdx = idx++;
                idToIdx.set(he.id, relIdx);
                for (const src of (he.sources || [])) {
                    const si = idToIdx.get(src);
                    if (si !== undefined) d.links.push({ source: si, target: relIdx, type: 'source' });
                }
                for (const tgt of (he.targets || [])) {
                    const ti = idToIdx.get(tgt);
                    if (ti !== undefined) d.links.push({ source: relIdx, target: ti, type: 'target' });
                }
            }
        }
        reifyHyperedges(data);

        (() => {
            const graphDiv = document.getElementById('graph');
            const controls = document.getElementById('controls');

            if (typeof ForceGraph3D !== 'function' || typeof THREE === 'undefined') {
                alert('Missing WebGL libs. Serve via http://localhost.');
                return;
            }

            const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
            const ric = window.requestIdleCallback || ((cb) => setTimeout(() => cb({ timeRemaining: () => 0 }), 0));
            const rafThrottle = (fn) => {
                let scheduled = false;
                return (...args) => {
                    if (scheduled) return;
                    scheduled = true;
                    requestAnimationFrame(() => { scheduled = false; fn(...args); });
                };
            };

            // Overlay
            let overlay = document.getElementById('kgOverlay');
            if (!overlay) {
                overlay = document.createElement('div');
                overlay.id = 'kgOverlay';
                overlay.innerHTML = '<div class="panel"><div class="title">Working...</div><div class="sub">Building subgraph</div></div>';
                document.body.appendChild(overlay);
            }
            const overlayTitle = overlay.querySelector('.title');
            const overlaySub = overlay.querySelector('.sub');

            function showOverlay(title, sub) {
                if (overlayTitle) overlayTitle.textContent = title || 'Working...';
                if (overlaySub) overlaySub.textContent = sub || '';
                overlay.classList.add('show');
            }
            function hideOverlay() { overlay.classList.remove('show'); }

            let hud = document.getElementById('kgHud');
            if (!hud) {
                hud = document.createElement('div');
                hud.id = 'kgHud';
                hud.textContent = '3D: initializing...';
                document.body.appendChild(hud);
            }

            // Add extra controls - matching graph.html with augmentation additions
            controls.insertAdjacentHTML('beforeend', `
                <div class="kg-row">
                    <input class="kg-input" id="kgSearch" placeholder="Search node (press /)..." />
                    <button class="kg-btn" id="kgGo">Go</button>
                </div>
                <div class="kg-row">
                    <button class="kg-btn" id="kgExpand1">+ 1 hop</button>
                    <button class="kg-btn" id="kgExpand2">+ 2 hops</button>
                    <button class="kg-btn" id="kgReset">Reset</button>
                    <button class="kg-btn" id="kgCamReset">Reset camera</button>
                </div>
                <label style="margin-top:10px;">
                    <input type="checkbox" id="kgAutoFreeze" checked> Auto-freeze physics
                </label>
                <label>
                    <input type="checkbox" id="kgArrows"> Directional arrows
                </label>
                <label>
                    Max nodes: <span id="kgMaxNodesVal">2500</span>
                    <input type="range" id="kgMaxNodes" min="200" max="8000" step="100" value="2500">
                </label>
                <label>
                    Depth spread: <span id="kgDepthVal">120</span>
                    <input type="range" id="kgDepth" min="0" max="600" step="10" value="120">
                </label>

                <div style="border-top:1px solid #444; margin-top:12px; padding-top:10px;">
                    <label style="font-weight:600; color:#4fc3f7;">Clustering</label>
                    <label style="margin-top:6px;">
                        <input type="checkbox" id="kgClusterOn" checked> Cluster coloring
                    </label>
                    <label>
                        Clustering mode:
                        <select id="kgClusterMode" class="kg-input" style="width: 180px;">
                            <option value="topology" selected>Topology (Louvain)</option>
                            <option value="spatial">Spatial (DBSCAN)</option>
                        </select>
                    </label>
                    <label id="kgTopoResRow">
                        Resolution: <span id="kgTopoResVal">1.00</span>
                        <input type="range" id="kgTopoRes" min="0.20" max="3.00" step="0.05" value="1.00">
                    </label>
                    <label id="kgSpatialRadiusRow" style="display:none;">
                        Cluster radius: <span id="kgClusterRadiusVal">40</span>
                        <input type="range" id="kgClusterRadius" min="10" max="200" step="2" value="40">
                    </label>
                    <label id="kgSpatialMinRow" style="display:none;">
                        Min cluster size: <span id="kgMinClusterVal">5</span>
                        <input type="range" id="kgMinCluster" min="3" max="200" step="1" value="5">
                    </label>
                    <label>
                        Palette:
                        <select id="kgPalette" class="kg-input" style="width: 180px;">
                            <option value="classic" selected>Classic</option>
                            <option value="pastel">Pastel</option>
                            <option value="neon">Neon</option>
                            <option value="mono">Mono + accent</option>
                        </select>
                    </label>
                    <div class="kg-row">
                        <button class="kg-btn" id="kgRecluster">Cluster now</button>
                        <label style="margin:0;">
                            <input type="checkbox" id="kgAutoCluster" checked> Auto (on freeze)
                        </label>
                    </div>
                </div>

                <div class="aug-section" style="border-top:1px solid #444; margin-top:12px; padding-top:10px;">
                    <label style="font-weight:600; color:#e040fb;">Augmentation Layer</label>
                    <label style="margin-top:8px;">
                        <input type="checkbox" id="showAug"> Show Augmentations
                    </label>
                    <label>
                        Aug Opacity: <span id="augOpacityVal">1.0</span>
                        <input type="range" id="augOpacity" min="0.1" max="1" step="0.1" value="1.0">
                    </label>
                    <label>
                        <input type="checkbox" id="augOnlyMode"> Augmentation-only mode
                    </label>
                    <div class="kg-row">
                        <button class="kg-btn" id="focusAug">Focus Aug</button>
                    </div>
                    <div id="augDetails">
                        <strong>Insight Details</strong>
                        <div id="augInfo">Click an augmentation node.</div>
                    </div>
                </div>

                <div id="kgDetails" style="margin-top:10px; font-size:0.82em; color:#cfcfcf; line-height:1.35;"></div>
            `);

            const minDegreeEl = document.getElementById('minDegree');
            const degreeValueEl = document.getElementById('degreeValue');
            const linkStrengthEl = document.getElementById('linkStrength');
            const repulsionEl = document.getElementById('repulsion');
            const showLabelsEl = document.getElementById('showLabels');
            const searchEl = document.getElementById('kgSearch');
            const goBtn = document.getElementById('kgGo');
            const exp1Btn = document.getElementById('kgExpand1');
            const exp2Btn = document.getElementById('kgExpand2');
            const resetBtn = document.getElementById('kgReset');
            const camResetBtn = document.getElementById('kgCamReset');
            const autoFreezeEl = document.getElementById('kgAutoFreeze');
            const arrowsEl = document.getElementById('kgArrows');
            const maxNodesEl = document.getElementById('kgMaxNodes');
            const maxNodesValEl = document.getElementById('kgMaxNodesVal');
            const depthEl = document.getElementById('kgDepth');
            const depthValEl = document.getElementById('kgDepthVal');
            const detailsEl = document.getElementById('kgDetails');
            console.log('detailsEl found:', detailsEl);

            // Clustering controls
            const clusterOnEl = document.getElementById('kgClusterOn');
            const clusterModeEl = document.getElementById('kgClusterMode');
            const clusterRadiusEl = document.getElementById('kgClusterRadius');
            const clusterRadiusValEl = document.getElementById('kgClusterRadiusVal');
            const minClusterEl = document.getElementById('kgMinCluster');
            const minClusterValEl = document.getElementById('kgMinClusterVal');
            const topoResEl = document.getElementById('kgTopoRes');
            const topoResValEl = document.getElementById('kgTopoResVal');
            const paletteEl = document.getElementById('kgPalette');
            const reclusterBtn = document.getElementById('kgRecluster');
            const autoClusterEl = document.getElementById('kgAutoCluster');
            const topoResRow = document.getElementById('kgTopoResRow');
            const spatialRadiusRow = document.getElementById('kgSpatialRadiusRow');
            const spatialMinRow = document.getElementById('kgSpatialMinRow');

            // Augmentation controls
            const showAugEl = document.getElementById('showAug');
            const augOpacityEl = document.getElementById('augOpacity');
            const augOpacityValEl = document.getElementById('augOpacityVal');
            const augOnlyEl = document.getElementById('augOnlyMode');
            const focusAugBtn = document.getElementById('focusAug');
            const augDetailsEl = document.getElementById('augDetails');
            const augInfoEl = document.getElementById('augInfo');

            // ---- PHASE 1: Index base graph only (fast) ----
            const N = data.nodes.length;
            for (let i = 0; i < N; i++) {
                const n = data.nodes[i];
                n.gid = i;
                n.isAug = false;
            }

            // Build base edges index
            let edges = new Array(data.links.length);
            let incident = Array.from({ length: N }, () => []);
            for (let i = 0; i < data.links.length; i++) {
                const l = data.links[i];
                const s = +((typeof l.source === 'object') ? l.source.index : l.source);
                const t = +((typeof l.target === 'object') ? l.target.index : l.target);
                edges[i] = { s, t, type: l.type, isAug: false };
                incident[s].push(i);
                incident[t].push(i);
            }

            // Build entity degree list for filtering
            const entityByDegree = [];
            for (let i = 0; i < N; i++) {
                if (data.nodes[i].type === 'entity') entityByDegree.push(i);
            }
            entityByDegree.sort((a, b) => ((data.nodes[b].degree || 0) - (data.nodes[a].degree || 0)));

            let labelsLower = data.nodes.map(n => (n.label || n.id || '').toLowerCase());

            // Store augmentation data for later merge
            let augmentationMerged = false;
            const nodeIdToIdx = new Map();
            for (let i = 0; i < N; i++) nodeIdToIdx.set(data.nodes[i].id, i);

            // Function to merge augmentation data (called on demand)
            function mergeAugmentation() {
                if (augmentationMerged) return;
                augmentationMerged = true;
                console.log('Merging augmentation data...');

                const augNodeIdToIdx = new Map();
                for (const augNode of augData.nodes) {
                    const newIdx = data.nodes.length;
                    data.nodes.push({
                        gid: newIdx,
                        id: augNode.id,
                        label: augNode.label,
                        type: 'relation',
                        isAug: true,
                        insightId: augNode.insight_id,
                        confidence: augNode.confidence,
                        evidenceChunks: augNode.evidence_chunk_ids || [],
                        witnessEdges: augNode.witness_edges || []
                    });
                    augNodeIdToIdx.set(augNode.id, newIdx);
                }

                for (const augLink of augData.links) {
                    let srcIdx = nodeIdToIdx.get(augLink.source);
                    if (srcIdx === undefined) srcIdx = augNodeIdToIdx.get(augLink.source);
                    let tgtIdx = nodeIdToIdx.get(augLink.target);
                    if (tgtIdx === undefined) tgtIdx = augNodeIdToIdx.get(augLink.target);

                    if (srcIdx !== undefined && tgtIdx !== undefined) {
                        data.links.push({ source: srcIdx, target: tgtIdx, type: augLink.type, isAug: true });

                        // Populate sources/targets on aug relation nodes so the info panel can show them
                        const augNodeIdx = augNodeIdToIdx.get(augLink.source);
                        const augNodeIdx2 = augNodeIdToIdx.get(augLink.target);
                        if (augLink.type === 'source' && augNodeIdx2 !== undefined) {
                            // entity (augLink.source) -> aug node (augLink.target): entity is a source of this relation
                            const n = data.nodes[augNodeIdx2];
                            if (!n.sources) n.sources = [];
                            n.sources.push(augLink.source);
                        } else if (augLink.type === 'target' && augNodeIdx !== undefined) {
                            // aug node (augLink.source) -> entity (augLink.target): entity is a target of this relation
                            const n = data.nodes[augNodeIdx];
                            if (!n.targets) n.targets = [];
                            n.targets.push(augLink.target);
                        }
                    }
                }

                // Rebuild edges and incident arrays
                edges = new Array(data.links.length);
                incident = Array.from({ length: data.nodes.length }, () => []);
                for (let i = 0; i < data.links.length; i++) {
                    const l = data.links[i];
                    const s = +((typeof l.source === 'object') ? l.source.index : l.source);
                    const t = +((typeof l.target === 'object') ? l.target.index : l.target);
                    edges[i] = { s, t, type: l.type, isAug: !!l.isAug };
                    incident[s].push(i);
                    incident[t].push(i);
                }

                labelsLower = data.nodes.map(n => (n.label || n.id || '').toLowerCase());
                console.log(`After merge: ${data.nodes.length} nodes, ${data.links.length} links`);
            }

            // State - matching graph.html with augmentation additions
            const state = {
                minDegree: +minDegreeEl.value,
                linkStrength: +linkStrengthEl.value,
                repulsion: +repulsionEl.value,
                showLabels: !!showLabelsEl.checked,
                autoFreeze: true,
                showArrows: false,
                maxNodes: +maxNodesEl.value,
                depthSpread: +depthEl.value,
                selectedGid: null,
                visible: new Set(),
                subNodes: [],
                subLinks: [],
                subNodesByGid: new Map(),
                graph: null,
                fps: { t0: performance.now(), frames: 0, value: 0 },
                // Clustering state
                clusterOn: true,
                clusterMode: 'topology',
                clusterRadius: 40,
                minClusterSize: 5,
                topoResolution: 1.0,
                palette: 'classic',
                autoCluster: true,
                clusters: [],
                clusterLabels: [],
                // Augmentation state
                showAug: true,  // Start in augmentation-only mode
                augOpacity: 1.0,
                augOnlyMode: true,
                degreeDebounce: null
            };

            function applyDepth() {
                const spread = state.depthSpread;
                for (let i = 0; i < data.nodes.length; i++) {
                    const n = data.nodes[i];
                    const base = n.isAug ? 0.6 : (n.type === 'relation' ? 0.45 : 1.0);
                    n.z = (Math.random() - 0.5) * spread * base;
                    if (n.isAug) n.z += spread * 0.3; // Float augmentation slightly above
                }
            }

            function seedVisibleByDegree(minDegree, maxNodes) {
                const vis = new Set();

                // Always include augmentation nodes if enabled or in aug-only mode
                if (state.showAug || state.augOnlyMode) {
                    for (let i = 0; i < data.nodes.length; i++) {
                        if (data.nodes[i].isAug) vis.add(i);
                    }
                }

                if (state.augOnlyMode) {
                    // Only show augmentation and connected nodes
                    for (let i = 0; i < data.nodes.length; i++) {
                        if (data.nodes[i].isAug && incident[i]) {
                            vis.add(i);  // Ensure the aug node itself is added
                            for (const ei of incident[i]) {
                                const e = edges[ei];
                                if (!e) continue;
                                const other = (e.s === i) ? e.t : e.s;
                                vis.add(other);
                            }
                        }
                    }
                    return vis;
                }

                if (!state.augOnlyMode) {
                    for (let k = 0; k < entityByDegree.length && vis.size < maxNodes; k++) {
                        const idx = entityByDegree[k];
                        const n = data.nodes[idx];
                        if ((n.degree || 0) >= minDegree) vis.add(idx);
                        else break;
                    }
                }

                if (!state.augOnlyMode && vis.size < 50) {
                    for (let k = 0; k < Math.min(200, entityByDegree.length) && vis.size < Math.min(maxNodes, 200); k++) {
                        vis.add(entityByDegree[k]);
                    }
                }

                const relQueue = [];
                for (const idx of vis) {
                    if (!incident[idx]) continue;
                    for (const ei of incident[idx]) {
                        const e = edges[ei];
                        if (!e) continue;
                        const other = (e.s === idx) ? e.t : e.s;
                        if (data.nodes[other] && (data.nodes[other].type === 'relation' || data.nodes[other].isAug) && !vis.has(other)) {
                            relQueue.push(other);
                        }
                    }
                }
                for (let i = 0; i < relQueue.length && vis.size < maxNodes; i++) vis.add(relQueue[i]);

                if (state.selectedGid != null) vis.add(state.selectedGid);
                return vis;
            }

            function buildSubgraphFromVisible() {
                const subNodes = [];
                state.visible.forEach(i => {
                    if (data.nodes[i]) subNodes.push(data.nodes[i]);
                });

                const edgeIds = new Set();
                for (const i of state.visible) {
                    if (incident[i]) {
                        for (const ei of incident[i]) edgeIds.add(ei);
                    }
                }

                const subLinks = [];
                edgeIds.forEach(ei => {
                    const e = edges[ei];
                    if (!e) return;
                    if (state.visible.has(e.s) && state.visible.has(e.t)) {
                        // Filter augmentation links if showAug is off
                        if (e.isAug && !state.showAug) return;
                        subLinks.push(e);
                    }
                });

                state.subNodes = subNodes;
                state.subLinks = subLinks;

                const vn = document.getElementById('visibleNodes');
                const vl = document.getElementById('visibleLinks');
                if (vn) vn.textContent = String(subNodes.length);
                if (vl) vl.textContent = String(subLinks.length);
            }

            function rebuildSubgraphIndex() {
                state.subNodesByGid = new Map();
                for (const n of state.subNodes) state.subNodesByGid.set(n.gid, n);
            }

            function updateDetails(node) {
                if (!detailsEl) return;
                if (!node) {
                    detailsEl.innerHTML = '<span style="opacity:0.8;">Click a node to see details.</span>';
                    augDetailsEl.classList.remove('show');
                    return;
                }

                const lines = [];
                lines.push(`<div><b>${escapeHtml(node.label || node.id || 'Node')}</b></div>`);
                lines.push(`<div style="opacity:0.85;">type: <code>${escapeHtml(node.type || '')}</code></div>`);

                if (node.isAug) {
                    augDetailsEl.classList.add('show');
                    let augHtml = `<strong>${escapeHtml(node.label)}</strong><br>`;
                    if (node.insightId) augHtml += `Insight: ${escapeHtml(node.insightId)}<br>`;
                    if (typeof node.confidence === 'number') augHtml += `Confidence: ${node.confidence.toFixed(3)}<br>`;
                    if (node.evidenceChunks && node.evidenceChunks.length) {
                        augHtml += `Evidence: ${escapeHtml(node.evidenceChunks.slice(0, 3).join(', '))}`;
                        if (node.evidenceChunks.length > 3) augHtml += '...';
                    }
                    augInfoEl.innerHTML = augHtml;
                    // Also show degree and relation info for augmented nodes
                    if (typeof node.degree === 'number') lines.push(`<div style="opacity:0.85;">degree: <code>${node.degree}</code></div>`);
                    if (node.type === 'relation') {
                        if (node.sources && node.sources.length) lines.push(`<div style="opacity:0.85;">sources: <code>${escapeHtml(node.sources.slice(0, 8).join(', '))}${node.sources.length>8?' ...':''}</code></div>`);
                        if (node.targets && node.targets.length) lines.push(`<div style="opacity:0.85;">targets: <code>${escapeHtml(node.targets.slice(0, 8).join(', '))}${node.targets.length>8?' ...':''}</code></div>`);
                    }
                } else {
                    augDetailsEl.classList.remove('show');
                    if (typeof node.degree === 'number') lines.push(`<div style="opacity:0.85;">degree: <code>${node.degree}</code></div>`);
                    if (node.type === 'relation') {
                        if (node.sources && node.sources.length) lines.push(`<div style="opacity:0.85;">sources: <code>${escapeHtml(node.sources.slice(0, 8).join(', '))}${node.sources.length>8?' ...':''}</code></div>`);
                        if (node.targets && node.targets.length) lines.push(`<div style="opacity:0.85;">targets: <code>${escapeHtml(node.targets.slice(0, 8).join(', '))}${node.targets.length>8?' ...':''}</code></div>`);
                        if (typeof node.confidence === 'number') lines.push(`<div style="opacity:0.85;">confidence: <code>${node.confidence}</code></div>`);
                    }
                }
                detailsEl.innerHTML = lines.join('');
            }

            function escapeHtml(s) {
                return String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
            }

            function expandSelected(hops) {
                if (state.selectedGid == null) return;

                const maxNodes = state.maxNodes;
                const q = [state.selectedGid];
                const dist = new Map([[state.selectedGid, 0]]);

                while (q.length) {
                    const u = q.shift();
                    const d = dist.get(u) || 0;
                    if (d >= hops) continue;

                    for (const ei of incident[u]) {
                        const e = edges[ei];
                        const v = (e.s === u) ? e.t : e.s;
                        if (!dist.has(v)) {
                            dist.set(v, d + 1);
                            q.push(v);
                        }
                        if (state.visible.size < maxNodes) state.visible.add(v);
                    }
                    if (state.visible.size >= maxNodes) break;
                }

                rebuildAndRender('Expanding neighborhood...');
            }

            // Graph init
            graphDiv.innerHTML = '';

            const Graph = ForceGraph3D()(graphDiv)
                .nodeId('gid')
                .nodeLabel(n => (n.label || n.id || ''))
                .backgroundColor('rgba(0,0,0,0)')
                .showNavInfo(false)
                .enableNodeDrag(true)
                .linkSource('source')
                .linkTarget('target')
                .cooldownTime(2000)  // Prevent runaway physics simulation
                .warmupTicks(0);     // Don't pre-calculate - let rebuildAndRender handle it

            state.graph = Graph;

            // -------------------- CLUSTERING (from graph.html) --------------------
            const PALETTES = {
                classic: ['#4fc3f7','#ff9800','#ab47bc','#66bb6a','#ef5350','#ffa726','#26c6da','#8d6e63','#78909c','#d4e157'],
                pastel:  ['#a3d5ff','#ffd6a5','#bdb2ff','#caffbf','#ffadad','#fdffb6','#9bf6ff','#ffc6ff','#b8f2e6','#f1c0e8'],
                neon:    ['#00e5ff','#ffea00','#ff1744','#76ff03','#e040fb','#ff9100','#1de9b6','#f500ff','#00c853','#2979ff'],
                mono:    ['#4fc3f7','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc','#cfd8dc']
            };

            function getClusterColor(cid) {
                if (cid == null || cid < 0) return '#4fc3f7';
                const pal = PALETTES[state.palette] || PALETTES.classic;
                return pal[cid % pal.length];
            }

            function clearClusterLabels() {
                if (!state.clusterLabels?.length) return;
                const scene = Graph.scene();
                for (const s of state.clusterLabels) scene.remove(s);
                state.clusterLabels = [];
            }

            function drawClusterLabels() {
                clearClusterLabels();
                if (!state.clusterOn || typeof SpriteText === 'undefined') return;

                const scene = Graph.scene();
                for (const cl of state.clusters) {
                    const rep = state.subNodesByGid.get(cl.repGid);
                    if (!rep) continue;

                    const labelTxt = (rep.label || rep.id || `Cluster ${cl.id}`);
                    const sprite = new SpriteText(labelTxt);
                    sprite.fontFace = 'Arial Black';
                    sprite.textHeight = 10;
                    sprite.color = 'rgba(255,255,255,0.95)';
                    sprite.backgroundColor = 'rgba(0,0,0,0.50)';
                    sprite.padding = 6;
                    sprite.borderRadius = 10;
                    sprite.material.depthWrite = false;

                    sprite.position.set(rep.x || 0, (rep.y || 0) + 18, rep.z || 0);
                    scene.add(sprite);
                    state.clusterLabels.push(sprite);
                }
            }

            function chooseRepresentative(clusterNodeGids) {
                let cx = 0, cy = 0, cz = 0, cnt = 0;
                for (const gid of clusterNodeGids) {
                    const n = state.subNodesByGid.get(gid);
                    if (!n) continue;
                    cx += (n.x || 0); cy += (n.y || 0); cz += (n.z || 0);
                    cnt++;
                }
                if (!cnt) return clusterNodeGids[0] ?? null;
                cx /= cnt; cy /= cnt; cz /= cnt;

                let bestGid = null;
                let bestD = Infinity;
                for (const gid of clusterNodeGids) {
                    const n = state.subNodesByGid.get(gid);
                    if (!n || n.type !== 'entity') continue;
                    const dx = (n.x || 0) - cx, dy = (n.y || 0) - cy, dz = (n.z || 0) - cz;
                    const d2 = dx*dx + dy*dy + dz*dz;
                    if (d2 < bestD) { bestD = d2; bestGid = gid; }
                }
                if (bestGid != null) return bestGid;

                for (const gid of clusterNodeGids) {
                    const n = state.subNodesByGid.get(gid);
                    if (!n) continue;
                    const dx = (n.x || 0) - cx, dy = (n.y || 0) - cy, dz = (n.z || 0) - cz;
                    const d2 = dx*dx + dy*dy + dz*dz;
                    if (d2 < bestD) { bestD = d2; bestGid = gid; }
                }
                return bestGid;
            }

            // DBSCAN spatial clustering
            function clusterSpatialDBSCAN() {
                const eps = state.clusterRadius;
                const minPts = state.minClusterSize;

                const nodes = state.subNodes.filter(n => !n.isAug); // Exclude aug nodes
                const n = nodes.length;
                const labels = new Array(n).fill(0);
                let cid = 0;

                const eps2 = eps * eps;
                function regionQuery(i) {
                    const ni = nodes[i];
                    const xi = ni.x || 0, yi = ni.y || 0, zi = ni.z || 0;
                    const res = [];
                    for (let j = 0; j < n; j++) {
                        const nj = nodes[j];
                        const dx = (nj.x || 0) - xi, dy = (nj.y || 0) - yi, dz = (nj.z || 0) - zi;
                        if ((dx*dx + dy*dy + dz*dz) <= eps2) res.push(j);
                    }
                    return res;
                }

                function expandCluster(i, neighbors, cid) {
                    labels[i] = cid;
                    for (let k = 0; k < neighbors.length; k++) {
                        const j = neighbors[k];
                        if (labels[j] === -1) labels[j] = cid;
                        if (labels[j] !== 0) continue;
                        labels[j] = cid;
                        const n2 = regionQuery(j);
                        if (n2.length >= minPts) {
                            for (const x of n2) neighbors.push(x);
                        }
                    }
                }

                for (let i = 0; i < n; i++) {
                    if (labels[i] !== 0) continue;
                    const neighbors = regionQuery(i);
                    if (neighbors.length < minPts) {
                        labels[i] = -1;
                    } else {
                        cid++;
                        expandCluster(i, neighbors, cid);
                    }
                }

                for (let i = 0; i < n; i++) {
                    nodes[i].__cluster = labels[i] > 0 ? (labels[i] - 1) : -1;
                }
                // Set aug nodes to no cluster
                for (const node of state.subNodes) {
                    if (node.isAug) node.__cluster = -1;
                }

                const groups = new Map();
                for (const node of nodes) {
                    const c = node.__cluster ?? -1;
                    if (c < 0) continue;
                    if (!groups.has(c)) groups.set(c, []);
                    groups.get(c).push(node.gid);
                }

                state.clusters = [];
                for (const [id, gids] of groups.entries()) {
                    const rep = chooseRepresentative(gids);
                    state.clusters.push({ id, nodes: gids, repGid: rep });
                }
            }

            // Louvain-style topology clustering
            function clusterTopologyLouvain() {
                // Cluster only entity nodes; use ALL links (including aug) so that
                // augmentation-only mode still has connectivity to project through
                const nodes = state.subNodes.filter(n => n.type === 'entity' && !n.isAug);
                if (nodes.length === 0) { state.clusters = []; return; }

                const idxByGid = new Map();
                for (let i = 0; i < nodes.length; i++) idxByGid.set(nodes[i].gid, i);

                // Build entity co-occurrence by projecting through ALL relation intermediaries
                // (both base-graph relation nodes and aug nodes, which are type='relation')
                const adj = Array.from({ length: nodes.length }, () => new Map());

                // Collect entity lists incident to each relation/aug node
                const nodeByGid = new Map();
                for (const n of state.subNodes) nodeByGid.set(n.gid, n);

                const relToEntities = new Map();
                for (const e of state.subLinks) {  // include aug links
                    const src = nodeByGid.get(e.s);
                    const tgt = nodeByGid.get(e.t);
                    if (!src || !tgt) continue;
                    // entity -> relation (base or aug)
                    if (src.type === 'entity' && !src.isAug && tgt.type === 'relation') {
                        if (!relToEntities.has(e.t)) relToEntities.set(e.t, []);
                        relToEntities.get(e.t).push(e.s);
                    }
                    // relation (base or aug) -> entity
                    if (tgt.type === 'entity' && !tgt.isAug && src.type === 'relation') {
                        if (!relToEntities.has(e.s)) relToEntities.set(e.s, []);
                        relToEntities.get(e.s).push(e.t);
                    }
                }

                // Project: entities sharing a relation node get a co-occurrence edge
                for (const entityGids of relToEntities.values()) {
                    const eIdxs = entityGids.map(g => idxByGid.get(g)).filter(i => i !== undefined);
                    for (let p = 0; p < eIdxs.length; p++) {
                        for (let q = p + 1; q < eIdxs.length; q++) {
                            const a = eIdxs[p], b = eIdxs[q];
                            adj[a].set(b, (adj[a].get(b) || 0) + 1);
                            adj[b].set(a, (adj[b].get(a) || 0) + 1);
                        }
                    }
                }

                const resolution = state.topoResolution;
                const n = nodes.length;
                let community = new Array(n);
                for (let i = 0; i < n; i++) community[i] = i;

                let k = new Array(n).fill(0);
                let m2 = 0;
                for (let i = 0; i < n; i++) {
                    let sum = 0;
                    for (const w of adj[i].values()) sum += w;
                    k[i] = sum;
                    m2 += sum;
                }
                if (m2 === 0) {
                    for (const node of state.subNodes) node.__cluster = -1;
                    state.clusters = [];
                    return;
                }

                let tot = new Array(n).fill(0);
                for (let i = 0; i < n; i++) tot[community[i]] += k[i];

                function neighCommWeights(i) {
                    const map = new Map();
                    for (const [j, w] of adj[i].entries()) {
                        const cj = community[j];
                        map.set(cj, (map.get(cj) || 0) + w);
                    }
                    return map;
                }

                let changed = true;
                let iter = 0;
                while (changed && iter < 30) {
                    changed = false;
                    iter++;
                    for (let i = 0; i < n; i++) {
                        const ci = community[i];
                        const ki = k[i];
                        tot[ci] -= ki;
                        const ncw = neighCommWeights(i);
                        let bestC = ci;
                        let bestGain = 0;
                        for (const [c, wic] of ncw.entries()) {
                            const totC = tot[c];
                            const gain = wic - resolution * ki * totC / m2;
                            if (gain > bestGain) { bestGain = gain; bestC = c; }
                        }
                        community[i] = bestC;
                        tot[bestC] += ki;
                        if (bestC !== ci) changed = true;
                    }
                }

                const remap = new Map();
                let nextId = 0;
                for (let i = 0; i < n; i++) {
                    const c = community[i];
                    if (!remap.has(c)) remap.set(c, nextId++);
                    nodes[i].__cluster = remap.get(c);
                }
                // Aug nodes are never part of a cluster
                for (const node of state.subNodes) {
                    if (node.isAug) node.__cluster = -1;
                }

                const groups = new Map();
                for (const node of nodes) {
                    const c = node.__cluster ?? -1;
                    if (c < 0) continue;
                    if (!groups.has(c)) groups.set(c, []);
                    groups.get(c).push(node.gid);
                }

                state.clusters = [];
                for (const [id, gids] of groups.entries()) {
                    const rep = chooseRepresentative(gids);
                    state.clusters.push({ id, nodes: gids, repGid: rep });
                }
            }

            function runClustering() {
                if (state.clusterMode === 'spatial') {
                    clusterSpatialDBSCAN();
                } else {
                    clusterTopologyLouvain();
                }
                applyStyles();
                drawClusterLabels();
            }

            function applyStyles() {
                const showAug = state.showAug;
                const opacity = state.augOpacity;
                const clusteringEnabled = !!state.clusterOn;

                Graph
                    .nodeColor(n => {
                        // Augmentation nodes always purple
                        if (n.isAug) {
                            return showAug ? `rgba(224, 64, 251, ${opacity})` : 'rgba(224, 64, 251, 0.1)';
                        }
                        // If clustering enabled, use cluster color
                        if (clusteringEnabled) {
                            const cid = n.__cluster ?? -1;
                            return getClusterColor(cid);
                        }
                        // Ontology: Color by class/instance level
                        if (n.properties && n.properties.node_level) {
                            if (n.properties.node_level === 'class') {
                                return '#4A90E2';  // Blue for classes
                            } else if (n.properties.node_level === 'instance') {
                                return '#7ED321';  // Green for instances
                            }
                        }
                        // Default coloring
                        return n.type === 'relation' ? '#ff9800' : '#4fc3f7';
                    })
                    .nodeVal(n => {
                        if (n.isAug) return showAug ? 4 : 1;
                        return n.type === 'relation' ? 1.5 : Math.max(1.2, 1 + (n.degree || 0) * 0.06);
                    })
                    .linkColor(l => {
                        // Augmentation links always purple
                        if (l.isAug) {
                            return showAug ? `rgba(224, 64, 251, ${opacity * 0.7})` : 'rgba(224, 64, 251, 0.05)';
                        }
                        // If clustering enabled, color by cluster
                        if (clusteringEnabled) {
                            const a = state.subNodesByGid.get(l.source) || state.subNodesByGid.get(l.source?.gid) || null;
                            const b = state.subNodesByGid.get(l.target) || state.subNodesByGid.get(l.target?.gid) || null;
                            const ca = a ? (a.__cluster ?? -1) : -1;
                            const cb = b ? (b.__cluster ?? -1) : -1;

                            if (ca >= 0 && ca === cb) return getClusterColor(ca);
                            return 'rgba(255,255,255,0.10)';
                        }
                        return l.type === 'source' ? 'rgba(79,195,247,0.55)' : 'rgba(255,152,0,0.55)';
                    })
                    .linkWidth(l => {
                        if (l.isAug) return 1.5;
                        if (clusteringEnabled) {
                            const a = state.subNodesByGid.get(l.source) || state.subNodesByGid.get(l.source?.gid) || null;
                            const b = state.subNodesByGid.get(l.target) || state.subNodesByGid.get(l.target?.gid) || null;
                            const ca = a ? (a.__cluster ?? -1) : -1;
                            const cb = b ? (b.__cluster ?? -1) : -1;
                            return (ca >= 0 && ca === cb) ? 0.9 : 0.35;
                        }
                        return 0.6;
                    })
                    .linkDirectionalArrowLength(state.showArrows ? 3 : 0)
                    .linkDirectionalArrowRelPos(0.5);

                if (Graph.linkLineDash) {
                    Graph.linkLineDash(l => {
                        // Augmentation links: dashed
                        if (l.isAug) return [2, 2];
                        // Phase 2: Hierarchical relations (instance_of, subclass_of, is_a): dashed
                        if (l.properties && l.properties.relation_type === 'hierarchical') {
                            return [4, 4];  // Slightly longer dashes for hierarchy
                        }
                        return null;  // Solid line for regular relations
                    });
                }

                Graph.refresh();
            }

            // Apply 3D text labels only to important/selected nodes (prevents performance freeze)
            function applyLabels() {
                state.showLabels = !!showLabelsEl.checked;
                if (typeof SpriteText === 'undefined') return;

                Graph.nodeThreeObject(node => {
                    if (!state.showLabels) return null;

                    const isSelected = (state.selectedGid != null && node.gid === state.selectedGid);
                    // Only show labels for high-degree entities or augmentation nodes
                    const important = (node.type === 'entity' && (node.degree || 0) >= Math.max(40, state.minDegree));
                    const isAugNode = node.isAug;
                    if (!isSelected && !important && !isAugNode) return null;

                    const sprite = new SpriteText(node.label || node.id || '');
                    sprite.textHeight = isSelected ? 6 : (isAugNode ? 5 : 4);
                    sprite.color = isAugNode ? 'rgba(224, 64, 251, 0.95)' : 'rgba(255,255,255,0.9)';
                    sprite.backgroundColor = 'rgba(0,0,0,0.35)';
                    sprite.padding = 2;
                    sprite.borderRadius = 6;
                    sprite.material.depthWrite = false;
                    sprite.position.y = 8;
                    return sprite;
                }).nodeThreeObjectExtend(true);

                Graph.refresh();
            }

            // Apply physics forces (matching graph.html)
            function applyForces() {
                if (Graph.d3Force('charge')) Graph.d3Force('charge').strength(-state.repulsion);
                if (Graph.d3Force('link')) Graph.d3Force('link').strength(state.linkStrength);
            }

            // Apply freeze settings
            function applyFreeze() {
                if (state.autoFreeze) {
                    Graph.cooldownTime(2000);
                } else {
                    Graph.cooldownTime(Infinity);
                }
            }

            function rebuildAndRender(msg) {
                showOverlay(msg || 'Updating...', 'Filtering & rebuilding visible subgraph');

                ric(() => {
                    buildSubgraphFromVisible();
                    rebuildSubgraphIndex();

                    const gdata = {
                        nodes: state.subNodes,
                        links: state.subLinks.map(e => ({ source: e.s, target: e.t, type: e.type, isAug: e.isAug }))
                    };

                    Graph.graphData(gdata);
                    applyForces();
                    applyFreeze();
                    applyStyles();
                    applyLabels();

                    Graph.d3ReheatSimulation();

                    // Cluster after a short delay for layout to settle
                    if (state.autoCluster && state.clusterOn) {
                        setTimeout(() => runClustering(), 1400);
                    }

                    hideOverlay();
                    updateHud();
                });
            }

            function rebuildFromDegree() {
                showOverlay('Seeding visible subgraph...', 'Using degree threshold + caps');
                ric(() => {
                    state.visible = seedVisibleByDegree(state.minDegree, state.maxNodes);
                    if (state.selectedGid != null) state.visible.add(state.selectedGid);
                    rebuildAndRender('Rendering subgraph...');
                });
            }

            function updateHud() {
                const now = performance.now();
                state.fps.frames++;
                const dt = now - state.fps.t0;
                if (dt >= 800) {
                    state.fps.value = Math.round((state.fps.frames * 1000) / dt);
                    state.fps.frames = 0;
                    state.fps.t0 = now;
                }
                const augCount = state.subNodes.filter(n => n.isAug).length;
                hud.textContent = `3D | ${state.subNodes.length} nodes | ${state.subLinks.length} links | ${augCount} aug | ${state.fps.value} fps`;
            }

            // Camera reset function
            function resetCamera() {
                Graph.cameraPosition({ x: 0, y: 0, z: 420 }, { x: 0, y: 0, z: 0 }, 700);
            }

            // Event handlers
            Graph.onNodeClick(node => {
                console.log('Node clicked:', node);
                state.selectedGid = node ? node.gid : null;
                console.log('Calling updateDetails with node:', node ? node.label : null);
                updateDetails(node);
            });

            // Debounced degree filter - uses rebuildFromDegree to properly seed visible nodes
            minDegreeEl.addEventListener('input', () => {
                state.minDegree = +minDegreeEl.value;
                degreeValueEl.textContent = String(state.minDegree);
                if (state.degreeDebounce) clearTimeout(state.degreeDebounce);
                state.degreeDebounce = setTimeout(() => rebuildFromDegree(), 150);
            });

            linkStrengthEl.addEventListener('input', () => {
                state.linkStrength = +linkStrengthEl.value;
                applyForces();
                Graph.d3ReheatSimulation();
            });

            repulsionEl.addEventListener('input', () => {
                state.repulsion = +repulsionEl.value;
                applyForces();
                Graph.d3ReheatSimulation();
            });

            // Debounced max nodes control
            maxNodesEl.addEventListener('input', () => {
                state.maxNodes = +maxNodesEl.value;
                maxNodesValEl.textContent = String(state.maxNodes);
                if (state.maxNodesDebounce) clearTimeout(state.maxNodesDebounce);
                state.maxNodesDebounce = setTimeout(() => rebuildFromDegree(), 200);
            });

            depthEl.addEventListener('input', () => {
                state.depthSpread = +depthEl.value;
                depthValEl.textContent = String(state.depthSpread);
                applyDepth();
                Graph.refresh();
            });

            goBtn.addEventListener('click', () => {
                const q = (searchEl.value || '').trim().toLowerCase();
                if (!q) return;
                let idx = labelsLower.indexOf(q);
                if (idx === -1) {
                    for (let i = 0; i < labelsLower.length; i++) {
                        if (labelsLower[i].includes(q)) { idx = i; break; }
                    }
                }
                if (idx === -1) {
                    detailsEl.innerHTML = `<div style="color:#ffb74d;">No match for "${escapeHtml(q)}".</div>`;
                    return;
                }
                state.selectedGid = idx;
                state.visible.add(idx);
                rebuildAndRender('Searching...');
                updateDetails(data.nodes[idx]);
            });

            searchEl.addEventListener('keydown', e => { if (e.key === 'Enter') goBtn.click(); });

            exp1Btn.addEventListener('click', () => expandSelected(1));
            exp2Btn.addEventListener('click', () => expandSelected(2));
            resetBtn.addEventListener('click', () => {
                state.selectedGid = null;
                updateDetails(null);
                rebuildAndRender('Resetting view...');
            });
            camResetBtn.addEventListener('click', resetCamera);

            showAugEl.addEventListener('change', () => {
                state.showAug = showAugEl.checked;
                if (state.showAug) mergeAugmentation();  // Merge aug data on first enable
                rebuildAndRender('Toggling augmentation...');
            });

            augOpacityEl.addEventListener('input', () => {
                state.augOpacity = +augOpacityEl.value;
                augOpacityValEl.textContent = state.augOpacity.toFixed(1);
                applyStyles();
            });

            augOnlyEl.addEventListener('change', () => {
                state.augOnlyMode = augOnlyEl.checked;
                if (state.augOnlyMode) {
                    mergeAugmentation();  // Merge aug data if entering aug-only mode
                    // Also enable showAug when entering aug-only mode
                    state.showAug = true;
                    showAugEl.checked = true;
                } else {
                    state.showAug = false;
                    if (showAugEl) showAugEl.checked = false;
                }
                // Reseed visible nodes so we return to the same base-graph view as graph.html.
                rebuildFromDegree();
            });

            focusAugBtn.addEventListener('click', () => {
                const augNodes = state.subNodes.filter(n => n.isAug);
                if (augNodes.length === 0) return;

                let cx = 0, cy = 0, cz = 0;
                for (const n of augNodes) {
                    cx += n.x || 0;
                    cy += n.y || 0;
                    cz += n.z || 0;
                }
                cx /= augNodes.length;
                cy /= augNodes.length;
                cz /= augNodes.length;

                Graph.cameraPosition({ x: cx, y: cy, z: cz + 300 }, { x: cx, y: cy, z: cz }, 1000);
            });

            // Directional arrows toggle
            arrowsEl.addEventListener('change', () => {
                state.showArrows = !!arrowsEl.checked;
                applyStyles();
            });

            // Show labels toggle (only renders labels for high-degree/selected nodes)
            showLabelsEl.addEventListener('change', () => {
                applyLabels();
            });

            // Clustering event handlers
            clusterOnEl.addEventListener('change', () => {
                state.clusterOn = !!clusterOnEl.checked;
                if (state.clusterOn) runClustering();
                else {
                    clearClusterLabels();
                    applyStyles();
                }
            });

            clusterModeEl.addEventListener('change', () => {
                state.clusterMode = clusterModeEl.value;
                if (state.clusterMode === 'spatial') {
                    topoResRow.style.display = 'none';
                    spatialRadiusRow.style.display = '';
                    spatialMinRow.style.display = '';
                } else {
                    topoResRow.style.display = '';
                    spatialRadiusRow.style.display = 'none';
                    spatialMinRow.style.display = 'none';
                }
                if (state.clusterOn) runClustering();
            });

            clusterRadiusEl.addEventListener('input', () => {
                state.clusterRadius = +clusterRadiusEl.value;
                clusterRadiusValEl.textContent = String(state.clusterRadius);
            });
            clusterRadiusEl.addEventListener('change', () => { if (state.clusterOn) runClustering(); });

            minClusterEl.addEventListener('input', () => {
                state.minClusterSize = +minClusterEl.value;
                minClusterValEl.textContent = String(state.minClusterSize);
            });
            minClusterEl.addEventListener('change', () => { if (state.clusterOn) runClustering(); });

            topoResEl.addEventListener('input', () => {
                state.topoResolution = +topoResEl.value;
                topoResValEl.textContent = state.topoResolution.toFixed(2);
            });
            topoResEl.addEventListener('change', () => { if (state.clusterOn) runClustering(); });

            paletteEl.addEventListener('change', () => {
                state.palette = paletteEl.value;
                applyStyles();
                drawClusterLabels();
            });

            reclusterBtn.addEventListener('click', () => runClustering());

            autoClusterEl.addEventListener('change', () => {
                state.autoCluster = !!autoClusterEl.checked;
            });

            // Tooltip hover
            Graph.onNodeHover(rafThrottle((node) => {
                const tt = document.getElementById('tooltip');
                if (!tt) return;
                if (!node) { tt.style.opacity = 0; return; }
                tt.style.opacity = 1;
                let info = `<strong>${escapeHtml(node.label || node.id || '')}</strong><br/>`;
                info += `<span style="opacity:0.85;">${escapeHtml(node.type || '')}`;
                if (typeof node.degree === 'number') info += ` deg ${node.degree}`;
                if (node.isAug && typeof node.confidence === 'number') info += ` conf ${node.confidence.toFixed(2)}`;
                info += '</span>';
                // Show base classes for instance nodes
                if (node.properties && node.properties.node_level === 'instance' && node.properties.base_classes) {
                    info += `<br/><span style="opacity:0.75; color:#7ED321;">⬆ ${escapeHtml(node.properties.base_classes)}</span>`;
                }
                // V2: Show full description for augmented nodes with properties
                if (node.isAug && node.properties && node.properties.full_description) {
                    info += `<br/><span style="opacity:0.7; color:#90caf9; font-size:0.9em;">${escapeHtml(node.properties.full_description)}</span>`;
                }
                tt.innerHTML = info;
            }));

            // Auto-freeze handler
            autoFreezeEl.addEventListener('change', () => {
                state.autoFreeze = !!autoFreezeEl.checked;
                if (state.autoFreeze) {
                    Graph.cooldownTime(2000);
                } else {
                    Graph.cooldownTime(Infinity);
                }
                Graph.d3ReheatSimulation();
            });

            // FPS counter
            if (Graph.onRenderFramePost) {
                Graph.onRenderFramePost(updateHud);
            } else {
                setInterval(updateHud, 250);
            }

            // Window resize
            window.addEventListener('resize', () => {
                Graph.width(window.innerWidth);
                Graph.height(window.innerHeight);
            });

            // ---- Boot sequence (matching graph.html exactly) ----
            // Start in augmentation-only mode, but allow user to toggle back
            state.showAug = true;
            state.augOnlyMode = true;
            if (showAugEl) showAugEl.checked = true;
            if (augOnlyEl) augOnlyEl.checked = true;
            mergeAugmentation();

            // Function to update cluster mode UI (matching graph.html)
            function updateClusterModeUI() {
                if (state.clusterMode === 'spatial') {
                    if (topoResRow) topoResRow.style.display = 'none';
                    if (spatialRadiusRow) spatialRadiusRow.style.display = '';
                    if (spatialMinRow) spatialMinRow.style.display = '';
                } else {
                    if (topoResRow) topoResRow.style.display = '';
                    if (spatialRadiusRow) spatialRadiusRow.style.display = 'none';
                    if (spatialMinRow) spatialMinRow.style.display = 'none';
                }
            }

            // Initialize UI (matching graph.html boot sequence exactly)
            updateDetails(null);
            if (maxNodesValEl) maxNodesValEl.textContent = String(state.maxNodes);
            if (depthValEl) depthValEl.textContent = String(state.depthSpread);
            if (clusterRadiusValEl) clusterRadiusValEl.textContent = String(state.clusterRadius);
            if (minClusterValEl) minClusterValEl.textContent = String(state.minClusterSize);
            if (topoResValEl) topoResValEl.textContent = state.topoResolution.toFixed(2);
            updateClusterModeUI();

            applyDepth();
            resetCamera();
            console.log(`Boot: minDegree=${state.minDegree}, maxNodes=${state.maxNodes}`);
            state.visible = seedVisibleByDegree(state.minDegree, state.maxNodes);
            console.log(`After seedVisibleByDegree: ${state.visible.size} visible nodes`);
            buildSubgraphFromVisible();
            console.log(`After buildSubgraph: ${state.subNodes.length} subNodes, ${state.subLinks.length} subLinks`);

            hideOverlay();
            rebuildAndRender('Rendering initial 3D view...');
        })();
    </script>
</body>
</html>
)";

    file.close();
}

// V2: Label simplification helper
std::string AugmentationRenderer::simplify_label(const std::string& label) {
    if (label.empty()) return label;

    std::string simplified = label;

    // 1. Trim whitespace
    size_t start = simplified.find_first_not_of(" \t\n\r");
    size_t end = simplified.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    simplified = simplified.substr(start, end - start + 1);

    // 2. Remove common prefix patterns (should already be in properties, but just in case)
    std::vector<std::string> prefixes = {
        "Bridge: ", "Confounder: ", "Community: ", "Centrality: ",
        "Query: ", "k-Core: ", "k-Truss: ", "Motif: ",
        "Completion: ", "Substitute: ", "Contradiction: ", "Resolve: ",
        "Analogy: ", "Argument: ", "Text Similar: ", "Core: ", "Periphery: ",
        "Relation Induction", "Counterfactual", "Uncertainty", "Method/Outcome: "
    };

    for (const auto& prefix : prefixes) {
        if (simplified.find(prefix) == 0) {
            simplified = simplified.substr(prefix.length());
            break;
        }
    }

    // 3. Trim again after prefix removal
    start = simplified.find_first_not_of(" \t\n\r");
    end = simplified.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        simplified = simplified.substr(start, end - start + 1);
    }

    // 4. Remove special characters that might clutter labels
    // Keep only: alphanumeric, spaces, hyphens, underscores
    std::string cleaned;
    for (char c : simplified) {
        if (std::isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.') {
            cleaned += c;
        }
    }
    simplified = cleaned;

    // 5. Collapse multiple spaces to single space
    std::string result;
    bool prev_space = false;
    for (char c : simplified) {
        if (c == ' ') {
            if (!prev_space) {
                result += c;
                prev_space = true;
            }
        } else {
            result += c;
            prev_space = false;
        }
    }

    // 6. Truncate if still too long (>40 chars)
    if (result.length() > 40) {
        result = result.substr(0, 37) + "...";
    }

    return result;
}

void AugmentationRenderer::convert_cross_community_bridge_map(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Short label from the bridge entity
    rel_node.label = insight.seed_labels.empty() ? "community bridge" : insight.seed_labels[0];

    // V2: Full description from insight description field
    std::string desc = insight.description.empty()
        ? "Community Bridge: " + (insight.seed_labels.empty() ? "bridge entity" : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "cross_community_bridge_map";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (const auto& seed : insight.seed_nodes) {
        AugmentationLink link;
        link.source = normalize_graph_ref(seed);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_multi_resolution_community(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Short label from first seed
    rel_node.label = insight.seed_labels.empty() ? "multi-res community" : insight.seed_labels[0];

    // V2: Full description from insight description field
    std::string desc = insight.description.empty()
        ? "Multi-Resolution Community: " + (insight.seed_labels.empty() ? "community" : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "multi_resolution_community";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 10; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_meta_pattern(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();

    // V2: Short label from first seed
    rel_node.label = insight.seed_labels.empty() ? "meta-pattern" : insight.seed_labels[0];

    // V2: Full description from insight description field
    std::string desc = insight.description.empty()
        ? "Meta-Pattern: " + (insight.seed_labels.empty() ? "structural template" : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "meta_pattern";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 10; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

// =============================================================================
// Epistemic Discovery converters
// =============================================================================

void AugmentationRenderer::convert_evidence_debt(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "evidence debt" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Evidence debt: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "evidence_debt";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_consensus_frontier(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "consensus frontier" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Consensus frontier: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "consensus_frontier";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_boundary_condition_map(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "boundary condition" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Boundary condition: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "boundary_condition_map";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_failure_mode_topology(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "failure mode" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Failure mode: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "failure_mode_topology";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_benchmark_dependence(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "benchmark dependence" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Benchmark dependence: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "benchmark_dependence";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_concept_drift(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "concept drift" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Concept drift: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "concept_drift";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_premise_bottleneck(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "premise bottleneck" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Premise bottleneck: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "premise_bottleneck";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

void AugmentationRenderer::convert_translation_gap(const Insight& insight, AugmentationData& data) {
    if (insight.seed_nodes.empty()) return;

    AugmentationNode rel_node;
    rel_node.id = make_aug_node_id();
    rel_node.label = insight.seed_labels.empty() ? "translation gap" : insight.seed_labels[0];

    std::string desc = insight.description.empty()
        ? "Translation gap: " + (insight.seed_labels.empty() ? insight.seed_nodes[0] : insight.seed_labels[0])
        : insight.description;
    rel_node.properties["full_description"] = desc;
    rel_node.properties["insight_type"] = "translation_gap";

    rel_node.type = "relation";
    rel_node.is_new = true;
    rel_node.insight_id = insight.insight_id;
    rel_node.confidence = insight.score;
    rel_node.properties["confidence"] = std::to_string(insight.score);
    rel_node.evidence_chunk_ids = insight.evidence_chunk_ids;
    rel_node.witness_edges = insight.witness_edges;
    data.nodes.push_back(rel_node);

    for (size_t i = 0; i < insight.seed_nodes.size() && i < 5; ++i) {
        AugmentationLink link;
        link.source = normalize_graph_ref(insight.seed_nodes[i]);
        link.target = rel_node.id;
        link.type = "source";
        link.is_new = true;
        data.links.push_back(link);
    }
}

} // namespace kg

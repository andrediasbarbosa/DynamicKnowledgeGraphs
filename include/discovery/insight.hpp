#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <stdexcept>

namespace kg {

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
    CONSTRAINED_RULE,
    DIFFUSION,
    SURPRISE,
    COMMUNITY_LINK,
    PATH_RANK,
    HYPOTHESIS,
    RULE,
    EMBEDDING_LINK,  // TransE/RotatE/ComplEx embedding-based link prediction
    AUTHOR_CHAIN
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
        case InsightType::CONSTRAINED_RULE: return "constrained_rule";
        case InsightType::DIFFUSION: return "diffusion";
        case InsightType::SURPRISE: return "surprise";
        case InsightType::COMMUNITY_LINK: return "community_link";
        case InsightType::PATH_RANK: return "path_rank";
        case InsightType::HYPOTHESIS: return "hypothesis";
        case InsightType::RULE: return "rule";
        case InsightType::EMBEDDING_LINK: return "embedding_link";
        case InsightType::AUTHOR_CHAIN: return "author_chain";
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
    if (s == "constrained_rule" || s == "constrained-rule" || s == "rule_constrained") return InsightType::CONSTRAINED_RULE;
    if (s == "diffusion") return InsightType::DIFFUSION;
    if (s == "surprise") return InsightType::SURPRISE;
    if (s == "community_link" || s == "community") return InsightType::COMMUNITY_LINK;
    if (s == "path_rank" || s == "pathrank") return InsightType::PATH_RANK;
    if (s == "hypothesis" || s == "hypotheses") return InsightType::HYPOTHESIS;
    if (s == "rule") return InsightType::RULE;
    if (s == "embedding_link" || s == "embedding" || s == "transe") return InsightType::EMBEDDING_LINK;
    if (s == "author_chain" || s == "authorchain" || s == "author-chains") return InsightType::AUTHOR_CHAIN;
    return InsightType::BRIDGE; // default
}

struct Insight {
    std::string insight_id;             // "run_001:bridge:000187"
    InsightType type;
    std::vector<std::string> seed_nodes;   // Primary node IDs involved
    std::vector<std::string> witness_edges; // Edge IDs that support this insight
    std::vector<std::string> witness_nodes; // All node IDs in the witness subgraph
    std::vector<std::string> evidence_chunk_ids; // Source chunk IDs for provenance
    double score = 0.0;
    std::map<std::string, double> score_breakdown; // "support", "novelty", "specificity"
    std::vector<std::string> novelty_tags;
    nlohmann::json llm = nullptr;       // LLM synthesis output (null until synthesized)

    // Additional fields for specific insight types
    std::string description;            // Human-readable description
    std::vector<std::string> seed_labels; // Labels for seed nodes (for display)

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["insight_id"] = insight_id;
        j["type"] = insight_type_to_string(type);
        j["seed_nodes"] = seed_nodes;
        j["witness_edges"] = witness_edges;
        j["witness_nodes"] = witness_nodes;
        j["evidence_chunk_ids"] = evidence_chunk_ids;
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
        ins.seed_nodes = j.value("seed_nodes", std::vector<std::string>{});
        ins.witness_edges = j.value("witness_edges", std::vector<std::string>{});
        ins.witness_nodes = j.value("witness_nodes", std::vector<std::string>{});
        ins.evidence_chunk_ids = j.value("evidence_chunk_ids", std::vector<std::string>{});
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

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

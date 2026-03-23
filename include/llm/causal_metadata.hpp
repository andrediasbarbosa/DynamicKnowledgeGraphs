#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace kg {

/**
 * @brief Type of causal relationship
 */
enum class CausalRelationType {
    DIRECT_CAUSE,        ///< A directly causes B
    NECESSARY,           ///< A is necessary for B (B requires A)
    SUFFICIENT,          ///< A is sufficient for B (A guarantees B)
    CONTRIBUTING,        ///< A contributes to B (among other factors)
    PREVENTING,          ///< A prevents/inhibits B
    ENABLING,            ///< A enables B (makes B possible)
    MECHANISM            ///< A causes B via specific mechanism M
};

/**
 * @brief Strength of causal relationship
 */
enum class CausalStrength {
    WEAK,                ///< Weak causal link
    MODERATE,            ///< Moderate causal link
    STRONG,              ///< Strong causal link
    DETERMINISTIC        ///< Deterministic/invariant causation
};

/**
 * @brief Temporal aspect of causation
 */
enum class Temporality {
    IMMEDIATE,           ///< Effect occurs immediately
    SHORT_TERM,          ///< Effect occurs within hours/days
    LONG_TERM,           ///< Effect occurs over months/years
    DELAYED              ///< Effect is significantly delayed
};

/**
 * @brief Rich metadata for causal relationships
 *
 * Captures nuanced causal information including mechanism,
 * conditions, strength, and temporal aspects.
 */
struct CausalMetadata {
    // Core causal properties
    CausalRelationType type = CausalRelationType::DIRECT_CAUSE;
    CausalStrength strength = CausalStrength::MODERATE;
    Temporality temporality = Temporality::IMMEDIATE;

    // Mechanism information
    std::string mechanism_description;           ///< How A causes B
    std::string mechanism_type;                  ///< "physical", "chemical", "social", etc.
    std::vector<std::string> mechanism_chain;    ///< A → M1 → M2 → B

    // Logical conditions
    std::vector<std::string> necessary_conditions;   ///< Required for causation
    std::vector<std::string> sufficient_conditions;  ///< Guarantee causation
    std::vector<std::string> contributing_factors;   ///< Other contributing causes
    std::vector<std::string> inhibitors;             ///< Factors that prevent/weaken

    // Evidence and confidence
    double confidence = 0.5;                     ///< Confidence in causal claim (0-1)
    std::vector<std::string> evidence_sources;   ///< Text snippets supporting claim
    int citation_count = 0;                      ///< Number of supporting citations

    // Temporal context
    std::string temporal_context;                ///< "within hours", "over decades", etc.
    bool bidirectional = false;                  ///< Can B also cause A?

    // Context and scope
    std::string context;                         ///< Conditions under which causation holds
    std::vector<std::string> scope_limitations;  ///< Boundary conditions

    /**
     * @brief Serialize to JSON
     */
    nlohmann::json to_json() const;

    /**
     * @brief Deserialize from JSON
     */
    static CausalMetadata from_json(const nlohmann::json& j);

    /**
     * @brief Get numeric strength score (0-1)
     */
    double get_strength_score() const;

    /**
     * @brief Check if this is a strong causal relationship
     */
    bool is_strong() const { return strength >= CausalStrength::STRONG; }

    /**
     * @brief Check if mechanism is specified
     */
    bool has_mechanism() const { return !mechanism_description.empty(); }

    /**
     * @brief Get human-readable causal type string
     */
    std::string get_type_string() const;

    /**
     * @brief Get human-readable strength string
     */
    std::string get_strength_string() const;

    /**
     * @brief Get human-readable temporality string
     */
    std::string get_temporality_string() const;
};

/**
 * @brief Helper functions for causal type conversion
 */
namespace causal {
    std::string to_string(CausalRelationType type);
    CausalRelationType type_from_string(const std::string& str);

    std::string to_string(CausalStrength strength);
    CausalStrength strength_from_string(const std::string& str);

    std::string to_string(Temporality temp);
    Temporality temporality_from_string(const std::string& str);
}

} // namespace kg

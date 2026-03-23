#include "llm/causal_metadata.hpp"
#include <stdexcept>
#include <algorithm>

namespace kg {

// ============================================================================
// CausalMetadata Implementation
// ============================================================================

nlohmann::json CausalMetadata::to_json() const {
    nlohmann::json j;

    j["type"] = causal::to_string(type);
    j["strength"] = causal::to_string(strength);
    j["temporality"] = causal::to_string(temporality);

    if (!mechanism_description.empty()) {
        j["mechanism_description"] = mechanism_description;
    }
    if (!mechanism_type.empty()) {
        j["mechanism_type"] = mechanism_type;
    }
    if (!mechanism_chain.empty()) {
        j["mechanism_chain"] = mechanism_chain;
    }

    if (!necessary_conditions.empty()) {
        j["necessary_conditions"] = necessary_conditions;
    }
    if (!sufficient_conditions.empty()) {
        j["sufficient_conditions"] = sufficient_conditions;
    }
    if (!contributing_factors.empty()) {
        j["contributing_factors"] = contributing_factors;
    }
    if (!inhibitors.empty()) {
        j["inhibitors"] = inhibitors;
    }

    j["confidence"] = confidence;
    if (!evidence_sources.empty()) {
        j["evidence_sources"] = evidence_sources;
    }
    if (citation_count > 0) {
        j["citation_count"] = citation_count;
    }

    if (!temporal_context.empty()) {
        j["temporal_context"] = temporal_context;
    }
    j["bidirectional"] = bidirectional;

    if (!context.empty()) {
        j["context"] = context;
    }
    if (!scope_limitations.empty()) {
        j["scope_limitations"] = scope_limitations;
    }

    return j;
}

CausalMetadata CausalMetadata::from_json(const nlohmann::json& j) {
    CausalMetadata meta;

    if (j.contains("type")) {
        meta.type = causal::type_from_string(j["type"]);
    }
    if (j.contains("strength")) {
        meta.strength = causal::strength_from_string(j["strength"]);
    }
    if (j.contains("temporality")) {
        meta.temporality = causal::temporality_from_string(j["temporality"]);
    }

    if (j.contains("mechanism_description")) {
        meta.mechanism_description = j["mechanism_description"];
    }
    if (j.contains("mechanism_type")) {
        meta.mechanism_type = j["mechanism_type"];
    }
    if (j.contains("mechanism_chain")) {
        meta.mechanism_chain = j["mechanism_chain"].get<std::vector<std::string>>();
    }

    if (j.contains("necessary_conditions")) {
        meta.necessary_conditions = j["necessary_conditions"].get<std::vector<std::string>>();
    }
    if (j.contains("sufficient_conditions")) {
        meta.sufficient_conditions = j["sufficient_conditions"].get<std::vector<std::string>>();
    }
    if (j.contains("contributing_factors")) {
        meta.contributing_factors = j["contributing_factors"].get<std::vector<std::string>>();
    }
    if (j.contains("inhibitors")) {
        meta.inhibitors = j["inhibitors"].get<std::vector<std::string>>();
    }

    if (j.contains("confidence")) {
        meta.confidence = j["confidence"];
    }
    if (j.contains("evidence_sources")) {
        meta.evidence_sources = j["evidence_sources"].get<std::vector<std::string>>();
    }
    if (j.contains("citation_count")) {
        meta.citation_count = j["citation_count"];
    }

    if (j.contains("temporal_context")) {
        meta.temporal_context = j["temporal_context"];
    }
    if (j.contains("bidirectional")) {
        meta.bidirectional = j["bidirectional"];
    }

    if (j.contains("context")) {
        meta.context = j["context"];
    }
    if (j.contains("scope_limitations")) {
        meta.scope_limitations = j["scope_limitations"].get<std::vector<std::string>>();
    }

    return meta;
}

double CausalMetadata::get_strength_score() const {
    switch (strength) {
        case CausalStrength::WEAK:          return 0.25;
        case CausalStrength::MODERATE:      return 0.50;
        case CausalStrength::STRONG:        return 0.75;
        case CausalStrength::DETERMINISTIC: return 1.00;
        default:                            return 0.50;
    }
}

std::string CausalMetadata::get_type_string() const {
    return causal::to_string(type);
}

std::string CausalMetadata::get_strength_string() const {
    return causal::to_string(strength);
}

std::string CausalMetadata::get_temporality_string() const {
    return causal::to_string(temporality);
}

// ============================================================================
// Helper Functions
// ============================================================================

namespace causal {

std::string to_string(CausalRelationType type) {
    switch (type) {
        case CausalRelationType::DIRECT_CAUSE:  return "direct_cause";
        case CausalRelationType::NECESSARY:     return "necessary";
        case CausalRelationType::SUFFICIENT:    return "sufficient";
        case CausalRelationType::CONTRIBUTING:  return "contributing";
        case CausalRelationType::PREVENTING:    return "preventing";
        case CausalRelationType::ENABLING:      return "enabling";
        case CausalRelationType::MECHANISM:     return "mechanism";
        default:                                return "direct_cause";
    }
}

CausalRelationType type_from_string(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "direct_cause" || lower == "direct")    return CausalRelationType::DIRECT_CAUSE;
    if (lower == "necessary")                            return CausalRelationType::NECESSARY;
    if (lower == "sufficient")                           return CausalRelationType::SUFFICIENT;
    if (lower == "contributing" || lower == "contributes") return CausalRelationType::CONTRIBUTING;
    if (lower == "preventing" || lower == "prevents")    return CausalRelationType::PREVENTING;
    if (lower == "enabling" || lower == "enables")       return CausalRelationType::ENABLING;
    if (lower == "mechanism")                            return CausalRelationType::MECHANISM;

    return CausalRelationType::DIRECT_CAUSE;  // Default
}

std::string to_string(CausalStrength strength) {
    switch (strength) {
        case CausalStrength::WEAK:           return "weak";
        case CausalStrength::MODERATE:       return "moderate";
        case CausalStrength::STRONG:         return "strong";
        case CausalStrength::DETERMINISTIC:  return "deterministic";
        default:                             return "moderate";
    }
}

CausalStrength strength_from_string(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "weak")                     return CausalStrength::WEAK;
    if (lower == "moderate" || lower == "medium") return CausalStrength::MODERATE;
    if (lower == "strong")                   return CausalStrength::STRONG;
    if (lower == "deterministic" || lower == "certain") return CausalStrength::DETERMINISTIC;

    return CausalStrength::MODERATE;  // Default
}

std::string to_string(Temporality temp) {
    switch (temp) {
        case Temporality::IMMEDIATE:    return "immediate";
        case Temporality::SHORT_TERM:   return "short_term";
        case Temporality::LONG_TERM:    return "long_term";
        case Temporality::DELAYED:      return "delayed";
        default:                        return "immediate";
    }
}

Temporality temporality_from_string(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "immediate" || lower == "instant") return Temporality::IMMEDIATE;
    if (lower == "short_term" || lower == "short")  return Temporality::SHORT_TERM;
    if (lower == "long_term" || lower == "long")    return Temporality::LONG_TERM;
    if (lower == "delayed")                         return Temporality::DELAYED;

    return Temporality::IMMEDIATE;  // Default
}

} // namespace causal

} // namespace kg

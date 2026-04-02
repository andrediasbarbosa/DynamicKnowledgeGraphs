/**
 * @file causal_extraction_demo.cpp
 * @brief Phase 2 Day 14: Demonstration of causal extraction prompts
 *
 * This demo shows how to use the enhanced causal extraction prompts
 * to extract rich causal relationships from scientific text.
 */

#include "llm/llm_provider.hpp"
#include "llm/causal_metadata.hpp"
#include <iostream>
#include <fstream>
#include <string>

using namespace kg;

// Sample scientific text with causal relationships
const char* SAMPLE_TEXT = R"(
Climate Change and Ocean Acidification

Increased atmospheric CO2 concentrations directly cause global temperature rise
through the greenhouse effect. CO2 molecules trap infrared radiation emitted from
Earth's surface, preventing heat from escaping to space. This mechanism operates
over decades, with strong evidence from ice core data.

Ocean acidification results from CO2 absorption by seawater. When CO2 dissolves
in ocean water, it forms carbonic acid, which lowers the pH of seawater. This
chemical process is deterministic and immediate. The resulting acidification is
sufficient to impair calcium carbonate formation in marine organisms such as
corals and shellfish.
)";

int main() {
    std::cout << "==================================================================\n";
    std::cout << "  Phase 2 Day 14: Causal Extraction Demonstration\n";
    std::cout << "==================================================================\n\n";

    // 1. Show the difference between standard and causal prompts
    std::cout << "1. STANDARD RELATION EXTRACTION PROMPT\n";
    std::cout << "--------------------------------------\n";
    std::string standard_prompt = PromptTemplates::relation_extraction_system_prompt();
    std::cout << "Length: " << standard_prompt.length() << " characters\n";
    std::cout << "Focus: General relation extraction\n";
    std::cout << "Output: sources, relation, targets, confidence\n";
    std::cout << "\n";

    std::cout << "2. CAUSAL EXTRACTION PROMPT (Phase 2)\n";
    std::cout << "--------------------------------------\n";
    std::string causal_prompt = PromptTemplates::causal_extraction_system_prompt();
    std::cout << "Length: " << causal_prompt.length() << " characters\n";
    std::cout << "Focus: Causal relationships with rich metadata\n";
    std::cout << "Extracts:\n";
    std::cout << "  - Causal type (7 types: direct, necessary, sufficient, etc.)\n";
    std::cout << "  - Causal strength (4 levels: weak, moderate, strong, deterministic)\n";
    std::cout << "  - Mechanism description (HOW A causes B)\n";
    std::cout << "  - Mechanism type (physical, chemical, biological, etc.)\n";
    std::cout << "  - Mechanism chain (intermediate steps)\n";
    std::cout << "  - Necessary/sufficient conditions\n";
    std::cout << "  - Contributing factors and inhibitors\n";
    std::cout << "  - Temporal aspects (immediate, short-term, long-term, delayed)\n";
    std::cout << "  - Evidence sources and confidence\n";
    std::cout << "\n";

    std::cout << "Prompt size increase: +"
              << (causal_prompt.length() - standard_prompt.length())
              << " characters ("
              << (100.0 * (causal_prompt.length() - standard_prompt.length()) / standard_prompt.length())
              << "% increase)\n\n";

    // 2. Show sample text
    std::cout << "3. SAMPLE SCIENTIFIC TEXT\n";
    std::cout << "-------------------------\n";
    std::cout << SAMPLE_TEXT << "\n\n";

    // 3. Generate user prompts
    std::cout << "4. GENERATED PROMPTS FOR LLM\n";
    std::cout << "----------------------------\n";

    std::string standard_user = PromptTemplates::relation_extraction_user_prompt(SAMPLE_TEXT);
    std::string causal_user = PromptTemplates::causal_extraction_user_prompt(SAMPLE_TEXT);

    std::cout << "Standard extraction user prompt: " << standard_user.length() << " chars\n";
    std::cout << "Causal extraction user prompt: " << causal_user.length() << " chars\n";
    std::cout << "\n";

    // 4. Show expected output structure
    std::cout << "5. EXPECTED CAUSAL EXTRACTION OUTPUT\n";
    std::cout << "------------------------------------\n";
    std::cout << "For the relationship: \"CO2 causes global warming through greenhouse effect\"\n\n";
    std::cout << "{\n";
    std::cout << "  \"sources\": [\"CO2\"],\n";
    std::cout << "  \"relation\": \"causes\",\n";
    std::cout << "  \"targets\": [\"global warming\"],\n";
    std::cout << "  \"confidence\": 0.95,\n";
    std::cout << "  \"causal\": {\n";
    std::cout << "    \"type\": \"direct_cause\",\n";
    std::cout << "    \"strength\": \"strong\",\n";
    std::cout << "    \"temporality\": \"long_term\",\n";
    std::cout << "    \"mechanism_description\": \"greenhouse effect - CO2 traps infrared radiation\",\n";
    std::cout << "    \"mechanism_type\": \"physical\",\n";
    std::cout << "    \"mechanism_chain\": [\n";
    std::cout << "      \"CO2 emission\",\n";
    std::cout << "      \"infrared trapping\",\n";
    std::cout << "      \"temperature rise\"\n";
    std::cout << "    ],\n";
    std::cout << "    \"temporal_context\": \"over decades\",\n";
    std::cout << "    \"confidence\": 0.95,\n";
    std::cout << "    \"evidence_sources\": [\"ice core data\", \"atmospheric measurements\"]\n";
    std::cout << "  }\n";
    std::cout << "}\n\n";

    // 5. Show how to use in CLI
    std::cout << "6. USING CAUSAL EXTRACTION IN CLI\n";
    std::cout << "----------------------------------\n";
    std::cout << "Standard extraction:\n";
    std::cout << "  $ kg run --input paper.pdf --output runs/\n\n";
    std::cout << "Causal extraction (Phase 2):\n";
    std::cout << "  $ kg run --input paper.pdf --output runs/ --causal\n\n";
    std::cout << "The --causal flag switches to the enhanced causal extraction prompts,\n";
    std::cout << "which will extract rich causal metadata for every relationship found.\n\n";

    // 6. Parse a sample causal JSON
    std::cout << "7. PARSING TEST\n";
    std::cout << "---------------\n";
    std::string sample_json = R"({
        "relations": [{
            "sources": ["CO2"],
            "relation": "causes",
            "targets": ["ocean acidification"],
            "confidence": 0.95,
            "causal": {
                "type": "direct_cause",
                "strength": "deterministic",
                "temporality": "immediate",
                "mechanism_description": "CO2 dissolves forming carbonic acid",
                "mechanism_type": "chemical",
                "confidence": 0.98
            }
        }]
    })";

    try {
        std::vector<ExtractedRelation> rels = parse_relations_json(sample_json);
        if (!rels.empty() && rels[0].is_causal()) {
            const auto& rel = rels[0];
            const auto& causal = *rel.causal_metadata;

            std::cout << "✅ Successfully parsed causal relation:\n";
            std::cout << "   " << rel.sources[0] << " → " << rel.targets[0] << "\n";
            std::cout << "   Type: " << causal.get_type_string() << "\n";
            std::cout << "   Strength: " << causal.get_strength_string()
                      << " (" << causal.get_strength_score() << ")\n";
            std::cout << "   Mechanism: " << causal.mechanism_description << "\n";
            std::cout << "   Mechanism Type: " << causal.mechanism_type << "\n";
        } else {
            std::cout << "❌ Failed to parse or no causal metadata found\n";
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << "\n";
    }
    std::cout << "\n";

    // 7. Summary
    std::cout << "==================================================================\n";
    std::cout << "  SUMMARY\n";
    std::cout << "==================================================================\n\n";
    std::cout << "Phase 2 causal extraction features:\n";
    std::cout << "✅ Enhanced 3,700+ character system prompt\n";
    std::cout << "✅ 7 causal types (direct, necessary, sufficient, etc.)\n";
    std::cout << "✅ 4 strength levels with numeric scores\n";
    std::cout << "✅ Rich mechanism extraction (description, type, chain)\n";
    std::cout << "✅ Logical conditions (necessary, sufficient, contributing, inhibitors)\n";
    std::cout << "✅ Temporal aspects (immediate, short-term, long-term, delayed)\n";
    std::cout << "✅ Evidence tracking and confidence scoring\n";
    std::cout << "✅ Full JSON serialization/deserialization\n";
    std::cout << "✅ CLI integration via --causal flag\n";
    std::cout << "✅ Backward compatible with standard extraction\n\n";

    std::cout << "Next steps:\n";
    std::cout << "1. Test with real scientific PDFs using: kg run --input paper.pdf --causal\n";
    std::cout << "2. Validate extracted causal metadata quality\n";
    std::cout << "3. Tune prompts based on results\n";
    std::cout << "4. Add causal-specific discovery operators\n\n";

    return 0;
}

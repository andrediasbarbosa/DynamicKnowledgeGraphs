#include "pipeline/graph_cleaner.hpp"
#include "llm/llm_provider.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <regex>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace kg {

// ============================================================================
// CleaningReport Implementation
// ============================================================================

void CleaningReport::print_summary() const {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  Graph Quality Control Report\n";
    std::cout << "======================================================================\n";
    std::cout << "\n";
    std::cout << "Initial:   " << initial_nodes << " nodes, " << initial_edges << " edges\n";
    std::cout << "\n";

    if (level1_removed > 0) {
        std::cout << "Level 1: Rule-Based Filtering\n";
        std::cout << "  Removed by length:     " << removed_by_length << "\n";
        std::cout << "  Removed by stopwords:  " << removed_by_stopwords << "\n";
        std::cout << "  Removed by numbers:    " << removed_by_numbers << "\n";
        std::cout << "  Removed by artifacts:  " << removed_by_artifacts << "\n";
        std::cout << "  Level 1 total:         " << level1_removed << " ("
                  << (100.0 * level1_removed / initial_nodes) << "%)\n";
        std::cout << "\n";
    }

    if (level2_removed > 0) {
        std::cout << "Level 2: Statistical Filtering\n";
        std::cout << "  Removed by degree:     " << removed_by_degree << "\n";
        std::cout << "  Removed by importance: " << removed_by_importance << "\n";
        std::cout << "  Removed by outliers:   " << removed_by_outliers << "\n";
        std::cout << "  Level 2 total:         " << level2_removed << " ("
                  << (100.0 * level2_removed / initial_nodes) << "%)\n";
        std::cout << "\n";
    }

    if (validated_by_llm > 0) {
        std::cout << "Level 3: LLM Validation\n";
        std::cout << "  Validated by LLM:      " << validated_by_llm << "\n";
        std::cout << "  Removed by LLM:        " << removed_by_llm << " ("
                  << (100.0 * removed_by_llm / validated_by_llm) << "%)\n";
        std::cout << "  Level 3 total:         " << level3_removed << "\n";
        std::cout << "\n";
    }

    int total_removed = level1_removed + level2_removed + level3_removed;
    std::cout << "Final:     " << final_nodes << " nodes, " << final_edges << " edges\n";
    std::cout << "Removed:   " << total_removed << " nodes ("
              << (100.0 * total_removed / initial_nodes) << "%)\n";
    std::cout << "Time:      " << cleaning_time_ms << " ms\n";
    std::cout << "\n";
    std::cout << "======================================================================\n";
}

nlohmann::json CleaningReport::to_json() const {
    nlohmann::json j;
    j["initial_nodes"] = initial_nodes;
    j["initial_edges"] = initial_edges;
    j["final_nodes"] = final_nodes;
    j["final_edges"] = final_edges;

    j["level1"] = {
        {"removed_by_length", removed_by_length},
        {"removed_by_stopwords", removed_by_stopwords},
        {"removed_by_numbers", removed_by_numbers},
        {"removed_by_artifacts", removed_by_artifacts},
        {"total_removed", level1_removed}
    };

    j["level1_5"] = {
        {"semantic_duplicates_found", semantic_duplicates_found},
        {"semantic_duplicates_merged", semantic_duplicates_merged}
    };

    j["level2"] = {
        {"removed_by_degree", removed_by_degree},
        {"removed_by_importance", removed_by_importance},
        {"removed_by_outliers", removed_by_outliers},
        {"total_removed", level2_removed}
    };

    j["level3"] = {
        {"validated_by_llm", validated_by_llm},
        {"removed_by_llm", removed_by_llm},
        {"total_removed", level3_removed}
    };

    j["total_removed"] = level1_removed + level2_removed + level3_removed;
    j["cleaning_time_ms"] = cleaning_time_ms;

    return j;
}

std::string CleaningReport::generate_html_report() const {
    std::stringstream html;

    int total_removed = level1_removed + level2_removed + level3_removed;
    double removal_rate = initial_nodes > 0 ? (100.0 * total_removed / initial_nodes) : 0.0;

    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Quality Control Report</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 40px 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px;
            text-align: center;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            font-weight: 700;
        }
        .header p {
            font-size: 1.1em;
            opacity: 0.9;
        }
        .content {
            padding: 40px;
        }
        .summary-cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 40px;
        }
        .card {
            background: #f8f9fa;
            border-radius: 8px;
            padding: 25px;
            border-left: 4px solid #667eea;
        }
        .card h3 {
            color: #495057;
            font-size: 0.9em;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-bottom: 10px;
            font-weight: 600;
        }
        .card .value {
            font-size: 2.5em;
            font-weight: 700;
            color: #212529;
            margin-bottom: 5px;
        }
        .card .label {
            color: #6c757d;
            font-size: 0.95em;
        }
        .card.success { border-left-color: #28a745; }
        .card.warning { border-left-color: #ffc107; }
        .card.danger { border-left-color: #dc3545; }
        .card.info { border-left-color: #17a2b8; }

        .section {
            margin-bottom: 40px;
        }
        .section h2 {
            color: #212529;
            font-size: 1.8em;
            margin-bottom: 20px;
            padding-bottom: 10px;
            border-bottom: 2px solid #e9ecef;
        }
        .level-box {
            background: white;
            border: 1px solid #dee2e6;
            border-radius: 8px;
            padding: 25px;
            margin-bottom: 20px;
        }
        .level-box h3 {
            color: #495057;
            font-size: 1.3em;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
        }
        .level-badge {
            display: inline-block;
            background: #667eea;
            color: white;
            padding: 5px 12px;
            border-radius: 20px;
            font-size: 0.75em;
            margin-left: 10px;
            font-weight: 600;
        }
        .stat-row {
            display: flex;
            justify-content: space-between;
            padding: 12px 0;
            border-bottom: 1px solid #f1f3f5;
        }
        .stat-row:last-child {
            border-bottom: none;
        }
        .stat-label {
            color: #6c757d;
            font-weight: 500;
        }
        .stat-value {
            color: #212529;
            font-weight: 600;
        }
        .progress-bar {
            width: 100%;
            height: 30px;
            background: #e9ecef;
            border-radius: 15px;
            overflow: hidden;
            margin-top: 20px;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #28a745 0%, #20c997 100%);
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: 600;
            font-size: 0.9em;
            transition: width 0.3s ease;
        }
        .comparison-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }
        .comparison-table th,
        .comparison-table td {
            padding: 15px;
            text-align: left;
            border-bottom: 1px solid #dee2e6;
        }
        .comparison-table th {
            background: #f8f9fa;
            color: #495057;
            font-weight: 600;
            text-transform: uppercase;
            font-size: 0.85em;
            letter-spacing: 0.5px;
        }
        .comparison-table tr:last-child td {
            border-bottom: none;
        }
        .metric-change {
            display: inline-block;
            padding: 4px 10px;
            border-radius: 12px;
            font-size: 0.85em;
            font-weight: 600;
        }
        .metric-change.positive {
            background: #d4edda;
            color: #155724;
        }
        .metric-change.negative {
            background: #f8d7da;
            color: #721c24;
        }
        .footer {
            background: #f8f9fa;
            padding: 20px 40px;
            text-align: center;
            color: #6c757d;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🧹 Quality Control Report</h1>
            <p>3-Level Graph Cleaning Analysis</p>
        </div>

        <div class="content">
            <!-- Summary Cards -->
            <div class="summary-cards">
                <div class="card info">
                    <h3>Initial Graph</h3>
                    <div class="value">)" << initial_nodes << R"(</div>
                    <div class="label">entities, )" << initial_edges << R"( relations</div>
                </div>
                <div class="card success">
                    <h3>Final Graph</h3>
                    <div class="value">)" << final_nodes << R"(</div>
                    <div class="label">entities, )" << final_edges << R"( relations</div>
                </div>
                <div class="card warning">
                    <h3>Entities Removed</h3>
                    <div class="value">)" << total_removed << R"(</div>
                    <div class="label">)" << std::fixed << std::setprecision(1) << removal_rate << R"(% of total</div>
                </div>
                <div class="card danger">
                    <h3>Processing Time</h3>
                    <div class="value">)" << std::fixed << std::setprecision(0) << cleaning_time_ms << R"(</div>
                    <div class="label">milliseconds</div>
                </div>
            </div>

            <!-- Retention Rate -->
            <div class="section">
                <h2>Retention Rate</h2>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: )"
         << std::fixed << std::setprecision(1) << (100.0 - removal_rate) << R"(%">
                        )" << final_nodes << " / " << initial_nodes << " entities retained ("
         << std::fixed << std::setprecision(1) << (100.0 - removal_rate) << R"(%)</div>
                </div>
            </div>

            <!-- Level-by-Level Breakdown -->
            <div class="section">
                <h2>Cleaning Breakdown</h2>
)";

    // Level 1
    if (level1_removed > 0) {
        html << R"(
                <div class="level-box">
                    <h3>Level 1: Rule-Based Filtering <span class="level-badge">FAST</span></h3>
                    <div class="stat-row">
                        <span class="stat-label">Removed by length constraints</span>
                        <span class="stat-value">)" << removed_by_length << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">Removed stopwords</span>
                        <span class="stat-value">)" << removed_by_stopwords << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">Removed pure numbers</span>
                        <span class="stat-value">)" << removed_by_numbers << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">Removed artifacts (fig, table, etc.)</span>
                        <span class="stat-value">)" << removed_by_artifacts << R"(</span>
                    </div>
                    <div class="stat-row" style="border-top: 2px solid #667eea; margin-top: 10px; padding-top: 15px;">
                        <span class="stat-label" style="color: #667eea; font-weight: 600;">Level 1 Total</span>
                        <span class="stat-value" style="color: #667eea; font-size: 1.2em;">)" << level1_removed
             << " (" << std::fixed << std::setprecision(1)
             << (initial_nodes > 0 ? 100.0 * level1_removed / initial_nodes : 0.0) << R"(%)</span>
                    </div>
                </div>
)";
    }

    // Level 2
    if (level2_removed > 0) {
        html << R"(
                <div class="level-box">
                    <h3>Level 2: Statistical Filtering <span class="level-badge">MEDIUM</span></h3>
                    <div class="stat-row">
                        <span class="stat-label">Removed by low degree (connectivity)</span>
                        <span class="stat-value">)" << removed_by_degree << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">Removed by low importance</span>
                        <span class="stat-value">)" << removed_by_importance << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">Removed as statistical outliers</span>
                        <span class="stat-value">)" << removed_by_outliers << R"(</span>
                    </div>
                    <div class="stat-row" style="border-top: 2px solid #667eea; margin-top: 10px; padding-top: 15px;">
                        <span class="stat-label" style="color: #667eea; font-weight: 600;">Level 2 Total</span>
                        <span class="stat-value" style="color: #667eea; font-size: 1.2em;">)" << level2_removed
             << " (" << std::fixed << std::setprecision(1)
             << (initial_nodes > 0 ? 100.0 * level2_removed / initial_nodes : 0.0) << R"(%)</span>
                    </div>
                </div>
)";
    }

    // Level 3
    if (validated_by_llm > 0) {
        html << R"(
                <div class="level-box">
                    <h3>Level 3: LLM Validation <span class="level-badge">ACCURATE</span></h3>
                    <div class="stat-row">
                        <span class="stat-label">Entities validated by LLM</span>
                        <span class="stat-value">)" << validated_by_llm << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">Removed by LLM</span>
                        <span class="stat-value">)" << removed_by_llm << R"(</span>
                    </div>
                    <div class="stat-row">
                        <span class="stat-label">LLM rejection rate</span>
                        <span class="stat-value">)" << std::fixed << std::setprecision(1)
             << (validated_by_llm > 0 ? 100.0 * removed_by_llm / validated_by_llm : 0.0) << R"(%</span>
                    </div>
                    <div class="stat-row" style="border-top: 2px solid #667eea; margin-top: 10px; padding-top: 15px;">
                        <span class="stat-label" style="color: #667eea; font-weight: 600;">Level 3 Total</span>
                        <span class="stat-value" style="color: #667eea; font-size: 1.2em;">)" << level3_removed
             << " (" << std::fixed << std::setprecision(1)
             << (initial_nodes > 0 ? 100.0 * level3_removed / initial_nodes : 0.0) << R"(%)</span>
                    </div>
                </div>
)";
    }

    // Comparison Table
    html << R"(
            </div>

            <!-- Before/After Comparison -->
            <div class="section">
                <h2>Before/After Comparison</h2>
                <table class="comparison-table">
                    <thead>
                        <tr>
                            <th>Metric</th>
                            <th>Before Cleaning</th>
                            <th>After Cleaning</th>
                            <th>Change</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr>
                            <td><strong>Entities</strong></td>
                            <td>)" << initial_nodes << R"(</td>
                            <td>)" << final_nodes << R"(</td>
                            <td><span class="metric-change )"
         << (total_removed > 0 ? "negative" : "positive") << R"(">
                                -)" << total_removed << " ("
         << std::fixed << std::setprecision(1) << removal_rate << R"(%)</span>
                            </td>
                        </tr>
                        <tr>
                            <td><strong>Relations</strong></td>
                            <td>)" << initial_edges << R"(</td>
                            <td>)" << final_edges << R"(</td>
                            <td><span class="metric-change )"
         << ((initial_edges - final_edges) > 0 ? "negative" : "positive") << R"(">
                                -)" << (initial_edges - final_edges) << " ("
         << std::fixed << std::setprecision(1)
         << (initial_edges > 0 ? 100.0 * (initial_edges - final_edges) / initial_edges : 0.0) << R"(%)</span>
                            </td>
                        </tr>
                        <tr>
                            <td><strong>Avg. Relations per Entity</strong></td>
                            <td>)" << std::fixed << std::setprecision(2)
         << (initial_nodes > 0 ? (double)initial_edges / initial_nodes : 0.0) << R"(</td>
                            <td>)" << std::fixed << std::setprecision(2)
         << (final_nodes > 0 ? (double)final_edges / final_nodes : 0.0) << R"(</td>
                            <td><span class="metric-change )"
         << ((final_nodes > 0 && initial_nodes > 0 &&
              ((double)final_edges / final_nodes) > ((double)initial_edges / initial_nodes))
             ? "positive" : "negative") << R"(">
                                )" << (final_nodes > 0 && initial_nodes > 0
                                      ? (((double)final_edges / final_nodes) - ((double)initial_edges / initial_nodes) >= 0 ? "+" : "")
                                      : "")
         << std::fixed << std::setprecision(2)
         << (final_nodes > 0 && initial_nodes > 0
             ? ((double)final_edges / final_nodes) - ((double)initial_edges / initial_nodes)
             : 0.0) << R"(</span>
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </div>

        <div class="footer">
            <p>Generated by Knowledge Graph Quality Control System v1.0</p>
            <p>Processing completed in )" << std::fixed << std::setprecision(0) << cleaning_time_ms << R"( ms</p>
        </div>
    </div>
</body>
</html>
)";

    return html.str();
}

// ============================================================================
// GraphCleaner Implementation
// ============================================================================

GraphCleaner::GraphCleaner() {
    // Initialize stopwords
    stopwords_ = {
        "a", "an", "the", "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "do", "does", "did", "will", "would", "should",
        "could", "may", "might", "must", "can", "of", "in", "to", "for", "with",
        "on", "at", "from", "by", "about", "as", "into", "through", "during",
        "before", "after", "above", "below", "between", "under", "again",
        "further", "then", "once", "here", "there", "when", "where", "why",
        "how", "all", "each", "every", "both", "few", "more", "most", "other",
        "some", "such", "no", "nor", "not", "only", "own", "same", "so", "than",
        "too", "very", "s", "t", "just", "now"
    };

    // Initialize extraction artifacts
    artifacts_ = {
        "fig", "figure", "table", "page", "section", "chapter", "appendix",
        "ref", "reference", "see", "also", "cf", "et", "al", "i.e", "e.g",
        "etc", "vs", "v", "via", "ibid", "op", "cit", "loc"
    };
}

GraphCleaner::~GraphCleaner() = default;

CleaningReport GraphCleaner::clean(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    std::shared_ptr<LLMProvider> llm
) {
    auto start_time = std::chrono::steady_clock::now();

    CleaningReport report;
    report.initial_nodes = entities.size();
    report.initial_edges = relations.size();

    // Level 1: Rule-based filtering (fast, removes obvious noise)
    if (config.enable_rule_based) {
        level1_rule_based_filtering(entities, relations, config, report);
    }

    // Level 1.5: Semantic deduplication (merges semantically similar entities)
    if (config.enable_semantic_dedup && llm) {
        level15_semantic_deduplication(entities, relations, config, llm, report);
    }

    // Level 2: Statistical filtering (medium speed, removes outliers)
    if (config.enable_statistical) {
        level2_statistical_filtering(entities, relations, config, report);
    }

    // Level 3: LLM validation (slow but accurate)
    if (config.enable_llm_validation && llm) {
        level3_llm_validation(entities, relations, config, llm, report);
    }

    report.final_nodes = std::count_if(entities.begin(), entities.end(),
                                       [](const auto& e) { return e.is_valid; });
    report.final_edges = std::count_if(relations.begin(), relations.end(),
                                       [](const auto& r) { return r.is_valid; });

    auto end_time = std::chrono::steady_clock::now();
    report.cleaning_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return report;
}

// ============================================================================
// Level 1: Rule-Based Filtering
// ============================================================================

void GraphCleaner::level1_rule_based_filtering(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    CleaningReport& report
) {
    if (config.progress_callback) {
        config.progress_callback("Level 1: Rule-based", 0, entities.size());
    }

    int processed = 0;
    for (auto& entity : entities) {
        if (!entity.is_valid) continue;

        // Trim whitespace from label for accurate checking
        std::string label = trim(entity.label);
        std::string label_lower = to_lowercase(label);

        // Check length
        if (!is_valid_length(label, config.min_node_length, config.max_node_length)) {
            entity.is_valid = false;
            entity.removal_reason = "invalid_length";
            report.removed_by_length++;
            report.level1_removed++;
            continue;
        }

        // Check single character
        if (config.remove_single_chars && is_single_char(label)) {
            entity.is_valid = false;
            entity.removal_reason = "single_char";
            report.removed_by_length++;
            report.level1_removed++;
            continue;
        }

        // Check stopwords
        if (config.remove_stopwords && is_stopword(label_lower)) {
            entity.is_valid = false;
            entity.removal_reason = "stopword";
            report.removed_by_stopwords++;
            report.level1_removed++;
            continue;
        }

        // Check pure numbers
        if (config.remove_numbers && is_pure_number(label)) {
            entity.is_valid = false;
            entity.removal_reason = "pure_number";
            report.removed_by_numbers++;
            report.level1_removed++;
            continue;
        }

        // Check extraction artifacts
        if (config.remove_artifacts && is_extraction_artifact(label_lower)) {
            entity.is_valid = false;
            entity.removal_reason = "artifact";
            report.removed_by_artifacts++;
            report.level1_removed++;
            continue;
        }

        // Check pure punctuation
        if (is_pure_punctuation(label)) {
            entity.is_valid = false;
            entity.removal_reason = "punctuation";
            report.removed_by_artifacts++;
            report.level1_removed++;
            continue;
        }

        processed++;
        if (config.progress_callback && processed % 100 == 0) {
            config.progress_callback("Level 1: Rule-based", processed, entities.size());
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        valid_ids.insert(e.id);
    }
    remove_invalid_relations(relations, valid_ids);
}

// ============================================================================
// Level 1.5: Semantic Deduplication
// ============================================================================

void GraphCleaner::level15_semantic_deduplication(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    std::shared_ptr<LLMProvider> llm,
    CleaningReport& report
) {
    if (!llm) return;

    if (config.progress_callback) {
        config.progress_callback("Level 1.5: Semantic Dedup", 0, entities.size());
    }

    // Collect valid entities for semantic comparison
    std::vector<CleanableEntity*> valid_entities;
    for (auto& entity : entities) {
        if (entity.is_valid) {
            valid_entities.push_back(&entity);
        }
    }

    if (valid_entities.size() < 2) return;  // Need at least 2 entities to deduplicate

    // Build map: canonical label -> list of entity pointers
    std::map<std::string, std::vector<CleanableEntity*>> duplicate_groups;

    // Process entities in batches for semantic comparison
    for (size_t i = 0; i < valid_entities.size(); i += config.semantic_batch_size) {
        size_t batch_end = std::min(i + config.semantic_batch_size, valid_entities.size());
        std::vector<std::string> batch_labels;

        for (size_t j = i; j < batch_end; j++) {
            batch_labels.push_back(valid_entities[j]->label);
        }

        // Ask LLM to identify semantic duplicates in this batch
        auto duplicates = identify_semantic_duplicates(batch_labels, llm);

        // Group entities by their canonical form
        for (const auto& dup_group : duplicates) {
            for (const auto& label : dup_group.variants) {
                // Find the entity with this label
                for (size_t j = i; j < batch_end; j++) {
                    if (valid_entities[j]->label == label) {
                        duplicate_groups[dup_group.canonical].push_back(valid_entities[j]);
                        break;
                    }
                }
            }
        }

        if (config.progress_callback) {
            config.progress_callback("Level 1.5: Semantic Dedup", batch_end, valid_entities.size());
        }
    }

    // Merge duplicate entities
    for (const auto& [canonical, group] : duplicate_groups) {
        if (group.size() > 1) {
            report.semantic_duplicates_found++;

            // Keep entity with highest confidence/degree, mark others as invalid
            CleanableEntity* keep = group[0];
            for (size_t i = 1; i < group.size(); i++) {
                if (group[i]->confidence > keep->confidence ||
                    (group[i]->confidence == keep->confidence && group[i]->degree > keep->degree)) {
                    keep = group[i];
                }
            }

            // Update the kept entity to use canonical label
            keep->label = canonical;

            // Mark other variants as invalid (will be merged later)
            for (auto* entity : group) {
                if (entity != keep) {
                    entity->is_valid = false;
                    entity->removal_reason = "semantic_duplicate_of_" + keep->label;
                    report.semantic_duplicates_merged++;
                }
            }
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        if (e.is_valid) {
            valid_ids.insert(e.id);
        }
    }
    remove_invalid_relations(relations, valid_ids);
}

std::vector<GraphCleaner::SemanticDuplicateGroup> GraphCleaner::identify_semantic_duplicates(
    const std::vector<std::string>& labels,
    std::shared_ptr<LLMProvider> llm
) const {
    std::vector<SemanticDuplicateGroup> result;

    if (labels.size() < 2) return result;

    // Build prompt for LLM to identify semantic duplicates
    std::stringstream prompt;
    prompt << "You are identifying semantic duplicates in entity labels extracted from scientific documents.\n";
    prompt << "Semantic duplicates are entities that refer to the same concept but have different labels.\n\n";
    prompt << "Examples:\n";
    prompt << "- 'ML' and 'machine learning' are semantic duplicates\n";
    prompt << "- 'AI' and 'artificial intelligence' are semantic duplicates\n";
    prompt << "- 'neural network' and 'neural networks' are semantic duplicates (singular/plural)\n";
    prompt << "- 'CO2' and 'carbon dioxide' are semantic duplicates\n\n";
    prompt << "Entity labels:\n";
    for (size_t i = 0; i < labels.size(); i++) {
        prompt << (i + 1) << ". \"" << labels[i] << "\"\n";
    }
    prompt << "\nIdentify groups of semantic duplicates. For each group, specify the canonical form (preferred label).\n";
    prompt << "Respond in JSON format:\n";
    prompt << "{\n";
    prompt << "  \"duplicates\": [\n";
    prompt << "    {\"canonical\": \"machine learning\", \"variants\": [\"ML\", \"machine learning\"]},\n";
    prompt << "    {\"canonical\": \"artificial intelligence\", \"variants\": [\"AI\", \"artificial intelligence\"]}\n";
    prompt << "  ]\n";
    prompt << "}\n";
    prompt << "\nIf no semantic duplicates are found, return: {\"duplicates\": []}\n";

    try {
        LLMResponse response = llm->complete(prompt.str());
        if (!response.success) {
            return result;  // Empty result on failure
        }

        // Parse JSON response
        auto json = nlohmann::json::parse(response.content);
        if (json.contains("duplicates") && json["duplicates"].is_array()) {
            for (const auto& dup : json["duplicates"]) {
                if (dup.contains("canonical") && dup.contains("variants")) {
                    SemanticDuplicateGroup group;
                    group.canonical = dup["canonical"].get<std::string>();

                    if (dup["variants"].is_array()) {
                        for (const auto& variant : dup["variants"]) {
                            if (variant.is_string()) {
                                group.variants.push_back(variant.get<std::string>());
                            }
                        }
                    }

                    if (group.variants.size() > 1) {
                        result.push_back(group);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // Parsing failed, return empty result
        std::cerr << "Semantic dedup parsing error: " << e.what() << std::endl;
    }

    return result;
}

// ============================================================================
// Level 2: Statistical Filtering
// ============================================================================

void GraphCleaner::level2_statistical_filtering(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    CleaningReport& report
) {
    if (config.progress_callback) {
        config.progress_callback("Level 2: Statistical", 0, entities.size());
    }

    // Compute node degrees
    std::map<std::string, int> degree_map;
    compute_node_degrees(entities, relations, degree_map);

    // Update entity degrees
    for (auto& entity : entities) {
        entity.degree = degree_map[entity.id];
    }

    // Compute importance scores (simple version: normalized degree)
    compute_importance_scores(entities, relations);

    // Filter by degree
    int processed = 0;
    for (auto& entity : entities) {
        if (!entity.is_valid) continue;

        // Check minimum degree
        if (entity.degree < config.min_degree) {
            entity.is_valid = false;
            entity.removal_reason = "low_degree";
            report.removed_by_degree++;
            report.level2_removed++;
            continue;
        }

        // Check importance
        if (entity.importance < config.min_importance) {
            entity.is_valid = false;
            entity.removal_reason = "low_importance";
            report.removed_by_importance++;
            report.level2_removed++;
            continue;
        }

        // Check for statistical outliers
        if (config.remove_outliers && is_statistical_outlier(entity, 3.0)) {
            entity.is_suspicious = true;  // Don't remove yet, flag for LLM validation
        }

        processed++;
        if (config.progress_callback && processed % 100 == 0) {
            config.progress_callback("Level 2: Statistical", processed, entities.size());
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        valid_ids.insert(e.id);
    }
    remove_invalid_relations(relations, valid_ids);
}

// ============================================================================
// Level 3: LLM Validation
// ============================================================================

void GraphCleaner::level3_llm_validation(
    std::vector<CleanableEntity>& entities,
    std::vector<CleanableRelation>& relations,
    const CleaningConfig& config,
    std::shared_ptr<LLMProvider> llm,
    CleaningReport& report
) {
    if (!llm) return;

    // Collect entities to validate
    std::vector<CleanableEntity*> to_validate;
    for (auto& entity : entities) {
        if (!entity.is_valid) continue;

        bool should_validate = false;
        if (config.validation_mode == "all") {
            should_validate = true;
        } else if (config.validation_mode == "suspicious") {
            should_validate = entity.is_suspicious;
        }

        if (should_validate) {
            to_validate.push_back(&entity);
        }
    }

    if (to_validate.empty()) return;

    if (config.progress_callback) {
        config.progress_callback("Level 3: LLM Validation", 0, to_validate.size());
    }

    // Batch validate
    for (size_t i = 0; i < to_validate.size(); i += config.llm_batch_size) {
        size_t batch_end = std::min(i + config.llm_batch_size, to_validate.size());
        std::vector<std::string> batch_labels;

        for (size_t j = i; j < batch_end; j++) {
            batch_labels.push_back(to_validate[j]->label);
        }

        // Validate batch
        auto results = batch_validate_entities(batch_labels, llm);

        // Apply results
        for (size_t j = 0; j < results.size() && (i + j) < to_validate.size(); j++) {
            report.validated_by_llm++;
            if (!results[j]) {
                to_validate[i + j]->is_valid = false;
                to_validate[i + j]->removal_reason = "llm_validation_failed";
                report.removed_by_llm++;
                report.level3_removed++;
            }
        }

        if (config.progress_callback) {
            config.progress_callback("Level 3: LLM Validation", batch_end, to_validate.size());
        }
    }

    // Remove invalid entities
    remove_invalid_entities(entities);

    // Remove relations with invalid entities
    std::set<std::string> valid_ids;
    for (const auto& e : entities) {
        valid_ids.insert(e.id);
    }
    remove_invalid_relations(relations, valid_ids);
}

// ============================================================================
// Helper Methods: Level 1
// ============================================================================

bool GraphCleaner::is_valid_length(const std::string& s, int min_len, int max_len) const {
    int len = s.length();
    return len >= min_len && len <= max_len;
}

bool GraphCleaner::is_stopword(const std::string& s) const {
    return stopwords_.count(s) > 0;
}

bool GraphCleaner::is_pure_number(const std::string& s) const {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::isdigit(c) || c == '.' || c == ',' || c == '-';
    });
}

bool GraphCleaner::is_pure_punctuation(const std::string& s) const {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return std::ispunct(c) || std::isspace(c);
    });
}

bool GraphCleaner::is_extraction_artifact(const std::string& s) const {
    return artifacts_.count(s) > 0;
}

bool GraphCleaner::is_single_char(const std::string& s) const {
    // Check if string has only one alphanumeric character (ignoring whitespace/punctuation)
    int alpha_count = 0;
    for (char c : s) {
        if (std::isalnum(c)) {
            alpha_count++;
            if (alpha_count > 1) return false;
        }
    }
    return alpha_count == 1;
}

// ============================================================================
// Helper Methods: Level 2
// ============================================================================

void GraphCleaner::compute_node_degrees(
    const std::vector<CleanableEntity>& entities,
    const std::vector<CleanableRelation>& relations,
    std::map<std::string, int>& degree_map
) const {
    // Initialize
    for (const auto& e : entities) {
        degree_map[e.id] = 0;
    }

    // Count degrees
    for (const auto& r : relations) {
        if (r.is_valid) {
            degree_map[r.source]++;
            degree_map[r.target]++;
        }
    }
}

void GraphCleaner::compute_importance_scores(
    std::vector<CleanableEntity>& entities,
    const std::vector<CleanableRelation>& relations
) const {
    // Simple importance: normalized degree * confidence
    int max_degree = 1;
    for (const auto& e : entities) {
        if (e.degree > max_degree) {
            max_degree = e.degree;
        }
    }

    for (auto& e : entities) {
        e.importance = (e.degree / static_cast<double>(max_degree)) * e.confidence;
    }
}

bool GraphCleaner::is_statistical_outlier(const CleanableEntity& entity, double threshold) const {
    // Simple outlier detection: flag very low importance
    return entity.importance < 0.1 && entity.degree < 2;
}

// ============================================================================
// Helper Methods: Level 3
// ============================================================================

std::vector<bool> GraphCleaner::batch_validate_entities(
    const std::vector<std::string>& labels,
    std::shared_ptr<LLMProvider> llm
) const {
    std::string prompt = build_entity_validation_prompt(labels);

    try {
        LLMResponse llm_response = llm->complete(prompt);
        if (!llm_response.success) {
            // If LLM call failed, assume all valid
            return std::vector<bool>(labels.size(), true);
        }
        std::string response = llm_response.content;

        // Parse JSON response
        auto json = nlohmann::json::parse(response);
        std::vector<bool> results;

        if (json.contains("results") && json["results"].is_array()) {
            for (const auto& result : json["results"]) {
                if (result.is_string()) {
                    std::string status = result.get<std::string>();
                    results.push_back(status == "VALID" || status == "valid");
                } else if (result.is_boolean()) {
                    results.push_back(result.get<bool>());
                }
            }
        }

        // Fallback: if parsing failed, assume all valid
        while (results.size() < labels.size()) {
            results.push_back(true);
        }

        return results;

    } catch (const std::exception& e) {
        std::cerr << "LLM validation error: " << e.what() << "\n";
        // Fallback: assume all valid
        return std::vector<bool>(labels.size(), true);
    }
}

std::vector<bool> GraphCleaner::batch_validate_relations(
    const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
    std::shared_ptr<LLMProvider> llm
) const {
    // Similar to batch_validate_entities but for relations
    // TODO: Implement if needed
    return std::vector<bool>(triples.size(), true);
}

std::string GraphCleaner::build_entity_validation_prompt(
    const std::vector<std::string>& labels
) const {
    std::stringstream ss;
    ss << "You are validating entities extracted from scientific documents.\n";
    ss << "Classify each entity as VALID or INVALID.\n\n";
    ss << "VALID: Meaningful concepts, entities, technical terms, or domain-specific terminology\n";
    ss << "INVALID: Single letters, pure numbers, extraction artifacts, nonsense, generic stopwords\n\n";
    ss << "Entities to validate:\n";

    for (size_t i = 0; i < labels.size(); i++) {
        ss << (i + 1) << ". \"" << labels[i] << "\"\n";
    }

    ss << "\nRespond in JSON format:\n";
    ss << "{\"results\": [\"VALID\", \"INVALID\", ...]}\n\n";
    ss << "Response:";

    return ss.str();
}

std::string GraphCleaner::build_relation_validation_prompt(
    const std::vector<std::tuple<std::string, std::string, std::string>>& triples
) const {
    std::stringstream ss;
    ss << "Validate if these relationships make semantic sense:\n\n";

    for (size_t i = 0; i < triples.size(); i++) {
        const auto& [source, relation, target] = triples[i];
        ss << (i + 1) << ". \"" << source << "\" --[" << relation << "]--> \"" << target << "\"\n";
    }

    ss << "\nRespond in JSON format:\n";
    ss << "{\"results\": [true, false, ...]}\n\n";
    ss << "Response:";

    return ss.str();
}

// ============================================================================
// Utility Methods
// ============================================================================

std::string GraphCleaner::to_lowercase(const std::string& s) const {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string GraphCleaner::trim(const std::string& s) const {
    if (s.empty()) return s;

    // Find first non-whitespace
    size_t start = 0;
    while (start < s.length() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }

    // Find last non-whitespace
    size_t end = s.length();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }

    return s.substr(start, end - start);
}

void GraphCleaner::remove_invalid_entities(std::vector<CleanableEntity>& entities) {
    entities.erase(
        std::remove_if(entities.begin(), entities.end(),
                      [](const CleanableEntity& e) { return !e.is_valid; }),
        entities.end()
    );
}

void GraphCleaner::remove_invalid_relations(
    std::vector<CleanableRelation>& relations,
    const std::set<std::string>& valid_entity_ids
) {
    for (auto& r : relations) {
        if (valid_entity_ids.count(r.source) == 0 || valid_entity_ids.count(r.target) == 0) {
            r.is_valid = false;
        }
    }

    relations.erase(
        std::remove_if(relations.begin(), relations.end(),
                      [](const CleanableRelation& r) { return !r.is_valid; }),
        relations.end()
    );
}

} // namespace kg

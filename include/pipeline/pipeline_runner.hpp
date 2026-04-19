#pragma once

#include "discovery/insight.hpp"
#include "graph/hypergraph.hpp"
#include "index/hypergraph_index.hpp"
#include "pipeline/graph_cleaner.hpp"
#include <map>
#include <string>
#include <vector>

namespace kg {

struct PipelineRunOptions {
    std::string input_path;
    std::string output_base = "runs/";
    std::string config_path;
    std::vector<std::string> operators;
    std::string title;
    int max_examples = 10;
    int from_stage = 1;
    std::string existing_run_dir;
    bool preprocess = false;
    bool use_causal = false;
    bool with_ontology = false;
    bool enable_qc = true;
    int min_node_length = 2;
    int min_degree = 2;
    bool llm_validate = false;
    std::string validation_mode = "suspicious";
    bool semantic_dedup = false;
    double semantic_threshold = 0.85;
};

struct RunPaths {
    std::string run_dir;
    std::string run_id;
    std::string step1_dir;
    std::string step2_dir;
    std::string step3_dir;
    std::string step4_dir;
    std::string step5_dir;
    std::string graph_path;
    std::string graph_raw_path;
    std::string index_path;
    std::string insights_path;
    std::string manifest_path;
};

struct StageContext {
    RunPaths paths;
    std::vector<std::string> pdf_paths;
    std::string title;
    std::string source_name;
    Hypergraph graph;
    HypergraphIndex index;
    InsightCollection insights;
    HypergraphStatistics graph_stats;
    CleaningReport qc_report;
    std::map<InsightType, int> insight_counts;
    size_t relations_normalized = 0;
    size_t nodes_merged = 0;
    bool preprocess_ran = false;
};

class PipelineRunner {
public:
    explicit PipelineRunner(PipelineRunOptions options);

    int run();

private:
    PipelineRunOptions options_;
    StageContext context_;

    void validate_options() const;
    void initialize_run();
    void derive_title();
    void build_source_name();

    void run_extraction_stage();
    void run_preprocess_stage();
    void run_quality_control_stage();
    void run_ontology_stage();
    void run_index_stage();
    void run_discovery_stage();
    void run_render_stage();
    void run_report_stage();
    void finalize_manifest();
    void print_summary(std::chrono::steady_clock::time_point pipeline_start) const;
};

}  // namespace kg

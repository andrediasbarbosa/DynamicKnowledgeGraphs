#include "cli/cli.hpp"
#include "graph/hypergraph.hpp"
#include "index/hypergraph_index.hpp"
#include "discovery/discovery_engine.hpp"
#include "discovery/report_generator.hpp"
#include "render/augmentation_renderer.hpp"
#include "pipeline/extraction_pipeline.hpp"
#include "llm/llm_provider.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

using namespace kg;

// ============== Helper Functions ==============

// Generate timestamp-based run ID
std::string generate_run_id() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "run_" << std::put_time(std::gmtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

// Get basename from path (without extension)
std::string get_basename(const std::string& path) {
    fs::path p(path);
    return p.stem().string();
}

// Find PDF files helper (from pipeline)
std::vector<std::string> find_pdfs(const std::string& path) {
    std::vector<std::string> pdfs;

    if (fs::is_regular_file(path)) {
        // Single file
        if (path.size() > 4 && path.substr(path.size() - 4) == ".pdf") {
            pdfs.push_back(path);
        }
    } else if (fs::is_directory(path)) {
        // Directory - find all PDFs
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().string();
                if (fname.size() > 4 && fname.substr(fname.size() - 4) == ".pdf") {
                    pdfs.push_back(fname);
                }
            }
        }
    }

    return pdfs;
}

// All available discovery operators
const std::vector<std::string> ALL_OPERATORS = {
    "bridges", "completions", "motifs", "substitutions",
    "diffusion", "surprise", "rules", "community", "pathrank", "embedding", "author_chain", "hypotheses"
};

// Expand "all" to all operators
std::vector<std::string> expand_operators(const std::vector<std::string>& ops) {
    for (const auto& op : ops) {
        if (op == "all") {
            return ALL_OPERATORS;
        }
    }
    return ops;
}

std::string format_duration(std::chrono::steady_clock::duration d) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    std::stringstream ss;
    if (ms >= 1000) {
        ss << std::fixed << std::setprecision(2) << (ms / 1000.0) << "s";
    } else {
        ss << ms << "ms";
    }
    return ss.str();
}

// ============== kg index ==============
int cmd_index(const Args& args) {
    std::string input_path = args.require("input");
    std::string output_dir = args.get("output", "index/").value;
    auto s_values = args.get("s-components", "2,3,4").as_int_list();

    std::cout << "Loading hypergraph from: " << input_path << "\n";
    Hypergraph graph = Hypergraph::load_from_json(input_path);

    auto stats = graph.compute_statistics();
    std::cout << "Loaded " << stats.num_nodes << " nodes and " << stats.num_edges << " edges\n";

    std::cout << "Building index...\n";
    std::cout << "  - Computing s-components for s = ";
    for (size_t i = 0; i < s_values.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << s_values[i];
    }
    std::cout << "\n";

    HypergraphIndex index;
    index.source_graph_path = input_path;
    index.build(graph, s_values);

    // Ensure output directory exists
    fs::create_directories(output_dir);

    std::string index_path = output_dir;
    if (index_path.back() != '/') index_path += "/";
    index_path += "hypergraph_index.json";

    std::cout << "Saving index to: " << index_path << "\n";
    index.save_to_json(index_path);

    index.print_summary();

    std::cout << "\nIndex created successfully!\n";
    return 0;
}

// ============== kg discover ==============
int cmd_discover(const Args& args) {
    std::string input_path = args.require("input");
    std::string index_path = args.get("index", "").value;
    std::string output_path = args.require("output");
    auto operators = expand_operators(args.get("operators", "bridges,completions,motifs").as_list());
    std::string run_id = args.get("run-id", "").value;

    std::cout << "Loading hypergraph from: " << input_path << "\n";
    Hypergraph graph = Hypergraph::load_from_json(input_path);

    auto stats = graph.compute_statistics();
    std::cout << "Loaded " << stats.num_nodes << " nodes and " << stats.num_edges << " edges\n";

    // Load or build index
    HypergraphIndex index;
    if (!index_path.empty() && fs::exists(index_path)) {
        // If index_path is a directory, append the default filename
        if (fs::is_directory(index_path)) {
            index_path = (fs::path(index_path) / "hypergraph_index.json").string();
        }
        std::cout << "Loading index from: " << index_path << "\n";
        index = HypergraphIndex::load_from_json(index_path);
    } else {
        std::cout << "Building index (no cached index provided)...\n";
        index.build(graph, {2, 3, 4});
    }

    std::cout << "Running discovery operators: ";
    for (size_t i = 0; i < operators.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << operators[i];
    }
    std::cout << "\n";

    DiscoveryEngine engine(graph, index);
    if (!run_id.empty()) {
        engine.set_run_id(run_id);
    }

    engine.set_progress_callback([](const std::string& stage, int current, int total) {
        std::cout << "  [" << stage << "] " << current << "/" << total << "\r" << std::flush;
    });

    InsightCollection insights = engine.run_operators(operators);
    insights.source_graph = input_path;

    // Ensure output directory exists
    fs::path out_path(output_path);
    if (out_path.has_parent_path()) {
        fs::create_directories(out_path.parent_path());
    }

    std::cout << "\nSaving " << insights.insights.size() << " insights to: " << output_path << "\n";
    insights.save_to_json(output_path);

    // Print summary
    std::cout << "\nDiscovery Summary:\n";
    std::map<std::string, int> by_type;
    for (const auto& ins : insights.insights) {
        by_type[insight_type_to_string(ins.type)]++;
    }
    for (const auto& [type, count] : by_type) {
        std::cout << "  " << type << ": " << count << "\n";
    }

    std::cout << "\nDiscovery complete!\n";
    return 0;
}

// ============== kg render ==============
int cmd_render(const Args& args) {
    std::string input_path = args.require("input");
    std::string insights_path = args.get("insights", "").value;
    std::string output_dir = args.require("output");
    std::string title = args.get("title", "Knowledge Graph").value;

    std::cout << "Loading hypergraph from: " << input_path << "\n";
    Hypergraph graph = Hypergraph::load_from_json(input_path);

    auto stats = graph.compute_statistics();
    std::cout << "Loaded " << stats.num_nodes << " nodes and " << stats.num_edges << " edges\n";

    // Ensure output directory exists
    fs::create_directories(output_dir);

    std::string out_base = output_dir;
    if (out_base.back() != '/') out_base += "/";

    // Always export baseline HTML
    std::string baseline_html = out_base + "final_graph.html";
    std::cout << "Exporting baseline HTML to: " << baseline_html << "\n";
    graph.export_to_html(baseline_html, title);

    // If insights provided, create augmented view
    if (!insights_path.empty() && fs::exists(insights_path)) {
        std::cout << "Loading insights from: " << insights_path << "\n";
        InsightCollection insights = InsightCollection::load_from_json(insights_path);
        std::cout << "Loaded " << insights.insights.size() << " insights\n";

        AugmentationRenderer renderer(graph);
        AugmentationData augmentation = renderer.convert(insights);

        // Save augmentation JSON
        std::string aug_json_path = out_base + "augmentation.json";
        std::cout << "Saving augmentation data to: " << aug_json_path << "\n";
        augmentation.save_to_json(aug_json_path);

        // Save viewer bundle (insights with metadata)
        std::string bundle_path = out_base + "viewer_bundle.json";
        std::cout << "Saving viewer bundle to: " << bundle_path << "\n";
        insights.save_to_json(bundle_path);

        // Export augmented HTML
        std::string aug_html = out_base + "final_graph_augmented.html";
        std::cout << "Exporting augmented HTML to: " << aug_html << "\n";
        renderer.export_augmented_html(aug_html, title, augmentation);

        std::cout << "\nAugmentation summary:\n";
        std::cout << "  New nodes: " << augmentation.nodes.size() << "\n";
        std::cout << "  New links: " << augmentation.links.size() << "\n";
    }

    // Write README
    std::string readme_path = out_base + "README.txt";
    std::ofstream readme(readme_path);
    readme << "Knowledge Graph Viewer\n";
    readme << "======================\n\n";
    readme << "Files:\n";
    readme << "  final_graph.html          - Baseline graph viewer\n";
    if (!insights_path.empty()) {
        readme << "  final_graph_augmented.html - Augmented view with discovery insights\n";
        readme << "  augmentation.json         - Overlay data for augmented nodes/links\n";
        readme << "  viewer_bundle.json        - Full insight data with metadata\n";
    }
    readme << "\nTo view:\n";
    readme << "  cd " << output_dir << "\n";
    readme << "  python3 -m http.server 8080\n";
    readme << "  # Open http://localhost:8080/final_graph.html in browser\n";
    readme.close();

    std::cout << "\nRender complete! See " << readme_path << " for usage instructions.\n";
    return 0;
}

// ============== kg report ==============
int cmd_report(const Args& args) {
    std::string input_path = args.require("input");
    std::string insights_path = args.require("insights");
    std::string output_path = args.require("output");
    std::string title = args.get("title", "Knowledge Discovery Report").value;
    std::string source = args.get("source", "").value;
    std::string format = args.get("format", "auto").value;
    int max_examples = args.get("max-examples", "5").as_int();

    std::cout << "Loading hypergraph from: " << input_path << "\n";
    Hypergraph graph = Hypergraph::load_from_json(input_path);

    auto stats = graph.compute_statistics();
    std::cout << "Loaded " << stats.num_nodes << " nodes and " << stats.num_edges << " edges\n";

    std::cout << "Loading insights from: " << insights_path << "\n";
    InsightCollection insights = InsightCollection::load_from_json(insights_path);
    std::cout << "Loaded " << insights.insights.size() << " insights\n";

    // Configure report
    ReportConfig config;
    config.title = title;
    config.source_document = source.empty() ? input_path : source;
    config.max_examples_per_type = max_examples;
    config.include_evidence = true;
    config.include_statistics = true;

    // Determine format from extension or explicit format
    bool use_html = false;
    if (format == "html") {
        use_html = true;
    } else if (format == "markdown" || format == "md") {
        use_html = false;
    } else {
        // Auto-detect from extension
        use_html = (output_path.size() > 5 &&
                   (output_path.substr(output_path.size() - 5) == ".html" ||
                    output_path.substr(output_path.size() - 4) == ".htm"));
    }

    config.format = use_html ? ReportFormat::HTML : ReportFormat::MARKDOWN;
    config.markdown_format = !use_html;
    config.llm_examples_per_type = 1;

    // Generate report
    std::cout << "Generating " << (use_html ? "HTML" : "Markdown") << " report...\n";
    ReportGenerator generator(graph);
    auto report_llm = LLMProviderFactory::create_from_config_file();
    if (report_llm) {
        config.include_llm_examples = true;
        generator.set_llm_provider(std::move(report_llm));
        std::cout << "LLM examples enabled for report synthesis.\n";
    }
    std::string report = use_html ? generator.generate_html(insights, config)
                                  : generator.generate(insights, config);

    // Ensure output directory exists
    fs::path out_path(output_path);
    if (out_path.has_parent_path()) {
        fs::create_directories(out_path.parent_path());
    }

    // Save report
    generator.save_to_file(output_path, report);
    std::cout << "Report saved to: " << output_path << "\n";

    // Print summary
    std::map<InsightType, int> counts;
    for (const auto& insight : insights.insights) {
        counts[insight.type]++;
    }

    std::cout << "\nReport Summary:\n";
    if (counts[InsightType::BRIDGE] > 0)
        std::cout << "  - " << counts[InsightType::BRIDGE] << " bridge entities\n";
    if (counts[InsightType::COMPLETION] > 0)
        std::cout << "  - " << counts[InsightType::COMPLETION] << " potential completions\n";
    if (counts[InsightType::MOTIF] > 0)
        std::cout << "  - " << counts[InsightType::MOTIF] << " recurring patterns\n";
    if (counts[InsightType::SUBSTITUTION] > 0)
        std::cout << "  - " << counts[InsightType::SUBSTITUTION] << " substitutions\n";
    if (counts[InsightType::SURPRISE] > 0)
        std::cout << "  - " << counts[InsightType::SURPRISE] << " surprising discoveries\n";
    if (counts[InsightType::DIFFUSION] > 0)
        std::cout << "  - " << counts[InsightType::DIFFUSION] << " influence pathways\n";
    if (counts[InsightType::PATH_RANK] > 0)
        std::cout << "  - " << counts[InsightType::PATH_RANK] << " path-ranked links\n";
    if (counts[InsightType::AUTHOR_CHAIN] > 0)
        std::cout << "  - " << counts[InsightType::AUTHOR_CHAIN] << " author reference chains\n";
    if (counts[InsightType::COMMUNITY_LINK] > 0)
        std::cout << "  - " << counts[InsightType::COMMUNITY_LINK] << " community links\n";
    if (counts[InsightType::RULE] > 0)
        std::cout << "  - " << counts[InsightType::RULE] << " association rules\n";
    if (counts[InsightType::EMBEDDING_LINK] > 0)
        std::cout << "  - " << counts[InsightType::EMBEDDING_LINK] << " embedding predictions\n";
    if (counts[InsightType::HYPOTHESIS] > 0)
        std::cout << "  - " << counts[InsightType::HYPOTHESIS] << " hypotheses\n";

    std::cout << "\nReport generation complete!\n";
    return 0;
}

// ============== kg stats ==============
int cmd_stats(const Args& args) {
    std::string input_path = args.require("input");

    std::cout << "Loading hypergraph from: " << input_path << "\n";
    Hypergraph graph = Hypergraph::load_from_json(input_path);

    auto stats = graph.compute_statistics();

    std::cout << "\nHypergraph Statistics:\n";
    std::cout << "  Nodes: " << stats.num_nodes << "\n";
    std::cout << "  Edges: " << stats.num_edges << "\n";
    std::cout << "  Avg node degree: " << stats.avg_node_degree << "\n";
    std::cout << "  Max node degree: " << stats.max_node_degree << "\n";
    std::cout << "  Avg edge size: " << stats.avg_edge_size << "\n";
    std::cout << "  Max edge size: " << stats.max_edge_size << "\n";
    std::cout << "  Duplicate edges: " << stats.num_duplicate_edges << "\n";

    // Top hubs
    auto hubs = graph.get_top_hubs(10);
    std::cout << "\nTop 10 Hubs:\n";
    for (const auto& [node_id, degree] : hubs) {
        const auto* node = graph.get_node(node_id);
        std::string label = node ? node->label : "?";
        std::cout << "  " << label << " (degree " << degree << ")\n";
    }

    return 0;
}

// ============== kg run (Full Pipeline) ==============
int cmd_run(const Args& args) {
    std::string input_path = args.get("input", "").value;
    std::string output_base = args.get("output", "runs/").value;
    std::string config_path = args.get("config", "").value;
    auto operators = expand_operators(args.get("operators", "bridges,surprise").as_list());
    std::string title = args.get("title", "").value;
    int max_examples = args.get("max-examples", "10").as_int();
    int from_stage = args.get("from-stage", "1").as_int();
    std::string existing_run_dir = args.get("run-dir", "").value;

    // Validate stage range
    if (from_stage < 1 || from_stage > 5) {
        std::cerr << "Error: --from-stage must be between 1 and 5\n";
        std::cerr << "  1 = extraction, 2 = indexing, 3 = discovery, 4 = render, 5 = report\n";
        return 1;
    }

    // If resuming from later stage, require run-dir
    if (from_stage > 1 && existing_run_dir.empty()) {
        std::cerr << "Error: --run-dir is required when --from-stage > 1\n";
        std::cerr << "  Specify the existing run directory to resume from.\n";
        return 1;
    }

    // If starting from stage 1, require input
    if (from_stage == 1 && input_path.empty()) {
        std::cerr << "Error: --input is required when starting from stage 1\n";
        return 1;
    }

    std::string run_dir;
    std::string run_id;
    std::vector<std::string> pdf_paths;

    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  Knowledge Discovery Pipeline\n";
    std::cout << "======================================================================\n";
    std::cout << "\n";

    if (from_stage > 1) {
        // Resuming from existing run
        run_dir = existing_run_dir;
        // Remove trailing slash if present
        if (!run_dir.empty() && run_dir.back() == '/') {
            run_dir.pop_back();
        }

        // Extract run_id from directory name
        run_id = fs::path(run_dir).filename().string();

        // Validate directory exists
        if (!fs::exists(run_dir)) {
            std::cerr << "Error: Run directory does not exist: " << run_dir << "\n";
            return 1;
        }

        std::cout << "Resuming run: " << run_id << "\n";
        std::cout << "From stage:   " << from_stage << " (";
        switch (from_stage) {
            case 2: std::cout << "indexing"; break;
            case 3: std::cout << "discovery"; break;
            case 4: std::cout << "render"; break;
            case 5: std::cout << "report"; break;
        }
        std::cout << ")\n";
        std::cout << "Run dir:      " << run_dir << "\n";

        // Load manifest if exists to get title and source info
        std::string manifest_path = run_dir + "/manifest.json";
        if (fs::exists(manifest_path)) {
            std::ifstream mf(manifest_path);
            nlohmann::json manifest = nlohmann::json::parse(mf);
            if (title.empty() && manifest.contains("title")) {
                title = manifest["title"].get<std::string>();
            }
            if (manifest.contains("sources")) {
                for (const auto& src : manifest["sources"]) {
                    pdf_paths.push_back(src.get<std::string>());
                }
            }
        }
    } else {
        // Fresh run from stage 1
        run_id = generate_run_id();

        std::cout << "Run ID: " << run_id << "\n";
        std::cout << "Input:  " << input_path << "\n";

        // Find PDF files
        pdf_paths = find_pdfs(input_path);
        if (pdf_paths.empty()) {
            std::cerr << "Error: No PDF files found at: " << input_path << "\n";
            return 1;
        }

        std::cout << "Found " << pdf_paths.size() << " PDF file(s):\n";
        for (const auto& p : pdf_paths) {
            std::cout << "  - " << p << "\n";
        }

        // Create run output directory
        run_dir = output_base;
        if (run_dir.back() != '/') run_dir += "/";
        run_dir += run_id;
        fs::create_directories(run_dir);

        std::cout << "\nOutput: " << run_dir << "/\n";
    }

    // Derive title from first PDF if not specified
    if (title.empty() && !pdf_paths.empty()) {
        title = get_basename(pdf_paths[0]) + " Knowledge Discovery";
    } else if (title.empty()) {
        title = "Knowledge Discovery";
    }

    // Define paths for all artifacts
    std::string graph_path = run_dir + "/graph.json";
    std::string index_path = run_dir + "/index.json";
    std::string insights_path = run_dir + "/insights.json";

    // Declare variables used across stages
    Hypergraph graph;
    HypergraphIndex index;
    InsightCollection insights;
    HypergraphStatistics graph_stats;

    // =========================================================================
    // Stage 1: Extract Knowledge Graph
    // =========================================================================
    auto stage1_start = std::chrono::steady_clock::now();
    if (from_stage <= 1) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 1: Knowledge Extraction\n";
        std::cout << "----------------------------------------------------------------------\n";

        // Load pipeline config
        PipelineConfig pipeline_config;
        if (!config_path.empty() && fs::exists(config_path)) {
            std::cout << "Loading config from: " << config_path << "\n";
            pipeline_config = PipelineConfig::from_json_file(config_path);
        } else {
            std::cout << "Loading config from .llm_config.json or environment...\n";
            pipeline_config = load_config_with_fallback("");
        }

        // Override output directory to our run folder
        pipeline_config.output_directory = run_dir;
        pipeline_config.save_intermediate = true;
        pipeline_config.save_extractions = true;

        // Validate config
        std::string config_error;
        if (!pipeline_config.validate(config_error)) {
            std::cerr << "Configuration error: " << config_error << "\n";
            std::cerr << "Please set up .llm_config.json or environment variables.\n";
            return 1;
        }

        std::cout << "  Provider: " << pipeline_config.llm_provider << "\n";
        std::cout << "  Model:    " << pipeline_config.llm_model << "\n";

        // Run extraction pipeline
        ExtractionPipeline pipeline(pipeline_config);
        pipeline.set_progress_callback([](const std::string& stage, int current, int total, const std::string& msg) {
            std::cout << "  [" << stage << "] " << current << "/" << total;
            if (!msg.empty()) std::cout << " - " << msg;
            std::cout << "\r" << std::flush;
        });

        try {
            graph = pipeline.process_pdfs(pdf_paths);
        } catch (const std::exception& e) {
            std::cerr << "\nExtraction error: " << e.what() << "\n";
            return 1;
        }

        graph_stats = graph.compute_statistics();
        std::cout << "\n  Extracted: " << graph_stats.num_nodes << " entities, "
                  << graph_stats.num_edges << " relationships\n";

        // Save graph
        graph.export_to_json(graph_path, true);
        std::cout << "  Saved: graph.json\n";

        // Save pipeline stats
        auto pipeline_stats = pipeline.get_statistics();
        std::string stats_path = run_dir + "/extraction_stats.json";
        std::ofstream stats_file(stats_path);
        stats_file << pipeline_stats.to_json().dump(2);
        stats_file.close();
        std::cout << "  Saved: extraction_stats.json\n";
    } else {
        // Load existing graph
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 1: Knowledge Extraction [SKIPPED - loading existing]\n";
        std::cout << "----------------------------------------------------------------------\n";

        if (!fs::exists(graph_path)) {
            std::cerr << "Error: Required file not found: " << graph_path << "\n";
            return 1;
        }

        std::cout << "  Loading: graph.json\n";
        graph = Hypergraph::load_from_json(graph_path);
        graph_stats = graph.compute_statistics();
        std::cout << "  Loaded: " << graph_stats.num_nodes << " entities, "
                  << graph_stats.num_edges << " relationships\n";
    }
    std::cout << "  Stage 1 time: " << format_duration(std::chrono::steady_clock::now() - stage1_start) << "\n";

    // =========================================================================
    // Stage 2: Build Index
    // =========================================================================
    auto stage2_start = std::chrono::steady_clock::now();
    if (from_stage <= 2) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 2: Building Index\n";
        std::cout << "----------------------------------------------------------------------\n";

        index.source_graph_path = graph_path;
        index.build(graph, {2, 3, 4});

        index.save_to_json(index_path);
        std::cout << "  S-components computed for s = 2, 3, 4\n";
        std::cout << "  Saved: index.json\n";
    } else {
        // Load existing index
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 2: Building Index [SKIPPED - loading existing]\n";
        std::cout << "----------------------------------------------------------------------\n";

        if (!fs::exists(index_path)) {
            std::cerr << "Error: Required file not found: " << index_path << "\n";
            return 1;
        }

        std::cout << "  Loading: index.json\n";
        index = HypergraphIndex::load_from_json(index_path);
        std::cout << "  Loaded index with " << index.s_components.size() << " s-component sets\n";
    }
    std::cout << "  Stage 2 time: " << format_duration(std::chrono::steady_clock::now() - stage2_start) << "\n";

    // =========================================================================
    // Stage 3: Run Discovery
    // =========================================================================
    std::map<InsightType, int> insight_counts;

    auto stage3_start = std::chrono::steady_clock::now();
    if (from_stage <= 3) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 3: Knowledge Discovery\n";
        std::cout << "----------------------------------------------------------------------\n";

        std::cout << "  Operators: ";
        for (size_t i = 0; i < operators.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << operators[i];
        }
        std::cout << "\n";

        DiscoveryEngine engine(graph, index);
        engine.set_run_id(run_id);
        engine.set_progress_callback([](const std::string& stage, int current, int total) {
            std::cout << "  [" << stage << "] " << current << "/" << total << "\r" << std::flush;
        });

        insights = engine.run_operators(operators);
        insights.source_graph = graph_path;

        insights.save_to_json(insights_path);

        // Count by type
        for (const auto& ins : insights.insights) {
            insight_counts[ins.type]++;
        }

        std::cout << "\n  Discovered " << insights.insights.size() << " insights:\n";
        for (const auto& [type, count] : insight_counts) {
            std::cout << "    - " << insight_type_to_string(type) << ": " << count << "\n";
        }
        std::cout << "  Saved: insights.json\n";
    } else {
        // Load existing insights
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 3: Knowledge Discovery [SKIPPED - loading existing]\n";
        std::cout << "----------------------------------------------------------------------\n";

        if (!fs::exists(insights_path)) {
            std::cerr << "Error: Required file not found: " << insights_path << "\n";
            return 1;
        }

        std::cout << "  Loading: insights.json\n";
        insights = InsightCollection::load_from_json(insights_path);

        // Count by type
        for (const auto& ins : insights.insights) {
            insight_counts[ins.type]++;
        }

        std::cout << "  Loaded " << insights.insights.size() << " insights\n";
    }
    std::cout << "  Stage 3 time: " << format_duration(std::chrono::steady_clock::now() - stage3_start) << "\n";

    // =========================================================================
    // Stage 4: Generate Visualizations
    // =========================================================================
    AugmentationData augmentation;

    auto stage4_start = std::chrono::steady_clock::now();
    if (from_stage <= 4) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 4: Generating Visualizations\n";
        std::cout << "----------------------------------------------------------------------\n";

        // Baseline HTML
        std::string baseline_html = run_dir + "/graph.html";
        graph.export_to_html(baseline_html, title);
        std::cout << "  Saved: graph.html (baseline viewer)\n";

        // Augmented HTML with insights
        AugmentationRenderer renderer(graph);
        augmentation = renderer.convert(insights);

        std::string aug_json = run_dir + "/augmentation.json";
        augmentation.save_to_json(aug_json);
        std::cout << "  Saved: augmentation.json\n";

        std::string aug_html = run_dir + "/graph_augmented.html";
        renderer.export_augmented_html(aug_html, title, augmentation);
        std::cout << "  Saved: graph_augmented.html (with " << augmentation.nodes.size()
                  << " augmented nodes)\n";


        // DOT visualization
        std::string dot_path = run_dir + "/graph.dot";
        graph.export_to_dot(dot_path);
        std::cout << "  Saved: graph.dot\n";
    } else {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 4: Generating Visualizations [SKIPPED]\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Using existing visualization files\n";
    }
    std::cout << "  Stage 4 time: " << format_duration(std::chrono::steady_clock::now() - stage4_start) << "\n";

    // =========================================================================
    // Stage 5: Generate Reports
    // =========================================================================
    // Get source document name (needed for manifest even if skipping reports)
    std::string source_name;
    for (const auto& p : pdf_paths) {
        if (!source_name.empty()) source_name += ", ";
        source_name += get_basename(p) + ".pdf";
    }

    auto stage5_start = std::chrono::steady_clock::now();
    if (from_stage <= 5) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 5: Generating Reports\n";
        std::cout << "----------------------------------------------------------------------\n";

        ReportConfig report_config;
        report_config.title = title;
        report_config.source_document = source_name;
        report_config.max_examples_per_type = max_examples;
        report_config.include_evidence = true;
        report_config.include_statistics = true;
        report_config.llm_examples_per_type = 1;

        ReportGenerator report_gen(graph);
        auto report_llm = LLMProviderFactory::create_from_config_file();
        if (report_llm) {
            report_config.include_llm_examples = true;
            report_gen.set_llm_provider(std::move(report_llm));
            std::cout << "  LLM examples enabled for report synthesis.\n";
        }

        // Markdown report
        report_config.format = ReportFormat::MARKDOWN;
        std::string md_report = report_gen.generate(insights, report_config);
        std::string md_path = run_dir + "/report.md";
        report_gen.save_to_file(md_path, md_report);
        std::cout << "  Saved: report.md\n";

        // HTML report
        report_config.format = ReportFormat::HTML;
        std::string html_report = report_gen.generate_html(insights, report_config);
        std::string html_path = run_dir + "/report.html";
        report_gen.save_to_file(html_path, html_report);
        std::cout << "  Saved: report.html\n";
    } else {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 5: Generating Reports [SKIPPED]\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Using existing report files\n";
    }
    std::cout << "  Stage 5 time: " << format_duration(std::chrono::steady_clock::now() - stage5_start) << "\n";

    auto stage6_start = std::chrono::steady_clock::now();
    // =========================================================================
    // Stage 6: Create Manifest
    // =========================================================================
    std::cout << "\n";
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "  Stage 6: Finalizing\n";
    std::cout << "----------------------------------------------------------------------\n";

    // Create or update manifest JSON
    nlohmann::json manifest;
    std::string manifest_path = run_dir + "/manifest.json";

    // Load existing manifest if resuming
    if (from_stage > 1 && fs::exists(manifest_path)) {
        std::ifstream existing_mf(manifest_path);
        manifest = nlohmann::json::parse(existing_mf);
    }

    manifest["run_id"] = run_id;
    manifest["last_updated_utc"] = []() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S UTC");
        return ss.str();
    }();

    // Set created time only if new run
    if (from_stage == 1 || !manifest.contains("created_utc")) {
        manifest["created_utc"] = manifest["last_updated_utc"];
    }

    manifest["title"] = title;

    // Track completed stages
    manifest["pipeline"]["from_stage"] = from_stage;
    manifest["pipeline"]["completed_stages"] = nlohmann::json::array({1, 2, 3, 4, 5, 6});

    // Sources
    manifest["sources"] = nlohmann::json::array();
    for (const auto& p : pdf_paths) {
        manifest["sources"].push_back(p);
    }

    // Statistics
    manifest["statistics"]["entities"] = graph_stats.num_nodes;
    manifest["statistics"]["relationships"] = graph_stats.num_edges;
    manifest["statistics"]["insights"] = insights.insights.size();
    for (const auto& [type, count] : insight_counts) {
        manifest["statistics"]["insights_by_type"][insight_type_to_string(type)] = count;
    }

    // Artifacts
    manifest["artifacts"]["graph"] = "graph.json";
    manifest["artifacts"]["index"] = "index.json";
    manifest["artifacts"]["insights"] = "insights.json";
    manifest["artifacts"]["augmentation"] = "augmentation.json";
    manifest["artifacts"]["visualizations"]["baseline"] = "graph.html";
    manifest["artifacts"]["visualizations"]["augmented"] = "graph_augmented.html";
    manifest["artifacts"]["visualizations"]["dot"] = "graph.dot";
    manifest["artifacts"]["reports"]["markdown"] = "report.md";
    manifest["artifacts"]["reports"]["html"] = "report.html";
    manifest["artifacts"]["extraction_stats"] = "extraction_stats.json";

    std::ofstream manifest_file(manifest_path);
    manifest_file << manifest.dump(2);
    manifest_file.close();
    std::cout << "  Saved: manifest.json\n";

    // Create README
    std::ofstream readme(run_dir + "/README.txt");
    readme << "Knowledge Discovery Run: " << run_id << "\n";
    readme << std::string(50, '=') << "\n\n";
    readme << "Title: " << title << "\n";
    readme << "Sources: " << source_name << "\n\n";
    readme << "Artifacts:\n";
    readme << "  Data:\n";
    readme << "    graph.json           - Extracted knowledge graph\n";
    readme << "    index.json           - S-component index\n";
    readme << "    insights.json        - Discovered insights\n";
    readme << "    augmentation.json    - Augmentation overlay data\n";
    readme << "    extraction_stats.json - Pipeline statistics\n";
    readme << "    manifest.json        - Run metadata\n";
    readme << "\n";
    readme << "  Visualizations:\n";
    readme << "    graph.html           - Interactive 3D graph viewer\n";
    readme << "    graph_augmented.html - Augmented view with insights\n";
    readme << "    graph.dot            - GraphViz DOT format\n";
    readme << "\n";
    readme << "  Reports:\n";
    readme << "    report.md            - Markdown report\n";
    readme << "    report.html          - Styled HTML report\n";
    readme << "\n";
    readme << "To view:\n";
    readme << "  cd " << run_dir << "\n";
    readme << "  python3 -m http.server 8080\n";
    readme << "  # Open http://localhost:8080/graph_augmented.html\n";
    readme << "  # Or open http://localhost:8080/report.html\n";
    readme.close();
    std::cout << "  Saved: README.txt\n";
    std::cout << "  Stage 6 time: " << format_duration(std::chrono::steady_clock::now() - stage6_start) << "\n";

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  Pipeline Complete!\n";
    std::cout << "======================================================================\n";
    std::cout << "\n";
    std::cout << "Run ID:       " << run_id << "\n";
    std::cout << "Output:       " << run_dir << "/\n";
    std::cout << "Entities:     " << graph_stats.num_nodes << "\n";
    std::cout << "Relations:    " << graph_stats.num_edges << "\n";
    std::cout << "Insights:     " << insights.insights.size() << "\n";
    std::cout << "\n";
    std::cout << "To view results:\n";
    std::cout << "  cd " << run_dir << " && python3 -m http.server 8080\n";
    std::cout << "  Open: http://localhost:8080/graph_augmented.html\n";
    std::cout << "  Or:   http://localhost:8080/report.html\n";
    std::cout << "\n";

    return 0;
}

// ============== Main ==============
int main(int argc, char** argv) {
    CLI cli("kg", "1.0.0");

    // kg index
    cli.register_command({
        "index",
        "Build indices and caches for a hypergraph",
        {
            {"input", "i", "Input hypergraph JSON file", "", true, false},
            {"output", "o", "Output directory for index files", "index/", false, false},
            {"s-components", "s", "Comma-separated s-values for s-components", "2,3,4", false, false}
        },
        cmd_index
    });

    // kg discover
    cli.register_command({
        "discover",
        "Run discovery operators to find insights",
        {
            {"input", "i", "Input hypergraph JSON file", "", true, false},
            {"index", "x", "Index directory (optional, will build if not provided)", "", false, false},
            {"output", "o", "Output path for insights JSON", "", true, false},
            {"operators", "p", "Operators: bridges,completions,motifs,substitutions,diffusion,surprise,rules,community,pathrank,embedding,author_chain,hypotheses (or 'all')", "bridges,completions,motifs", false, false},
            {"run-id", "r", "Run ID for tracking", "", false, false}
        },
        cmd_discover
    });

    // kg render
    cli.register_command({
        "render",
        "Export graph visualization with optional augmentation",
        {
            {"input", "i", "Input hypergraph JSON file", "", true, false},
            {"insights", "n", "Insights JSON file (optional, for augmented view)", "", false, false},
            {"output", "o", "Output directory for HTML and JSON files", "", true, false},
            {"title", "t", "Title for the visualization", "Knowledge Graph", false, false}
        },
        cmd_render
    });

    // kg stats
    cli.register_command({
        "stats",
        "Print statistics about a hypergraph",
        {
            {"input", "i", "Input hypergraph JSON file", "", true, false}
        },
        cmd_stats
    });

    // kg report
    cli.register_command({
        "report",
        "Generate a natural language report from insights",
        {
            {"input", "i", "Input hypergraph JSON file", "", true, false},
            {"insights", "n", "Insights JSON file", "", true, false},
            {"output", "o", "Output path for report (.md or .html)", "", true, false},
            {"format", "f", "Output format: auto, markdown, html (default: auto from extension)", "auto", false, false},
            {"title", "t", "Report title", "Knowledge Discovery Report", false, false},
            {"source", "s", "Source document name", "", false, false},
            {"max-examples", "m", "Max examples per insight type", "5", false, false}
        },
        cmd_report
    });

    // kg run (full pipeline)
    cli.register_command({
        "run",
        "Run full pipeline: extract -> index -> discover -> render -> report",
        {
            {"input", "i", "Input PDF file or directory containing PDFs", "", false, false},
            {"output", "o", "Base output directory (run folder will be created inside)", "runs/", false, false},
            {"config", "c", "Path to LLM config file (optional)", "", false, false},
            {"operators", "p", "Discovery operators (e.g., bridges,diffusion,surprise,community,pathrank,embedding,author_chain,hypotheses or 'all')", "bridges,surprise", false, false},
            {"title", "t", "Title for reports and visualizations", "", false, false},
            {"max-examples", "m", "Max examples per insight type in reports", "10", false, false},
            {"from-stage", "f", "Start from stage (1=extract, 2=index, 3=discover, 4=render, 5=report)", "1", false, false},
            {"run-dir", "d", "Existing run directory to resume (required if from-stage > 1)", "", false, false}
        },
        cmd_run
    });

    return cli.run(argc, argv);
}

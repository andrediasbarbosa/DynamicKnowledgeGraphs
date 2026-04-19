#include "cli/cli.hpp"
#include "graph/hypergraph.hpp"
#include "index/hypergraph_index.hpp"
#include "discovery/discovery_engine.hpp"
#include "discovery/report_generator.hpp"
#include "render/augmentation_renderer.hpp"
#include "discovery/operator_registry.hpp"
#include "pipeline/extraction_pipeline.hpp"
#include "pipeline/graph_cleaner.hpp"
#include "pipeline/pipeline_runner.hpp"
#include "llm/llm_provider.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cctype>

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

std::string join_operators(const std::vector<std::string>& ops) {
    std::ostringstream ss;
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i > 0) ss << ",";
        ss << ops[i];
    }
    return ss.str();
}

std::string build_operator_help(const std::string& prefix) {
    std::ostringstream ss;
    ss << prefix << join_operators(kg::all_discovery_operators())
       << " (or 'all'); legacy: constrained_rule";
    return ss.str();
}

// Expand "all" to all operators
std::vector<std::string> expand_operators(const std::vector<std::string>& ops) {
    for (const auto& op : ops) {
        if (op == "all") {
            return kg::all_discovery_operators();
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

struct PreprocessStats {
    size_t relations_normalized = 0;
    size_t nodes_merged = 0;
};

std::string normalize_relation_label(const std::string& relation) {
    std::string lower;
    lower.reserve(relation.size());
    for (unsigned char c : relation) {
        if (std::isalnum(c) || c == ' ') {
            lower.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '_' || c == '-') {
            lower.push_back(' ');
        }
    }
    // collapse spaces
    std::string collapsed;
    bool last_space = false;
    for (char c : lower) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_space) collapsed.push_back(' ');
            last_space = true;
        } else {
            collapsed.push_back(c);
            last_space = false;
        }
    }
    if (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
    if (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();

    static const std::unordered_map<std::string, std::string> synonym_map = {
        {"uses", "uses"},
        {"utilizes", "uses"},
        {"employs", "uses"},
        {"applies", "uses"},
        {"leverages", "uses"},
        {"is a", "is_a"},
        {"is an", "is_a"},
        {"type of", "is_a"},
        {"kind of", "is_a"},
        {"part of", "part_of"},
        {"component of", "part_of"},
        {"belongs to", "part_of"},
        {"causes", "causes"},
        {"leads to", "causes"},
        {"results in", "causes"},
        {"induces", "causes"},
        {"affects", "affects"},
        {"influences", "affects"},
        {"impacts", "affects"},
        {"associated with", "related_to"},
        {"related to", "related_to"},
        {"linked to", "related_to"},
        {"connects to", "related_to"},
        {"requires", "requires"},
        {"needs", "requires"},
        {"depends on", "requires"},
        {"produces", "produces"},
        {"yields", "produces"},
        {"generates", "produces"},
        {"creates", "produces"},
        {"improves", "improves"},
        {"enhances", "improves"},
        {"increases", "improves"},
        {"reduces", "reduces"},
        {"decreases", "reduces"},
        {"lowers", "reduces"}
    };

    auto it = synonym_map.find(collapsed);
    if (it != synonym_map.end()) return it->second;
    return collapsed.empty() ? relation : collapsed;
}

void normalize_relations(Hypergraph& graph, PreprocessStats& stats) {
    auto edges = graph.get_all_edges();
    for (const auto& edge : edges) {
        auto* mutable_edge = graph.get_hyperedge(edge.id);
        if (!mutable_edge) continue;
        std::string canonical = normalize_relation_label(mutable_edge->relation);
        if (!canonical.empty() && canonical != mutable_edge->relation) {
            mutable_edge->properties["original_relation"] = mutable_edge->relation;
            mutable_edge->relation = canonical;
            stats.relations_normalized++;
        }
    }
}

std::string normalize_label_key_simple(const std::string& label) {
    // Lowercase and convert separators/punctuation to spaces.
    std::string out;
    out.reserve(label.size());
    for (unsigned char c : label) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '_' || c == '-' || c == '/' || c == '\\') {
            out.push_back(' ');
        } else {
            // Treat any other punctuation as a space
            out.push_back(' ');
        }
    }
    std::string collapsed;
    bool last_space = false;
    for (char c : out) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_space) collapsed.push_back(' ');
            last_space = true;
        } else {
            collapsed.push_back(c);
            last_space = false;
        }
    }
    if (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
    if (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();

    // Strip leading/trailing punctuation-like remnants
    auto strip_edge_punct = [](std::string& s) {
        auto is_punct_like = [](unsigned char ch) {
            return !std::isalnum(ch) && !std::isspace(ch);
        };
        while (!s.empty() && is_punct_like(static_cast<unsigned char>(s.front()))) {
            s.erase(s.begin());
        }
        while (!s.empty() && is_punct_like(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
    };
    strip_edge_punct(collapsed);

    return collapsed;
}

void merge_aliases(Hypergraph& graph, PreprocessStats& stats) {
    HypergraphIndex dummy_index;
    DiscoveryEngine engine(graph, dummy_index);
    DiscoveryConfig cfg;
    cfg.entity_resolution_min_label_similarity = 0.9;
    cfg.entity_resolution_min_neighbor_overlap = 0.1;
    cfg.entity_resolution_max_candidates = 500;
    engine.set_config(cfg);

    auto candidates = engine.find_entity_resolutions();
    std::unordered_set<std::string> merged;
    for (const auto& ins : candidates) {
        if (ins.seed_nodes.size() < 2) continue;
        std::string a = ins.seed_nodes[0];
        std::string b = ins.seed_nodes[1];
        std::string key = a < b ? a + "|" + b : b + "|" + a;
        if (merged.find(key) != merged.end()) continue;
        merged.insert(key);

        const auto* na = graph.get_node(a);
        const auto* nb = graph.get_node(b);
        if (!na || !nb) continue;

        std::string keep = (na->degree >= nb->degree) ? a : b;
        std::string remove = (keep == a) ? b : a;
        graph.merge_nodes(keep, remove);
        stats.nodes_merged++;
    }

    // Fallback: exact normalized label matches
    auto nodes = graph.get_all_nodes();
    std::unordered_map<std::string, std::vector<std::string>> by_norm;
    for (const auto& node : nodes) {
        std::string norm = normalize_label_key_simple(node.label);
        if (!norm.empty()) by_norm[norm].push_back(node.id);
    }
    for (auto& [norm, ids] : by_norm) {
        if (ids.size() < 2) continue;
        std::string keep = ids[0];
        for (size_t i = 1; i < ids.size(); ++i) {
            graph.merge_nodes(keep, ids[i]);
            stats.nodes_merged++;
        }
    }
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

    // Phase 2: Parse causal filtering options
    CausalFilterConfig causal_filter;
    if (args.has("causal-strength")) {
        causal_filter.strengths = args.get("causal-strength", "").as_list();
    }
    if (args.has("causal-type")) {
        causal_filter.types = args.get("causal-type", "").as_list();
    }
    if (args.has("mechanism-type")) {
        causal_filter.mechanism_types = args.get("mechanism-type", "").as_list();
    }
    if (args.has("temporality")) {
        causal_filter.temporalities = args.get("temporality", "").as_list();
    }

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

    // Phase 2: Display active causal filters
    if (causal_filter.has_filters()) {
        std::cout << "Causal filters active:\n";
        if (!causal_filter.strengths.empty()) {
            std::cout << "  Strength: ";
            for (size_t i = 0; i < causal_filter.strengths.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << causal_filter.strengths[i];
            }
            std::cout << "\n";
        }
        if (!causal_filter.types.empty()) {
            std::cout << "  Type: ";
            for (size_t i = 0; i < causal_filter.types.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << causal_filter.types[i];
            }
            std::cout << "\n";
        }
        if (!causal_filter.mechanism_types.empty()) {
            std::cout << "  Mechanism: ";
            for (size_t i = 0; i < causal_filter.mechanism_types.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << causal_filter.mechanism_types[i];
            }
            std::cout << "\n";
        }
        if (!causal_filter.temporalities.empty()) {
            std::cout << "  Temporality: ";
            for (size_t i = 0; i < causal_filter.temporalities.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << causal_filter.temporalities[i];
            }
            std::cout << "\n";
        }
    }

    DiscoveryEngine engine(graph, index);
    if (!run_id.empty()) {
        engine.set_run_id(run_id);
    }
    auto discovery_llm = LLMProviderFactory::create_from_config_file();
    if (discovery_llm) {
        engine.set_llm_provider(std::shared_ptr<LLMProvider>(std::move(discovery_llm)));
    }

    // Phase 2: Set causal filter
    engine.set_causal_filter(causal_filter);

    engine.set_progress_callback([](const std::string& stage, int current, int total) {
        std::cout << "  [" << stage << "] " << current << "/" << total << "\r" << std::flush;
    });

    InsightCollection insights = engine.run_operators(operators);

    // Phase 2: Apply causal filtering
    int original_count = insights.insights.size();
    insights.insights = engine.apply_causal_filter(insights.insights);
    int filtered_count = insights.insights.size();

    if (causal_filter.has_filters() && original_count != filtered_count) {
        std::cout << "\nFiltered: " << original_count << " → " << filtered_count
                  << " insights (" << (original_count - filtered_count) << " removed)\n";
    }

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

    // Load LLM config once (used for Graph-RAG embedding)
    std::string rag_provider, rag_key, rag_model, rag_base;
    {
        std::string cfg_file = ".llm_config.json";
        if (fs::exists(cfg_file)) {
            try {
                std::ifstream f(cfg_file);
                auto j = nlohmann::json::parse(f);
                rag_provider = j.value("provider", "openai");
                rag_key      = j.value("api_key",  "");
                rag_model    = j.value("model",     "gpt-4o-mini");
            } catch (...) {}
        }
    }

    // If insights provided, create augmented views
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

        // Graph-RAG viewer with insight nodes included
        std::string rag_html = out_base + "final_graph_rag.html";
        std::cout << "Exporting Graph-RAG HTML (with " << augmentation.nodes.size()
                  << " insight nodes) to: " << rag_html << "\n";
        graph.export_to_html_rag(rag_html, title, rag_provider, rag_key, rag_model,
                                 rag_base, augmentation.to_json());

        std::cout << "\nAugmentation summary:\n";
        std::cout << "  New nodes: " << augmentation.nodes.size() << "\n";
        std::cout << "  New links: " << augmentation.links.size() << "\n";
    } else {
        // No insights — export Graph-RAG with base graph only
        std::string rag_html = out_base + "final_graph_rag.html";
        std::cout << "Exporting Graph-RAG HTML (base graph only) to: " << rag_html << "\n";
        graph.export_to_html_rag(rag_html, title, rag_provider, rag_key, rag_model, rag_base);
    }

    // Write README
    std::string readme_path = out_base + "README.txt";
    std::ofstream readme(readme_path);
    readme << "Knowledge Graph Viewer\n";
    readme << "======================\n\n";
    readme << "Files:\n";
    readme << "  final_graph.html          - Baseline graph viewer\n";
    readme << "  final_graph_rag.html      - Graph-RAG viewer with LLM chat window\n";
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

    // V2: Load pipeline statistics if available (from Step_2_Extraction/)
    fs::path insights_file(insights_path);
    fs::path run_dir = insights_file.parent_path().parent_path();  // Go up from Step_5_Discovery to run dir
    fs::path stats_file_path = run_dir / "Step_2_Extraction" / "extraction_stats.json";
    std::cout << "[DEBUG] Looking for stats at: " << stats_file_path << "\n";
    if (fs::exists(stats_file_path)) {
        try {
            std::ifstream stats_file(stats_file_path);
            stats_file >> config.pipeline_stats;
            std::cout << "✓ Loaded pipeline statistics from Step_2_Extraction/\n";
            std::cout << "  Relations before dedup: " << config.pipeline_stats.value("relations_before_dedup", 0) << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not load pipeline statistics: " << e.what() << "\n";
        }
    } else {
        std::cout << "[DEBUG] Stats file does not exist at expected path\n";
    }
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
    if (counts[InsightType::META_PATH_PATTERN] > 0)
        std::cout << "  - " << counts[InsightType::META_PATH_PATTERN] << " meta-path patterns\n";
    if (counts[InsightType::COMMUNITY_LINK] > 0)
        std::cout << "  - " << counts[InsightType::COMMUNITY_LINK] << " community links\n";
    if (counts[InsightType::CENTRALITY] > 0)
        std::cout << "  - " << counts[InsightType::CENTRALITY] << " centrality findings\n";
    if (counts[InsightType::COMMUNITY_DETECTION] > 0)
        std::cout << "  - " << counts[InsightType::COMMUNITY_DETECTION] << " community clusters\n";
    if (counts[InsightType::K_CORE] > 0)
        std::cout << "  - " << counts[InsightType::K_CORE] << " k-core nodes\n";
    if (counts[InsightType::K_TRUSS] > 0)
        std::cout << "  - " << counts[InsightType::K_TRUSS] << " k-truss edges\n";
    if (counts[InsightType::CLAIM_STANCE] > 0)
        std::cout << "  - " << counts[InsightType::CLAIM_STANCE] << " claim stance insights\n";
    if (counts[InsightType::RELATION_INDUCTION] > 0)
        std::cout << "  - " << counts[InsightType::RELATION_INDUCTION] << " relation inductions\n";
    if (counts[InsightType::ANALOGICAL_TRANSFER] > 0)
        std::cout << "  - " << counts[InsightType::ANALOGICAL_TRANSFER] << " analogical transfers\n";
    if (counts[InsightType::UNCERTAINTY_SAMPLING] > 0)
        std::cout << "  - " << counts[InsightType::UNCERTAINTY_SAMPLING] << " uncertainty samples\n";
    if (counts[InsightType::COUNTERFACTUAL] > 0)
        std::cout << "  - " << counts[InsightType::COUNTERFACTUAL] << " counterfactual probes\n";
    if (counts[InsightType::HYPEREDGE_PREDICTION] > 0)
        std::cout << "  - " << counts[InsightType::HYPEREDGE_PREDICTION] << " hyperedge predictions\n";
    if (counts[InsightType::RULE] > 0)
        std::cout << "  - " << counts[InsightType::RULE] << " association rules\n";
    if (counts[InsightType::EMBEDDING_LINK] > 0)
        std::cout << "  - " << counts[InsightType::EMBEDDING_LINK] << " embedding predictions\n";
    if (counts[InsightType::HYPOTHESES_1] > 0)
        std::cout << "  - " << counts[InsightType::HYPOTHESES_1] << " hypotheses (v1)\n";
    if (counts[InsightType::HYPOTHESES_2] > 0)
        std::cout << "  - " << counts[InsightType::HYPOTHESES_2] << " mechanistic hypotheses (v2)\n";
    if (counts[InsightType::MECHANISM_CONSOLIDATION] > 0)
        std::cout << "  - " << counts[InsightType::MECHANISM_CONSOLIDATION] << " mechanism consolidations\n";
    if (counts[InsightType::EVIDENCE_FUSION_LINK] > 0)
        std::cout << "  - " << counts[InsightType::EVIDENCE_FUSION_LINK] << " evidence-fusion links\n";
    if (counts[InsightType::META_PATH_ANOMALY] > 0)
        std::cout << "  - " << counts[InsightType::META_PATH_ANOMALY] << " meta-path anomalies\n";
    if (counts[InsightType::INTERVENTION_BOTTLENECK] > 0)
        std::cout << "  - " << counts[InsightType::INTERVENTION_BOTTLENECK] << " intervention bottlenecks\n";
    if (counts[InsightType::COMPETING_MECHANISM] > 0)
        std::cout << "  - " << counts[InsightType::COMPETING_MECHANISM] << " competing mechanisms\n";
    if (counts[InsightType::SCHEMA_REPAIR] > 0)
        std::cout << "  - " << counts[InsightType::SCHEMA_REPAIR] << " schema repairs\n";
    if (counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] > 0)
        std::cout << "  - " << counts[InsightType::CROSS_COMMUNITY_MECHANISM_BRIDGE] << " cross-community bridges\n";

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

// ============== kg list-operators ==============
int cmd_list_operators(const Args& /*args*/) {
    const auto& ops = kg::all_discovery_operators();
    std::cout << "Discovery operators (" << ops.size() << "):\n";
    for (const auto& op : ops) {
        std::cout << "  - " << op << "\n";
    }
    std::cout << "\nLegacy/experimental (not in list): constrained_rule\n";
    return 0;
}

// ============== kg run (Full Pipeline) ==============
int cmd_run(const Args& args) {
    PipelineRunOptions options;
    options.input_path = args.get("input", "").value;
    options.output_base = args.get("output", "runs/").value;
    options.config_path = args.get("config", "").value;
    options.operators = expand_operators(args.get("operators", "all").as_list());
    options.title = args.get("title", "").value;
    options.max_examples = args.get("max-examples", "10").as_int();
    options.from_stage = args.get("from-stage", "1").as_int();
    options.existing_run_dir = args.get("run-dir", "").value;
    options.preprocess = args.has("preprocess");
    options.use_causal = args.has("causal");
    options.with_ontology = args.has("with-ontology");
    options.enable_qc = !args.has("no-qc");
    options.min_node_length = args.get("min-node-length", "2").as_int();
    options.min_degree = args.get("min-degree", "2").as_int();
    options.llm_validate = args.has("llm-validate");
    options.validation_mode = args.get("validation-mode", "suspicious").value;
    options.semantic_dedup = args.has("semantic-dedup");
    options.semantic_threshold = args.get("semantic-threshold", "0.85").as_double();

    try {
        PipelineRunner runner(std::move(options));
        return runner.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
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
            {"operators", "p", build_operator_help("Operators: "), "bridges,completions,motifs", false, false},
            {"run-id", "r", "Run ID for tracking", "", false, false},
            // Phase 2: Causal filtering options
            {"causal-strength", "", "Filter by causal strength (weak,moderate,strong,deterministic)", "", false, false},
            {"causal-type", "", "Filter by causal type (necessary,sufficient,direct_cause,contributing,preventing,enabling,mechanism)", "", false, false},
            {"mechanism-type", "", "Filter by mechanism type (physical,chemical,biological,social,economic,computational)", "", false, false},
            {"temporality", "", "Filter by temporal characteristics (immediate,short_term,long_term,delayed)", "", false, false}
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

    // kg list-operators
    cli.register_command({
        "list-operators",
        "List all discovery operators available at runtime",
        {},
        cmd_list_operators
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
            {"operators", "p", build_operator_help("Discovery operators (e.g., ") + ")", "all", false, false},
            {"title", "t", "Title for reports and visualizations", "", false, false},
            {"max-examples", "m", "Max examples per insight type in reports", "10", false, false},
            {"from-stage", "f", "Start from stage (1=extract, 2=index, 3=discover, 4=render, 5=report)", "1", false, false},
            {"run-dir", "d", "Existing run directory to resume (required if from-stage > 1)", "", false, false},
            {"preprocess", "P", "Normalize relations and merge aliases before indexing", "", false, true},
            {"causal", "C", "Use causal extraction prompts (Phase 2 feature)", "", false, true},
            {"with-ontology", "O", "Enable class/instance classification for entities", "", false, true},
            // Quality Control flags
            {"no-qc", "", "Disable quality control entirely", "", false, true},
            {"min-node-length", "", "Minimum character length for entity labels", "2", false, false},
            {"min-degree", "", "Minimum node degree (connectivity)", "1", false, false},
            {"llm-validate", "", "Enable Level 3 LLM validation for quality control", "", false, true},
            {"validation-mode", "", "LLM validation mode: all, suspicious, none", "suspicious", false, false},
            {"semantic-dedup", "", "Enable semantic deduplication (requires LLM)", "", false, true},
            {"semantic-threshold", "", "Similarity threshold for semantic deduplication", "0.85", false, false}
        },
        cmd_run
    });

    return cli.run(argc, argv);
}

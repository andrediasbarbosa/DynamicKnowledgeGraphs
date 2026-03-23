#include "cli/cli.hpp"
#include "graph/hypergraph.hpp"
#include "index/hypergraph_index.hpp"
#include "discovery/discovery_engine.hpp"
#include "discovery/report_generator.hpp"
#include "render/augmentation_renderer.hpp"
#include "discovery/operator_registry.hpp"
#include "pipeline/extraction_pipeline.hpp"
#include "pipeline/graph_cleaner.hpp"
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
    std::string input_path = args.get("input", "").value;
    std::string output_base = args.get("output", "runs/").value;
    std::string config_path = args.get("config", "").value;
    auto operators = expand_operators(args.get("operators", "all").as_list());
    std::string title = args.get("title", "").value;
    int max_examples = args.get("max-examples", "10").as_int();
    int from_stage = args.get("from-stage", "1").as_int();
    std::string existing_run_dir = args.get("run-dir", "").value;
    bool preprocess = args.has("preprocess");
    bool use_causal = args.has("causal");  // Phase 2: Causal extraction mode

    // Quality control configuration
    bool enable_qc = !args.has("no-qc");  // Quality control enabled by default
    int min_node_length = args.get("min-node-length", "2").as_int();
    int min_degree = args.get("min-degree", "1").as_int();
    bool llm_validate = args.has("llm-validate");
    std::string validation_mode = args.get("validation-mode", "suspicious").value;

    // Validate stage range
    if (from_stage < 1 || from_stage > 5) {
        std::cerr << "Error: --from-stage must be between 1 and 5\n";
        std::cerr << "  1 = extraction, 2 = indexing, 3 = discovery, 4 = render, 5 = report\n";
        return 1;
    }

    if (preprocess && from_stage > 2) {
        std::cerr << "Error: --preprocess can only be used with --from-stage 1 or 2\n";
        std::cerr << "  Preprocessing changes the graph and requires rebuilding the index.\n";
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

    // V2: Create step-based output folders for organized artifacts
    std::string step1_dir = run_dir + "/Step_1_Loading";
    std::string step2_dir = run_dir + "/Step_2_Extraction";
    std::string step3_dir = run_dir + "/Step_3_QualityControl";
    std::string step4_dir = run_dir + "/Step_4_GraphBuilding";
    std::string step5_dir = run_dir + "/Step_5_Discovery";

    fs::create_directories(step1_dir);
    fs::create_directories(step2_dir);
    fs::create_directories(step3_dir);
    fs::create_directories(step4_dir);
    fs::create_directories(step5_dir);

    // Derive title from first PDF if not specified
    if (title.empty() && !pdf_paths.empty()) {
        title = get_basename(pdf_paths[0]) + " Knowledge Discovery";
    } else if (title.empty()) {
        title = "Knowledge Discovery";
    }

    // V2: Define paths for all artifacts in step-based folders
    std::string graph_path = step4_dir + "/graph.json";
    std::string graph_raw_path = step4_dir + "/graph_raw.json";
    std::string index_path = step5_dir + "/index.json";
    std::string insights_path = step5_dir + "/insights.json";

    // Declare variables used across stages
    Hypergraph graph;
    HypergraphIndex index;
    InsightCollection insights;
    HypergraphStatistics graph_stats;
    PreprocessStats preprocess_stats;
    bool preprocess_ran = false;

    // Track total pipeline runtime
    auto pipeline_start = std::chrono::steady_clock::now();

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

        // V2: Override output directory to Step_2_Extraction folder
        pipeline_config.output_directory = step2_dir;
        pipeline_config.save_intermediate = true;
        pipeline_config.save_extractions = true;

        // Phase 2: Use causal extraction prompts if --causal flag is set
        if (use_causal) {
            pipeline_config.custom_system_prompt = PromptTemplates::causal_extraction_system_prompt();
            std::cout << "  Causal extraction mode: ENABLED\n";
        }

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

        // V2: Save graph to Step_4_GraphBuilding folder (raw if preprocessing enabled)
        if (preprocess) {
            graph.export_to_json(graph_raw_path, true);
            std::cout << "  Saved: Step_4_GraphBuilding/graph_raw.json\n";
        } else {
            graph.export_to_json(graph_path, true);
            std::cout << "  Saved: Step_4_GraphBuilding/graph.json\n";
        }

        // V2: Save pipeline stats to Step_2_Extraction folder
        auto pipeline_stats = pipeline.get_statistics();
        std::string stats_path = step2_dir + "/extraction_stats.json";
        std::ofstream stats_file(stats_path);
        stats_file << pipeline_stats.to_json().dump(2);
        stats_file.close();
        std::cout << "  Saved: Step_2_Extraction/extraction_stats.json\n";
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
    // Stage 1.5: Preprocess (normalize relations + merge aliases)
    // =========================================================================
    if (preprocess && from_stage <= 2) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 1.5: Preprocess Graph\n";
        std::cout << "----------------------------------------------------------------------\n";

        if (!fs::exists(graph_raw_path)) {
            graph.export_to_json(graph_raw_path, true);
            std::cout << "  Saved: Step_4_GraphBuilding/graph_raw.json\n";
        } else {
            std::cout << "  Found existing: Step_4_GraphBuilding/graph_raw.json\n";
        }

        normalize_relations(graph, preprocess_stats);
        merge_aliases(graph, preprocess_stats);
        preprocess_ran = true;

        graph_stats = graph.compute_statistics();
        graph.export_to_json(graph_path, true);

        std::cout << "  Normalized relations: " << preprocess_stats.relations_normalized << "\n";
        std::cout << "  Merged nodes:         " << preprocess_stats.nodes_merged << "\n";
        std::cout << "  Preprocessed graph:   " << graph_stats.num_nodes << " entities, "
                  << graph_stats.num_edges << " relationships\n";
        std::cout << "  Saved: Step_4_GraphBuilding/graph.json\n";
    }

    // =========================================================================
    // Stage 1.75: Quality Control (3-Level Graph Cleaning)
    // =========================================================================
    CleaningReport qc_report;
    if (enable_qc && from_stage <= 2) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 1.75: Quality Control\n";
        std::cout << "----------------------------------------------------------------------\n";

        auto qc_start = std::chrono::steady_clock::now();

        // Helper lambda to trim whitespace
        auto trim = [](const std::string& s) {
            if (s.empty()) return s;
            size_t start = 0;
            while (start < s.length() && std::isspace(static_cast<unsigned char>(s[start]))) {
                start++;
            }
            size_t end = s.length();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
                end--;
            }
            return s.substr(start, end - start);
        };

        // Convert graph entities to CleanableEntity format
        std::vector<CleanableEntity> cleanable_entities;
        auto all_nodes = graph.get_all_nodes();
        for (const auto& node : all_nodes) {
            CleanableEntity ce;
            ce.id = node.id;
            ce.label = trim(node.label);  // Trim whitespace for accurate validation
            // Try to get type from properties
            auto type_it = node.properties.find("type");
            if (type_it != node.properties.end()) {
                ce.type = type_it->second;
            }
            // Try to get confidence from properties
            auto conf_it = node.properties.find("confidence");
            if (conf_it != node.properties.end()) {
                try {
                    ce.confidence = std::stod(conf_it->second);
                } catch (...) {
                    // Keep default confidence of 1.0
                }
            }
            cleanable_entities.push_back(ce);
        }

        // Convert graph edges to CleanableRelation format
        std::vector<CleanableRelation> cleanable_relations;
        auto all_edges = graph.get_all_edges();
        for (const auto& edge : all_edges) {
            if (!edge.sources.empty() && !edge.targets.empty()) {
                CleanableRelation cr;
                cr.id = edge.id;
                cr.source = edge.sources[0];
                cr.relation = edge.relation;
                cr.target = edge.targets[0];
                cr.confidence = edge.confidence;
                cleanable_relations.push_back(cr);
            }
        }

        std::cout << "  Initial: " << cleanable_entities.size() << " entities, "
                  << cleanable_relations.size() << " relations\n";

        // Configure cleaning
        CleaningConfig qc_config;
        qc_config.min_node_length = min_node_length;
        qc_config.min_degree = min_degree;
        qc_config.enable_llm_validation = llm_validate;
        qc_config.validation_mode = validation_mode;
        qc_config.progress_callback = [](const std::string& msg, int current, int total) {
            std::cout << "  [QC] " << msg << " " << current << "/" << total << "\r" << std::flush;
        };

        // Run cleaning
        GraphCleaner cleaner;
        std::shared_ptr<LLMProvider> qc_llm = nullptr;
        if (llm_validate) {
            qc_llm = std::shared_ptr<LLMProvider>(
                LLMProviderFactory::create_from_config_file(config_path)
            );
            if (!qc_llm) {
                std::cerr << "  Warning: LLM validation requested but LLM provider not available\n";
                qc_config.enable_llm_validation = false;
            }
        }

        qc_report = cleaner.clean(cleanable_entities, cleanable_relations, qc_config, qc_llm);

        // Apply cleaning results to graph - remove invalid entities
        std::set<std::string> valid_entity_ids;
        for (const auto& ce : cleanable_entities) {
            if (ce.is_valid) {
                valid_entity_ids.insert(ce.id);
            } else {
                graph.remove_node(ce.id);
            }
        }

        // Build map of invalid relation ids
        std::set<std::string> invalid_relation_ids;
        for (const auto& cr : cleanable_relations) {
            if (!cr.is_valid) {
                invalid_relation_ids.insert(cr.id);
            }
        }

        // Remove invalid relations and relations referencing removed nodes
        auto edges_after_cleaning = graph.get_all_edges();
        for (const auto& edge : edges_after_cleaning) {
            // Check if this relation was marked invalid
            if (invalid_relation_ids.count(edge.id) > 0) {
                graph.remove_hyperedge(edge.id);
                continue;
            }

            // Check if any node in this edge was removed
            auto edge_nodes = edge.get_all_nodes();
            bool has_invalid_node = false;
            for (const auto& node_id : edge_nodes) {
                if (valid_entity_ids.find(node_id) == valid_entity_ids.end()) {
                    has_invalid_node = true;
                    break;
                }
            }
            if (has_invalid_node) {
                graph.remove_hyperedge(edge.id);
            }
        }

        // Update graph statistics
        graph_stats = graph.compute_statistics();

        // Save cleaned graph
        graph.export_to_json(graph_path, true);

        // Save cleaning report (JSON)
        std::string qc_report_path = step3_dir + "/cleaning_report.json";
        std::ofstream qc_file(qc_report_path);
        qc_file << qc_report.to_json().dump(2);
        qc_file.close();

        // Save cleaning report (HTML)
        std::string qc_html_path = step3_dir + "/quality_control_report.html";
        std::ofstream qc_html_file(qc_html_path);
        qc_html_file << qc_report.generate_html_report();
        qc_html_file.close();

        auto qc_duration = std::chrono::steady_clock::now() - qc_start;
        std::cout << "\n  Cleaned: " << qc_report.final_nodes << " entities ("
                  << (qc_report.initial_nodes - qc_report.final_nodes) << " removed), "
                  << qc_report.final_edges << " relations ("
                  << (qc_report.initial_edges - qc_report.final_edges) << " removed)\n";
        std::cout << "  Level 1 (rules):   removed " << qc_report.level1_removed << " entities\n";
        std::cout << "  Level 2 (stats):   removed " << qc_report.level2_removed << " entities\n";
        if (llm_validate) {
            std::cout << "  Level 3 (LLM):     removed " << qc_report.level3_removed << " entities\n";
        }
        std::cout << "  Saved: Step_3_QualityControl/cleaning_report.json\n";
        std::cout << "  Saved: Step_3_QualityControl/quality_control_report.html\n";
        std::cout << "  QC time: " << format_duration(qc_duration) << "\n";
    } else if (!enable_qc) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "  Stage 1.75: Quality Control [DISABLED]\n";
        std::cout << "----------------------------------------------------------------------\n";
    }

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
        std::cout << "  Saved: Step_5_Discovery/index.json\n";
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
        auto discovery_llm = LLMProviderFactory::create_from_config_file(config_path);
        if (discovery_llm) {
            engine.set_llm_provider(std::shared_ptr<LLMProvider>(std::move(discovery_llm)));
        }
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
        std::cout << "  Saved: Step_5_Discovery/insights.json\n";
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

        // V2: Baseline HTML in Step_5_Discovery
        std::string baseline_html = step5_dir + "/graph.html";
        graph.export_to_html(baseline_html, title);
        std::cout << "  Saved: Step_5_Discovery/graph.html (baseline viewer)\n";

        // Augmented HTML with insights (must come before Graph-RAG so we can pass augmentation)
        AugmentationRenderer renderer(graph);
        augmentation = renderer.convert(insights);

        std::string aug_json = step5_dir + "/augmentation.json";
        augmentation.save_to_json(aug_json);
        std::cout << "  Saved: Step_5_Discovery/augmentation.json\n";

        std::string aug_html = step5_dir + "/graph_augmented.html";
        renderer.export_augmented_html(aug_html, title, augmentation);
        std::cout << "  Saved: graph_augmented.html (with " << augmentation.nodes.size()
                  << " augmented nodes)\n";

        // Graph-RAG HTML viewer — includes base graph + all insight-operator nodes/links
        // Read LLM config here so this works even when resuming from --from-stage 4
        std::string rag_provider, rag_key, rag_model;
        {
            std::string cfg_file = ".llm_config.json";
            if (!config_path.empty() && fs::exists(config_path)) cfg_file = config_path;
            if (fs::exists(cfg_file)) {
                try {
                    std::ifstream rf(cfg_file);
                    auto rj = nlohmann::json::parse(rf);
                    rag_provider = rj.value("provider", "openai");
                    rag_key      = rj.value("api_key",  "");
                    rag_model    = rj.value("model",    "gpt-4o-mini");
                } catch (...) {}
            }
        }
        std::string rag_html = step5_dir + "/graph_rag.html";
        graph.export_to_html_rag(rag_html, title,
            rag_provider, rag_key, rag_model,
            /*base_url=*/"",
            augmentation.to_json());
        std::cout << "  Saved: Step_5_Discovery/graph_rag.html (Graph-RAG chat viewer, "
                  << augmentation.nodes.size() << " insight nodes included)\n";

        // DOT visualization
        std::string dot_path = step5_dir + "/graph.dot";
        graph.export_to_dot(dot_path);
        std::cout << "  Saved: Step_5_Discovery/graph.dot\n";
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
        std::string md_path = step5_dir + "/report.md";
        report_gen.save_to_file(md_path, md_report);
        std::cout << "  Saved: Step_5_Discovery/report.md\n";

        // V2: HTML report in Step_5_Discovery
        report_config.format = ReportFormat::HTML;
        std::string html_report = report_gen.generate_html(insights, report_config);
        std::string html_path = step5_dir + "/report.html";
        report_gen.save_to_file(html_path, html_report);
        std::cout << "  Saved: Step_5_Discovery/report.html\n";

        // V2: Pattern library export in Step_5_Discovery
        std::string pattern_lib_path = step5_dir + "/pattern_library.json";
        report_gen.export_pattern_library(insights, pattern_lib_path);
        std::cout << "  Saved: Step_5_Discovery/pattern_library.json\n";
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
    if (preprocess) {
        manifest["artifacts"]["graph_raw"] = "graph_raw.json";
        manifest["preprocess"]["enabled"] = preprocess_ran;
        manifest["preprocess"]["relations_normalized"] = preprocess_stats.relations_normalized;
        manifest["preprocess"]["nodes_merged"] = preprocess_stats.nodes_merged;
    }
    manifest["artifacts"]["index"] = "Step_5_Discovery/index.json";
    manifest["artifacts"]["insights"] = "Step_5_Discovery/insights.json";
    manifest["artifacts"]["augmentation"] = "Step_5_Discovery/augmentation.json";
    manifest["artifacts"]["visualizations"]["baseline"] = "Step_5_Discovery/graph.html";
    manifest["artifacts"]["visualizations"]["augmented"] = "Step_5_Discovery/graph_augmented.html";
    manifest["artifacts"]["visualizations"]["rag"] = "Step_5_Discovery/graph_rag.html";
    manifest["artifacts"]["visualizations"]["dot"] = "Step_5_Discovery/graph.dot";
    manifest["artifacts"]["reports"]["markdown"] = "Step_5_Discovery/report.md";
    manifest["artifacts"]["reports"]["html"] = "Step_5_Discovery/report.html";
    manifest["artifacts"]["extraction_stats"] = "Step_2_Extraction/extraction_stats.json";

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
    readme << "    Step_4_GraphBuilding/graph.json        - Extracted knowledge graph\n";
    if (preprocess) {
        readme << "    Step_4_GraphBuilding/graph_raw.json    - Raw graph prior to preprocessing\n";
    }
    readme << "    Step_5_Discovery/index.json            - S-component index\n";
    readme << "    Step_5_Discovery/insights.json         - Discovered insights\n";
    readme << "    Step_5_Discovery/augmentation.json     - Augmentation overlay data\n";
    readme << "    Step_2_Extraction/extraction_stats.json - Pipeline statistics\n";
    readme << "    manifest.json                          - Run metadata\n";
    readme << "\n";
    readme << "  Visualizations:\n";
    readme << "    Step_5_Discovery/graph.html            - Interactive 3D graph viewer\n";
    readme << "    Step_5_Discovery/graph_augmented.html  - Augmented view with insights\n";
    readme << "    Step_5_Discovery/graph_rag.html        - Graph-RAG chat viewer\n";
    readme << "    Step_5_Discovery/graph.dot             - GraphViz DOT format\n";
    readme << "\n";
    readme << "  Reports:\n";
    readme << "    Step_5_Discovery/report.md             - Markdown report\n";
    readme << "    Step_5_Discovery/report.html           - Styled HTML report (with Phase 2 causal badges)\n";
    readme << "\n";
    readme << "To view:\n";
    readme << "  cd " << run_dir << "/Step_5_Discovery\n";
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

    // Display total runtime in minutes
    auto pipeline_end = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(pipeline_end - pipeline_start);
    double total_minutes = total_duration.count() / 60.0;
    std::cout << "Total runtime: " << std::fixed << std::setprecision(2) << total_minutes << " minutes\n";
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
            {"causal", "C", "Use causal extraction prompts (Phase 2 feature)", "", false, true}
        },
        cmd_run
    });

    return cli.run(argc, argv);
}

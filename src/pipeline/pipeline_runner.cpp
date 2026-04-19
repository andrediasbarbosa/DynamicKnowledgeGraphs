#include "pipeline/pipeline_runner.hpp"

#include "discovery/discovery_engine.hpp"
#include "discovery/report_generator.hpp"
#include "llm/llm_provider.hpp"
#include "pipeline/extraction_pipeline.hpp"
#include "render/augmentation_renderer.hpp"
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace kg {

namespace {

std::string generate_run_id() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "run_" << std::put_time(std::gmtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string get_basename(const std::string& path) {
    return fs::path(path).stem().string();
}

std::vector<std::string> find_pdfs(const std::string& path) {
    std::vector<std::string> pdfs;

    if (fs::is_regular_file(path)) {
        if (path.size() > 4 && path.substr(path.size() - 4) == ".pdf") {
            pdfs.push_back(path);
        }
    } else if (fs::is_directory(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const std::string filename = entry.path().string();
            if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".pdf") {
                pdfs.push_back(filename);
            }
        }
    }

    return pdfs;
}

std::string format_duration(std::chrono::steady_clock::duration duration) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::stringstream ss;
    if (ms >= 1000) {
        ss << std::fixed << std::setprecision(2) << (ms / 1000.0) << "s";
    } else {
        ss << ms << "ms";
    }
    return ss.str();
}

std::string trim_copy(const std::string& value) {
    if (value.empty()) {
        return value;
    }

    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

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

    std::string collapsed;
    bool last_space = false;
    for (char c : lower) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_space) {
                collapsed.push_back(' ');
            }
            last_space = true;
        } else {
            collapsed.push_back(c);
            last_space = false;
        }
    }

    if (!collapsed.empty() && collapsed.front() == ' ') {
        collapsed.erase(collapsed.begin());
    }
    if (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }

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

    const auto it = synonym_map.find(collapsed);
    if (it != synonym_map.end()) {
        return it->second;
    }
    return collapsed.empty() ? relation : collapsed;
}

std::string normalize_label_key_simple(const std::string& label) {
    std::string output;
    output.reserve(label.size());
    for (unsigned char c : label) {
        if (std::isalnum(c)) {
            output.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '_' || c == '-' || c == '/' || c == '\\') {
            output.push_back(' ');
        } else {
            output.push_back(' ');
        }
    }

    std::string collapsed;
    bool last_space = false;
    for (char c : output) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_space) {
                collapsed.push_back(' ');
            }
            last_space = true;
        } else {
            collapsed.push_back(c);
            last_space = false;
        }
    }

    if (!collapsed.empty() && collapsed.front() == ' ') {
        collapsed.erase(collapsed.begin());
    }
    if (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }

    return collapsed;
}

void normalize_relations(Hypergraph& graph, size_t& relations_normalized) {
    const auto edges = graph.get_all_edges();
    for (const auto& edge : edges) {
        auto* mutable_edge = graph.get_hyperedge(edge.id);
        if (!mutable_edge) {
            continue;
        }

        const std::string canonical = normalize_relation_label(mutable_edge->relation);
        if (!canonical.empty() && canonical != mutable_edge->relation) {
            mutable_edge->properties["original_relation"] = mutable_edge->relation;
            mutable_edge->relation = canonical;
            ++relations_normalized;
        }
    }
}

void merge_aliases(Hypergraph& graph, size_t& nodes_merged) {
    HypergraphIndex dummy_index;
    DiscoveryEngine engine(graph, dummy_index);
    DiscoveryConfig config;
    config.entity_resolution_min_label_similarity = 0.9;
    config.entity_resolution_min_neighbor_overlap = 0.1;
    config.entity_resolution_max_candidates = 500;
    engine.set_config(config);

    const auto candidates = engine.find_entity_resolutions();
    std::unordered_set<std::string> merged_pairs;
    for (const auto& insight : candidates) {
        if (insight.seed_nodes.size() < 2) {
            continue;
        }

        const std::string& a = insight.seed_nodes[0];
        const std::string& b = insight.seed_nodes[1];
        const std::string key = a < b ? a + "|" + b : b + "|" + a;
        if (!merged_pairs.insert(key).second) {
            continue;
        }

        const auto* node_a = graph.get_node(a);
        const auto* node_b = graph.get_node(b);
        if (!node_a || !node_b) {
            continue;
        }

        const std::string keep = node_a->degree >= node_b->degree ? a : b;
        const std::string remove = keep == a ? b : a;
        graph.merge_nodes(keep, remove);
        ++nodes_merged;
    }

    const auto nodes = graph.get_all_nodes();
    std::unordered_map<std::string, std::vector<std::string>> by_normalized_label;
    for (const auto& node : nodes) {
        const std::string normalized = normalize_label_key_simple(node.label);
        if (!normalized.empty()) {
            by_normalized_label[normalized].push_back(node.id);
        }
    }

    for (auto& [normalized_label, ids] : by_normalized_label) {
        (void)normalized_label;
        if (ids.size() < 2) {
            continue;
        }

        const std::string keep = ids.front();
        for (size_t i = 1; i < ids.size(); ++i) {
            graph.merge_nodes(keep, ids[i]);
            ++nodes_merged;
        }
    }
}

std::tuple<std::string, std::string, std::string> load_rag_runtime_config(const std::string& config_path) {
    std::string provider;
    std::string api_key;
    std::string model;

    std::string config_file = ".llm_config.json";
    if (!config_path.empty() && fs::exists(config_path)) {
        config_file = config_path;
    }

    if (fs::exists(config_file)) {
        try {
            std::ifstream config_stream(config_file);
            const auto config_json = nlohmann::json::parse(config_stream);
            provider = config_json.value("provider", "openai");
            api_key = config_json.value("api_key", "");
            model = config_json.value("model", "gpt-4o-mini");
        } catch (...) {
        }
    }

    return {provider, api_key, model};
}

void print_stage_header(const std::string& title) {
    std::cout << "\n";
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "  " << title << "\n";
    std::cout << "----------------------------------------------------------------------\n";
}

std::string current_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

}  // namespace

PipelineRunner::PipelineRunner(PipelineRunOptions options)
    : options_(std::move(options)) {}

int PipelineRunner::run() {
    validate_options();
    initialize_run();
    derive_title();
    build_source_name();

    const auto pipeline_start = std::chrono::steady_clock::now();

    run_extraction_stage();
    run_preprocess_stage();
    run_quality_control_stage();
    run_ontology_stage();
    run_index_stage();
    run_discovery_stage();
    run_render_stage();
    run_report_stage();
    finalize_manifest();
    print_summary(pipeline_start);

    return 0;
}

void PipelineRunner::validate_options() const {
    if (options_.from_stage < 1 || options_.from_stage > 5) {
        throw std::runtime_error(
            "--from-stage must be between 1 and 5\n"
            "  1 = extraction, 2 = indexing, 3 = discovery, 4 = render, 5 = report"
        );
    }

    if (options_.preprocess && options_.from_stage > 2) {
        throw std::runtime_error(
            "--preprocess can only be used with --from-stage 1 or 2\n"
            "  Preprocessing changes the graph and requires rebuilding the index."
        );
    }

    if (options_.from_stage > 1 && options_.existing_run_dir.empty()) {
        throw std::runtime_error("--run-dir is required when --from-stage > 1");
    }

    if (options_.from_stage == 1 && options_.input_path.empty()) {
        throw std::runtime_error("--input is required when starting from stage 1");
    }
}

void PipelineRunner::initialize_run() {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  Knowledge Discovery Pipeline\n";
    std::cout << "======================================================================\n";
    std::cout << "\n";

    context_.title = options_.title;

    if (options_.from_stage > 1) {
        context_.paths.run_dir = options_.existing_run_dir;
        if (!context_.paths.run_dir.empty() && context_.paths.run_dir.back() == '/') {
            context_.paths.run_dir.pop_back();
        }

        context_.paths.run_id = fs::path(context_.paths.run_dir).filename().string();
        if (!fs::exists(context_.paths.run_dir)) {
            throw std::runtime_error("Run directory does not exist: " + context_.paths.run_dir);
        }

        std::cout << "Resuming run: " << context_.paths.run_id << "\n";
        std::cout << "From stage:   " << options_.from_stage << " (";
        switch (options_.from_stage) {
            case 2: std::cout << "indexing"; break;
            case 3: std::cout << "discovery"; break;
            case 4: std::cout << "render"; break;
            case 5: std::cout << "report"; break;
            default: break;
        }
        std::cout << ")\n";
        std::cout << "Run dir:      " << context_.paths.run_dir << "\n";

        const std::string manifest_path = context_.paths.run_dir + "/manifest.json";
        if (fs::exists(manifest_path)) {
            std::ifstream manifest_stream(manifest_path);
            const auto manifest = nlohmann::json::parse(manifest_stream);
            if (context_.title.empty() && manifest.contains("title")) {
                context_.title = manifest["title"].get<std::string>();
            }
            if (manifest.contains("sources")) {
                for (const auto& source : manifest["sources"]) {
                    context_.pdf_paths.push_back(source.get<std::string>());
                }
            }
        }
    } else {
        context_.paths.run_id = generate_run_id();

        std::cout << "Run ID: " << context_.paths.run_id << "\n";
        std::cout << "Input:  " << options_.input_path << "\n";

        context_.pdf_paths = find_pdfs(options_.input_path);
        if (context_.pdf_paths.empty()) {
            throw std::runtime_error("No PDF files found at: " + options_.input_path);
        }

        std::cout << "Found " << context_.pdf_paths.size() << " PDF file(s):\n";
        for (const auto& path : context_.pdf_paths) {
            std::cout << "  - " << path << "\n";
        }

        context_.paths.run_dir = options_.output_base;
        if (!context_.paths.run_dir.empty() && context_.paths.run_dir.back() != '/') {
            context_.paths.run_dir += "/";
        }
        context_.paths.run_dir += context_.paths.run_id;
        fs::create_directories(context_.paths.run_dir);

        std::cout << "\nOutput: " << context_.paths.run_dir << "/\n";
    }

    context_.paths.step1_dir = context_.paths.run_dir + "/Step_1_Loading";
    context_.paths.step2_dir = context_.paths.run_dir + "/Step_2_Extraction";
    context_.paths.step3_dir = context_.paths.run_dir + "/Step_3_QualityControl";
    context_.paths.step4_dir = context_.paths.run_dir + "/Step_4_GraphBuilding";
    context_.paths.step5_dir = context_.paths.run_dir + "/Step_5_Discovery";
    context_.paths.graph_path = context_.paths.step4_dir + "/graph.json";
    context_.paths.graph_raw_path = context_.paths.step4_dir + "/graph_raw.json";
    context_.paths.index_path = context_.paths.step5_dir + "/index.json";
    context_.paths.insights_path = context_.paths.step5_dir + "/insights.json";
    context_.paths.manifest_path = context_.paths.run_dir + "/manifest.json";

    fs::create_directories(context_.paths.step1_dir);
    fs::create_directories(context_.paths.step2_dir);
    fs::create_directories(context_.paths.step3_dir);
    fs::create_directories(context_.paths.step4_dir);
    fs::create_directories(context_.paths.step5_dir);
}

void PipelineRunner::derive_title() {
    if (context_.title.empty() && !context_.pdf_paths.empty()) {
        context_.title = get_basename(context_.pdf_paths.front()) + " Knowledge Discovery";
    } else if (context_.title.empty()) {
        context_.title = "Knowledge Discovery";
    }
}

void PipelineRunner::build_source_name() {
    context_.source_name.clear();
    for (const auto& path : context_.pdf_paths) {
        if (!context_.source_name.empty()) {
            context_.source_name += ", ";
        }
        context_.source_name += get_basename(path) + ".pdf";
    }
}

void PipelineRunner::run_extraction_stage() {
    const auto stage_start = std::chrono::steady_clock::now();

    if (options_.from_stage <= 1) {
        print_stage_header("Stage 1: Knowledge Extraction");

        PipelineConfig pipeline_config = load_config_with_fallback(options_.config_path);
        pipeline_config.output_directory = context_.paths.step2_dir;
        pipeline_config.save_intermediate = true;
        pipeline_config.save_extractions = true;

        if (options_.use_causal && options_.with_ontology) {
            pipeline_config.custom_system_prompt = PromptTemplates::causal_ontology_extraction_system_prompt();
            std::cout << "  Causal + Ontology mode: ENABLED (LLM-based classification with causal metadata)\n";
        } else if (options_.use_causal) {
            pipeline_config.custom_system_prompt = PromptTemplates::causal_extraction_system_prompt();
            std::cout << "  Causal extraction mode: ENABLED\n";
        } else if (options_.with_ontology) {
            pipeline_config.custom_system_prompt = PromptTemplates::relation_extraction_system_prompt_with_ontology();
            std::cout << "  Ontology classification mode: ENABLED (LLM-based)\n";
        }

        if (options_.with_ontology && !options_.use_causal && pipeline_config.llm_provider == "gemini") {
            std::cout << "\n  WARNING: Gemini models may struggle with ontology classification.\n";
            std::cout << "  If you see many extraction failures, try:\n";
            std::cout << "    1. Remove --with-ontology (heuristics will still be applied), OR\n";
            std::cout << "    2. Use provider=openai in .llm_config.json for LLM-based classification\n\n";
        }

        std::string config_error;
        if (!pipeline_config.validate(config_error)) {
            throw std::runtime_error(
                "Configuration error: " + config_error +
                "\nPlease set up .llm_config.json or environment variables."
            );
        }

        std::cout << "  Provider: " << pipeline_config.llm_provider << "\n";
        std::cout << "  Model:    " << pipeline_config.llm_model << "\n";

        if (options_.use_causal && pipeline_config.llm_provider == "gemini") {
            std::cout << "\n  WARNING: Gemini models may struggle with complex causal extraction.\n";
            std::cout << "  If you see many extraction failures, try:\n";
            std::cout << "    1. Re-run without --causal flag, OR\n";
            std::cout << "    2. Use provider=openai in .llm_config.json\n\n";
        }

        ExtractionPipeline pipeline(pipeline_config);
        pipeline.set_progress_callback([](const std::string& stage, int current, int total, const std::string& message) {
            std::cout << "  [" << stage << "] " << current << "/" << total;
            if (!message.empty()) {
                std::cout << " - " << message;
            }
            std::cout << "\r" << std::flush;
        });

        try {
            context_.graph = pipeline.process_pdfs(context_.pdf_paths);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Extraction error: ") + e.what());
        }

        context_.graph_stats = context_.graph.compute_statistics();
        std::cout << "\n  Extracted: " << context_.graph_stats.num_nodes << " entities, "
                  << context_.graph_stats.num_edges << " relationships\n";

        if (options_.preprocess) {
            context_.graph.export_to_json(context_.paths.graph_raw_path, true);
            std::cout << "  Saved: Step_4_GraphBuilding/graph_raw.json\n";
        } else {
            context_.graph.export_to_json(context_.paths.graph_path, true);
            std::cout << "  Saved: Step_4_GraphBuilding/graph.json\n";
        }

        const auto pipeline_stats = pipeline.get_statistics();
        const std::string stats_path = context_.paths.step2_dir + "/extraction_stats.json";
        std::ofstream stats_stream(stats_path);
        stats_stream << pipeline_stats.to_json().dump(2);
        std::cout << "  Saved: Step_2_Extraction/extraction_stats.json\n";
    } else {
        print_stage_header("Stage 1: Knowledge Extraction [SKIPPED - loading existing]");

        if (!fs::exists(context_.paths.graph_path)) {
            throw std::runtime_error("Required file not found: " + context_.paths.graph_path);
        }

        std::cout << "  Loading: graph.json\n";
        context_.graph = Hypergraph::load_from_json(context_.paths.graph_path);
        context_.graph_stats = context_.graph.compute_statistics();
        std::cout << "  Loaded: " << context_.graph_stats.num_nodes << " entities, "
                  << context_.graph_stats.num_edges << " relationships\n";
    }

    std::cout << "  Stage 1 time: " << format_duration(std::chrono::steady_clock::now() - stage_start) << "\n";
}

void PipelineRunner::run_preprocess_stage() {
    if (!(options_.preprocess && options_.from_stage <= 2)) {
        return;
    }

    print_stage_header("Stage 1.5: Preprocess Graph");

    if (!fs::exists(context_.paths.graph_raw_path)) {
        context_.graph.export_to_json(context_.paths.graph_raw_path, true);
        std::cout << "  Saved: Step_4_GraphBuilding/graph_raw.json\n";
    } else {
        std::cout << "  Found existing: Step_4_GraphBuilding/graph_raw.json\n";
    }

    normalize_relations(context_.graph, context_.relations_normalized);
    merge_aliases(context_.graph, context_.nodes_merged);
    context_.preprocess_ran = true;

    context_.graph_stats = context_.graph.compute_statistics();
    context_.graph.export_to_json(context_.paths.graph_path, true);

    std::cout << "  Normalized relations: " << context_.relations_normalized << "\n";
    std::cout << "  Merged nodes:         " << context_.nodes_merged << "\n";
    std::cout << "  Preprocessed graph:   " << context_.graph_stats.num_nodes << " entities, "
              << context_.graph_stats.num_edges << " relationships\n";
    std::cout << "  Saved: Step_4_GraphBuilding/graph.json\n";
}

void PipelineRunner::run_quality_control_stage() {
    if (!options_.enable_qc) {
        print_stage_header("Stage 1.75: Quality Control [DISABLED]");
        return;
    }

    if (options_.from_stage > 2) {
        return;
    }

    print_stage_header("Stage 1.75: Quality Control");
    const auto qc_start = std::chrono::steady_clock::now();

    std::vector<CleanableEntity> cleanable_entities;
    const auto all_nodes = context_.graph.get_all_nodes();
    cleanable_entities.reserve(all_nodes.size());
    for (const auto& node : all_nodes) {
        CleanableEntity entity;
        entity.id = node.id;
        entity.label = trim_copy(node.label);

        const auto type_it = node.properties.find("type");
        if (type_it != node.properties.end()) {
            entity.type = type_it->second;
        }

        const auto confidence_it = node.properties.find("confidence");
        if (confidence_it != node.properties.end()) {
            try {
                entity.confidence = std::stod(confidence_it->second);
            } catch (...) {
            }
        }

        cleanable_entities.push_back(std::move(entity));
    }

    std::vector<CleanableHyperedge> cleanable_hyperedges;
    const auto all_edges = context_.graph.get_all_edges();
    cleanable_hyperedges.reserve(all_edges.size());
    for (const auto& edge : all_edges) {
        CleanableHyperedge hyperedge;
        hyperedge.id = edge.id;
        hyperedge.sources = edge.sources;
        hyperedge.relation = edge.relation;
        hyperedge.targets = edge.targets;
        hyperedge.confidence = edge.confidence;
        cleanable_hyperedges.push_back(std::move(hyperedge));
    }

    std::cout << "  Initial: " << cleanable_entities.size() << " entities, "
              << cleanable_hyperedges.size() << " hyperedges\n";

    std::set<std::string> original_node_ids;
    for (const auto& entity : cleanable_entities) {
        original_node_ids.insert(entity.id);
    }

    CleaningConfig qc_config;
    qc_config.min_node_length = options_.min_node_length;
    qc_config.min_degree = options_.min_degree;
    qc_config.enable_semantic_dedup = options_.semantic_dedup;
    qc_config.semantic_threshold = options_.semantic_threshold;
    qc_config.enable_llm_validation = options_.llm_validate;
    qc_config.validation_mode = options_.validation_mode;
    qc_config.progress_callback = [](const std::string& message, int current, int total) {
        std::cout << "  [QC] " << message << " " << current << "/" << total << "\r" << std::flush;
    };

    GraphCleaner cleaner;
    std::shared_ptr<LLMProvider> qc_llm = nullptr;
    if (options_.llm_validate || options_.semantic_dedup) {
        qc_llm = std::shared_ptr<LLMProvider>(LLMProviderFactory::create_from_config_file(options_.config_path).release());
        if (!qc_llm) {
            std::cerr << "  Warning: LLM-assisted QC requested but LLM provider not available\n";
            qc_config.enable_llm_validation = false;
        }
    }

    context_.qc_report = cleaner.clean(cleanable_entities, cleanable_hyperedges, qc_config, qc_llm);

    std::set<std::string> remaining_node_ids;
    for (const auto& entity : cleanable_entities) {
        remaining_node_ids.insert(entity.id);
    }

    for (const auto& node_id : original_node_ids) {
        if (remaining_node_ids.count(node_id) == 0) {
            context_.graph.remove_node(node_id);
        }
    }

    std::set<std::string> remaining_hyperedge_ids;
    for (const auto& hyperedge : cleanable_hyperedges) {
        remaining_hyperedge_ids.insert(hyperedge.id);
    }

    const auto edges_after_cleaning = context_.graph.get_all_edges();
    for (const auto& edge : edges_after_cleaning) {
        bool should_remove = remaining_hyperedge_ids.count(edge.id) == 0;
        if (!should_remove) {
            for (const auto& node_id : edge.get_all_nodes()) {
                if (remaining_node_ids.count(node_id) == 0) {
                    should_remove = true;
                    break;
                }
            }
        }

        if (should_remove) {
            context_.graph.remove_hyperedge(edge.id);
        }
    }

    for (const auto& entity : cleanable_entities) {
        auto* node = context_.graph.get_node(entity.id);
        if (node && node->label != entity.label) {
            node->label = entity.label;
        }
    }

    std::map<std::string, std::vector<std::string>> label_to_ids;
    for (const auto& entity : cleanable_entities) {
        label_to_ids[entity.label].push_back(entity.id);
    }

    int duplicates_merged = 0;
    for (const auto& [label, ids] : label_to_ids) {
        (void)label;
        if (ids.size() < 2) {
            continue;
        }

        const std::string keep_id = ids.front();
        for (size_t i = 1; i < ids.size(); ++i) {
            context_.graph.merge_nodes(keep_id, ids[i]);
            ++duplicates_merged;
        }
    }

    if (duplicates_merged > 0) {
        std::cout << "  Merged " << duplicates_merged << " duplicate nodes after label simplification\n";
    }

    context_.graph_stats = context_.graph.compute_statistics();
    context_.graph.export_to_json(context_.paths.graph_path, true);

    {
        const std::string qc_report_path = context_.paths.step3_dir + "/cleaning_report.json";
        std::ofstream qc_stream(qc_report_path);
        qc_stream << context_.qc_report.to_json().dump(2);
    }
    std::cout << "  Saved: Step_3_QualityControl/cleaning_report.json\n";

    {
        const std::string qc_html_path = context_.paths.step3_dir + "/quality_control_report.html";
        std::ofstream qc_html_stream(qc_html_path);
        qc_html_stream << context_.qc_report.generate_html_report();
    }
    std::cout << "  Saved: Step_3_QualityControl/quality_control_report.html\n";
    std::cout << "  Saved: Step_4_GraphBuilding/graph.json (cleaned)\n";

    const auto qc_duration = std::chrono::steady_clock::now() - qc_start;
    std::cout << "\n  Cleaned: " << context_.qc_report.final_nodes << " entities ("
              << (context_.qc_report.initial_nodes - context_.qc_report.final_nodes) << " removed), "
              << context_.qc_report.final_edges << " hyperedges ("
              << (context_.qc_report.initial_edges - context_.qc_report.final_edges) << " removed)\n";
    std::cout << "  Level 1 (rules):   removed " << context_.qc_report.level1_removed << " entities\n";
    if (options_.semantic_dedup && context_.qc_report.semantic_duplicates_merged > 0) {
        std::cout << "  Level 1.5 (semantic): merged " << context_.qc_report.semantic_duplicates_merged
                  << " semantic duplicates (" << context_.qc_report.semantic_duplicates_found << " groups)\n";
    }
    std::cout << "  Level 2 (stats):   removed " << context_.qc_report.level2_removed << " entities\n";
    if (options_.llm_validate) {
        std::cout << "  Level 3 (LLM):     removed " << context_.qc_report.level3_removed << " entities\n";
    }
    std::cout << "  QC time: " << format_duration(qc_duration) << "\n";
}

void PipelineRunner::run_ontology_stage() {
    if (!options_.with_ontology) {
        print_stage_header("Stage 1.9: Ontology Classification [DISABLED]");
        std::cout << "  Use --with-ontology to enable class/instance detection\n";
        return;
    }

    if (options_.from_stage > 2) {
        return;
    }

    print_stage_header("Stage 1.9: Ontology Classification");
    const auto ontology_start = std::chrono::steady_clock::now();

    const int classified = ExtractionPipeline::apply_heuristic_classification(context_.graph);
    const int augmented = ExtractionPipeline::augment_instance_base_classes(context_.graph);
    context_.graph.export_to_json(context_.paths.graph_path, true);
    context_.graph_stats = context_.graph.compute_statistics();

    const auto ontology_duration = std::chrono::steady_clock::now() - ontology_start;
    std::cout << "  Classified " << classified << " entities using heuristics\n";
    std::cout << "  Node levels: class (general concept) vs instance (specific example)\n";
    std::cout << "  Augmented " << augmented << " instance nodes with base class info\n";
    std::cout << "  Saved: Step_4_GraphBuilding/graph.json (with ontology)\n";
    std::cout << "  Ontology time: " << format_duration(ontology_duration) << "\n";
}

void PipelineRunner::run_index_stage() {
    const auto stage_start = std::chrono::steady_clock::now();

    if (options_.from_stage <= 2) {
        print_stage_header("Stage 2: Building Index");

        context_.index.source_graph_path = context_.paths.graph_path;
        context_.index.build(context_.graph, {2, 3, 4});
        context_.index.save_to_json(context_.paths.index_path);

        std::cout << "  S-components computed for s = 2, 3, 4\n";
        std::cout << "  Saved: Step_5_Discovery/index.json\n";
    } else {
        print_stage_header("Stage 2: Building Index [SKIPPED - loading existing]");

        if (!fs::exists(context_.paths.index_path)) {
            throw std::runtime_error("Required file not found: " + context_.paths.index_path);
        }

        std::cout << "  Loading: index.json\n";
        context_.index = HypergraphIndex::load_from_json(context_.paths.index_path);
        std::cout << "  Loaded index with " << context_.index.s_components.size() << " s-component sets\n";
    }

    std::cout << "  Stage 2 time: " << format_duration(std::chrono::steady_clock::now() - stage_start) << "\n";
}

void PipelineRunner::run_discovery_stage() {
    const auto stage_start = std::chrono::steady_clock::now();
    context_.insight_counts.clear();

    if (options_.from_stage <= 3) {
        print_stage_header("Stage 3: Knowledge Discovery");

        std::cout << "  Operators: ";
        for (size_t i = 0; i < options_.operators.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << options_.operators[i];
        }
        std::cout << "\n";

        DiscoveryEngine engine(context_.graph, context_.index);
        engine.set_run_id(context_.paths.run_id);
        auto discovery_llm = LLMProviderFactory::create_from_config_file(options_.config_path);
        if (discovery_llm) {
            engine.set_llm_provider(std::shared_ptr<LLMProvider>(std::move(discovery_llm)));
        }
        engine.set_progress_callback([](const std::string& stage, int current, int total) {
            std::cout << "  [" << stage << "] " << current << "/" << total << "\r" << std::flush;
        });

        context_.insights = engine.run_operators(options_.operators);
        context_.insights.source_graph = context_.paths.graph_path;

        if (options_.semantic_dedup) {
            std::cout << "\n  Deduplicating insights (augmented nodes)...\n";
            const int before_count = static_cast<int>(context_.insights.insights.size());
            DiscoveryEngine::deduplicate_insights(context_.insights, options_.semantic_threshold);
            const int after_count = static_cast<int>(context_.insights.insights.size());
            const int removed = before_count - after_count;
            if (removed > 0) {
                std::cout << "  Removed " << removed << " duplicate insights ("
                          << (100.0 * removed / before_count) << "%)\n";
            }
        }

        context_.insights.save_to_json(context_.paths.insights_path);
        std::cout << "  Saved: Step_5_Discovery/insights.json\n";
    } else {
        print_stage_header("Stage 3: Knowledge Discovery [SKIPPED - loading existing]");

        if (!fs::exists(context_.paths.insights_path)) {
            throw std::runtime_error("Required file not found: " + context_.paths.insights_path);
        }

        std::cout << "  Loading: insights.json\n";
        context_.insights = InsightCollection::load_from_json(context_.paths.insights_path);

        if (options_.semantic_dedup) {
            std::cout << "  Deduplicating loaded insights...\n";
            const int before_count = static_cast<int>(context_.insights.insights.size());
            DiscoveryEngine::deduplicate_insights(context_.insights, options_.semantic_threshold);
            const int after_count = static_cast<int>(context_.insights.insights.size());
            const int removed = before_count - after_count;
            if (removed > 0) {
                std::cout << "  Removed " << removed << " duplicate insights ("
                          << (100.0 * removed / before_count) << "%)\n";
            }
        }

        std::cout << "  Loaded " << context_.insights.insights.size() << " insights\n";
    }

    for (const auto& insight : context_.insights.insights) {
        context_.insight_counts[insight.type]++;
    }

    if (options_.from_stage <= 3) {
        std::cout << "\n  Discovered " << context_.insights.insights.size() << " insights:\n";
        for (const auto& [type, count] : context_.insight_counts) {
            std::cout << "    - " << insight_type_to_string(type) << ": " << count << "\n";
        }
    }

    std::cout << "  Stage 3 time: " << format_duration(std::chrono::steady_clock::now() - stage_start) << "\n";
}

void PipelineRunner::run_render_stage() {
    const auto stage_start = std::chrono::steady_clock::now();

    if (options_.from_stage <= 4) {
        print_stage_header("Stage 4: Generating Visualizations");

        const std::string baseline_html = context_.paths.step5_dir + "/graph.html";
        context_.graph.export_to_html(baseline_html, context_.title);
        std::cout << "  Saved: Step_5_Discovery/graph.html (baseline viewer)\n";

        AugmentationRenderer renderer(context_.graph);
        AugmentationData augmentation = renderer.convert(context_.insights);

        const std::string augmentation_path = context_.paths.step5_dir + "/augmentation.json";
        augmentation.save_to_json(augmentation_path);
        std::cout << "  Saved: Step_5_Discovery/augmentation.json\n";

        const std::string augmented_html = context_.paths.step5_dir + "/graph_augmented.html";
        renderer.export_augmented_html(augmented_html, context_.title, augmentation);
        std::cout << "  Saved: graph_augmented.html (with " << augmentation.nodes.size()
                  << " augmented nodes)\n";

        const auto [provider, api_key, model] = load_rag_runtime_config(options_.config_path);
        const std::string rag_html = context_.paths.step5_dir + "/graph_rag.html";
        context_.graph.export_to_html_rag(
            rag_html,
            context_.title,
            provider,
            api_key,
            model,
            "",
            augmentation.to_json()
        );
        std::cout << "  Saved: Step_5_Discovery/graph_rag.html (Graph-RAG chat viewer, "
                  << augmentation.nodes.size() << " insight nodes included)\n";

        const std::string dot_path = context_.paths.step5_dir + "/graph.dot";
        context_.graph.export_to_dot(dot_path);
        std::cout << "  Saved: Step_5_Discovery/graph.dot\n";
    } else {
        print_stage_header("Stage 4: Generating Visualizations [SKIPPED]");
        std::cout << "  Using existing visualization files\n";
    }

    std::cout << "  Stage 4 time: " << format_duration(std::chrono::steady_clock::now() - stage_start) << "\n";
}

void PipelineRunner::run_report_stage() {
    const auto stage_start = std::chrono::steady_clock::now();

    if (options_.from_stage <= 5) {
        print_stage_header("Stage 5: Generating Reports");

        ReportConfig report_config;
        report_config.title = context_.title;
        report_config.source_document = context_.source_name;
        report_config.max_examples_per_type = options_.max_examples;
        report_config.include_evidence = true;
        report_config.include_statistics = true;
        report_config.llm_examples_per_type = 1;

        const std::string extraction_stats_path = context_.paths.step2_dir + "/extraction_stats.json";
        if (fs::exists(extraction_stats_path)) {
            try {
                std::ifstream extraction_stream(extraction_stats_path);
                extraction_stream >> report_config.pipeline_stats;
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not load extraction statistics: " << e.what() << "\n";
            }
        }

        const std::string qc_stats_path = context_.paths.step3_dir + "/cleaning_report.json";
        if (fs::exists(qc_stats_path)) {
            try {
                std::ifstream qc_stream(qc_stats_path);
                nlohmann::json qc_json;
                qc_stream >> qc_json;
                report_config.pipeline_stats["quality_control"] = qc_json;
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not load QC statistics: " << e.what() << "\n";
            }
        }

        ReportGenerator report_generator(context_.graph);
        auto report_llm = LLMProviderFactory::create_from_config_file(options_.config_path);
        if (report_llm) {
            report_config.include_llm_examples = true;
            report_generator.set_llm_provider(std::move(report_llm));
            std::cout << "  LLM examples enabled for report synthesis.\n";
        }

        report_config.format = ReportFormat::MARKDOWN;
        const std::string markdown_report = report_generator.generate(context_.insights, report_config);
        const std::string markdown_path = context_.paths.step5_dir + "/report.md";
        report_generator.save_to_file(markdown_path, markdown_report);
        std::cout << "  Saved: Step_5_Discovery/report.md\n";

        report_config.format = ReportFormat::HTML;
        const std::string html_report = report_generator.generate_html(context_.insights, report_config);
        const std::string html_path = context_.paths.step5_dir + "/report.html";
        report_generator.save_to_file(html_path, html_report);
        std::cout << "  Saved: Step_5_Discovery/report.html\n";

        const std::string pattern_library_path = context_.paths.step5_dir + "/pattern_library.json";
        report_generator.export_pattern_library(context_.insights, pattern_library_path);
        std::cout << "  Saved: Step_5_Discovery/pattern_library.json\n";
    } else {
        print_stage_header("Stage 5: Generating Reports [SKIPPED]");
        std::cout << "  Using existing report files\n";
    }

    std::cout << "  Stage 5 time: " << format_duration(std::chrono::steady_clock::now() - stage_start) << "\n";
}

void PipelineRunner::finalize_manifest() {
    const auto stage_start = std::chrono::steady_clock::now();
    print_stage_header("Stage 6: Finalizing");

    nlohmann::json manifest;
    if (options_.from_stage > 1 && fs::exists(context_.paths.manifest_path)) {
        std::ifstream manifest_stream(context_.paths.manifest_path);
        manifest = nlohmann::json::parse(manifest_stream);
    }

    manifest["run_id"] = context_.paths.run_id;
    manifest["last_updated_utc"] = current_utc_timestamp();
    if (options_.from_stage == 1 || !manifest.contains("created_utc")) {
        manifest["created_utc"] = manifest["last_updated_utc"];
    }

    manifest["title"] = context_.title;
    manifest["pipeline"]["from_stage"] = options_.from_stage;
    manifest["pipeline"]["completed_stages"] = nlohmann::json::array({1, 2, 3, 4, 5, 6});

    manifest["sources"] = nlohmann::json::array();
    for (const auto& path : context_.pdf_paths) {
        manifest["sources"].push_back(path);
    }

    manifest["statistics"]["entities"] = context_.graph_stats.num_nodes;
    manifest["statistics"]["relationships"] = context_.graph_stats.num_edges;
    manifest["statistics"]["insights"] = context_.insights.insights.size();
    for (const auto& [type, count] : context_.insight_counts) {
        manifest["statistics"]["insights_by_type"][insight_type_to_string(type)] = count;
    }

    manifest["artifacts"]["graph"] = "Step_4_GraphBuilding/graph.json";
    if (options_.preprocess) {
        manifest["artifacts"]["graph_raw"] = "Step_4_GraphBuilding/graph_raw.json";
        manifest["preprocess"]["enabled"] = context_.preprocess_ran;
        manifest["preprocess"]["relations_normalized"] = context_.relations_normalized;
        manifest["preprocess"]["nodes_merged"] = context_.nodes_merged;
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
    manifest["artifacts"]["reports"]["pattern_library"] = "Step_5_Discovery/pattern_library.json";
    manifest["artifacts"]["extraction_stats"] = "Step_2_Extraction/extraction_stats.json";

    {
        std::ofstream manifest_stream(context_.paths.manifest_path);
        manifest_stream << manifest.dump(2);
    }
    std::cout << "  Saved: manifest.json\n";

    const std::string readme_path = context_.paths.run_dir + "/README.txt";
    std::ofstream readme(readme_path);
    readme << "Knowledge Discovery Run: " << context_.paths.run_id << "\n";
    readme << std::string(50, '=') << "\n\n";
    readme << "Title: " << context_.title << "\n";
    readme << "Sources: " << context_.source_name << "\n\n";
    readme << "Artifacts:\n";
    readme << "  Data:\n";
    readme << "    Step_4_GraphBuilding/graph.json         - Extracted knowledge graph\n";
    if (options_.preprocess) {
        readme << "    Step_4_GraphBuilding/graph_raw.json     - Raw graph prior to preprocessing\n";
    }
    readme << "    Step_5_Discovery/index.json             - S-component index\n";
    readme << "    Step_5_Discovery/insights.json          - Discovered insights\n";
    readme << "    Step_5_Discovery/augmentation.json      - Augmentation overlay data\n";
    readme << "    Step_2_Extraction/extraction_stats.json - Pipeline statistics\n";
    readme << "    manifest.json                           - Run metadata\n\n";
    readme << "  Visualizations:\n";
    readme << "    Step_5_Discovery/graph.html             - Interactive 3D graph viewer\n";
    readme << "    Step_5_Discovery/graph_augmented.html   - Augmented view with insights\n";
    readme << "    Step_5_Discovery/graph_rag.html         - Graph-RAG chat viewer\n";
    readme << "    Step_5_Discovery/graph.dot              - GraphViz DOT format\n\n";
    readme << "  Reports:\n";
    readme << "    Step_5_Discovery/report.md              - Markdown report\n";
    readme << "    Step_5_Discovery/report.html            - Styled HTML report\n";
    readme << "    Step_5_Discovery/pattern_library.json   - Structured pattern export\n\n";
    readme << "To view:\n";
    readme << "  cd " << context_.paths.run_dir << "\n";
    readme << "  python3 -m http.server 8080\n";
    readme << "  # Open http://localhost:8080/Step_5_Discovery/graph_augmented.html\n";
    readme << "  # Or open http://localhost:8080/Step_5_Discovery/report.html\n";
    std::cout << "  Saved: README.txt\n";
    std::cout << "  Stage 6 time: " << format_duration(std::chrono::steady_clock::now() - stage_start) << "\n";
}

void PipelineRunner::print_summary(std::chrono::steady_clock::time_point pipeline_start) const {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "  Pipeline Complete!\n";
    std::cout << "======================================================================\n";
    std::cout << "\n";
    std::cout << "Run ID:       " << context_.paths.run_id << "\n";
    std::cout << "Output:       " << context_.paths.run_dir << "/\n";
    std::cout << "Entities:     " << context_.graph_stats.num_nodes << "\n";
    std::cout << "Relations:    " << context_.graph_stats.num_edges << "\n";
    std::cout << "Insights:     " << context_.insights.insights.size() << "\n";
    std::cout << "\n";
    std::cout << "To view results:\n";
    std::cout << "  cd " << context_.paths.run_dir << " && python3 -m http.server 8080\n";
    std::cout << "  Open: http://localhost:8080/Step_5_Discovery/graph_augmented.html\n";
    std::cout << "  Or:   http://localhost:8080/Step_5_Discovery/report.html\n";
    std::cout << "\n";

    const auto pipeline_end = std::chrono::steady_clock::now();
    const auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(pipeline_end - pipeline_start);
    const double total_minutes = total_duration.count() / 60.0;
    std::cout << "Total runtime: " << std::fixed << std::setprecision(2) << total_minutes << " minutes\n";
    std::cout << "\n";
}

}  // namespace kg

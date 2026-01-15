# Knowledge Graph Extraction System - Design Document

## Executive Summary

This document outlines the architecture for a generic C++ WSL application that extracts Knowledge Graphs (KGs) and Knowledge Hypergraphs from PDF corpora using multiple LLM providers and extraction methodologies.

## 1. System Overview

### 1.1 Core Objectives
- **Modularity**: Support multiple KG extraction methodologies through plugin architecture
- **LLM Flexibility**: Seamlessly integrate OpenAI, Gemini, and future LLM providers
- **Scalability**: Process large PDF corpora efficiently
- **Extensibility**: Easy addition of new extraction methods and LLM providers
- **Future-Proof**: Initial non-agentic pipeline with foundation for agentic evolution

### 1.2 Key Features
- PDF text extraction with metadata preservation
- Multiple KG extraction methodologies (pairwise, hypergraph, n-ary relations)
- Unified LLM client interface supporting OpenAI and Gemini
- Graph storage with multiple export formats (JSON, GraphML, DOT, Neo4j)
- Configuration-driven pipeline execution
- Comprehensive logging and error handling
- Batch processing with progress tracking

---

## 2. Architecture Design

### 2.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                      │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   CLI      │  │  Config      │  │  Pipeline    │        │
│  │   Parser   │  │  Manager     │  │  Orchestrator│        │
│  └────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│                   Processing Pipeline Layer                 │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   PDF      │  │  Extraction  │  │  Graph       │        │
│  │   Loader   │  │  Strategy    │  │  Builder     │        │
│  └────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│                      LLM Provider Layer                     │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │  OpenAI    │  │   Gemini     │  │  Abstract    │        │
│  │  Client    │  │   Client     │  │  LLM Client  │        │
│  └────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│                    Data Storage Layer                       │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │ Hypergraph │  │  Standard    │  │  Export      │        │
│  │   Store    │  │  Graph Store │  │  Handlers    │        │
│  └────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│                    Utility Layer                            │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │  Logger    │  │  Error       │  │  Cache       │        │
│  │            │  │  Handler     │  │  Manager     │        │
│  └────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Component Design

### 3.1 Core Components

#### 3.1.1 Document Processing Module

**PDFProcessor**
```cpp
class PDFProcessor {
public:
    struct DocumentMetadata {
        std::string filename;
        std::string filepath;
        int page_count;
        std::string extraction_method;
        std::chrono::system_clock::time_point processed_time;
    };

    struct PageContent {
        int page_number;
        std::string text;
        std::vector<std::string> tables;  // Future: structured table data
        std::vector<std::string> figures; // Future: figure captions
    };

    virtual ~PDFProcessor() = default;
    virtual std::vector<PageContent> extract_pages(const std::string& pdf_path) = 0;
    virtual DocumentMetadata get_metadata(const std::string& pdf_path) = 0;
};

class PopplerPDFProcessor : public PDFProcessor {
    // Implementation using poppler-cpp
};
```

**TextChunker**
```cpp
class TextChunker {
public:
    struct Chunk {
        std::string text;
        int start_page;
        int end_page;
        size_t start_offset;
        size_t end_offset;
        std::string chunk_id;
    };

    enum ChunkStrategy {
        FIXED_SIZE,
        SENTENCE_BOUNDARY,
        PARAGRAPH_BOUNDARY,
        SEMANTIC_SIMILARITY
    };

    std::vector<Chunk> chunk_text(
        const std::string& text,
        ChunkStrategy strategy,
        size_t target_size,
        size_t overlap = 0
    );
};
```

---

#### 3.1.2 LLM Provider Interface

**Abstract Base Class**
```cpp
class LLMProvider {
public:
    struct RequestConfig {
        std::string model;
        double temperature = 0.0;
        int max_tokens = 4096;
        bool json_mode = true;
        int retry_count = 3;
        std::chrono::milliseconds timeout{30000};
    };

    struct Response {
        std::string content;
        int tokens_used;
        std::chrono::milliseconds latency;
        bool success;
        std::string error_message;
    };

    virtual ~LLMProvider() = default;
    virtual Response query(
        const std::string& prompt,
        const RequestConfig& config
    ) = 0;
    virtual std::string get_provider_name() const = 0;
    virtual bool is_available() const = 0;
};
```

**Concrete Implementations**
```cpp
class OpenAIProvider : public LLMProvider {
private:
    std::string api_key_;
    std::string base_url_ = "https://api.openai.com/v1";

public:
    explicit OpenAIProvider(std::string api_key);
    Response query(const std::string& prompt, const RequestConfig& config) override;
    std::string get_provider_name() const override { return "OpenAI"; }
    bool is_available() const override;
};

class GeminiProvider : public LLMProvider {
private:
    std::string api_key_;
    std::string base_url_ = "https://generativelanguage.googleapis.com/v1beta";

public:
    explicit GeminiProvider(std::string api_key);
    Response query(const std::string& prompt, const RequestConfig& config) override;
    std::string get_provider_name() const override { return "Gemini"; }
    bool is_available() const override;
};

// Factory for creating LLM providers
class LLMProviderFactory {
public:
    static std::unique_ptr<LLMProvider> create(
        const std::string& provider_name,
        const std::string& api_key
    );
};
```

---

#### 3.1.3 Extraction Strategy Pattern

**Abstract Strategy**
```cpp
class ExtractionStrategy {
public:
    struct ExtractionResult {
        std::vector<KnowledgeTriple> triples;
        std::vector<HyperEdge> hyperedges;
        nlohmann::json metadata;
        bool success;
        std::string error_message;
    };

    struct KnowledgeTriple {
        std::string subject;
        std::string predicate;
        std::string object;
        std::string source_chunk_id;
        double confidence = 1.0;
    };

    struct HyperEdge {
        std::vector<std::string> sources;
        std::string relation;
        std::vector<std::string> targets;
        std::string source_chunk_id;
        double confidence = 1.0;
    };

    virtual ~ExtractionStrategy() = default;
    virtual ExtractionResult extract(
        const std::string& text,
        LLMProvider& llm,
        const std::string& chunk_id
    ) = 0;
    virtual std::string get_strategy_name() const = 0;
};
```

**Concrete Strategies**
```cpp
// 1. Traditional pairwise triple extraction
class PairwiseTripleExtraction : public ExtractionStrategy {
public:
    ExtractionResult extract(
        const std::string& text,
        LLMProvider& llm,
        const std::string& chunk_id
    ) override;
    std::string get_strategy_name() const override { return "PairwiseTriple"; }
};

// 2. Hypergraph extraction (as in the paper)
class HypergraphExtraction : public ExtractionStrategy {
public:
    enum ExtractionPass {
        GRAMMATICAL_EXACT,
        SEMANTIC_CONSERVATIVE
    };

    ExtractionResult extract(
        const std::string& text,
        LLMProvider& llm,
        const std::string& chunk_id
    ) override;
    std::string get_strategy_name() const override { return "Hypergraph"; }

private:
    ExtractionResult grammatical_pass(const std::string& text, LLMProvider& llm);
    ExtractionResult semantic_pass(const std::string& text, LLMProvider& llm);
};

// 3. N-ary relation extraction
class NAryRelationExtraction : public ExtractionStrategy {
public:
    ExtractionResult extract(
        const std::string& text,
        LLMProvider& llm,
        const std::string& chunk_id
    ) override;
    std::string get_strategy_name() const override { return "NAryRelation"; }
};

// 4. Composite material specific extraction
class CompositeMaterialExtraction : public ExtractionStrategy {
public:
    ExtractionResult extract(
        const std::string& text,
        LLMProvider& llm,
        const std::string& chunk_id
    ) override;
    std::string get_strategy_name() const override { return "CompositeMaterial"; }
private:
    std::vector<std::string> detect_composite_patterns(const std::string& text);
};
```

---

#### 3.1.4 Graph Data Structures

**Standard Graph (Pairwise)**
```cpp
class StandardGraph {
public:
    struct Node {
        std::string id;
        std::string label;
        std::map<std::string, std::string> properties;
        int degree = 0;
    };

    struct Edge {
        std::string id;
        std::string source;
        std::string target;
        std::string relation;
        std::map<std::string, std::string> properties;
    };

    void add_node(const Node& node);
    void add_edge(const Edge& edge);
    Node* get_node(const std::string& id);
    std::vector<Edge> get_edges(const std::string& node_id);

    // Graph operations
    std::vector<std::string> find_path(const std::string& start, const std::string& end);
    std::vector<Node> get_neighbors(const std::string& node_id, int hops = 1);
    std::map<std::string, int> compute_node_degrees();

    // Export methods
    void export_to_json(const std::string& filename);
    void export_to_graphml(const std::string& filename);
    void export_to_dot(const std::string& filename);

private:
    std::map<std::string, Node> nodes_;
    std::vector<Edge> edges_;
    std::map<std::string, std::vector<std::string>> adjacency_list_;
};
```

**Hypergraph (Higher-Order)**
```cpp
class Hypergraph {
public:
    struct HyperNode {
        std::string id;
        std::string label;
        std::map<std::string, std::string> properties;
        std::vector<std::string> incident_edges;
    };

    struct HyperEdge {
        std::string id;
        std::vector<std::string> sources;
        std::string relation;
        std::vector<std::string> targets;
        std::map<std::string, std::string> properties;
        std::string provenance; // chunk_id
    };

    void add_hyperedge(const HyperEdge& edge);
    void merge_similar_nodes(double similarity_threshold = 0.95);

    // Hypergraph-specific operations
    std::vector<HyperEdge> find_s_connected_path(
        const std::string& start,
        const std::string& end,
        int min_intersection_size = 1
    );
    std::vector<std::vector<HyperEdge>> find_k_shortest_paths(
        const std::string& start,
        const std::string& end,
        int k = 3,
        int min_intersection_size = 1
    );
    std::map<std::string, int> compute_node_degrees();
    std::vector<std::string> get_high_degree_hubs(int top_k = 20);

    // Projections
    StandardGraph to_clique_expansion();
    StandardGraph to_star_expansion();

    // Export methods
    void export_to_json(const std::string& filename);
    void export_to_dot(const std::string& filename);
    nlohmann::json to_incidence_matrix();

private:
    std::map<std::string, HyperNode> nodes_;
    std::map<std::string, HyperEdge> hyperedges_;
    std::map<std::string, std::vector<std::string>> node_to_edges_;
};
```

---

#### 3.1.5 Pipeline Orchestration

**Main Pipeline Controller**
```cpp
class ExtractionPipeline {
public:
    struct PipelineConfig {
        std::string pdf_directory;
        std::string output_directory;
        std::string llm_provider; // "openai" or "gemini"
        std::string llm_api_key;
        std::string llm_model;
        std::string extraction_strategy; // "pairwise", "hypergraph", "nary", "composite"
        bool enable_caching = true;
        bool enable_deduplication = true;
        int batch_size = 10;
        int num_threads = 4;
    };

    explicit ExtractionPipeline(const PipelineConfig& config);

    void run();
    void process_single_pdf(const std::string& pdf_path);
    void export_results(const std::string& format); // "json", "graphml", "dot", "neo4j"

private:
    PipelineConfig config_;
    std::unique_ptr<PDFProcessor> pdf_processor_;
    std::unique_ptr<LLMProvider> llm_provider_;
    std::unique_ptr<ExtractionStrategy> extraction_strategy_;
    std::unique_ptr<Hypergraph> hypergraph_;
    std::unique_ptr<StandardGraph> standard_graph_;

    void load_configuration();
    void initialize_components();
    std::vector<std::string> discover_pdfs();
    void process_batch(const std::vector<std::string>& pdf_paths);
    void deduplicate_entities();
    void merge_graphs();
};
```

---

## 4. Configuration System

### 4.1 Configuration File Format (YAML)

```yaml
# config.yaml
pipeline:
  pdf_directory: "./tests"
  output_directory: "./output"
  batch_size: 10
  num_threads: 4
  enable_caching: true
  enable_deduplication: true

llm:
  provider: "openai"  # or "gemini"
  api_key: "${OPENAI_API_KEY}"  # Environment variable
  model: "gpt-4o"  # or "gemini-1.5-flash"
  temperature: 0.0
  max_tokens: 4096
  timeout_ms: 30000
  retry_count: 3

extraction:
  strategy: "hypergraph"  # "pairwise", "hypergraph", "nary", "composite"
  chunking:
    strategy: "fixed_size"  # "fixed_size", "sentence", "paragraph", "semantic"
    chunk_size: 10000
    overlap: 500

  hypergraph_options:
    enable_dual_pass: true
    similarity_threshold: 0.95
    merge_frequency: 10

pdf_processing:
  extractor: "poppler"
  preserve_layout: true
  extract_tables: false
  extract_figures: false

export:
  formats:
    - "json"
    - "dot"
    - "graphml"
  include_metadata: true
  include_provenance: true

logging:
  level: "info"  # "debug", "info", "warn", "error"
  file: "./logs/extraction.log"
  console: true
```

---

## 5. Directory Structure

```
KnowledgeGraphExtractor/
├── CMakeLists.txt
├── config.yaml
├── README.md
├── DESIGN.md
│
├── include/
│   ├── core/
│   │   ├── pipeline.hpp
│   │   ├── config_manager.hpp
│   │   └── types.hpp
│   │
│   ├── document/
│   │   ├── pdf_processor.hpp
│   │   ├── text_chunker.hpp
│   │   └── document_metadata.hpp
│   │
│   ├── llm/
│   │   ├── llm_provider.hpp
│   │   ├── openai_provider.hpp
│   │   ├── gemini_provider.hpp
│   │   └── llm_factory.hpp
│   │
│   ├── extraction/
│   │   ├── extraction_strategy.hpp
│   │   ├── pairwise_extraction.hpp
│   │   ├── hypergraph_extraction.hpp
│   │   ├── nary_extraction.hpp
│   │   └── composite_extraction.hpp
│   │
│   ├── graph/
│   │   ├── standard_graph.hpp
│   │   ├── hypergraph.hpp
│   │   ├── graph_algorithms.hpp
│   │   └── graph_exporters.hpp
│   │
│   └── utils/
│       ├── logger.hpp
│       ├── error_handler.hpp
│       ├── cache_manager.hpp
│       ├── http_client.hpp
│       └── string_utils.hpp
│
├── src/
│   ├── core/
│   │   ├── pipeline.cpp
│   │   └── config_manager.cpp
│   │
│   ├── document/
│   │   ├── poppler_processor.cpp
│   │   └── text_chunker.cpp
│   │
│   ├── llm/
│   │   ├── openai_provider.cpp
│   │   ├── gemini_provider.cpp
│   │   └── llm_factory.cpp
│   │
│   ├── extraction/
│   │   ├── pairwise_extraction.cpp
│   │   ├── hypergraph_extraction.cpp
│   │   ├── nary_extraction.cpp
│   │   └── composite_extraction.cpp
│   │
│   ├── graph/
│   │   ├── standard_graph.cpp
│   │   ├── hypergraph.cpp
│   │   ├── graph_algorithms.cpp
│   │   └── graph_exporters.cpp
│   │
│   ├── utils/
│   │   ├── logger.cpp
│   │   ├── cache_manager.cpp
│   │   └── http_client.cpp
│   │
│   └── main.cpp
│
├── tests/
│   ├── 1page.pdf
│   ├── unit/
│   │   ├── test_pdf_processor.cpp
│   │   ├── test_llm_providers.cpp
│   │   ├── test_extraction.cpp
│   │   └── test_graph.cpp
│   └── integration/
│       └── test_full_pipeline.cpp
│
├── external/
│   ├── nlohmann_json/
│   ├── yaml-cpp/
│   └── spdlog/
│
└── output/
    ├── graphs/
    ├── logs/
    └── cache/
```

---

## 6. Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
- [ ] Set up CMake build system
- [ ] Implement configuration manager (YAML parsing)
- [ ] Create logger and error handling utilities
- [ ] Implement HTTP client wrapper for CURL
- [ ] Design and implement core data structures (Node, Edge, HyperEdge)

### Phase 2: Document Processing (Week 3)
- [ ] Implement PDFProcessor with poppler-cpp
- [ ] Create TextChunker with multiple strategies
- [ ] Add document metadata extraction
- [ ] Unit tests for PDF processing

### Phase 3: LLM Integration (Week 4)
- [ ] Implement abstract LLMProvider interface
- [ ] Create OpenAIProvider implementation
- [ ] Create GeminiProvider implementation
- [ ] Add LLMProviderFactory
- [ ] Implement retry logic and error handling
- [ ] Unit tests for LLM providers

### Phase 4: Extraction Strategies (Weeks 5-6)
- [ ] Implement PairwiseTripleExtraction
- [ ] Implement HypergraphExtraction (dual-pass)
- [ ] Implement NAryRelationExtraction
- [ ] Implement CompositeMaterialExtraction
- [ ] Create prompt templates for each strategy
- [ ] Unit tests for extraction strategies

### Phase 5: Graph Management (Week 7)
- [ ] Implement StandardGraph class
- [ ] Implement Hypergraph class
- [ ] Add graph algorithms (pathfinding, degree calculations)
- [ ] Implement node deduplication with embeddings
- [ ] Unit tests for graph operations

### Phase 6: Export and Visualization (Week 8)
- [ ] Implement JSON exporter
- [ ] Implement GraphML exporter
- [ ] Implement DOT (Graphviz) exporter
- [ ] Add Neo4j export support
- [ ] Create visualization helper scripts

### Phase 7: Pipeline Integration (Week 9)
- [ ] Implement ExtractionPipeline orchestrator
- [ ] Add batch processing support
- [ ] Implement caching mechanism
- [ ] Add progress tracking and reporting
- [ ] Integration tests

### Phase 8: Testing and Documentation (Week 10)
- [ ] Comprehensive unit tests
- [ ] Integration tests with test PDFs
- [ ] Performance benchmarking
- [ ] User documentation
- [ ] API documentation

---

## 7. Technology Stack

### Core Libraries
- **C++ Standard**: C++17 or C++20
- **JSON**: nlohmann/json
- **YAML**: yaml-cpp
- **HTTP**: libcurl
- **PDF**: poppler-cpp
- **Logging**: spdlog
- **Testing**: Google Test
- **Build**: CMake 3.20+

### Optional/Future Libraries
- **Embeddings**: onnxruntime (for local sentence embeddings)
- **Graph Algorithms**: Boost Graph Library (BGL)
- **Database**: Neo4j C++ driver

---

## 8. Usage Examples

### 8.1 Basic Usage

```bash
# Run with default configuration
./kg_extractor --config config.yaml

# Specify custom PDF directory and LLM provider
./kg_extractor \
  --pdf-dir ./my_pdfs \
  --llm-provider openai \
  --llm-model gpt-4o \
  --strategy hypergraph \
  --output-dir ./results

# Process single PDF
./kg_extractor \
  --single-pdf ./tests/paper.pdf \
  --strategy pairwise \
  --export-format json
```

### 8.2 Configuration-Driven

```cpp
// main.cpp
int main(int argc, char* argv[]) {
    // Load configuration
    ConfigManager config("config.yaml");

    // Create pipeline
    ExtractionPipeline::PipelineConfig pipeline_config;
    pipeline_config.pdf_directory = config.get<std::string>("pipeline.pdf_directory");
    pipeline_config.llm_provider = config.get<std::string>("llm.provider");
    pipeline_config.llm_api_key = std::getenv("OPENAI_API_KEY");
    pipeline_config.extraction_strategy = config.get<std::string>("extraction.strategy");

    ExtractionPipeline pipeline(pipeline_config);

    // Run extraction
    pipeline.run();

    // Export results
    pipeline.export_results("json");
    pipeline.export_results("dot");

    return 0;
}
```

---

## 9. Extension Points for Agentic Evolution

### 9.1 Agent Architecture Hooks

The current design includes extension points for future agentic capabilities:

```cpp
// Future: Agent interface for autonomous reasoning
class GraphAgent {
public:
    virtual std::vector<HyperEdge> reason(
        const Hypergraph& graph,
        const std::string& query
    ) = 0;
};

// Future: Multi-agent collaboration
class MultiAgentSystem {
    std::vector<std::unique_ptr<GraphAgent>> agents_;
public:
    void add_agent(std::unique_ptr<GraphAgent> agent);
    nlohmann::json collaborative_reasoning(const std::string& query);
};
```

### 9.2 Tool Interface for Agents

```cpp
class GraphTraversalTool {
public:
    std::vector<HyperEdge> find_paths(
        const std::string& start,
        const std::string& end,
        int intersection_size,
        int k_paths
    );

    std::vector<std::string> get_neighbors(
        const std::string& node_id,
        int hops
    );
};
```

---

## 10. Performance Considerations

### 10.1 Optimization Strategies
- **Parallel PDF Processing**: Use thread pool for batch processing
- **LLM Request Batching**: Batch multiple chunks in single request when possible
- **Caching**: Cache LLM responses to avoid redundant API calls
- **Incremental Graph Building**: Merge graphs periodically rather than at end
- **Memory Management**: Use smart pointers and RAII patterns

### 10.2 Scalability Targets
- Process 1000+ PDFs efficiently
- Handle graphs with 100k+ nodes and 500k+ edges
- Support concurrent LLM requests
- Minimize memory footprint through streaming processing

---

## 11. Error Handling and Logging

### 11.1 Error Categories
- PDF processing errors (corrupt files, extraction failures)
- LLM API errors (rate limits, timeouts, invalid responses)
- Graph operation errors (invalid paths, missing nodes)
- Configuration errors (invalid YAML, missing parameters)

### 11.2 Logging Strategy
- Structured logging with spdlog
- Log levels: DEBUG, INFO, WARN, ERROR
- Separate logs for:
  - Main pipeline execution
  - LLM API interactions
  - Graph operations
  - Errors and exceptions

---

## 12. Future Enhancements

### Short-Term (3-6 months)
- Add support for Claude/Anthropic API
- Implement local embedding models (ONNX)
- Add graph visualization web interface
- Support for incremental updates
- Enhanced entity deduplication

### Medium-Term (6-12 months)
- Agentic reasoning capabilities
- Multi-agent collaboration framework
- Graph-based RAG (Retrieval-Augmented Generation)
- Real-time processing pipeline
- Support for additional document formats (Word, HTML)

### Long-Term (12+ months)
- Distributed processing across multiple machines
- Graph database integration (Neo4j, ArangoDB)
- Advanced graph neural network integration
- Automated hypothesis generation
- Active learning for extraction improvement

---

## 13. Testing Strategy

### 13.1 Unit Tests
- Test each component in isolation
- Mock LLM responses for deterministic testing
- Test edge cases and error conditions

### 13.2 Integration Tests
- End-to-end pipeline tests with sample PDFs
- Test different extraction strategies
- Verify graph integrity and correctness

### 13.3 Performance Tests
- Benchmark PDF processing speed
- Measure LLM API latency
- Profile memory usage
- Test with large corpora

---

## 14. Documentation

### 14.1 User Documentation
- Installation guide
- Quick start tutorial
- Configuration reference
- Usage examples
- Troubleshooting guide

### 14.2 Developer Documentation
- Architecture overview
- API reference (Doxygen)
- Contributing guidelines
- Adding new extraction strategies
- Adding new LLM providers

---

## 15. Conclusion

This design provides a robust, modular foundation for knowledge graph extraction from PDF corpora. The architecture supports:

- **Flexibility**: Easy swapping of LLM providers and extraction strategies
- **Extensibility**: Plugin architecture for new components
- **Scalability**: Efficient batch processing and caching
- **Future-Proofing**: Foundation for agentic evolution

The phased implementation approach ensures steady progress while maintaining code quality and testability.


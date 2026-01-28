# Knowledge Graph Extractor

A generic C++ application for extracting Knowledge Graphs and Knowledge Hypergraphs from PDF corpora using multiple LLM providers and extraction methodologies.

## Overview

This project implements the hypergraph-based knowledge representation methodology from the paper:
**"Higher-Order Knowledge Representations for Agentic Scientific Reasoning"** (Stewart & Buehler, 2026)

### Key Features

- **Higher-Order Relationships**: Hypergraph data structure preserving multi-entity interactions
- **Multiple LLM Providers**: Seamless integration with OpenAI and Gemini
- **Flexible Extraction**: Multiple strategies (pairwise, hypergraph, n-ary relations)
- **Advanced Algorithms**: s-connected paths, k-shortest paths, node deduplication
- **Rich Analysis**: Degree distributions, power law fitting, hub detection
- **Multiple Export Formats**: JSON, GraphML, DOT (Graphviz), incidence matrices

## Project Status

✅ **Hypergraph Data Structure** - Complete
✅ **PDF Processing** - Complete
✅ **LLM Integration** - Complete (OpenAI & Gemini)
✅ **Full Pipeline Integration** - Complete
🚧 **Agentic Reasoning** - Planned

## Prerequisites

### Required
- CMake 3.15+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- libcurl
- nlohmann/json (auto-downloaded if not found)

### Optional
- poppler-cpp (for PDF processing)
- Google Test (for unit tests)
- Doxygen (for documentation)

### Installation (Ubuntu/WSL)

```bash
# Update package list
sudo apt-get update

# Install required packages
sudo apt-get install -y \
    cmake \
    g++ \
    libcurl4-openssl-dev \
    nlohmann-json3-dev

# Install optional packages
sudo apt-get install -y \
    libpoppler-cpp-dev \
    libgtest-dev \
    doxygen \
    graphviz
```

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Run example
./bin/hypergraph_example
```

### Build Options

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Disable tests
cmake -DBUILD_TESTS=OFF ..

# Enable documentation
cmake -DBUILD_DOCS=ON ..

# Specify installation directory
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

## Quick Start

### Example 1: Basic Hypergraph Usage

```cpp
#include "graph/hypergraph.hpp"

using namespace kg;

int main() {
    // Create hypergraph
    Hypergraph graph;

    // Add hyperedge: [PCL, chitosan] compose [nanofibers]
    graph.add_hyperedge(
        {"PCL", "chitosan"},      // sources
        "compose",                 // relation
        {"nanofibers"}            // targets
    );

    // Add complex hyperedge: multiple sources and targets
    graph.add_hyperedge(
        {"Cerium oxide", "Chitosan", "Hydroxyethylcellulose"},
        "compose",
        {"antibacterial films", "biocompatible scaffolds"}
    );

    // Get statistics
    auto stats = graph.compute_statistics();
    std::cout << "Nodes: " << stats.num_nodes << "\n";
    std::cout << "Edges: " << stats.num_edges << "\n";

    // Export
    graph.export_to_json("output.json");
    graph.export_to_dot("output.dot");

    return 0;
}
```

### Example 2: Path Finding

```cpp
// Find shortest s-connected path
auto path = graph.find_shortest_path("PCL", "bone regeneration", 1);

if (!path.empty()) {
    for (const auto& edge : path) {
        std::cout << edge.relation << "\n";
    }
}

// Find multiple paths
auto k_paths = graph.find_k_shortest_paths("chitosan", "cell adhesion", 3, 1);

std::cout << "Found " << k_paths.paths.size() << " paths\n";
```

### Example 3: Graph Analysis

```cpp
// Get top hubs
auto hubs = graph.get_top_hubs(20);
for (const auto& [node_id, degree] : hubs) {
    std::cout << node_id << ": " << degree << "\n";
}

// Compute degree distribution
auto distribution = graph.compute_degree_distribution();

// Fit power law
auto [exponent, r_squared] = graph.fit_power_law();
std::cout << "Power law exponent: " << exponent << "\n";
std::cout << "R²: " << r_squared << "\n";

// Find s-connected components
auto components = graph.find_s_connected_components(2);
std::cout << "Found " << components.size() << " components\n";
```

### Example 4: Node Deduplication

```cpp
// Add embeddings to nodes (from your embedding model)
for (auto& [id, node] : graph.get_all_nodes()) {
    node.embedding = your_embedding_model.encode(node.label);
    graph.add_node(node);  // Update with embedding
}

// Merge similar nodes (e.g., "PCL" and "polycaprolactone")
graph.merge_similar_nodes(0.95);  // 95% similarity threshold
```

## Running the Examples

### Hypergraph Example

```bash
cd build
./bin/hypergraph_example
```

This will:
1. Create an `output_json/` directory
2. Build a sample knowledge hypergraph
3. Demonstrate all major features
4. Export to JSON and DOT formats in `output_json/`
5. Perform graph analysis

**Output files created:**
- `output_json/hypergraph_example.json` - Complete graph serialization
- `output_json/hypergraph_example.dot` - Graphviz visualization format
- `output_json/incidence_matrix.json` - Node × hyperedge matrix

### Historical Knowledge Example

```bash
cd build
./bin/historical_knowledge_example
```

This example extracts a knowledge graph from [tests/1page.pdf](tests/1page.pdf), demonstrating:
1. Real-world knowledge extraction from historical text
2. Tracking knowledge transmission across time and geography
3. Representing two interweaving threads (history and mathematics)
4. Path analysis showing how knowledge flowed from Baghdad to the metric system

**Graph characteristics:**
- 75 entities (people, places, concepts, artifacts)
- 42 relationships (spanning centuries of knowledge transmission)
- Scale-free topology (R² = 0.858) with key hubs: Baghdad, geometry, Euclid's Elements
- 14 knowledge clusters identified

**Output files created:**
- `output_json/historical_knowledge.json` - Complete graph (23KB)
- `output_json/historical_knowledge.dot` - Visualization format (9.3KB)
- `output_json/historical_knowledge_matrix.json` - Incidence matrix (31KB)

### PDF Processing Example

```bash
cd build
./bin/pdf_processing_example
```

This example demonstrates PDF text extraction and chunking strategies:
1. Loading PDFs using Poppler
2. Extracting text and metadata (title, author, pages, word count)
3. Multiple chunking strategies:
   - **Fixed-size**: 500 chars with 100 char overlap (6 chunks)
   - **Page-based**: One chunk per page (1 chunk)
   - **Paragraph-based**: Group paragraphs (semantic units)
   - **Sentence-based**: Group sentences (5 chunks)
4. Batch processing multiple PDFs
5. Exporting chunks to JSON for downstream processing

**Chunking strategies comparison:**
- Fixed-size: Uniform chunks, good for consistent processing
- Page-based: Natural document boundaries, preserves context
- Paragraph: Semantic units, best for topic coherence
- Sentence: Granular control, good for fine-grained extraction

**Output files created:**
- `output_json/chunks_fixed_size.json` - Fixed-size chunks with metadata
- `output_json/chunks_page_based.json` - Page-based chunks

**Use cases:**
- Feed chunks to LLM for knowledge extraction
- Create document embeddings
- Topic modeling and clustering
- Question answering systems

### LLM Knowledge Extraction Example

```bash
cd build
./bin/llm_extraction_example
```

This example demonstrates the complete PDF → LLM → Hypergraph pipeline:

1. **LLM Provider Configuration**: Supports OpenAI and Gemini APIs
2. **Relation Extraction**: Extract entities and relationships from text using LLMs
3. **Hypergraph Building**: Convert extracted relations to hypergraph structure
4. **Batch Processing**: Process multiple text chunks with rate limiting
5. **PDF Integration**: Extract from PDF chunks automatically

**Setup:**

**Option 1: Config File (Recommended)**
```bash
# Create config from template
cp .llm_config.json.example .llm_config.json

# Edit with your API key (OpenAI or Gemini)
nano .llm_config.json

# Run extraction
./build/bin/llm_extraction_example
```

**Option 2: Environment Variables**
```bash
# For OpenAI
export OPENAI_API_KEY='your-key-here'

# OR for Gemini
export GEMINI_API_KEY='your-key-here'
export KG_LLM_PROVIDER='gemini'

# Run extraction
./build/bin/llm_extraction_example
```

See [CONFIG_GUIDE.md](CONFIG_GUIDE.md) for detailed configuration options.

**Demo Mode:**
Runs without API keys to demonstrate the workflow with sample data.

**Features:**
- Automatic retry with exponential backoff
- Token usage tracking
- Confidence scores for extracted relations
- JSON output for all extractions
- Integration with PDF chunking strategies
- Rate limiting for batch processing

**Output files:**
- `output_json/extracted_graph.json` - Hypergraph from single extraction
- `output_json/batch_extracted_graph.json` - Hypergraph from PDF batch
- `output_json/demo_graph.json` - Demo mode output

**Complete Pipeline:**
```
PDF → Text Chunks → LLM Extraction → Relations → Hypergraph
```

### KG CLI Pipeline (Recommended)

The `kg` CLI runs the full extraction → discovery → reporting pipeline and supports optional preprocessing.

**Full pipeline (default operators):**
```bash
./build/bin/kg run -i document.pdf -t "My Knowledge Graph"
```

**Enable preprocessing (normalize relations + merge aliases):**
```bash
./build/bin/kg run -i document.pdf -t "My Knowledge Graph" --preprocess
```

**Custom operators:**
```bash
./build/bin/kg run -i document.pdf -p "bridges,completions,core_periphery,centrality,claim_stance,uncertainty_sampling"
```

**Output Structure:**
```
runs/run_YYYYMMDD_HHMMSS/
├── graph.json              # Extracted (or preprocessed) hypergraph
├── graph_raw.json          # Raw graph before preprocessing (if enabled)
├── index.json              # S-component index
├── insights.json           # Discovery insights
├── augmentation.json       # Augmentation overlay
├── graph.html              # Baseline viewer
├── graph_augmented.html    # Augmented viewer
├── graph.dot               # GraphViz DOT
├── report.md               # Markdown report
└── report.html             # HTML report
```

See [PIPELINE.md](PIPELINE.md) for the full operator list and CLI usage.

### End-to-End Pipeline Example

```bash
cd build
./bin/pipeline_example
```

This is the **complete production-ready pipeline** that integrates all components:

**Features:**
- 📄 **PDF Processing**: Automatic text extraction and intelligent chunking
- 🤖 **LLM Integration**: OpenAI and Gemini API support
- 📊 **Batch Processing**: Process entire document collections
- 🔄 **Progress Tracking**: Real-time progress callbacks
- 💾 **Intermediate Saves**: Save per-document graphs and extractions
- 📈 **Statistics Tracking**: Token usage, timing, success rates
- 🔗 **Graph Merging**: Combine knowledge from multiple documents
- ⚙️ **Configurable**: JSON config files or environment variables

**Configuration:**

**Quick Setup:**
```bash
# 1. Create API key config
cp .llm_config.json.example .llm_config.json
# Edit with your API key (OpenAI or Gemini)

# 2. (Optional) Customize pipeline settings
cp example_pipeline_config.json my_config.json
# Edit chunking, output settings, etc.
```

**Example `.llm_config.json`:**
```json
{
  "provider": "openai",
  "api_key": "your-key-here",
  "model": "gpt-4",
  "temperature": 0.0,
  "max_tokens": 2000
}
```

**Or use environment variables:**
```bash
export OPENAI_API_KEY='your-key'
# OR
export GEMINI_API_KEY='your-key'
export KG_LLM_PROVIDER='gemini'
```

See [CONFIG_GUIDE.md](CONFIG_GUIDE.md) for all configuration options.

**Run Pipeline:**
```bash
# Process all PDFs in tests/ directory
./bin/pipeline_example

# Process specific files
./bin/pipeline_example doc1.pdf doc2.pdf doc3.pdf

# Use custom config
./bin/pipeline_example --config my_config.json
```

**Output Structure:**
```
output_json/
├── final_graph.json              # Complete merged hypergraph
├── final_graph.dot               # Visualization
├── pipeline_stats.json           # Execution statistics
├── <doc_id>_graph.json          # Per-document graphs
└── <doc_id>_extractions.json    # Raw LLM outputs
```

**Pipeline Statistics:**
The pipeline tracks comprehensive metrics:
- Documents processed/failed
- LLM API calls and token usage
- Timing breakdown (PDF, LLM, graph building)
- Final graph statistics (nodes, edges, topology)

**Production Use:**
```cpp
// Create pipeline
PipelineConfig config = PipelineConfig::from_json_file("config.json");
ExtractionPipeline pipeline(config);

// Set progress callback
pipeline.set_progress_callback([](const std::string& stage, int current, int total, const std::string& msg) {
    std::cout << stage << ": " << current << "/" << total << " - " << msg << std::endl;
});

// Process documents
Hypergraph graph = pipeline.process_directory("corpus/", true);

// Save results
graph.export_to_json("knowledge_graph.json");

// Get statistics
auto stats = pipeline.get_statistics();
stats.print_summary();
```

### Visualize Results

```bash
# Convert DOT to PNG using Graphviz
dot -Tpng output_json/hypergraph_example.dot -o output_json/hypergraph.png
dot -Tpng output_json/historical_knowledge.dot -o output_json/historical_knowledge.png

# View the graphs
xdg-open output_json/hypergraph.png  # Linux
open output_json/hypergraph.png      # macOS
```

## Architecture

See [DESIGN.md](DESIGN.md) for complete architecture documentation.

```
├── include/
│   └── graph/
│       └── hypergraph.hpp          # Main hypergraph interface
├── src/
│   └── graph/
│       ├── hypergraph.cpp          # Core implementation
│       └── hypergraph_extended.cpp # Export/import/analysis
├── examples/
│   └── hypergraph_example.cpp      # Usage examples
└── tests/
    └── unit/
        └── test_hypergraph.cpp     # Unit tests
```

## Hypergraph Data Structure

### Core Concepts

**HyperNode**: Entity in the knowledge graph
- Unique ID and label
- Properties (metadata)
- Optional embedding vector
- List of incident hyperedges

**HyperEdge**: Multi-entity relationship
- Multiple source nodes
- Named relation
- Multiple target nodes
- Provenance tracking (document, chunk, page)

**Example Hyperedge**:
```
Sources: [PCL, chitosan, collagen]
Relation: "form"
Targets: [composite scaffold, bone tissue]
```

### Key Algorithms

#### s-Connected Paths
Find paths where adjacent hyperedges share ≥s nodes:
```cpp
// Find path with at least 2 shared nodes at each step
auto path = graph.find_shortest_path("start", "end", 2);
```

#### K-Shortest Paths
Yen's algorithm adapted for hypergraphs:
```cpp
// Find 3 alternative paths
auto paths = graph.find_k_shortest_paths("start", "end", 3, 1);
```

#### Node Deduplication
Merge semantically similar nodes using embeddings:
```cpp
// Merge nodes with cosine similarity ≥ 0.95
graph.merge_similar_nodes(0.95);
```

## API Reference

### Creating Hypergraphs

```cpp
// Constructor
Hypergraph graph;

// Add hyperedge
std::string edge_id = graph.add_hyperedge(
    {"source1", "source2"},  // sources
    "relation",              // relation
    {"target1"}              // targets
);

// Add node explicitly
HyperNode node;
node.id = "PCL";
node.label = "Polycaprolactone";
node.properties["type"] = "polymer";
graph.add_node(node);
```

### Querying

```cpp
// Get node
const HyperNode* node = graph.get_node("PCL");

// Get hyperedge
const HyperEdge* edge = graph.get_hyperedge("edge_123");

// Get incident edges
auto edges = graph.get_incident_edges("PCL");

// Check existence
bool has_node = graph.has_node("PCL");
bool has_edge = graph.has_edge("edge_123");
```

### Graph Operations

```cpp
// Remove elements
graph.remove_hyperedge("edge_123");
graph.remove_node("PCL");

// Merge graphs
graph.merge(other_graph, true);  // true = deduplicate

// Clear
graph.clear();

// Size
size_t n_nodes = graph.num_nodes();
size_t n_edges = graph.num_edges();
```

### Path Finding

```cpp
// Shortest path
auto path = graph.find_shortest_path(start, end, min_intersection);

// K-shortest paths
auto result = graph.find_k_shortest_paths(start, end, k, min_intersection);

// Neighborhood
auto neighbors = graph.get_neighborhood(node_id, hops, min_intersection);

// Subgraph extraction
auto subgraph = graph.extract_subgraph(node_set);
```

### Analysis

```cpp
// Statistics
auto stats = graph.compute_statistics();

// Degree
int degree = graph.get_node_degree("PCL");
auto all_degrees = graph.compute_node_degrees();

// Hubs
auto hubs = graph.get_top_hubs(20);

// Distribution
auto dist = graph.compute_degree_distribution();

// Power law
auto [exponent, r2] = graph.fit_power_law();

// Rich club
double coef = graph.compute_rich_club_coefficient(threshold);

// Hub integration
auto scores = graph.compute_hub_integration_scores(20);

// Components
auto components = graph.find_s_connected_components(s);
```

### Export/Import

```cpp
// JSON
graph.export_to_json("output.json");
Hypergraph loaded = Hypergraph::load_from_json("input.json");

// DOT (Graphviz)
graph.export_to_dot("output.dot");

// Incidence matrix
auto matrix = graph.to_incidence_matrix();
```

## Testing

```bash
cd build
make test

# Or run directly
./bin/test_hypergraph --gtest_verbose
```

## Roadmap

### Phase 1: Foundation ✅
- [x] Hypergraph data structure
- [x] Core algorithms (paths, components)
- [x] Export/import functionality
- [x] Graph analysis tools

### Phase 2: Integration 🚧
- [ ] LLM provider interface
- [ ] OpenAI integration
- [ ] Gemini integration
- [ ] PDF processing

### Phase 3: Extraction 📋
- [ ] Pairwise triple extraction
- [ ] Hypergraph extraction (dual-pass)
- [ ] N-ary relation extraction
- [ ] Domain-specific extractors

### Phase 4: Pipeline 📋
- [ ] Configuration system
- [ ] Batch processing
- [ ] Caching mechanism
- [ ] Progress tracking

### Phase 5: Advanced 📋
- [ ] Embedding integration (ONNX)
- [ ] Graph database export (Neo4j)
- [ ] Web visualization interface
- [ ] Agentic reasoning framework

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Citation

If you use this code in your research, please cite:

```bibtex
@article{stewart2026higher,
  title={Higher-Order Knowledge Representations for Agentic Scientific Reasoning},
  author={Stewart, Isabella A. and Buehler, Markus J.},
  journal={arXiv preprint arXiv:2601.04878},
  year={2026}
}
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Authors

- **Design & Implementation**: Based on Stewart & Buehler (2026)
- **Development**: Your Team

## References

### Books

1. **Building Knowledge Graphs: A Practitioner's Guide**
   - Authors: Jesus Barrasa, Jim Webber
   - Publisher: O'Reilly Media, 2023
   - ISBN: 978-1-098-12711-4
   - A comprehensive guide to building knowledge graphs in practice

2. **Knowledge Graphs and LLMs in Action**
   - Authors: Alessandro Negro, Vlastimil Kus, Giuseppe Futia, Fabio
   - Publisher: Manning Publications, 2025
   - ISBN: 978-1-633-43989-4
   - Practical techniques for combining knowledge graphs with large language models

### Papers

- Stewart, I. A. & Buehler, M. J. (2026). "Higher-Order Knowledge Representations for Agentic Scientific Reasoning". arXiv:2601.04878

## Acknowledgments

- Original paper: Stewart & Buehler, "Higher-Order Knowledge Representations for Agentic Scientific Reasoning"
- nlohmann/json library
- Poppler PDF library
- Google Test framework

## Support

For questions or issues:
- Open an issue on GitHub
- Email: [your-email]
- Documentation: [DESIGN.md](DESIGN.md)

---

**Status**: Active Development
**Version**: 1.0.0
**Last Updated**: January 2025

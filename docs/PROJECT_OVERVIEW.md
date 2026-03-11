# Dynamic Knowledge Graph System - Holistic Project Overview

**Version**: 1.0.0
**Last Updated**: February 2026
**Project Type**: PhD Research - Knowledge Graph Extraction & Discovery

---

## 🎯 Project Vision

A complete end-to-end system for extracting knowledge graphs from academic PDFs, discovering insights through automated operators, and serving interactive Graph-RAG queries with 3D visualization.

**Research Goal**: Automatically construct and analyze hypergraph knowledge representations from scholarly documents, enabling novel insight discovery and intelligent question answering.

---

## 📐 System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    DYNAMIC KNOWLEDGE GRAPH SYSTEM                │
└─────────────────────────────────────────────────────────────────┘
         │
         ├─── 1. C++ EXTRACTION & DISCOVERY PIPELINE (kg CLI)
         │    └── PDF → Extract → Index → Discover → Render → Report
         │
         ├─── 2. GRAPH-RAG INTERACTIVE TOOL (Development)
         │    └── Kuzu DB + FastAPI + 3D Visualization + Chat
         │
         ├─── 3. DEPLOYMENT VERSION (Standalone)
         │    └── Minimal bundle for corporate/production use
         │
         └─── 4. PYTHON PORTING (Alternative)
              └── Lightweight Azure OpenAI extraction path
```

---

## 1️⃣ C++ Extraction & Discovery Pipeline

### Purpose
Build knowledge graphs from PDF documents and run automated discovery operators to find patterns, anomalies, and insights.

### Location
- **Main entry**: `src/main.cpp`
- **Binary**: `build/bin/kg`
- **Headers**: `include/`
- **Source**: `src/`

### Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  INPUT: PDF Documents                                        │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  STAGE 1: EXTRACTION (extraction_pipeline.cpp)              │
│  • PDF Processing (PDFProcessor)                             │
│  • Text Chunking (fixed/page/paragraph/sentence)            │
│  • LLM Entity & Relation Extraction (OpenAI/Gemini)         │
│  • Hypergraph Construction (Hypergraph)                      │
│  ────────────────────────────────────────────────────────    │
│  Output: graph.json, extraction_stats.json                   │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  STAGE 1.5: PREPROCESSING (Optional)                         │
│  • Relation normalization                                    │
│  • Alias merging                                             │
│  • Entity deduplication                                      │
│  ────────────────────────────────────────────────────────    │
│  Output: graph.json (cleaned), graph_raw.json (backup)       │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  STAGE 2: INDEXING (hypergraph_index.cpp)                   │
│  • Build adjacency maps (entity → neighbors)                 │
│  • Create reverse indexes (predicate → edges)                │
│  • Degree computation                                        │
│  ────────────────────────────────────────────────────────    │
│  Output: index.json                                          │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  STAGE 3: DISCOVERY (discovery_engine.cpp)                  │
│  • Run 55 discovery operators                               │
│    - Structural: hubs, bridges, communities                  │
│    - Semantic: synonyms, contradictions                      │
│    - Temporal: trends, evolution                             │
│    - Statistical: outliers, anomalies                        │
│  • Generate insights with confidence scores                  │
│  ────────────────────────────────────────────────────────    │
│  Output: insights.json                                       │
└──────────────────────────────────────────────────────────────┘
                           │
                  ┌────────┴─────────┐
                  │                  │
                  ▼                  ▼
┌─────────────────────────┐  ┌──────────────────────────────┐
│  STAGE 4: RENDERING     │  │  STAGE 5: REPORTING          │
│  (augmentation_renderer)│  │  (report_generator.cpp)      │
│  • 3D HTML graphs       │  │  • Structured reports        │
│  • ForceGraph3D         │  │  • Insight categorization    │
│  • Graphviz DOT         │  │  • Statistics                │
│  • Augmentation nodes   │  │  • Markdown + HTML           │
│  ─────────────────────  │  │  ──────────────────────────  │
│  Output:                │  │  Output:                     │
│  • graph.html           │  │  • report.md                 │
│  • graph_augmented.html │  │  • report.html               │
│  • graph_rag.html       │  │                              │
│  • augmentation.json    │  │                              │
│  • graph.dot            │  │                              │
└─────────────────────────┘  └──────────────────────────────┘
                  │                  │
                  └────────┬─────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  STAGE 6: FINALIZATION                                       │
│  • Create manifest.json (run metadata)                       │
│  • Generate README.txt (run summary)                         │
│  ────────────────────────────────────────────────────────    │
│  Final Directory: runs/run_YYYYMMDD_HHMMSS/                  │
└──────────────────────────────────────────────────────────────┘
```

### Key Components

#### **Hypergraph (`include/graph/hypergraph.hpp`)**
- Core data structure for knowledge representation
- Supports N-ary relationships (hyperedges)
- Entity and edge metadata
- Confidence scoring

#### **Discovery Engine (`include/discovery/discovery_engine.hpp`)**
- 55 automated operators
- Operator registry system
- Parallel execution support
- Insight generation with evidence

#### **LLM Provider (`include/llm/llm_provider.hpp`)**
- OpenAI API integration
- Gemini API integration
- Prompt engineering for entity/relation extraction
- Rate limiting and error handling

#### **PDF Processor (`include/pdf/pdf_processor.hpp`)**
- Multi-format PDF support via Poppler
- Text extraction and cleaning
- Metadata preservation
- Chunking strategies

### Command-Line Interface

```bash
# Full pipeline
./build/bin/kg run -i pdfs/ -t "My Research" -p all --preprocess

# Individual stages
./build/bin/kg index -i graph.json -o index.json
./build/bin/kg discover -g graph.json -x index.json -p all
./build/bin/kg render -g graph.json -s insights.json
./build/bin/kg report -g graph.json -s insights.json

# Utilities
./build/bin/kg list-operators
./build/bin/kg stats -i graph.json
```

### Output Artifacts (per run)

```
runs/run_YYYYMMDD_HHMMSS/
├── graph.json              # Knowledge graph (nodes + hyperedges)
├── graph_raw.json          # Pre-preprocessing backup (if --preprocess)
├── index.json              # Adjacency & reverse indexes
├── insights.json           # Discovery operator results
├── augmentation.json       # Augmentation nodes for visualization
├── extraction_stats.json   # Extraction metrics
├── graph.html              # 3D visualization (base graph)
├── graph_augmented.html    # 3D visualization (with discoveries)
├── graph_rag.html          # Graph-RAG ready visualization
├── graph.dot               # Graphviz source
├── report.md               # Markdown report
├── report.html             # HTML report
├── manifest.json           # Run metadata
└── README.txt              # Human-readable summary
```

---

## 2️⃣ Graph-RAG Interactive Tool (Development Version)

### Purpose
Serve the generated knowledge graph through an interactive web interface with:
- 3D visualization
- Natural language chat (Graph-RAG)
- Kuzu database explorer
- Cypher query interface

### Location
- **Backend**: `src/graph_rag_tool/backend/graph_rag_server.py`
- **Frontend**: `src/graph_rag_tool/backend/template.html`
- **Port**: `http://localhost:8000`

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  WEB BROWSER (localhost:8000)                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  TEMPLATE.HTML (Frontend)                           │   │
│  │  • ForceGraph3D (3D WebGL rendering)                │   │
│  │  • Database Explorer (5 tabs)                       │   │
│  │  • Chat interface                                   │   │
│  │  • Settings panel                                   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           │ HTTP/REST
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  FASTAPI BACKEND (graph_rag_server.py)                      │
│  ┌───────────────────────────────────────────────────┐     │
│  │  API ENDPOINTS (15 endpoints)                      │     │
│  │  • POST /api/query      - Graph-RAG chat          │     │
│  │  • POST /api/cypher     - Direct Cypher execution │     │
│  │  • POST /api/upload     - Graph JSON upload       │     │
│  │  • GET  /api/schema     - Schema information      │     │
│  │  • GET  /api/quality    - Data quality check      │     │
│  │  • GET  /api/relations  - List relation types     │     │
│  │  • GET  /api/entities   - Search entities         │     │
│  │  • POST /api/path       - Find paths between nodes│     │
│  │  • ... (see full list below)                       │     │
│  └───────────────────────────────────────────────────┘     │
│                           │                                  │
│                           ▼                                  │
│  ┌───────────────────────────────────────────────────┐     │
│  │  KUZU DATABASE (Embedded Graph DB)                │     │
│  │  • Entity nodes (id, name, label, docs, degree)   │     │
│  │  • RELATED edges (relation_type, confidence)      │     │
│  │  • Cypher query engine                            │     │
│  │  • Vector embeddings (384d or 1536d)              │     │
│  └───────────────────────────────────────────────────┘     │
│                           │                                  │
│                           ▼                                  │
│  ┌───────────────────────────────────────────────────┐     │
│  │  LLM INTEGRATION                                   │     │
│  │  • Azure OpenAI (chat + embeddings)               │     │
│  │  • OpenAI API (chat + embeddings)                 │     │
│  │  • Cypher query generation                        │     │
│  │  • Answer synthesis                               │     │
│  └───────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### Features

#### **3D Graph Visualization**
- WebGL-accelerated force-directed layout
- Interactive node/edge selection
- Camera controls (zoom, pan, rotate)
- Entity highlighting
- Multi-hop expansion (1-hop, 2-hop)

#### **Database Explorer (Ctrl+K)**
5 tabs:
1. **Overview** - Entity count, relation types, top entities, data quality
2. **Schema** - Browse all relation types with samples
3. **Query** - Write and execute Cypher queries
4. **Explorer** - Search entities, view details, explore relations
5. **Paths** - Find shortest paths between entities

#### **Graph-RAG Chat**
- Natural language → Cypher generation
- Graph traversal-based retrieval
- LLM answer synthesis
- Entity highlighting in visualization
- Citation with source documents

#### **RAG Modes**
1. **Graph-RAG**: Cypher query + graph structure
2. **Document-RAG**: Vector similarity (no graph)
3. **Compare**: Side-by-side comparison

### Technology Stack

| Component | Technology |
|-----------|-----------|
| Backend Framework | FastAPI (Python) |
| Database | Kuzu 0.7.1 (embedded graph DB) |
| Query Language | Cypher |
| Frontend | HTML5 + Vanilla JS |
| 3D Rendering | ForceGraph3D (Three.js) |
| Embeddings | sentence-transformers (local) OR Azure OpenAI (cloud) |
| LLM | Azure OpenAI / OpenAI API |

### API Endpoints (Complete List)

```
GET  /                                      # Serve frontend
GET  /api/health                            # Health check
GET  /api/schema                            # Graph schema info
GET  /api/quality                           # Data quality metrics
GET  /api/relations                         # List all relation types
GET  /api/relations/{type}/samples          # Sample relationships
GET  /api/entities                          # Search entities
GET  /api/entities/{id}                     # Get entity details
GET  /api/relations/{composite_hash}           # Get entity relationships
POST /api/query                             # Graph-RAG chat query
POST /api/cypher                            # Execute Cypher query
POST /api/path                              # Find shortest path
POST /api/upload                            # Upload graph JSON
POST /api/clean                             # Clean database
POST /api/generate-embeddings               # Generate embeddings (Azure)
POST /api/reload                            # Reload from file
```

---

## 3️⃣ Deployment Version (Standalone Bundle)

### Purpose
Lightweight, production-ready standalone tool for corporate/firewall environments.

### Location
`src/graph_rag_tool/deployment/`

### Bundle Contents

```
deployment/
├── graph_rag_server.py          # Backend (all-in-one file)
├── template.html                # Frontend (all-in-one file, 163KB)
├── requirements.txt             # Python dependencies (local mode)
├── requirements-azure.txt       # Python dependencies (Azure-only mode)
├── start.sh                     # Linux/macOS startup script
├── start.bat                    # Windows startup script
├── start-azure.sh               # Azure mode startup (Linux)
├── start-azure.bat              # Azure mode startup (Windows)
├── examples/
│   └── sample_graph.json        # Sample data for testing
├── docs/
│   ├── API_REFERENCE.md         # Complete API documentation
│   ├── USER_GUIDE.md            # Feature walkthrough
│   └── TROUBLESHOOTING.md       # Common issues
├── README.md                    # Main deployment guide
├── QUICK_START.md               # 5-minute setup
├── DEPLOYMENT_CHECKLIST.md      # Pre-deployment verification
├── AZURE_CORPORATE_DEPLOYMENT.md# Corporate/firewall setup
├── NATIVE_WINDOWS_VERIFIED.md   # Windows native setup
├── BUNDLE_MANIFEST.md           # Complete file listing
├── VERSION.txt                  # Version information
└── data/                        # Place your graph files here
```

### Deployment Modes

#### **Local Mode (Default)**
- Uses `sentence-transformers` for embeddings (all-MiniLM-L6-v2, 384d)
- Downloads model from HuggingFace (first run only, ~90MB)
- No external API calls for embeddings
- Free to use

**Setup:**
```bash
pip install -r requirements.txt
python graph_rag_server.py
```

#### **Azure Mode (Corporate/Firewall)**
- Uses Azure OpenAI for embeddings (text-embedding-3-small, 1536d)
- NO downloads from HuggingFace (firewall-safe)
- Requires Azure OpenAI access
- Costs: ~$0.0001 per 1K tokens

**Setup:**
```bash
set EMBEDDING_MODE=azure          # Windows
export EMBEDDING_MODE=azure       # Linux
pip install -r requirements-azure.txt
python graph_rag_server.py
```

### Key Features

✅ **Minimal Dependencies** - Only 4 core packages (FastAPI, Uvicorn, Kuzu, python-multipart)
✅ **Single File Deployment** - Backend + frontend in 2 files
✅ **No Build Process** - Pure Python, no compilation
✅ **Cross-Platform** - Windows, Linux, macOS, WSL
✅ **Firewall Compatible** - Azure-only mode for corporate networks
✅ **Embedded Database** - No external database server required
✅ **Portable** - Copy folder anywhere, run immediately

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| Python | 3.8 | 3.11 or 3.12 |
| RAM | 2 GB | 4 GB |
| Disk | 500 MB | 1 GB |
| Browser | Chrome 90+ | Chrome/Edge latest |

### Quick Start (3 Steps)

```bash
# 1. Install dependencies
pip install -r requirements.txt

# 2. Start server
python graph_rag_server.py

# 3. Open browser
# Navigate to http://localhost:8000
```

---

## 4️⃣ Python Porting (Alternative Implementation)

### Purpose
Lightweight Python-only extraction path using Azure OpenAI for LLM calls.

### Location
`src/python_porting/`

### Use Cases
- Quick prototyping without C++ build
- Azure OpenAI exclusive environments
- Jupyter notebook integration
- Python-only workflows

### Features
- PDF text extraction
- Entity/relation extraction via Azure OpenAI
- JSON graph output (compatible with Graph-RAG tool)
- Simplified pipeline (no discovery operators)

---

## 🔄 Typical Workflows

### Workflow 1: Full Research Pipeline

```bash
# 1. Extract knowledge graph from PDFs
./build/bin/kg run -i research_papers/ -t "My Study" -p all --preprocess

# 2. Review outputs
cd runs/run_YYYYMMDD_HHMMSS/
open graph.html          # View 3D graph
open report.html         # Review insights

# 3. Serve with Graph-RAG tool
cd ../../src/graph_rag_tool/backend/
python graph_rag_server.py

# 4. Upload graph to tool
# Open http://localhost:8000
# Click "Load Graph JSON" → select graph.json

# 5. Query and explore
# Use chat interface, Database Explorer, Cypher queries
```

### Workflow 2: Deployment to Production

```bash
# 1. Build graph using C++ pipeline (on development machine)
./build/bin/kg run -i pdfs/ -t "Production Graph" -p all

# 2. Copy deployment bundle
cp runs/run_YYYYMMDD_HHMMSS/graph.json src/graph_rag_tool/deployment/data/

# 3. Package deployment folder
cd src/graph_rag_tool/deployment/
zip -r graph_rag_bundle.zip .

# 4. Deploy to production server
scp graph_rag_bundle.zip prod-server:/app/
ssh prod-server "cd /app && unzip graph_rag_bundle.zip"

# 5. Start on production (Azure mode for corporate firewall)
ssh prod-server
export EMBEDDING_MODE=azure
pip install -r requirements-azure.txt
python graph_rag_server.py
```

### Workflow 3: Iterative Discovery

```bash
# 1. Initial extraction
./build/bin/kg run -i pdfs/ -t "Initial" -p basic

# 2. Review initial insights
less runs/run_YYYYMMDD_HHMMSS/report.md

# 3. Re-run with more operators (from Stage 3)
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS -p all

# 4. Re-render and re-report (from Stage 4)
./build/bin/kg run -f 4 -d runs/run_YYYYMMDD_HHMMSS

# 5. Compare results
diff runs/run_YYYYMMDD_HHMMSS/report.md output/report.md
```

---

## 📊 Data Flow

```
┌──────────────┐
│  PDF Files   │
└──────┬───────┘
       │
       ▼
┌────────────────────────────────┐
│  C++ Pipeline (kg run)         │
│  • Extract entities/relations  │
│  • Build hypergraph           │
│  • Index for fast lookup      │
│  • Run discovery operators    │
│  • Generate visualizations    │
└────────────────┬───────────────┘
                 │
                 ├─► graph.json ──────────────┐
                 ├─► index.json               │
                 ├─► insights.json            │
                 ├─► graph.html               │
                 └─► report.md                │
                                              │
                                              ▼
                 ┌────────────────────────────────────┐
                 │  Graph-RAG Tool                    │
                 │  1. Upload graph.json              │
                 │  2. Load into Kuzu DB              │
                 │  3. Generate embeddings            │
                 │  4. Serve via FastAPI              │
                 │  5. Query via chat/Cypher/Explorer │
                 └────────────────────────────────────┘
                                              │
                                              ▼
                                    ┌─────────────────┐
                                    │  User Insights  │
                                    │  • Answers      │
                                    │  • Discoveries  │
                                    │  • Exports      │
                                    └─────────────────┘
```

---

## 🛠️ Key Technologies

### C++ Pipeline

| Component | Technology/Library |
|-----------|-------------------|
| Build System | CMake 3.14+ |
| Compiler | GCC 9+ / Clang 10+ / MSVC 2019+ |
| JSON | nlohmann/json |
| HTTP | libcurl |
| PDF | Poppler-cpp |
| Testing | Google Test |
| Visualization | Graphviz |

### Graph-RAG Tool

| Component | Technology/Library |
|-----------|-------------------|
| Backend | Python 3.8+, FastAPI |
| Database | Kuzu 0.7.1 |
| Query Language | Cypher |
| Frontend | HTML5, Vanilla JavaScript |
| 3D Graphics | ForceGraph3D, Three.js |
| Embeddings | sentence-transformers OR Azure OpenAI |
| LLM | Azure OpenAI / OpenAI API |
| Server | Uvicorn (ASGI) |

---

## 📁 Project Structure

```
Batch4/
├── CMakeLists.txt                    # C++ build configuration
├── build.sh                          # Build helper script
├── .llm_config.json                  # LLM configuration (gitignored)
│
├── src/                              # C++ source code
│   ├── main.cpp                      # CLI entry point
│   ├── pipeline/                     # Extraction pipeline
│   ├── llm/                          # LLM providers
│   ├── pdf/                          # PDF processing
│   ├── graph/                        # Hypergraph implementation
│   ├── index/                        # Indexing system
│   ├── discovery/                    # Discovery operators
│   ├── render/                       # Visualization renderers
│   ├── cli/                          # CLI utilities
│   │
│   ├── graph_rag_tool/               # Interactive Graph-RAG tool
│   │   ├── backend/                  # FastAPI server
│   │   ├── frontend/                 # HTML/JS UI
│   │   └── deployment/               # Standalone bundle
│   │
│   └── python_porting/               # Python-only extraction
│
├── include/                          # C++ headers
│   ├── pipeline/
│   ├── llm/
│   ├── pdf/
│   ├── graph/
│   ├── index/
│   ├── discovery/
│   ├── render/
│   └── cli/
│
├── examples/                         # Example programs
│   ├── pipeline_example.cpp
│   ├── pdf_processing_example.cpp
│   ├── llm_extraction_example.cpp
│   └── hypergraph_example.cpp
│
├── tests/                            # Test data & test PDFs
│
├── build/                            # Build artifacts (gitignored)
│   └── bin/
│       └── kg                        # Main executable
│
├── runs/                             # Pipeline output runs (gitignored)
│   └── run_YYYYMMDD_HHMMSS/
│       ├── graph.json
│       ├── insights.json
│       ├── report.md
│       └── ...
│
├── output/                           # Latest run symlinks (gitignored)
│   ├── graph.json -> runs/run_*/graph.json
│   └── report.md -> runs/run_*/report.md
│
├── References/                       # Design specs & references
│
└── Documentation (in root)
    ├── README.md                     # Project overview
    ├── PIPELINE.md                   # Technical pipeline details
    ├── FEATURES.md                   # Feature guide
    ├── DEPLOYMENT.md                 # Installation & deployment
    ├── TROUBLESHOOTING.md            # Common issues
    ├── CHANGELOG.md                  # Version history
    └── PROJECT_OVERVIEW.md           # This file
```

---

## 🚀 Quick Start Guide

### For Researchers (Full Pipeline)

```bash
# 1. Install C++ dependencies
sudo apt-get install cmake g++ libcurl4-openssl-dev nlohmann-json3-dev libpoppler-cpp-dev

# 2. Build
./build.sh build

# 3. Configure LLM
cp .llm_config.json.example .llm_config.json
# Edit with your OpenAI/Gemini credentials

# 4. Run pipeline
./build/bin/kg run -i path/to/pdfs/ -t "My Research" -p all --preprocess

# 5. View results
cd runs/run_YYYYMMDD_HHMMSS/
open graph.html
open report.md
```

### For Developers (Graph-RAG Tool)

```bash
# 1. Navigate to backend
cd src/graph_rag_tool/backend/

# 2. Install Python dependencies
pip install -r requirements.txt

# 3. Start server
python graph_rag_server.py

# 4. Open browser
# Go to http://localhost:8000

# 5. Upload graph
# Click "Load Graph JSON" → select a graph.json file
```

### For Deployment (Production)

```bash
# 1. Navigate to deployment folder
cd src/graph_rag_tool/deployment/

# 2. Choose mode
# Local mode (with model downloads):
pip install -r requirements.txt

# Azure mode (firewall-safe):
export EMBEDDING_MODE=azure
pip install -r requirements-azure.txt

# 3. Start server
python graph_rag_server.py

# 4. Access
# Open http://localhost:8000
```

---

## 📈 Performance Metrics

### C++ Pipeline

| Operation | Performance |
|-----------|-------------|
| PDF Processing | ~5-20 pages/sec (depends on complexity) |
| LLM Extraction | ~10-30 chunks/min (API rate limited) |
| Indexing | < 1 sec for 1000 entities |
| Discovery (all operators) | 5-30 sec for 1000 entities |
| Rendering | < 5 sec for 1000 entities |

### Graph-RAG Tool

| Operation | Performance |
|-----------|-------------|
| Graph Load | < 5 sec for 10K entities |
| Cypher Query | < 500ms for most queries |
| Vector Search | < 200ms for 10K entities |
| Embedding Generation | ~100 entities/min (Azure), ~500 entities/min (local) |
| 3D Rendering | Smooth for < 5K nodes, use clustering for larger |

---

## 🔒 Security Considerations

### API Keys
- **Never commit** `.llm_config.json` (gitignored)
- Store in environment variables for production
- Use Azure Key Vault for enterprise deployments
- Rotate keys regularly

### Network Security
- Graph-RAG tool binds to `0.0.0.0:8000` by default
- For production: use reverse proxy (nginx/Apache) with HTTPS
- Corporate deployments: restrict to internal network only
- Use VPN for remote access

### Data Privacy
- PDF documents may contain sensitive information
- Knowledge graphs inherit sensitivity from source documents
- Consider data anonymization for shared deployments
- Implement access controls for multi-user scenarios

---

## 📝 Version History

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

**Current Version**: 1.0.0
- C++ pipeline: Stable, production-ready
- Graph-RAG tool: Feature-complete, tested on Windows/Linux
- Deployment bundle: Corporate-ready, firewall-compatible

---

## 🤝 Contributing

This is a PhD research project. For questions or collaboration inquiries:
- Review [FEATURES.md](FEATURES.md) for capabilities
- Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for known issues
- See [PIPELINE.md](PIPELINE.md) for technical architecture

---

## 📚 Further Reading

### Core Documentation
1. [README.md](README.md) - Start here
2. [PIPELINE.md](PIPELINE.md) - Technical deep dive
3. [FEATURES.md](FEATURES.md) - Feature showcase
4. [DEPLOYMENT.md](DEPLOYMENT.md) - Installation guide

### Deployment Guides
1. `src/graph_rag_tool/deployment/README.md` - Deployment overview
2. `src/graph_rag_tool/deployment/QUICK_START.md` - 5-minute setup
3. `src/graph_rag_tool/deployment/AZURE_CORPORATE_DEPLOYMENT.md` - Corporate setup
4. `src/graph_rag_tool/deployment/docs/USER_GUIDE.md` - Feature walkthrough

### Component-Specific
1. `src/graph_rag_tool/README.md` - Graph-RAG tool details
2. `src/python_porting/README.md` - Python extraction guide
3. `References/` - Design specifications

---

## 🎯 Summary

This project provides a **complete end-to-end system** for:

1. ✅ **Extracting** knowledge graphs from academic PDFs
2. ✅ **Discovering** insights through automated analysis
3. ✅ **Visualizing** graphs in interactive 3D
4. ✅ **Querying** via natural language (Graph-RAG)
5. ✅ **Exploring** via Cypher and database tools
6. ✅ **Deploying** to production (local or corporate/Azure)

**Choose your workflow:**
- **Research**: Use C++ pipeline for full extraction + discovery
- **Development**: Use Graph-RAG tool for interactive exploration
- **Production**: Use deployment bundle for serving

---

**Project Status**: ✅ Production Ready
**Last Updated**: February 2026
**Maintained By**: PhD Research Team

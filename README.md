# Knowledge Graph Extractor

C++ and Python tooling for extracting hypergraph knowledge graphs from PDF text, running discovery operators, and serving an interactive Graph RAG interface.

## What Is In This Repo

- `kg` CLI (C++): end-to-end pipeline `extract -> index -> discover -> render -> report`
- Graph RAG app (`src/graph_rag_tool/`): FastAPI + Kuzu backend with 3D UI, chat, and DB Explorer
- Python-only extractor (`src/python_porting/`): lightweight Azure OpenAI extraction path

## Documentation

### Main Guides

- **[README.md](README.md)** - This file (project overview)
- **[DEPLOYMENT.md](DEPLOYMENT.md)** - Installation & deployment instructions (Windows/Linux, Local/Azure)
- **[FEATURES.md](FEATURES.md)** - Complete feature guide (RAG modes, UI, capabilities)
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and solutions
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and recent changes
- **[PIPELINE.md](PIPELINE.md)** - Technical pipeline details

### Component-Specific

- `src/graph_rag_tool/README.md` - Graph-RAG tool specifics
- `src/graph_rag_tool/backend/README.md` - Backend API details
- `src/python_porting/README.md` - Python extractor docs

**Note:** Generated run artifacts (`runs/*/report.md`, `output/report.md`) are runtime outputs, not canonical docs.

## C++ Pipeline (`kg`) Quick Start

### 1. Install dependencies (Ubuntu/WSL)

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libcurl4-openssl-dev nlohmann-json3-dev
# Optional (PDF + tests + visualization)
sudo apt-get install -y libpoppler-cpp-dev libgtest-dev graphviz
```

### 2. Build

```bash
./build.sh build
# binaries in build/bin/
```

### 3. Configure LLM

Use config file (recommended):

```bash
cp .llm_config.json.example .llm_config.json
# edit provider/api_key/model
```

Gemini template: `.llm_config_gemini.json.example`

Environment fallback:

```bash
export OPENAI_API_KEY='...'
# or
export GEMINI_API_KEY='...'
export KG_LLM_PROVIDER='gemini'
```

### 4. Run pipeline

```bash
./build/bin/kg run -i /path/to/file.pdf -t "My Run"
```

Useful options:

- `--preprocess` normalize relation labels and merge aliases before indexing
- `-p all` run all 55 discovery operators
- `-f <stage> -d <run_dir>` resume existing runs from stage 2-5

Examples:

```bash
./build/bin/kg run -i ./pdfs/ -t "Corpus Run" -p all --preprocess
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS
./build/bin/kg list-operators
```

CLI commands:

```bash
./build/bin/kg --help
```

Available commands: `run`, `index`, `discover`, `render`, `report`, `stats`, `list-operators`.

Pipeline output per run (`runs/run_YYYYMMDD_HHMMSS/`):

- `graph.json`, `index.json`, `insights.json`, `augmentation.json`
- `graph.html`, `graph_augmented.html`, `graph.dot`
- `report.md`, `report.html`, `manifest.json`, `extraction_stats.json`

### HTML Report Features

The `report.html` provides an interactive, category-organized view of discovered insights:

**🎨 Modern Interface:**
- Dark theme with smooth animations
- Category-organized layout (Combinatorial, Exploratory, Transformational)
- Hierarchical table of contents
- Horizontal bar chart showing insight distribution

**📊 Smart Navigation:**
- All sections start collapsed (except Categories Overview and Executive Summary)
- Click section headers to expand/collapse
- Click chart bars to jump to category sections
- Clean initial view for better overview

**📖 See:** [REPORT_UI_IMPROVEMENTS.md](REPORT_UI_IMPROVEMENTS.md) | [CATEGORY_REORGANIZATION.md](CATEGORY_REORGANIZATION.md)

## Graph-RAG Tool (Kuzu + FastAPI + 3D UI)

Interactive knowledge graph exploration with natural language queries, 3D visualization, and database explorer.

### Quick Start

**Windows (Anaconda Prompt):**
```bash
cd C:\Users\homea\Documents\PhD\DynamicKGs\Batch4\src\graph_rag_tool\backend
python graph_rag_server.py
```

**Linux/Mac:**
```bash
cd src/graph_rag_tool/backend
python graph_rag_server.py
```

**Open Browser:**
```
http://localhost:8000
```

⚠️ **Important:** Use `http://localhost:8000` (NOT `file://` protocol)

### Features

- **3 RAG Modes**: Graph-RAG (Cypher), Document-RAG (vector similarity), Compare both
- **3D Visualization**: WebGL force-directed graph with clustering
- **Kuzu Explorer**: Cypher queries, entity search, path finding, quality metrics
- **Dual Embedding**: Local (sentence-transformers) or Azure OpenAI
- **Export**: CSV/JSON query results

📖 **Full Documentation:** [DEPLOYMENT.md](DEPLOYMENT.md) | [FEATURES.md](FEATURES.md) | [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

### API Endpoints

- App: `http://localhost:8000`
- API docs: `http://localhost:8000/docs`

**Core APIs:**
- `GET /api/health` - Server status
- `GET /api/schema` - Graph schema
- `POST /api/query` - Natural language query (3 RAG modes)
- `POST /api/cypher` - Execute Cypher query
- `GET /api/entities` - Search entities
- `GET /api/relations` - List relation types
- `GET /api/relations/{composite_hash}` - Get incoming/outgoing relations for an entity hash
- `POST /api/path` - Find shortest path
- `GET /api/quality` - Data quality metrics

## Python-Only Extractor (Optional)

See `src/python_porting/README.md`.

Fast start:

```bash
cd src/python_porting
conda env create -f environment.yml
conda activate kg-extractor
# set Azure credentials in kg_extractor.py
python kg_extractor.py /path/to/file.pdf --output ./output --verbose
```

## Tests and Data Notes

- Unit tests: `./build.sh test` (requires Google Test)
- Current `tests/` folder mostly contains ZIP archives. Unzip PDFs before running `kg run -i tests/...`.

## Status

- `kg` CLI and discovery pipeline are active and production-usable.
- Graph RAG backend supports chat query planning plus direct DB Explorer operations.

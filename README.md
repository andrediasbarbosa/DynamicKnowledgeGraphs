# Knowledge Graph Extractor

This repository contains three active code paths:

- `kg` CLI: the C++ pipeline for PDF extraction, quality control, indexing, discovery, rendering, and reporting
- `src/graph_rag_tool/`: a FastAPI + Kuzu Graph RAG application for interactive graph exploration
- `src/python_porting/`: a lightweight Python-only extractor for Azure OpenAI workflows

## Repo Layout

- `src/main.cpp`: `kg` CLI entrypoint
- `include/`, `src/`: C++ pipeline, discovery engine, rendering, and LLM integrations
- `src/graph_rag_tool/backend/`: active Graph RAG backend and frontend template
- `src/graph_rag_tool/deployment/`: standalone deployment bundle for the Graph RAG app
- `src/python_porting/`: Python-only extractor
- `docs/`: maintained code-facing documentation

## Quick Start: `kg`

### 1. Install dependencies

Ubuntu/WSL:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libcurl4-openssl-dev nlohmann-json3-dev
sudo apt-get install -y libpoppler-cpp-dev libgtest-dev graphviz
```

`libpoppler-cpp-dev` enables PDF extraction. Without it, the project still configures, but PDF processing targets are skipped.

### 2. Build

```bash
./build.sh build
```

Main binary:

```bash
./build/bin/kg
```

### 3. Configure the LLM

Recommended:

```bash
cp .llm_config.json.example .llm_config.json
```

Then set `provider`, `api_key`, and `model`.

Environment-variable fallback is also supported:

```bash
export OPENAI_API_KEY='...'
# or
export GEMINI_API_KEY='...'
export KG_LLM_PROVIDER='gemini'
```

### 4. Run the pipeline

```bash
./build/bin/kg run -i /path/to/pdfs -t "My Run"
```

Common options:

- `-p all` is the default for `kg run`
- `--preprocess` normalizes relation labels and merges alias-like entities before quality control
- `--with-ontology` adds class/instance classification
- `--causal` switches extraction prompts to the causal prompt set
- `-f <stage> -d <run_dir>` resumes an existing run

Examples:

```bash
./build/bin/kg run -i tests/ -p all --preprocess
./build/bin/kg run -i tests/ --with-ontology --semantic-dedup
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS
./build/bin/kg list-operators
```

Current CLI defaults:

- `kg run`: all 62 operators from `all_discovery_operators()`
- `kg discover`: `bridges,completions,motifs`

## Quick Start: Graph RAG

```bash
cd src/graph_rag_tool/backend
pip install -r requirements.txt
python graph_rag_server.py
```

Open:

- `http://localhost:8000`
- `http://localhost:8000/docs`

The active runtime lives in `src/graph_rag_tool/backend/`. The `deployment/` folder is a separately packaged standalone bundle.

## Quick Start: Python Port

```bash
cd src/python_porting
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python kg_extractor.py /path/to/file.pdf --output ./output --verbose
```

This path is intentionally smaller in scope than `kg`: it extracts a graph and generates `graph.json` plus `graph.html`, but it does not run the full discovery/report pipeline.

## Documentation

Maintained docs:

- [`docs/QUICK_START.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/QUICK_START.md)
- [`docs/CLI_REFERENCE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/CLI_REFERENCE.md)
- [`docs/PIPELINE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/PIPELINE.md)
- [`docs/OUTPUT_FOLDER_STRUCTURE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/OUTPUT_FOLDER_STRUCTURE.md)
- [`docs/QUALITY_CONTROL.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/QUALITY_CONTROL.md)
- [`docs/KNOWLEDGE_DISCOVERY_CATEGORIES.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/KNOWLEDGE_DISCOVERY_CATEGORIES.md)
- [`docs/DEPLOYMENT.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/DEPLOYMENT.md)
- [`docs/TROUBLESHOOTING.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/TROUBLESHOOTING.md)
- [`docs/PROJECT_OVERVIEW.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/PROJECT_OVERVIEW.md)
- [`src/graph_rag_tool/README.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/src/graph_rag_tool/README.md)
- [`src/graph_rag_tool/backend/README.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/src/graph_rag_tool/backend/README.md)
- [`src/graph_rag_tool/backend/docs/API_REFERENCE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/src/graph_rag_tool/backend/docs/API_REFERENCE.md)
- [`src/python_porting/README.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/src/python_porting/README.md)

Generated artifacts such as `runs/*/Step_5_Discovery/report.md` and `output/report.md` are runtime outputs, not canonical project documentation.

## Status

- `kg` is the primary maintained pipeline
- the default operator registry currently exposes 62 operators at runtime
- `bias_audit` and `community_recommendation` are implemented but not part of the default `all` registry
- 4 legacy insight types remain in `InsightType` for compatibility but are not executed by the default registry

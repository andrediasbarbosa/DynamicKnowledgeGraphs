# Knowledge Graph Extractor

A C++ pipeline that extracts structured knowledge graphs from PDF documents, runs multi-operator discovery, and produces interactive 3D visualisations and reports. Three active code paths:

- `kg` CLI: the primary C++ pipeline — extraction, quality control, indexing, discovery, rendering, and reporting
- `src/graph_rag_tool/`: a FastAPI + Kuzu Graph RAG application for interactive graph exploration and natural language querying
- `src/python_porting/`: a lightweight Python-only extractor for Azure OpenAI workflows

## Repo Layout

```
src/main.cpp                        kg CLI entrypoint
include/ / src/                     C++ pipeline, discovery engine, rendering, LLM integration
src/graph_rag_tool/backend/         active Graph RAG backend and frontend template
src/graph_rag_tool/deployment/      standalone deployment bundle for the Graph RAG app
src/python_porting/                 Python-only extractor
docs/                               maintained code-facing documentation
```

## Pipeline Overview

`kg run` executes up to 6 sequential stages. Each stage saves its outputs under a timestamped run directory (`runs/run_YYYYMMDD_HHMMSS/`).

```
Stage 1   — Knowledge Extraction
Stage 1.5 — Preprocess        (opt: --preprocess)
Stage 1.75— Quality Control   (on by default, disable with --no-qc)
Stage 1.9 — Ontology Class.   (opt: --with-ontology)
Stage 2   — Index Building
Stage 3   — Knowledge Discovery
Stage 4   — Visualisation
Stage 5   — Report Generation
Stage 6   — Manifest
```

### Stage 1 — Knowledge Extraction

Processes one or more PDF files through `ExtractionPipeline`. Each document is chunked and sent to the configured LLM to extract entities and hyperedges. Supports three extraction prompt sets:

- default: standard relation extraction
- `--with-ontology`: adds class/instance classification to each entity
- `--causal`: switches to causal extraction prompts (causal chains, confounders, mechanisms)
- `--causal --with-ontology`: combined causal + ontology prompt

Outputs:
- `Step_2_Extraction/extraction_stats.json` — per-document extraction stats
- `Step_4_GraphBuilding/graph_raw.json` — raw graph before preprocessing (if `--preprocess`)
- `Step_4_GraphBuilding/graph.json` — extracted hypergraph

### Stage 1.5 — Preprocess (optional)

Activated with `--preprocess`. Runs two passes over the raw graph before quality control:

1. **Relation normalisation** — standardises relation label variants to a canonical form
2. **Alias merging** — collapses entity nodes that refer to the same real-world entity

Outputs: updates `Step_4_GraphBuilding/graph.json` in-place (raw saved as `graph_raw.json`).

### Stage 1.75 — Quality Control

A 5-level graph cleaning pass enabled by default (disable with `--no-qc`):

| Level | Name | What it does |
|-------|------|-------------|
| 1 | Rule-based filtering | Removes entities that are too short, purely numeric, or below minimum degree |
| 1.2 | Label simplification | Trims whitespace, normalises casing, collapses duplicate-label nodes |
| 1.5 | Semantic deduplication | Embeds entity labels and merges near-duplicate nodes (`--semantic-dedup`) |
| 2 | Statistical filtering | Removes low-degree outliers based on graph connectivity statistics |
| 3 | LLM validation | Asks the LLM to validate borderline entities (`--llm-validate`) |

Outputs:
- `Step_3_QualityControl/cleaning_report.json`
- `Step_3_QualityControl/quality_control_report.html`
- `Step_4_GraphBuilding/graph.json` (cleaned)

### Stage 1.9 — Ontology Classification (optional)

Activated with `--with-ontology`. Applies heuristic classification to assign each entity a `node_level` of `class` (general concept) or `instance` (specific example). Augments instance nodes with base class information. Updates `Step_4_GraphBuilding/graph.json`.

### Stage 2 — Index Building

Builds a `HypergraphIndex` over the cleaned graph, computing s-components for s = 2, 3, 4. S-components capture higher-order connectivity: nodes sharing at least `s` hyperedges form a component. Used by discovery operators to reason about graph structure.

Output: `Step_5_Discovery/index.json`

### Stage 3 — Knowledge Discovery

Runs discovery operators from the registry against the graph and index. The default `all` set runs all 62 operators. Operators are grouped into three categories:

- **Combinatorial** — structural patterns: bridges, motifs, paths, communities, hub nodes, dependency chains
- **Exploratory** — cross-domain links: analogy, gap detection, causal chains, citation impact, diffusion
- **Transformational** — reframing: contradiction, counterfactual, anomaly, uncertainty, schema repair, hypotheses

Each operator emits typed `Insight` objects. If `--semantic-dedup` is also enabled, near-duplicate insights are merged before saving.

Output: `Step_5_Discovery/insights.json`

### Stage 4 — Visualisation

Generates three interactive HTML viewers and a DOT file:

| File | Description |
|------|-------------|
| `graph.html` | Baseline 3D force-directed viewer; Louvain clustering on entity co-occurrence projection |
| `graph_augmented.html` | Augmented viewer overlaying insight nodes onto the base graph; supports aug-only and combined modes |
| `graph_rag.html` | Graph-RAG chat viewer; natural language Q&A over the graph using the configured LLM |
| `graph.dot` | Graphviz DOT export for external rendering |

Also saves `augmentation.json` — the insight-to-node mapping consumed by the augmented viewer.

### Stage 5 — Report Generation

Generates a discovery report from the collected insights:

| File | Description |
|------|-------------|
| `report.md` | Markdown report with per-insight-type summaries and evidence |
| `report.html` | Styled HTML version of the same report |
| `pattern_library.json` | Structured export of all discovered structural motifs and patterns |

If a live LLM is available, the report generator synthesises short natural-language examples for each insight type.

### Stage 6 — Manifest

Writes `manifest.json` at the run root summarising all artifacts, pipeline configuration, entity/relation/insight counts, and UTC timestamps. Updated on every resume.

---

## Quick Start: `kg`

### 1. Install dependencies

Ubuntu/WSL:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libcurl4-openssl-dev nlohmann-json3-dev
sudo apt-get install -y libpoppler-cpp-dev libgtest-dev graphviz
```

`libpoppler-cpp-dev` enables PDF extraction. Without it the project still configures, but PDF processing targets are skipped.

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

Then set `provider`, `api_key`, and `model`. Environment-variable fallback is also supported:

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

Common flags:

| Flag | Default | Description |
|------|---------|-------------|
| `-i <path>` | required | Input PDF file or directory |
| `-t <title>` | derived from filename | Run title |
| `-p <operators>` | `all` | Comma-separated operator list, or `all` |
| `--preprocess` | off | Normalise relations and merge aliases before QC |
| `--with-ontology` | off | Add class/instance classification |
| `--causal` | off | Use causal extraction prompts |
| `--no-qc` | off | Skip quality control |
| `--semantic-dedup` | off | Merge near-duplicate entities and insights by embedding similarity |
| `--llm-validate` | off | Use LLM for Level 3 QC validation |
| `-f <stage>` | 1 | Resume from stage (requires `-d`) |
| `-d <run_dir>` | auto | Existing run directory to resume |
| `--max-examples <n>` | 10 | Max insight examples per type in the report |

Examples:

```bash
./build/bin/kg run -i docs/ -p all --preprocess
./build/bin/kg run -i docs/ --with-ontology --semantic-dedup
./build/bin/kg run -i docs/ --causal --with-ontology
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS
./build/bin/kg list-operators
```

---

## Quick Start: Graph RAG

```bash
cd src/graph_rag_tool/backend
pip install -r requirements.txt
python graph_rag_server.py
```

Open:

- `http://localhost:8000`
- `http://localhost:8000/docs`

Load a `graph.json` or `graph_rag.html` output from a completed run. Supports natural language queries over the graph using Kuzu as the graph database and configurable embedding backends (`local` = sentence-transformers 384-d, `azure` = Azure OpenAI 1536-d).

The active runtime lives in `src/graph_rag_tool/backend/`. The `deployment/` folder is a separately packaged standalone bundle.

---

## Quick Start: Python Port

```bash
cd src/python_porting
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python kg_extractor.py /path/to/file.pdf --output ./output --verbose
```

Outputs `graph.json` and `graph.html`. Does not run quality control, discovery, or report generation — use the `kg` CLI for the full pipeline.

---

## Documentation

- [docs/QUICK_START.md](docs/QUICK_START.md)
- [docs/CLI_REFERENCE.md](docs/CLI_REFERENCE.md)
- [docs/PIPELINE.md](docs/PIPELINE.md)
- [docs/OUTPUT_FOLDER_STRUCTURE.md](docs/OUTPUT_FOLDER_STRUCTURE.md)
- [docs/QUALITY_CONTROL.md](docs/QUALITY_CONTROL.md)
- [docs/KNOWLEDGE_DISCOVERY_CATEGORIES.md](docs/KNOWLEDGE_DISCOVERY_CATEGORIES.md)
- [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md)
- [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)
- [docs/PROJECT_OVERVIEW.md](docs/PROJECT_OVERVIEW.md)
- [src/graph_rag_tool/README.md](src/graph_rag_tool/README.md)
- [src/graph_rag_tool/backend/README.md](src/graph_rag_tool/backend/README.md)
- [src/graph_rag_tool/backend/docs/API_REFERENCE.md](src/graph_rag_tool/backend/docs/API_REFERENCE.md)
- [src/python_porting/README.md](src/python_porting/README.md)

Generated artifacts such as `runs/*/Step_5_Discovery/report.md` are runtime outputs, not canonical project documentation.

## Status

- `kg` is the primary maintained pipeline
- the default operator registry exposes 62 operators at runtime
- `bias_audit` and `community_recommendation` are implemented but not part of the default `all` registry
- 4 legacy insight types remain in `InsightType` for compatibility but are not executed by the default registry

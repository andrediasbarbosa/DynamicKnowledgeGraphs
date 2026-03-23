# PIPELIN: Project Pipeline Steps, Inputs, and Outputs

This document describes the implemented pipeline behavior from code inspection, primarily:
- `src/main.cpp` (`kg run`, `kg index`, `kg discover`, `kg render`, `kg report`)
- `src/pipeline/extraction_pipeline.cpp`
- `include/pipeline/extraction_pipeline.hpp`

## 1. End-to-End Pipeline (`kg run`)

Command:
```bash
./build/bin/kg run -i <pdf_or_directory> [options]
```

### Pipeline Diagram

```text
+------------------+
|   Input PDF(s)   |
+------------------+
         |
         v
+-------------------------------------------+
| Stage 1: Extract                          |
| outputs: graph_raw.json,                  |
|          extraction_stats.json            |
+-------------------------------------------+
         |
         v
+-------------------------------------------+
| Stage 1.75: Quality Control               |
| outputs: graph.json (cleaned),            |
|          cleaning_report.json,            |
|          quality_control_report.html      |
+-------------------------------------------+
         |
         v
   +---------------------+
   |   --preprocess ?    |
   +---------------------+
      | yes         | no
      v             v
+----------------------+     +----------------------+
| Stage 1.5: Preprocess|---->| Stage 2: Index       |
| output: graph.json   |     | output: index.json   |
+----------------------+     +----------------------+
                                   |
                                   v
                         +----------------------+
                         | Stage 3: Discover    |
                         | output: insights.json|
                         +----------------------+
                                   |
                                   +-----------------------------+
                                   |                             |
                                   v                             v
                  +--------------------------------+   +---------------------------+
                  | Stage 4: Render                |   | Stage 5: Report          |
                  | outputs: graph.html,           |   | outputs: report.md,      |
                  | graph_augmented.html,          |   |          report.html      |
                  | graph_rag.html,                |   +---------------------------+
                  | augmentation.json, graph.dot   |              |
                  +--------------------------------+              |
                                   |                             |
                                   +-------------+---------------+
                                                 |
                                                 v
                                   +----------------------------+
                                   | Stage 6: Finalize          |
                                   | outputs: manifest.json,    |
                                   |          README.txt        |
                                   +----------------------------+
```

### Stage Overview

| Stage | Name | Main Input | Main Output |
|---|---|---|---|
| 1 | Knowledge Extraction | PDF file(s) + LLM config | `graph_raw.json`, `extraction_stats.json` |
| 1.75 | Quality Control | `graph_raw.json` + QC config | `graph.json` (cleaned), `cleaning_report.json`, `quality_control_report.html` |
| 1.5 (optional) | Preprocess Graph | In-memory graph from Stage 1.75 | Updated `graph.json`, optional `graph_raw.json` |
| 2 | Build Index | `graph.json` | `index.json` |
| 3 | Knowledge Discovery | `graph.json` + `index.json` + operator list | `insights.json` |
| 4 | Generate Visualizations | `graph.json` + `insights.json` | `graph.html`, `graph_augmented.html`, `graph_rag.html`, `augmentation.json`, `graph.dot` |
| 5 | Generate Reports | `graph.json` + `insights.json` | `report.md`, `report.html` |
| 6 | Finalize Run Metadata | Prior stage outputs + run metadata | `manifest.json`, `README.txt` |

## 2. Detailed Stage I/O

## Stage 1: Knowledge Extraction

Implemented by `ExtractionPipeline::process_pdfs()` and friends.

### Expected inputs
- CLI:
  - `--input/-i`: PDF path or directory containing PDFs
  - `--config/-c` (optional), else `.llm_config.json` or environment fallback
- Required LLM settings (from config/env):
  - provider (`openai` or `gemini`)
  - API key
  - model
- Internal input objects:
  - `std::vector<std::string> pdf_paths`
  - `PipelineConfig`

### Processing steps
1. Load each PDF into `PDFDocument` (`PDFProcessor::load_pdf`).
2. Chunk text (`chunking_strategy`: `fixed`/`page`/`paragraph`/`sentence`).
3. For each chunk, call LLM extraction (`LLMProvider::extract_relations`).
4. Convert extracted relations to `HyperEdge` objects and build per-document graph.
5. Merge per-document graphs into one `Hypergraph`.
6. Save per-document intermediates (if enabled):
   - `<document_id>_graph.json`
   - `<document_id>_extractions.json`

### Expected outputs
- Always:
  - `extraction_stats.json`
- If `--preprocess` is **not** used:
  - `graph.json`
- If `--preprocess` **is** used:
  - `graph_raw.json` (raw extracted graph before preprocessing)

## Stage 1.75: Quality Control

Runs by default after extraction (can be disabled with `--no-qc`).

### Expected inputs
- `graph_raw.json` (or in-memory graph from Stage 1)
- Quality control configuration from CLI flags:
  - `--min-node-length` (default: 2)
  - `--min-degree` (default: 1)
  - `--llm-validate` (default: false)
  - `--validation-mode` (default: "suspicious")
  - `--semantic-dedup` (default: false)
  - `--semantic-threshold` (default: 0.85)

### Processing steps
1. **Level 1: Rule-based filtering**
   - Remove entities based on length constraints
   - Filter stopwords ("the", "a", "fig", etc.)
   - Remove pure numbers and pure punctuation
   - Detect extraction artifacts ("figure", "section", etc.)
   - Trim whitespace from all entity labels

2. **Level 1.5: Semantic deduplication** (optional, requires LLM)
   - Group entities in batches
   - Use LLM to identify semantic duplicates
   - Merge similar entities (e.g., "ML" → "machine learning")
   - Update relations to canonical entities

3. **Level 2: Statistical filtering**
   - Compute node degrees and importance scores
   - Remove isolated nodes (degree < threshold)
   - Flag statistical outliers

4. **Level 3: LLM validation** (optional)
   - Batch validate suspicious or all entities
   - Ask LLM to classify as VALID/INVALID
   - Remove entities rejected by LLM

5. **Graph connectivity analysis**
   - Find connected components using DFS
   - Calculate graph density, clustering coefficient
   - Count isolated nodes
   - Identify largest component

### Expected outputs
- `graph.json` (cleaned graph)
- `cleaning_report.json` (JSON statistics)
- `quality_control_report.html` (visual report with charts)

### Output folder
- `Step_3_QualityControl/`

## Stage 1.5 (optional): Preprocess Graph

Runs only when `--preprocess` is provided (and `--from-stage <= 2`).

### Expected inputs
- In-memory graph from Stage 1 (or existing `graph.json` when resuming from stage 2)
- Preprocess rules implemented in `src/main.cpp`:
  - relation label normalization (synonym canonicalization)
  - alias/entity merge heuristics

### Expected outputs
- `graph.json` (preprocessed graph)
- `graph_raw.json` retained as raw baseline (if generated)
- Preprocess statistics stored in manifest:
  - normalized relation count
  - merged node count

## Stage 2: Build Index

### Expected inputs
- `graph.json`
- s-values hardcoded in `kg run` as `{2,3,4}`

### Processing
- `HypergraphIndex::build(graph, {2,3,4})`
- Builds:
  - relation-to-edge index
  - label-to-node index
  - s-components cache
  - degree ranking
  - entity co-occurrence map

### Expected outputs
- `index.json`

## Stage 3: Knowledge Discovery

### Expected inputs
- `graph.json`
- `index.json`
- Discovery operator list (`-p/--operators`, default `all`)
- Optional LLM provider for synthesis/enrichment

### Processing
- `DiscoveryEngine::run_operators(operators)`
- Produces typed insights (bridge, completion, motif, etc.)

### Expected outputs
- `insights.json`

## Stage 4: Generate Visualizations

### Expected inputs
- `graph.json`
- `insights.json`
- title (`--title` or derived)

### Processing and outputs
- Baseline graph viewer:
  - `graph.html`
- Insight augmentation payload:
  - `augmentation.json`
- Augmented viewer:
  - `graph_augmented.html`
- Graph-RAG HTML viewer (with augmentation embedded):
  - `graph_rag.html`
- GraphViz export:
  - `graph.dot`

## Stage 5: Generate Reports

### Expected inputs
- `graph.json`
- `insights.json`
- report settings (`title`, `max_examples`)
- Optional LLM provider for report examples

### Processing
Generates two report formats:
1. **Markdown Report** (`report.md`) - Text-based summary
2. **HTML Report** (`report.html`) - Interactive web report

### Expected outputs
- `report.md` - Markdown format report
- `report.html` - Interactive HTML report with:
  - **Category-organized layout**: Insights grouped by epistemological categories (Combinatorial, Exploratory, Transformational)
  - **Hierarchical table of contents**: Nested navigation with categories and subsections
  - **Category distribution chart**: Horizontal bar chart showing insight distribution across categories
  - **Collapsible sections**: All module sections start collapsed except first two (Categories Overview and Executive Summary)
  - **Statistics dashboard**: Graph metrics and insight counts
  - **Responsive design**: Modern dark theme with smooth animations

### HTML Report Features

**Initial View:**
- Statistics bar with graph metrics
- Category distribution bar chart (click bars to navigate to sections)
- Expanded: Categories Overview and Executive Summary
- Collapsed: All insight module sections

**Navigation:**
- Click section headers to expand/collapse
- Click TOC links to jump to specific sections
- Click chart bars to navigate to category sections

**Organization:**
```
📊 Statistics
📂 Knowledge Discovery Categories (expanded)
📝 Executive Summary (expanded)
📋 Table of Contents
🧩 Combinatorial Insights (collapsed)
   └─ Motif, K-Core, K-Truss, etc.
🔍 Exploratory Insights (collapsed)
   └─ Bridge, Completion, Domain Bridge, etc.
🔄 Transformational Insights (collapsed)
   └─ Surprise, Contradiction, Hypotheses, etc.
```

**Performance:**
- Clean initial load (collapsed sections)
- Faster page rendering
- Better for large result sets

See [REPORT_UI_IMPROVEMENTS.md](REPORT_UI_IMPROVEMENTS.md) for details.

## Stage 6: Finalizing

### Expected inputs
- Outputs from stages 1-5
- run metadata (`run_id`, timestamps, source PDFs, stats)

### Expected outputs
- `manifest.json` containing:
  - run metadata
  - stage completion metadata
  - stats summary
  - artifact paths
  - preprocess stats (if enabled)
- `README.txt` with run artifact guide

## 3. Resume / Partial Execution Contract

`kg run` supports resume with:
- `--from-stage/-f` in `[1..5]`
- `--run-dir/-d` required when `from-stage > 1`

Expected required files when resuming:
- from stage 2: requires existing `graph.json`
- from stage 3: requires existing `index.json` (and `graph.json`)
- from stage 4: requires existing `insights.json` (plus prior graph/index)
- from stage 5: requires existing `graph.json`, `index.json`, and `insights.json`

If required files are missing, pipeline exits with error.

## 4. Standalone Command I/O (Non-`run`)

## `kg index`
- Input: `--input` hypergraph JSON
- Output: `<output_dir>/hypergraph_index.json`

## `kg discover`
- Input: `--input` graph JSON, optional `--index`
- Output: `--output` insights JSON

## `kg render`
- Input: `--input` graph JSON, optional `--insights`
- Output directory files:
  - `final_graph.html`
  - `final_graph_rag.html`
  - if insights supplied: `final_graph_augmented.html`, `augmentation.json`, `viewer_bundle.json`
  - `README.txt`

## `kg report`
- Input: `--input` graph JSON + `--insights` JSON
- Output: report file path (`.md` or `.html`)

## 5. Primary Data Types Passed Between Stages

- Graph object: `Hypergraph`
- Index object: `HypergraphIndex`
- Discovery object: `InsightCollection`
- Augmentation object: `AugmentationData`
- Extraction telemetry: `PipelineStatistics`

These are serialized to JSON artifacts between stages for persistence and resume.

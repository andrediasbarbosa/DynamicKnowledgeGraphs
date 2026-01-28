# Knowledge Graph Pipeline CLI Guide

This document explains how to use the `kg` command-line tool to extract knowledge graphs from PDF documents, discover insights, and generate visualizations and reports.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Pipeline Overview](#pipeline-overview)
3. [Configuration](#configuration)
4. [Commands Reference](#commands-reference)
5. [Pipeline Stages](#pipeline-stages)
6. [Output Files](#output-files)
7. [Examples](#examples)
8. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Full Pipeline (Recommended)

Run the complete pipeline from PDF to visualization in one command:

```bash
# Basic usage
./build/bin/kg run -i document.pdf -t "My Knowledge Graph"

# With custom output directory
./build/bin/kg run -i document.pdf -o ./my_output -t "Research Paper Analysis"

# Process multiple PDFs from a directory
./build/bin/kg run -i ./pdf_folder/ -t "Multi-Document Analysis"
```

### View Results

After the pipeline completes:

```bash
cd runs/run_YYYYMMDD_HHMMSS
python3 -m http.server 8080
# Open http://localhost:8080/final_graph_augmented.html in browser
```

---

## Pipeline Overview

The `kg` tool processes documents through 5 sequential stages, with an optional preprocessing step between extract and index:

```
┌─────────────┐     ┌──────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ 1. EXTRACT  │───▶│ 1.5 PREPROC  │───▶│  2. INDEX   │──▶│ 3. DISCOVER │───▶│ 4. RENDER   │──▶│  5. REPORT  │
│             │     │ (optional)   │    │             │    │             │    │             │    │             │
│ PDF → Graph │     │ Normalize    │    │ Build       │    │ Find        │    │ Generate    │    │ Generate    │
│ (LLM-based) │     │ + Merge      │    │ S-component │    │ Insights    │    │ Interactive │    │ Markdown &  │
│             │     │ Aliases      │    │ Index       │    │ (bridges,   │    │ 3D HTML     │    │ HTML Report │
│             │     │              │    │             │    │ patterns)   │    │ Viewer      │    │             │
└─────────────┘     └──────────────┘    └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

---

## Configuration

### LLM Configuration

Create a `.llm_config.json` file in the project root:

**For OpenAI:**
```json
{
  "provider": "openai",
  "api_key": "your-openai-api-key",
  "model": "gpt-4o-mini",
  "temperature": 0.0,
  "max_tokens": 2000,
  "timeout_seconds": 60,
  "max_retries": 3,
  "verbose": false
}
```

**For Google Gemini:**
```json
{
  "provider": "gemini",
  "api_key": "your-gemini-api-key",
  "model": "gemini-2.0-flash",
  "temperature": 0.0,
  "max_tokens": 2000,
  "timeout_seconds": 60,
  "max_retries": 3,
  "verbose": false
}
```

### Environment Variables

Alternatively, set your API key via environment variable:

```bash
export OPENAI_API_KEY="your-key"
# or
export GEMINI_API_KEY="your-key"
```

---

## Commands Reference

### `kg run` - Full Pipeline

Run the complete extraction-to-report pipeline.

```
Usage: kg run [options]

Options:
  --input, -i <value>       Input PDF file or directory containing PDFs
  --output, -o <value>      Base output directory (default: runs/)
  --config, -c <value>      Path to LLM config file (optional)
  --operators, -p <value>   Discovery operators (default: all)
  --title, -t <value>       Title for reports and visualizations
  --max-examples, -m <value> Max examples per insight type (default: 10)
  --from-stage, -f <value>  Start from stage 1-5 (default: 1)
  --run-dir, -d <value>     Existing run directory to resume
  --preprocess, -P          Normalize relations and merge aliases before indexing
```

**Examples:**

```bash
# Full pipeline
kg run -i paper.pdf -t "Research Analysis"

# Resume from stage 3 (discovery)
kg run -f 3 -d runs/run_20260118_164315

# Custom operators
kg run -i paper.pdf -p "bridges,completions,motifs,surprise"
```

---

### `kg index` - Build Index

Build s-component indices for a hypergraph.

```
Usage: kg index --input <value> [options]

Options:
  --input, -i <value>       Input hypergraph JSON file [required]
  --output, -o <value>      Output directory (default: index/)
  --s-components, -s <value> S-values for s-components (default: 2,3,4)
```

**Example:**

```bash
kg index -i graph.json -o ./index_output -s "2,3,4,5"
```

---

### `kg discover` - Find Insights

Run discovery operators to find structural insights in the graph.

```
Usage: kg discover --input <value> --output <value> [options]

Options:
  --input, -i <value>       Input hypergraph JSON file [required]
  --index, -x <value>       Index directory (optional)
  --output, -o <value>      Output path for insights JSON [required]
  --operators, -p <value>   Operators to run (default: bridges,completions,motifs)
  --run-id, -r <value>      Run ID for tracking
```

**Available Operators:**

| Operator | Description |
|----------|-------------|
| `bridges` | Find bridge nodes connecting different communities |
| `completions` | Suggest missing links based on structural patterns |
| `motifs` | Detect recurring structural patterns |
| `substitutions` | Find potentially interchangeable entities |
| `surprise` | Identify statistically unexpected connections |
| `contradictions` | Flag conflicts where affirmed and negated claims both appear |
| `entity_resolution` | Detect likely duplicate or alias entities |
| `core_periphery` | Identify core vs peripheral entities and hub/authority roles |
| `text_similarity` | Link entities with similar labels using text semantics |
| `argument_support` | Propose relations supported by argument/evidence paths |
| `active_learning` | Generate high-value verification queries for the graph |
| `method_outcome` | Identify method or outcome entities in the graph |
| `centrality` | Rank entities by structural centrality in a bipartite projection |
| `community_detection` | Detect dense communities (Louvain on projection) |
| `k_core` | Highlight nodes in dense k-core regions |
| `k_truss` | Identify triangle-reinforced edges in dense subgraphs |
| `claim_stance` | Classify claim stance (supports/opposes/neutral) |
| `relation_induction` | Suggest canonical relation type names |
| `analogical_transfer` | Propose new links by analogy |
| `uncertainty_sampling` | Surface low-confidence relations for verification |
| `counterfactual` | Generate falsification probes for claims |
| `hyperedge_prediction` | Predict relations using overlap patterns |
| `diffusion` | Map influence pathways via random-walk diffusion |
| `rules` | Mine association rules between relation types |
| `community` | Propose cross-cluster links with similar relation signatures |
| `pathrank` | Rank link candidates using short-path evidence |
| `embedding` | Predict missing links using embeddings |
| `author_chain` | Trace citation chains across authors |
| `hypotheses` | Synthesize testable hypotheses from findings |

**Example:**

```bash
kg discover -i graph.json -o insights.json -p "bridges,completions,motifs,contradictions,entity_resolution,core_periphery,text_similarity,argument_support,active_learning,method_outcome,centrality,community_detection,k_core,k_truss,claim_stance,relation_induction,analogical_transfer,uncertainty_sampling,counterfactual,hyperedge_prediction"
```

---

### `kg render` - Generate Visualizations

Export interactive 3D graph visualizations.

```
Usage: kg render --input <value> --output <value> [options]

Options:
  --input, -i <value>       Input hypergraph JSON file [required]
  --insights, -n <value>    Insights JSON file (for augmented view)
  --output, -o <value>      Output directory [required]
  --title, -t <value>       Visualization title (default: Knowledge Graph)
```

**Example:**

```bash
# Baseline graph only
kg render -i graph.json -o ./viz -t "My Graph"

# With augmentation overlay
kg render -i graph.json -n insights.json -o ./viz -t "Augmented Graph"
```

**Output Files:**

- `final_graph.html` - Baseline interactive 3D viewer
- `final_graph_augmented.html` - Augmented view with discovery insights
- `augmentation.json` - Overlay data for augmented nodes/links
- `viewer_bundle.json` - Full insight metadata

---

### `kg report` - Generate Reports

Generate natural language reports from insights.

```
Usage: kg report --input <value> --insights <value> --output <value> [options]

Options:
  --input, -i <value>       Input hypergraph JSON file [required]
  --insights, -n <value>    Insights JSON file [required]
  --output, -o <value>      Output path (.md or .html) [required]
  --format, -f <value>      Format: auto, markdown, html (default: auto)
  --title, -t <value>       Report title
  --source, -s <value>      Source document name
  --max-examples, -m <value> Max examples per insight type (default: 5)
```

**Example:**

```bash
# Markdown report
kg report -i graph.json -n insights.json -o report.md -t "Analysis Report"

# HTML report
kg report -i graph.json -n insights.json -o report.html -t "Analysis Report"
```

---

### `kg stats` - Graph Statistics

Print statistics about a hypergraph.

```
Usage: kg stats --input <value>

Options:
  --input, -i <value>       Input hypergraph JSON file [required]
```

**Example:**

```bash
kg stats -i graph.json
```

---

## Pipeline Stages

### Stage 1: Extract (PDF to Graph)

- Processes PDF documents using Poppler
- Chunks text into semantic units
- Uses LLM to extract entities and relationships
- Builds hypergraph structure

**Requires:** LLM API key configured

### Stage 1.5: Preprocess (optional)

- Normalizes relation labels to canonical forms
- Merges likely aliases (entity resolution + exact label normalization)
- Saves `graph_raw.json` and overwrites `graph.json` with the preprocessed graph

### Stage 2: Index

- Computes s-components (connected components at various overlap levels)
- Builds adjacency indices
- Calculates node degrees and statistics

### Stage 3: Discover

- Runs structural analysis operators
- Identifies bridges, motifs, completions
- Computes surprise scores
- Generates insight collection

### Stage 4: Render

- Generates interactive 3D WebGL visualizations
- Creates baseline and augmented views
- Exports graph data in multiple formats

### Stage 5: Report

- Generates human-readable reports
- Summarizes key insights
- Provides examples and statistics

---

## Output Files

A typical run produces the following in `runs/run_YYYYMMDD_HHMMSS/`:

| File | Description |
|------|-------------|
| `graph.json` | Full hypergraph in JSON format |
| `graph_raw.json` | Raw graph prior to preprocessing (if enabled) |
| `*_extractions.json` | Raw LLM extraction results |
| `extraction_stats.json` | Extraction statistics |
| `index.json` | S-component index |
| `insights.json` | Discovery insights |
| `final_graph.html` | Interactive 3D baseline viewer |
| `final_graph_augmented.html` | Augmented viewer with insights |
| `augmentation.json` | Augmentation overlay data |
| `graph.dot` | GraphViz DOT format |
| `report.md` | Markdown report |
| `report.html` | Styled HTML report |
| `manifest.json` | Run metadata |

---

## Examples

### Example 1: Analyze a Research Paper

```bash
# Run full pipeline
./build/bin/kg run \
  -i "papers/transformer_architecture.pdf" \
  -t "Transformer Architecture Analysis" \
  -p "bridges,completions,surprise"

# View results
cd runs/run_*
python3 -m http.server 8080
```

### Example 2: Process Multiple Documents

```bash
# Put all PDFs in a folder
./build/bin/kg run \
  -i ./research_papers/ \
  -t "Literature Review" \
  -o ./literature_output
```

### Example 3: Resume from Discovery Stage

If extraction completed but you want to re-run discovery with different operators:

```bash
./build/bin/kg run \
  -f 3 \
  -d runs/run_20260118_164315 \
  -p "bridges,motifs,substitutions,contradictions,entity_resolution,core_periphery,text_similarity,argument_support,active_learning,method_outcome,surprise"
```

### Example 4: Re-render with New Title

```bash
./build/bin/kg run \
  -f 4 \
  -d runs/run_20260118_164315 \
  -t "Updated Visualization Title"
```

### Example 5: Individual Commands

```bash
# Step by step manual pipeline
kg index -i graph.json -o ./index
kg discover -i graph.json -x ./index -o insights.json -p "bridges,surprise"
kg render -i graph.json -n insights.json -o ./output -t "My Graph"
kg report -i graph.json -n insights.json -o report.html
```

---

## Troubleshooting

### LLM API Errors

```
Error: Failed to get LLM response
```

- Check your API key in `.llm_config.json`
- Verify network connectivity
- Check API rate limits

### PDF Processing Errors

```
Error: Failed to extract text from PDF
```

- Ensure Poppler is installed: `apt install poppler-utils`
- Check PDF is not encrypted or corrupted

### Visualization Freezes

If the HTML viewer is unresponsive:

1. Try the baseline viewer (`final_graph.html`) first
2. Increase "Min Entity Degree" slider to reduce visible nodes
3. Disable "Show Labels" checkbox
4. Use a WebGL-capable browser (Chrome/Firefox recommended)

### Memory Issues

For large documents:

- Process documents individually rather than in batches
- Increase system memory or use a machine with more RAM
- Reduce chunk size in pipeline config

### Resume Failed Run

```bash
# Check what stage failed
cat runs/run_*/manifest.json

# Resume from the failed stage
kg run -f <stage_number> -d runs/run_YYYYMMDD_HHMMSS
```

---

## Building from Source

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)

# Binary will be at build/bin/kg
```

---

## Version

```bash
kg --help
# Shows: Version: 1.0.0
```

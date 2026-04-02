# Pipeline

This document reflects the current `kg run` implementation in `src/main.cpp`.

## Command

```bash
./build/bin/kg run -i <pdf-or-directory> [options]
```

## Runtime Order

The console stage numbers and the output folder names are not identical. The current execution order is:

1. `Stage 1`: Knowledge Extraction
2. `Stage 1.5`: Preprocess Graph, optional
3. `Stage 1.75`: Quality Control, enabled by default
4. `Stage 1.9`: Ontology Classification, optional
5. `Stage 2`: Build Index
6. `Stage 3`: Knowledge Discovery
7. `Stage 4`: Generate Visualizations
8. `Stage 5`: Generate Reports
9. `Stage 6`: Finalize run metadata

The run directory still uses the step folders:

- `Step_1_Loading/`
- `Step_2_Extraction/`
- `Step_3_QualityControl/`
- `Step_4_GraphBuilding/`
- `Step_5_Discovery/`

## Stage Details

### Stage 1: Knowledge Extraction

Inputs:

- `--input/-i`: PDF file or directory
- optional `--config/-c`
- otherwise `.llm_config.json` or environment fallback

Behavior:

- loads PDFs with `PDFProcessor`
- chunks text according to `PipelineConfig`
- calls the configured LLM provider for relation extraction
- builds a `Hypergraph`

Outputs:

- `Step_2_Extraction/extraction_stats.json`
- `Step_4_GraphBuilding/graph.json` when `--preprocess` is off
- `Step_4_GraphBuilding/graph_raw.json` when `--preprocess` is on
- per-document extraction files in `Step_2_Extraction/` when intermediates are enabled

### Stage 1.5: Preprocess Graph

Runs only with `--preprocess`.

Behavior:

- normalizes relation labels
- merges alias-like entities using entity-resolution candidates

Outputs:

- preserves `Step_4_GraphBuilding/graph_raw.json`
- writes `Step_4_GraphBuilding/graph.json`
- stores counts in `manifest.json`

### Stage 1.75: Quality Control

Runs unless `--no-qc` is provided.

Behavior:

- rule-based filtering
- label simplification
- optional semantic deduplication
- statistical filtering
- optional LLM validation
- connectivity analysis

Inputs:

- `--min-node-length`
- `--min-degree`
- `--llm-validate`
- `--validation-mode`
- `--semantic-dedup`
- `--semantic-threshold`

Outputs:

- `Step_3_QualityControl/cleaning_report.json`
- `Step_3_QualityControl/quality_control_report.html`
- updated `Step_4_GraphBuilding/graph.json`

### Stage 1.9: Ontology Classification

Runs with `--with-ontology`.

Behavior:

- applies heuristic class/instance classification to nodes missing `node_level`
- augments instance nodes with base-class information

Output:

- rewrites `Step_4_GraphBuilding/graph.json`

### Stage 2: Build Index

Behavior:

- builds `HypergraphIndex`
- currently computes s-components for `2,3,4`

Output:

- `Step_5_Discovery/index.json`

### Stage 3: Knowledge Discovery

Behavior:

- expands `all` to the active runtime registry from `include/discovery/operator_registry.hpp`
- applies optional causal filters in `kg discover`
- writes typed insights

Defaults:

- `kg run`: `all`
- `kg discover`: `bridges,completions,motifs`

Output:

- `Step_5_Discovery/insights.json`

### Stage 4: Generate Visualizations

Outputs:

- `Step_5_Discovery/graph.html`
- `Step_5_Discovery/augmentation.json`
- `Step_5_Discovery/graph_augmented.html`
- `Step_5_Discovery/graph_rag.html`
- `Step_5_Discovery/graph.dot`

### Stage 5: Generate Reports

Outputs:

- `Step_5_Discovery/report.md`
- `Step_5_Discovery/report.html`
- `Step_5_Discovery/pattern_library.json`

### Stage 6: Finalize

Outputs:

- `manifest.json`
- `README.txt`

## Resume Behavior

Resume from an existing run:

```bash
./build/bin/kg run -f <stage> -d runs/run_YYYYMMDD_HHMMSS
```

Rules enforced by the CLI:

- `--from-stage` must be `1` through `5`
- `--run-dir` is required when `--from-stage > 1`
- `--preprocess` can only be used with `--from-stage 1` or `2`

## Related Docs

- [`CLI_REFERENCE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/CLI_REFERENCE.md)
- [`OUTPUT_FOLDER_STRUCTURE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/OUTPUT_FOLDER_STRUCTURE.md)
- [`QUALITY_CONTROL.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/QUALITY_CONTROL.md)

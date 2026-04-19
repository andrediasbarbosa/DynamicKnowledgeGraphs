# `kg` Usage Guide

This is the compact, task-oriented guide for the `kg` CLI.

For the full option list, see [`CLI_REFERENCE.md`](CLI_REFERENCE.md). For stage-by-stage behavior, see [`PIPELINE.md`](PIPELINE.md).

## Binary

```bash
./build/bin/kg
```

## Get Help

```bash
./build/bin/kg --help
./build/bin/kg run --help
./build/bin/kg discover --help
```

The runtime help text is the source of truth for current flags.

## Most Common Command

Run the full PDF-to-report pipeline:

```bash
./build/bin/kg run -i /path/to/pdfs -t "My Run"
```

What `kg run` does:

1. Extracts relations from PDFs with the configured LLM
2. Optionally preprocesses the graph
3. Runs quality control by default
4. Builds indices
5. Runs discovery operators
6. Generates HTML graph views and reports
7. Writes run metadata

## `kg run` Cheatsheet

```bash
./build/bin/kg run -i <pdf-or-dir> [options]
```

Important options:

- `-i, --input`: input PDF or directory of PDFs
- `-o, --output`: base output directory, default `runs/`
- `-c, --config`: LLM config file
- `-p, --operators`: comma-separated operator list or `all`
- `-t, --title`: title for reports and visualizations
- `-m, --max-examples`: max examples per insight type in reports
- `-f, --from-stage`: resume from stage `1` to `5`
- `-d, --run-dir`: existing run directory when resuming
- `-P, --preprocess`: normalize relations and merge aliases
- `-C, --causal`: causal extraction mode
- `-O, --with-ontology`: ontology classification
- `--no-qc`: disable quality control
- `--llm-validate`: enable LLM validation in QC
- `--validation-mode`: `all`, `suspicious`, or `none`
- `--semantic-dedup`: semantic deduplication
- `--semantic-threshold`: semantic dedup threshold

## Common Workflows

Basic run:

```bash
./build/bin/kg run -i papers/
```

Run with preprocessing:

```bash
./build/bin/kg run -i papers/ --preprocess
```

Run with ontology classification:

```bash
./build/bin/kg run -i papers/ --with-ontology
```

Run with causal extraction:

```bash
./build/bin/kg run -i papers/ --causal
```

Run with semantic deduplication:

```bash
./build/bin/kg run -i papers/ --semantic-dedup
```

Run a selected operator set:

```bash
./build/bin/kg run -i papers/ -p bridges,motifs,community_detection
```

Resume an existing run from discovery:

```bash
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS
```

## Stage Map

`kg run` executes these logical stages:

- `1`: extraction
- `1.5`: preprocess, optional
- `1.75`: quality control, default on
- `1.9`: ontology classification, optional
- `2`: index building
- `3`: discovery
- `4`: visualization
- `5`: report generation
- `6`: manifest/finalization

Resume rules:

- `--from-stage` must be `1` through `5`
- `--run-dir` is required when `--from-stage > 1`
- `--preprocess` only makes sense when resuming from stage `1` or `2`

## Output Locations

By default, each run creates a timestamped directory under `runs/`.

Most useful artifacts:

- `Step_4_GraphBuilding/graph.json`
- `Step_5_Discovery/insights.json`
- `Step_5_Discovery/graph_augmented.html`
- `Step_5_Discovery/report.html`
- `manifest.json`

See [`OUTPUT_FOLDER_STRUCTURE.md`](OUTPUT_FOLDER_STRUCTURE.md) for the full layout.

## Other Commands

Build graph indices:

```bash
./build/bin/kg index -i graph.json
```

Run discovery on an existing graph:

```bash
./build/bin/kg discover -i graph.json -o insights.json
```

Render a graph view:

```bash
./build/bin/kg render -i graph.json -n insights.json -o out/
```

Generate a report from existing graph and insights:

```bash
./build/bin/kg report -i graph.json -n insights.json -o report.html
```

Print graph statistics:

```bash
./build/bin/kg stats -i graph.json
```

List live discovery operators:

```bash
./build/bin/kg list-operators
```

## Related Docs

- [`CLI_REFERENCE.md`](CLI_REFERENCE.md)
- [`PIPELINE.md`](PIPELINE.md)
- [`QUICK_START.md`](QUICK_START.md)
- [`QUALITY_CONTROL.md`](QUALITY_CONTROL.md)
- [`OUTPUT_FOLDER_STRUCTURE.md`](OUTPUT_FOLDER_STRUCTURE.md)

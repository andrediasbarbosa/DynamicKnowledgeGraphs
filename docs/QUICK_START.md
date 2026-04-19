# Quick Start

This is the shortest path to a successful `kg` run.

## 1. Build

```bash
./build.sh build
```

If you need PDF extraction support, install Poppler first:

```bash
sudo apt-get install -y libpoppler-cpp-dev
```

## 2. Configure the LLM

```bash
cp .llm_config.json.example .llm_config.json
```

Set at least:

- `provider`
- `api_key`
- `model`

## 3. Run the full pipeline

```bash
./build/bin/kg run -i /path/to/file-or-folder -t "My Run"
```

What happens:

1. PDFs are chunked and sent to the configured LLM for extraction.
2. The graph is optionally preprocessed.
3. Quality control runs by default.
4. The graph is indexed, discovery operators run, HTML views are generated, and reports are written.

## Common Variants

Run all operators with preprocessing:

```bash
./build/bin/kg run -i papers/ -p all --preprocess
```

Enable ontology classification:

```bash
./build/bin/kg run -i papers/ --with-ontology
```

Enable semantic deduplication:

```bash
./build/bin/kg run -i papers/ --semantic-dedup
```

Resume an existing run from discovery:

```bash
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS
```

Inspect operators:

```bash
./build/bin/kg list-operators
```

## Outputs

Each run creates a timestamped folder under `runs/` by default. The canonical artifact layout is documented in [`OUTPUT_FOLDER_STRUCTURE.md`](OUTPUT_FOLDER_STRUCTURE.md).

The main results are typically:

- `Step_4_GraphBuilding/graph.json`
- `Step_5_Discovery/insights.json`
- `Step_5_Discovery/graph_augmented.html`
- `Step_5_Discovery/report.html`

## Next Docs

- [`CLI_REFERENCE.md`](CLI_REFERENCE.md)
- [`PIPELINE.md`](PIPELINE.md)
- [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md)

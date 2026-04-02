# CLI Reference

Runtime binary:

```bash
./build/bin/kg
```

## Global Help

```bash
./build/bin/kg --help
```

Commands:

- `run`
- `index`
- `discover`
- `render`
- `report`
- `stats`
- `list-operators`

## `kg run`

Full pipeline:

```bash
./build/bin/kg run -i <input> [options]
```

Important options:

- `-i, --input`: PDF file or directory
- `-o, --output`: base output directory, default `runs/`
- `-c, --config`: LLM config file
- `-p, --operators`: discovery operator list, default `all`
- `-t, --title`: report/visualization title
- `-m, --max-examples`: max examples per insight type in reports, default `10`
- `-f, --from-stage`: resume stage `1` to `5`, default `1`
- `-d, --run-dir`: existing run directory for resume
- `-P, --preprocess`: normalize relations and merge aliases before indexing
- `-C, --causal`: use causal extraction prompts
- `-O, --with-ontology`: enable ontology classification
- `--no-qc`: disable quality control
- `--min-node-length`: minimum entity label length, default `2`
- `--min-degree`: minimum node degree, CLI default `1`
- `--llm-validate`: enable LLM validation in QC
- `--validation-mode`: `all`, `suspicious`, or `none`
- `--semantic-dedup`: enable semantic deduplication
- `--semantic-threshold`: similarity threshold, default `0.85`

Examples:

```bash
./build/bin/kg run -i papers/
./build/bin/kg run -i papers/ --preprocess --semantic-dedup
./build/bin/kg run -i papers/ --with-ontology --causal
./build/bin/kg run -f 3 -d runs/run_YYYYMMDD_HHMMSS
```

## `kg index`

```bash
./build/bin/kg index -i <graph.json> [options]
```

Options:

- `-i, --input`: input hypergraph JSON
- `-o, --output`: index directory, default `index/`
- `-s, --s-components`: comma-separated s-values, default `2,3,4`

## `kg discover`

```bash
./build/bin/kg discover -i <graph.json> -o <insights.json> [options]
```

Options:

- `-i, --input`: input hypergraph JSON
- `-x, --index`: optional index directory
- `-o, --output`: output insights JSON
- `-p, --operators`: operator list, default `bridges,completions,motifs`
- `-r, --run-id`: run identifier
- `--causal-strength`
- `--causal-type`
- `--mechanism-type`
- `--temporality`

## `kg render`

```bash
./build/bin/kg render -i <graph.json> -o <output-dir> [options]
```

Options:

- `-i, --input`: input hypergraph JSON
- `-n, --insights`: optional insights JSON
- `-o, --output`: output directory
- `-t, --title`: title, default `Knowledge Graph`

## `kg report`

```bash
./build/bin/kg report -i <graph.json> -n <insights.json> -o <report> [options]
```

Options:

- `-i, --input`: input hypergraph JSON
- `-n, --insights`: insights JSON
- `-o, --output`: output path
- `-f, --format`: `auto`, `markdown`, `html`
- `-t, --title`: title, default `Knowledge Discovery Report`
- `-s, --source`: source document label
- `-m, --max-examples`: max examples per type, default `5`

## `kg stats`

```bash
./build/bin/kg stats -i <graph.json>
```

## `kg list-operators`

```bash
./build/bin/kg list-operators
```

The runtime currently reports 62 operators in the default registry.

## Operator Defaults

- `kg run` expands `all` using `all_discovery_operators()`
- `kg discover` does not default to `all`; it defaults to `bridges,completions,motifs`

For the live list, prefer:

```bash
./build/bin/kg list-operators
```

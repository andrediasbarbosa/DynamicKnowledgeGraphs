# Python Knowledge Graph Extractor

This is the lightweight Python-only path in the repository. It is useful when you want direct Azure OpenAI extraction without building the C++ pipeline.

## Scope

What it does:

- extracts text from PDFs with `pdfplumber`
- calls Azure OpenAI for relation extraction
- builds a hypergraph-style `graph.json`
- generates an interactive `graph.html`

What it does not do:

- build the `kg` index
- run the 62-operator discovery registry
- generate `kg` reports or augmented discovery overlays
- support staged resume flow

For those, use `./build/bin/kg`.

## Setup

### `venv`

```bash
cd src/python_porting
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Conda

```bash
cd src/python_porting
conda env create -f environment.yml
conda activate kg-extractor
```

## Azure Configuration

Set these directly in `kg_extractor.py`:

- `AZURE_OPENAI_KEY`
- `AZURE_OPENAI_ENDPOINT`

Current defaults in code:

- API version: `2024-05-01-preview`
- deployment: `gpt-5.2`

## Run

```bash
python kg_extractor.py /path/to/file.pdf --output ./output --verbose
```

Useful flags:

- `--chunk-size`
- `--chunk-overlap`
- `--max-chunks`

## Outputs

- `output/graph.json`
- `output/graph.html`

To view:

```bash
cd output
python3 -m http.server 8080
```

Then open `http://localhost:8080/graph.html`.

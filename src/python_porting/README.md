# Python Knowledge Graph Extractor

Lightweight single-script extractor for PDF -> hypergraph JSON -> interactive HTML.

This path is useful for quick Azure OpenAI-driven extraction without building the C++ pipeline.

## What It Does

- Extracts text chunks from PDFs using `pdfplumber`
- Calls Azure OpenAI for relation extraction
- Builds hypergraph-style `graph.json`
- Generates `graph.html` for interactive viewing

## Requirements

- Python 3.10+
- Azure OpenAI API key and endpoint
- Conda (recommended) or `venv`

## Install

### Conda (recommended)

```bash
cd src/python_porting
conda env create -f environment.yml
conda activate kg-extractor
```

### Pip/venv

```bash
cd src/python_porting
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## Configure Azure Credentials

Edit `kg_extractor.py` and set:

- `AZURE_OPENAI_KEY`
- `AZURE_OPENAI_ENDPOINT`

Preconfigured defaults in code:

- `AZURE_OPENAI_API_VERSION = "2024-05-01-preview"`
- `AZURE_OPENAI_DEPLOYMENT = "gpt-5.2"`

## Run

```bash
python kg_extractor.py /path/to/input.pdf --output ./output --verbose
```

Options:

- `--chunk-size` default `800`
- `--chunk-overlap` default `100`
- `--max-chunks` for test runs

## Output

- `output/graph.json`
- `output/graph.html`
- `output/README.txt`

View:

```bash
cd output
python3 -m http.server 8080
# open http://localhost:8080/graph.html
```

## Known Scope Limits vs C++ `kg`

Not included here:

- 55 discovery operators
- report generation
- augmented insight overlays
- full multi-stage pipeline/resume flow

For those, use the C++ CLI in root: `./build/bin/kg`.

## Troubleshooting

- Credentials missing: set Azure key/endpoint in `kg_extractor.py`.
- 404/resource errors: verify deployment name and endpoint.
- Empty graph: increase chunk size or inspect PDF extractability.

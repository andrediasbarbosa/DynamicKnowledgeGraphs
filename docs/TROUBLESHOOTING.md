# Troubleshooting

## `kg` Build And Runtime

### `kg` does not exist after build

Check:

```bash
./build.sh build
ls build/bin
```

Expected binary: `build/bin/kg`

### PDF extraction is unavailable

Cause:

- Poppler was not installed when CMake configured the project

Fix:

```bash
sudo apt-get install -y libpoppler-cpp-dev
./build.sh rebuild
```

### `kg run` fails with config or API-key errors

Fix one of:

```bash
cp .llm_config.json.example .llm_config.json
```

or export provider credentials in the environment.

### Resume run fails with missing files

`--from-stage > 1` requires `--run-dir`, and the run directory must already contain the earlier artifacts. Check for:

- `Step_4_GraphBuilding/graph.json`
- `Step_5_Discovery/index.json`
- `Step_5_Discovery/insights.json`

### Discovery results do not match the operator list you expected

Remember:

- `kg run` defaults to `all`
- `kg discover` defaults to `bridges,completions,motifs`

Use:

```bash
./build/bin/kg list-operators
```

## Graph RAG

### Browser opens a local file instead of the app

Wrong:

```text
file:///...
```

Right:

```text
http://localhost:8000
```

### Graph RAG server starts but the UI is blank or API calls fail

Make sure you started it from the backend directory:

```bash
cd src/graph_rag_tool/backend
python graph_rag_server.py
```

The server serves `template.html` from the current working directory.

### Port 8000 is already in use

Linux/macOS:

```bash
lsof -ti:8000 | xargs kill
```

Windows:

```cmd
netstat -ano | findstr :8000
taskkill /PID <PID> /F
```

### Local embedding model is unavailable

If `sentence-transformers` cannot load, either install the standard requirements again or switch to Azure embeddings:

```bash
export EMBEDDING_MODE=azure
python graph_rag_server.py
```

### Uploaded graph loads in the frontend but queries fail

Check the backend logs for database initialization errors. The Kuzu database path is `./graph_rag_db` relative to `src/graph_rag_tool/backend/`.

## Python Port

### Python extractor exits immediately with missing Azure config

Set these in `src/python_porting/kg_extractor.py`:

- `AZURE_OPENAI_KEY`
- `AZURE_OPENAI_ENDPOINT`

### Python extractor creates output but the graph is empty

Typical causes:

- PDF text extraction returned little or no text
- the Azure deployment name is wrong
- credentials or endpoint are invalid

Inspect the source PDF and the Azure deployment settings first.

# Deployment

This repository has three different runtime targets. They do not share the same setup.

## 1. `kg` C++ Pipeline

### Dependencies

Ubuntu/WSL:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libcurl4-openssl-dev nlohmann-json3-dev
sudo apt-get install -y libpoppler-cpp-dev libgtest-dev graphviz
```

Notes:

- `libpoppler-cpp-dev` enables PDF extraction
- `libgtest-dev` is only needed for unit tests
- `graphviz` is only needed if you want to work with `graph.dot`

### Build

```bash
./build.sh build
```

### Runtime Configuration

Recommended:

```bash
cp .llm_config.json.example .llm_config.json
```

Fallback environment variables are supported for provider selection and API keys.

### Run

```bash
./build/bin/kg run -i /path/to/pdfs
```

## 2. Graph RAG Application

Active runtime directory:

```bash
src/graph_rag_tool/backend
```

### Standard Local Setup

```bash
cd src/graph_rag_tool/backend
pip install -r requirements.txt
python graph_rag_server.py
```

This starts:

- app: `http://localhost:8000`
- FastAPI docs: `http://localhost:8000/docs`

### Embedding Modes

The backend reads `EMBEDDING_MODE`:

- `local` (default): sentence-transformers, 384-dimensional embeddings
- `azure`: Azure OpenAI embeddings, 1536-dimensional embeddings

Azure mode example:

```bash
export EMBEDDING_MODE=azure
python graph_rag_server.py
```

The standalone deployment bundle lives in:

```bash
src/graph_rag_tool/deployment/
```

Use that folder only when you want the packaged distribution, not the active development copy.

## 3. Python Port

```bash
cd src/python_porting
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

Then:

```bash
python kg_extractor.py /path/to/file.pdf --output ./output --verbose
```

This path requires Azure credentials to be set directly in `kg_extractor.py`.

## Windows Notes

- the Graph RAG deployment bundle contains `start.bat` and `start.sh`
- the active backend in `src/graph_rag_tool/backend/` is started directly with `python graph_rag_server.py`
- when using the browser UI, use `http://localhost:8000`, not a `file://` URL

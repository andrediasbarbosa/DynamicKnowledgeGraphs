# Graph RAG Backend

This directory is the active runtime copy of the Graph RAG application.

## Start

```bash
cd src/graph_rag_tool/backend
pip install -r requirements.txt
python graph_rag_server.py
```

Open:

- `http://localhost:8000`
- `http://localhost:8000/docs`

Run it from this directory. The root route serves `template.html` from the current working directory.

## What It Provides

- 3D graph visualization
- natural-language query endpoint with `graph-rag`, `doc-rag`, and `compare` modes
- Kuzu database explorer
- upload, reload, clean, schema, quality, entity, relation, Cypher, and path endpoints

## Runtime Behavior

- database path: `./graph_rag_db`
- embedding mode comes from `EMBEDDING_MODE`
  - `local`: sentence-transformers, 384 dimensions
  - `azure`: Azure OpenAI embeddings, 1536 dimensions
- the UI uploads a graph JSON file and the backend initializes Kuzu from it

## Main Files

- `graph_rag_server.py`: FastAPI backend
- `template.html`: frontend served at `/`
- `requirements.txt`: standard local requirements
- `docs/API_REFERENCE.md`: endpoint reference

## Graph JSON Notes

Supported shapes:

```json
{
  "nodes": [...],
  "hyperedges": [...]
}
```

or

```json
{
  "nodes": [...],
  "links": [...]
}
```

Entity uniqueness in the backend is based on a synthetic composite key built from:

- `degree`
- sorted `docs`
- `id`

That key is also hashed into the `composite_hash` values exposed by the relation APIs.

# Graph RAG Tool

This directory contains two distinct copies of the Graph RAG app:

- `backend/`: the active development/runtime copy
- `deployment/`: a standalone packaged bundle

Use `backend/` unless you explicitly want the packaged deployment bundle.

## Active Runtime

Start from:

```bash
cd src/graph_rag_tool/backend
pip install -r requirements.txt
python graph_rag_server.py
```

Open:

- `http://localhost:8000`
- `http://localhost:8000/docs`

## Main Capabilities

- 3D graph viewer
- Graph-RAG, document-RAG, and compare modes through `/api/query`
- Kuzu-backed explorer for schema, Cypher, entities, relations, paths, and quality metrics
- upload or reload graph JSON files into the backend

## Graph Formats

The UI and backend work with either:

- hypergraph-style JSON: `nodes` + `hyperedges`
- pairwise graph JSON: `nodes` + `links`

The backend stores pairwise `RELATED` edges in Kuzu and keeps per-entity composite hashes for clean lookup URLs.

## Docs

- [`backend/README.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/src/graph_rag_tool/backend/README.md)
- [`backend/docs/API_REFERENCE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/src/graph_rag_tool/backend/docs/API_REFERENCE.md)
- `deployment/README.md` for the standalone bundle

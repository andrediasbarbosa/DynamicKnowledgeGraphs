# Graph RAG Tool

Interactive graph QA UI with FastAPI + Kuzu backend.

## Components

- `template.html`: frontend (3D graph, chat, DB Explorer panel)
- `backend/graph_rag_server.py`: backend API + Kuzu integration
- `graph_data.json`: default graph loaded on startup
- `uploaded_graph.json`: latest uploaded graph snapshot

## Quick Start

```bash
cd src/graph_rag_tool/backend
./start.sh
```

Open:

- `http://localhost:8000` for the app
- `http://localhost:8000/docs` for API docs

Windows launcher: `src/graph_rag_tool/backend/start.bat`.

## UI Features

### 1. 3D Graph Viewer

- Loads hypergraph JSON (`nodes + hyperedges`) or pairwise graph JSON (`nodes + links`)
- Degree filter, neighborhood expansion, search, clustering controls
- Highlight integration from chat and DB Explorer

### 2. Graph-RAG Chat

- Uses `/api/query` for natural-language Q&A over Kuzu-backed Cypher results
- Supports OpenAI, Gemini, OpenAI-compatible, and Azure modes via UI config

### 3. DB Explorer Panel

Open with `🗄️ DB Explorer` button (or `Ctrl+K`/`Cmd+K`).

Tabs:

- `Overview`: entity/relationship stats, top entities, source docs
- `Schema`: relation-type browser + sample edges
- `Query`: custom Cypher runner (`/api/cypher`) + JSON/CSV export + history
- `Explorer`: entity search/details and relation navigation
- `Paths`: shortest-path search between entities (`/api/path`)

`Esc` closes open side panels.

## Graph Loading

### 1. UI Upload (recommended)

Use header button `📁 Load Graph JSON`.

Upload flow:

1. Validate JSON
2. Render 3D graph immediately in frontend
3. Upload to backend (`/api/upload`) and initialize Kuzu
4. Refresh schema/overview state

### 2. Reload from path

```bash
curl -X POST http://localhost:8000/api/reload \
  -H "Content-Type: application/json" \
  -d '{"json_path":"/absolute/path/to/graph.json"}'
```

### 3. Upload by API

```bash
curl -X POST http://localhost:8000/api/upload \
  -F "file=@/absolute/path/to/graph.json"
```

## Important Behavior: Auto-Clean on Page Load

The frontend currently calls `POST /api/clean` when the page loads to reset Kuzu tables for a fresh session before upload.

- Good for clean local sessions
- If you want persistent DB state across page refreshes, remove or disable that call in `template.html`

## Expected JSON Shape

```json
{
  "nodes": [
    {"id": "entity_id", "label": "Entity Name"}
  ],
  "hyperedges": [
    {
      "id": "edge_0",
      "sources": ["entity_a"],
      "targets": ["entity_b"],
      "relation": "related_to",
      "confidence": 0.9
    }
  ]
}
```

Notes:

- Backend maps each hyperedge into pairwise `RELATED` edges for Kuzu.
- If present, `source_document`, `source_chunk_id`, `edge_id` are preserved.

## API Summary

See `src/graph_rag_tool/backend/README.md` for endpoint details.

## Troubleshooting

- Port in use: stop existing process on `8000`.
- Empty schema: upload or reload a graph.
- If viewer renders but query endpoints fail: backend not running or DB not initialized.
- Startup from wrong directory can break relative paths; use `backend/start.sh`.

# API Reference - Graph-RAG Kuzu Server

Code-accurate reference for `src/graph_rag_tool/backend/graph_rag_server.py`.

Base URL: `http://localhost:8000`

## Runtime Notes

- Database path is fixed in code: `./graph_rag_db`
- Embedding mode is controlled by `EMBEDDING_MODE`:
  - `local` (default): `sentence-transformers` (`all-MiniLM-L6-v2`, 384-d)
  - `azure`: Azure OpenAI embeddings (`text-embedding-3-small`, 1536-d)

## Endpoints

### GET `/`
- Serves `template.html`

### GET `/api/health`
- Returns service health.
- Example:
```json
{
  "status": "online",
  "service": "Graph RAG Backend",
  "version": "1.0.0"
}
```

### POST `/api/query`
Natural-language query endpoint (3 modes).

Request body:
```json
{
  "question": "What connects X to Y?",
  "llm_config": {
    "provider": "openai",
    "api_key": "...",
    "model": "gpt-4o-mini"
  },
  "conversation_history": [],
  "mode": "graph-rag"
}
```

`mode` values:
- `graph-rag`: LLM -> Cypher -> Kuzu -> LLM answer
- `doc-rag`: vector/document retrieval answer
- `compare`: runs both and returns combined response

Response shape:
```json
{
  "answer": "...",
  "cypher_query": "...",
  "graph_results": [],
  "highlighted_entities": [],
  "stats": {}
}
```

### GET `/api/schema`
Returns live schema summary.

Response shape:
```json
{
  "entity_count": 0,
  "relation_types": ["related_to", "causes"],
  "top_entities": ["entity_a (deg 12)", "entity_b (deg 11)"],
  "documents": ["doc1.pdf", "doc2.pdf"]
}
```

### GET `/api/quality`
Returns graph quality metrics:
- `health_score`
- `issues` (`low_confidence`, `orphaned_entities`, `missing_labels`)
- `percentages`
- `totals`

### POST `/api/cypher`
Execute custom Cypher.

Request:
```json
{
  "query": "MATCH (e:Entity) RETURN e.name LIMIT 10",
  "limit": 100
}
```

Response:
```json
{
  "columns": ["e.name"],
  "rows": [{"e.name": "example"}],
  "row_count": 1
}
```

Notes:
- `limit` max is `1000`

### POST `/api/clean`
Drops `RELATED` and `Entity` tables.

### POST `/api/generate-embeddings`
Generates embeddings for entities missing vectors (Azure mode).

Request:
```json
{
  "llm_config": {
    "azure_key": "...",
    "azure_endpoint": "https://...openai.azure.com",
    "azure_embedding_deployment": "text-embedding-3-small",
    "azure_version": "2024-05-01-preview"
  }
}
```

Behavior:
- In `local` mode: returns `status: "skipped"`
- In `azure` mode: processes entities and returns processed counts

### POST `/api/reload`
Reload graph JSON into Kuzu.

Request:
```json
{
  "json_path": "./path/to/graph.json"
}
```

If `json_path` is omitted, server uses `GRAPH_JSON_PATH` env var or `../graph_data.json`.

### POST `/api/upload`
Upload and load a graph JSON file.

Request: `multipart/form-data` with field `file`.

Response includes:
- `status`, `message`
- `stats` (`entity_count`, `relation_types`, `filename`)
- `details` (`nodes_processed`, `edges_processed`, `timing`, `processing_log`)

### GET `/api/relations`
Returns all relation types and counts.

### GET `/api/relations/{relation_type}/samples`
Returns sample edges for a relation type.

Query params:
- `limit` (default `10`)

### GET `/api/entities`
Entity search.

Query params:
- `q` (default empty)
- `limit` (default `20`)

Response item shape:
```json
{
  "composite_hash": "a1b2c3d4e5f67890",
  "id": "entity_id",
  "label": "Entity Label",
  "name": "entity label",
  "degree": 7,
  "docs": ["doc.pdf"]
}
```

### GET `/api/relations/{composite_hash}`
Get incoming/outgoing relations for a hash identifier.

Query params:
- `limit` (default `50`)

Response shape:
```json
{
  "outgoing": [
    {
      "relation_type": "causes",
      "target": "target name",
      "target_id": "target_id",
      "target_hash": "abcd1234...",
      "confidence": 0.9
    }
  ],
  "incoming": [
    {
      "relation_type": "related_to",
      "source": "source name",
      "source_id": "source_id",
      "source_hash": "efgh5678...",
      "confidence": 0.7
    }
  ]
}
```

### GET `/api/entities/{entity_id:path}`
Get entity details.

Optional query params for composite-exact lookup:
- `degree` (int)
- `docs` (JSON array string, e.g. `[%22Doc1.pdf%22,%22Doc2.pdf%22]`)

Without those params, endpoint returns first match by `id`.

### POST `/api/path`
Find path between two entities.

Request:
```json
{
  "source": "entity_a",
  "target": "entity_b",
  "max_hops": 3
}
```

Response is an array of path summaries with `path`, `hops`, and `note`.

## Data Model (Current)

```cypher
CREATE NODE TABLE Entity (
    composite_key STRING,
    composite_hash STRING,
    id STRING,
    name STRING,
    label STRING,
    degree INT64,
    docs STRING[],
    embedding DOUBLE[],
    PRIMARY KEY (composite_key)
)

CREATE REL TABLE RELATED (
    FROM Entity TO Entity,
    relation_type STRING,
    confidence DOUBLE,
    source_document STRING,
    source_chunk_id STRING,
    edge_id STRING
)
```

## Error Format

Most errors are returned as:
```json
{ "detail": "..." }
```

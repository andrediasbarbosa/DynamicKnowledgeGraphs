# Graph RAG Tool: Kuzu Database Upgrade Proposal

> Status note (2026-02-20, updated 2026-03-09): this is a historical proposal document.
> Implementation has since landed in `src/graph_rag_tool/backend/graph_rag_server.py`.
> Some schema/API snippets below are intentionally historical (for design traceability) and do not match runtime behavior.
> For current behavior, use `README.md`, `src/graph_rag_tool/README.md`, and `src/graph_rag_tool/backend/docs/API_REFERENCE.md`.

## Executive Summary

This document outlines the migration path from the current client-side JavaScript DSL implementation to a Kuzu database-backed architecture using Cypher queries. The upgrade introduces a Python backend for database operations while maintaining the existing 3D visualization frontend.

---

## Current Architecture Analysis

### Technology Stack
- **Frontend**: Single HTML file with embedded JavaScript
- **Graph Storage**: In-memory JavaScript objects loaded from JSON
- **Query Language**: Custom DSL (JSON-based structured queries)
- **LLM Integration**: Direct browser calls to OpenAI/Azure/Gemini APIs
- **Visualization**: 3D Force Graph (WebGL)

### Current Query Pipeline

```
User Question
    ↓
[LLM Call 1] → Generate DSL Query (JSON)
    ↓
{
  "intent": "find_neighbors",
  "entities": ["abd al-rahman iii"],
  "relation_types": ["restored", "asserted"],
  "max_hops": 2,
  "min_confidence": 0.0,
  "direction": "outbound"
}
    ↓
[JavaScript Execution] → Traverse in-memory graph indexes
    ↓
Triples/Paths Result
    ↓
[LLM Call 2] → Generate natural language answer
    ↓
User sees answer + visualization
```

### Current DSL Example

```json
{
  "intent": "find_neighbors",
  "entities": ["'abd al-rahman iii"],
  "relation_types": ["restored"],
  "direction": "outbound",
  "max_hops": 2,
  "min_confidence": 0.7,
  "limit": 25
}
```

### DSL Capabilities
1. **Entity resolution** - Name matching with fuzzy fallback
2. **Graph traversal** - BFS with hop limits
3. **Filtering** - By relation type, confidence, document source
4. **Path finding** - Shortest paths between entities
5. **Aggregation** - Grouping by relation type, entity, or document
6. **Directionality** - Outbound, inbound, or both

---

## Proposed Kuzu-Based Architecture

### Technology Stack
- **Frontend**: Modified HTML with REST API calls
- **Backend**: Python FastAPI server
- **Graph Storage**: Kuzu embedded database (property graph)
- **Query Language**: Cypher queries
- **LLM Integration**: Backend Python SDK calls
- **Visualization**: Same 3D Force Graph

### New Query Pipeline

```
User Question
    ↓
[Frontend] → POST /api/query → [Backend]
    ↓
[LLM Call 1 - Python SDK] → Generate Cypher Query
    ↓
MATCH (e:Entity {name: "'abd al-rahman iii"})-[r:RELATION]->(t:Entity)
WHERE r.relation_type IN ['restored', 'asserted']
  AND r.confidence >= 0.7
RETURN e.name AS source, r.relation_type AS predicate,
       t.name AS target, r.confidence, r.source_document
LIMIT 25
    ↓
[Kuzu Execution] → Query local database
    ↓
Results (JSON)
    ↓
[LLM Call 2] → Generate natural language answer
    ↓
[Backend] → Response → [Frontend displays answer]
```

---

## Kuzu Database Schema

### Node Tables

```cypher
-- Entity nodes
CREATE NODE TABLE Entity (
    id STRING,
    name STRING,
    label STRING,
    degree INT64,
    type STRING DEFAULT 'entity',
    docs STRING[],
    PRIMARY KEY (id)
);

-- Create index for fast lookup
CREATE INDEX entity_name_idx ON Entity(name);
```

### Relationship Tables

```cypher
-- Hyperedge relationships (source entities → relation)
CREATE REL TABLE HAS_SOURCE (
    FROM Entity TO Relation
);

-- Relation → target entities
CREATE REL TABLE HAS_TARGET (
    FROM Relation TO Entity
);

-- Alternative: Direct entity-to-entity edges with properties
CREATE REL TABLE RELATION (
    FROM Entity TO Entity,
    relation_type STRING,
    confidence DOUBLE,
    source_document STRING,
    source_chunk_id STRING,
    edge_id STRING
);
```

### Recommended Approach: Flattened Schema

For better Cypher compatibility and query simplicity:

```cypher
-- Single relationship table with all edge metadata
CREATE NODE TABLE Entity (
    id STRING PRIMARY KEY,
    name STRING,
    label STRING,
    degree INT64,
    docs STRING[]
);

CREATE REL TABLE RELATED (
    FROM Entity TO Entity,
    relation_type STRING,
    confidence DOUBLE,
    source_document STRING,
    source_chunk_id STRING,
    edge_id STRING,
    properties MAP
);

CREATE INDEX rel_type_idx ON RELATED(relation_type);
```

---

## Implementation Components

### 1. Python Backend Server (`graph_rag_backend.py`)

```python
#!/usr/bin/env python3
"""
Graph RAG Backend with Kuzu Database
Provides REST API for Cypher query execution and LLM integration
"""

import os
import json
from pathlib import Path
from typing import List, Dict, Any

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import kuzu
from openai import AzureOpenAI, OpenAI

app = FastAPI(title="Graph RAG Backend")

# CORS for local development
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global database connection
db_path = "./graph_rag_db"
db = None
conn = None


class QueryRequest(BaseModel):
    question: str
    llm_config: Dict[str, str]  # API keys, endpoints, etc.
    conversation_history: List[Dict[str, str]] = []


class QueryResponse(BaseModel):
    answer: str
    cypher_query: str
    graph_results: List[Dict[str, Any]]
    highlighted_entities: List[str]
    stats: Dict[str, Any]


def init_kuzu_db(graph_json_path: str):
    """Initialize Kuzu database from graph JSON file"""
    global db, conn

    # Create/open database
    if Path(db_path).exists():
        print(f"Opening existing database at {db_path}")
    else:
        print(f"Creating new database at {db_path}")
        Path(db_path).mkdir(parents=True, exist_ok=True)

    db = kuzu.Database(db_path)
    conn = kuzu.Connection(db)

    # Load JSON data
    with open(graph_json_path, 'r') as f:
        graph_data = json.load(f)

    # Check if tables exist
    try:
        result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt")
        existing_count = result.get_next()[0]
        print(f"Database already initialized with {existing_count} entities")
        return
    except:
        print("Initializing new database schema...")

    # Create schema
    conn.execute("""
        CREATE NODE TABLE Entity (
            id STRING,
            name STRING,
            label STRING,
            degree INT64,
            docs STRING[],
            PRIMARY KEY (id)
        )
    """)

    conn.execute("""
        CREATE REL TABLE RELATED (
            FROM Entity TO Entity,
            relation_type STRING,
            confidence DOUBLE,
            source_document STRING,
            source_chunk_id STRING,
            edge_id STRING
        )
    """)

    # Build entity mapping
    print(f"Loading {len(graph_data.get('nodes', []))} nodes...")
    entity_map = {}

    for node in graph_data.get('nodes', []):
        if node.get('isAug'):  # Skip augmented nodes
            continue
        if node.get('type') == 'entity':
            entity_id = node.get('id', '')
            entity_name = node.get('label', entity_id).lower()
            entity_map[entity_id] = entity_name

            # Insert entity
            docs_list = node.get('docs', [])
            conn.execute(
                "CREATE (e:Entity {id: $id, name: $name, label: $label, degree: $degree, docs: $docs})",
                {
                    "id": entity_id,
                    "name": entity_name,
                    "label": node.get('label', entity_id),
                    "degree": node.get('degree', 0),
                    "docs": docs_list
                }
            )

    # Load hyperedges
    print(f"Loading {len(graph_data.get('hyperedges', []))} hyperedges...")
    edge_count = 0

    for edge in graph_data.get('hyperedges', []):
        sources = edge.get('sources', [])
        targets = edge.get('targets', [])
        relation = edge.get('relation', 'related')
        confidence = edge.get('confidence', 1.0)
        source_doc = edge.get('source_document', '')
        source_chunk = edge.get('source_chunk_id', '')
        edge_id = edge.get('id', '')

        # Create edges for each source-target pair
        for src in sources:
            src_id = src if src in entity_map else None
            if not src_id:
                continue

            for tgt in targets:
                tgt_id = tgt if tgt in entity_map else None
                if not tgt_id:
                    continue

                try:
                    conn.execute("""
                        MATCH (s:Entity {id: $src}), (t:Entity {id: $tgt})
                        CREATE (s)-[r:RELATED {
                            relation_type: $rel,
                            confidence: $conf,
                            source_document: $doc,
                            source_chunk_id: $chunk,
                            edge_id: $eid
                        }]->(t)
                    """, {
                        "src": src_id,
                        "tgt": tgt_id,
                        "rel": relation,
                        "conf": confidence,
                        "doc": source_doc,
                        "chunk": source_chunk,
                        "eid": edge_id
                    })
                    edge_count += 1
                except Exception as e:
                    print(f"Error creating edge {src_id} -> {tgt_id}: {e}")

    print(f"Database initialized: {len(entity_map)} entities, {edge_count} relationships")


def get_graph_schema() -> Dict[str, Any]:
    """Extract schema information from Kuzu database"""
    global conn

    # Get entity count
    result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt")
    entity_count = result.get_next()[0]

    # Get relationship types
    result = conn.execute("""
        MATCH ()-[r:RELATED]->()
        RETURN DISTINCT r.relation_type AS rel_type, COUNT(*) AS cnt
        ORDER BY cnt DESC
        LIMIT 40
    """)
    relation_types = []
    while result.has_next():
        row = result.get_next()
        relation_types.append(row[0])

    # Get top entities by degree
    result = conn.execute("""
        MATCH (e:Entity)
        RETURN e.name, e.degree
        ORDER BY e.degree DESC
        LIMIT 15
    """)
    top_entities = []
    while result.has_next():
        row = result.get_next()
        top_entities.append(f"{row[0]} (deg {row[1]})")

    # Get documents
    result = conn.execute("""
        MATCH ()-[r:RELATED]->()
        WITH DISTINCT r.source_document AS doc
        WHERE doc <> ''
        RETURN doc
    """)
    documents = []
    while result.has_next():
        row = result.get_next()
        documents.append(row[0])

    return {
        "entity_count": entity_count,
        "relation_types": relation_types,
        "top_entities": top_entities,
        "documents": documents
    }


def build_cypher_planner_prompt(schema: Dict[str, Any]) -> str:
    """Build LLM prompt for generating Cypher queries"""
    return f"""You are a knowledge graph query planner. Convert the user's natural-language question into a Cypher query for a Kuzu property graph database.

GRAPH SCHEMA:
- Nodes: Entity (id, name, label, degree, docs[])
- Relationships: RELATED (relation_type, confidence, source_document, source_chunk_id, edge_id)

LIVE GRAPH STATISTICS:
- Entities: {schema['entity_count']} total
- Relation types ({len(schema['relation_types'])} types - use EXACT spelling):
  {', '.join(schema['relation_types'])}
- Top entities by connectivity (use EXACT name spelling):
  {', '.join(schema['top_entities'])}
- Source documents: {', '.join(schema.get('documents', [])[:10])}

CRITICAL RULES:
1. Entity names in the Cypher query MUST match exactly (case-insensitive) from the top entities list
2. Relation types in WHERE clauses MUST match exactly from the relation types list
3. Use CASE-INSENSITIVE matching: LOWER(e.name) = LOWER('entity name')
4. Always include confidence in results for transparency
5. Limit results to 25-40 to avoid overwhelming the context

CYPHER QUERY PATTERNS:

**Find neighbors (outbound):**
```cypher
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER('entity name')
  AND r.relation_type IN ['type1', 'type2']  // optional filter
  AND r.confidence >= 0.5  // optional threshold
RETURN e.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
LIMIT 25
```

**Find neighbors (inbound - who connects TO this entity):**
```cypher
MATCH (s:Entity)-[r:RELATED]->(e:Entity)
WHERE LOWER(e.name) = LOWER('entity name')
  AND r.confidence >= 0.5
RETURN s.name AS source, r.relation_type AS predicate, e.name AS target,
       r.confidence, r.source_document
LIMIT 25
```

**Find neighbors (bidirectional):**
```cypher
MATCH (e:Entity)-[r:RELATED]-(t:Entity)
WHERE LOWER(e.name) = LOWER('entity name')
RETURN e.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
LIMIT 30
```

**Find paths between entities:**
```cypher
MATCH path = (e1:Entity)-[r:RELATED*1..3]->(e2:Entity)
WHERE LOWER(e1.name) = LOWER('entity1')
  AND LOWER(e2.name) = LOWER('entity2')
RETURN [node IN nodes(path) | node.name] AS path,
       [rel IN relationships(path) | rel.relation_type] AS relations,
       [rel IN relationships(path) | rel.confidence] AS confidences
LIMIT 10
```

**Aggregate by relation type:**
```cypher
MATCH ()-[r:RELATED]->()
RETURN r.relation_type AS relation,
       COUNT(*) AS count,
       AVG(r.confidence) AS avg_confidence,
       COLLECT(DISTINCT r.source_document)[0..3] AS example_docs
ORDER BY count DESC
LIMIT 30
```

**Find by relation type (no specific entity):**
```cypher
MATCH (s:Entity)-[r:RELATED]->(t:Entity)
WHERE r.relation_type IN ['constructed', 'governed_by']
  AND r.confidence >= 0.7
RETURN s.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
ORDER BY r.confidence DESC
LIMIT 25
```

**Multi-hop traversal:**
```cypher
MATCH (e:Entity)-[r1:RELATED]->(t1:Entity)-[r2:RELATED]->(t2:Entity)
WHERE LOWER(e.name) = LOWER('starting entity')
  AND r1.confidence >= 0.6
  AND r2.confidence >= 0.6
RETURN e.name AS start,
       r1.relation_type AS rel1, t1.name AS intermediate,
       r2.relation_type AS rel2, t2.name AS end,
       (r1.confidence + r2.confidence) / 2 AS avg_confidence
LIMIT 20
```

RESPONSE FORMAT:
Return ONLY valid Cypher code with NO markdown, NO preamble, NO explanation.
If you need to explain your reasoning, add it as a // comment on the first line.

Example responses:
```
// Finding what 'abd al-rahman iii restored
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER("'abd al-rahman iii")
  AND r.relation_type = 'restored'
RETURN e.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
LIMIT 25
```
"""


async def generate_cypher_query(question: str, schema: Dict[str, Any], llm_config: Dict[str, str]) -> str:
    """Use LLM to generate Cypher query from natural language question"""

    system_prompt = build_cypher_planner_prompt(schema)

    # Initialize appropriate LLM client
    provider = llm_config.get('provider', 'openai')

    if provider == 'azure':
        client = AzureOpenAI(
            api_key=llm_config['azure_key'],
            api_version=llm_config.get('azure_version', '2024-05-01-preview'),
            azure_endpoint=llm_config['azure_endpoint']
        )
        response = client.chat.completions.create(
            model=llm_config.get('azure_deployment', 'gpt-4'),
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": question}
            ],
            temperature=0.2,
            max_tokens=800
        )
    else:
        client = OpenAI(api_key=llm_config['api_key'])
        response = client.chat.completions.create(
            model=llm_config.get('model', 'gpt-4o-mini'),
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": question}
            ],
            temperature=0.2,
            max_tokens=800
        )

    cypher_query = response.choices[0].message.content.strip()

    # Clean up markdown code blocks if present
    if cypher_query.startswith('```'):
        lines = cypher_query.split('\n')
        cypher_query = '\n'.join(lines[1:-1] if lines[-1].strip() == '```' else lines[1:])

    return cypher_query


def execute_cypher_query(cypher: str) -> List[Dict[str, Any]]:
    """Execute Cypher query against Kuzu database"""
    global conn

    try:
        result = conn.execute(cypher)

        # Convert results to list of dictionaries
        rows = []
        while result.has_next():
            row = result.get_next()
            row_dict = {}
            for i, col_name in enumerate(result.get_column_names()):
                row_dict[col_name] = row[i]
            rows.append(row_dict)

        return rows

    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Cypher execution error: {str(e)}")


def build_context_from_results(
    results: List[Dict[str, Any]],
    cypher_query: str,
    question: str
) -> str:
    """Build rich context string from Cypher query results"""

    ctx = "=== KNOWLEDGE GRAPH CONTEXT ===\n\n"
    ctx += f"CYPHER QUERY EXECUTED:\n{cypher_query}\n\n"
    ctx += f"RESULTS ({len(results)} records):\n\n"

    for i, row in enumerate(results[:40], 1):
        # Format based on available fields
        if 'source' in row and 'predicate' in row and 'target' in row:
            # Triple format
            conf = row.get('confidence', 'N/A')
            doc = row.get('source_document', '')
            ctx += f"{i}. {row['source']} --[{row['predicate']}, conf:{conf}]--> {row['target']}"
            if doc:
                ctx += f" (from: {doc})"
            ctx += "\n"
        else:
            # Generic format
            ctx += f"{i}. " + " | ".join(f"{k}: {v}" for k, v in row.items()) + "\n"

    return ctx


@app.on_event("startup")
async def startup_event():
    """Initialize database on server startup"""
    graph_json = os.getenv("GRAPH_JSON_PATH", "./src/graph_rag_tool/graph_data.json")

    if not Path(graph_json).exists():
        print(f"Warning: Graph JSON not found at {graph_json}")
        print("Database will be empty until JSON is loaded")
    else:
        init_kuzu_db(graph_json)


@app.post("/api/query", response_model=QueryResponse)
async def query_graph(req: QueryRequest):
    """
    Main endpoint: accepts natural language question, generates Cypher,
    executes against Kuzu, and returns LLM-generated answer
    """

    # Get current schema
    schema = get_graph_schema()

    # Step 1: Generate Cypher query
    cypher_query = await generate_cypher_query(req.question, schema, req.llm_config)

    # Step 2: Execute Cypher
    results = execute_cypher_query(cypher_query)

    # Step 3: Build context for answer generation
    context = build_context_from_results(results, cypher_query, req.question)

    # Step 4: Generate natural language answer
    system_prompt = """You are an AI assistant analyzing a knowledge graph from academic documents.

The graph contains entities and relationships extracted from documents, with confidence scores [0-1].

Rules:
1. Reference entities exactly as they appear in the query results
2. Cite relationship types and confidence scores when discussing connections
3. Mention source documents as evidence
4. Distinguish high-confidence (>0.8) from uncertain (<0.5) information
5. At the END, add exactly one line:
   HIGHLIGHTED_ENTITIES: entity1, entity2, relation_type1
   (comma-separated list of entities and relation types mentioned, using exact names)
"""

    provider = req.llm_config.get('provider', 'openai')

    if provider == 'azure':
        client = AzureOpenAI(
            api_key=req.llm_config['azure_key'],
            api_version=req.llm_config.get('azure_version', '2024-05-01-preview'),
            azure_endpoint=req.llm_config['azure_endpoint']
        )
        response = client.chat.completions.create(
            model=req.llm_config.get('azure_deployment', 'gpt-4'),
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": context},
                {"role": "assistant", "content": "I have analyzed the graph context. Please ask your question."},
                *req.conversation_history,
                {"role": "user", "content": req.question}
            ],
            temperature=0.3,
            max_tokens=1200
        )
    else:
        client = OpenAI(api_key=req.llm_config['api_key'])
        response = client.chat.completions.create(
            model=req.llm_config.get('model', 'gpt-4o-mini'),
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": context},
                {"role": "assistant", "content": "I have analyzed the graph context. Please ask your question."},
                *req.conversation_history,
                {"role": "user", "content": req.question}
            ],
            temperature=0.3,
            max_tokens=1200
        )

    answer_text = response.choices[0].message.content

    # Parse highlighted entities
    import re
    match = re.search(r'HIGHLIGHTED_ENTITIES:\s*(.+?)(?:\n|$)', answer_text, re.IGNORECASE)
    highlighted = []
    if match:
        highlighted = [e.strip() for e in match.group(1).split(',')]
        answer_text = re.sub(r'\n?HIGHLIGHTED_ENTITIES:.*$', '', answer_text, flags=re.IGNORECASE).strip()

    return QueryResponse(
        answer=answer_text,
        cypher_query=cypher_query,
        graph_results=results,
        highlighted_entities=highlighted,
        stats={
            "results_count": len(results),
            "entity_count": schema["entity_count"],
            "execution_time_ms": 0  # Could add timing
        }
    )


@app.get("/api/schema")
async def get_schema():
    """Get current graph schema information"""
    return get_graph_schema()


@app.post("/api/reload")
async def reload_graph(json_path: str = None):
    """Reload graph from JSON file"""
    if json_path is None:
        json_path = os.getenv("GRAPH_JSON_PATH", "./src/graph_rag_tool/graph_data.json")

    # Drop existing tables
    try:
        conn.execute("DROP TABLE RELATED")
        conn.execute("DROP TABLE Entity")
    except:
        pass

    init_kuzu_db(json_path)
    return {"status": "success", "message": f"Graph reloaded from {json_path}"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
```

### 2. Frontend Modifications

Replace the DSL execution logic with REST API calls:

```javascript
// NEW: Call backend API instead of local DSL execution
async function executeKuzuQuery(question, llmConfig, conversationHistory) {
    const response = await fetch('http://localhost:8000/api/query', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            question: question,
            llm_config: llmConfig,
            conversation_history: conversationHistory
        })
    });

    if (!response.ok) {
        const error = await response.json();
        throw new Error(error.detail || 'Query failed');
    }

    return await response.json();
}

// MODIFIED: Updated query handler
async function handleQuery() {
    const query = queryInput.value.trim();
    if (!query) return;

    queryInput.value = '';
    sendBtn.disabled = true;
    appendMessage('user', query);

    const thinking = appendThinking();

    try {
        setThinkingStage('Generating Cypher query...');

        // Call backend API
        const llmConfig = getLLMConfig();
        const result = await executeKuzuQuery(query, llmConfig, conversationHistory);

        thinking.remove();

        // Display Cypher query
        appendCypherDebug(result.cypher_query, result.graph_results, result.stats);

        // Display answer
        const msgDiv = appendMessage('assistant', result.answer, result.highlighted_entities);

        // Update conversation history
        conversationHistory.push(
            { role: 'user', content: query },
            { role: 'assistant', content: result.answer }
        );

        if (conversationHistory.length > 12) {
            conversationHistory.splice(0, 2);
        }

        // Apply highlights
        if (result.highlighted_entities.length) {
            applyHighlights(result.highlighted_entities);
        }

        chatHistory.scrollTop = chatHistory.scrollHeight;

    } catch (err) {
        thinking.remove();
        appendMessage('assistant', `Error: ${err.message}`);
    } finally {
        sendBtn.disabled = false;
        queryInput.focus();
    }
}

// NEW: Display Cypher query debug info
function appendCypherDebug(cypherQuery, results, stats) {
    const wrap = document.createElement('div');
    wrap.className = 'dsl-debug';

    let resultText = `Found ${results.length} results\n`;
    resultText += `Total entities in graph: ${stats.entity_count}\n\n`;

    results.slice(0, 25).forEach((row, i) => {
        resultText += `${i + 1}. `;
        if (row.source && row.predicate && row.target) {
            resultText += `${row.source} --[${row.predicate}`;
            if (row.confidence) resultText += `, conf:${row.confidence.toFixed(2)}`;
            resultText += `]--> ${row.target}`;
            if (row.source_document) resultText += ` (${row.source_document})`;
        } else {
            resultText += JSON.stringify(row);
        }
        resultText += '\n';
    });

    wrap.innerHTML = `
        <button class="dsl-toggle open">
            ⚡ Cypher Query & Results
            <span class="dsl-arrow" style="transform:rotate(180deg)">▾</span>
        </button>
        <div class="dsl-body">
            <div class="dsl-section">
                <div class="dsl-section-title">Cypher Query</div>
                <pre class="dsl-pre">${escapeHtml(cypherQuery)}</pre>
            </div>
            <div class="dsl-section">
                <div class="dsl-section-title">Query Results</div>
                <pre class="dsl-pre">${escapeHtml(resultText)}</pre>
            </div>
        </div>
    `;

    chatHistory.appendChild(wrap);

    wrap.querySelector('.dsl-toggle').addEventListener('click', function() {
        const body = wrap.querySelector('.dsl-body');
        const arrow = this.querySelector('.dsl-arrow');
        const collapsed = body.classList.toggle('closed');
        arrow.style.transform = collapsed ? '' : 'rotate(180deg)';
    });
}
```

### 3. Installation & Setup

**requirements.txt:**
```txt
fastapi==0.109.0
uvicorn[standard]==0.27.0
kuzu==0.3.0
openai==1.12.0
pydantic==2.6.0
```

**Installation:**
```bash
# Install Kuzu and dependencies
pip install kuzu fastapi uvicorn openai pydantic

# Or using the requirements file
pip install -r requirements.txt
```

**Running the backend:**
```bash
# Set graph JSON path (optional, defaults to ./src/graph_rag_tool/graph_data.json)
export GRAPH_JSON_PATH=/path/to/graph_data.json

# Start FastAPI server
python graph_rag_backend.py

# Server runs on http://localhost:8000
# API docs available at http://localhost:8000/docs
```

---

## Migration Benefits

### Advantages of Kuzu + Cypher

1. **Standard Query Language**
   - Cypher is industry-standard (Neo4j, Memgraph, etc.)
   - Better LLM understanding (trained on Cypher examples)
   - More expressive than custom DSL

2. **Persistent Storage**
   - Data stored on disk, not in memory
   - Faster startup (no JSON parsing)
   - Scales to larger graphs

3. **Query Optimization**
   - Kuzu's query planner optimizes execution
   - Automatic indexing strategies
   - Better performance for complex traversals

4. **Advanced Graph Operations**
   - Variable-length path matching: `-[*1..5]->`
   - Shortest path algorithms: `shortestPath()`
   - Subgraph pattern matching
   - Graph algorithms (PageRank, centrality)

5. **Better Developer Experience**
   - Interactive query testing with Kuzu shell
   - Standard debugging tools
   - Clear separation of concerns (backend/frontend)

### Trade-offs

**Pros:**
- ✅ Standard, powerful query language
- ✅ Persistent database storage
- ✅ Better scalability
- ✅ LLM-friendly (Cypher is well-known)
- ✅ Advanced graph algorithms

**Cons:**
- ❌ Requires backend server (more complex deployment)
- ❌ Additional dependency (Kuzu database)
- ❌ Can't run purely in browser anymore
- ❌ Network latency for queries

---

## Comparison: DSL vs Cypher

### DSL Query Example
```json
{
  "intent": "find_neighbors",
  "entities": ["'abd al-rahman iii"],
  "relation_types": ["restored", "constructed"],
  "direction": "outbound",
  "max_hops": 2,
  "min_confidence": 0.7,
  "limit": 25
}
```

### Equivalent Cypher Query
```cypher
MATCH (e:Entity)-[r:RELATED*1..2]->(t:Entity)
WHERE LOWER(e.name) = LOWER("'abd al-rahman iii")
  AND ALL(rel IN relationships(r) WHERE rel.relation_type IN ['restored', 'constructed'])
  AND ALL(rel IN relationships(r) WHERE rel.confidence >= 0.7)
RETURN DISTINCT
  e.name AS source,
  [rel IN relationships(r) | rel.relation_type] AS relations,
  t.name AS target,
  [rel IN relationships(r) | rel.confidence] AS confidences
LIMIT 25
```

**Cypher is more expressive:**
- Can specify complex path patterns
- Supports aggregations in traversal
- Better control over result shape
- Native support for paths, shortest paths

---

## Implementation Roadmap

### Phase 1: Backend Setup (Week 1)
- [ ] Install Kuzu and dependencies
- [ ] Create FastAPI server skeleton
- [ ] Implement database initialization from JSON
- [ ] Test basic Cypher query execution

### Phase 2: API Development (Week 1-2)
- [ ] Implement schema extraction endpoint
- [ ] Build Cypher generation prompt
- [ ] Create query execution endpoint
- [ ] Add error handling and validation

### Phase 3: Frontend Integration (Week 2)
- [ ] Replace DSL calls with REST API
- [ ] Update UI to show Cypher queries
- [ ] Modify highlighting logic
- [ ] Test end-to-end flow

### Phase 4: Testing & Optimization (Week 3)
- [ ] Test with real queries
- [ ] Optimize Cypher prompt
- [ ] Add caching for schema
- [ ] Performance profiling

### Phase 5: Documentation & Deployment (Week 3-4)
- [ ] API documentation
- [ ] Deployment guide (Docker)
- [ ] Migration guide for users
- [ ] Example query library

---

## Example Queries

### Question: "What did 'Abd al-Rahman III restore?"

**Generated Cypher:**
```cypher
// Finding entities restored by 'Abd al-Rahman III
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER("'abd al-rahman iii")
  AND r.relation_type = 'restored'
  AND r.confidence >= 0.6
RETURN e.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
ORDER BY r.confidence DESC
LIMIT 25
```

### Question: "Find all construction relationships with high confidence"

**Generated Cypher:**
```cypher
// Finding high-confidence construction relationships
MATCH (s:Entity)-[r:RELATED]->(t:Entity)
WHERE r.relation_type IN ['constructed', 'built', 'established']
  AND r.confidence >= 0.85
RETURN s.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
ORDER BY r.confidence DESC
LIMIT 30
```

### Question: "What connects the Umayyad Emirate to Cordoba?"

**Generated Cypher:**
```cypher
// Finding paths between Umayyad Emirate and Cordoba
MATCH path = (e1:Entity)-[r:RELATED*1..3]->(e2:Entity)
WHERE LOWER(e1.name) = LOWER('umayyad emirate')
  AND LOWER(e2.name) = LOWER('cordoba')
  AND ALL(rel IN relationships(path) WHERE rel.confidence >= 0.5)
RETURN
  [node IN nodes(path) | node.name] AS entities,
  [rel IN relationships(path) | rel.relation_type] AS relations,
  [rel IN relationships(path) | rel.confidence] AS confidences,
  LENGTH(path) AS path_length
ORDER BY path_length ASC
LIMIT 10
```

---

## Deployment Options

### Option 1: Local Development
```bash
# Terminal 1: Start backend
python graph_rag_backend.py

# Terminal 2: Serve frontend
cd src/graph_rag_tool
python -m http.server 8080

# Access at http://localhost:8080
```

### Option 2: Docker Compose
```yaml
version: '3.8'
services:
  backend:
    build: ./backend
    ports:
      - "8000:8000"
    volumes:
      - ./graph_rag_db:/app/graph_rag_db
      - ./src/graph_rag_tool/graph_data.json:/app/graph_data.json
    environment:
      - GRAPH_JSON_PATH=/app/graph_data.json

  frontend:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./src/graph_rag_tool:/usr/share/nginx/html
```

### Option 3: Production Deployment
- Backend: Deploy to cloud (AWS Lambda, Google Cloud Run, Azure Functions)
- Frontend: Static hosting (Netlify, Vercel, GitHub Pages)
- Database: Persistent volume for Kuzu DB files
- Load balancing for multiple users

---

## Conclusion

The migration from custom DSL to Kuzu + Cypher provides:

1. **More powerful querying** with industry-standard Cypher
2. **Better LLM integration** (Cypher is well-understood by models)
3. **Persistent storage** and improved performance
4. **Future extensibility** with graph algorithms

The trade-off is architectural complexity (backend required), but this aligns well with the existing Python tooling in the project and provides a more robust, scalable foundation for the Graph RAG system.

---

## Next Steps

1. Review this proposal and approve architecture
2. Set up development environment with Kuzu
3. Implement Phase 1 (backend skeleton)
4. Test with sample queries
5. Iterate on Cypher generation prompt
6. Full integration and testing

---

**Contact:** For questions about this proposal, refer to project documentation or raise an issue.

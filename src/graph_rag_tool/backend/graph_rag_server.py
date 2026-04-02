#!/usr/bin/env python3
"""
Graph RAG Backend with Kuzu Database
Provides REST API for Cypher query execution and LLM integration
"""

import os
import json
import shutil
from pathlib import Path
from typing import List, Dict, Any, Optional
import re
import hashlib
from urllib.parse import unquote

from fastapi import FastAPI, HTTPException, UploadFile, File, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pydantic import BaseModel
import kuzu
from openai import AzureOpenAI, OpenAI

# Optional import for sentence-transformers (not needed for Azure-only deployment)
try:
    from sentence_transformers import SentenceTransformer
    SENTENCE_TRANSFORMERS_AVAILABLE = True
except ImportError:
    SENTENCE_TRANSFORMERS_AVAILABLE = False
    print("⚠️ sentence-transformers not available - using Azure OpenAI embeddings only")

app = FastAPI(title="Graph RAG Backend", version="1.0.0")


def make_composite_key(degree: int, docs: list, entity_id: str) -> str:
    """
    Create a synthetic composite key from degree, docs, and id
    Format: "degree||sorted_docs||id"
    This allows Kuzu to use a single-column primary key while maintaining composite uniqueness
    """
    docs_str = ",".join(sorted(docs)) if docs else ""
    return f"{degree}||{docs_str}||{entity_id}"


def hash_composite_key(composite_key: str) -> str:
    """
    Generate a short hash from composite key for clean API URLs
    Uses MD5 for speed (not cryptographic security)
    Returns 16-character hex hash
    """
    return hashlib.md5(composite_key.encode('utf-8')).hexdigest()[:16]

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
db: Optional[kuzu.Database] = None
conn: Optional[kuzu.Connection] = None

# Embedding configuration
EMBEDDING_MODE = os.getenv("EMBEDDING_MODE", "local")  # Options: "local", "azure"
embedding_model: Any = None  # SentenceTransformer in local mode, None in Azure mode

# Embedding dimensions
EMBEDDING_DIM_LOCAL = 384   # all-MiniLM-L6-v2
EMBEDDING_DIM_AZURE = 1536  # text-embedding-3-small
EMBEDDING_DIM = EMBEDDING_DIM_LOCAL if EMBEDDING_MODE == "local" else EMBEDDING_DIM_AZURE


class QueryRequest(BaseModel):
    question: str
    llm_config: Dict[str, str]
    conversation_history: List[Dict[str, str]] = []
    mode: str = "graph-rag"  # Options: "graph-rag", "doc-rag", "compare"


class QueryResponse(BaseModel):
    answer: str
    cypher_query: str
    graph_results: List[Dict[str, Any]]
    highlighted_entities: List[str]
    stats: Dict[str, Any]


class ReloadRequest(BaseModel):
    json_path: Optional[str] = None


class CypherRequest(BaseModel):
    query: str
    limit: int = 100


class PathRequest(BaseModel):
    source: str
    target: str
    max_hops: int = 3


def get_embedding(text: str, llm_config: Optional[Dict[str, str]] = None) -> List[float]:
    """
    Generate embedding vector for text
    Supports two modes:
    - LOCAL: sentence-transformers (all-MiniLM-L6-v2, 384 dims)
    - AZURE: Azure OpenAI embeddings (text-embedding-3-small, 1536 dims)
    """
    global embedding_model

    if EMBEDDING_MODE == "azure":
        # Use Azure OpenAI embeddings
        if llm_config is None:
            print("⚠️ Azure mode requires llm_config, returning zero vector")
            return [0.0] * EMBEDDING_DIM

        try:
            client = AzureOpenAI(
                api_key=llm_config.get('azure_key', ''),
                api_version=llm_config.get('azure_version', '2024-05-01-preview'),
                azure_endpoint=llm_config.get('azure_endpoint', '')
            )

            # Azure OpenAI embeddings
            response = client.embeddings.create(
                model=llm_config.get('azure_embedding_deployment', 'text-embedding-3-small'),
                input=text
            )

            embedding = response.data[0].embedding
            return embedding

        except Exception as e:
            print(f"⚠️ Error generating Azure embedding: {e}")
            return [0.0] * EMBEDDING_DIM

    else:
        # LOCAL mode: Use sentence-transformers
        if not SENTENCE_TRANSFORMERS_AVAILABLE:
            print("⚠️ sentence-transformers not available, returning zero vector")
            return [0.0] * EMBEDDING_DIM

        if embedding_model is None:
            print("⚠️ Embedding model not loaded, returning zero vector")
            return [0.0] * EMBEDDING_DIM

        try:
            # Generate embedding
            embedding = embedding_model.encode(text, convert_to_numpy=True)
            return embedding.tolist()
        except Exception as e:
            print(f"⚠️ Error generating embedding: {e}")
            return [0.0] * EMBEDDING_DIM


def init_kuzu_db(graph_json_path: str, force_reload: bool = False):
    """Initialize Kuzu database from graph JSON file"""
    global db, conn

    # Check if database already exists
    db_exists = Path(db_path).exists() and Path(db_path).is_dir()

    # If path exists but is empty or not a valid DB, remove it
    if db_exists:
        try:
            # Try to open existing database
            db = kuzu.Database(db_path)
            conn = kuzu.Connection(db)
            print(f"✓ Opening existing database at {db_path}")
        except Exception as e:
            # Invalid database, remove and recreate
            print(f"→ Removing invalid database directory: {e}")
            shutil.rmtree(db_path)
            db_exists = False

    if not db_exists:
        print(f"✓ Creating new database at {db_path}")
        # Let Kuzu create the directory
        db = kuzu.Database(db_path)
        conn = kuzu.Connection(db)

    # Check if already initialized (skip if force_reload is True)
    if not force_reload:
        try:
            result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt")
            existing_count = result.get_next()[0]
            print(f"✓ Database already initialized with {existing_count} entities")
            return
        except:
            print("→ Initializing new database schema...")
    else:
        print("→ Force reload - reinitializing database schema...")

    # Load JSON data
    if not Path(graph_json_path).exists():
        raise FileNotFoundError(f"Graph JSON not found at {graph_json_path}")

    with open(graph_json_path, 'r', encoding='utf-8') as f:
        graph_data = json.load(f)

    # Create schema
    print("→ Creating Entity table with embedding support...")
    conn.execute("""
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
    """)

    print("→ Creating RELATED relationship table...")
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
    nodes = graph_data.get('nodes', [])
    print(f"→ Processing {len(nodes)} nodes...")

    entity_map = {}  # Maps entity_id -> {'name': str, 'degree': int, 'docs': list}
    entity_count = 0
    duplicate_count = 0

    print("→ Generating embeddings for entities...")
    for i, node in enumerate(nodes):
        if node.get('isAug'):  # Skip augmented nodes
            continue

        # Treat all non-augmented nodes as entities (graph_data.json doesn't have type field)
        node_type = node.get('type', 'entity')
        if node_type == 'entity' or 'type' not in node:
            entity_id = node.get('id', '')
            entity_label = node.get('label', entity_id)
            entity_name = entity_label.lower()
            docs_list = node.get('docs', [])
            degree = node.get('degree', 0)

            # Sort docs array for consistent order-independent comparison
            docs_list_sorted = sorted(docs_list) if docs_list else []

            # Create synthetic composite key and hash
            composite_key = make_composite_key(degree, docs_list_sorted, entity_id)
            composite_hash = hash_composite_key(composite_key)

            # Check if we've already seen this composite key
            if composite_key in entity_map.get('_seen_keys', set()):
                duplicate_count += 1
                if duplicate_count <= 10:  # Show first 10 duplicates
                    print(f"  ⚠️ Skipping duplicate: {entity_id} (degree={degree}, docs count={len(docs_list_sorted)})")
                continue

            # Store complete composite key info for relationship matching
            entity_map[entity_id] = {
                'name': entity_name,
                'degree': degree,
                'docs': docs_list_sorted,
                'id': entity_id,
                'composite_key': composite_key,
                'composite_hash': composite_hash
            }

            # Track seen composite keys to skip duplicates
            if '_seen_keys' not in entity_map:
                entity_map['_seen_keys'] = set()
            entity_map['_seen_keys'].add(composite_key)

            # Generate embedding for entity (LOCAL mode only)
            # In AZURE mode, embeddings are generated later via API call
            embedding = []
            if EMBEDDING_MODE == "local" and SENTENCE_TRANSFORMERS_AVAILABLE and embedding_model:
                entity_text = f"{node.get('label', '')}: {entity_label}"
                embedding = get_embedding(entity_text)

            # Insert entity with embedding (use composite_key as primary key, composite_hash for API)
            try:
                conn.execute(
                    "CREATE (e:Entity {composite_key: $composite_key, composite_hash: $composite_hash, id: $id, name: $name, label: $label, degree: $degree, docs: $docs, embedding: $embedding})",
                    {
                        "composite_key": composite_key,
                        "composite_hash": composite_hash,
                        "id": entity_id,
                        "name": entity_name,
                        "label": entity_label,
                        "degree": degree,
                        "docs": docs_list_sorted,
                        "embedding": embedding
                    }
                )
                entity_count += 1
            except Exception as e:
                # Catch primary key violations and other DB errors
                if "primary key" in str(e).lower() or "duplicate" in str(e).lower():
                    duplicate_count += 1
                    if duplicate_count <= 10:
                        print(f"  ⚠️ Skipping duplicate (DB constraint): {entity_id}")
                else:
                    # Re-raise non-duplicate errors
                    raise

            # Progress indicator
            if (i + 1) % 100 == 0:
                print(f"  Processed {i + 1}/{len(nodes)} entities...")

    print(f"✓ Loaded {entity_count} entities with embeddings")
    if duplicate_count > 0:
        print(f"  ⚠️ Skipped {duplicate_count} duplicate entities")

    # Load hyperedges
    hyperedges = graph_data.get('hyperedges', [])
    print(f"→ Processing {len(hyperedges)} hyperedges...")

    edge_count = 0
    errors = 0

    for i, edge in enumerate(hyperedges):
        if i % 100 == 0 and i > 0:
            print(f"  Processed {i}/{len(hyperedges)} edges...")

        sources = edge.get('sources', [])
        targets = edge.get('targets', [])
        relation = edge.get('relation', 'related')
        confidence = edge.get('confidence', 1.0)
        source_doc = edge.get('source_document', '')
        source_chunk = edge.get('source_chunk_id', '')
        edge_id = edge.get('id', '')

        # Create edges for each source-target pair
        for src in sources:
            # Sources might be entity IDs or names - check both
            src_entity = None
            if src in entity_map:
                src_entity = entity_map[src]
            else:
                # Try finding by lowercase name
                for eid, einfo in entity_map.items():
                    if einfo['name'] == src.lower():
                        src_entity = einfo
                        break

            if not src_entity:
                continue

            for tgt in targets:
                tgt_entity = None
                if tgt in entity_map:
                    tgt_entity = entity_map[tgt]
                else:
                    # Try finding by lowercase name
                    for eid, einfo in entity_map.items():
                        if einfo['name'] == tgt.lower():
                            tgt_entity = einfo
                            break

                if not tgt_entity:
                    continue

                try:
                    # Match using synthetic composite key
                    conn.execute("""
                        MATCH (s:Entity {composite_key: $src_key}),
                              (t:Entity {composite_key: $tgt_key})
                        CREATE (s)-[r:RELATED {
                            relation_type: $rel,
                            confidence: $conf,
                            source_document: $doc,
                            source_chunk_id: $chunk,
                            edge_id: $eid
                        }]->(t)
                    """, {
                        "src_key": src_entity['composite_key'],
                        "tgt_key": tgt_entity['composite_key'],
                        "rel": relation,
                        "conf": confidence,
                        "doc": source_doc,
                        "chunk": source_chunk,
                        "eid": edge_id
                    })
                    edge_count += 1
                except Exception as e:
                    errors += 1
                    if errors < 10:  # Only print first few errors
                        print(f"  Warning: Error creating edge {src_entity['id']} -> {tgt_entity['id']}: {e}")

    print(f"✓ Database initialized: {entity_count} entities, {edge_count} relationships")
    if errors > 0:
        print(f"  ({errors} edges skipped due to errors)")


def get_graph_schema() -> Dict[str, Any]:
    """Extract schema information from Kuzu database"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    # Get entity count
    result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt")
    entity_count = result.get_next()[0]

    # Get relationship types
    result = conn.execute("""
        MATCH ()-[r:RELATED]->()
        RETURN DISTINCT r.relation_type AS rel_type, COUNT(*) AS cnt
        ORDER BY cnt DESC
        LIMIT 50
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
        LIMIT 20
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
        LIMIT 20
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
  {', '.join(schema['relation_types'][:30])}
- Top entities by connectivity (use EXACT name spelling):
  {', '.join(schema['top_entities'])}
- Source documents: {', '.join(schema.get('documents', [])[:8])}

CRITICAL RULES:
1. Entity names must match exactly (case-insensitive) - use LOWER() for comparison
2. Relation types must match exactly from the list above
3. Always return confidence scores for transparency
4. Limit results to 25-40 rows
5. Use property access syntax: e.name, r.relation_type, r.confidence

CYPHER PATTERNS FOR COMMON QUERIES:

**Find neighbors (outbound):**
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER('entity name')
  AND r.confidence >= 0.5
RETURN e.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
LIMIT 25

**Find neighbors (inbound - what connects TO this entity):**
MATCH (s:Entity)-[r:RELATED]->(e:Entity)
WHERE LOWER(e.name) = LOWER('entity name')
RETURN s.name AS source, r.relation_type AS predicate, e.name AS target,
       r.confidence, r.source_document
LIMIT 25

**Find by relation type only:**
MATCH (s:Entity)-[r:RELATED]->(t:Entity)
WHERE r.relation_type = 'specific_relation'
  AND r.confidence >= 0.7
RETURN s.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
ORDER BY r.confidence DESC
LIMIT 30

**Aggregate by relation type:**
MATCH ()-[r:RELATED]->()
RETURN r.relation_type AS relation,
       COUNT(*) AS count,
       AVG(r.confidence) AS avg_confidence
ORDER BY count DESC
LIMIT 30

**Multi-hop (2 hops):**
MATCH (e:Entity)-[r1:RELATED]->(t1:Entity)-[r2:RELATED]->(t2:Entity)
WHERE LOWER(e.name) = LOWER('starting entity')
  AND r1.confidence >= 0.6
RETURN e.name, r1.relation_type, t1.name, r2.relation_type, t2.name,
       (r1.confidence + r2.confidence) / 2.0 AS avg_conf
LIMIT 20

RESPONSE FORMAT:
Return ONLY valid Cypher code. NO markdown blocks, NO explanations.
Add a // comment on line 1 if you want to explain your reasoning.

Example:
// Finding what 'abd al-rahman iii restored
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER("'abd al-rahman iii")
  AND r.relation_type = 'restored'
RETURN e.name AS source, r.relation_type AS predicate, t.name AS target,
       r.confidence, r.source_document
LIMIT 25
"""


async def generate_cypher_query(question: str, schema: Dict[str, Any], llm_config: Dict[str, str]) -> str:
    """Use LLM to generate Cypher query from natural language question"""

    system_prompt = build_cypher_planner_prompt(schema)

    provider = llm_config.get('provider', 'openai')

    try:
        if provider == 'azure':
            client = AzureOpenAI(
                api_key=llm_config.get('azure_key', ''),
                api_version=llm_config.get('azure_version', '2024-05-01-preview'),
                azure_endpoint=llm_config.get('azure_endpoint', '')
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
            client = OpenAI(api_key=llm_config.get('api_key', ''))
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
            if cypher_query.startswith('cypher'):
                cypher_query = cypher_query[6:].strip()

        return cypher_query

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"LLM error: {str(e)}")


def execute_cypher_query(cypher: str) -> List[Dict[str, Any]]:
    """Execute Cypher query against Kuzu database"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        result = conn.execute(cypher)

        # Convert results to list of dictionaries
        rows = []
        column_names = result.get_column_names()

        while result.has_next():
            row = result.get_next()
            row_dict = {}
            for i, col_name in enumerate(column_names):
                value = row[i]
                # Handle different value types
                if isinstance(value, (list, tuple)):
                    row_dict[col_name] = list(value)
                else:
                    row_dict[col_name] = value
            rows.append(row_dict)

        return rows

    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Cypher execution error: {str(e)}")


def build_context_from_results(results: List[Dict[str, Any]], cypher_query: str) -> str:
    """Build rich context string from Cypher query results"""

    ctx = "=== KNOWLEDGE GRAPH CONTEXT ===\n\n"
    ctx += f"CYPHER QUERY EXECUTED:\n{cypher_query}\n\n"
    ctx += f"RESULTS ({len(results)} records):\n\n"

    if len(results) == 0:
        ctx += "⚠️ NO DATA FOUND - The query returned 0 records from the knowledge graph.\n"
        ctx += "This means the requested information is NOT in the loaded graph database.\n"
        ctx += "You MUST acknowledge this in your response.\n"
        return ctx

    for i, row in enumerate(results[:40], 1):
        # Format based on available fields
        if 'source' in row and 'predicate' in row and 'target' in row:
            # Triple format
            conf = row.get('confidence', 'N/A')
            if isinstance(conf, (int, float)):
                conf = f"{conf:.2f}"
            doc = row.get('source_document', '')
            ctx += f"{i}. {row['source']} --[{row['predicate']}, conf:{conf}]--> {row['target']}"
            if doc:
                ctx += f" (from: {doc})"
            ctx += "\n"
        else:
            # Generic format
            ctx += f"{i}. " + " | ".join(f"{k}: {v}" for k, v in row.items()) + "\n"

    return ctx


def extract_keywords(text: str) -> List[str]:
    """Extract keywords from question text for document search"""
    # Remove common question words and stopwords
    stopwords = {'what', 'where', 'when', 'who', 'why', 'how', 'is', 'are', 'was', 'were',
                 'the', 'a', 'an', 'of', 'in', 'on', 'at', 'to', 'for', 'with', 'from',
                 'about', 'by', 'this', 'that', 'these', 'those', 'and', 'or', 'but'}

    # Extract words (alphanumeric + hyphen)
    words = re.findall(r'\b[\w-]+\b', text.lower())

    # Filter stopwords and short words
    keywords = [w for w in words if w not in stopwords and len(w) > 2]

    return keywords[:5]  # Limit to top 5 keywords


def find_documents_by_keywords(keywords: List[str]) -> List[Dict[str, Any]]:
    """Search for entities and their documents matching keywords (no graph traversal)"""
    global conn

    if not conn or not keywords:
        return []

    try:
        # Build case-insensitive search for entity names
        conditions = " OR ".join([f"LOWER(e.name) CONTAINS '{kw.lower()}'" for kw in keywords])

        # Query entities matching keywords, get their documents
        query = f"""
            MATCH (e:Entity)
            WHERE {conditions}
            RETURN e.name AS entity, e.label AS label, e.docs AS documents, e.degree AS degree
            ORDER BY e.degree DESC
            LIMIT 50
        """

        result = conn.execute(query)
        docs = []

        while result.has_next():
            row = result.get_next()
            docs.append({
                'entity': row[0],
                'label': row[1],
                'documents': row[2] if row[2] else [],
                'degree': row[3] if row[3] else 0
            })

        return docs

    except Exception as e:
        print(f"Error searching documents: {e}")
        return []


def find_documents_by_embedding(query: str, llm_config: Optional[Dict[str, str]] = None, top_k: int = 20, min_similarity: float = 0.3) -> List[Dict[str, Any]]:
    """Search for entities using vector similarity (true semantic RAG)"""
    global conn, embedding_model

    if not conn:
        return []

    # Check if we can generate embeddings
    if EMBEDDING_MODE == "local" and (not SENTENCE_TRANSFORMERS_AVAILABLE or embedding_model is None):
        print("="*60)
        print("⚠️ VECTOR-RAG UNAVAILABLE - Using keyword fallback")
        print(f"  Reason: Embedding model not loaded")
        print(f"  SENTENCE_TRANSFORMERS_AVAILABLE: {SENTENCE_TRANSFORMERS_AVAILABLE}")
        print(f"  embedding_model is None: {embedding_model is None}")
        print(f"  Fix: Restart server after installing sentence-transformers")
        print(f"       pip install sentence-transformers")
        print("="*60)
        keywords = extract_keywords(query)
        return find_documents_by_keywords(keywords)

    if EMBEDDING_MODE == "azure" and llm_config is None:
        print("⚠️ Azure mode requires llm_config, falling back to keyword search")
        print("  Generate embeddings first: POST /api/generate-embeddings")
        keywords = extract_keywords(query)
        return find_documents_by_keywords(keywords)

    try:
        # Generate query embedding
        query_embedding = get_embedding(query, llm_config)

        # Kuzu doesn't have built-in cosine similarity function in all versions
        # We'll fetch all entities with embeddings and compute similarity in Python
        # For production, use Kuzu's vector index if available

        query_cypher = """
            MATCH (e:Entity)
            WHERE e.embedding IS NOT NULL
            RETURN e.name AS entity,
                   e.label AS label,
                   e.docs AS documents,
                   e.degree AS degree,
                   e.embedding AS embedding
        """

        result = conn.execute(query_cypher)
        docs = []

        # Compute cosine similarity for each entity
        import numpy as np

        query_vec = np.array(query_embedding)
        query_norm = np.linalg.norm(query_vec)

        while result.has_next():
            row = result.get_next()
            entity_embedding = row[4]

            if entity_embedding is None or len(entity_embedding) == 0:
                continue

            # Compute cosine similarity
            entity_vec = np.array(entity_embedding)
            entity_norm = np.linalg.norm(entity_vec)

            if entity_norm == 0 or query_norm == 0:
                similarity = 0.0
            else:
                similarity = float(np.dot(query_vec, entity_vec) / (query_norm * entity_norm))

            # Filter by minimum similarity
            if similarity >= min_similarity:
                docs.append({
                    'entity': row[0],
                    'label': row[1],
                    'documents': row[2] if row[2] else [],
                    'degree': row[3] if row[3] else 0,
                    'similarity': similarity
                })

        # Sort by similarity (descending) and take top_k
        docs.sort(key=lambda x: x['similarity'], reverse=True)
        return docs[:top_k]

    except Exception as e:
        print(f"Error in vector search: {e}")
        import traceback
        traceback.print_exc()
        # Fallback to keyword search
        keywords = extract_keywords(query)
        return find_documents_by_keywords(keywords)


def build_document_context(docs: List[Dict[str, Any]], keywords: List[str]) -> str:
    """Build context from documents only (no relationships)"""

    ctx = "=== DOCUMENT RAG CONTEXT ===\n\n"
    ctx += f"SEARCH KEYWORDS: {', '.join(keywords)}\n\n"
    ctx += f"MATCHING ENTITIES ({len(docs)} found):\n\n"

    if len(docs) == 0:
        ctx += "⚠️ NO MATCHING ENTITIES - No entities found matching the search keywords.\n"
        ctx += "This means the requested information is NOT in the loaded documents.\n"
        ctx += "You MUST acknowledge this in your response.\n"
        return ctx

    # List entities with their source documents
    for i, doc_info in enumerate(docs[:30], 1):
        entity = doc_info['entity']
        label = doc_info['label']
        docs_list = doc_info['documents']
        degree = doc_info['degree']

        ctx += f"{i}. Entity: {entity} (Type: {label}, Degree: {degree})\n"
        if docs_list:
            ctx += f"   Source Documents: {', '.join(docs_list[:5])}\n"
        ctx += "\n"

    return ctx


def build_vector_rag_context(docs: List[Dict[str, Any]], query: str) -> str:
    """Build context from vector similarity search results"""

    ctx = "=== VECTOR RAG CONTEXT (Semantic Similarity) ===\n\n"
    ctx += f"QUERY: {query}\n\n"
    ctx += f"SIMILAR ENTITIES ({len(docs)} found, ranked by semantic similarity):\n\n"

    if len(docs) == 0:
        ctx += "⚠️ NO SIMILAR ENTITIES - No entities found with sufficient semantic similarity.\n"
        ctx += "This means the requested information is NOT in the loaded documents.\n"
        ctx += "You MUST acknowledge this in your response.\n"
        return ctx

    # List entities with their similarity scores
    for i, doc_info in enumerate(docs[:30], 1):
        entity = doc_info['entity']
        label = doc_info['label']
        docs_list = doc_info['documents']
        degree = doc_info['degree']
        similarity = doc_info.get('similarity', 0.0)

        ctx += f"{i}. Entity: {entity} (Type: {label})\n"
        ctx += f"   Similarity Score: {similarity:.3f} (0.0-1.0, higher = more relevant)\n"
        ctx += f"   Graph Degree: {degree}\n"
        if docs_list:
            ctx += f"   Source Documents: {', '.join(docs_list[:5])}\n"
        ctx += "\n"

    return ctx


async def query_document_rag(req: QueryRequest) -> QueryResponse:
    """Document-only RAG using vector similarity (no graph traversal)"""

    # Use vector similarity search for semantic matching
    docs = find_documents_by_embedding(req.question, req.llm_config, top_k=20, min_similarity=0.3)

    # Build vector-based context
    context = build_vector_rag_context(docs, req.question)

    # Calculate average similarity for stats
    avg_similarity = sum(d.get('similarity', 0.0) for d in docs) / len(docs) if docs else 0.0

    # Generate answer using LLM
    system_prompt = """You are an AI assistant analyzing documents from a knowledge base using semantic similarity.

You have access to entities ranked by how semantically similar they are to the query, but NOT their relationships.

Rules:
1. **CRITICAL**: If no similar entities are found, you MUST start your answer with:
   "⚠️ No semantically similar entities found for this query."
   Then you may optionally provide general context, but make it CLEAR it's not from the documents.

2. **Similarity Scores**: Entities are ranked by similarity (0.0-1.0). Higher scores = more relevant.
   - 0.8-1.0: Highly relevant
   - 0.5-0.8: Moderately relevant
   - 0.3-0.5: Somewhat relevant

3. Reference entities and their similarity scores as evidence
4. Do NOT invent relationships between entities (you don't have that information)
5. Focus on what entities exist, their relevance, and which documents mention them
6. Keep your answer concise and focused (under 500 words)
7. At the END, add exactly one line:
   HIGHLIGHTED_ENTITIES: entity1, entity2, entity3
   (comma-separated list of entities mentioned, using exact names from results ONLY)
"""

    provider = req.llm_config.get('provider', 'openai')

    try:
        if provider == 'azure':
            client = AzureOpenAI(
                api_key=req.llm_config.get('azure_key', ''),
                api_version=req.llm_config.get('azure_version', '2024-05-01-preview'),
                azure_endpoint=req.llm_config.get('azure_endpoint', '')
            )
            response = client.chat.completions.create(
                model=req.llm_config.get('azure_deployment', 'gpt-4'),
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": context},
                    {"role": "assistant", "content": "I have analyzed the document context. Please ask your question."},
                    *req.conversation_history,
                    {"role": "user", "content": req.question}
                ],
                temperature=0.3,
                max_tokens=1200
            )
        else:
            client = OpenAI(api_key=req.llm_config.get('api_key', ''))
            response = client.chat.completions.create(
                model=req.llm_config.get('model', 'gpt-4o-mini'),
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": context},
                    {"role": "assistant", "content": "I have analyzed the document context. Please ask your question."},
                    *req.conversation_history,
                    {"role": "user", "content": req.question}
                ],
                temperature=0.3,
                max_tokens=1200
            )

        answer_text = response.choices[0].message.content

        # Parse highlighted entities
        match = re.search(r'HIGHLIGHTED_ENTITIES:\s*(.+?)(?:\n|$)', answer_text, re.IGNORECASE)
        highlighted = []
        if match:
            highlighted = [e.strip() for e in match.group(1).split(',') if e.strip()]
            answer_text = re.sub(r'\n?HIGHLIGHTED_ENTITIES:.*$', '', answer_text, flags=re.IGNORECASE).strip()

        return QueryResponse(
            answer=answer_text,
            cypher_query=f"VECTOR SEARCH: Top {len(docs)} entities by semantic similarity (avg: {avg_similarity:.3f})",
            graph_results=[],  # No graph traversal results
            highlighted_entities=highlighted,
            stats={
                "results_count": len(docs),
                "entity_count": len(docs),
                "avg_similarity": avg_similarity,
                "mode": "vector-rag"
            }
        )

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"LLM error during answer generation: {str(e)}")


@app.on_event("startup")
async def startup_event():
    """Initialize database and embedding model on server startup"""
    global embedding_model

    print(f"→ Embedding mode: {EMBEDDING_MODE}")
    print(f"→ sentence-transformers available: {SENTENCE_TRANSFORMERS_AVAILABLE}")

    # Load embedding model (LOCAL mode only)
    if EMBEDDING_MODE == "local":
        if SENTENCE_TRANSFORMERS_AVAILABLE:
            print("→ Loading sentence-transformers model (all-MiniLM-L6-v2)...")
            print("  (First run will download ~90MB model from HuggingFace)")
            try:
                from sentence_transformers import SentenceTransformer as STModel
                embedding_model = STModel('all-MiniLM-L6-v2')
                print(f"✓ Embedding model loaded successfully!")
                print(f"  Model type: {type(embedding_model)}")
                print(f"  Embedding dimension: {EMBEDDING_DIM}")
            except Exception as e:
                print(f"❌ FAILED to load embedding model!")
                print(f"  Error: {e}")
                import traceback
                traceback.print_exc()
                print("  Vector-based RAG will NOT be available")
                print("  Falling back to keyword-based search")
        else:
            print("⚠️ sentence-transformers not available")
            print("  Install with: pip install sentence-transformers")
            print("  Vector-based RAG will use keyword fallback")
    else:
        # AZURE mode
        print(f"→ Using Azure OpenAI embeddings (dimension: {EMBEDDING_DIM})")
        print("  Embeddings will be generated via /api/generate-embeddings endpoint")

    # Skip auto-loading graph JSON on startup
    # Users will load graphs via the frontend UI
    print("→ Database initialization...")

    # Initialize empty database structure
    global db, conn
    if db is None:
        try:
            # Try to open existing database
            if Path(db_path).exists():
                print(f"→ Found existing database at {db_path}")
                db = kuzu.Database(db_path)
                conn = kuzu.Connection(db)

                # Verify database is valid by checking for Entity table
                try:
                    result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt")
                    entity_count = result.get_next()[0]
                    print(f"✓ Database opened successfully with {entity_count} entities")
                except Exception as schema_error:
                    # Database exists but schema is invalid or incompatible
                    print(f"⚠️ Existing database has invalid schema: {schema_error}")
                    print(f"→ Removing corrupted database directory")
                    conn = None
                    db = None
                    shutil.rmtree(db_path)
                    # Create fresh database
                    db = kuzu.Database(db_path)
                    conn = kuzu.Connection(db)
                    print(f"✓ Created fresh database at {db_path}")
            else:
                # No existing database, create new one
                db = kuzu.Database(db_path)
                conn = kuzu.Connection(db)
                print(f"✓ Created new database at {db_path}")

            print("  Load graph via frontend: Click '📁 Load Graph JSON' button")

        except Exception as e:
            print(f"❌ CRITICAL: Failed to initialize database!")
            print(f"  Error: {e}")
            print(f"  Solution: Manually delete {db_path} and restart server")
            import traceback
            traceback.print_exc()

    # Final status check
    print("\n" + "="*60)
    print("STARTUP COMPLETE - System Status:")
    print("="*60)
    print(f"Embedding Mode: {EMBEDDING_MODE}")
    print(f"Embedding Model Loaded: {embedding_model is not None}")
    if embedding_model is not None:
        print(f"Model Type: {type(embedding_model).__name__}")
        print("✓ Vector-RAG: AVAILABLE")
    else:
        print("⚠️ Vector-RAG: NOT AVAILABLE (will use keyword fallback)")
    print(f"Database Connected: {conn is not None}")
    print("="*60 + "\n")


@app.get("/")
async def root():
    """Serve the main Graph RAG interface"""
    return FileResponse("template.html")


@app.get("/api/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "online",
        "service": "Graph RAG Backend",
        "version": "1.0.0"
    }


@app.post("/api/query", response_model=QueryResponse)
async def query_graph(req: QueryRequest):
    """
    Main endpoint: accepts natural language question, generates Cypher,
    executes against Kuzu, and returns LLM-generated answer.
    Supports three modes: graph-rag, doc-rag, compare
    """

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    # Route based on mode
    if req.mode == "doc-rag":
        # Document-only RAG (no graph traversal)
        return await query_document_rag(req)

    elif req.mode == "compare":
        # Generate both results and combine
        graph_result = await query_graph_rag(req)
        doc_result = await query_document_rag(req)

        # Combine results
        combined_answer = f"=== GRAPH-RAG ANSWER ===\n\n{graph_result.answer}\n\n"
        combined_answer += f"=== DOCUMENT-RAG ANSWER (keyword-based) ===\n\n{doc_result.answer}"

        return QueryResponse(
            answer=combined_answer,
            cypher_query=f"GRAPH: {graph_result.cypher_query}\n\nDOC: {doc_result.cypher_query}",
            graph_results=graph_result.graph_results,
            highlighted_entities=list(set(graph_result.highlighted_entities + doc_result.highlighted_entities)),
            stats={
                "graph_results": graph_result.stats.get("results_count", 0),
                "doc_results": doc_result.stats.get("results_count", 0),
                "mode": "compare"
            }
        )

    # Default: graph-rag mode
    return await query_graph_rag(req)


async def query_graph_rag(req: QueryRequest) -> QueryResponse:
    """Original graph-based RAG with full path traversal"""

    # Get current schema
    schema = get_graph_schema()

    # Step 1: Generate Cypher query
    cypher_query = await generate_cypher_query(req.question, schema, req.llm_config)

    # Step 2: Execute Cypher
    results = execute_cypher_query(cypher_query)

    # Step 3: Build context for answer generation
    context = build_context_from_results(results, cypher_query)

    # Step 4: Generate natural language answer
    system_prompt = """You are an AI assistant analyzing a knowledge graph from academic documents.

The graph contains entities and relationships extracted from documents, with confidence scores [0-1].

Rules:
1. **CRITICAL**: If the query results show 0 records, you MUST start your answer with:
   "⚠️ No data found in the knowledge graph for this query."
   Then you may optionally provide general context, but make it CLEAR it's not from the graph.

2. Reference entities exactly as they appear in the query results
3. Cite relationship types and confidence scores when discussing connections
4. Mention source documents as evidence
5. Distinguish high-confidence (>0.8) from uncertain (<0.5) information
6. Keep your answer concise and focused (under 500 words)
7. At the END, add exactly one line:
   HIGHLIGHTED_ENTITIES: entity1, entity2, relation_type1
   (comma-separated list of entities and relation types mentioned, using exact names from graph results ONLY)
"""

    provider = req.llm_config.get('provider', 'openai')

    try:
        if provider == 'azure':
            client = AzureOpenAI(
                api_key=req.llm_config.get('azure_key', ''),
                api_version=req.llm_config.get('azure_version', '2024-05-01-preview'),
                azure_endpoint=req.llm_config.get('azure_endpoint', '')
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
            client = OpenAI(api_key=req.llm_config.get('api_key', ''))
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
        match = re.search(r'HIGHLIGHTED_ENTITIES:\s*(.+?)(?:\n|$)', answer_text, re.IGNORECASE)
        highlighted = []
        if match:
            highlighted = [e.strip() for e in match.group(1).split(',') if e.strip()]
            answer_text = re.sub(r'\n?HIGHLIGHTED_ENTITIES:.*$', '', answer_text, flags=re.IGNORECASE).strip()

        return QueryResponse(
            answer=answer_text,
            cypher_query=cypher_query,
            graph_results=results,
            highlighted_entities=highlighted,
            stats={
                "results_count": len(results),
                "entity_count": schema["entity_count"],
                "mode": "graph-rag"
            }
        )

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"LLM error during answer generation: {str(e)}")


@app.get("/api/schema")
async def get_schema():
    """Get current graph schema information"""
    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")
    return get_graph_schema()


@app.get("/api/quality")
async def check_data_quality():
    """Analyze graph data quality and return health metrics"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        # Count total entities
        result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt")
        total_entities = result.get_next()[0]

        # Count total relationships
        result = conn.execute("MATCH ()-[r:RELATED]->() RETURN COUNT(r) AS cnt")
        total_relationships = result.get_next()[0]

        # Check for low confidence relationships (< 0.5)
        result = conn.execute("""
            MATCH ()-[r:RELATED]->()
            WHERE r.confidence < 0.5
            RETURN COUNT(r) AS cnt
        """)
        low_confidence = result.get_next()[0]

        # Check for orphaned entities (degree = 0)
        result = conn.execute("""
            MATCH (e:Entity)
            WHERE e.degree = 0 OR e.degree IS NULL
            RETURN COUNT(e) AS cnt
        """)
        orphaned_entities = result.get_next()[0]

        # Check for entities with missing/empty labels
        result = conn.execute("""
            MATCH (e:Entity)
            WHERE e.label IS NULL OR e.label = ''
            RETURN COUNT(e) AS cnt
        """)
        missing_labels = result.get_next()[0]

        # Calculate percentages
        low_confidence_pct = (low_confidence / total_relationships * 100) if total_relationships > 0 else 0
        orphaned_pct = (orphaned_entities / total_entities * 100) if total_entities > 0 else 0
        missing_labels_pct = (missing_labels / total_entities * 100) if total_entities > 0 else 0

        # Calculate health score (0-100)
        # Penalize: low confidence (30%), orphaned entities (40%), missing labels (30%)
        deductions = (
            (low_confidence_pct * 0.3) +
            (orphaned_pct * 0.4) +
            (missing_labels_pct * 0.3)
        )
        health_score = max(0, min(100, 100 - deductions))

        return {
            "health_score": round(health_score, 1),
            "issues": {
                "low_confidence": low_confidence,
                "orphaned_entities": orphaned_entities,
                "missing_labels": missing_labels
            },
            "percentages": {
                "low_confidence_pct": round(low_confidence_pct, 2),
                "orphaned_pct": round(orphaned_pct, 2),
                "missing_labels_pct": round(missing_labels_pct, 2)
            },
            "totals": {
                "entities": total_entities,
                "relationships": total_relationships
            }
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Quality check failed: {str(e)}")


@app.post("/api/cypher")
async def execute_cypher(req: CypherRequest):
    """Execute custom Cypher query against Kuzu database"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    # Security: limit query complexity
    if req.limit > 1000:
        raise HTTPException(status_code=400, detail="Limit too high (max 1000)")

    try:
        # Execute query
        result = conn.execute(req.query)

        # Extract column names
        columns = [col for col in result.get_column_names()]

        # Extract rows
        rows = []
        row_count = 0
        while result.has_next() and row_count < req.limit:
            row = result.get_next()
            row_dict = {}
            for i, col in enumerate(columns):
                # Handle different data types
                value = row[i]
                # Convert Kuzu types to JSON-serializable types
                if hasattr(value, '__dict__'):
                    # Complex objects - convert to string
                    row_dict[col] = str(value)
                else:
                    row_dict[col] = value
            rows.append(row_dict)
            row_count += 1

        return {
            "columns": columns,
            "rows": rows,
            "row_count": len(rows)
        }
    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Query execution failed: {str(e)}")


@app.post("/api/clean")
async def clean_database():
    """Clean/reset the database by dropping all tables"""
    global db, conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        # Drop existing tables
        conn.execute("DROP TABLE IF EXISTS RELATED")
        conn.execute("DROP TABLE IF EXISTS Entity")
        print("✓ Database cleaned - all tables dropped")
        return {"status": "success", "message": "Database cleaned successfully"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to clean database: {str(e)}")


class EmbeddingRequest(BaseModel):
    llm_config: Dict[str, str]


@app.post("/api/generate-embeddings")
async def generate_embeddings(req: EmbeddingRequest):
    """
    Generate embeddings for all entities (Azure mode)
    Used when embeddings weren't generated during graph load
    """
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    if EMBEDDING_MODE != "azure":
        return {
            "status": "skipped",
            "message": "Embeddings already generated in local mode"
        }

    try:
        # Get all entities without embeddings (include composite key)
        result = conn.execute("""
            MATCH (e:Entity)
            WHERE e.embedding IS NULL OR size(e.embedding) = 0
            RETURN e.composite_key AS composite_key, e.id AS id, e.label AS label, e.name AS name
        """)

        entities_to_process = []
        while result.has_next():
            row = result.get_next()
            entities_to_process.append({
                'composite_key': row[0],
                'id': row[1],
                'label': row[2],
                'name': row[3]
            })

        total = len(entities_to_process)

        if total == 0:
            return {
                "status": "success",
                "message": "All entities already have embeddings",
                "processed": 0,
                "total": 0
            }

        print(f"→ Generating Azure embeddings for {total} entities...")

        # Generate embeddings in batches
        processed = 0
        for i, entity in enumerate(entities_to_process):
            entity_text = f"{entity['label']}: {entity['name']}"
            embedding = get_embedding(entity_text, req.llm_config)

            # Update entity with embedding (match using composite key)
            conn.execute(
                "MATCH (e:Entity {composite_key: $composite_key}) SET e.embedding = $embedding",
                {
                    "composite_key": entity['composite_key'],
                    "embedding": embedding
                }
            )

            processed += 1

            if (i + 1) % 10 == 0:
                print(f"  Generated embeddings for {i + 1}/{total} entities...")

        print(f"✓ Generated embeddings for {processed} entities")

        return {
            "status": "success",
            "message": f"Generated embeddings for {processed} entities",
            "processed": processed,
            "total": total
        }

    except Exception as e:
        import traceback
        traceback.print_exc()
        raise HTTPException(status_code=500, detail=f"Failed to generate embeddings: {str(e)}")


@app.post("/api/reload")
async def reload_graph(req: ReloadRequest):
    """Reload graph from JSON file"""
    global db, conn

    json_path = req.json_path
    if json_path is None:
        json_path = os.getenv("GRAPH_JSON_PATH", "../graph_data.json")

    if not Path(json_path).exists():
        raise HTTPException(status_code=404, detail=f"Graph JSON not found at {json_path}")

    # Drop existing tables
    try:
        if conn:
            conn.execute("DROP TABLE RELATED")
            conn.execute("DROP TABLE Entity")
    except:
        pass

    # Reinitialize (force reload)
    init_kuzu_db(json_path, force_reload=True)
    return {"status": "success", "message": f"Graph reloaded from {json_path}"}


@app.post("/api/upload")
async def upload_graph(file: UploadFile = File(...)):
    """Upload and load a new graph JSON file with detailed progress tracking"""
    global db, conn
    import time

    start_time = time.time()
    processing_log = []

    # Validate file type
    if not file.filename.endswith('.json'):
        raise HTTPException(status_code=400, detail="File must be a JSON file")

    # Save uploaded file
    upload_path = Path("../uploaded_graph.json")
    try:
        # Step 1: Read and validate JSON
        step_start = time.time()
        contents = await file.read()
        file_size_kb = len(contents) / 1024
        processing_log.append(f"Received {file_size_kb:.1f} KB file")

        try:
            graph_data = json.loads(contents)
            node_count = len(graph_data.get('nodes', []))
            edge_count = len(graph_data.get('hyperedges', graph_data.get('links', [])))
            processing_log.append(f"Parsed JSON: {node_count} nodes, {edge_count} edges")
        except json.JSONDecodeError as e:
            raise HTTPException(status_code=400, detail=f"Invalid JSON: {str(e)}")

        validation_time = time.time() - step_start

        # Step 2: Save file
        step_start = time.time()
        with open(upload_path, 'wb') as f:
            f.write(contents)
        processing_log.append(f"Saved to {upload_path}")
        save_time = time.time() - step_start

        # Step 3: Close connection and remove database directory
        step_start = time.time()
        try:
            if conn:
                processing_log.append("Closing database connection...")
                conn = None
            if db:
                db = None

            # Remove entire database directory for clean reload
            if Path(db_path).exists():
                processing_log.append(f"Removing database directory: {db_path}")
                shutil.rmtree(db_path)
                processing_log.append("Database directory removed")
            else:
                processing_log.append("No existing database directory")
        except Exception as e:
            processing_log.append(f"Warning during cleanup: {str(e)}")
        cleanup_time = time.time() - step_start

        # Step 4: Load new graph into Kuzu (force reload with fresh database)
        step_start = time.time()
        processing_log.append("Initializing fresh Kuzu database...")
        try:
            init_kuzu_db(str(upload_path), force_reload=True)
            load_time = time.time() - step_start
            processing_log.append(f"Database initialized in {load_time:.2f}s")
        except Exception as e:
            processing_log.append(f"ERROR during initialization: {str(e)}")
            raise HTTPException(status_code=500, detail=f"Failed to initialize database: {str(e)}")

        # Step 5: Get final stats
        schema = get_graph_schema()
        total_time = time.time() - start_time

        processing_log.append(f"Loaded {schema['entity_count']} entities")
        processing_log.append(f"Created {len(schema['relation_types'])} relation types")

        return {
            "status": "success",
            "message": f"Graph loaded from {file.filename}",
            "stats": {
                "entity_count": schema["entity_count"],
                "relation_types": len(schema["relation_types"]),
                "filename": file.filename
            },
            "details": {
                "nodes_processed": f"{node_count} nodes parsed",
                "edges_processed": f"{edge_count} edges parsed",
                "file_size_kb": round(file_size_kb, 1),
                "processing_log": processing_log,
                "timing": {
                    "validation": round(validation_time, 3),
                    "save": round(save_time, 3),
                    "cleanup": round(cleanup_time, 3),
                    "load_database": round(load_time, 3),
                    "total": round(total_time, 3)
                }
            }
        }

    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error loading graph: {str(e)}")


# ══════════════════════════════════════════════════════════════════════════
# PHASE 2: DATABASE EXPLORER ENDPOINTS
# ══════════════════════════════════════════════════════════════════════════

@app.get("/api/relations")
async def get_relations():
    """Get all relation types with counts"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        result = conn.execute("""
            MATCH ()-[r:RELATED]->()
            RETURN r.relation_type AS type, COUNT(*) AS count
            ORDER BY count DESC
        """)

        relations = []
        while result.has_next():
            row = result.get_next()
            relations.append({
                "type": row[0],
                "count": row[1]
            })

        return relations
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get relations: {str(e)}")


@app.get("/api/relations/{relation_type}/samples")
async def get_relation_samples(relation_type: str, limit: int = 10):
    """Get sample relationships of a specific type"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        result = conn.execute("""
            MATCH (s:Entity)-[r:RELATED]->(t:Entity)
            WHERE r.relation_type = $rel_type
            RETURN s.name AS source, t.name AS target, r.confidence, r.source_document
            ORDER BY r.confidence DESC
            LIMIT {limit}
        """.format(limit=limit), {"rel_type": relation_type})

        samples = []
        while result.has_next():
            row = result.get_next()
            samples.append({
                "source": row[0],
                "target": row[1],
                "confidence": row[2],
                "source_document": row[3] if row[3] else ""
            })

        return samples
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get samples: {str(e)}")


@app.get("/api/entities")
async def search_entities(q: str = "", limit: int = 20):
    """Search entities by name (case-insensitive)"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        # Check if Entity table exists
        try:
            test_result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt LIMIT 1")
            if not test_result.has_next():
                return []  # Database exists but has no entities
        except Exception:
            # Entity table doesn't exist
            return []

        result = conn.execute("""
            MATCH (e:Entity)
            WHERE LOWER(e.name) CONTAINS LOWER($query)
            RETURN e.composite_hash AS hash, e.id AS id, e.label AS label, e.name AS name,
                   e.degree AS degree, e.docs AS docs
            ORDER BY e.degree DESC
            LIMIT {limit}
        """.format(limit=limit), {"query": q})

        entities = []
        while result.has_next():
            row = result.get_next()
            entities.append({
                "composite_hash": row[0],  # For clean API calls
                "id": row[1],
                "label": row[2],
                "name": row[3],
                "degree": row[4] if row[4] else 0,
                "docs": row[5] if row[5] else []
            })

        return entities
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Search failed: {str(e)}")


@app.get("/api/relations/{composite_hash}")
async def get_entity_relations(
    composite_hash: str,
    limit: int = 50
):
    """
    Get all incoming and outgoing relationships for an entity by its composite hash.

    The composite_hash is a 16-character identifier derived from the entity's
    composite primary key (degree, docs, id). This guarantees uniqueness even
    when entity IDs are duplicated across different contexts.

    Example: GET /api/relations/a1b2c3d4e5f6g7h8
    """
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    print(f"🔍 Relations API called for hash: {composite_hash}")

    try:
        # Check if Entity table exists
        try:
            test_result = conn.execute("MATCH (e:Entity) RETURN COUNT(e) AS cnt LIMIT 1")
            if not test_result.has_next():
                return {"outgoing": [], "incoming": []}
        except Exception:
            raise HTTPException(status_code=400, detail="No graph loaded. Please upload a graph JSON file first.")

        # Verify entity exists by composite_hash (guaranteed unique)
        verify_result = conn.execute("""
            MATCH (e:Entity {composite_hash: $hash})
            RETURN e.name, e.id
            LIMIT 1
        """, {"hash": composite_hash})

        if not verify_result.has_next():
            print(f"⚠️ Entity not found with hash: {composite_hash}")
            return {"outgoing": [], "incoming": []}

        entity_row = verify_result.get_next()
        print(f"✓ Found entity: {entity_row[0]} (id: {entity_row[1]})")

        # Get outgoing relationships (include composite_hash for navigation)
        out_result = conn.execute("""
            MATCH (e:Entity {composite_hash: $hash})-[r:RELATED]->(t:Entity)
            RETURN r.relation_type, t.name, t.id, t.composite_hash, r.confidence
            ORDER BY r.confidence DESC
            LIMIT {limit}
        """.format(limit=limit), {"hash": composite_hash})

        outgoing = []
        while out_result.has_next():
            row = out_result.get_next()
            outgoing.append({
                "relation_type": row[0],
                "target": row[1],
                "target_id": row[2],
                "target_hash": row[3],  # For clicking to navigate
                "confidence": row[4] if row[4] else 0.0
            })

        # Get incoming relationships (include composite_hash for navigation)
        in_result = conn.execute("""
            MATCH (s:Entity)-[r:RELATED]->(e:Entity {composite_hash: $hash})
            RETURN r.relation_type, s.name, s.id, s.composite_hash, r.confidence
            ORDER BY r.confidence DESC
            LIMIT {limit}
        """.format(limit=limit), {"hash": composite_hash})

        incoming = []
        while in_result.has_next():
            row = in_result.get_next()
            incoming.append({
                "relation_type": row[0],
                "source": row[1],
                "source_id": row[2],
                "source_hash": row[3],  # For clicking to navigate
                "confidence": row[4] if row[4] else 0.0
            })

        print(f"✅ Found {len(outgoing)} outgoing, {len(incoming)} incoming relations")

        return {
            "outgoing": outgoing,
            "incoming": incoming
        }
    except Exception as e:
        print(f"❌ Error getting relations: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Failed to get relations: {str(e)}")


@app.get("/api/entities/{entity_id:path}")
async def get_entity(
    entity_id: str,
    degree: int = None,
    docs: str = Query(None, description="JSON array of document names, e.g. [\"doc1.pdf\",\"doc2.pdf\"]")
):
    """
    Get detailed information about a specific entity

    With composite primary key (degree, docs, id), you can optionally provide:
    - degree: entity degree (integer)
    - docs: JSON-encoded array of document names (properly handles special characters)
      Example: ?docs=["Document, Part 1.pdf","Report (2024).pdf"]

    If not provided, returns first matching entity by ID

    Note: entity_id supports special characters (spaces, slashes, etc.) - they will be URL-decoded
    """
    global conn

    # URL decode entity_id to handle special characters
    entity_id = unquote(entity_id)

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        # If composite key fields provided, do exact match
        if degree is not None and docs is not None:
            # Parse docs as JSON array (handles special characters properly)
            try:
                docs_list = json.loads(docs) if isinstance(docs, str) else docs
                if not isinstance(docs_list, list):
                    raise ValueError("docs must be a JSON array")
            except (json.JSONDecodeError, ValueError) as e:
                raise HTTPException(
                    status_code=400,
                    detail=f"Invalid docs parameter. Must be JSON array like [\"doc1.pdf\",\"doc2.pdf\"]. Error: {str(e)}"
                )

            # Sort arrays and create composite key
            docs_list_sorted = sorted(docs_list)
            composite_key = make_composite_key(degree, docs_list_sorted, entity_id)

            result = conn.execute("""
                MATCH (e:Entity {composite_key: $composite_key})
                RETURN e.id, e.label, e.name, e.degree, e.docs, e.composite_hash
            """, {"composite_key": composite_key})
        else:
            # Fallback: match by ID only, return first match
            result = conn.execute("""
                MATCH (e:Entity {id: $id})
                RETURN e.id, e.label, e.name, e.degree, e.docs, e.composite_hash
                LIMIT 1
            """, {"id": entity_id})

        if not result.has_next():
            raise HTTPException(status_code=404, detail="Entity not found")

        row = result.get_next()
        return {
            "id": row[0],
            "label": row[1],
            "name": row[2],
            "degree": row[3] if row[3] else 0,
            "docs": row[4] if row[4] else [],
            "composite_hash": row[5]  # Include hash for relations API
        }
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to get entity: {str(e)}")


@app.post("/api/path")
async def find_path(req: PathRequest):
    """Find shortest path between two entities"""
    global conn

    if not conn:
        raise HTTPException(status_code=500, detail="Database not initialized")

    try:
        # Find paths using simple pattern matching (Kuzu 0.7.1 compatible)
        # Note: This is a simplified version that works with current Kuzu
        result = conn.execute("""
            MATCH (e1:Entity)-[r:RELATED*1..{max_hops}]-(e2:Entity)
            WHERE LOWER(e1.name) = LOWER($source)
              AND LOWER(e2.name) = LOWER($target)
            RETURN e1.name AS source, e2.name AS target, LENGTH(r) AS hops
            LIMIT 5
        """.format(max_hops=req.max_hops), {
            "source": req.source,
            "target": req.target
        })

        paths = []
        while result.has_next():
            row = result.get_next()
            source_name = row[0]
            target_name = row[1]
            hop_count = row[2] if row[2] is not None else 1

            # Simple path representation without intermediate nodes
            # (Kuzu 0.7.1 doesn't support nodes()/relationships() functions)
            path_steps = [
                {"entity": source_name, "relation": "connected_via"},
                {"entity": target_name, "relation": None}
            ]

            paths.append({
                "path": path_steps,
                "hops": hop_count,
                "note": f"{hop_count}-hop path found (intermediate nodes not shown in Kuzu 0.7.1)"
            })

        return paths
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Path search failed: {str(e)}")


if __name__ == "__main__":
    import uvicorn
    print("=" * 60)
    print("Graph RAG Backend Server with Kuzu Database")
    print("=" * 60)
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")

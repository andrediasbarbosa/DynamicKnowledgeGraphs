# Graph-RAG Tool - Features & Capabilities

Complete guide to all features, RAG modes, and user interface components.

---

## Table of Contents

1. [Core Capabilities](#core-capabilities)
2. [RAG Modes](#rag-modes)
3. [Kuzu Database Explorer](#kuzu-database-explorer)
4. [3D Graph Visualization](#3d-graph-visualization)
5. [Chat Interface](#chat-interface)
6. [Pipeline Components](#pipeline-components)

---

## Core Capabilities

### Knowledge Graph Construction

**From Documents to Graph:**
1. **PDF Processing** → Extract text from research papers
2. **Entity Extraction** → Identify entities using LLM
3. **Relationship Discovery** → Find connections between entities
4. **Graph Building** → Store in Kuzu graph database
5. **Embedding Generation** → Create vector representations

### Query & Retrieval

**Three RAG Modes:**
- **Graph-RAG**: Cypher query generation + graph traversal
- **Document-RAG**: Vector similarity search (no graph structure)
- **Compare Both**: Side-by-side comparison of both methods

### Visualization

- **3D Force-Directed Graph**: Interactive WebGL rendering
- **Entity Clustering**: Automatic community detection
- **Relationship Highlighting**: Click entities to see connections
- **Camera Controls**: Zoom, pan, rotate

---

## RAG Modes

### 1. Graph-RAG (Structured Retrieval)

**How It Works:**
1. User asks natural language question
2. LLM generates Cypher query
3. Query executes on Kuzu database
4. Graph traversal finds related entities/relationships
5. LLM generates answer from structured results

**Best For:**
- "How is X related to Y?"
- "Find all connections between A and B"
- "What entities are connected through Z?"
- Multi-hop relationship queries

**Visual Indicator:**
- Cyan-bordered panel: "📊 Graph-RAG: Cypher Query & Graph Traversal"
- Shows generated Cypher query
- Displays entity relationships: `Subject → [Predicate] → Object`

**Example:**
```cypher
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER('Damascus')
RETURN e.name, r.relation_type, t.name, r.confidence
ORDER BY r.confidence DESC
```

### 2. Document-RAG (Semantic Search)

**How It Works:**
1. User asks natural language question
2. Question converted to embedding vector
3. Cosine similarity search against entity embeddings
4. Top-k most similar entities retrieved
5. LLM generates answer from ranked entities

**Best For:**
- "Find entities similar to X"
- "What mentions Y?"
- Keyword-based discovery
- Exploratory queries without known relationships

**Visual Indicator:**
- Orange-bordered panel: "📄 Document-RAG: Vector Similarity Search"
- Shows similarity scores (0.0-1.0)
- Star ratings: ⭐⭐⭐ (high), ⭐⭐ (medium), ⭐ (low)
- States: "NO graph traversal" and "NO relationship information"

**Example Results:**
```
1. Damascus ⭐⭐⭐ (0.892) [from: paper1.pdf]
2. Syria ⭐⭐⭐ (0.834) [from: paper2.pdf]
3. Mosque ⭐⭐ (0.653) [from: paper1.pdf]
```

### 3. Compare Both

**How It Works:**
- Executes both Graph-RAG and Document-RAG simultaneously
- Returns side-by-side answers
- Helps users understand which approach works better

**Visual Layout:**
```
┌────────────────────┬────────────────────┐
│ 📊 Graph-RAG       │ 📄 Document-RAG    │
│                    │                    │
│ (Answer using      │ (Answer using      │
│  graph structure)  │  similarity only)  │
└────────────────────┴────────────────────┘
```

**Best For:**
- Evaluating retrieval quality
- Understanding method differences
- Complex queries where approach matters

---

## Kuzu Database Explorer

### Overview Tab

**Statistics Display:**
- Total entities in graph
- Number of relationship types
- Top entities by degree (most connected)
- Source documents list

**Data Quality Dashboard:**
- Health score (0-100)
- Issue detection:
  - Low confidence relationships (< 0.5)
  - Orphaned entities (degree = 0)
  - Missing labels
- Percentage breakdowns
- Visual progress bar

**Usage:**
```
Click "🔍 Check Data Quality" button
→ See health score and detected issues
→ Use provided queries to investigate problems
```

### Schema Tab

**Features:**
- Lists all relation types with counts
- Expandable cards showing sample relationships
- Filter input for relation types
- Sample display: `Source → Target [confidence]`

**Usage:**
```
Browse relationship types
→ Click to expand and see samples
→ Filter by name to find specific types
```

### Query Tab

**Cypher Query Editor:**
- Syntax-highlighted textarea
- 20+ pre-built query templates
- Execute button
- Clear button

**Query Templates (Organized by Category):**

**📍 Entity Discovery (5 templates):**
1. Find entity neighbors
2. Entities by degree (most connected)
3. Entities by document
4. High confidence entities
5. Entities by label type

**🔗 Relationship Analysis (6 templates):**
1. Find path between entities
2. Browse by relation type
3. Bidirectional relationships
4. 2-hop connections
5. 3-hop connections
6. Relationships by confidence range

**📊 Aggregations (5 templates):**
1. Aggregate relation stats
2. Count by relation type
3. Entities per document
4. Average confidence by type
5. Degree distribution

**🔧 Maintenance & Quality (5 templates):**
1. Find orphaned entities
2. Low confidence relationships
3. Entities without labels
4. Find duplicate relationships
5. Find self-loop relationships

**Results Display:**
- Table format with sortable columns
- Row count indicator
- Export buttons (CSV, JSON)

**Query History:**
- Automatic saving of last 10 queries
- Timestamp for each query
- One-click reload
- Clear history option

**Export Functionality:**
- **CSV Export**: Download as comma-separated values
- **JSON Export**: Download as structured JSON
- Filename includes timestamp
- Handles special characters and null values

**Usage:**
```
Select template from dropdown
→ Edit query as needed
→ Click "▶️ Execute Query"
→ View results in table
→ Export as CSV or JSON
```

### Explorer Tab

**Entity Search:**
- Search by entity name (partial match)
- Results sorted by degree
- Shows: name, label, degree, document count
- Click entity to see details

**Entity Details:**
- Entity ID, name, label
- Degree (connection count)
- Source documents list
- "Highlight in Graph" button → highlights entity in 3D view
- "Explore Relations" button → loads relationship table

**Relationship Explorer:**
- **Outgoing**: Entities this entity points to
- **Incoming**: Entities pointing to this entity
- Table shows: direction, relation type, connected entity, confidence

**Clear Button:**
- Resets search input
- Clears all results
- Hides details panel

**Usage:**
```
Enter entity name → Click "🔍 Search"
→ Click result to see details
→ Click "Highlight in Graph" to visualize
→ Click "Clear" to reset
```

### Paths Tab

**Path Finder:**
- Source entity input
- Target entity input
- Max hops slider (1-5, default 3)
- "Find Shortest Path" button

**Path Display:**
- Visual chain: `Entity1 → [relation] → Entity2 → [relation] → Entity3`
- Shows hop count
- Multiple paths if found (limit 5)
- Auto-highlights all entities in path on 3D graph
- "No path found" message if none exists

**Usage:**
```
Enter source and target entities
→ Adjust max hops if needed
→ Click "🔍 Find Shortest Path"
→ View path chain
→ Entities auto-highlighted in 3D graph
```

### Documents Tab

**Document Explorer:**
- Dropdown list of all source documents
- Statistics per document:
  - Entity count
  - Max degree entity
  - Top entity name
- Entity list from selected document
- Filter entities by name
- Click entity to highlight in graph

**Usage:**
```
Click "📁 Load Documents" button
→ Select document from dropdown
→ View statistics
→ Browse entity list
→ Click entity to highlight in 3D graph
→ Use filter to search within document
```

---

## 3D Graph Visualization

### Rendering

**Technology:**
- ForceGraph3D with WebGL
- Real-time physics simulation
- Force-directed layout
- Automatic clustering

**Node Representation:**
- Size: Proportional to degree (connections)
- Color: By cluster/community
- Label: Entity name (hover to see)

**Edge Representation:**
- Lines between connected entities
- Color: Indicates relationship type
- Thickness: Based on confidence score

### Controls

**Mouse:**
- **Left-click + drag**: Rotate view
- **Right-click + drag**: Pan view
- **Scroll wheel**: Zoom in/out
- **Click node**: Select and show details

**Buttons:**
- **Reset Camera**: Return to default view
- **Re-cluster**: Recalculate communities
- **Expand 1-hop**: Show immediate neighbors
- **Expand 2-hop**: Show 2-hop neighborhood
- **Reset View**: Clear selections

**Search:**
- Search by entity name
- Auto-zoom to selected entity
- Highlight entity and connections

### Highlighting

**Auto-highlight When:**
- Entity clicked in Explorer tab
- Path found in Paths tab
- Query result in Chat
- Document entity clicked

**Highlight Colors:**
- Selected entity: Gold
- Connected entities: Orange
- Relationships: Orange lines

---

## Chat Interface

### Natural Language Queries

**How to Use:**
1. Select RAG mode (Graph-RAG, Document-RAG, or Compare)
2. Type natural language question
3. Press Enter or click Send
4. View answer with methodology explanation

**Answer Components:**

**For Graph-RAG:**
- Methodology header: "📊 Graph-RAG"
- Cypher query panel (collapsible)
- Graph traversal results
- LLM-generated answer
- Highlighted entities (clickable)

**For Document-RAG:**
- Methodology header: "📄 Document-RAG"
- Vector similarity panel (collapsible)
- Ranked entities with scores
- LLM-generated answer
- Highlighted entities (clickable)

**For Compare:**
- Two-column layout
- Graph-RAG answer on left
- Document-RAG answer on right
- Comparison insights

### Settings Panel

**LLM Configuration:**
- Provider: OpenAI, Azure OpenAI
- API Key
- Model selection
- Base URL (for custom endpoints)

**Embedding Configuration:**
- Mode: Local or Azure
- Dimension: 384 (local) or 1536 (Azure)

**Graph Settings:**
- Auto-load graph on startup (disabled by default)
- Default highlight color
- Camera settings

---

## Pipeline Components

### 1. Document Processing

**Input:** PDF research papers

**Steps:**
1. Text extraction
2. Section identification
3. Metadata extraction (title, authors, date)

**Output:** Clean text chunks

### 2. Entity Extraction

**Input:** Text chunks

**LLM Prompt:**
```
Extract named entities from the following text.
Return: {entities: [{name, label, context}]}
```

**Entity Types:**
- PERSON, ORGANIZATION, LOCATION
- CONCEPT, EVENT, DATE
- Custom domain-specific types

**Output:** Entity list with labels

### 3. Relationship Extraction

**Input:** Text chunks + entities

**LLM Prompt:**
```
Identify relationships between entities.
Return: {relationships: [{source, target, type, confidence}]}
```

**Relationship Extraction:**
- Co-occurrence analysis
- Syntactic patterns
- Semantic understanding
- Confidence scoring (0.0-1.0)

**Output:** Triple list (subject, predicate, object)

### 4. Graph Construction

**Input:** Entities + relationships

**Kuzu Database Schema:**
```cypher
CREATE NODE TABLE Entity (
  id STRING PRIMARY KEY,
  name STRING,
  label STRING,
  degree INT64,
  docs STRING[],
  embedding DOUBLE[]
);

CREATE REL TABLE RELATED (
  FROM Entity TO Entity,
  relation_type STRING,
  confidence DOUBLE,
  source_document STRING
);
```

**Deduplication:**
- Entity matching by name (case-insensitive)
- Relationship deduplication
- Confidence aggregation

**Output:** Populated graph database

### 5. Embedding Generation

**Local Mode (all-MiniLM-L6-v2):**
- Dimension: 384
- Model size: 90MB
- Speed: ~1000 entities/second
- Offline capable

**Azure Mode (text-embedding-3-small):**
- Dimension: 1536
- Cost: $0.0001 per 1K tokens
- Requires API access
- Higher quality

**Storage:**
- Embeddings stored in Entity table
- Used for vector similarity search
- Updated when entities added/modified

### 6. Query Processing

**Graph-RAG Pipeline:**
```
Question → LLM (text-to-Cypher) → Kuzu DB → Results → LLM (answer) → User
```

**Document-RAG Pipeline:**
```
Question → Embedding → Similarity Search → Top-k Entities → LLM (answer) → User
```

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Enter` | Send chat message / Execute query |
| `Esc` | Close panels |
| `Ctrl+K` | Focus entity search (Kuzu Explorer) |

---

## Export Formats

### CSV Export

**Format:**
```csv
"Column1","Column2","Column3"
"Value1","Value2","Value3"
```

**Features:**
- Proper quote escaping
- Handles null values (empty string)
- UTF-8 encoding
- Excel-compatible

### JSON Export

**Format:**
```json
[
  {
    "column1": "value1",
    "column2": "value2"
  }
]
```

**Features:**
- Pretty-printed (2-space indent)
- Preserves data types
- Null values included
- Valid JSON structure

---

## Performance Considerations

### Graph Size Limits

- **Tested up to:** 10,000 entities, 50,000 relationships
- **3D rendering:** Optimized for up to 5,000 visible nodes
- **Query response:** < 1 second for most queries
- **Embedding generation:** ~1-5 minutes per 1,000 entities

### Caching

- **Query history:** localStorage (last 50 queries)
- **Document list:** Cached after first load (5 minutes)
- **Quality metrics:** Cached (5 minutes)
- **Embeddings:** Stored in database (persistent)

### Optimization Tips

1. **Use filters** in Kuzu Explorer to limit result sets
2. **Limit exports** to necessary data
3. **Clear history** periodically
4. **Reset database** if corrupted
5. **Use clustering** to organize large graphs

---

## Next Steps

- **Deployment:** See [DEPLOYMENT.md](DEPLOYMENT.md)
- **Troubleshooting:** See [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Changes:** See [CHANGELOG.md](CHANGELOG.md)

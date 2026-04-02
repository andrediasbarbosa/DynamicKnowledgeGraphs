# User Guide - Graph-RAG Kuzu Explorer

Complete guide to using all features of the Graph-RAG Kuzu Explorer.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [3D Visualization](#3d-visualization)
3. [Database Explorer](#database-explorer)
4. [Graph-RAG Chat](#graph-rag-chat)
5. [Tips & Tricks](#tips--tricks)

---

## Getting Started

### Loading Your First Graph

1. **Start the server**: `./start.sh` (or `start.bat` on Windows)
2. **Open browser**: Navigate to http://localhost:8000
3. **Load graph**: Click "📁 Load Graph JSON" button (top-right)
4. **Select file**: Choose your graph JSON file
5. **Wait**: Progress shows: Parse → Render → Upload → Database
6. **Success**: See "✅ Graph Loaded Successfully" overlay

**First Time?** Try `examples/sample_graph.json` to explore features.

---

## 3D Visualization

The main area shows your knowledge graph in interactive 3D.

### Navigation

| Action | How |
|--------|-----|
| **Rotate** | Left-click + drag |
| **Zoom** | Scroll wheel |
| **Pan** | Right-click + drag |
| **Focus Node** | Click node |

### Left Panel Controls

#### Search & Navigate

**Search Box**:
- Type entity name
- Press "Go" to find and center
- Entity highlights in gold

**Pin Node**:
- Click node in graph
- Click "📍 Pin" button
- Node becomes pinned (won't move)

**Expand**:
- With node pinned, click "+ 1 hop" or "+ 2 hops"
- Shows connected entities
- Limited by "Max nodes" setting

**Reset**:
- "Reset": Clear pinned nodes and return to full graph
- "Reset camera": Center view on graph

#### Visualization Settings

**Auto-freeze physics**:
- ☑ Checked: Graph stops moving when stable
- ☐ Unchecked: Continuous physics simulation

**Directional arrows**:
- ☑ Checked: Show arrow heads on relationships
- ☐ Unchecked: Simple lines (better performance)

**Max nodes**:
- Slider: 200 - 8000
- Limits number of nodes rendered
- Lower = better performance

**Depth spread**:
- Slider: 0 - 600
- Visual separation between graph layers
- Higher = more 3D depth

### Node Colors

Nodes are colored by category/label:
- Each unique label gets a distinct color
- Color persists across sessions
- Helps identify entity types visually

### Tooltips

Hover over nodes/edges to see:
- **Node**: Entity name
- **Edge**: Relationship type and confidence

---

## Database Explorer

Press **Ctrl+K** (or **Cmd+K** on Mac) to open the Database Explorer panel.

### Overview Tab

#### Statistics Cards

**Total Entities**: Count of all nodes in database
**Relationship Types**: Number of unique relationship types

#### Top Entities

List of most connected entities (highest degree):
- Shows entity name and connection count
- Sorted by degree (descending)
- Click to highlight in 3D graph

#### Source Documents

List of all source documents:
- Shows where graph data originated
- Helps understand data provenance

---

### Schema Tab

Explore your graph's structure.

#### Relation Type Cards

Each card shows:
- **Relation type**: e.g., "authored", "located in"
- **Count**: Number of relationships of this type
- **Expand**: Click to see sample relationships

#### Sample Relationships

When expanded, shows table with:
- **Source**: Starting entity
- **Target**: Ending entity
- **Confidence**: Relationship confidence score
- **Document**: Source document

#### Filtering

Use filter input to search relation types:
- Type to filter (e.g., "auth" finds "authored", "co-authored")
- Case-insensitive
- Click "Clear" to reset

---

### Query Tab

Write and execute Cypher queries.

#### Query Templates

Dropdown with 4 pre-built queries:
1. **Find entity neighbors**: All connected entities
2. **Find path between entities**: Shortest path finder
3. **Browse by relation type**: Filter by relationship
4. **Aggregate statistics**: Count and avg confidence

**To use**:
- Select template from dropdown
- Edit placeholders (e.g., `'entity name here'`)
- Click "Execute"

#### Query Editor

Text area for writing Cypher queries:
- Monospace font for readability
- Syntax highlighting (basic)
- Multiple lines supported

**Common Query Patterns**:

```cypher
-- Find all connections for an entity
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE LOWER(e.name) = LOWER('spain')
RETURN e.name, r.relation_type, t.name, r.confidence
ORDER BY r.confidence DESC
LIMIT 25

-- Find entities by relation type
MATCH (s:Entity)-[r:RELATED]->(t:Entity)
WHERE r.relation_type = 'authored'
RETURN s.name, t.name, r.confidence
LIMIT 30

-- Count relationships by type
MATCH ()-[r:RELATED]->()
RETURN r.relation_type AS type, COUNT(*) AS count
ORDER BY count DESC

-- Find high-confidence relationships
MATCH (s:Entity)-[r:RELATED]->(t:Entity)
WHERE r.confidence >= 0.9
RETURN s.name, r.relation_type, t.name, r.confidence
ORDER BY r.confidence DESC
LIMIT 50
```

#### Query Results

After execution, results shown in table:
- **Columns**: Based on RETURN clause
- **Rows**: Query results (limited by LIMIT clause)
- **Count**: Total rows returned

**Export Results**:
- **📥 JSON**: Download as JSON file
- **📥 CSV**: Download as CSV file

#### Query History

Shows last 10 queries:
- Click to reload query into editor
- Persists until page reload or new graph loaded
- Useful for repeating/modifying queries

---

### Explorer Tab

Search and explore individual entities.

#### Entity Search

**Search Input**:
- Type entity name (partial match OK)
- Case-insensitive
- Searches across all entities

**Search Results**:
- Shows matching entities
- Sorted by degree (most connected first)
- Shows entity name and connection count

#### Entity Details

Click entity in search results to view:
- **Entity Name**: Full name
- **Degree**: Number of connections
- **Source Documents**: Where entity appears

**Actions**:
- **✨ Highlight in Graph**: Highlights entity in gold in 3D view
- **🔍 Explore Relations**: Load all relationships

#### Relationship Explorer

After clicking "🔍 Explore Relations":

**Outgoing Relationships**:
- Relationships where this entity is the source
- Format: Entity → [relation] → Target

**Incoming Relationships**:
- Relationships where this entity is the target
- Format: Source → [relation] → Entity

**Click Related Entity**:
- Loads details for that entity
- Allows navigation through graph

---

### Paths Tab

Find shortest paths between two entities.

#### Path Inputs

**Source Entity**:
- Type starting entity name
- Exact match (case-insensitive)

**Target Entity**:
- Type ending entity name
- Exact match (case-insensitive)

**Max Hops**:
- Slider: 1-5
- Maximum path length to search
- Lower = faster, higher = finds longer paths

#### Finding Paths

1. Enter source and target entity names
2. Adjust max hops if needed
3. Click "🔍 Find Shortest Path"
4. Wait for results (1-3 seconds)

**Shortcut**: Press **Enter** in either input to execute search

#### Path Results

Shows up to 5 shortest paths:

**Path Format**:
```
Entity A → [relationship] → Entity B → [relationship] → Entity C
```

**Path Details**:
- Shows all intermediate entities
- Shows relationship types
- Shows hop count
- Sorted by hop count (shortest first)

**Visual Highlight**:
- All entities in path highlighted in 3D graph
- Helps visualize connections

---

## Graph-RAG Chat

Click "💬 Graph-RAG Chat" to open AI-powered chat interface.

### Asking Questions

**Natural Language**:
- Ask questions in plain English
- No need to write Cypher
- AI generates queries automatically

**Example Questions**:
- "What is the relationship between Spain and Al-Andalus?"
- "Show me all entities connected to machine learning"
- "What are the most important entities in the graph?"
- "Find all authors in the database"

### Chat Features

**Context-Aware**:
- AI uses graph data to answer
- Shows Cypher query used
- Provides citations from graph

**Conversation History**:
- Maintains chat history
- Can reference previous questions
- Clear history to start fresh

**Limitations**:
- Requires API key for AI service
- May not understand very complex questions
- Best for factual queries about graph data

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| **Ctrl+K** (Cmd+K) | Toggle Database Explorer |
| **Esc** | Close panels |
| **Enter** | Execute (in query/path inputs) |

---

## Tips & Tricks

### Performance Optimization

**Large Graphs** (10,000+ entities):
1. Lower "Max nodes" slider to 1000-1500
2. Enable "Auto-freeze physics"
3. Don't expand more than 2 hops at once
4. Use search to focus on subsets
5. Add LIMIT to all queries

**Slow Queries**:
1. Always use LIMIT clause
2. Filter early with WHERE
3. Reduce max hops in path finding
4. Use indexed fields (e.g., e.name)

### Better Visualization

**Clearer View**:
1. Disable "Directional arrows" for simpler view
2. Adjust "Depth spread" to separate layers
3. Use "Reset camera" to re-center
4. Pin important nodes to keep them stable

**Finding Connections**:
1. Search for entity in left panel
2. Click "📍 Pin"
3. Click "+ 1 hop" to see neighbors
4. Repeat for connected entities

### Effective Querying

**Start Simple**:
```cypher
-- Begin with basic query
MATCH (e:Entity)
RETURN e.name
LIMIT 10

-- Add filtering
MATCH (e:Entity)
WHERE e.degree > 5
RETURN e.name, e.degree
ORDER BY e.degree DESC
LIMIT 10

-- Add relationships
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE e.degree > 5
RETURN e.name, r.relation_type, t.name
LIMIT 25
```

**Use Templates**:
- Start with template from dropdown
- Modify for your specific needs
- Save useful queries in external file

**Export Results**:
- Export to JSON for further analysis
- Export to CSV for spreadsheets
- Use query history to revisit queries

### Graph Exploration Workflow

**1. Overview**:
- Open DB Explorer (Ctrl+K)
- Check Overview tab for statistics
- Note top entities

**2. Schema**:
- Review Schema tab
- See what relationship types exist
- Check sample relationships

**3. Deep Dive**:
- Use Explorer tab to search specific entities
- Use Paths tab to find connections
- Use Query tab for custom analysis

**4. Iterate**:
- Follow interesting connections
- Export findings
- Refine queries

---

## Data Management

### Loading New Graph

**Fresh Start**:
1. Click "📁 Load Graph JSON"
2. Select new file
3. Database automatically clears
4. DB Explorer resets
5. 3D visualization updates

**Everything Resets**:
- Query history cleared
- Search results cleared
- Path results cleared
- Statistics updated

### Multiple Sessions

**Each page load**:
- Database automatically cleaned
- Must upload graph again
- Ensures clean state

**Why?**:
- Prevents data mixing
- Guarantees consistency
- Simplifies workflow

---

## Best Practices

### Graph JSON Preparation

1. **Validate** JSON before upload
2. **Include** all required fields
3. **Use** consistent naming
4. **Optimize** large graphs (remove unnecessary fields)
5. **Document** data sources in docs array

### Exploration Strategy

1. **Start** with Overview to understand scale
2. **Review** Schema to see relationships
3. **Search** for known entities
4. **Explore** connections from there
5. **Query** for patterns and insights

### Query Writing

1. **Always** use LIMIT
2. **Filter** early with WHERE
3. **Order** results meaningfully
4. **Test** with small limits first
5. **Export** results for offline analysis

---

## Common Workflows

### Find Related Entities

1. Open DB Explorer (Ctrl+K)
2. Go to Explorer tab
3. Search for entity
4. Click "🔍 Explore Relations"
5. Click related entities to navigate

### Analyze Relationship Type

1. Go to Schema tab
2. Find relationship type
3. Click to expand samples
4. Go to Query tab
5. Use "Browse by relation type" template
6. Execute to see all instances

### Find Connection Path

1. Go to Paths tab
2. Enter source entity
3. Enter target entity
4. Adjust max hops (try 3 first)
5. Click "🔍 Find Shortest Path"
6. View highlighted path in 3D

### Custom Analysis

1. Go to Query tab
2. Select appropriate template
3. Modify for your needs
4. Execute query
5. Export results (JSON or CSV)
6. Analyze in external tools

---

## Advanced Features

### Cypher Query Capabilities

**Pattern Matching**:
```cypher
-- Multiple hops
MATCH (a)-[*2..3]-(b)

-- Optional patterns
OPTIONAL MATCH (e)-[r]-(t)

-- Multiple patterns
MATCH (a)-[r1]->(b), (b)-[r2]->(c)
```

**Aggregations**:
```cypher
-- Count, average, sum
RETURN COUNT(*), AVG(r.confidence), SUM(e.degree)

-- Group by
RETURN r.relation_type, COUNT(*) AS count
```

**Filtering**:
```cypher
-- Multiple conditions
WHERE e.degree > 5 AND r.confidence >= 0.8

-- Pattern in string
WHERE LOWER(e.name) CONTAINS LOWER('spain')

-- In list
WHERE r.relation_type IN ['authored', 'wrote']
```

### Graph Navigation Patterns

**Hub Discovery**:
```cypher
-- Find most connected entities
MATCH (e:Entity)
RETURN e.name, e.degree
ORDER BY e.degree DESC
LIMIT 20
```

**Bridge Finding**:
```cypher
-- Find entities connecting two groups
MATCH (a:Entity)-[:RELATED]-(bridge:Entity)-[:RELATED]-(b:Entity)
WHERE LOWER(a.name) CONTAINS 'spain'
  AND LOWER(b.name) CONTAINS 'islam'
RETURN DISTINCT bridge.name, bridge.degree
ORDER BY bridge.degree DESC
```

**Document Tracing**:
```cypher
-- Find all entities from a document
MATCH (e:Entity)
WHERE 'document.pdf' IN e.docs
RETURN e.name, e.degree
ORDER BY e.degree DESC
```

---

## Summary

**Essential Features**:
- 📊 **3D Visualization**: Interactive graph exploration
- 🗄️ **DB Explorer**: 5 tabs for comprehensive analysis
- 💬 **Graph-RAG Chat**: AI-powered Q&A
- ⌨️ **Keyboard Shortcuts**: Fast navigation

**Best Used For**:
- Knowledge graph exploration
- Relationship discovery
- Pattern analysis
- Data validation
- Research and investigation

**Remember**:
- Press **Ctrl+K** for DB Explorer
- Use **Query Templates** for common tasks
- **Export results** for offline analysis
- **Refresh page** for clean start

---

**Happy Exploring!** 🚀

For troubleshooting, see `docs/TROUBLESHOOTING.md`
For API details, see `docs/API_REFERENCE.md`

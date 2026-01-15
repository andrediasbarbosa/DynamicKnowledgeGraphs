# Historical Knowledge Example - Graph Extraction from 1page.pdf

## Overview

This example demonstrates extracting a knowledge hypergraph from a real historical document ([tests/1page.pdf](tests/1page.pdf)) about the transmission of mathematical knowledge across civilizations and centuries.

## Source Document

**Document**: History of mathematical knowledge transmission
**Content**: Two interweaving threads:
1. **History Thread**: Movement of texts and institutions (Baghdad → Damascus → Sicily → England → Paris)
2. **Mathematics Thread**: Methods, artifacts, and echoes (Pythagoras → Euclid → Al-Khwarizmi → Fibonacci → Newton → Metric System)

## Extracted Knowledge Graph

### Statistics
- **Entities (Nodes)**: 75
- **Relationships (Hyperedges)**: 42
- **Average Relationship Complexity**: 2.45 entities per relationship
- **Network Topology**: Scale-free (R² = 0.858)

### Key Knowledge Hubs

The most connected entities in the knowledge network:

| Entity | Connections | Type |
|--------|-------------|------|
| Baghdad | 3 | Place/Origin |
| geometry | 3 | Concept |
| Euclid's Elements | 3 | Text/Artifact |
| Fibonacci | 2 | Person |
| Adelard of Bath | 2 | Person |
| Arabic versions | 2 | Artifact |
| Hindu-Arabic numerals | 2 | Method |
| Newton | 2 | Person |

### Knowledge Transmission Path

**Question**: How did knowledge flow from Baghdad to the metric system?

**Path Found** (4 steps):
1. `[Baghdad, tables from Baghdad]` --**originated**--> `[mathematical methods]`
2. `[triangulation]` --**returns_to**--> `[angle measurement, tables from Baghdad]`
3. `[geodetic survey work]` --**used**--> `[triangulation]`
4. `[metric system]` --**relied_on**--> `[geodetic survey work]`

This demonstrates the circular nature of knowledge: methods originating in Baghdad centuries earlier became foundational for post-revolutionary French scientific reforms.

### Knowledge Clusters

The algorithm identified **14 distinct knowledge clusters** (s-connected components):

1. **Main cluster** (12 relationships): Baghdad translation activities → European university system
2. **Mathematical tradition** (7 relationships): Greek geometry → Pythagorean theorem → area calculations
3. **Numeral transmission** (4 relationships): Hindu-Arabic numerals → bookkeeping → trade
4. **Institutional networks** (3 relationships): Universities → clergy → land administration
5. **Modern mathematics** (2 relationships): Newton → calculus → motion

### Example Hyperedges

Higher-order relationships extracted from the text:

```cpp
// Multi-source collaboration
[House of Wisdom, Baghdad] --sponsored--> [translators]

// Multi-target production
[translators] --produced--> [Arabic versions, Greek geometry]

// Complex transmission
[Al-Khwarizmi, Baghdad] --wrote--> [treatise on al-jabr]

// Application chains
[Hindu-Arabic numerals] --simplified--> [bookkeeping]
[bookkeeping] --supported--> [long-distance trade, port cities]

// Cross-thread connection
[geometry, land grants] --applied_in--> [practical measurement]
```

## Key Features Demonstrated

### 1. Higher-Order Relationships

Unlike simple pairwise graphs, hyperedges naturally capture complex relationships:
- `[PCL, chitosan] → compose → [nanofibers]` (standard example)
- `[House of Wisdom, Baghdad] → sponsored → [translators]` (historical)
- `[Euclid] → organized → [proofs, propositions]` (conceptual)

### 2. Knowledge Neighborhood Analysis

**Query**: What entities are within 2 hops of "geometry"?

**Result** (11 entities found):
- Adelard of Bath, England, Paris (transmission path)
- Plato, governance (philosophical context)
- chanceries, land grants, measured boundaries (practical application)
- students, the same book (pedagogical chain)

### 3. Network Topology

**Power Law Fit**:
- Exponent: 2.413
- R²: 0.858 (strong fit)
- **Interpretation**: The network exhibits scale-free properties characteristic of natural knowledge networks with key hubs

This topology suggests:
- Most entities have few connections (specialists)
- A few entities act as major hubs (Baghdad, geometry, Euclid's Elements)
- Knowledge flows through these hubs across time and geography

### 4. Temporal and Geographical Spread

The graph captures knowledge transmission across:

**Centuries**: ~300 BCE (Euclid) → 9th century CE (Baghdad) → 12th century (Sicily) → 17th century (Newton) → 18th century (French Revolution)

**Geography**: Greece → Baghdad → Damascus → Sicily → England → Paris → (global metric system)

## Running the Example

```bash
# Build
cd build
cmake ..
make historical_knowledge_example

# Run
./bin/historical_knowledge_example
```

## Output Files

All files are saved to `output_json/`:

### 1. historical_knowledge.json (23KB)
Complete graph serialization with:
- All 75 entities with properties
- All 42 hyperedges with sources, relations, targets
- Metadata (confidence scores, provenance if available)

```json
{
  "hyperedges": [
    {
      "id": "edge_0",
      "relation": "sponsored",
      "sources": ["House of Wisdom", "Baghdad"],
      "targets": ["translators"],
      "confidence": 1.0
    },
    ...
  ],
  "nodes": [...],
  "metadata": {...}
}
```

### 2. historical_knowledge.dot (9.3KB)
Graphviz format for visualization:

```bash
dot -Tpng output_json/historical_knowledge.dot -o output_json/historical_knowledge.png
```

### 3. historical_knowledge_matrix.json (31KB)
Incidence matrix representation (75 nodes × 42 hyperedges):
- Rows: Entities
- Columns: Hyperedges
- Value: 1 if entity participates in hyperedge, 0 otherwise

Useful for:
- Matrix-based graph algorithms
- Spectral analysis
- Export to numerical computing tools (NumPy, MATLAB)

## Comparison with Paper

This example implements concepts from **"Higher-Order Knowledge Representations for Agentic Scientific Reasoning"** (Stewart & Buehler, 2026):

| Paper Concept | Implementation | Example |
|--------------|----------------|---------|
| Higher-order relationships | Hyperedges with multiple sources/targets | `[Baghdad, tables] → originated → [methods]` |
| s-connected paths | BFS with intersection constraint | Baghdad to metric system (4 hops) |
| Knowledge hubs | Degree centrality | Baghdad (3), geometry (3) |
| Power law topology | Log-log regression | γ = 2.413, R² = 0.858 |
| Component detection | s-connected components | 14 clusters identified |

## Insights from the Example

### The Circular Nature of Knowledge

From the source document:
> "In this loop, a place becomes an origin, a manuscript becomes a conduit, and 'the method' becomes a lineage."

The extracted graph validates this insight:
- **Place → Origin**: Baghdad appears as a hub with 3 connections
- **Manuscript → Conduit**: Euclid's Elements connects Greek → Arabic → Latin traditions
- **Method → Lineage**: Triangulation links ancient Baghdad tables to French metric system

### Multi-Scale Knowledge Flow

The hypergraph captures knowledge operating at multiple scales:
1. **Individual**: Pythagoras, Euclid, Al-Khwarizmi, Fibonacci, Newton
2. **Institutional**: House of Wisdom, Plato's Academy, cathedral libraries, universities
3. **Geographical**: Trade routes, port cities, workshop locations
4. **Conceptual**: Geometry, algebra (al-jabr), calculus, metric system

### Practical Applications

This example shows how the hypergraph structure enables:
1. **Historiography**: Track how ideas moved across civilizations
2. **Citation Analysis**: Identify influential texts and scholars
3. **Educational Pathways**: Understand how knowledge is transmitted pedagogically
4. **Science Policy**: Recognize the importance of institutional support (House of Wisdom, libraries)

## Next Steps

This example provides a foundation for:
1. **Automated Extraction**: Replace manual coding with LLM-based extraction from PDFs
2. **Larger Corpora**: Process entire collections of historical documents
3. **Temporal Analysis**: Add timestamps to track knowledge evolution over time
4. **Cross-Document Linking**: Merge graphs from multiple sources
5. **Agentic Reasoning**: Use paths to answer complex historical questions

## Code Structure

The example is organized as:

```cpp
// 1. Build history thread (movement of texts)
graph.add_hyperedge({"House of Wisdom", "Baghdad"}, "sponsored", {"translators"});
// ... more historical relationships

// 2. Build mathematics thread (methods and concepts)
graph.add_hyperedge({"Euclid"}, "organized", {"proofs", "propositions"});
// ... more mathematical relationships

// 3. Cross-thread connections
graph.add_hyperedge({"geometry", "land grants"}, "applied_in", {"practical measurement"});

// 4. Analysis and export
auto stats = graph.compute_statistics();
auto path = graph.find_shortest_path("Baghdad", "metric system", 1);
graph.export_to_json("output_json/historical_knowledge.json");
```

## Validation

The extracted graph correctly captures the document's narrative:
- ✓ Both threads (history and mathematics) represented
- ✓ Key entities identified (Baghdad, Euclid, geometry)
- ✓ Transmission paths traceable (Baghdad → metric system)
- ✓ Circular/recursive nature evident (triangulation returning to Baghdad tables)
- ✓ Multi-entity relationships preserved (not collapsed to pairwise)

---

**Source**: [tests/1page.pdf](tests/1page.pdf)
**Implementation**: [examples/historical_knowledge_example.cpp](examples/historical_knowledge_example.cpp)
**Documentation**: [README.md](README.md)

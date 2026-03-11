# Knowledge Discovery Categories

## Overview

The knowledge discovery system classifies insights into **three high-level categories** based on how they generate new knowledge from the graph structure. This categorization provides a richer framework for understanding the epistemological nature of each discovery.

## The Three Categories

### 🧩 Combinatorial Insights
**Pattern Detection & Structural Combinations**

These insights emerge from **combining existing knowledge in new ways**, identifying patterns, structures, and recurring motifs that reveal the underlying organization of the knowledge graph.

**Key Characteristics:**
- Detect patterns and regularities
- Identify structural properties
- Find recurring substructures
- Reveal organizational principles

**Insight Types in this Category:**
- **Motif**: Recurring subgraph patterns
- **K-Core**: Tightly connected cores
- **K-Truss**: Triangle-reinforced edges
- **Community Detection**: Dense clusters
- **Core-Periphery**: Hub-periphery structure
- **Meta-Path Pattern**: Relation sequence templates
- **Rule**: Logical rules and constraints
- **Taxonomy**: Hierarchical relationships
- **Compositional Reasoning**: Composed relations via relation algebra
- **Transitive Closure**: Transitive relationship completion
- **Mechanism Consolidation**: Clustered mechanistic hypotheses
- **Meta-Path**: Specific multi-hop paths following relation patterns
- **Centrality**: Structural importance rankings

**Example:**
> "The motif (A)-[method]->(B)-[outcome]->(C) appears 15 times across different research domains, suggesting a common experimental pattern."

---

### 🔍 Exploratory Insights
**Path Finding & Connection Discovery**

These insights **discover new connections, paths, and bridges** between concepts, exploring previously unexplored areas of the knowledge space and revealing hidden relationships.

**Key Characteristics:**
- Find new connections
- Discover bridges between domains
- Explore unexplored paths
- Predict missing links

**Insight Types in this Category:**
- **Bridge**: Entities connecting separate regions
- **Completion**: Missing relation prediction
- **Long Chain**: Extended reasoning paths
- **Path Rank**: Important traversal paths
- **Community Link**: Cross-cluster connections
- **Embedding Link**: Embedding-based link prediction (TransE, RotatE, ComplEx)
- **Hyperedge Prediction**: Multi-entity relationship prediction
- **Domain Bridge**: Cross-domain connectors
- **Explanatory Chain**: Multi-hop explanation paths
- **Author Chain**: Author collaboration/citation chains
- **Causal Chain**: Directed causal pathways
- **Cross-Community Mechanism Bridge**: Mechanistic chains spanning domains
- **Diffusion**: Influence propagation pathways
- **Relation Induction**: New relation type discovery
- **Evidence Fusion Link**: Multi-operator consensus links
- **Text Similarity**: Semantic similarity links
- **Argument Support**: Evidence-grounded relations
- **Method Outcome**: Method-outcome relationships

**Example:**
> "Bridge entity 'epigenetics' connects neuroscience cluster with developmental biology cluster through 5 intermediate concepts."

---

### 🔄 Transformational Insights
**Reframing & Perspective Shifts**

These insights **transform understanding** by reframing concepts, generating hypotheses, identifying contradictions, and suggesting alternative causal mechanisms or paradigms.

**Key Characteristics:**
- Reframe understanding
- Generate new hypotheses
- Identify anomalies and surprises
- Suggest alternative explanations
- Question assumptions

**Insight Types in this Category:**
- **Substitution**: Interchangeable concepts
- **Contradiction**: Conflicting claims
- **Entity Resolution**: Duplicate/alias detection
- **Analogical Transfer**: Analogy-based suggestions
- **Counterfactual**: Falsification questions
- **Surprise**: Statistically unusual connections
- **Hypotheses 1**: Testable hypotheses from findings
- **Hypotheses 2**: Graph-native mechanistic hypotheses
- **Hypotheses 3**: Counterfactual causal hypotheses
- **Meta-Path Anomaly**: Rare but plausible relation sequences
- **Intervention Bottleneck**: Critical pathway bottlenecks
- **Competing Mechanism**: Alternative causal mechanisms
- **Schema Repair**: Schema completion suggestions
- **Intervention Point**: Causal chain disconnection points
- **Feedback Loop**: Cyclical causal relationships
- **Confounder**: Confounding variable detection
- **Logical Entailment**: Inferred implications
- **Schema Violation**: Pattern violations
- **Claim Stance**: Stance classification (support/oppose/neutral)
- **Uncertainty Sampling**: Low-confidence verification targets
- **Active Learning**: High-value verification queries

**Example:**
> "Counterfactual hypothesis: If 'neural plasticity' is removed from the causal chain, does 'learning enhancement' still hold?"

---

## Category Distribution

When running discovery, the report includes a **Category Overview** section showing:

- Count of insights per category
- Average confidence score per category
- Brief description of what each category represents

Example output:
```
## Knowledge Discovery Categories

### 🧩 Combinatorial Insights (45)
Pattern Detection & Structural Combinations
- Average Score: 0.782

### 🔍 Exploratory Insights (63)
Path Finding & Connection Discovery
- Average Score: 0.691

### 🔄 Transformational Insights (28)
Reframing & Perspective Shifts
- Average Score: 0.845
```

---

## Implementation Details

### Automatic Classification

Categories are **automatically assigned** when insights are created:

```cpp
Insight insight;
insight.set_type(InsightType::BRIDGE);  // Automatically sets category = EXPLORATORY
```

### Category Mapping Function

The mapping is defined in `include/discovery/insight.hpp`:

```cpp
inline InsightCategory get_insight_category(InsightType type) {
    switch (type) {
        case InsightType::MOTIF:
        case InsightType::K_CORE:
        // ... more combinatorial types
            return InsightCategory::COMBINATORIAL;

        case InsightType::BRIDGE:
        case InsightType::COMPLETION:
        // ... more exploratory types
            return InsightCategory::EXPLORATORY;

        case InsightType::CONTRADICTION:
        case InsightType::HYPOTHESES_1:
        // ... more transformational types
            return InsightCategory::TRANSFORMATIONAL;
    }
}
```

### JSON Output

Insights include category in their JSON representation:

```json
{
  "insight_id": "run_001:bridge:000187",
  "type": "bridge",
  "category": "exploratory",
  "seed_nodes": ["neural_plasticity", "learning"],
  "score": 0.85,
  ...
}
```

The summary includes category distribution:

```json
{
  "summary_by_category": {
    "combinatorial": 45,
    "exploratory": 63,
    "transformational": 28
  }
}
```

---

## Use Cases

### Research Analysis
Use category distribution to understand the **nature of discoveries**:
- High combinatorial → Strong structural patterns
- High exploratory → Many hidden connections
- High transformational → Paradigm shifts needed

### Report Organization
Reports can be organized by category to group similar epistemological approaches together.

### Insight Filtering
Filter insights by category for specific research goals:
- Need new connections? → Focus on exploratory
- Looking for patterns? → Focus on combinatorial
- Want alternative explanations? → Focus on transformational

---

## Future Enhancements

Potential extensions:
1. **Category-specific scoring**: Different scoring criteria per category
2. **Category balance analysis**: Detect over/under-representation
3. **Cross-category synthesis**: Combine insights across categories
4. **Interactive filtering**: UI controls to show/hide categories
5. **Category-based recommendations**: Suggest which operators to run based on gaps

---

## References

- `include/discovery/insight.hpp` - Category definitions and mapping
- `src/discovery/report_generator.cpp` - Category overview generation
- `src/discovery/discovery_engine.cpp` - Automatic category assignment

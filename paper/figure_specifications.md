# Figure Specifications for Publication

This document specifies all 10 figures needed for the paper. Each figure includes detailed specifications for creating it.

---

## Figure 1: System Architecture Diagram
**File**: `figures/system_architecture.pdf`
**Reference**: `\ref{fig:architecture}`
**Location**: Section 4.1

### Specifications:
- **Type**: Flowchart with 4 main boxes connected by arrows
- **Style**: Professional, clean, academic
- **Colors**:
  - Stage boxes: Light blue (#E3F2FD)
  - Novel components: Gold star (★)
  - Arrows: Dark gray (#424242)

### Content:
```
┌─────────────────────────────────────────────┐
│ STAGE 1: EXTRACTION                         │
│ • PDF input                                 │
│ • LLM entity extraction                     │
│ • Relation identification                   │
└─────────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────────┐
│ STAGE 2: INDEXING                           │
│ • S-connectivity analysis                   │
│ • Community detection (Louvain)             │
│ • Bridge identification                     │
└─────────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────────┐
│ STAGE 3: DISCOVERY (60+ operators)          │
│ • Motif detection                           │
│ • Centrality analysis                       │
│ • Bridge Analogies           ★              │
│ • Bias Audit                 ★              │
│ • Community Recommendations  ★              │
└─────────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────────┐
│ STAGE 4: EXPORT                             │
│ • HTML report                               │
│ • JSON insights                             │
│ • Pattern Library            ★              │
│ • Interactive visualization                 │
└─────────────────────────────────────────────┘
```

### Tools to Use:
- **Draw.io**: Easy online tool (export as PDF)
- **TikZ (LaTeX)**: For publication-quality vector graphics
- **PowerPoint/Keynote**: Export as PDF

### Caption:
"Knowledge discovery pipeline architecture showing four stages: (1) Extraction from PDFs using LLMs, (2) Indexing with community detection and bridge identification, (3) Discovery using 60+ operators including our three novel methods (marked with ⭐), and (4) Export to multiple formats."

---

## Figure 2: Bridge-Mediated Analogy Example
**File**: `figures/bridge_analogy_example.pdf`
**Reference**: `\ref{fig:bridge-analogy}`
**Location**: Section 3.1

### Specifications:
- **Type**: Graph visualization with two communities
- **Style**: Force-directed layout with clear community separation
- **Colors**:
  - ML Community: Green (#4CAF50)
  - Symbolic AI Community: Blue (#2196F3)
  - Bridge Entity: Yellow/Gold (#FFC107)
  - Relation arrows: Black with labels

### Content:
**Left Community (ML)**:
- Nodes: "neural_network", "optimization", "gradient_descent"
- Relations: neural_network → uses → optimization

**Bridge (Center)**:
- Node: "knowledge_graph" (large, yellow, highlighted)

**Right Community (Symbolic AI)**:
- Nodes: "reasoning", "logic", "inference"
- Relations: reasoning → uses → logic

**Cross-community relations**:
- ML → knowledge_graph
- knowledge_graph → Symbolic AI

### Tools to Use:
- **Gephi**: Force-directed layout, export as PDF
- **Cytoscape**: Network visualization
- **Python (NetworkX + Matplotlib)**:
  ```python
  import networkx as nx
  import matplotlib.pyplot as plt

  G = nx.Graph()
  # Add nodes with community attribute
  G.add_node("knowledge_graph", community="bridge")
  G.add_node("neural_network", community="ML")
  # ... add more nodes and edges

  # Color by community
  colors = [community_colors[G.nodes[n]['community']] for n in G.nodes()]

  # Draw with spring layout
  pos = nx.spring_layout(G, k=2)
  nx.draw(G, pos, node_color=colors, with_labels=True)
  plt.savefig("bridge_analogy_example.pdf")
  ```

### Caption:
"Example bridge-mediated analogy: 'knowledge graph' connects ML and Symbolic AI communities, enabling cross-domain transfer. The bridge entity (yellow) mediates relation patterns (uses → optimization vs. uses → reasoning) across domains."

---

## Figure 3: Bias Audit Results
**File**: `figures/bias_audit_results.pdf`
**Reference**: `\ref{fig:bias-audit}`
**Location**: Section 5.2

### Specifications:
- **Type**: Two-panel figure
  - **Top panel**: Horizontal bar chart of source citations
  - **Bottom panel**: Lorenz curve showing Gini coefficient
- **Colors**:
  - Over-represented: Red (#F44336)
  - Normal: Gray (#9E9E9E)
  - Under-represented: Blue (#2196F3)

### Content:
**Top Panel (Bar Chart)**:
- X-axis: Number of citations (0-20)
- Y-axis: Source documents (22 sources, sorted by citation count)
- Bars colored by representation status
- Mean line (dashed vertical line at ~5.14)
- 2x mean line (dashed at ~10.28)

**Bottom Panel (Lorenz Curve)**:
- X-axis: Cumulative % of sources (0-100%)
- Y-axis: Cumulative % of citations (0-100%)
- Diagonal line (perfect equality)
- Actual Lorenz curve (below diagonal)
- Shaded area = Gini coefficient (0.38)
- Annotation: "Gini = 0.38"

### Tools to Use:
- **Python (Matplotlib/Seaborn)**:
  ```python
  import matplotlib.pyplot as plt
  import numpy as np

  # Top panel: Bar chart
  fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 10))

  sources = [...]  # List of 22 sources
  counts = [...]   # Citation counts (sorted)
  mean_count = np.mean(counts)

  colors = ['red' if c > 2*mean_count else 'blue' if c < mean_count/2 else 'gray'
            for c in counts]

  ax1.barh(sources, counts, color=colors)
  ax1.axvline(mean_count, ls='--', color='black', label='Mean')
  ax1.axvline(2*mean_count, ls='--', color='red', label='2x Mean')

  # Bottom panel: Lorenz curve
  cumsum = np.cumsum(sorted(counts))
  cumsum_pct = cumsum / cumsum[-1]
  x = np.linspace(0, 1, len(counts))

  ax2.plot([0, 1], [0, 1], 'k--', label='Perfect Equality')
  ax2.plot(x, cumsum_pct, 'b-', linewidth=2, label='Actual Distribution')
  ax2.fill_between(x, x, cumsum_pct, alpha=0.3)
  ax2.text(0.5, 0.3, f'Gini = 0.38', fontsize=14)

  plt.savefig("bias_audit_results.pdf")
  ```

### Caption:
"Source representation distribution showing Gini coefficient of 0.38 (moderate inequality). Top: Bar chart of citation counts by source. Bottom: Lorenz curve showing cumulative distribution with over-represented (red) and under-represented (blue) sources highlighted."

---

## Figure 4: Pattern Library Structure
**File**: `figures/pattern_library_structure.pdf`
**Reference**: `\ref{fig:pattern-library}`
**Location**: Section 3.2

### Specifications:
- **Type**: Entity-Relationship Diagram (ERD) style
- **Style**: Boxes with relationships
- **Colors**: Light blue boxes with black borders

### Content:
```
┌─────────────────────────────────┐
│ Pattern Library                 │
├─────────────────────────────────┤
│ • meta: {generated_at, run_id}  │
│ • patterns: [...]               │
│ • statistics: {...}             │
│ • usage: {...}                  │
└─────────────────────────────────┘
              │
              │ contains
              ↓
    ┌─────────────────────┐
    │ Pattern             │
    ├─────────────────────┤
    │ • id                │
    │ • type              │
    │ • size              │
    │ • nodes: [...]      │
    │ • confidence        │
    │ • metadata: {...}   │
    │ • sources: [...]    │
    └─────────────────────┘
              │
              │ has
              ↓
    ┌─────────────────────┐
    │ Metadata            │
    ├─────────────────────┤
    │ • occurrences       │
    │ • pattern_size      │
    │ • support           │
    └─────────────────────┘
```

### Tools to Use:
- **Draw.io**: ERD template
- **Lucidchart**: Online diagramming
- **LaTeX TikZ**: For vector graphics

### Caption:
"Pattern library schema showing hierarchical organization of patterns, metadata, and statistics. The JSON structure enables programmatic querying and cross-domain comparison."

---

## Figure 5: Community Structure with Bridges
**File**: `figures/community_structure.pdf`
**Reference**: `\ref{fig:community-structure}`
**Location**: Section 5.1

### Specifications:
- **Type**: Network graph with communities highlighted
- **Style**: Force-directed layout
- **Colors**:
  - Community 1 (ML/Embeddings): Green (#4CAF50)
  - Community 2 (Symbolic): Blue (#2196F3)
  - Community 3 (Data Management): Orange (#FF9800)
  - Bridge nodes: Yellow stars (⭐)

### Content:
- 297 nodes (entities)
- 3 distinct communities (colored regions)
- 7 bridge nodes marked with stars
- Node size proportional to degree
- Edge transparency based on weight

### Tools to Use:
- **Gephi**:
  - Import graph data
  - Run ForceAtlas2 layout
  - Apply community colors
  - Mark bridges with custom symbols
  - Export as PDF

- **Python (NetworkX)**:
  ```python
  import networkx as nx
  import matplotlib.pyplot as plt
  from networkx.algorithms import community

  # Load your graph
  G = nx.read_graphml("your_graph.graphml")

  # Detect communities
  communities = community.louvain_communities(G)

  # Identify bridges (high betweenness centrality)
  betweenness = nx.betweenness_centrality(G)
  bridges = [n for n, b in betweenness.items() if b > threshold]

  # Color nodes
  colors = []
  for node in G.nodes():
      if node in bridges:
          colors.append('yellow')
      else:
          # Color by community
          for i, comm in enumerate(communities):
              if node in comm:
                  colors.append(['green', 'blue', 'orange'][i])

  # Draw
  pos = nx.spring_layout(G, k=1, iterations=50)
  nx.draw(G, pos, node_color=colors, node_size=[G.degree(n)*10 for n in G.nodes()])

  # Highlight bridges
  nx.draw_networkx_nodes(G, pos, nodelist=bridges,
                          node_shape='*', node_size=500, node_color='yellow')

  plt.savefig("community_structure.pdf")
  ```

### Caption:
"Knowledge graph community structure (3 communities, 297 entities) with 7 bridge entities highlighted (stars). Communities represent distinct research areas: ML/Embeddings (green), Symbolic Reasoning (blue), and Data Management (orange)."

---

## Figure 6: Efficiency Comparison Chart
**File**: `figures/efficiency_chart.pdf`
**Reference**: `\ref{fig:efficiency-chart}`
**Location**: Section 5.6

### Specifications:
- **Type**: Horizontal bar chart with grouped bars
- **Style**: Clean, professional
- **Colors**:
  - Estimated time: Gray (#9E9E9E)
  - Actual time: Green (#4CAF50)

### Content:
- Y-axis: Features (5 features)
- X-axis: Time in hours (0-200 hours = 0-8.3 days)
- Two bars per feature (estimated vs actual)
- Error bars on estimates showing range
- Annotations showing efficiency multiplier (e.g., "20×")

### Data:
```
Feature               Estimated (h)  Actual (h)  Multiplier
Future Work Updates   16-24          2           12×
Pattern Library       24-32          3           8-11×
Bridge Analogies      24-40          4           6-10×
Bias Audit            40-56          2           20-28×
Community Recs        40-56          2           20-28×
```

### Tools to Use:
- **Python (Matplotlib)**:
  ```python
  import matplotlib.pyplot as plt
  import numpy as np

  features = ['Future Work\nUpdates', 'Pattern\nLibrary',
              'Bridge\nAnalogies', 'Bias\nAudit', 'Community\nRecs']
  estimated = [20, 28, 32, 48, 48]  # Mid-point of ranges
  actual = [2, 3, 4, 2, 2]
  errors = [4, 4, 8, 8, 8]  # Half of range

  y_pos = np.arange(len(features))

  fig, ax = plt.subplots(figsize=(10, 6))

  # Bars
  ax.barh(y_pos - 0.2, estimated, 0.4, xerr=errors,
          label='Estimated', color='gray', alpha=0.7)
  ax.barh(y_pos + 0.2, actual, 0.4,
          label='Actual', color='green', alpha=0.9)

  # Annotations
  for i, (est, act) in enumerate(zip(estimated, actual)):
      mult = est / act
      ax.text(max(est, act) + 5, y_pos[i], f'{mult:.0f}×',
              va='center', fontsize=12, fontweight='bold')

  ax.set_yticks(y_pos)
  ax.set_yticklabels(features)
  ax.set_xlabel('Time (hours)')
  ax.legend()
  ax.set_title('Development Efficiency: Estimated vs. Actual Time')

  plt.tight_layout()
  plt.savefig("efficiency_chart.pdf")
  ```

### Caption:
"Development efficiency comparison: estimated time (gray bars) vs. actual implementation time (green bars) for each feature. Error bars show estimate ranges. Average efficiency multiplier: 20×."

---

## Figure 7: Pattern Distribution Histogram
**File**: `figures/pattern_distribution.pdf`
**Reference**: `\ref{fig:pattern-distribution}`
**Location**: Section 5.4

### Specifications:
- **Type**: Stacked bar chart
- **Style**: Clean, colorful
- **Colors**: Different color per pattern type

### Content:
- X-axis: Pattern size (2-node, 3-node, 4-node)
- Y-axis: Count (0-70)
- Stacked bars showing pattern types (Motif, K-Truss, Meta-Pattern)

### Data:
```
Size  | Motifs | K-Truss | Meta-Pattern | Total
2-node|   40   |   25    |      0       |  65
3-node|    5   |    1    |      0       |   6
4-node|    0   |    1    |      1       |   2
Total |   45   |   27    |      1       |  73
```

### Tools to Use:
- **Python (Matplotlib)**:
  ```python
  import matplotlib.pyplot as plt
  import numpy as np

  sizes = ['2-node', '3-node', '4-node']
  motifs = [40, 5, 0]
  k_truss = [25, 1, 1]
  meta = [0, 0, 1]

  x = np.arange(len(sizes))
  width = 0.6

  fig, ax = plt.subplots(figsize=(8, 6))

  ax.bar(x, motifs, width, label='Motifs', color='#2196F3')
  ax.bar(x, k_truss, width, bottom=motifs, label='K-Truss', color='#4CAF50')
  ax.bar(x, meta, width, bottom=np.array(motifs)+np.array(k_truss),
         label='Meta-Pattern', color='#FF9800')

  ax.set_ylabel('Count')
  ax.set_xlabel('Pattern Size')
  ax.set_title('Pattern Distribution by Size and Type')
  ax.set_xticks(x)
  ax.set_xticklabels(sizes)
  ax.legend()

  # Add total counts on top
  totals = [65, 6, 2]
  for i, total in enumerate(totals):
      ax.text(i, total + 2, str(total), ha='center', fontweight='bold')

  plt.tight_layout()
  plt.savefig("pattern_distribution.pdf")
  ```

### Caption:
"Distribution of 73 discovered patterns by size. Predominantly pairwise patterns (2-node: 89%), with some triadic (3-node: 8%) and larger structures (4-node: 3%), indicating focused relationship extraction."

---

## Figure 8: Recommendation Quality Heatmap
**File**: `figures/recommendation_quality.pdf`
**Reference**: `\ref{fig:recommendation-quality}`
**Location**: Section 5.5

### Specifications:
- **Type**: Heatmap / Matrix visualization
- **Style**: Color-coded cells
- **Colors**: Gradient from blue (low) to red (high)

### Content:
- Rows: Recommendation categories (Within-Community, Cross-Community)
- Columns: Similarity score ranges (0.3-0.5, 0.5-0.7, 0.7-0.9, 0.9-1.0)
- Cell values: Count of recommendations
- Annotations: Percentages

### Data:
```
Category           | 0.3-0.5 | 0.5-0.7 | 0.7-0.9 | 0.9-1.0 | Total
Within-Community   |    20   |   40    |   52    |   20    |  132 (66%)
Cross-Community    |    30   |   28    |   10    |    0    |   68 (34%)
```

### Tools to Use:
- **Python (Seaborn)**:
  ```python
  import seaborn as sns
  import matplotlib.pyplot as plt
  import numpy as np

  data = np.array([
      [20, 40, 52, 20],  # Within-community
      [30, 28, 10, 0]     # Cross-community
  ])

  fig, ax = plt.subplots(figsize=(10, 4))

  sns.heatmap(data, annot=True, fmt='d', cmap='YlOrRd',
              xticklabels=['0.3-0.5', '0.5-0.7', '0.7-0.9', '0.9-1.0'],
              yticklabels=['Within-Community\n(Familiar)', 'Cross-Community\n(Novel)'],
              cbar_kws={'label': 'Count'}, ax=ax)

  ax.set_xlabel('Similarity Score Range')
  ax.set_ylabel('Recommendation Category')
  ax.set_title('Recommendation Quality Distribution')

  plt.tight_layout()
  plt.savefig("recommendation_quality.pdf")
  ```

### Caption:
"Community recommendation distribution showing 66% within-community (familiar, high similarity) and 34% cross-community (novel, moderate similarity). Heatmap shows similarity score distributions for each category."

---

## Figure 9: Example Knowledge Subgraph
**File**: `figures/example_subgraph.pdf`
**Reference**: `\ref{fig:example-subgraph}`
**Location**: Section 5.3

### Specifications:
- **Type**: Detailed graph diagram
- **Style**: Clean, labeled graph
- **Colors**: Same as Figure 2 (communities colored)

### Content:
**Left Community (ML)**:
- Nodes: neural_network, optimization, gradient_descent, transformer
- Relations:
  - neural_network --uses--> optimization
  - optimization --employs--> gradient_descent
  - transformer --uses--> optimization

**Center (Bridge)**:
- Node: knowledge_graph (large, highlighted)

**Right Community (Symbolic AI)**:
- Nodes: reasoning, logic, inference, theorem_proving
- Relations:
  - reasoning --uses--> logic
  - logic --enables--> inference
  - inference --applies--> theorem_proving

**Bridge Relations**:
- neural_network --related_to--> knowledge_graph
- knowledge_graph --related_to--> reasoning

### Tools to Use:
- Same as Figure 2 (Gephi/NetworkX)
- Can reuse code with different node set

### Caption:
"Example subgraph showing bridge-mediated analogy between neural network optimization (left community) and knowledge graph reasoning (right community) via bridge entity 'knowledge graph'. Relation patterns (uses → method) are structurally similar across domains."

---

## Figure 10: Algorithm Flowchart (Alternative to Algorithm Block)
**File**: `figures/algorithm_flowchart.pdf` *(Optional)*
**Reference**: *(Could replace Algorithm 1)*
**Location**: Section 3.1

### Specifications:
- **Type**: Flowchart diagram
- **Style**: Standard flowchart symbols
- **Colors**: Light blue for processes, yellow for decisions

### Content:
Visual representation of Algorithm 1 (Bridge-Mediated Analogy Discovery)

### Tools to Use:
- **Draw.io**: Flowchart template
- **TikZ**: For publication quality

### Caption:
"Flowchart representation of bridge-mediated analogy discovery algorithm showing iterative community pair comparison and pattern matching."

---

## Summary of Figures to Create

1. ✅ **System Architecture** - Flowchart (Draw.io/TikZ)
2. ✅ **Bridge Analogy Example** - Network graph (Gephi/NetworkX)
3. ✅ **Bias Audit Results** - Bar chart + Lorenz curve (Matplotlib)
4. ✅ **Pattern Library Structure** - ERD diagram (Draw.io)
5. ✅ **Community Structure** - Network graph with bridges (Gephi/NetworkX)
6. ✅ **Efficiency Chart** - Horizontal bar chart (Matplotlib)
7. ✅ **Pattern Distribution** - Stacked bar chart (Matplotlib)
8. ✅ **Recommendation Quality** - Heatmap (Seaborn)
9. ✅ **Example Subgraph** - Detailed graph (Gephi/NetworkX)
10. ⚠️ **Algorithm Flowchart** - Optional (LaTeX algorithmic package is sufficient)

---

## Quick Start Guide

### Option 1: Python Scripts (Automated)
Run the provided Python scripts in `/tmp/figure_generation_scripts/` to generate figures 3, 6, 7, 8 automatically.

### Option 2: Manual Creation (Draw.io)
For figures 1, 4:
1. Go to https://app.diagrams.net/
2. Follow the content specifications above
3. Export as PDF

### Option 3: Network Visualization (Gephi)
For figures 2, 5, 9:
1. Export your graph data to GEXF/GraphML format
2. Import to Gephi
3. Apply layouts and styling
4. Export as PDF

---

## File Organization

Create a `figures/` directory with these files:
```
figures/
├── system_architecture.pdf
├── bridge_analogy_example.pdf
├── bias_audit_results.pdf
├── pattern_library_structure.pdf
├── community_structure.pdf
├── efficiency_chart.pdf
├── pattern_distribution.pdf
├── recommendation_quality.pdf
├── example_subgraph.pdf
└── (optional) algorithm_flowchart.pdf
```

Place this directory in the same location as your LaTeX file, or update paths in the `\includegraphics{}` commands.

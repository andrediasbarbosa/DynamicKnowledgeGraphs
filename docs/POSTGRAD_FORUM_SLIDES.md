---
marp: true
paginate: true
size: 16:9
title: Dynamic Knowledge Graph Discovery from Academic PDFs
---

# Dynamic Knowledge Graph Discovery from Academic PDFs

### Hypergraphs, discovery operators, Graph-RAG, and causal next steps

Post-grad Forum Presentation  
DynamicKGs / Batch4  

Presenter: _[Your Name]_  
Date: _[Add Date]_

---

# Why This Project?

- Research papers contain rich structure, but most of it remains trapped in unstructured text.
- Standard pipelines usually stop at extraction or simple retrieval.
- This project asks a harder question:
- Can we extract knowledge graphs from papers and then automatically discover new patterns, bridges, and hypotheses?

**Core idea:** move from `PDF -> text` to `PDF -> hypergraph -> discovery -> interactive reasoning`.

---

# The Problem We Are Tackling

- Academic literature is large, fragmented, and cross-disciplinary.
- Important relations are often higher-order, not just pairwise.
- Interesting insights may sit between communities rather than inside one paper.
- Bias and over-representation can distort what a KG "discovers."

**Research goal:** build a pipeline that is useful for both knowledge extraction and knowledge discovery.

---

# What The System Does

- Extracts entities and relations from PDF text.
- Represents them as a **directed hypergraph** rather than a simple graph.
- Builds indexes for traversal, community structure, and pattern search.
- Runs **55+ discovery operators** over the graph.
- Produces reports, HTML visualizations, and a Graph-RAG interface.

**Why hypergraphs?**  
Some relations involve multiple sources and multiple targets. A hyperedge preserves that structure.

---

# End-to-End Pipeline

1. **PDF processing**
2. **Chunking**
3. **LLM extraction**
4. **Hypergraph construction**
5. **Indexing**
6. **Discovery**
7. **Rendering + reporting**
8. **Interactive Graph-RAG exploration**

**Working principle:** each stage adds structure, not just more text.

---

# Main Design Features

- **Hypergraph representation**
- Preserves n-ary relationships instead of flattening everything into triples

- **Operator-based discovery engine**
- Structural, semantic, exploratory, and transformational insights

- **Interactive output**
- HTML reports, graph views, augmentation overlays, Graph-RAG

- **Open architecture**
- C++ core pipeline
- Python extraction path
- FastAPI + Kuzu backend for interactive querying

---

# Why The Hypergraph Matters

- Many research statements are not naturally binary.
- Example pattern:
- `[method, dataset] --evaluated_with--> [metric, score]`

Compared with ordinary graphs, hypergraphs help with:

- preserving compositional relations
- supporting richer path reasoning
- discovering motifs and bridge structures
- keeping more context during downstream analysis

---

# Discovery Layer

The system supports a broad family of operators, including:

- bridges
- completions
- motifs
- community detection
- centrality
- k-core / k-truss
- meta-paths
- hypotheses
- evidence fusion
- causal operators in the V2 branch

**Framing:** the KG is not the final product. The KG is the substrate for automated discovery.

---

# Novel Contributions Highlighted in the Project

- **Bridge-mediated analogical reasoning**
- Uses graph bridges to identify cross-domain analogies

- **Pattern library export**
- Reusable structural templates from discovered motifs and graph patterns

- **Bias audit metrics**
- Uses Gini-style analysis to inspect representation imbalance

- **Community-aware recommendations**
- Suggests entities that are locally relevant and cross-community novel

---

# High-Level Implementation

- **Core language:** C++17
- **Data model:** custom hypergraph implementation
- **Serialization:** JSON
- **LLM integration:** OpenAI / Gemini
- **Graph-RAG backend:** FastAPI + Kuzu
- **Frontend:** interactive 3D graph and report views

**Design choice:** performance-sensitive graph logic stays in C++, while user-facing interaction is served through the web stack.

---

# Example Technology Split

**C++ pipeline**

- extraction orchestration
- graph building
- indexing
- discovery operators
- report generation

**Python / web layer**

- lightweight extraction path
- Graph-RAG server
- natural-language query layer
- 3D visualization and data exploration

This makes the project useful both as a research system and as a demoable tool.

---

# Interesting Results

From the current evaluation and publication materials:

- **100+ cross-domain analogies** discovered
- **84% precision** on manually validated bridge analogies
- **70% cross-domain pattern transferability**
- **Gini coefficient 0.38** for representation inequality
- **~20x development efficiency** compared with traditional manual implementation

These results position the project as both a methodology contribution and a working system.

---

# Evaluation Snapshot

Example corpus used in the publication materials:

- **22 PDF papers**
- **297 entities**
- **182 relations**
- **113 insights discovered**

This is not "big data" scale.
It is a research-scale corpus designed to test whether discovery methods produce useful structure and interpretable outputs.

---

# Why The Results Matter

- The analogy results suggest the system can find non-obvious structural correspondences across domains.
- The pattern library suggests some graph structures are reusable, not one-off artifacts.
- The bias audit shows we can inspect discovery quality, not just discovery quantity.
- The Graph-RAG layer makes the outputs explorable by humans, not just machine-readable.

**Takeaway:** the project tries to combine discovery, interpretability, and responsible analysis.

---

# Suggested Visuals For These Slides

- `paper/figures/bridge_analogy_example.pdf`
- `paper/figures/community_structure.pdf`
- `paper/figures/example_subgraph.pdf`
- `paper/figures/bias_audit_results.pdf`
- `paper/figures/efficiency_chart.pdf`
- `paper/figures/pattern_distribution.pdf`

Use 2-3 of these rather than overloading the deck.

Good pairing:

- analogy figure
- community structure figure
- efficiency or bias audit figure

---

# What Is Still Difficult?

- LLM extraction can over-generate low-value or noisy relations.
- Generic entities and placeholder symbols can pollute the graph.
- Manual validation is still expensive.
- Scaling to much larger corpora will need more engineering.
- Discovery quality depends heavily on extraction quality.

This is important to say openly in a post-grad setting.
The project is promising, but not "solved."

---

# Current Upgrade Direction: V2

The next major direction is:

- richer chunking with overlap
- structured extraction
- causal metadata
- causal discovery operators
- stronger provenance and verification

Conceptually, this moves the system from:

`association-heavy KG discovery`

to:

`mechanism-aware and intervention-aware KG discovery`

---

# Why The V2 Direction Is Important

- It addresses missed relations at chunk boundaries.
- It provides more structured evidence for causal reasoning.
- It improves the quality of downstream hypotheses.
- It helps bridge the gap between surface extraction and more explanatory AI systems.

This is also where the project connects to broader discussions about explainability, neuro-symbolic reasoning, and the "AI chasm."

---

# Research Value For Post-Grad Students

- It is a good example of **end-to-end research engineering**.
- It combines:
- NLP
- graph mining
- visualization
- responsible AI
- retrieval and reasoning

- It shows how a PhD-style system can evolve from:
- extraction
- to analysis
- to interactive tooling
- to publication-ready evaluation

---

# Main Takeaways

- This project is not just a KG extractor.
- It is a **knowledge discovery system** built on extracted graphs.
- Its distinctive features are:
- hypergraph representation
- operator-driven discovery
- bridge-mediated analogies
- pattern libraries
- bias-aware analysis
- interactive Graph-RAG exploration

- The most important future work is improving extraction quality and causal structure.

---

# Discussion Questions

- When does a discovered graph pattern count as a genuine research insight?
- How much validation should happen before a discovered analogy is shown to users?
- Should fairness be audited at the graph level, the operator level, or the source-document level?
- Can causal reasoning be made reliable enough from literature extraction alone?

---

# Thank You

Questions and feedback welcome.

Useful follow-up materials in this repo:

- `README.md`
- `docs/PROJECT_OVERVIEW.md`
- `paper/publication_draft.md`
- `docs/V2_LLM_EXTRACTION_REDESIGN_PLAN.md`

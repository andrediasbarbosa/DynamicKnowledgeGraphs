Knowledge Discovery Run: run_20260322_181058
==================================================

Title: BenchMarking LLMs for KG Validation Knowledge Discovery
Sources: BenchMarking LLMs for KG Validation.pdf, Construxtion of KGs.pdf, Embedding Uncertainty in KGs.pdf, Evolvable KGs.pdf, Geranium Platform for Science KGs.pdf, KG Constrution.pdf, KG Curation.pdf, KG NeuroSymbolic.pdf, KG Querying.pdf, KG Reasoning.pdf, KGs a Practical Review.pdf, KGs and LLMs in RAG.pdf, KGs and LLMs.pdf, MultiHop QA on KGs.pdf, NeuroSym Reasoning.pdf, Paths over KG.pdf, Reasoning on KGs.pdf, Relational Data on KGs.pdf, Research from KGs.pdf, Robust RAG with KGs.pdf, Rule Guided Learning on KGs.pdf, Sheaf KGs.pdf

Artifacts:
  Data:
    graph.json           - Extracted knowledge graph
    index.json           - S-component index
    insights.json        - Discovered insights
    augmentation.json    - Augmentation overlay data
    extraction_stats.json - Pipeline statistics
    manifest.json        - Run metadata

  Visualizations:
    graph.html           - Interactive 3D graph viewer
    graph_augmented.html - Augmented view with insights
    graph.dot            - GraphViz DOT format

  Reports:
    report.md            - Markdown report
    report.html          - Styled HTML report

To view:
  cd runs/run_20260322_181058
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

Knowledge Discovery Run: run_20260325_102032
==================================================

Title: An Introduction to Knowledge Graphs Knowledge Discovery
Sources: An Introduction to Knowledge Graphs.pdf, Building Applications with AI Agents.pdf, Building Knowledge Graphs_ A Practitioner’s Guide.pdf, HigherOrdeerKGAgentic.pdf, Knowledge Graphs and LLMs in Action.pdf, Neuro-Symbolic AI Knowledge Discovery.pdf, Semantic AI in Knowledge Graphs.pdf

Artifacts:
  Data:
    Step_4_GraphBuilding/graph.json        - Extracted knowledge graph
    Step_5_Discovery/index.json            - S-component index
    Step_5_Discovery/insights.json         - Discovered insights
    Step_5_Discovery/augmentation.json     - Augmentation overlay data
    Step_2_Extraction/extraction_stats.json - Pipeline statistics
    manifest.json                          - Run metadata

  Visualizations:
    Step_5_Discovery/graph.html            - Interactive 3D graph viewer
    Step_5_Discovery/graph_augmented.html  - Augmented view with insights
    Step_5_Discovery/graph_rag.html        - Graph-RAG chat viewer
    Step_5_Discovery/graph.dot             - GraphViz DOT format

  Reports:
    Step_5_Discovery/report.md             - Markdown report
    Step_5_Discovery/report.html           - Styled HTML report (with Phase 2 causal badges)

To view:
  cd runs/run_20260325_102032/Step_5_Discovery
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

Knowledge Discovery Run: run_20260207_165334
==================================================

Title: An Introduction to KGs Knowledge Discovery
Sources: An Introduction to KGs.pdf, Knowledge Graphs and LLMs in Action.pdf, Semantic AI in Knowledge Graphs.pdf, Understanding Deep Learning.pdf

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
  cd runs/run_20260207_165334
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

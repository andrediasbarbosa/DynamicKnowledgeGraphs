Knowledge Discovery Run: run_20260118_130759
==================================================

Title: three_essays_cs_music_history Knowledge Discovery
Sources: three_essays_cs_music_history.pdf

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
  cd /mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/runs/run_20260118_130759
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

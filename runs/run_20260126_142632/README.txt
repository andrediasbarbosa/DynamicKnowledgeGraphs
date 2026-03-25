Knowledge Discovery Run: run_20260126_142632
==================================================

Title: An Introduction to Knowledge Graphs -- Umutcan Serles, Dieter Fensel -- 1st ed_ 2024, 1st ed_ 2023, 1st ed_ 2025, US, 2024 -- Springer International -- 9783031452550 -- 9d6c8ceaf40d17bda1bd03d56e7821e8 -- Anna’s Ar Knowledge Discovery
Sources: An Introduction to Knowledge Graphs -- Umutcan Serles, Dieter Fensel -- 1st ed_ 2024, 1st ed_ 2023, 1st ed_ 2025, US, 2024 -- Springer International -- 9783031452550 -- 9d6c8ceaf40d17bda1bd03d56e7821e8 -- Anna’s Ar.pdf, Knowledge Graphs and LLMs in Action -- Alessandro Negro, Vlastimil Kus, Giuseppe Futia, Fabio -- IN Action, New, 2025 -- Manning Publications Co_ LLC -- 9781633439894 -- 8059c4aeb07d5a88a6f60481b8360117 -- Anna’.pdf, Semantic AI in Knowledge Graphs -- Sahan Bulathwela, María Pérez-Ortiz, Emine Yilmaz, John -- Boca Raton, 2023 -- CRC Press LLC -- 9781000911183 -- e7dc190ec9f29089148d37758135e6f4 -- Anna’s Archive.pdf

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
  cd ../../runs/run_20260126_142632
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

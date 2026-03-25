Knowledge Discovery Run: run_20260127_154259
==================================================

Title: A_Bayesian_compatibility_model_for_graph Knowledge Discovery
Sources: A_Bayesian_compatibility_model_for_graph.pdf, Deterministic_search_for_relational_grap.pdf, Geometric_characterization_and_clusterin.pdf, Inexact_graph_matching_using_genetic_sea.pdf, MAtrixRepresentationOfGraphs.pdf, PatternVectorsFromAlgebraicGraphTheory.pdf, Spectral_embedding_of_graphs.pdf, SphericalFinal.pdf, StructuralMatchingByDiscreteRelaxation.pdf, Valid-optimal-assignment-kernels-and-applications-to-graph-classification-Paper.pdf

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
  cd runs/run_20260127_154259
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

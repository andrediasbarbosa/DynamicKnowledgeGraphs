Knowledge Discovery Run: run_20260331_212255
==================================================

Title: A_Bayesian_compatibility_model_for_graph Knowledge Discovery
Sources: A_Bayesian_compatibility_model_for_graph.pdf, Deterministic_search_for_relational_grap.pdf, Geometric_characterization_and_clusterin.pdf, Inexact_graph_matching_using_genetic_sea.pdf, MAtrixRepresentationOfGraphs.pdf, PatternVectorsFromAlgebraicGraphTheory.pdf, Spectral_embedding_of_graphs.pdf, SphericalFinal.pdf, StructuralMatchingByDiscreteRelaxation.pdf, Valid-optimal-assignment-kernels-and-applications-to-graph-classification-Paper.pdf

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
  cd runs/run_20260331_212255/Step_5_Discovery
  python3 -m http.server 8080
  # Open http://localhost:8080/graph_augmented.html
  # Or open http://localhost:8080/report.html

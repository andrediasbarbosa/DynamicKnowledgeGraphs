# Project Overview

## Purpose

The repository centers on a C++ knowledge-graph pipeline that extracts relations from PDFs, cleans the resulting graph, runs discovery operators, and emits HTML plus report artifacts. Two secondary paths sit beside it:

- a Graph RAG application in `src/graph_rag_tool/`
- a smaller Python-only extractor in `src/python_porting/`

## Primary Entry Points

- `src/main.cpp`: `kg` CLI
- `src/graph_rag_tool/backend/graph_rag_server.py`: Graph RAG backend
- `src/python_porting/kg_extractor.py`: Python extractor

## High-Level Architecture

```text
PDFs
  -> ExtractionPipeline
  -> optional preprocess
  -> GraphCleaner
  -> optional ontology augmentation
  -> HypergraphIndex
  -> DiscoveryEngine
  -> AugmentationRenderer + ReportGenerator
  -> run artifacts under runs/run_*/
```

## `kg` Pipeline

Core libraries:

- `src/pipeline/`: extraction and graph cleaning
- `src/llm/`: OpenAI/Gemini provider integrations plus extraction metadata
- `src/graph/`: hypergraph data structures
- `src/index/`: s-component and lookup indexing
- `src/discovery/`: discovery operators and report generation
- `src/render/`: augmented HTML rendering

The active runtime surface is:

- `kg run`
- `kg index`
- `kg discover`
- `kg render`
- `kg report`
- `kg stats`
- `kg list-operators`

The default discovery registry currently contains 62 operators. `bias_audit` and `community_recommendation` remain implemented in code but are not part of `all_discovery_operators()`.

## Graph RAG Component

`src/graph_rag_tool/backend/` is the active development/runtime copy. It serves:

- a 3D graph viewer
- Graph RAG and document-RAG query modes
- a Kuzu-backed explorer with Cypher, schema, path, entity, and quality endpoints

`src/graph_rag_tool/deployment/` is a standalone distribution bundle of that app.

## Python Port

`src/python_porting/` is intentionally narrower:

- PDF extraction with `pdfplumber`
- Azure OpenAI extraction
- `graph.json` and `graph.html` output

It does not run the full index/discovery/report stack from `kg`.

## Output Model

The C++ pipeline writes stage-oriented artifacts:

- `Step_2_Extraction/`
- `Step_3_QualityControl/`
- `Step_4_GraphBuilding/`
- `Step_5_Discovery/`

See [`OUTPUT_FOLDER_STRUCTURE.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/OUTPUT_FOLDER_STRUCTURE.md) for the exact layout.

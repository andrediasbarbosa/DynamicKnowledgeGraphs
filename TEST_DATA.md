# Test Data Guide

This document describes the test data available for validating and experimenting with the Knowledge Graph Pipeline.

## Table of Contents

1. [Test PDF Documents](#test-pdf-documents)
2. [Running Tests](#running-tests)
3. [Expected Results](#expected-results)
4. [Creating Custom Test Data](#creating-custom-test-data)

---

## Test PDF Documents

The `tests/` directory contains sample PDF documents for testing the pipeline:

| Document | Description | Recommended For |
|----------|-------------|-----------------|
| `An Introduction to Knowledge Graphs.pdf` | Introductory material on KG concepts | Basic pipeline validation |
| `Building Knowledge Graphs_ A Practitioner's Guide.pdf` | Practical guide to KG construction | Testing entity extraction |
| `Knowledge Graphs and LLMs in Action.pdf` | Integration of KGs with LLMs | Testing complex relationships |
| `Semantic AI in Knowledge Graphs.pdf` | Semantic AI approaches | Testing semantic extraction |
| `Building Applications with AI Agents.pdf` | AI agent architectures | Testing diverse entity types |
| `HigherOrdeerKGAgentic.pdf` | Higher-order KG methods | Testing hyperedge extraction |
| `Neuro-Symbolic AI Knowledge Discovery.pdf` | Neuro-symbolic approaches | Testing discovery operators |

---

## Running Tests

### Quick Validation Test

Run the pipeline on a single test document:

```bash
./build/bin/kg run -i "tests/An Introduction to Knowledge Graphs.pdf" -t "KG Introduction Test"
```

Optional: enable preprocessing (normalize relations + merge aliases):

```bash
./build/bin/kg run -i "tests/An Introduction to Knowledge Graphs.pdf" -t "KG Introduction Test" --preprocess
```

### Full Test Suite

Process all test documents:

```bash
./build/bin/kg run -i tests/ -t "Full Test Suite"
```

### Unit Tests

Run C++ unit tests:

```bash
cd build
ctest --output-on-failure
```

Or run directly:

```bash
./build/bin/test_hypergraph
```

### Stage-Specific Testing

Test individual pipeline stages:

```bash
# Test extraction only (requires LLM API)
./build/bin/kg run -i "tests/An Introduction to Knowledge Graphs.pdf" -f 1 -t "Extraction Test"

# Test discovery with existing graph (no LLM needed)
./build/bin/kg run -f 3 -d runs/<existing_run_dir>

# Test rendering only
./build/bin/kg run -f 4 -d runs/<existing_run_dir>
```

---

## Expected Results

### Typical Entity Counts

For a single ~20 page PDF document, expect approximately:

| Metric | Typical Range |
|--------|---------------|
| Entities (nodes) | 50-200 |
| Relationships (edges) | 100-500 |
| Bridge insights | 5-20 |
| Motif insights | 0-10 |
| Completion insights | 0-15 |
| Surprise insights | 0-10 |

### Common Entity Types

The pipeline typically extracts:

- **Concepts**: Knowledge Graph, Machine Learning, Neural Network
- **Technologies**: Python, TensorFlow, SPARQL
- **Methods**: Embedding, Classification, Inference
- **People**: Authors, researchers (from citations)
- **Organizations**: Universities, companies

### Validation Checklist

After running the pipeline, verify:

- [ ] `graph.json` contains nodes and edges
- [ ] `index.json` has s-component data
- [ ] `insights.json` contains discovery results
- [ ] `graph.html` opens in browser without errors
- [ ] `graph_augmented.html` shows augmented nodes (colored differently)
- [ ] `report.html` renders properly with insight summaries

---

## Creating Custom Test Data

### Ideal Test Document Characteristics

For best results, test documents should:

1. **Be text-based** (not scanned images)
2. **Have clear structure** (sections, headings)
3. **Contain domain-specific terminology**
4. **Include relationships** (X is-a Y, A causes B, etc.)
5. **Be 5-50 pages** (too short lacks data, too long increases cost)

### Creating Synthetic Test Data

You can create a test graph manually:

```json
{
  "nodes": [
    {"id": "concept_1", "label": "Knowledge Graph", "type": "entity"},
    {"id": "concept_2", "label": "Machine Learning", "type": "entity"},
    {"id": "rel_1", "label": "uses", "type": "relation"}
  ],
  "edges": [
    {
      "id": "e_1",
      "sources": ["concept_1"],
      "targets": ["concept_2"],
      "relation": "uses"
    }
  ]
}
```

Then test discovery and rendering without extraction:

```bash
# Skip extraction, start from indexing
./build/bin/kg index -i custom_graph.json -o ./test_index
./build/bin/kg discover -i custom_graph.json -x ./test_index -o insights.json -p "bridges,completions,core_periphery,centrality,community_detection,k_core,k_truss,claim_stance,relation_induction,analogical_transfer,uncertainty_sampling,counterfactual,hyperedge_prediction"
./build/bin/kg render -i custom_graph.json -n insights.json -o ./test_output
```

---

## Test Resources

The `tests/ReportResources/` directory contains:

- `graph.html` - Sample baseline visualization
- `report.html` - Sample HTML report

These can be used as reference outputs for comparison.

---

## Troubleshooting Tests

### No Entities Extracted

- Check LLM API key is configured
- Verify PDF contains extractable text (not scanned)
- Try a different test document

### No Insights Generated

- Graph may be too small (need more entities/edges)
- Try loosening discovery thresholds
- Ensure s-components are being computed

### Visualization Not Loading

- Check browser console for JavaScript errors
- Verify `graph.json` is valid JSON
- Try the baseline viewer before augmented

### Memory Issues with Large Tests

- Process one document at a time
- Increase available system memory
- Reduce chunk size in config

---

## Performance Benchmarks

Typical processing times (varies by hardware and LLM provider):

| Stage | Time per 10 pages |
|-------|-------------------|
| Extraction | 2-5 minutes |
| Indexing | 1-5 seconds |
| Discovery | 5-30 seconds |
| Rendering | 1-5 seconds |
| Reporting | 1-3 seconds |

**Note**: Extraction time depends heavily on LLM API latency and rate limits.


** CORPORA DATA
https://digitalcorpora.org/corpora/file-corpora/cc-main-2021-31-pdf-untruncated/
https://digitalcorpora.s3.amazonaws.com/s3_browser.html
https://digitalcorpora.s3.amazonaws.com/s3_browser.html#corpora/files/CC-MAIN-2021-31-PDF-UNTRUNCATED/
https://digitalcorpora.s3.amazonaws.com/s3_browser.html#corpora/files/CC-MAIN-2021-31-PDF-UNTRUNCATED/zipfiles/0000-0999/

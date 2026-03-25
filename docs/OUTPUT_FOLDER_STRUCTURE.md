# V2 Output Folder Structure

**Status:** ✅ Implemented
**Version:** V2 with step-based organization
**Date:** 2026-03-22

---

## Overview

All pipeline outputs are now organized into step-based subfolders for clarity and easier debugging. Each major pipeline stage has its own dedicated folder.

---

## Folder Structure

```
runs/
└── <run_id>/                           # e.g., run_20260322_143052/
    │
    ├── Step_1_Loading/                 # PDF Loading & Chunking
    │   └── (future: chunk metadata)
    │
    ├── Step_2_Extraction/              # LLM Extraction
    │   ├── extraction_stats.json       # V2 statistics (before/after dedup)
    │   └── <document_id>_extractions.json  # Raw extraction results per PDF
    │
    ├── Step_3_Deduplication/           # V2 Deduplication (reserved)
    │   └── (future: dedup details)
    │
    ├── Step_4_GraphBuilding/           # Hypergraph Construction
    │   ├── graph_raw.json             # Before preprocessing
    │   └── graph.json                 # After normalization & alias merging
    │
    ├── Step_5_Discovery/               # Discovery & Reporting
    │   ├── index.json                 # S-component index
    │   ├── insights.json              # All discovered insights
    │   ├── graph.html                 # Interactive graph viewer
    │   ├── graph_augmented.html       # Graph with insight overlays
    │   ├── augmentation.json          # Insight augmentation data
    │   ├── report.md                  # Markdown report
    │   ├── report.html                # HTML report (with V2 stats)
    │   └── pattern_library.json       # Exported pattern library
    │
    └── manifest.json                   # Run metadata

```

---

## Step Descriptions

### **Step 1: Loading**
- **Purpose:** PDF loading and text chunking
- **V2 Features:**
  - Sliding window chunking with 15% overlap
  - Percentage-based overlap configuration
- **Artifacts:** (Future: chunk metadata for debugging)

---

### **Step 2: Extraction**
- **Purpose:** LLM-based relation extraction
- **V2 Features:**
  - Automatic provenance attachment
  - Per-chunk extraction tracking
- **Artifacts:**
  - `extraction_stats.json` - Pipeline statistics including:
    - Total relations extracted
    - Relations before/after deduplication
    - Duplicates merged
    - Multi-provenance relations
    - Average confidence boost
  - `<document_id>_extractions.json` - Raw extraction results per PDF

---

### **Step 3: Deduplication**
- **Purpose:** Merge duplicate relations from overlapping chunks
- **V2 Features:**
  - Normalized key matching
  - Confidence boosting
  - Provenance aggregation
- **Artifacts:** (Reserved for future detailed dedup reports)
- **Note:** Deduplication happens in-memory; results saved in Step 2 stats

---

### **Step 4: Graph Building**
- **Purpose:** Convert extracted relations to hypergraph
- **V2 Features:**
  - Handles deduplicated relations
  - Preserves provenance metadata
- **Artifacts:**
  - `graph_raw.json` - Original graph before preprocessing
  - `graph.json` - Final graph after normalization and alias merging

---

### **Step 5: Discovery**
- **Purpose:** Run discovery operators and generate reports
- **V2 Features:**
  - V2 statistics section in HTML report
  - Future: Causal insight sections (Phase 3-6)
- **Artifacts:**
  - `index.json` - Hypergraph index for fast queries
  - `insights.json` - All discovered insights
  - `graph.html` - Interactive baseline viewer
  - `graph_augmented.html` - Graph with insight highlights
  - `augmentation.json` - Insight augmentation data
  - `report.md` - Markdown report
  - `report.html` - Styled HTML report with V2 stats
  - `pattern_library.json` - Pattern library export

---

## Console Output Examples

### New Step-Based Messages

```
Output: runs/run_20260322_143052/

Stage 1: Knowledge Extraction
----------------------------------------------------------------------
  Extracted: 450 entities, 1000 relationships
  Saved: Step_4_GraphBuilding/graph_raw.json
  Saved: Step_2_Extraction/extraction_stats.json

Stage 1.5: Preprocess Graph
----------------------------------------------------------------------
  Normalized relations: 23
  Merged nodes: 8
  Saved: Step_4_GraphBuilding/graph.json

Stage 2: Building Index
----------------------------------------------------------------------
  S-components computed for s = 2, 3, 4
  Saved: Step_5_Discovery/index.json

Stage 3: Knowledge Discovery
----------------------------------------------------------------------
  Discovered 89 insights
  Saved: Step_5_Discovery/insights.json

Stage 4: Generating Visualizations
----------------------------------------------------------------------
  Saved: Step_5_Discovery/graph.html (baseline viewer)
  Saved: Step_5_Discovery/augmentation.json
  Saved: Step_5_Discovery/graph_augmented.html

Stage 5: Generate Reports
----------------------------------------------------------------------
  Saved: Step_5_Discovery/report.md
  Saved: Step_5_Discovery/report.html
  Saved: Step_5_Discovery/pattern_library.json
```

---

## Benefits

**1. Organization**
- Clear separation of pipeline stages
- Easy to find specific artifacts
- Logical flow matches pipeline stages

**2. Debugging**
- Can inspect outputs at each step
- Track data flow through pipeline
- Identify where issues occur

**3. Selective Re-running**
- Can restart from any step
- Intermediate artifacts preserved
- No need to re-run entire pipeline

**4. Version Control**
- Clear versioning of outputs
- Easy to compare runs
- Track evolution of results

---

## Migration from V1

**V1 Structure (Flat):**
```
runs/run_xyz/
├── graph.json
├── graph_raw.json
├── extraction_stats.json
├── index.json
├── insights.json
├── report.html
└── report.md
```

**V2 Structure (Organized):**
```
runs/run_xyz/
├── Step_1_Loading/
├── Step_2_Extraction/
│   └── extraction_stats.json
├── Step_3_Deduplication/
├── Step_4_GraphBuilding/
│   ├── graph.json
│   └── graph_raw.json
└── Step_5_Discovery/
    ├── index.json
    ├── insights.json
    ├── report.html
    └── report.md
```

**Backward Compatibility:**
- New runs use step-based structure
- Old runs (V1) still loadable
- No breaking changes to APIs

---

## Future Enhancements

**Step 1:**
- Save chunk metadata for debugging
- Chunk overlap visualization

**Step 3:**
- Detailed deduplication report
- Merge decision log
- Confidence boost analysis

**Phase 3-6:**
- Causal metadata extraction logs
- Do-calculus computation traces
- Intervention simulation results

---

**Last Updated:** 2026-03-22

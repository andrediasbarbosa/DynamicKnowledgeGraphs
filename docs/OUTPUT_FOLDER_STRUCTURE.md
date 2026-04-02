# Output Folder Structure

`kg run` writes a timestamped run directory under `runs/` by default.

## Canonical Layout

```text
runs/
└── run_YYYYMMDD_HHMMSS/
    ├── Step_1_Loading/
    ├── Step_2_Extraction/
    │   ├── extraction_stats.json
    │   └── <document_id>_extractions.json
    ├── Step_3_QualityControl/
    │   ├── cleaning_report.json
    │   └── quality_control_report.html
    ├── Step_4_GraphBuilding/
    │   ├── graph.json
    │   └── graph_raw.json            # only when --preprocess is used
    ├── Step_5_Discovery/
    │   ├── index.json
    │   ├── insights.json
    │   ├── augmentation.json
    │   ├── graph.html
    │   ├── graph_augmented.html
    │   ├── graph_rag.html
    │   ├── graph.dot
    │   ├── report.md
    │   ├── report.html
    │   └── pattern_library.json
    ├── manifest.json
    └── README.txt
```

## Notes

- `Step_1_Loading/` is created even when no explicit loading artifacts are written there.
- extraction intermediates live in `Step_2_Extraction/`
- quality-control artifacts live in `Step_3_QualityControl/`
- the cleaned graph that downstream stages consume is `Step_4_GraphBuilding/graph.json`
- index, discovery, render, and report artifacts all live under `Step_5_Discovery/`

## Most Useful Files

For typical inspection:

- `Step_4_GraphBuilding/graph.json`
- `Step_5_Discovery/insights.json`
- `Step_5_Discovery/graph_augmented.html`
- `Step_5_Discovery/report.html`
- `manifest.json`

## Resume Expectations

Resume commands rely on the existing run directory containing the expected artifacts. Example:

```bash
./build/bin/kg run -f 4 -d runs/run_YYYYMMDD_HHMMSS
```

That assumes at least:

- `Step_4_GraphBuilding/graph.json`
- `Step_5_Discovery/index.json`
- `Step_5_Discovery/insights.json`

## Generated Root Outputs

Files such as `output/report.md` and `runs/*/Step_5_Discovery/report.md` are generated run artifacts, not source documentation.

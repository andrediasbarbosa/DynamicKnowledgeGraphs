# V2 Quick Start Guide

**TL;DR:** Causal inference + sliding window chunking upgrade

---

## What's New in V2?

### 1. Causal Inference 🧠
**Before (V1):** "Drug X is associated with symptom relief"
**After (V2):** "Drug X **causes** symptom relief (confidence: 0.87, evidence: experimental, confounders: [age, genetics])"

**Enables queries like:**
- "What causes X?"
- "What if we do(X)?" (intervention)
- "What would have happened if...?" (counterfactual)

### 2. Sliding Window Chunking 📊
**Before (V1):** Fixed chunks miss boundary-spanning relations
**After (V2):** 15% overlap ensures complete relation capture

```
V1: [Chunk 1][Chunk 2][Chunk 3]  ← gaps at boundaries
V2: [Chunk 1]
         [Chunk 2]      ← 15% overlap
              [Chunk 3]
```

---

## Implementation Phases

| Phase | Duration | Key Deliverable |
|-------|----------|-----------------|
| **1. Enhanced Chunking** | 2 weeks | Percentage-based overlap |
| **2. Causal Metadata** | 3 weeks | CausalHypergraph class |
| **3. Causal Extraction** | 4 weeks | LLM causal prompt |
| **4. Causal DAG** | 3 weeks | DAG construction |
| **5. Do-Calculus** | 5 weeks | Intervention queries |
| **6. Discovery Operators** | 4 weeks | Causal chain detection |
| **7. Evaluation** | 3 weeks | Benchmark validation |
| **8. Documentation** | 2 weeks | Publication draft |

**Total:** 26 weeks (~6 months)

---

## Key Files to Create

```
include/causal/
├── causal_metadata.hpp       # Causal edge metadata
├── causal_hypergraph.hpp     # Extended hypergraph
├── causal_dag.hpp            # DAG representation
└── do_calculus.hpp           # Intervention logic

src/causal/
└── [implementations]
```

---

## Configuration Example

```json
{
  "chunk_overlap_percentage": 0.15,
  "causal_inference": {
    "enable": true,
    "require_temporal_precedence": true
  }
}
```

---

## Success Metrics

✅ **Chunking:** Capture ≥95% boundary relations (vs 80% in V1)
✅ **Causal Precision:** ≥75% accurate causal edges
✅ **Direction Accuracy:** ≥85% correct X→Y vs Y→X
✅ **Expert Rating:** ≥80% "reasonable" causal claims

---

## Next Actions

1. **Review** [V2_CAUSAL_DESIGN.md](V2_CAUSAL_DESIGN.md) (full spec)
2. **Create branch:** `git checkout -b v2-causal-inference`
3. **Start Phase 1:** Enhanced chunking (2 weeks)
4. **Set up benchmark:** 100 manually annotated papers

---

**Questions?** See full design doc: [V2_CAUSAL_DESIGN.md](V2_CAUSAL_DESIGN.md)

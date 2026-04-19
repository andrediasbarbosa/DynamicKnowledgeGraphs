# Knowledge Graph V2: Causal Inference & Enhanced Chunking

**Version:** 2.0.0-alpha
**Date:** 2026-03-22
**Status:** Phase 2 (Causal Metadata) implemented; Phases 3-8 in progress
**Authors:** Research Team + Claude Sonnet 4.5

---

## Executive Summary

This document outlines a fundamental architectural upgrade to the Knowledge Graph Discovery system to support **causal inference** and **optimized chunking** with sliding windows. The upgrade addresses two critical limitations:

1. **Lack of causal reasoning**: Current system represents correlations but cannot distinguish causation, confounding, or intervention effects
2. **Chunk boundary artifacts**: Fixed chunking misses entities/relations that span chunk boundaries

**Key Goals:**
- Enable Pearl's causal hierarchy (association → intervention → counterfactual)
- Implement percentage-based sliding window chunking (15% default overlap)
- Extract and represent causal metadata from scientific literature
- Support do-calculus queries and backdoor/frontdoor adjustment

---

## 1. Motivation & Background

### 1.1 The Causal Inference Gap

**Current Limitation:**
The existing hypergraph represents **associative relationships** extracted from text:
```
(Entity1) --[relation]--> (Entity2)
```

This captures patterns like:
- "X is correlated with Y"
- "X co-occurs with Y"
- "X and Y are mentioned together"

But **cannot distinguish**:
- **Causation**: X causes Y (X → Y)
- **Confounding**: Z causes both X and Y (X ← Z → Y)
- **Mediation**: X causes Y through M (X → M → Y)
- **Colliders**: X and Y both cause Z (X → Z ← Y)
- **Interventional effects**: What happens if we *force* X to a value?

**Why This Matters:**
Scientific reasoning requires understanding **mechanisms**, not just associations. Example from medicine:
- Association: "Patients taking drug X have better outcomes"
- Causal: "Drug X *causes* better outcomes" (vs confounded by healthier patients choosing the drug)

### 1.2 Gary Marcus's "AI Chasm" Connection

This upgrade directly addresses Marcus's critique in *Rebooting AI*:

| Marcus's Gap | How V2 Bridges It |
|--------------|-------------------|
| **Robust knowledge representation** | Causal DAGs + hypergraphs |
| **Causal understanding** | Pearl's 3-level hierarchy |
| **Compositional reasoning** | Do-calculus + path analysis |
| **Explainability** | Traceable causal paths |
| **Hybrid neuro-symbolic** | LLM extraction → symbolic causal graph |

**From Pattern Matching → Reasoning About Mechanisms**

### 1.3 The Chunking Problem

**Current Behavior:**
Text is split into non-overlapping chunks (500 chars, page-based, etc.)

**Problem:**
Relations spanning chunk boundaries are missed:

```
Chunk 1: "...recent studies on climate change have shown that increased CO2 levels"
                                                                              ↓ [BOUNDARY]
Chunk 2: "lead to ocean acidification, which threatens coral reef ecosystems..."
```

The relation `(CO2, causes, ocean acidification)` is **lost** because it spans the boundary.

**Solution:**
Sliding window with 15% overlap ensures cross-boundary relations are captured in at least one chunk.

---

## 2. Causal Inference Architecture

### 2.1 Causal Metadata Schema

Each hyperedge gets augmented with **causal metadata**:

```json
{
  "edge_id": "e_12345",
  "nodes": ["climate_change", "sea_level_rise", "coastal_flooding"],
  "relation": "causes",

  "causal_metadata": {
    "causal_type": "direct_cause",         // Type of causal relationship
    "direction": ["climate_change", "sea_level_rise"],  // Cause → Effect
    "confidence": 0.87,                     // Extraction confidence
    "evidence_type": "experimental",        // observational | experimental | theoretical
    "temporal_order": "climate_before_sea_level",  // Temporal precedence
    "mechanism": "thermal_expansion",       // Mediating mechanism
    "confounders": ["natural_variability"], // Potential confounders
    "effect_size": "large",                 // small | medium | large | unknown
    "source_text": "Rising global temperatures cause thermal expansion...",
    "source_chunk": "doc_001_chunk_5",
    "extraction_method": "llm_causal_prompt_v2"
  },

  "provenance": {
    "document_id": "ipcc_ar6_2021",
    "page": 42,
    "chunk_id": "doc_001_chunk_5",
    "timestamp": "2026-03-22T10:30:00Z"
  }
}
```

### 2.2 Causal Relationship Types

| Type | Symbol | Description | Example |
|------|--------|-------------|---------|
| **Direct Cause** | X → Y | X directly causes Y | Drug → Symptom Relief |
| **Indirect Cause** | X → M → Y | X causes Y through mediator M | Exercise → Endorphins → Mood |
| **Common Cause (Confounder)** | X ← Z → Y | Z causes both X and Y | Genetics → (Height, Shoe Size) |
| **Collider** | X → Z ← Y | Both X and Y cause Z | Talent → Success ← Effort |
| **Bidirectional** | X ↔ Y | Mutual causation / feedback loop | Poverty ↔ Poor Health |
| **Preventive** | X ⊸ Y | X prevents Y | Vaccine ⊸ Disease |
| **Necessary Cause** | X ⇒ Y | Y requires X (but X doesn't guarantee Y) | Oxygen ⇒ Fire |
| **Sufficient Cause** | X ⇒! Y | X guarantees Y | Decapitation ⇒! Death |

### 2.3 Pearl's Causal Hierarchy Implementation

**Level 1: Association** (already supported)
- Query: "What is P(Y | X)?" - Probability of Y given we observe X
- Example: "What fraction of patients on drug X recover?"
- Operations: Conditional probability, correlation

**Level 2: Intervention** (NEW in V2)
- Query: "What is P(Y | do(X))?" - Probability of Y if we *force* X
- Example: "What if we *give* drug X to patients (vs observing who takes it)?"
- Operations: do-calculus, backdoor adjustment, frontdoor adjustment

**Level 3: Counterfactual** (NEW in V2)
- Query: "What would have happened if X had been different?"
- Example: "Would patient Z have survived if given drug X instead of placebo?"
- Operations: Potential outcomes, twin networks

### 2.4 Causal Graph Representation

**Augment Hypergraph with Causal DAG:**

```cpp
class CausalHypergraph : public Hypergraph {
public:
    // Causal-specific queries
    std::vector<NodeID> get_causes(NodeID effect);
    std::vector<NodeID> get_effects(NodeID cause);
    std::vector<NodeID> get_confounders(NodeID x, NodeID y);
    std::vector<NodeID> get_mediators(NodeID cause, NodeID effect);

    // Do-calculus operations
    double intervention_probability(NodeID intervention_node,
                                    const std::map<NodeID, double>& values);

    // Backdoor criterion
    bool has_backdoor_path(NodeID cause, NodeID effect);
    std::vector<NodeID> find_backdoor_adjustment_set(NodeID cause, NodeID effect);

    // Frontdoor criterion
    std::vector<NodeID> find_frontdoor_adjustment_set(NodeID cause, NodeID effect);

    // Path analysis
    std::vector<CausalPath> find_causal_paths(NodeID from, NodeID to);
    bool is_d_separated(NodeID x, NodeID y, const std::vector<NodeID>& conditioning_set);

    // Export causal structure
    json export_causal_dag() const;
    void import_causal_dag(const json& dag);

private:
    std::map<EdgeID, CausalMetadata> causal_metadata_;
    DirectedAcyclicGraph causal_dag_;  // Extracted causal structure
};
```

### 2.5 Causal Extraction from Text

**Enhanced LLM Prompt for Causal Extraction:**

```python
CAUSAL_EXTRACTION_PROMPT = """
Extract causal relationships from the following scientific text.

For each causal relationship, identify:
1. **Cause** (independent variable / treatment / intervention)
2. **Effect** (dependent variable / outcome)
3. **Causal Type**: direct_cause | indirect_cause | confounder | collider | preventive
4. **Evidence Type**: experimental | observational | theoretical | review
5. **Mechanism** (if mentioned): How does the cause lead to the effect?
6. **Confounders** (if mentioned): Variables that might explain the association
7. **Temporal Order**: Does the text establish that cause precedes effect?
8. **Effect Size**: small | medium | large | unknown
9. **Confidence**: Your confidence in the causal claim (0.0 to 1.0)

**Indicators of causation:**
- Explicit causal language: "causes", "leads to", "results in", "induces", "produces"
- Interventional language: "treatment with X resulted in Y", "administration of X led to Y"
- Mechanistic explanations: "X activates Y through pathway Z"
- Temporal precedence: "X occurred before Y"
- Dose-response: "Higher levels of X produce stronger Y"
- Experimental evidence: "randomized controlled trial", "experiment"

**Indicators of mere correlation:**
- "associated with", "correlated with", "linked to", "related to"
- No temporal information
- Observational studies without controls

Text:
{text}

Output as JSON array:
[
  {{
    "cause": "entity1",
    "effect": "entity2",
    "causal_type": "direct_cause",
    "evidence_type": "experimental",
    "mechanism": "mechanism description",
    "confounders": ["confounder1", "confounder2"],
    "temporal_order": "cause_before_effect",
    "effect_size": "large",
    "confidence": 0.85,
    "source_text": "exact quote from text"
  }}
]
"""
```

### 2.6 Causal Discovery Operators

**New operators for V2:**

| Operator | Description | Output |
|----------|-------------|--------|
| **causal_chain_detection** | Find chains X → Y → Z | Causal pathways |
| **confounder_identification** | Find X ← Z → Y patterns | Potential confounders |
| **mediator_analysis** | Find X → M → Y with mediation statistics | Indirect effects |
| **collider_detection** | Find X → Z ← Y (important for d-separation) | Selection bias risks |
| **intervention_simulation** | Estimate P(Y \| do(X)) using adjustment | Predicted intervention effect |
| **counterfactual_reasoning** | "What if X had been different?" | Counterfactual estimates |
| **causal_sufficiency_test** | Check for hidden confounders | Unobserved confounder warnings |
| **temporal_precedence_validation** | Verify cause precedes effect | Temporal consistency check |
| **dose_response_analysis** | Analyze strength of causal effect | Effect size gradients |

---

## 3. Enhanced Sliding Window Chunking

### 3.1 Current vs Proposed Chunking

**Current (V1):**
```cpp
FixedSizeChunking(size_t chunk_size, size_t overlap)
// Example: chunk_size=500, overlap=100 (20% of chunk_size)
```

**Limitations:**
- Overlap specified in **characters**, not percentage
- Not configurable via pipeline config
- Optimal overlap varies by document type

**Proposed (V2):**
```cpp
struct ChunkingConfig {
    size_t chunk_size = 500;              // Characters per chunk
    double overlap_percentage = 0.15;     // 15% overlap (default)
    bool adaptive_overlap = false;        // Future: adjust based on content
    std::string boundary_strategy = "word"; // word | sentence | paragraph
};
```

### 3.2 Overlap Percentage Calculation

**Formula:**
```
overlap_chars = chunk_size * overlap_percentage
next_chunk_start = current_chunk_end - overlap_chars
```

**Example with 15% overlap:**
```
chunk_size = 500
overlap_percentage = 0.15
overlap_chars = 500 * 0.15 = 75 characters

Chunk 1: chars 0-500     (500 chars)
Chunk 2: chars 425-925   (500 chars, starts at 500-75=425)
Chunk 3: chars 850-1350  (500 chars, starts at 925-75=850)
```

**Overlap region visualization:**
```
Chunk 1: [====================]
                    [overlap]
Chunk 2:            [====================]
                               [overlap]
Chunk 3:                       [====================]
```

### 3.3 Deduplication Strategy for Overlapping Extractions

**Problem:** Overlapping chunks may extract the same entity/relation multiple times.

**Solution:** Track extraction provenance and merge duplicates

```cpp
struct ExtractionProvenance {
    std::string chunk_id;
    size_t start_char;
    size_t end_char;
    double confidence;
};

struct Entity {
    std::string label;
    std::string type;
    std::vector<ExtractionProvenance> extractions;  // All chunks where found

    // Merge duplicate entities
    void merge_with(const Entity& other) {
        extractions.insert(extractions.end(),
                          other.extractions.begin(),
                          other.extractions.end());
        // Keep highest confidence
        confidence = std::max(confidence, other.confidence);
    }
};
```

**Deduplication Algorithm:**
1. Extract relations from all chunks (including overlaps)
2. For each entity pair `(e1, e2)`:
   - If relation found in multiple chunks → merge, keep highest confidence
   - Track all source chunks in provenance
3. For overlapping text spans:
   - Keep extraction with highest confidence
   - Record all supporting chunks

**Benefits:**
- Increases recall (captures boundary-spanning relations)
- Maintains precision (deduplication prevents inflated counts)
- Enables confidence boosting (multiple extractions increase confidence)

### 3.4 Configuration Integration

**Add to `PipelineConfig`:**
```cpp
struct PipelineConfig {
    // ... existing fields ...

    // Enhanced Chunking Configuration (V2)
    double chunk_overlap_percentage = 0.15;  // Default 15% overlap
    bool enable_overlap_deduplication = true;
    double overlap_confidence_boost = 0.1;   // Boost confidence if found in overlap
    std::string overlap_strategy = "sliding_window";  // sliding_window | hierarchical
};
```

**JSON config example:**
```json
{
  "chunking_strategy": "fixed",
  "chunk_size": 500,
  "chunk_overlap_percentage": 0.15,
  "enable_overlap_deduplication": true,
  "overlap_confidence_boost": 0.1
}
```

---

## 4. Implementation Architecture

### 4.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     V2 Architecture                          │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────┐     ┌──────────────────┐                   │
│  │   PDF       │────▶│  Enhanced        │                   │
│  │  Processor  │     │  Chunking        │                   │
│  └─────────────┘     │  (15% overlap)   │                   │
│                      └────────┬─────────┘                   │
│                               │                              │
│                               ▼                              │
│                      ┌──────────────────┐                   │
│                      │  Causal LLM      │                   │
│                      │  Extractor       │                   │
│                      │  (New Prompt)    │                   │
│                      └────────┬─────────┘                   │
│                               │                              │
│                               ▼                              │
│                      ┌──────────────────┐                   │
│                      │  Overlap         │                   │
│                      │  Deduplicator    │                   │
│                      └────────┬─────────┘                   │
│                               │                              │
│                               ▼                              │
│  ┌────────────────────────────────────────────────┐         │
│  │         Causal Hypergraph Builder              │         │
│  │  - Augment edges with causal metadata          │         │
│  │  - Build causal DAG                             │         │
│  │  - Validate temporal consistency                │         │
│  └────────────────┬───────────────────────────────┘         │
│                   │                                          │
│                   ▼                                          │
│  ┌────────────────────────────────────────────────┐         │
│  │         Causal Discovery Engine                 │         │
│  │  - Causal chain detection                       │         │
│  │  - Confounder identification                    │         │
│  │  - Intervention simulation (do-calculus)        │         │
│  │  - Counterfactual reasoning                     │         │
│  └────────────────┬───────────────────────────────┘         │
│                   │                                          │
│                   ▼                                          │
│  ┌────────────────────────────────────────────────┐         │
│  │         Causal Query Interface                  │         │
│  │  - "What causes X?"                             │         │
│  │  - "What if we do(X)?"                          │         │
│  │  - "What would have happened if...?"            │         │
│  └─────────────────────────────────────────────────┘         │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Data Flow

```
PDF Document
    │
    ▼
[Text Extraction]
    │
    ▼
[Sliding Window Chunking (15% overlap)]
    │
    ├─▶ Chunk 1: chars 0-500
    ├─▶ Chunk 2: chars 425-925    (75 char overlap with Chunk 1)
    ├─▶ Chunk 3: chars 850-1350   (75 char overlap with Chunk 2)
    │
    ▼
[Causal LLM Extraction] (parallel for all chunks)
    │
    ├─▶ Chunk 1 → Relations₁ (with causal metadata)
    ├─▶ Chunk 2 → Relations₂ (with causal metadata)
    ├─▶ Chunk 3 → Relations₃ (with causal metadata)
    │
    ▼
[Overlap Deduplication]
    │
    ├─▶ Merge duplicate (e1, r, e2) tuples
    ├─▶ Aggregate confidence scores
    ├─▶ Track all source chunks in provenance
    │
    ▼
[Causal Hypergraph Construction]
    │
    ├─▶ Create nodes for entities
    ├─▶ Create hyperedges with causal metadata
    ├─▶ Build causal DAG from causal edges
    ├─▶ Validate temporal consistency
    │
    ▼
[Causal Discovery Operators]
    │
    ├─▶ Detect causal chains (X → Y → Z)
    ├─▶ Identify confounders (X ← Z → Y)
    ├─▶ Find mediators (X → M → Y)
    ├─▶ Simulate interventions (do(X))
    │
    ▼
[Query Interface / RAG]
    │
    └─▶ "What causes climate change?"
        "What if we reduce CO2 emissions?"
        "Would X have prevented Y?"
```

### 4.3 File Structure for V2

```
include/
├── causal/
│   ├── causal_metadata.hpp       # Causal metadata structures
│   ├── causal_hypergraph.hpp     # CausalHypergraph class
│   ├── causal_dag.hpp             # DAG representation
│   ├── do_calculus.hpp            # Intervention operations
│   └── causal_discovery.hpp       # Causal discovery operators
├── chunking/
│   ├── sliding_window_chunker.hpp # Enhanced chunking
│   └── overlap_deduplicator.hpp   # Deduplication logic
└── pipeline/
    └── causal_pipeline.hpp         # V2 pipeline

src/
├── causal/
│   ├── causal_metadata.cpp
│   ├── causal_hypergraph.cpp
│   ├── causal_dag.cpp
│   ├── do_calculus.cpp
│   └── causal_discovery.cpp
├── chunking/
│   ├── sliding_window_chunker.cpp
│   └── overlap_deduplicator.cpp
└── pipeline/
    └── causal_pipeline.cpp
```

---

## 5. Example Use Cases

### 5.1 Medical Research: Drug Side Effects

**Scenario:** Analyze papers on drug safety

**Current (V1):**
```
Query: "What is associated with Drug X?"
Result: {nausea, headache, liver_damage, patient_recovery}
Problem: Can't tell if drug *causes* side effects or just correlated
```

**V2 with Causal Inference:**
```
Query: "What does Drug X cause?"
Result:
  - Drug X → Nausea (confidence: 0.92, evidence: experimental, effect_size: medium)
  - Drug X → Liver Damage (confidence: 0.78, evidence: observational, effect_size: small)
  - Drug X ⊸ Tumor Growth (preventive, confidence: 0.85)

Confounders detected:
  - Patient Age ← Drug X → Liver Damage (age confounds liver damage)

Intervention query: "What if we give Drug X to all patients?"
P(Nausea | do(Drug X)) = 0.35 (vs P(Nausea | observe X) = 0.28)
→ Causal effect is stronger than observed correlation (selection bias)
```

### 5.2 Climate Science: Carbon Emissions

**Scenario:** Literature review on climate change mechanisms

**V2 Query:**
```
"What is the causal chain from CO2 emissions to coral bleaching?"

Result:
CO2 Emissions
  → Atmospheric CO2 ↑ (mechanism: combustion)
  → Ocean Absorption (mechanism: gas exchange)
  → Ocean Acidification (mechanism: carbonic acid formation)
  → Coral Bleaching (mechanism: calcium carbonate dissolution)

Mediators: [Atmospheric CO2, Ocean Absorption, Ocean Acidification]
Confounders: [Natural Climate Variability, El Niño]
Evidence: 87% experimental, 13% observational
Total effect size: Large (d = 2.3)

Counterfactual: "What if CO2 emissions were halved in 2000?"
Estimated reduction in coral bleaching: 42% (95% CI: 31-53%)
```

### 5.3 Educational Research: Learning Interventions

**Scenario:** Meta-analysis of teaching methods

**V2 Query:**
```
"What interventions cause improved student outcomes?"

Results ranked by causal effect size:
1. Active Learning → Test Scores (d = 0.47, experimental evidence)
2. Spaced Repetition → Long-term Retention (d = 0.38, experimental)
3. Metacognitive Training → Problem Solving (d = 0.32, quasi-experimental)

Confounders to adjust for:
- Student Motivation (affects both intervention adherence and outcomes)
- Prior Knowledge (pre-existing advantage)

Do-calculus backdoor adjustment:
P(High Scores | do(Active Learning)) = 0.73
  vs P(High Scores | observe Active Learning) = 0.68
→ True causal effect larger than naive observation (motivated students more likely to receive active learning)
```

---

## 6. Implementation Roadmap

### Phase 1: Enhanced Chunking (2 weeks)
**Goal:** Implement percentage-based sliding window chunking

**Tasks:**
- [ ] Modify `FixedSizeChunking` to support `overlap_percentage`
- [ ] Add `chunk_overlap_percentage` to `PipelineConfig`
- [ ] Implement overlap deduplication algorithm
- [ ] Add provenance tracking for overlapping extractions
- [ ] Unit tests for boundary cases
- [ ] Benchmark recall improvement on test corpus

**Deliverables:**
- Updated `pdf_processor.cpp` with percentage-based overlap
- Deduplication logic in `extraction_pipeline.cpp`
- Performance comparison report (recall, precision, runtime)

### Phase 2: Causal Metadata Schema (3 weeks)
**Goal:** Define and implement causal metadata structures

**Tasks:**
- [ ] Define `CausalMetadata` struct (causal_metadata.hpp)
- [ ] Extend `Hypergraph` → `CausalHypergraph`
- [ ] Implement causal edge type enum
- [ ] Add temporal precedence validation
- [ ] JSON serialization/deserialization
- [ ] Integrate into existing extraction pipeline

**Deliverables:**
- `include/causal/causal_metadata.hpp`
- `include/causal/causal_hypergraph.hpp`
- Backward-compatible with V1 graphs

### Phase 3: Causal Extraction (4 weeks)
**Goal:** Extract causal relationships from text using LLMs

**Tasks:**
- [ ] Design causal extraction prompt (see §2.5)
- [ ] Implement causal indicator detection (heuristics)
- [ ] Add evidence type classification (experimental vs observational)
- [ ] Extract temporal ordering from text
- [ ] Identify confounders and mediators
- [ ] Confidence calibration (validate against ground truth)
- [ ] A/B test: causal prompt vs standard extraction

**Deliverables:**
- Enhanced LLM prompt for causal extraction
- Extraction accuracy evaluation on benchmark dataset
- Confidence calibration curves

### Phase 4: Causal DAG Construction (3 weeks)
**Goal:** Build directed acyclic graph from causal edges

**Tasks:**
- [ ] Implement `CausalDAG` class with topological sorting
- [ ] Cycle detection and resolution (warn user)
- [ ] d-separation algorithm (check conditional independence)
- [ ] Export to standard formats (DOT, GraphML)
- [ ] Visualization integration (Graphviz rendering)

**Deliverables:**
- `include/causal/causal_dag.hpp`
- Cycle detection with user warnings
- Export to Graphviz DOT format

### Phase 5: Do-Calculus & Intervention (5 weeks)
**Goal:** Enable intervention queries using do-calculus

**Tasks:**
- [ ] Implement backdoor criterion check
- [ ] Implement frontdoor criterion check
- [ ] Backdoor adjustment formula (Pearl 1995)
- [ ] Frontdoor adjustment formula
- [ ] Propensity score estimation (for adjustment)
- [ ] Intervention effect estimation
- [ ] Sensitivity analysis (hidden confounders)

**Deliverables:**
- `include/causal/do_calculus.hpp`
- Intervention query API: `estimate_effect(do(X), Y, data)`
- Unit tests with synthetic causal graphs

### Phase 6: Causal Discovery Operators (4 weeks)
**Goal:** Implement automated causal discovery operators

**Tasks:**
- [ ] Causal chain detection (`causal_chain_detection`)
- [ ] Confounder identification (`confounder_identification`)
- [ ] Mediator analysis (`mediator_analysis`)
- [ ] Collider detection (`collider_detection`)
- [ ] Dose-response analysis (`dose_response_analysis`)
- [ ] Integrate into `DiscoveryEngine`

**Deliverables:**
- 5+ new causal discovery operators
- Integration into existing operator framework
- HTML report section for causal insights

### Phase 7: Evaluation & Validation (3 weeks)
**Goal:** Validate causal extraction quality

**Tasks:**
- [ ] Create gold-standard causal benchmark (100 papers, manually annotated)
- [ ] Measure precision/recall of causal extraction
- [ ] Compare intervention estimates to known effects (e.g., medical RCTs)
- [ ] User study with domain experts (rate causal claims)
- [ ] Calibration analysis (does confidence match accuracy?)

**Deliverables:**
- Benchmark dataset with ground truth causal relations
- Evaluation report with precision/recall curves
- User study results

### Phase 8: Documentation & Publication (2 weeks)
**Goal:** Document V2 and prepare publication

**Tasks:**
- [ ] Update user documentation (docs/)
- [ ] API reference for causal methods
- [ ] Tutorial notebook: "Causal Inference with Knowledge Graphs"
- [ ] Write V2 publication draft (extend existing paper)
- [ ] Prepare demo for conference presentation

**Deliverables:**
- Comprehensive V2 documentation
- Tutorial Jupyter notebook
- Extended publication draft

**Total Timeline:** ~26 weeks (6-7 months)

---

## 7. Technical Challenges & Mitigations

### 7.1 Challenge: Causal Extraction Accuracy

**Problem:** LLMs may hallucinate causal relationships or miss subtle confounders

**Mitigations:**
1. **Multi-model ensemble**: Extract with GPT-4, Claude, Gemini, vote on consensus
2. **Confidence calibration**: Validate confidence scores against ground truth
3. **Evidence type weighting**: Prioritize experimental > observational > theoretical
4. **Human-in-the-loop**: Flag low-confidence causal claims for expert review
5. **Temporal validation**: Require temporal precedence for causal claims

### 7.2 Challenge: Computational Cost of Overlap

**Problem:** 15% overlap increases extraction calls by ~15%

**Mitigations:**
1. **Batch processing**: Send multiple chunks per API call (where supported)
2. **Caching**: Cache entity extractions within overlapping regions
3. **Adaptive overlap**: Reduce overlap for low-density text (e.g., references)
4. **Parallel processing**: Distribute chunks across workers

**Cost Analysis:**
```
V1: 100 chunks → 100 LLM calls
V2: 100 chunks + 15 overlap chunks → 115 LLM calls (+15% cost)

Benefit: Recall improvement (estimated +10-20% for boundary relations)
Trade-off: Justified if research completeness is critical
```

### 7.3 Challenge: Cyclic Causation

**Problem:** Feedback loops (X → Y → X) violate DAG assumption

**Example:** Poverty → Poor Health → Poverty

**Mitigations:**
1. **Temporal slicing**: Represent as DAG over time (Poverty_t → Health_t+1 → Poverty_t+2)
2. **Warn user**: Flag cycles and suggest temporal interpretation
3. **Equilibrium analysis**: Use structural equation models (SEMs) instead of DAGs

### 7.4 Challenge: Hidden Confounders

**Problem:** Unobserved variables bias causal estimates

**Mitigations:**
1. **Sensitivity analysis**: "How strong would confounder need to be to reverse conclusion?"
2. **Instrumental variables**: Use natural experiments where available
3. **Propensity score matching**: Balance observed confounders
4. **Transparency**: Report assumptions clearly ("Assumes no unobserved confounders")

### 7.5 Challenge: Backward Compatibility

**Problem:** V2 changes may break existing pipelines

**Mitigations:**
1. **Feature flags**: Enable causal features opt-in (`enable_causal_inference=false` by default)
2. **Graceful degradation**: V2 can read V1 graphs (treat as non-causal)
3. **Migration tool**: Convert V1 graphs to V2 format
4. **Versioned config**: `pipeline_version: "2.0"` in config files

---

## 8. Evaluation Metrics

### 8.1 Chunking Quality

| Metric | Description | Target |
|--------|-------------|--------|
| **Boundary Recall** | % of relations spanning boundaries captured | >95% |
| **Dedup Precision** | % of merged duplicates that are true duplicates | >90% |
| **Runtime Overhead** | Additional time for overlap processing | <20% |
| **Cost Overhead** | Additional LLM API cost | <20% |

### 8.2 Causal Extraction Quality

| Metric | Description | Target |
|--------|-------------|--------|
| **Causal Precision** | % of extracted causal edges that are true causal | >75% |
| **Causal Recall** | % of true causal edges extracted | >60% |
| **Direction Accuracy** | % of edges with correct direction (X→Y vs Y→X) | >85% |
| **Confounder Detection** | % of known confounders identified | >50% |
| **Confidence Calibration** | Brier score (lower = better calibrated) | <0.15 |

### 8.3 Do-Calculus Accuracy

| Metric | Description | Target |
|--------|-------------|--------|
| **Intervention Estimate Error** | RMSE between estimated and true do(X) effect | <0.15 |
| **Backdoor Set Precision** | % of valid backdoor sets identified | >80% |
| **Counterfactual Accuracy** | Agreement with expert judgment | >70% |

---

## 9. Example Configuration

**V2 Pipeline Config (causal_config.json):**
```json
{
  "pipeline_version": "2.0",

  "llm_provider": "openai",
  "llm_model": "gpt-4-turbo",
  "llm_temperature": 0.0,
  "llm_max_tokens": 3000,

  "chunking_strategy": "fixed",
  "chunk_size": 600,
  "chunk_overlap_percentage": 0.15,
  "enable_overlap_deduplication": true,
  "overlap_confidence_boost": 0.1,

  "causal_inference": {
    "enable": true,
    "extraction_mode": "causal_prompt_v2",
    "require_temporal_precedence": true,
    "min_confidence_threshold": 0.6,
    "enable_confounder_detection": true,
    "enable_mediator_detection": true,
    "validate_dag_acyclicity": true,
    "warn_on_cycles": true
  },

  "do_calculus": {
    "enable": true,
    "adjustment_method": "backdoor",
    "propensity_score_bins": 10,
    "sensitivity_analysis": true
  },

  "deduplication": {
    "enable": true,
    "similarity_threshold": 0.85,
    "merge_strategy": "highest_confidence"
  },

  "output": {
    "save_causal_dag": true,
    "export_graphviz": true,
    "include_intervention_estimates": true
  }
}
```

---

## 10. Success Criteria

**V2 is successful if:**

✅ **Chunking:**
- Captures ≥95% of boundary-spanning relations (vs 80% in V1)
- Overhead <20% in runtime and cost

✅ **Causal Extraction:**
- Achieves ≥75% precision on causal edge identification
- ≥85% direction accuracy (X→Y vs Y→X)
- Expert users rate causal claims as "reasonable" >80% of the time

✅ **Do-Calculus:**
- Intervention estimates within 15% RMSE of ground truth (where available)
- Identifies valid adjustment sets ≥80% of the time

✅ **Integration:**
- Backward compatible with V1 (can disable causal features)
- Adds <50% complexity to codebase
- Maintains <2s/page extraction speed

✅ **Impact:**
- Enables novel research questions (counterfactuals, interventions)
- Publishable contribution (addresses Marcus's "AI chasm")
- Adopted by ≥3 external research groups (open-source impact)

---

## 11. Related Work & Positioning

### 11.1 Causal Discovery from Text

**Existing Systems:**
- **CauseNet** (Heindorf et al. 2020): Causal KB from Wikipedia, but limited to explicit causal phrases
- **COPEN** (Zhang et al. 2022): Causality extraction from news articles, but no do-calculus
- **BioCause** (Mihăilă et al. 2013): Biomedical causal relations, domain-specific

**Our Contribution:**
- General-purpose (any domain)
- Full Pearl hierarchy (association + intervention + counterfactual)
- Hypergraph representation (multi-way causal relations)
- Integrated into discovery pipeline

### 11.2 Chunking Strategies

**Existing Approaches:**
- **LangChain**: Supports overlap but not percentage-based
- **LlamaIndex**: Hierarchical chunking, no causal metadata
- **Semantic chunking** (Cheng et al. 2024): Uses embeddings, computationally expensive

**Our Contribution:**
- Percentage-based overlap (more intuitive than fixed chars)
- Provenance tracking for overlaps
- Confidence boosting for multiple extractions

### 11.3 Competitive Positioning

| System | Causal Inference | Overlap Chunking | Do-Calculus | Open Source |
|--------|------------------|------------------|-------------|-------------|
| **Semantic Scholar** | ❌ | ❌ | ❌ | ❌ |
| **CauseNet** | ✅ (limited) | ❌ | ❌ | ✅ |
| **LangChain RAG** | ❌ | ✅ | ❌ | ✅ |
| **Our V2** | ✅ | ✅ | ✅ | ✅ |

**Unique Combination:** Only system with causal inference + do-calculus + hypergraphs + open source

---

## 12. References & Further Reading

**Pearl's Causal Inference:**
- Pearl, J. (2009). *Causality: Models, Reasoning, and Inference* (2nd ed.). Cambridge University Press.
- Pearl, J., & Mackenzie, D. (2018). *The Book of Why*. Basic Books.

**Causal Discovery from Text:**
- Heindorf, S., et al. (2020). "CauseNet: Towards a Causality Graph Extracted from the Web." CIKM.
- Zhang, H., et al. (2022). "COPEN: Probing Conceptual Knowledge in Pre-trained Language Models." EMNLP.

**Chunking Strategies:**
- Cheng, D., et al. (2024). "Semantic Chunking for Retrieval-Augmented Generation." arXiv.

**Structural Causal Models:**
- Schölkopf, B., et al. (2021). "Toward Causal Representation Learning." IEEE.

**Gary Marcus - AI Chasm:**
- Marcus, G., & Davis, E. (2019). *Rebooting AI: Building Artificial Intelligence We Can Trust*. Pantheon.

---

## 13. Next Steps

**Immediate Actions (Week 1):**
1. Done: Review and approve design document
2. Done: Set up V2 development branch (`v2-causal-inference`)
3. Done: `CausalMetadata` struct and `CausalRelationType` enum implemented (`include/llm/causal_metadata.hpp`)
4. Done: `--causal` extraction flag integrated into `kg run`
5. Done: Causal discovery operators (`causal_chains`, `intervention_points`, `feedback_loops`, `confounders`) added to registry
6. Pending: Enhanced chunking with percentage-based overlap (Phase 1)
7. Pending: Causal DAG construction and do-calculus (Phases 4-5)
8. Pending: Benchmark dataset for causal extraction evaluation (Phase 7)

**Decision Points:**
- **Overlap percentage**: Confirm 15% as default or run ablation study (10%, 15%, 20%)?
- **Causal extraction model**: GPT-4 Turbo vs Claude Opus 4.6 vs ensemble?
- **Publication venue**: Target WWW 2027 or AAAI 2027 for V2 paper?

**Questions for Discussion:**
1. Should we support temporal causal graphs (X_t → Y_t+1) or just static DAGs?
2. How to handle missing data in intervention estimates?
3. Should counterfactuals be MVP or deferred to V2.1?

---

**Document Status:** ✅ Ready for Review
**Approval Required From:** Research Lead, Technical Architect
**Next Review Date:** 2026-03-29 (1 week)

---

*This design document is a living specification. Updates will be versioned and tracked in `docs/V2_CAUSAL_DESIGN.md`.*

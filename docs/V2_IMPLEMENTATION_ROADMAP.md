# V2 Implementation Roadmap

**Timeline:** 26 weeks (Mar 2026 - Sep 2026)
**Team:** 2-3 developers + 1 domain expert
**Budget:** ~$15K LLM API costs + compute

---

## Gantt Chart Overview

```
Week:  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26
       ├──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┼──┤

Phase 1: Enhanced Chunking
       [████████]

Phase 2: Causal Metadata
                [████████████]

Phase 3: Causal Extraction
                            [████████████████]

Phase 4: Causal DAG
                                            [████████████]

Phase 5: Do-Calculus
                                                        [████████████████████]

Phase 6: Discovery Operators
                                                                    [████████████████]

Phase 7: Evaluation
                                                                                [████████████]

Phase 8: Documentation
                                                                                        [████████]

Milestones:
       M1      M2          M3              M4                  M5                      M6      M7
       │       │           │               │                   │                       │       │
     Chunk  Metadata   Extraction       DAG             Do-Calculus            Operators   Launch
```

**Milestones:**
- **M1 (Week 2):** Chunking demo with 15% overlap
- **M2 (Week 5):** Causal metadata schema finalized
- **M3 (Week 9):** First causal extractions from LLM
- **M4 (Week 12):** DAG visualization working
- **M5 (Week 17):** First intervention query works
- **M6 (Week 23):** All operators integrated
- **M7 (Week 26):** V2.0 release + publication submission

---

## Phase Dependencies

```
                    ┌─────────────────┐
                    │  Phase 1:       │
                    │  Enhanced       │
                    │  Chunking       │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  Phase 2:       │
                    │  Causal         │
                    │  Metadata       │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                    ▼                 ▼
           ┌─────────────────┐  ┌─────────────────┐
           │  Phase 3:       │  │  Phase 4:       │
           │  Causal         │  │  Causal DAG     │
           │  Extraction     │  │  Construction   │
           └────────┬────────┘  └────────┬────────┘
                    │                     │
                    └──────────┬──────────┘
                               │
                               ▼
                    ┌─────────────────┐
                    │  Phase 5:       │
                    │  Do-Calculus    │
                    │  & Intervention │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                    ▼                 ▼
           ┌─────────────────┐  ┌─────────────────┐
           │  Phase 6:       │  │  Phase 7:       │
           │  Discovery      │  │  Evaluation     │
           │  Operators      │  │  & Validation   │
           └────────┬────────┘  └────────┬────────┘
                    │                     │
                    └──────────┬──────────┘
                               │
                               ▼
                    ┌─────────────────┐
                    │  Phase 8:       │
                    │  Documentation  │
                    │  & Publication  │
                    └─────────────────┘
```

---

## Detailed Phase Breakdown

### Phase 1: Enhanced Chunking (Weeks 1-2)

**Goals:**
- Percentage-based sliding window
- Deduplication of overlapping extractions
- Provenance tracking

**Tasks:**
```
Week 1:
  ☐ Modify FixedSizeChunking to accept overlap_percentage
  ☐ Update PipelineConfig with chunk_overlap_percentage
  ☐ Implement overlap calculation logic
  ☐ Unit tests for edge cases (EOF, small documents)

Week 2:
  ☐ Implement overlap deduplication algorithm
  ☐ Add provenance tracking to ExtractionResult
  ☐ Benchmark: V1 vs V2 recall on boundary relations
  ☐ Code review + merge to v2 branch
```

**Deliverables:**
- ✅ `pdf_processor.cpp` updated
- ✅ Benchmark report: V2 recall improvement
- ✅ Unit tests passing

**Risk:** Deduplication logic complexity
**Mitigation:** Start with simple threshold-based merging, iterate

---

### Phase 2: Causal Metadata (Weeks 3-5)

**Goals:**
- Define CausalMetadata struct
- Extend Hypergraph → CausalHypergraph
- JSON serialization

**Tasks:**
```
Week 3:
  ☐ Design CausalMetadata schema (see V2_CAUSAL_DESIGN.md §2.1)
  ☐ Create include/causal/causal_metadata.hpp
  ☐ Implement to_json() / from_json() methods
  ☐ Unit tests for serialization roundtrip

Week 4:
  ☐ Create CausalHypergraph class (extends Hypergraph)
  ☐ Add causal_metadata_ map<EdgeID, CausalMetadata>
  ☐ Implement get_causes(), get_effects() methods
  ☐ Backward compatibility: V1 graphs load without causal metadata

Week 5:
  ☐ Integration with existing pipeline
  ☐ Add causal_type enum (direct_cause, confounder, etc.)
  ☐ Temporal precedence validation logic
  ☐ Code review + documentation
```

**Deliverables:**
- ✅ `causal_metadata.hpp` + `.cpp`
- ✅ `causal_hypergraph.hpp` + `.cpp`
- ✅ Backward compatible with V1

**Risk:** Schema changes breaking existing code
**Mitigation:** Feature flag to disable causal features

---

### Phase 3: Causal Extraction (Weeks 6-9)

**Goals:**
- LLM prompt for causal relationship extraction
- Identify evidence types (experimental vs observational)
- Extract confounders and mechanisms

**Tasks:**
```
Week 6:
  ☐ Design causal extraction prompt (see §2.5)
  ☐ Implement causal indicator detection (regex for "causes", "leads to")
  ☐ Test on 10 sample papers, manual evaluation

Week 7:
  ☐ Add evidence type classification (experimental vs observational)
  ☐ Extract temporal ordering ("X before Y")
  ☐ Identify confounders from text (e.g., "controlled for Z")

Week 8:
  ☐ Mechanism extraction ("X causes Y via Z")
  ☐ Effect size extraction (small/medium/large)
  ☐ Confidence calibration (validate against ground truth)

Week 9:
  ☐ A/B test: causal prompt vs standard extraction
  ☐ Measure precision/recall on benchmark set (50 papers)
  ☐ Optimize prompt based on results
  ☐ Integration into ExtractionPipeline
```

**Deliverables:**
- ✅ Causal extraction prompt (v2)
- ✅ Precision ≥75% on benchmark
- ✅ Integrated into pipeline

**Risk:** LLM hallucinating causal relationships
**Mitigation:** Multi-model ensemble, confidence thresholding

---

### Phase 4: Causal DAG (Weeks 10-12)

**Goals:**
- Build directed acyclic graph from causal edges
- Topological sorting
- Cycle detection

**Tasks:**
```
Week 10:
  ☐ Implement CausalDAG class with adjacency list
  ☐ Topological sort (Kahn's algorithm)
  ☐ Cycle detection (warn user, suggest temporal slicing)

Week 11:
  ☐ d-separation algorithm (Pearl's criterion)
  ☐ Export to GraphViz DOT format
  ☐ Visualization integration (render DAG in report)

Week 12:
  ☐ Import/export to standard formats (JSON, GraphML)
  ☐ Unit tests with synthetic DAGs
  ☐ Performance optimization (large graphs >10K nodes)
```

**Deliverables:**
- ✅ `causal_dag.hpp` + `.cpp`
- ✅ GraphViz export working
- ✅ Cycle detection with warnings

**Risk:** Large graphs slow to process
**Mitigation:** Sparse matrix representation, lazy evaluation

---

### Phase 5: Do-Calculus & Intervention (Weeks 13-17)

**Goals:**
- Backdoor criterion implementation
- Frontdoor criterion implementation
- Intervention effect estimation

**Tasks:**
```
Week 13:
  ☐ Implement backdoor criterion check (Pearl 1995)
  ☐ Find backdoor adjustment sets (search algorithm)
  ☐ Unit tests with known DAG examples

Week 14:
  ☐ Implement frontdoor criterion check
  ☐ Find frontdoor adjustment sets
  ☐ Unit tests

Week 15:
  ☐ Backdoor adjustment formula implementation
  ☐ Propensity score estimation (logistic regression)
  ☐ Test on synthetic data with known ground truth

Week 16:
  ☐ Frontdoor adjustment formula
  ☐ Sensitivity analysis (how strong would confounder need to be?)
  ☐ Error bounds estimation

Week 17:
  ☐ Integration into query interface
  ☐ API: estimate_effect(do(X), Y, graph, data)
  ☐ Validation on real RCT data (if available)
  ☐ Documentation + examples
```

**Deliverables:**
- ✅ `do_calculus.hpp` + `.cpp`
- ✅ Intervention query API
- ✅ RMSE <15% on validation set

**Risk:** Statistical estimation accuracy
**Mitigation:** Cross-validation, bootstrapped confidence intervals

---

### Phase 6: Discovery Operators (Weeks 18-21)

**Goals:**
- Causal chain detection
- Confounder identification
- Mediator analysis
- Collider detection

**Tasks:**
```
Week 18:
  ☐ Implement causal_chain_detection operator
  ☐ Implement confounder_identification operator
  ☐ Add to operator registry

Week 19:
  ☐ Implement mediator_analysis operator
  ☐ Mediation statistics (direct vs indirect effects)
  ☐ Sobel test for mediation significance

Week 20:
  ☐ Implement collider_detection operator
  ☐ Warn about selection bias from colliders
  ☐ Implement dose_response_analysis operator

Week 21:
  ☐ Integration into DiscoveryEngine
  ☐ Add to HTML report rendering
  ☐ Update operator naming and descriptions
  ☐ End-to-end test with real papers
```

**Deliverables:**
- ✅ 5+ new causal operators
- ✅ Integrated into discovery engine
- ✅ HTML report sections

**Risk:** Operator complexity
**Mitigation:** Start with simple versions, iterate

---

### Phase 7: Evaluation & Validation (Weeks 22-24)

**Goals:**
- Gold standard benchmark dataset
- Precision/recall measurement
- User study with domain experts

**Tasks:**
```
Week 22:
  ☐ Create gold standard: 100 papers, manually annotated
  ☐ Annotate causal relationships, confounders, mechanisms
  ☐ Inter-annotator agreement (Krippendorff's alpha >0.7)

Week 23:
  ☐ Run V2 pipeline on benchmark set
  ☐ Measure precision, recall, F1 for causal extraction
  ☐ Measure direction accuracy (X→Y vs Y→X)
  ☐ Measure confounder detection accuracy

Week 24:
  ☐ User study: 5 domain experts rate 50 causal claims
  ☐ Likert scale: "How reasonable is this causal claim?" (1-5)
  ☐ Compare intervention estimates to known RCT results
  ☐ Calibration analysis (Brier score)
  ☐ Write evaluation report
```

**Deliverables:**
- ✅ Benchmark dataset (100 papers)
- ✅ Evaluation report with metrics
- ✅ User study results (≥80% "reasonable")

**Risk:** Low accuracy on benchmark
**Mitigation:** Iterate on prompt, add ensemble methods

---

### Phase 8: Documentation & Publication (Weeks 25-26)

**Goals:**
- User documentation
- API reference
- Publication draft
- Demo preparation

**Tasks:**
```
Week 25:
  ☐ Update README.md with V2 features
  ☐ Write tutorial: "Causal Inference with Knowledge Graphs"
  ☐ API reference for all causal methods
  ☐ Example notebooks (Jupyter)

Week 26:
  ☐ Extend paper/publication_draft.tex with V2 contributions
  ☐ Add evaluation results (§6)
  ☐ Add related work comparison (§2)
  ☐ Prepare demo for conference (5-minute video)
  ☐ Submit to WWW 2027 or AAAI 2027
  ☐ Tag release: v2.0.0
```

**Deliverables:**
- ✅ Complete documentation
- ✅ Tutorial notebook
- ✅ Publication submitted
- ✅ V2.0 release

**Risk:** Submission deadline missed
**Mitigation:** Start writing early, parallel to implementation

---

## Resource Allocation

### Team Composition

```
┌─────────────────────┬──────────────┬─────────────────────────┐
│ Role                │ Allocation   │ Responsibilities        │
├─────────────────────┼──────────────┼─────────────────────────┤
│ Lead Developer      │ 100% (26w)   │ Core implementation     │
│ ML Engineer         │ 75% (20w)    │ LLM prompts, causal ext │
│ Domain Expert       │ 25% (7w)     │ Validation, benchmark   │
│ DevOps              │ 10% (3w)     │ CI/CD, deployment       │
└─────────────────────┴──────────────┴─────────────────────────┘
```

### Budget Estimate

```
LLM API Costs:
  - Benchmark creation (100 papers × 10 chunks × $0.02) = $200
  - Development testing (500 papers × 10 chunks × $0.02) = $1,000
  - Evaluation (1000 papers × 10 chunks × $0.02) = $2,000
  - Prompt iteration (100 experiments × $10) = $1,000
  Subtotal: $4,200

Compute:
  - GPU for embeddings (6 months × $500/month) = $3,000
  - Storage (5TB × $0.02/GB × 6 months) = $600
  Subtotal: $3,600

Personnel (if external):
  - Lead Dev (6 months × $10K/month) = $60K
  - ML Engineer (5 months × $12K/month) = $60K
  - Domain Expert (consult, 7 weeks × $2K/week) = $14K
  Subtotal: $134K

Total (external): ~$142K
Total (internal team): ~$8K (API + compute only)
```

**Assumption:** Using internal research team (typical for PhD project)

---

## Risk Management

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **LLM extraction quality low** | Medium | High | Ensemble models, manual review |
| **Do-calculus accuracy poor** | Medium | Medium | Sensitivity analysis, confidence intervals |
| **Benchmark annotation slow** | High | Medium | Hire annotators, use Mechanical Turk |
| **Performance issues (large graphs)** | Medium | Low | Optimize early, use sparse structures |
| **Timeline slip** | Medium | Medium | Monthly check-ins, cut scope if needed |
| **Publication rejection** | Low | High | Target multiple venues, strong evaluation |

---

## Success Metrics (Reiteration)

| Metric | Target | Measured When |
|--------|--------|---------------|
| **Chunking recall** | ≥95% | Phase 1 |
| **Causal precision** | ≥75% | Phase 7 |
| **Direction accuracy** | ≥85% | Phase 7 |
| **Intervention RMSE** | <15% | Phase 7 |
| **Expert rating** | ≥80% "reasonable" | Phase 7 |
| **Publication accepted** | Yes | Week 30+ (after submission) |

---

## Decision Gates

**Gate 1 (Week 5):** Causal metadata schema approved?
- If NO → Revise schema, delay 1 week

**Gate 2 (Week 9):** Causal extraction precision ≥60%?
- If NO → Iterate on prompt, delay 2 weeks or pivot to rule-based

**Gate 3 (Week 17):** Do-calculus working on synthetic data?
- If NO → Simplify to backdoor adjustment only, defer frontdoor

**Gate 4 (Week 24):** Evaluation metrics meet targets?
- If NO → Delay publication, focus on quality improvements

---

## Contingency Plans

**If timeline slips by >4 weeks:**
- Cut Phase 5 scope: Backdoor adjustment only (no frontdoor)
- Cut Phase 6 scope: 3 operators instead of 5
- Defer Phase 8: Publication to next cycle (Q1 2027)

**If LLM extraction quality <70%:**
- Pivot to rule-based causal extraction (slower, but more accurate)
- Use hybrid: LLM + rules + human review

**If do-calculus accuracy poor:**
- Simplify to qualitative causation (no quantitative estimates)
- Focus on causal structure discovery, defer intervention effects to V3

---

## Communication Plan

**Weekly:**
- Stand-up (30 min): Progress, blockers, next steps
- Slack updates: Daily progress notes

**Bi-weekly:**
- Sprint demo (1 hour): Show working features
- Retrospective (30 min): What went well, what to improve

**Monthly:**
- Stakeholder update (30 min): High-level progress, risks
- Technical deep dive (1 hour): Architecture review

**Milestones:**
- Email announcement to collaborators
- Blog post for M3, M5, M7 (public visibility)

---

## Launch Checklist (Week 26)

```
☐ All unit tests passing (coverage >80%)
☐ Integration tests passing
☐ Benchmark evaluation complete
☐ User documentation published
☐ Tutorial notebook tested
☐ API reference complete
☐ CHANGELOG.md updated
☐ Release notes written
☐ GitHub release tagged (v2.0.0)
☐ Publication submitted
☐ Demo video recorded
☐ Blog post published
☐ Social media announcement
☐ Email to user mailing list
```

---

## Post-Launch (Weeks 27-30)

**Immediate:**
- Monitor bug reports
- Hotfix critical issues within 48h
- Gather user feedback

**1 Month:**
- Retrospective: What went well, what didn't
- Plan V2.1 (bug fixes + minor features)
- Respond to publication reviews

**3 Months:**
- Case studies with external users
- Conference presentation (if accepted)
- Plan V3 features (common sense KB, multi-modal)

---

## V3 Horizon (Q4 2026 - Q2 2027)

**Potential features:**
- Common sense ontology integration (ConceptNet, WordNet)
- Multi-modal extraction (figures, equations, tables)
- Active learning (query experts on uncertain causal claims)
- Temporal causal graphs (X_t → Y_t+1)
- First-principles reasoning (physics-based causal models)

**Research questions:**
- Can we learn causal models from images + text?
- How to combine extracted knowledge with expert knowledge?
- Can we detect novel causal mechanisms (not in training data)?

---

**Roadmap Status:** ✅ Approved for implementation
**Next Action:** Create `v2-causal-inference` branch and begin Phase 1
**Owner:** Research Lead + Development Team
**Review Date:** Weekly (every Monday 10am)

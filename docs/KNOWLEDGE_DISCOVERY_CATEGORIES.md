# Knowledge Discovery Categories

The report system groups insights into three top-level categories defined in `include/discovery/insight.hpp`.

## Current Registry Status

As of the current code:

- the default runtime registry contains 62 operators
- `bias_audit` and `community_recommendation` are implemented but not included in `all_discovery_operators()`
- 4 legacy insight types remain in `InsightType` for compatibility but are not part of the default registry:
  - `analogical_transfer`
  - `co_authorship`
  - `mechanism_consolidation`
  - `intervention_bottleneck`

## Categories

### Combinatorial

Focus:

- recurring structure
- hierarchy
- dense subgraphs
- relation patterns

Representative insight types:

- Motif
- K-Core
- K-Truss
- Community Detection
- Core-Periphery
- Centrality
- Taxonomy
- Compositional Reasoning
- Transitive Closure
- Meta-Path Pattern
- Multi-Resolution Community
- Cross-Community Bridge Map
- Meta-Pattern

### Exploratory

Focus:

- missing links
- bridges
- paths
- cross-domain or cross-community traversal

Representative insight types:

- Bridge
- Completion
- Long Chain
- Path Rank
- Community Link
- Hyperedge Prediction
- Domain Bridge
- Explanatory Chain
- Causal Chain
- Relation Induction
- Diffusion
- Text Similarity
- Author Chain
- Citation Impact
- Cross-Community Mechanism Bridge
- Translation Gap

### Transformational

Focus:

- reframing
- contradiction
- alternative mechanisms
- anomaly and uncertainty surfacing

Representative insight types:

- Substitution
- Contradiction
- Entity Resolution
- Counterfactual
- Surprise
- Hypotheses 1/2/3
- Meta-Path Anomaly
- Competing Mechanism
- Schema Repair
- Intervention Point
- Feedback Loop
- Confounder
- Logical Entailment
- Schema Violation
- Claim Stance
- Active Learning
- Uncertainty Sampling
- Evidence Debt
- Consensus Frontier
- Boundary Condition Map
- Failure Mode Topology
- Benchmark Dependence
- Concept Drift
- Premise Bottleneck

## CLI Names Vs Report Labels

Some registry names map to different human-readable labels in reports:

- `community` -> Community Link
- `pathrank` -> Path Rank
- `rules` -> Rule
- `embedding` -> Embedding Link
- `causal_chains` -> Causal Chain
- `intervention_points` -> Intervention Point
- `feedback_loops` -> Feedback Loop
- `confounders` -> Confounder
- `competing_mechanisms` -> Competing Mechanism

## Why This Matters

The category assignment affects:

- report grouping
- per-category summaries
- JSON category fields in exported insights

Each `Insight` gets its category automatically from `get_insight_category(InsightType)`.

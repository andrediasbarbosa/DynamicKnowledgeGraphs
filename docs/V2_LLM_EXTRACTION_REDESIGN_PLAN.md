# V2 LLM Extraction Redesign Plan

## Purpose

This document defines a concrete redesign of the chunk-to-knowledge-graph extraction pipeline.

The main objective is to improve the quality of discovered knowledge by making extraction:

1. typed rather than free-form
2. evidence-grounded rather than summary-like
3. overlap-aware rather than chunk-local
4. proposition-centric rather than raw triple-centric
5. compatible with the current C++ hypergraph and discovery stack

The redesign is motivated by current extraction artifacts such as:

- placeholder entities like `A`, `B`, `C`
- role variables like `h`, `t`, `r`, `l1`, `l2`, `l3`
- numeric literals promoted to nodes like `0.70` and `0.85`
- generic nouns promoted to central entities like `college`, `university`, `institute`

These are not discovery failures. They are extraction contract failures.

## Current Bottlenecks

The current pipeline has three structural constraints:

1. It uses a single-pass prompt that asks the model to emit final relations directly.
2. It does not enforce a typed schema in the C++ extraction path.
3. It stores only a thin edge representation, so later stages cannot distinguish:
   - entities vs literals
   - concrete claims vs examples
   - causal claims vs associative claims
   - threshold values vs domain concepts
   - strong evidence vs weak evidence

Relevant integration points in the current codebase:

- `src/llm/llm_provider.cpp`
- `include/llm/llm_provider.hpp`
- `src/pipeline/extraction_pipeline.cpp`
- `include/pipeline/extraction_pipeline.hpp`
- `include/graph/hypergraph.hpp`

## Target Design

### Core Principle

The extractor should produce a small typed information model first, and only then compile that model into hyperedges.

The LLM should not invent graph nodes directly. It should populate a constrained schema.

### Pipeline Stages

#### Stage A: Chunking and Local Context Assembly

Replace "one page = one chunk" as the default extraction mode.

Recommended default:

- strategy: sentence or paragraph windows
- window size: 6-10 sentences or 700-1200 chars
- overlap: 15-20 percent
- carry-forward context: title, abstract, section heading, previous window tail

Output:

- `TextChunk`
- `chunk_context`
  - document title
  - section heading
  - page range
  - local neighboring text

#### Stage B: Mention Extraction

Task:

- identify entity mentions
- identify literal mentions
- identify claim/evaluation cues
- identify section role

The model outputs mention records with spans and types, not graph edges.

#### Stage C: Mention Canonicalization

Resolve chunk-local mentions into canonical references:

- named entity normalization
- acronym expansion
- literal typing
- placeholder detection
- generic-term filtering

This step can be partly rule-based before any second LLM call.

#### Stage D: Proposition Extraction

Use the chunk text plus the validated mention inventory to extract propositions.

Important constraint:

- propositions must refer to mention IDs or canonical IDs
- the model is not allowed to emit new free-text entities at this stage

#### Stage E: Proposition Verification

Each proposition must be checked against an evidence span.

Verification can be:

- rule-based when the predicate is obvious
- LLM-based when the proposition is nuanced

This stage should reject:

- unsupported relations
- placeholder/tutorial/example relations
- generic taxonomy noise

#### Stage F: Cross-Chunk Reconciliation

Merge propositions across overlapping chunks using:

- canonical argument IDs
- normalized predicate
- matching evidence spans
- overlapping provenance

This replaces the current "merge by normalized triple string" approach.

#### Stage G: Graph Compilation

Compile validated propositions into the current hypergraph.

Preserve:

- typed arguments
- literals
- qualifiers
- causal metadata
- multi-source provenance
- quality flags

## Target Extraction Schema

The extractor should emit a JSON object with five top-level collections.

```json
{
  "mentions": [],
  "entities": [],
  "literals": [],
  "propositions": [],
  "evidence": []
}
```

### 1. Mention

Mentions are surface-form spans in a chunk.

```json
{
  "mention_id": "m_0001",
  "surface": "UKGE",
  "normalized": "UKGE",
  "mention_type": "entity",
  "entity_type": "method",
  "chunk_id": "doc_chunk_4",
  "page": 5,
  "start_char": 128,
  "end_char": 132,
  "sentence_id": "s_12",
  "quality_flags": []
}
```

Allowed `mention_type` values:

- `entity`
- `literal`
- `claim_trigger`
- `condition`
- `citation`

Allowed `entity_type` values:

- `person`
- `organization`
- `method`
- `model`
- `dataset`
- `metric`
- `task`
- `concept`
- `theory`
- `artifact`
- `location`
- `time`
- `other`

### 2. Entity

Entities are canonicalized graph-worthy concepts.

```json
{
  "entity_id": "ent_ukge",
  "canonical_label": "UKGE",
  "entity_type": "method",
  "aliases": ["UKGE", "uncertain knowledge graph embedding"],
  "source_mentions": ["m_0001", "m_0042"],
  "document_frequency": 1,
  "quality_flags": []
}
```

### 3. Literal

Literals are values, thresholds, counts, units, and ranges.

```json
{
  "literal_id": "lit_085",
  "surface": "0.85",
  "datatype": "number",
  "normalized_value": 0.85,
  "unit": null,
  "semantic_role": "threshold",
  "source_mentions": ["m_0102"],
  "quality_flags": []
}
```

Allowed `datatype` values:

- `number`
- `percentage`
- `range`
- `date`
- `string`
- `boolean`

Typical `semantic_role` values:

- `threshold`
- `count`
- `score`
- `confidence`
- `year`
- `parameter`
- `other`

### 4. Evidence

Every proposition must point to evidence.

```json
{
  "evidence_id": "ev_0007",
  "document_id": "Embedding_Uncertainty_in_KGs",
  "chunk_id": "Embedding_Uncertainty_in_KGs_chunk_4",
  "page": 5,
  "start_char": 220,
  "end_char": 314,
  "quote": "Thresholds are set to 0.85 for CN15k and NL27k, and 0.70 for PPI5k.",
  "section_role": "method",
  "sentence_ids": ["s_14"]
}
```

### 5. Proposition

Propositions are the actual graph-bearing facts.

```json
{
  "proposition_id": "prop_0018",
  "proposition_type": "evaluation_setting",
  "predicate": "uses_threshold",
  "arguments": [
    {"role": "subject", "ref_id": "ent_cn15k", "ref_type": "entity"},
    {"role": "subject", "ref_id": "ent_nl27k", "ref_type": "entity"},
    {"role": "value", "ref_id": "lit_085", "ref_type": "literal"}
  ],
  "qualifiers": {
    "polarity": "affirmed",
    "modality": "asserted",
    "section_role": "method"
  },
  "confidence": 0.93,
  "support_level": "explicit",
  "evidence_ids": ["ev_0007"],
  "quality_flags": []
}
```

Allowed `proposition_type` values:

- `relation`
- `taxonomy`
- `authorship`
- `citation`
- `claim`
- `causal_claim`
- `evaluation`
- `evaluation_setting`
- `comparison`
- `definition`

Important:

- thresholds should become `literal` arguments, not entity nodes
- tutorial/example variables should be flagged and not compiled into the final graph
- causal claims should carry the existing `causal_metadata`

## Quality Flags

The extractor should support explicit rejection and quarantine signals.

Recommended flags:

- `placeholder_symbol`
- `toy_example`
- `generic_noun`
- `underspecified_entity`
- `unresolved_acronym`
- `literal_not_entity`
- `weak_evidence`
- `cross_boundary_incomplete`
- `schema_violation`
- `possibly_hallucinated`

Compilation rule:

- propositions with severe flags should not enter the main graph
- they may be stored in a sidecar audit file for analysis

## Extraction Rules That Should Be Hard-Coded

These should not be left entirely to the LLM.

### Placeholder and Example Suppression

Reject or quarantine mentions matching patterns such as:

- single uppercase letters: `A`, `B`, `C`, `X`, `Y`, `Z`
- role variables: `h`, `t`, `r`
- sequence variables: `l1`, `l2`, `l3`

Exception:

- keep them only if they are stable domain identifiers with corpus support

### Literal Handling

Numbers such as `0.70`, `0.85`, `15,000`, and `241,158` should be parsed as literals.

Do not promote them to graph entities unless they are known named identifiers.

### Generic Term Gating

Generic nouns such as `college`, `university`, `institute`, `method`, `paper`, `model`, `dataset` should only become canonical entities when one of the following is true:

1. they appear in a definitional or taxonomic statement
2. they are part of a named multi-word term
3. they are grounded by repeated corpus evidence with stable neighborhood patterns

Otherwise they should remain uncompiled mentions or low-priority concept nodes.

## Structured LLM Usage

### Prompt Strategy

Use narrower prompts for narrower tasks.

Recommended prompts:

1. mention extraction prompt
2. proposition extraction prompt
3. verification prompt
4. optional causal enrichment prompt

Do not ask one prompt to do all four tasks.

### Provider-Side Structured Output

For OpenAI-compatible models:

- use schema-enforced JSON output
- avoid plain "respond with JSON" instructions when the API can enforce structure

For Gemini:

- use structured generation where available
- otherwise validate strictly and re-ask on schema failure

### Temperature and Retry Policy

Recommended settings:

- temperature: `0.0` to `0.2`
- retries only on transport/schema failure
- no retries for semantically bad but valid outputs; send to verifier instead

## C++ Integration Plan

### 1. Extend `llm_provider.hpp`

Add typed extraction objects:

- `ExtractedMention`
- `ExtractedEntity`
- `ExtractedLiteral`
- `ExtractedProposition`
- `ExtractionEvidence`
- `StructuredExtractionResult`

Keep `ExtractedRelation` temporarily for backward compatibility.

### 2. Extend `llm_provider.cpp`

Add new prompt builders:

- `mention_extraction_system_prompt()`
- `mention_extraction_user_prompt()`
- `proposition_extraction_system_prompt()`
- `proposition_extraction_user_prompt()`
- `verification_system_prompt()`
- `verification_user_prompt()`

Add parser functions for the new schema.

### 3. Extend `PipelineConfig`

Add configuration flags:

```cpp
bool enable_structured_extraction = true;
bool enable_verification_pass = true;
bool enable_placeholder_filtering = true;
bool enable_literal_typing = true;
bool enable_generic_entity_gating = true;
bool quarantine_low_quality_propositions = true;
std::string extraction_mode = "staged";   // "legacy" | "staged"
std::string default_chunking_strategy_v2 = "sentence_overlap";
```

### 4. Refactor `ExtractionPipeline::extract_from_chunks`

Current behavior:

- one chunk
- one LLM call
- final relations returned directly

Target behavior:

1. extract mentions and evidence spans
2. canonicalize mentions locally
3. extract propositions using mention IDs
4. verify propositions
5. attach provenance
6. emit structured results

### 5. Replace Triple-String Deduplication

Current deduplication in `deduplicate_relations()` merges by normalized string key.

Target deduplication key:

- normalized predicate
- canonical argument IDs
- qualifier signature
- overlapping evidence spans

Confidence merging should use:

- verifier agreement
- evidence count
- overlap confirmation
- cross-document repetition

### 6. Extend `HyperEdge`

Add or preserve:

- multi-provenance support
- typed argument roles
- serialized qualifiers
- quality flags
- proposition type

Current `HyperEdge` stores only one chunk/page plus generic properties.
That is too weak for downstream epistemic analysis.

### 7. Preserve Causal Metadata End to End

The existing code already supports causal metadata in the extraction structs and graph model.

The graph build step should explicitly copy causal metadata from extracted propositions into `HyperEdge`.

This should happen during graph compilation, not as a later patch.

## Mapping Propositions to Hyperedges

The current hypergraph model can still be used.

### Recommended Compilation Rule

- canonical entity arguments become graph nodes
- literal arguments become either:
  - literal nodes with `node_kind=literal`, or
  - edge properties when the literal is purely a qualifier
- proposition becomes a hyperedge

Example:

```json
{
  "predicate": "uses_threshold",
  "arguments": [
    {"role": "subject", "ref_id": "ent_cn15k", "ref_type": "entity"},
    {"role": "subject", "ref_id": "ent_nl27k", "ref_type": "entity"},
    {"role": "value", "ref_id": "lit_085", "ref_type": "literal"}
  ]
}
```

Compiles to:

- `sources`: `["CN15k", "NL27k"]`
- `relation`: `"uses_threshold"`
- `targets`: `["0.85"]` only if literals are allowed as nodes
- otherwise store `"threshold": "0.85"` in edge properties

Recommendation:

- use literal nodes when the value may participate in graph queries
- use edge properties when the value is purely local metadata

For thresholds, metrics, counts, and years, literal nodes are usually worth keeping if typed.

## Minimum Viable Implementation Order

### Phase 1: Quality Guardrails

Implement first:

1. placeholder suppression
2. literal typing
3. generic-term gating
4. structured JSON output in the C++ provider path

Expected benefit:

- immediate reduction in noisy nodes and false centrality insights

### Phase 2: Staged Extraction

Implement next:

1. mention extraction
2. proposition extraction referencing mention IDs
3. evidence object emission

Expected benefit:

- less label drift
- less entity invention
- better provenance

### Phase 3: Verification and Reconciliation

Implement next:

1. verifier pass
2. overlap-aware proposition merge
3. multi-provenance graph compilation

Expected benefit:

- higher precision
- better support for confidence scoring

### Phase 4: Discovery-Aware Compilation

Implement next:

1. proposition types
2. section-role metadata
3. causal propagation
4. qualifier-aware filtering before discovery

Expected benefit:

- stronger downstream operators
- more meaningful hypotheses

## Evaluation Plan

Track these metrics before and after rollout.

### Extraction Quality

- entity precision / recall
- proposition precision / recall
- literal typing accuracy
- placeholder suppression rate
- schema-valid response rate
- verifier rejection rate

### Graph Quality

- fraction of low-signal nodes
- fraction of hyperedges touching low-signal nodes
- duplicate proposition merge rate
- average provenance count per retained proposition

### Discovery Quality

- fraction of insights touching low-signal nodes
- expert-rated usefulness of top 20 insights
- contradiction precision
- mechanistic hypothesis precision
- report stability across reruns

## Immediate Coding Targets

The first implementation slice should touch these files:

- `include/llm/llm_provider.hpp`
- `src/llm/llm_provider.cpp`
- `include/pipeline/extraction_pipeline.hpp`
- `src/pipeline/extraction_pipeline.cpp`
- `include/graph/hypergraph.hpp`

Optional later work:

- `src/pdf/pdf_processor.cpp`
- `include/pdf/pdf_processor.hpp`

## Recommended First PR

The first PR should be intentionally narrow.

Scope:

1. add structured extraction schema types
2. add placeholder and literal filters
3. add provider-side structured JSON handling
4. add `legacy` vs `staged` extraction mode flag
5. keep current discovery unchanged

Success criterion:

- rerunning the latest test paper should remove `A/B/C`, `h/t/r`, `l1/l2/l3`, `0.70`, `0.85`, `college`, `university`, and `institute` from the main graph unless they are explicitly justified by the verifier

## Summary

The correct next step is not "more operators first."

The correct next step is:

1. redesign the extraction contract
2. emit typed objects instead of raw triples
3. verify propositions against evidence
4. reconcile across overlapping chunks
5. compile only validated propositions into the hypergraph

Once that is in place, the existing discovery engine should produce substantially better and more defensible insights without major operator changes.

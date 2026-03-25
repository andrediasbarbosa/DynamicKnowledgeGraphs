# InternalAuditManual_Published_Jan2022_Cardiff Knowledge Discovery

**Generated:** 2026-01-26 14:21:02 UTC  
**Source:** InternalAuditManual_Published_Jan2022_Cardiff.pdf  
**Run ID:** run_20260126_141907  

---

## Executive Summary

This report presents the results of automated knowledge discovery analysis on the extracted knowledge graph. A total of **10 insights** were identified across 2 different categories.

### Key Findings

- **7 community links** highlight cross-cluster candidates with similar relation signatures.
- **3 hypotheses** synthesize testable claims from the combined discovery findings.

## Knowledge Graph Statistics

| Metric | Value |
|--------|-------|
| Total Entities | 127 |
| Total Relationships | 71 |
| Average Degree | 1.55 |
| Max Degree | 17 |
| Insights Discovered | 10 |

## Augmentation Methods

This report uses twelve augmentation signals derived from the graph structure and local patterns:
- **Bridge**: identifies entities that connect otherwise separate regions of the graph.
- **Completion**: suggests missing relations based on similar neighborhood patterns.
- **Motif**: highlights recurring subgraph structures that indicate repeated concepts.
- **Substitution**: proposes interchangeable entities in similar contexts.
- **Diffusion**: maps likely influence pathways over the graph.
- **Surprise**: flags statistically unusual or novel connections.
- **Community Link**: proposes cross-cluster links with similar relation signatures.
- **Path Rank**: proposes links supported by multiple short graph paths.
- **Author Chain**: traces citation chains across authors.
- **Hypothesis**: synthesizes testable claims from combined discovery findings.
- **Rule**: mines association rules between relation types (e.g., if R1(X,Y) then R2(X,Z)).

- **Embedding Link**: predicts missing relations using TransE embeddings.

Alternatives to consider:
- Embedding-based link prediction (TransE, RotatE, ComplEx).
- GNN-based link prediction (GraphSAGE, GAT, RGCN).
- Path-based ranking or meta-path inference for schema-aware graphs.
- Community-aware augmentation that prioritizes cross-cluster links.

## LLM-Synthesized Examples

Top example per discovery method, with graph-context summaries and LLM expansions.

### Community Link

**Graph context:** A cross-cluster signal links **IA** and **Audit & Risk Committee** based on shared relation patterns across separate communities. Community link: IA <-> Audit & Risk Committee (relation overlap=0.50). This candidate link is supported by 20 evidence edges (confidence: very high).

**LLM expansion:** There is a significant relationship between Internal Audit (IA) and the Audit & Risk Committee, indicated by a 50% overlap in their relational patterns across different communities. This connection is reinforced by 20 evidence edges, suggesting a very high confidence in the link's validity. The shared relation patterns imply that both entities engage in similar activities or functions within their respective domains.

### Hypothesis

**Graph context:** Hypothesis: **IA** is meaningfully related to **Audit & Risk Committee** based on patterns in the augmented graph. Hypothesis grounded in community_link: IA <-> Audit & Risk Committee (Community link: IA <-> Audit & Risk Committee (relation overlap=0.50)). Evidence sources include InternalAuditManual_Published_Jan2022_Cardiff_chunk_1 and 7 others.

**LLM expansion:** The Internal Audit (IA) function is significantly related to the Audit & Risk Committee. This relationship is supported by a community link with a relation overlap of 0.50, indicating a moderate level of interconnectedness between the two entities in the augmented graph. Evidence for this hypothesis is drawn from the Internal Audit Manual and several other sources.

## Community Links

These candidates connect entities from different graph communities based on shared relation signatures.

### 1. IA <-> Audit & Risk Committee

There is a significant relationship between Internal Audit (IA) and the Audit & Risk Committee, indicated by a 50% overlap in their relational patterns across different communities. This connection is reinforced by 20 evidence edges, suggesting a very high confidence in the link's validity. The shared relation patterns imply that both entities engage in similar activities or functions within their respective domains.

### 2. Chief Operating Officer <-> Audit & Risk Committee

The Chief Operating Officer and the Audit & Risk Committee are connected through shared relation patterns across different communities, with a relation overlap of 0.33. This connection is substantiated by 19 evidence edges, indicating a very high confidence in the relationship's validity. The overlap suggests that both entities may engage in similar operational and oversight functions within their respective domains.

### 3. Chair of Audit & Risk Committee <-> Audit & Risk Committee

The Chair of the Audit & Risk Committee is closely related to the Audit & Risk Committee, with a relation overlap of 50%. This connection is supported by 18 evidence edges, indicating a very high confidence in the link between these two entities based on shared relation patterns across different communities.

### 4. UEB papers <-> Audit & Risk Committee

The UEB papers and the Audit & Risk Committee are closely related, sharing a complete overlap in their relational patterns. This strong connection is evidenced by 18 supporting edges, indicating a very high confidence in the relationship between these two entities within the graph.

### 5. Audit & Risk Committee <-> Audit & Risk Committee

The relationship indicates a strong connection between the Audit & Risk Committee and itself, highlighting a complete overlap in their relational patterns. This community link is supported by 17 evidence edges, demonstrating a very high confidence in the shared characteristics of the Audit & Risk Committee across different clusters.

### 6. Audit & Risk Committee <-> Audit & Risk Committee

The relationship indicates a connection between the Audit & Risk Committee and itself, highlighting a shared presence across different communities. This link is characterized by a relation overlap of 0.25 and is supported by 17 evidence edges, suggesting a very high confidence in the connection's validity.

### 7. Audit & Risk Committee <-> Audit & Risk Committee

The relationship indicates a community link between the Audit & Risk Committee and itself, suggesting a shared relevance across different clusters. This link has a relation overlap of 0.25 and is supported by 17 evidence edges, indicating a very high confidence in the connection based on similar relational patterns.

## Testable Hypotheses

These hypotheses synthesize findings across discovery methods into testable claims.

### 1. Hypothesis

The Internal Audit (IA) function is significantly related to the Audit & Risk Committee. This relationship is supported by a community link with a relation overlap of 0.50, indicating a moderate level of interconnectedness between the two entities in the augmented graph. Evidence for this hypothesis is drawn from the Internal Audit Manual and several other sources.

### 2. Hypothesis

The Chief Operating Officer (COO) is significantly related to the Audit & Risk Committee. This relationship is supported by a community link with a relation overlap of 0.33, indicating a moderate level of interconnectedness between the COO's responsibilities and the oversight functions of the Audit & Risk Committee, as evidenced by multiple sources including the Internal Audit Manual published in January 2022.

### 3. Hypothesis

The Chair of the Audit & Risk Committee is closely associated with the Audit & Risk Committee itself. This relationship is supported by a community link indicating a 50% overlap, suggesting that the roles and responsibilities of the Chair are significantly intertwined with the functions of the Committee. Evidence for this connection is drawn from multiple sources, including the Internal Audit Manual published in January 2022.

## Conclusions and Recommendations

Based on the analysis of 10 discovered insights, the following recommendations are made:

7. **Review Community Links**: The 7 cross-cluster links highlight structurally similar entities across communities. Validate candidates that bridge distinct topic areas.

8. **Test Hypotheses**: The 3 synthesized hypotheses translate graph discoveries into testable claims. Prioritize those with strong supporting evidence.

---

*This report was automatically generated by the Knowledge Discovery Engine. All insights should be validated by domain experts before taking action.*

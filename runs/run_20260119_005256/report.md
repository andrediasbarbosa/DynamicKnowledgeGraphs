# MathsHistoryBook Knowledge Discovery

**Generated:** 2026-01-19 18:08:40 UTC  
**Source:** MathsHistoryBook.pdf  
**Run ID:** run_20260119_005256  

---

## Executive Summary

This report presents the results of automated knowledge discovery analysis on the extracted knowledge graph. A total of **350 insights** were identified across 4 different categories.

### Key Findings

- **100 potential completions** suggest relationships that could be extended with additional entities.
- **100 recurring patterns (motifs)** were detected, indicating frequently co-occurring concept clusters.
- **50 surprising connections** were discovered that represent statistically unusual or novel relationships.
- **100 association rules** were mined, revealing predictable relationship patterns between different relation types.

## Knowledge Graph Statistics

| Metric | Value |
|--------|-------|
| Total Entities | 6094 |
| Total Relationships | 5410 |
| Average Degree | 2.09 |
| Max Degree | 71 |
| Insights Discovered | 350 |

## Augmentation Methods

This report uses seven augmentation signals derived from the graph structure and local patterns:
- **Bridge**: identifies entities that connect otherwise separate regions of the graph.
- **Completion**: suggests missing relations based on similar neighborhood patterns.
- **Motif**: highlights recurring subgraph structures that indicate repeated concepts.
- **Substitution**: proposes interchangeable entities in similar contexts.
- **Diffusion**: maps likely influence pathways over the graph.
- **Surprise**: flags statistically unusual or novel connections.
- **Rule**: mines association rules between relation types (e.g., if R1(X,Y) then R2(X,Z)).

Alternatives to consider:
- Embedding-based link prediction (TransE, RotatE, ComplEx).
- GNN-based link prediction (GraphSAGE, GAT, RGCN).
- Path-based ranking or meta-path inference for schema-aware graphs.
- Community-aware augmentation that prioritizes cross-cluster links.

## LLM-Synthesized Examples

Top example per discovery method, with graph-context summaries and LLM expansions.

### Completion

**Graph context:** A potential knowledge gap was identified: the relationship between **x** and **y** appears incomplete. Based on similar patterns in the graph, entities like **0** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

**LLM expansion:** The relationship between entities x and y suggests a combination or interaction that is currently underexplored. The presence of witness entities such as 0 and a indicates that there are additional potential connections that could enhance the understanding of this relationship, with 18 possible slot fillers identified for further exploration. This highlights a knowledge gap that could be addressed to improve the completeness of the graph.

### Motif

**Graph context:** A recurring pattern was discovered involving **C** and **D'**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

**LLM expansion:** The relationship between entities C and D' indicates a strong co-occurrence pattern, suggesting they are closely related in the graph. With a support of 3 and a lift of 1803.33, this motif highlights a significant and recurring connection, emphasizing their importance within the domain. The absence of different contexts further reinforces the idea that C and D' are conceptually linked.

### Surprise

**Graph context:** An unexpected connection was found: the combination of **real numbers 0-1**, **real numbers 0-4** and **real numbers 1-2** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

**LLM expansion:** The relationship indicates that the combination of the real numbers 0-1, 0-4, and 1-2 is statistically rare when analyzed together. This surprising connection, with a score of 0.47, suggests that these specific ranges may interact in a way that is not commonly recognized within the knowledge domain. Further exploration of this relationship could reveal insights into their combined properties or applications.

### Rule

**Graph context:** An association rule was discovered: Rule: has half-life(X, _) => decays into(X, _) [support=3, confidence=1.00, lift=901.67]. Example entities that satisfy this rule include **Rb-87**, **carbon (C-14)** and **uranium (U-238)**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

**LLM expansion:** The relationship indicates that if an entity has a half-life, it will decay into another entity. Rb-87, carbon (C-14), and uranium (U-238) are examples of isotopes that exhibit this behavior, as they all have defined half-lives and undergo radioactive decay into other elements or isotopes. This association rule demonstrates a strong predictive relationship within the context of radioactive decay processes.

## Knowledge Gaps (Potential Completions)

These insights identify relationships that appear incomplete based on patterns in the graph. They represent opportunities to enrich the knowledge base.

### 1. x + y

A potential knowledge gap was identified: the relationship between **x** and **y** appears incomplete. Based on similar patterns in the graph, entities like **0** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 2. 0 + 1

A potential knowledge gap was identified: the relationship between **0** and **1** appears incomplete. Based on similar patterns in the graph, entities like **11** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 3. energy + mass

A potential knowledge gap was identified: the relationship between **energy** and **mass** appears incomplete. Based on similar patterns in the graph, entities like **E = mc2** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 4. neutrons + protons

A potential knowledge gap was identified: the relationship between **neutrons** and **protons** appears incomplete. Based on similar patterns in the graph, entities like **Gell-Mann** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 5. a1 + a2

A potential knowledge gap was identified: the relationship between **a1** and **a2** appears incomplete. Based on similar patterns in the graph, entities like **a0** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 6. 1 + 2

A potential knowledge gap was identified: the relationship between **1** and **2** appears incomplete. Based on similar patterns in the graph, entities like **-3 series** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 7. a + b

A potential knowledge gap was identified: the relationship between **a** and **b** appears incomplete. Based on similar patterns in the graph, entities like **c** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 8. space + time

A potential knowledge gap was identified: the relationship between **space** and **time** appears incomplete. Based on similar patterns in the graph, entities like **Hermann Minkowski** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 9. Mathematics + Universe

A potential knowledge gap was identified: the relationship between **Mathematics** and **Universe** appears incomplete. Based on similar patterns in the graph, entities like **Earth** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

### 10. 2 + 3

A potential knowledge gap was identified: the relationship between **2** and **3** appears incomplete. Based on similar patterns in the graph, entities like **-3 series** might also be relevant to this relationship. This represents an opportunity to enrich the knowledge graph with additional connections (confidence: moderate).

## Recurring Patterns (Motifs)

Motifs are frequently co-occurring sets of entities that form recognizable patterns. They often represent core concepts or themes in the domain.

### 1. Pattern Cluster

A recurring pattern was discovered involving **C** and **D'**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 2. Pattern Cluster

A recurring pattern was discovered involving **0.8269** and **0.8750**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 3. Pattern Cluster

A recurring pattern was discovered involving **line1** and **line2**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 4. Pattern Cluster

A recurring pattern was discovered involving **area of circle** and **area of square**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 5. Pattern Cluster

A recurring pattern was discovered involving **0.5009** and **0.8750**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 6. Pattern Cluster

A recurring pattern was discovered involving **0.5009** and **0.8269**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 7. Pattern Cluster

A recurring pattern was discovered involving **0.3820** and **0.8750**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 8. Pattern Cluster

A recurring pattern was discovered involving **0.3820** and **0.8269**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 9. Pattern Cluster

A recurring pattern was discovered involving **e3** and **e5**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

### 10. Pattern Cluster

A recurring pattern was discovered involving **0.3820** and **0.5009**. These entities frequently co-occur across 0 different contexts, suggesting a strong thematic or conceptual relationship. This motif may represent a fundamental concept cluster in the domain.

## Surprising Discoveries

These connections are statistically unusual given the overall structure of the knowledge graph. They may represent novel findings, errors, or genuinely unexpected relationships worth investigating.

### 1. Unexpected Connection

An unexpected connection was found: the combination of **real numbers 0-1**, **real numbers 0-4** and **real numbers 1-2** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_211*

### 2. Unexpected Connection

An unexpected connection was found: the combination of **arcs**, **intervening galaxy** and **magnification** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_709*

### 3. Unexpected Connection

An unexpected connection was found: the combination of **2nd harmonic**, **3rd harmonic** and **4th harmonic** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_242*

### 4. Unexpected Connection

An unexpected connection was found: the combination of **East**, **North** and **South** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_520*

### 5. Unexpected Connection

An unexpected connection was found: the combination of **descriptive names**, **protective laws** and **registered names** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_1*

### 6. Unexpected Connection

An unexpected connection was found: the combination of **arcminutes**, **arcseconds** and **base 60 system** is statistically rare in this knowledge domain. With a surprise score of 0.47, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_31*

### 7. Unexpected Connection

An unexpected connection was found: the combination of **Publisher**, **broadcasting** and **reprinting** is statistically rare in this knowledge domain. With a surprise score of 0.46, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_1*

### 8. Unexpected Connection

An unexpected connection was found: the combination of **alcohol**, **cloud chamber** and **dry ice** is statistically rare in this knowledge domain. With a surprise score of 0.46, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_721*

### 9. Unexpected Connection

An unexpected connection was found: the combination of **1 unit**, **2r** and **4r** is statistically rare in this knowledge domain. With a surprise score of 0.46, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_613*

### 10. Unexpected Connection

An unexpected connection was found: the combination of **animal**, **bacterial** and **human** is statistically rare in this knowledge domain. With a surprise score of 0.46, this represents a potentially novel or overlooked relationship that warrants further investigation.

*Found in: MathsHistoryBook_chunk_406*

## Association Rules

Association rules reveal predictable patterns between different relation types. These rules can be used for knowledge inference, validation, or identifying structural regularities.

### 1. Rule: has half-life(X, _) => decays into(X, _) [support=3, confidence=1.00, lift=901.67]

An association rule was discovered: Rule: has half-life(X, _) => decays into(X, _) [support=3, confidence=1.00, lift=901.67]. Example entities that satisfy this rule include **Rb-87**, **carbon (C-14)** and **uranium (U-238)**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 2. Rule: have angle(X, _) => meet at(X, _) [support=3, confidence=1.00, lift=1352.50]

An association rule was discovered: Rule: have angle(X, _) => meet at(X, _) [support=3, confidence=1.00, lift=1352.50]. Example entities that satisfy this rule include **eight triangles**, **four triangles** and **twelve triangles**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 3. Rule: decays into(X, _) => has half-life(X, _) [support=3, confidence=0.50, lift=901.67]

An association rule was discovered: Rule: decays into(X, _) => has half-life(X, _) [support=3, confidence=0.50, lift=901.67]. Example entities that satisfy this rule include **Rb-87**, **carbon (C-14)** and **uranium (U-238)**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 4. Rule: meet at(X, _) => have angle(X, _) [support=3, confidence=0.75, lift=1352.50]

An association rule was discovered: Rule: meet at(X, _) => have angle(X, _) [support=3, confidence=0.75, lift=1352.50]. Example entities that satisfy this rule include **eight triangles**, **four triangles** and **twelve triangles**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 5. Rule: calculated(X, _) => achieved(X, _) [support=3, confidence=0.60, lift=1623.00]

An association rule was discovered: Rule: calculated(X, _) => achieved(X, _) [support=3, confidence=0.60, lift=1623.00]. Example entities that satisfy this rule include **Eratosthenes** and **Riemann**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 6. Rule: is divergent(X, _) => diverges to(X, _) [support=3, confidence=1.00, lift=1803.33]

An association rule was discovered: Rule: is divergent(X, _) => diverges to(X, _) [support=3, confidence=1.00, lift=1803.33]. Example entities that satisfy this rule include **harmonic series** and **series**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 7. Rule: diverges to(X, _) => is divergent(X, _) [support=3, confidence=1.00, lift=1803.33]

An association rule was discovered: Rule: diverges to(X, _) => is divergent(X, _) [support=3, confidence=1.00, lift=1803.33]. Example entities that satisfy this rule include **harmonic series** and **series**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 8. Rule: defines output(_, Y) => is plotted against(_, Y) [support=3, confidence=1.00, lift=5410.00]

An association rule was discovered: Rule: defines output(_, Y) => is plotted against(_, Y) [support=3, confidence=1.00, lift=5410.00]. Example entities that satisfy this rule include **X**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 9. Rule: left(X, _) => reaches(X, _) [support=3, confidence=1.00, lift=1803.33]

An association rule was discovered: Rule: left(X, _) => reaches(X, _) [support=3, confidence=1.00, lift=1803.33]. Example entities that satisfy this rule include **Andromeda galaxy** and **light**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

### 10. Rule: left(X, _) => moves towards(X, _) [support=3, confidence=1.00, lift=2705.00]

An association rule was discovered: Rule: left(X, _) => moves towards(X, _) [support=3, confidence=1.00, lift=2705.00]. Example entities that satisfy this rule include **Andromeda galaxy** and **light**. This pattern suggests a predictable relationship structure that could be used for knowledge inference or validation.

## Conclusions and Recommendations

Based on the analysis of 350 discovered insights, the following recommendations are made:

2. **Address Knowledge Gaps**: Review the 100 potential completions to determine if additional relationships should be added to create a more complete knowledge representation.

4. **Investigate Surprises**: The 50 surprising connections warrant manual review to determine if they represent genuine discoveries or potential data quality issues.

5. **Leverage Association Rules**: The 100 discovered rules can be used for automated knowledge inference, consistency checking, or to guide further data collection.

---

*This report was automatically generated by the Knowledge Discovery Engine. All insights should be validated by domain experts before taking action.*

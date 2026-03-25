# Cambridge_Handbook_Thinking_Reasoning Knowledge Discovery

**Generated:** 2026-03-24 12:12:46 UTC  
**Source:** Cambridge_Handbook_Thinking_Reasoning.pdf, Introduction-to-Philosophy-Epistemology.pdf  
**Run ID:** run_20260322_230605  

---

## Executive Summary

This report presents the results of automated knowledge discovery analysis on the extracted knowledge graph. A total of **146 insights** were identified across 16 different categories.

### Key Findings

- **8 bridge entities** were identified that connect separate knowledge clusters, revealing critical nodes for information flow.
- **21 core-periphery roles** highlight which entities sit at the structural core versus the periphery.
- **3 text similarity links** connect entities with highly similar labels.
- **1 centrality findings** rank entities by structural importance.
- **1 community clusters** summarize dense regions of the graph.
- **1 k-core nodes** identify entities in tightly connected cores.
- **2 k-truss edges** highlight triangle-reinforced relationships.
- **20 meta-path links** connect entities through typed relation sequences.
- **1 multi-resolution communities** reveal hierarchical clustering at multiple scales.
- **11 cross-community bridge maps** link bridge entities to the communities they connect.
- **1 meta-patterns** identify recurring structural templates across pattern insights.
- **20 intervention points** identify nodes whose removal disconnects causal chains.
- **7 confounders** flag nodes connected to both ends of suspected causal relations.
- **9 taxonomic relations** surface hierarchical is-a and part-of structures.
- **20 domain bridges** identify cross-domain or interdisciplinary connector nodes.
- **20 logical entailments** derive implicit implications from existing relations.

## Knowledge Graph Statistics

| Metric | Value |
|--------|-------|
| Total Entities | 5309 |
| Total Relationships | 4385 |
| Average Degree | 1.66 |
| Max Degree | 96 |
| Insights Discovered | 146 |

### Quality Control Statistics

| Metric | Value |
|--------|-------|
| Initial Entities | 5309 |
| Entities Removed | 35 |
| Removal Rate | 0.7% |
| Connected Components | 1345 |
| Largest Component | 2236 (42.4%) |
| Graph Density | 0.0003 |
| Clustering Coefficient | 0.022 |

## Knowledge Discovery Categories

Insights are classified into three categories based on how they generate new knowledge:

### 🧩 Combinatorial Insights (68)
**Pattern Detection & Structural Combinations**

These insights emerge from combining existing knowledge in new ways, identifying patterns, structures, and motifs that reveal the underlying organization of the knowledge graph.
- Average Score: 1.000

### 🔍 Exploratory Insights (31)
**Path Finding & Connection Discovery**

These insights discover new connections, paths, and bridges between concepts, exploring previously unexplored areas of the knowledge space.
- Average Score: 1.010

### 🔄 Transformational Insights (47)
**Reframing & Perspective Shifts**

These insights transform understanding by reframing concepts, generating hypotheses, identifying contradictions, and suggesting alternative causal mechanisms.
- Average Score: 1.000

## Augmentation Methods

This report uses a broad set of augmentation signals derived from the graph structure and local patterns:
- **Bridge**: identifies entities that connect otherwise separate regions of the graph.
- **Completion**: suggests missing relations based on similar neighborhood patterns.
- **Motif**: highlights recurring subgraph structures that indicate repeated concepts.
- **Substitution**: proposes interchangeable entities in similar contexts.
- **Contradiction**: flags conflicts where affirmed and negated claims both appear.
- **Entity Resolution**: detects likely duplicate or alias entities.
- **Core-Periphery**: maps central hubs versus peripheral entities.
- **Text Similarity**: links entities with highly similar labels.
- **Argument Support**: proposes relations grounded in evidence paths.
- **Active Learning**: surfaces high-value verification queries.
- **Method/Outcome**: identifies method or outcome entities.
- **Centrality**: ranks entities by structural importance in the bipartite graph.
- **Community Detection**: groups entities into dense clusters.
- **k-Core**: highlights nodes in tightly connected cores.
- **k-Truss**: identifies edges reinforced by shared triangles.
- **Claim Stance**: classifies claims as supporting/opposing/neutral.
- **Relation Induction**: proposes canonical relation type names.
- **Analogical Transfer**: suggests new links by analogy.
- **Uncertainty Sampling**: highlights low-confidence relations for verification.
- **Counterfactual**: asks falsification questions for key claims.
- **Hyperedge Prediction**: predicts relations using overlap patterns.
- **Diffusion**: maps likely influence pathways over the graph.
- **Surprise**: flags statistically unusual or novel connections.
- **Community Link**: proposes cross-cluster links with similar relation signatures.
- **Path Rank**: proposes links supported by multiple short graph paths.
- **Author Chain**: traces citation chains across authors.
- **Co-Authorship**: identifies frequent collaborators and research clusters.
- **Citation Impact**: ranks influential authors and works by citations/authorships.
- **Multi-Resolution Community**: reveals hierarchical community structure at multiple scales.
- **Cross-Community Bridge Map**: maps bridges to the communities they connect.
- **Meta-Pattern**: identifies recurring structural templates across pattern insights.
- **Hypothesis**: synthesizes testable claims from combined discovery findings.
- **Mechanism Consolidation**: clusters overlapping hypotheses into shared mechanisms.
- **Evidence Fusion Link**: combines multiple operator signals into consensus links.
- **Meta-Path Anomaly**: captures rare but plausible relation-sequence templates.
- **Intervention Bottleneck**: identifies high-leverage nodes across causal paths.
- **Competing Mechanism**: surfaces alternative explanations for the same outcome.
- **Schema Repair**: proposes executable graph consistency fixes.
- **Cross-Community Mechanism Bridge**: connects distant communities via mechanism paths.
- **Rule**: mines association rules between relation types (e.g., if R1(X,Y) then R2(X,Z)).
- **Embedding Link**: predicts missing relations using TransE embeddings.

Alternatives to consider:
- Embedding-based link prediction (TransE, RotatE, ComplEx).
- GNN-based link prediction (GraphSAGE, GAT, RGCN).
- Path-based ranking or meta-path inference for schema-aware graphs.
- Community-aware augmentation that prioritizes cross-cluster links.

## Bridge Entities

Bridge entities are nodes that connect otherwise disconnected parts of the knowledge graph. They serve as critical junctions for information flow and often represent interdisciplinary concepts.

### 1. knowledge

The entity **knowledge** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **a posteriori**, **basic concept** and **belief** become interconnected. This suggests that **knowledge** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Introduction-to-Philosophy-Epistemology`, `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Introduction-to-Philosophy-Epistemology_chunk_194 and 58 other sources*

### 2. problem solving

The entity **problem solving** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **abstract knowledge**, **analogical mapping** and **analogical reasoning** become interconnected. This suggests that **problem solving** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Cambridge_Handbook_Thinking_Reasoning_chunk_814 and 52 other sources*

### 3. reasoning

The entity **reasoning** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **age**, **aging** and **basal ganglia** become interconnected. This suggests that **reasoning** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Introduction-to-Philosophy-Epistemology`, `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Introduction-to-Philosophy-Epistemology_chunk_126 and 40 other sources*

### 4. similarity

The entity **similarity** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **alignable difference**, **aligned feature match** and **alignment-based model** become interconnected. This suggests that **similarity** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Cambridge_Handbook_Thinking_Reasoning_chunk_681 and 23 other sources*

### 5. working memory

The entity **working memory** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **aging**, **analogical reasoning** and **analogy** become interconnected. This suggests that **working memory** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Cambridge_Handbook_Thinking_Reasoning_chunk_478 and 27 other sources*

### 6. creativity

The entity **creativity** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **ability to produce work**, **adaptation to task demand** and **adaptive regression** become interconnected. This suggests that **creativity** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Cambridge_Handbook_Thinking_Reasoning_chunk_858 and 23 other sources*

### 7. intelligence

The entity **intelligence** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **adaptation**, **brain size** and **capacity to learn** become interconnected. This suggests that **intelligence** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Cambridge_Handbook_Thinking_Reasoning_chunk_381 and 28 other sources*

### 8. decision making

The entity **decision making** serves as a critical bridge in the knowledge graph. Through this entity, concepts like **academic discipline**, **ariely** and **attribute evaluation** become interconnected. This suggests that **decision making** plays a pivotal role in bridging different domains of knowledge.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`*Evidence from: Cambridge_Handbook_Thinking_Reasoning_chunk_278 and 31 other sources*

## Core–Periphery / Hub–Authority Analysis

These findings highlight entities that form the structural core of the graph as well as those in the periphery. Hub/authority scores reflect whether an entity tends to act as a source (hub) or target (authority) in relations.

### 1. performance score

The entity **performance score** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 2. intellectual humility

The entity **intellectual humility** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 3. pattern

The entity **pattern** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 4. arming lever

The entity **arming lever** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 5. train

The entity **train** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 6. distance flown

The entity **distance flown** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 7. object reference

The entity **object reference** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 8. obligation schema

The entity **obligation schema** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 9. semantic distinction

The entity **semantic distinction** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

### 10. gift

The entity **gift** sits in the periphery of the graph structure. Degree centrality: 0.00. Hub score: 0.00. Authority score: 0.00. Core integration: 0.00. These signals summarize how central this entity is to the knowledge graph.

## Text-Semantic Similarity Links

These links connect entities whose labels are highly similar based on TF-IDF token similarity. They can surface related concepts or naming variants.

### 1. bayes rule ~ bayes' rule

The labels **bayes rule** and **bayes' rule** are textually similar based on token overlap and TF-IDF weighting. Cosine similarity: 1.00. These entities may represent closely related concepts, variants, or aliases.



**Sources:** `Cambridge_Handbook_Thinking_Reasoning`### 2. bayes's theorem ~ bayes’s theorem

The labels **bayes's theorem** and **bayes’s theorem** are textually similar based on token overlap and TF-IDF weighting. Cosine similarity: 1.00. These entities may represent closely related concepts, variants, or aliases.



**Sources:** `Introduction-to-Philosophy-Epistemology`### 3. basic belief ~ basic_beliefs

The labels **basic belief** and **basic_beliefs** are textually similar based on token overlap and TF-IDF weighting. Cosine similarity: 1.00. These entities may represent closely related concepts, variants, or aliases.



**Sources:** `Introduction-to-Philosophy-Epistemology`## Centrality Rankings

These entities are structurally central in the bipartite projection of the hypergraph.

### 1. knowledge

The entity **knowledge** is structurally central in the bipartite projection. Centrality score: 1.00. Highly central entities often connect many relationship contexts.

## Community Detection

These clusters are dense groups discovered via Louvain on the projected graph.

### 1. Community Cluster

A dense community is centered around **problem solving**, **analogy** and **analogical reasoning**. Relative size score: 1.00. These nodes likely share thematic or structural overlap.

## k-Core Nodes

These nodes reside in dense k-core regions of the projected graph.

### 1. aging

The entity **aging** appears in a dense k-core. Core number: 4. High k-core nodes participate in tightly connected regions.

## k-Truss Edges

These edges lie inside dense triangle-reinforced subgraphs.

### 1. belief — justification

The pair **belief** – **justification** lies in a dense k-truss subgraph. Triangle support: 5. Truss edges are reinforced by shared neighbors.

### 2. decision making — kahneman

The pair **decision making** – **kahneman** lies in a dense k-truss subgraph. Triangle support: 5. Truss edges are reinforced by shared neighbors.

## Meta-Path Links

These candidates are inferred from typed relation sequences (meta-paths) that repeatedly connect pairs of entities. They provide interpretable, schema-aware evidence for missing links.

### 1. directional outcome ↔ concept organization

Meta-path reasoning links **directional outcome** and **concept organization**. Meta-path prediction: 'directional outcome' → 'concept organization' via path affects→affects (support: 63). 

### 2. aging ↔ human brain

Meta-path reasoning links **aging** and **human brain**. Meta-path prediction: 'aging' → 'human brain' via path affects→affects (support: 63). 

### 3. ambiguity ↔ spatial reasoning

Meta-path reasoning links **ambiguity** and **spatial reasoning**. Meta-path prediction: 'ambiguity' → 'spatial reasoning' via path affects→affects (support: 63). 

### 4. environmental influence ↔ cerebral glucose metabolism

Meta-path reasoning links **environmental influence** and **cerebral glucose metabolism**. Meta-path prediction: 'environmental influence' → 'cerebral glucose metabolism' via path affects→affects (support: 63). 

### 5. environmental influence ↔ performance

Meta-path reasoning links **environmental influence** and **performance**. Meta-path prediction: 'environmental influence' → 'performance' via path affects→affects (support: 63). 

### 6. environmental influence ↔ temporal discounting

Meta-path reasoning links **environmental influence** and **temporal discounting**. Meta-path prediction: 'environmental influence' → 'temporal discounting' via path affects→affects (support: 63). 

### 7. evans 2003 ↔ human brain

Meta-path reasoning links **evans 2003** and **human brain**. Meta-path prediction: 'evans 2003' → 'human brain' via path affects→affects (support: 63). 

### 8. executive control ↔ analytic intelligence

Meta-path reasoning links **executive control** and **analytic intelligence**. Meta-path prediction: 'executive control' → 'analytic intelligence' via path affects→affects (support: 63). 

### 9. executive control ↔ free recall learning

Meta-path reasoning links **executive control** and **free recall learning**. Meta-path prediction: 'executive control' → 'free recall learning' via path affects→affects (support: 63). 

### 10. executive control ↔ language processing

Meta-path reasoning links **executive control** and **language processing**. Meta-path prediction: 'executive control' → 'language processing' via path affects→affects (support: 63). 

## Intervention Points

Critical nodes whose removal would disconnect causal pathways.

### 1. decision making

Node **decision making** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 2. bia

Node **bia** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 3. preference

Node **preference** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 4. scientific discovery

Node **scientific discovery** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 5. technological innovation

Node **technological innovation** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 6. judgment

Node **judgment** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 7. effective practice

Node **effective practice** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 8. effective problem solving

Node **effective problem solving** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 9. reasoning

Node **reasoning** acts as a critical intervention point, whose removal would disconnect causal pathways.

### 10. human brain

Node **human brain** acts as a critical intervention point, whose removal would disconnect causal pathways.

## Confounders

Nodes that may confound causal relationships as common causes.

### 1. creativity

Node **creativity** may confound the relationship, acting as a common cause that influences multiple outcomes.

### 2. scientific thinking

Node **scientific thinking** may confound the relationship, acting as a common cause that influences multiple outcomes.

### 3. spatial reasoning

Node **spatial reasoning** may confound the relationship, acting as a common cause that influences multiple outcomes.

### 4. spatial reasoning

Node **spatial reasoning** may confound the relationship, acting as a common cause that influences multiple outcomes.

### 5. spatial reasoning

Node **spatial reasoning** may confound the relationship, acting as a common cause that influences multiple outcomes.

### 6. creativity

Node **creativity** may confound the relationship, acting as a common cause that influences multiple outcomes.

### 7. creativity

Node **creativity** may confound the relationship, acting as a common cause that influences multiple outcomes.

## Taxonomy Relationships

Hierarchical is-a and part-of relationships organizing concepts.

### 1. specific knowledge ↔ performance

A hierarchical relationship connects **specific knowledge** and **performance** through is-a or part-of relations.

### 2. high iq ↔ creativity

A hierarchical relationship connects **high iq** and **creativity** through is-a or part-of relations.

### 3. inductive inference ↔ similarity

A hierarchical relationship connects **inductive inference** and **similarity** through is-a or part-of relations.

### 4. judgment ↔ weighting bias

A hierarchical relationship connects **judgment** and **weighting bias** through is-a or part-of relations.

### 5. language production ↔ schizophrenia

A hierarchical relationship connects **language production** and **schizophrenia** through is-a or part-of relations.

### 6. phonetic perception ↔ linguistic experience

A hierarchical relationship connects **phonetic perception** and **linguistic experience** through is-a or part-of relations.

### 7. epistemic justification ↔ propositional knowledge

A hierarchical relationship connects **epistemic justification** and **propositional knowledge** through is-a or part-of relations.

### 8. epistemic appraisal ↔ ethical appraisal

A hierarchical relationship connects **epistemic appraisal** and **ethical appraisal** through is-a or part-of relations.

### 9. temporal contiguity ↔ causality

A hierarchical relationship connects **temporal contiguity** and **causality** through is-a or part-of relations.

## Domain Bridges

Cross-domain connectors enabling interdisciplinary knowledge transfer.

### 1. intelligence

Node **intelligence** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 2. chimpanzee

Node **chimpanzee** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 3. concept

Node **concept** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 4. cognitive development

Node **cognitive development** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 5. learning

Node **learning** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 6. decision making

Node **decision making** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 7. mental model

Node **mental model** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 8. belief

Node **belief** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 9. language

Node **language** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

### 10. justification

Node **justification** bridges multiple domains or disciplines, connecting otherwise separate areas of knowledge.

## Logical Entailments

Inferred implications derived from logical reasoning.

### 1. karl duncker ⇒ production system control structure

Logical relationships suggest that **karl duncker** may entail or imply **production system control structure**.

### 2. handbook ⇒ initial impasse

Logical relationships suggest that **handbook** may entail or imply **initial impasse**.

### 3. individualism ⇒ nisbett

Logical relationships suggest that **individualism** may entail or imply **nisbett**.

### 4. knowledge acquisition ⇒ belief-laden content

Logical relationships suggest that **knowledge acquisition** may entail or imply **belief-laden content**.

### 5. oakhill, j. ⇒ premise misinterpretation

Logical relationships suggest that **oakhill, j.** may entail or imply **premise misinterpretation**.

### 6. amnesiac patient ⇒ amnesic patient

Logical relationships suggest that **amnesiac patient** may entail or imply **amnesic patient**.

### 7. eliminative connectionist ⇒ lisaese

Logical relationships suggest that **eliminative connectionist** may entail or imply **lisaese**.

### 8. kallio ⇒ primate

Logical relationships suggest that **kallio** may entail or imply **primate**.

### 9. hagmayer ⇒ misperception

Logical relationships suggest that **hagmayer** may entail or imply **misperception**.

### 10. dunbar, k. ⇒ incremental analogy model

Logical relationships suggest that **dunbar, k.** may entail or imply **incremental analogy model**.

## Conclusions and Recommendations

Based on the analysis of 146 discovered insights, the following recommendations are made:

1. **Protect Bridge Entities**: The 8 identified bridge entities are critical for knowledge connectivity. Consider documenting these thoroughly and ensuring they are well-maintained.

5. **Review Core–Periphery Roles**: The 21 core-periphery insights highlight which entities anchor the graph versus those on the periphery.

6. **Review Text Similarity Links**: The 3 text similarity links surface entities with near-duplicate or closely related labels.

10. **Review Central Entities**: The 1 centrality findings highlight influential entities to prioritize for curation.

11. **Inspect Community Clusters**: The 1 detected communities can guide topic segmentation or subgraph analysis.

12. **Assess k-Core Nodes**: The 1 k-core entities represent dense cores worth validating or expanding.

13. **Validate k-Truss Links**: The 2 k-truss edges reflect strong local cohesion and should be verified.

41. **Validate Meta-Path Links**: The 20 meta-path links connect entities through typed relation sequences. Validate higher-confidence candidates for knowledge graph augmentation.

43. **Target Intervention Points**: The 20 intervention points are nodes whose removal disconnects causal chains. These are high-value targets for experimental design.

45. **Control for Confounders**: The 7 identified confounders connect to both ends of suspected causal relations. Account for these in experimental or statistical analyses.

46. **Refine Taxonomic Structure**: The 9 taxonomic relations surface is-a and part-of hierarchies. Use them to improve ontological consistency and support classification tasks.

47. **Leverage Domain Bridges**: The 20 domain bridge nodes connect distinct disciplines. These are prime candidates for interdisciplinary hypotheses.

48. **Expand via Logical Entailments**: The 20 entailments derive implicit implications from existing relations. Validate and add confirmed entailments to enrich the knowledge graph.

55. **Explore Hierarchical Structure**: The 1 multi-resolution communities reveal organization at different granularities. Use fine-grained scales for detailed analysis and coarse scales for high-level overview.

56. **Leverage Inter-Community Bridges**: The 11 bridge mappings identify entities that connect distinct communities. Focus on high-scoring bridges to understand cross-domain knowledge transfer.

57. **Recognize Structural Templates**: The 1 meta-patterns identify frequently recurring structural configurations. Use them to guide targeted searches for similar patterns elsewhere in the graph.

---

*This report was automatically generated by the Knowledge Discovery Engine. All insights should be validated by domain experts before taking action.*

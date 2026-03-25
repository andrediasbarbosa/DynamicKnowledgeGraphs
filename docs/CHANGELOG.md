# Changelog

## [Unreleased]

### Added (2026-03-25)
- **Level 1.2: Label Simplification** in Quality Control
  - Automatically simplifies verbose entity names
  - Removes filler phrases: "the process of", "the concept of", "a method for", etc.
  - Trims redundant suffixes: " process", " technique", " method"
  - Example: "the process of machine learning algorithms" → "machine learning algorithms"
  - Tracked in `labels_simplified` metric in cleaning reports
  - Reduces noise from overly descriptive entity names

- **Hub Node Identification** in Connectivity Analysis
  - Identifies the highest-degree node in the largest connected component
  - Represents the central concept in the knowledge graph
  - Displayed in all reports (HTML/JSON/console)
  - Format: `Hub: "machine learning" (degree: 45)`
  - Helps identify the most interconnected entity

- **Enhanced Connectivity Reporting**
  - Added `hub_node_label` and `hub_node_degree` to JSON reports
  - Updated HTML quality control report with hub node display
  - Improved console output with hub node information
  - Restructured JSON for better organization (nested level1/level2/level3 objects)

### Changed (2026-03-25)
- **LLM Provider Configuration**
  - Switched from OpenAI to Google Gemini support
  - Updated hardcoded model defaults: `gemini-1.5-flash` → `gemini-2.5-flash`
  - Fixed API compatibility issues with Gemini
  - Model configuration now supports latest Gemini models

- **Documentation Updates**
  - Updated `QUALITY_CONTROL.md` with new features (Level 1.2, Hub Node)
  - Enhanced examples and algorithm flow documentation
  - Updated metric descriptions and interpretations

### Removed (2026-03-25)
- **Obsolete Documentation Files** (12 files removed)
  - All `PHASE1_DAY*.md` daily summary files
  - All `PHASE2_DAY*.md` daily summary files
  - `SESSION_SUMMARY_*.md` session logs
  - `CATEGORY_REORGANIZATION.md` (outdated)
  - `CLEANUP_AUDIT.md` (outdated)
  - `DOCUMENTATION_UPDATES.md` (meta-doc no longer needed)
  - `PHASE1_PROGRESS.md` (progress tracking, obsolete)
  - `PHASE1_DEDUPLICATION_DESIGN.md` (superseded by QUALITY_CONTROL.md)
  - `PHASE2_ENHANCEMENT_PLAN.md` and `PHASE2_PLAN.md` (planning docs, obsolete)
  - `VISUAL_ENHANCEMENTS_*.md` (multiple versions, obsolete)
  - `REPORT_UI_IMPROVEMENTS.md` (merged into other docs)
  - `KUZU_UPGRADE_PROPOSAL.md` (proposal, no longer needed)
  - Streamlined docs/ directory from 42 to 18 files

### Fixed (2026-03-25)
- **Gemini LLM Integration**
  - Fixed model name incompatibility (v1beta API requires different model names)
  - Updated C++ code to use `gemini-2.5-flash` instead of non-existent `gemini-1.5-flash`
  - Fixed API endpoint format for Gemini generateContent calls
  - Tested and verified Gemini API connectivity

### Added (2026-03-06)
- **Knowledge Discovery Category System**:
  - Introduced 3-category classification for insights: **Combinatorial** (pattern detection), **Exploratory** (path finding), **Transformational** (reframing)
  - Added `InsightCategory` enum and automatic category assignment in `include/discovery/insight.hpp`
  - Updated all 67+ insight types with automatic category mapping
  - Category distribution now included in JSON output and reports
  - Documentation: `KNOWLEDGE_DISCOVERY_CATEGORIES.md`

### Changed (2026-03-06)
- **HTML Report Complete Reorganization**:
  - **Category-organized layout**: All 51 module sections reorganized by epistemological category
  - **Hierarchical Table of Contents**: Nested navigation with 3 main categories and subsections
  - **Improved chart visualization**: Replaced doughnut chart with horizontal bar chart showing category distribution
    - Click bars to navigate to category sections
    - Tooltips show count, percentage, and category descriptions
  - **Clean initial view**: All sections start collapsed except Categories Overview and Executive Summary
  - **Removed clutter**: Removed search box and filter controls for simplified interface
  - **Category section headers**: Beautiful gradient headers dividing content into 3 main sections
  - **Better performance**: Faster initial page load with collapsed sections
  - See: `REPORT_UI_IMPROVEMENTS.md` and `CATEGORY_REORGANIZATION.md`

### Added (2026-02-21)
- **Kuzu Explorer Enhancements**:
  - **Enhanced Query Templates**: Expanded from 4 to 20+ Cypher templates organized in 4 categories
    - 📍 Entity Discovery (5 templates): by degree, document, confidence, label
    - 🔗 Relationship Analysis (6 templates): paths, bidirectional, multi-hop, confidence ranges
    - 📊 Aggregations (5 templates): counts, averages, distributions
    - 🔧 Maintenance & Quality (5 templates): orphaned entities, low confidence, duplicates
  - **Data Quality Dashboard**: Added to Overview tab
    - Health score (0-100) with visual progress bar
    - Issue detection: low confidence relationships, orphaned entities, missing labels
    - Percentage breakdowns and color-coded display
  - **Entity Search Improvements**:
    - Clear button to reset search
    - Auto-clear previous results on new search
    - Entity details panel auto-hides when starting new search
    - Fixed entity display (name vs label) in results
- **New Backend Endpoints**:
  - `GET /api/relations` - List relation types with counts
  - `GET /api/relations/{composite_hash}` - Get incoming/outgoing relations for an entity
  - `GET /api/quality` - Calculate data quality health score and issues
- **Dual-Mode Deployment Support**:
  - Standard deployment: Local embeddings with sentence-transformers
  - Corporate deployment: Azure OpenAI only (firewall-compatible, no model downloads)
  - Environment variable: `EMBEDDING_MODE=local` or `EMBEDDING_MODE=azure`
  - Separate requirements files: `requirements.txt` and `requirements-azure.txt`
- **Documentation Consolidation**: Created 4 comprehensive guides
  - `DEPLOYMENT.md` - Complete deployment instructions (Windows/Linux, Local/Azure)
  - `FEATURES.md` - Full feature guide (RAG modes, UI components, capabilities)
  - `TROUBLESHOOTING.md` - Common issues and solutions
  - Updated `README.md` with clear navigation to all docs

### Changed (2026-02-21)
- **RAG Mode Visual Differentiation**: Enhanced Document-RAG vs Graph-RAG display
  - **Graph-RAG**: 🔵 Blue header + cyan-bordered panel, shows Cypher query and graph traversal results
  - **Document-RAG**: 🟠 Orange header + orange-bordered panel, shows vector similarity scores with star ratings
  - **Compare Mode**: Dedicated panel showing both methodologies side-by-side
  - Added methodology explanations to each panel
  - Added enhanced console logging for mode verification
- **Improved Methodology Display**:
  - Graph-RAG explicitly states: "🔵 GRAPH-RAG MODE (Cypher Query + Graph Traversal)"
  - Document-RAG explicitly states: "🟠 DOCUMENT-RAG MODE (Vector Similarity Search)" and "NO Cypher queries - pure semantic matching"
  - Similarity scores with star ratings: ⭐⭐⭐ (high 0.8+), ⭐⭐ (medium 0.5-0.8), ⭐ (low 0.3-0.5)
  - Each panel includes explanation box describing the approach
- **Kuzu DB Explorer**: Removed Documents tab to streamline interface
  - Panel now has 5 tabs: Overview, Schema, Query, Explorer, Paths
  - Backend endpoints preserved for potential future use

### Fixed (2026-02-21)
- **First Query 500 Error**: Fixed server error on initial API call before graph loaded
  - Modified `get_graph_schema()` to gracefully handle missing database tables
  - Returns empty schema instead of crashing when Entity table doesn't exist
  - Added validation in `query_graph_rag()` and `query_document_rag()` to detect empty schema
  - Now returns helpful "Please load graph first" message instead of 500 error
- **Kuzu Explorer "Explore Relations" Button**: Fixed entity highlighting bug
  - Button was using `entity.label` (type like "PERSON") instead of `entity.name` (actual name like "Damascus")
  - Now correctly highlights the selected entity in 3D graph visualization
- **Document-RAG Mode Rendering**: Fixed issue where Document-RAG showed same information as Graph-RAG
  - Added missing Compare mode handler
  - Enhanced visual differentiation with color-coded borders and emoji headers (🔵 Graph-RAG, 🟠 Document-RAG)
  - Fixed panel titles to clearly indicate mode
  - Added explicit methodology statements ("NO Cypher queries" for Document-RAG)
- **Kuzu Explorer Search**: Fixed entity search not clearing previous results
  - New searches now auto-clear previous entity details panel
  - Added Clear button for easy reset
  - Fixed entity display showing name instead of label
  - Added null-safety for edge cases
- **Kuzu 0.7.1 Compatibility**: Ensured all queries compatible with Kuzu 0.7.1 limitations
  - LIMIT parameters injected directly (no parameter binding)
  - Removed use of nodes()/relationships() functions (not available in 0.7.1)
  - All path queries use alternative approaches
- **Browser Access**: Clarified importance of using `http://localhost:8000` (not `file://` protocol)
  - Updated all documentation with this critical requirement
  - Added troubleshooting section for CORS errors

### Added (2026-02-20)
- **Kuzu DB Explorer UI (5 tabs)** in `src/graph_rag_tool/template.html`:
  - `Overview`, `Schema`, `Query`, `Explorer`, `Paths`
  - Custom Cypher query execution with query history
  - JSON/CSV export for query results
  - Entity search/details/relationship navigation
  - Path finder with hop control and graph highlighting
  - Keyboard shortcuts: `Ctrl/Cmd+K` toggle explorer, `Esc` close side panels
- **New backend endpoints** in `src/graph_rag_tool/backend/graph_rag_server.py`:
  - `POST /api/cypher`
  - `POST /api/clean`
  - `GET /api/relations`
  - `GET /api/relations/{relation_type}/samples`
  - `GET /api/entities`
  - `GET /api/entities/{entity_id}`
  - `GET /api/relations/{composite_hash}`
  - `POST /api/path`
- **Graph upload UX improvements**:
  - Upload flow now renders 3D graph in frontend first, then initializes backend DB
  - Overlay supports HTML progress rendering for structured status updates

### Changed (2026-02-20)
- **Documentation Consolidation**: Merged overlapping Markdown guides into a smaller canonical set aligned with current code paths and runtime behavior.
  - Root canonical docs: `README.md`, `CHANGELOG.md`, `KUZU_UPGRADE_PROPOSAL.md`
  - Submodule canonical docs: `src/graph_rag_tool/README.md`, `src/graph_rag_tool/backend/README.md`, `src/python_porting/README.md`
  - Removed legacy or duplicate docs that overlapped these guides (pipeline, config, setup, status, and quickstart variants).

### Added (2026-02-09)
- **Long-Chain Reasoning**: Added `long_chain` operator for distant multi-hop connections, with scoring based on hop length, confidence, and novelty.
- **Meta-Path Pattern Mining**: Added `meta_path_pattern` operator to surface frequent relation-sequence templates.
- **Mechanism Consolidation**: Added `mechanism_consolidation` operator to cluster overlapping mechanistic findings.
- **Cross-Operator Synthesis Operators**:
  - `intervention_bottleneck`: Finds high-leverage bottleneck nodes across causal chains
  - `competing_mechanisms`: Surfaces alternative mechanism explanations for the same outcomes
  - `schema_repair`: Proposes executable repair actions from schema violations and closure gaps
  - `cross_community_mechanism_bridge`: Detects mechanistic bridges across communities/domains
  - `meta_path_anomaly`: Flags rare but plausible relation-sequence templates
  - `evidence_fusion`: Fuses multi-operator evidence into consensus link candidates
- **Operator Registry**: `-p all` now includes all **55** registry operators, shared by `kg run`, `kg discover`, CLI help text, and `kg list-operators`.
- **Adaptive Thresholding**: New synthesis operators apply graph-size-aware thresholds when `DiscoveryConfig::adaptive_thresholds=true`.

### Fixed (2026-02-07)
- **HTML Report Generation**: Fixed missing operators in HTML reports so report modules and summaries are generated from the same operator registry as discovery execution.
  - Added CSS theme colors for 4 advanced reasoning operators
  - Added summary cards for taxonomy, domain_bridge, logical_entailment, compositional_reasoning
  - Added table of contents entries for all operators
  - Added detailed content sections with method explanations and findings display
  - Added recommendations section entries
  - Files modified: [src/discovery/report_generator.cpp](src/discovery/report_generator.cpp)
  - Operators affected: `taxonomy`, `domain_bridge`, `logical_entailment`, `compositional_reasoning`

### Added (2026-01-28)
- **Advanced Reasoning Operators**: Implemented 4 new discovery operators for advanced structural and logical reasoning
  - `taxonomy`: Hierarchical is-a and part-of relationship extraction
  - `domain_bridge`: Interdisciplinary connector node detection
  - `logical_entailment`: IF-THEN rule inference from graph patterns
  - `compositional_reasoning`: Relation composition discovery (R1 ∘ R2 = R3)
- **Total Operators (at this milestone)**: System reached 46 operators across 6 categories
  - Structural (11)
  - Advanced Reasoning (7)
  - Causal Discovery (4)
  - Semantic/LLM (12)
  - Active Learning (4)
  - Link Prediction (8)

### Previous Updates (2025-2026)

#### Hypothesis Generation Enhancement (January 2026)
- Replaced original `find_hypotheses()` with 3 specialized algorithms:
  - `hypotheses_1`: Bayesian Hypothesis Network with probabilistic belief propagation
  - `hypotheses_2`: Typed mechanistic chains with explanation subgraphs
  - `hypotheses_3`: Counterfactual causal hypothesis generation with intervention simulation
- Lowered causal infrastructure thresholds to enable hypotheses_3 results

#### LLM Integration Enhancement
- Added Google Gemini provider support alongside OpenAI
- Enhanced LLM-assisted discovery operators:
  - `claim_stance`: Classify claim stance (supports/opposes/neutral)
  - `relation_induction`: Suggest canonical relation type names
  - `analogical_transfer`: Propose links by structural analogy

#### Structural Operators
- Added advanced structural discovery:
  - `centrality`: Identify high-centrality nodes (degree, betweenness, closeness)
  - `community_detection`: Detect communities using Louvain algorithm
  - `k_core`: Find k-core subgraphs
  - `k_truss`: Identify k-truss cohesive subgraphs
  - `diffusion`: Map influence pathways using PageRank-style random walks

#### Causal Discovery
- Implemented causal reasoning operators:
  - `causal_chains`: Detect directed cause-effect pathways
  - `intervention_points`: Find high-leverage nodes in causal pathways
  - `feedback_loops`: Detect reinforcing and balancing cycles
  - `confounders`: Identify potential confounding variables

#### Active Learning
- Enhanced active learning capabilities:
  - `uncertainty_sampling`: Surface low-confidence relations
  - `counterfactual`: Generate falsification probes
  - `hyperedge_prediction`: Predict multi-entity relations

#### Visualization
- Replaced Canvas renderer with ForceGraph3D WebGL for better performance
- Added clustering support in 3D visualization
- Interactive HTML viewer with node/edge inspection

#### Preprocessing
- Added optional preprocessing stage between extraction and discovery:
  - Relation normalization (canonicalize relation types)
  - Alias merging (consolidate equivalent entities)
  - Enable with `--preprocess` flag

---

## Version History

### v1.0.0 - Production Ready (January 2026)
- Complete PDF processing pipeline
- LLM integration (OpenAI + Gemini)
- 46 discovery operators at v1.0.0 (expanded to 55 in current unreleased state)
- Interactive 3D visualization
- HTML and Markdown report generation
- Full documentation

### v0.9.0 - Beta (December 2025)
- Core pipeline implementation
- Basic discovery operators
- Initial visualization support

### v0.1.0 - Alpha (November 2025)
- Project initialization
- Foundation and architecture design

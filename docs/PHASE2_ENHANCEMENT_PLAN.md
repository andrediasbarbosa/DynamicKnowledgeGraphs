# Phase 2 Causal Metadata: Enhancement Opportunities

**Status:** Day 14 Review
**Date:** 2026-03-22

---

## 🎯 Current State

Phase 2 Days 11-14 implemented:
✅ Causal metadata structures (CausalMetadata)
✅ HyperEdge causal support (optional causal_metadata field)
✅ LLM causal extraction prompts
✅ JSON parsing and serialization
✅ CLI integration (`--causal` flag)

---

## 🔍 Discovery: Existing Causal Infrastructure

### **Causal-Related Discovery Operators (Already Exist)**

The system already has 4 causal-focused operators:

1. **causal_chains** (`InsightType::CAUSAL_CHAIN`)
   - Detects directed sequences of cause-effect relationships
   - Currently: Pattern-based (follows directed edges)
   - Location: Referenced in `report_generator.cpp:3696`, `insight.hpp:85`

2. **intervention_points** (`InsightType::INTERVENTION_POINT`)
   - Finds critical nodes whose removal disconnects causal pathways
   - Currently: Graph structure analysis
   - Location: `report_generator.cpp:3724`

3. **feedback_loops** (`InsightType::FEEDBACK_LOOP`)
   - Detects cycles in causal relationships (self-reinforcing dynamics)
   - Currently: Cycle detection in directed graphs
   - Location: `report_generator.cpp:3750`

4. **confounders** (`InsightType::CONFOUNDER`)
   - Finds nodes connected to both ends of suspected causal relations
   - Currently: Graph topology analysis
   - Location: `insight.hpp:88`

### **Report Generation (Already Exists)**

- `generate_causal_chains_section()` - Displays causal chain insights
- `describe_causal_chain()` - Generates narrative for chains
- `convert_causal_chain()` - Renders in augmented visualization

---

## ⚠️ Gap Analysis: Phase 2 Metadata Not Utilized

### **Problem:**

These operators and reports work with **graph structure** (directed edges, paths) but **don't leverage Phase 2 causal metadata**:

❌ **Not using:**
- Causal type (direct_cause, necessary, sufficient, etc.)
- Causal strength (weak, moderate, strong, deterministic)
- Mechanism descriptions
- Temporal aspects (immediate, long-term, etc.)
- Necessary/sufficient conditions
- Evidence sources and confidence

### **Current Report Output:**

```
A causal chain involving **CO2** and **global warming** suggests
a directional flow of influence through the graph.
```

### **Potential Enhanced Output (with Phase 2 metadata):**

```
A STRONG causal chain: **CO2** directly causes **global warming**
through the greenhouse effect (physical mechanism). This causal
relationship operates over the LONG-TERM (decades), with evidence
from ice core data and atmospheric measurements (confidence: 0.95).

Mechanism: CO2 molecules trap infrared radiation, preventing heat
from escaping to space.
```

---

## 🚀 Recommended Enhancements

### **Priority 1: Enhanced Report Narratives (Week 3)**

**File:** `src/discovery/report_generator.cpp`

**Enhancement:** Update `describe_causal_chain()` to show Phase 2 metadata

```cpp
std::string ReportGenerator::describe_causal_chain(const Insight& insight) const {
    std::stringstream ss;
    if (insight.seed_labels.empty()) return "";

    // Check if witness edges have causal metadata
    if (!insight.witness_edges.empty()) {
        // Look up edge from hypergraph
        auto edge_id = insight.witness_edges[0];
        auto edge = graph_->get_edge(edge_id);  // Need access to hypergraph

        if (edge.is_causal()) {
            const auto& causal = *edge.causal_metadata;

            // Show causal type and strength
            ss << "A **" << causal.get_strength_string() << "** causal chain: ";
            ss << "**" << insight.seed_labels[0] << "** ";
            ss << causal.get_type_string() << " **" << insight.seed_labels[1] << "**";

            // Show mechanism if available
            if (!causal.mechanism_description.empty()) {
                ss << " through " << causal.mechanism_description;
                if (!causal.mechanism_type.empty()) {
                    ss << " (" << causal.mechanism_type << " mechanism)";
                }
            }

            // Show temporality
            ss << ". This operates over the **" << causal.get_temporality_string() << "**";

            // Show evidence
            if (!causal.evidence_sources.empty()) {
                ss << ", with evidence from " << causal.evidence_sources[0];
            }

            ss << " (confidence: " << causal.confidence << ").";

            return ss.str();
        }
    }

    // Fallback to generic description if no causal metadata
    ss << "A causal chain involving **" << insight.seed_labels[0] << "**";
    if (insight.seed_labels.size() > 1) ss << " and **" << insight.seed_labels[1] << "**";
    ss << " suggests a directional flow of influence through the graph.";
    return ss.str();
}
```

**Benefit:** Reports automatically show rich causal information when available

---

### **Priority 2: Metadata-Aware Operators (Week 3-4)**

**Goal:** Enhance causal operators to use Phase 2 metadata

#### **2a. Causal Chain Operator Enhancement**

Filter and rank chains by:
- **Strength:** Prioritize STRONG and DETERMINISTIC causal links
- **Type:** Identify NECESSARY vs SUFFICIENT conditions
- **Mechanism:** Trace mechanism chains
- **Temporal consistency:** Validate temporal ordering

```cpp
// Pseudo-code
for (auto& chain : candidate_chains) {
    double strength_score = 0.0;
    int strong_links = 0;

    for (auto& edge_id : chain.edges) {
        auto edge = graph.get_edge(edge_id);
        if (edge.is_causal()) {
            strength_score += edge.causal_metadata->get_strength_score();
            if (edge.causal_metadata->strength >= CausalStrength::STRONG) {
                strong_links++;
            }
        }
    }

    chain.quality_score = (strength_score / chain.edges.size()) * strong_links;
}

// Rank by quality_score
```

#### **2b. Intervention Point Enhancement**

Identify **high-leverage** intervention points:
- Nodes in NECESSARY causal links (removing breaks causation)
- Nodes in DETERMINISTIC chains (predictable outcomes)
- Bottlenecks in mechanism chains

#### **2c. Feedback Loop Enhancement**

Classify feedback loops by:
- **Strength:** Strong vs weak reinforcement
- **Temporality:** Immediate vs delayed feedback
- **Type:** Positive (amplifying) vs negative (dampening)

---

### **Priority 3: HTML Report Visualization (Week 4)**

**File:** `src/discovery/report_generator.cpp` (HTML generation)

Add causal metadata badges/tags in HTML reports:

```html
<div class="causal-chain">
    <h3>CO2 → Global Warming</h3>
    <span class="badge badge-strength-strong">STRONG</span>
    <span class="badge badge-type-direct">Direct Cause</span>
    <span class="badge badge-temporality-long">Long-term</span>
    <p><strong>Mechanism:</strong> greenhouse effect (physical)</p>
    <p><strong>Evidence:</strong> ice core data, atmospheric measurements</p>
    <p><strong>Confidence:</strong> 0.95</p>
</div>
```

---

### **Priority 4: Causal Filtering & Queries (Week 5)**

Add CLI options to filter by causal properties:

```bash
# Show only STRONG causal chains
kg discover --input graph.json --operators causal_chains \
            --causal-strength strong,deterministic

# Show only mechanisms of specific type
kg discover --input graph.json --operators causal_chains \
            --mechanism-type physical,chemical

# Show only immediate causation
kg discover --input graph.json --operators causal_chains \
            --temporality immediate,short_term
```

---

## 📅 Implementation Timeline

### **Week 3 (Days 16-20): Mechanism Representation**
- ✅ Day 11-14: Complete (causal structures + prompts + CLI)
- ✅ **Day 16-17:** Enhance report narratives (Priority 1)
- ✅ **Day 18-19:** Update causal_chains operator (Priority 2a)
- ✅ **Day 20:** Testing & validation

### **Week 4 (Days 21-25): Conditions & Logic**
- ✅ **Day 19-20:** Enhance intervention_points operator (Priority 2b)
- ✅ **Day 19-20:** Enhance feedback_loops operator (Priority 2c)
- ✅ **Day 21:** HTML visualization enhancements (Priority 3)
- 🔨 **Day 22-23:** Causal filtering queries (Priority 4)

### **Week 5 (Days 26-30): Query & Reasoning**
- 🔨 **Day 26-27:** Causal filtering queries (Priority 4)
- 🔨 **Day 28-29:** Reasoning operators (transitive, mediators)
- 🔨 **Day 30:** Phase 2 completion & documentation

---

## 🎯 Success Metrics

### **Report Quality**
- ✅ Show causal type in 100% of causal chain reports
- ✅ Include mechanism description when available
- ✅ Display temporal and strength information
- ✅ Visual badges in HTML reports (Priority 3)

### **Operator Enhancement**
- ✅ Causal chains ranked by strength (not just path length)
- ✅ Intervention points prioritize NECESSARY links
- ✅ Feedback loops classified by strength/type

### **HTML Visualization** (Priority 3)
- ✅ Color-coded strength badges (green/yellow/red)
- ✅ NECESSARY/SUFFICIENT type highlighting
- ✅ Temporal indicators (immediate/delayed)
- ✅ Mechanism type badges

### **User Experience**
- ✅ No breaking changes (backward compatible)
- ✅ Graceful fallback when metadata absent
- ✅ Enhanced narratives readable and informative
- ✅ At-a-glance visual understanding

---

## ✅ Immediate Action Items

**For Day 15-16:**

1. **Pass hypergraph to ReportGenerator** (if not already)
   - ReportGenerator needs access to edges to read causal_metadata
   - Check constructor: `ReportGenerator(const Hypergraph& graph, ...)`

2. **Enhance `describe_causal_chain()`**
   - Add causal metadata display
   - Graceful fallback if no metadata

3. **Update HTML report CSS**
   - Add styling for causal badges
   - Color-code by strength (green=strong, yellow=moderate, red=weak)

4. **Test with real extraction**
   - Run: `kg run --input paper.pdf --causal`
   - Generate report and verify causal metadata appears

---

## 📝 Notes

- **Backward Compatibility:** All enhancements must work whether causal metadata is present or not
- **Opt-in:** Causal extraction remains opt-in via `--causal` flag
- **Graceful Degradation:** Reports should still work with non-causal graphs
- **Performance:** Reading causal metadata should not slow down discovery

---

**Status:** Enhancement plan ready for Week 3 implementation
**Estimated Effort:** ~8-10 days (spread across Weeks 3-5)
**Risk:** Low (additive features, backward compatible)

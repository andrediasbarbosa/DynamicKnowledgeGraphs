# Publication Materials

This folder contains manuscript assets and paper-development notes. It is not the canonical source for the repository's current runtime behavior. For code-accurate implementation docs, use the root [`README.md`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/README.md) and the maintained docs under [`docs/`](/mnt/c/Users/homea/Documents/PhD/DynamicKGs/Batch4/docs/).

# Publication: Fairness-Aware Knowledge Discovery with Bridge-Mediated Analogical Reasoning

This folder contains all materials for the academic publication documenting the novel knowledge discovery features implemented in this project.

---

## 📄 Main Files

### Manuscript
- **`publication_draft.tex`** - Complete LaTeX manuscript (~9,500 words)
- **`publication_draft.md`** - Original Markdown version
- **`references.bib`** - BibTeX bibliography (25 citations)

### Compilation
```bash
# Quick compile
pdflatex publication_draft.tex && bibtex publication_draft && pdflatex publication_draft.tex && pdflatex publication_draft.tex

# Or using latexmk
latexmk -pdf publication_draft.tex
```

---

## 🎨 Figures

### Auto-Generated (7/10)
Run these scripts to generate most figures automatically:

```bash
# Install dependencies
pip install matplotlib seaborn numpy networkx

# Generate statistical figures (3, 6, 7, 8)
./generate_figures.py --output-dir figures

# Generate network figures (2, 5, 9)
./generate_network_figures.py --output-dir figures
```

### Manual Creation (2/10)
See `figure_specifications.md` for detailed instructions:
- **Figure 1**: System Architecture (Draw.io or TikZ)
- **Figure 4**: Pattern Library Structure (Draw.io or TikZ)

All figure specifications: [figure_specifications.md](figure_specifications.md)

---

## 📚 Documentation

- **`README_PUBLICATION.md`** - Complete publication guide
  - Compilation instructions
  - Submission checklist
  - Target venues
  - Troubleshooting

- **`publication_changes_summary.md`** - Detailed changelog
  - All content additions
  - Format improvements
  - Quality checklist

- **`session_complete_summary.md`** - Implementation session recap
  - Features developed
  - Timeline and efficiency
  - Research contributions

- **`implementation_progress.md`** - Development progress
  - Feature implementation details
  - Technical approach

- **`latest_feasibility_assessment.md`** - Future work analysis
  - Feasibility ratings
  - Implementation estimates

---

## 🚀 Quick Start

### 1. Generate Figures (5 minutes)
```bash
cd paper
./generate_figures.py --output-dir figures
./generate_network_figures.py --output-dir figures
```

### 2. Create Missing Figures (15 minutes)
- Use Draw.io for Figures 1 and 4 (see specs)

### 3. Compile PDF (2 minutes)
```bash
pdflatex publication_draft.tex
bibtex publication_draft
pdflatex publication_draft.tex
pdflatex publication_draft.tex
```

### 4. Review
- Check all figures display
- Verify cross-references work
- Proofread content

---

## 📊 Paper Contributions

### Novel Methodologies
1. **Bridge-Mediated Analogical Reasoning**
   - Cross-domain analogy discovery via bridge entities
   - 84% precision on 100+ analogies

2. **Pattern Library System**
   - Reusable knowledge templates
   - 70% cross-domain transferability

3. **Bias Audit Metrics**
   - Gini-based fairness measurement
   - Proactive inequality detection

### Key Results
- 20× development efficiency
- 84% precision on bridge analogies
- 70% pattern transferability
- Gini coefficient 0.38 (moderate fairness)

---

## 🎯 Target Venues

**Top-tier Conferences:**
- KDD 2026 (deadline ~Feb 2026)
- WWW 2026 (deadline ~Oct 2025)
- ISWC 2026 (deadline ~Apr 2026)

**Journals:**
- Semantic Web Journal
- IEEE TKDE

See `README_PUBLICATION.md` for detailed submission guidance.

---

## ✅ Pre-Submission Checklist

- [ ] All 10 figures created and placed in `figures/`
- [ ] Author names and affiliations added
- [ ] Repository URL updated
- [ ] Document compiles without errors
- [ ] All cross-references work
- [ ] Bibliography complete
- [ ] Proofread thoroughly
- [ ] Supplementary materials prepared

---

## 📁 File Structure

```
paper/
├── README.md                          # This file
├── README_PUBLICATION.md              # Detailed guide
├── publication_draft.tex              # Main LaTeX manuscript
├── publication_draft.md               # Original markdown
├── references.bib                     # Bibliography
├── generate_figures.py                # Auto-generate figs 3,6,7,8
├── generate_network_figures.py        # Auto-generate figs 2,5,9
├── figure_specifications.md           # Figure specs
├── publication_changes_summary.md     # Changelog
├── session_complete_summary.md        # Session recap
├── implementation_progress.md         # Dev progress
├── latest_feasibility_assessment.md   # Future work
└── figures/                           # Figure directory
    ├── bias_audit_results.pdf         # (auto)
    ├── bridge_analogy_example.pdf     # (auto)
    ├── community_structure.pdf        # (auto)
    ├── efficiency_chart.pdf           # (auto)
    ├── example_subgraph.pdf           # (auto)
    ├── pattern_distribution.pdf       # (auto)
    ├── recommendation_quality.pdf     # (auto)
    ├── system_architecture.pdf        # (manual - TODO)
    └── pattern_library_structure.pdf  # (manual - TODO)
```

---

## 🔧 Troubleshooting

**LaTeX won't compile?**
- Check if all figures exist in `figures/`
- Install: `sudo apt-get install texlive-full`

**Python scripts fail?**
- Install dependencies: `pip install matplotlib seaborn numpy networkx`
- Make executable: `chmod +x generate_*.py`

**Figures look bad?**
- Already set to 300 DPI
- For higher quality: edit `dpi=600` in scripts

---

## 📝 Word Count

- **Abstract**: ~200 words
- **Main Text**: ~7,500 words
- **Appendices**: ~1,000 words
- **Total**: ~9,500 words

Suitable for full conference papers (8-10 pages).

---

## 📅 Timeline to Submission

- **Day 1**: Generate figures (auto + manual)
- **Day 2**: Add author info, proofread
- **Day 3**: Internal review, revisions
- **Day 4**: Finalize and submit

**Estimated time to submission-ready: 3-4 days**

---

For complete instructions and details, see [README_PUBLICATION.md](README_PUBLICATION.md)

*Last updated: 2026-03-11*

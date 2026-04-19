# Global Claude Code Instructions

## Communication style
- Be concise. No preamble, no trailing summaries.
- Use markdown links for file references, not backticks.
- No emojis unless explicitly asked.

## General behaviour
- Never commit without being explicitly asked.
- Never force-push without confirming first (except security history rewrites when asked).
- Never include *.pdf, *.zip, or test/run data in commits.
- Never hardcode API keys — flag and remove them if found.
- Don't add error handling, comments, or abstractions beyond what the task requires.

## This project (DynamicKGs/Batch4)
- C++ hypergraph pipeline with Python backends and HTML/JS 3D viewers.
- LLM config lives in `.llm_config.json` (gitignored) — never commit it.
- `runs/`, `tests/`, `output/` are gitignored — never stage files from these dirs.
- The three main viewers are `graph.html`, `graph_rag.html`, `graph_augmented.html`
  generated from `src/graph/hypergraph_extended.cpp` and `src/render/augmentation_renderer.cpp`.
- Clustering uses Louvain on an entity co-occurrence projection (bipartite graph —
  entities never share direct edges, must project through relation nodes).
- Branch: `v2-causal-inference`; main branch: `main`.

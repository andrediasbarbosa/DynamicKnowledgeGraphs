#!/usr/bin/env python3
"""
Deployment wrapper for the shared Graph RAG backend.

The backend implementation lives in ../backend/graph_rag_server.py.
This wrapper keeps the deployment entrypoint stable while avoiding a second
copy of the server code.
"""

from __future__ import annotations

import importlib.util
import os
from pathlib import Path


DEPLOYMENT_DIR = Path(__file__).resolve().parent
BACKEND_SERVER = DEPLOYMENT_DIR.parent / "backend" / "graph_rag_server.py"


def load_backend_app():
    spec = importlib.util.spec_from_file_location(
        "graph_rag_backend_server", BACKEND_SERVER
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load Graph RAG backend from {BACKEND_SERVER}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.app


# Keep deployment assets and database paths rooted in this directory.
os.chdir(DEPLOYMENT_DIR)
app = load_backend_app()


if __name__ == "__main__":
    import uvicorn

    print("=" * 60)
    print("Graph RAG Backend Server with Kuzu Database")
    print("=" * 60)
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")

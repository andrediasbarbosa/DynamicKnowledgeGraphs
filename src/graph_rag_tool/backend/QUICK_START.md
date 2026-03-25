# Graph-RAG Kuzu Explorer - Quick Start Guide

Get up and running in under 5 minutes!

---

## Prerequisites

- **Python 3.8+** installed
- **Modern web browser** (Chrome, Firefox, Edge, Safari)

---

## Installation (2 minutes)

### Step 1: Install Dependencies

```bash
pip install -r requirements.txt
```

This installs:
- FastAPI (web framework)
- Uvicorn (server)
- Kuzu (graph database)
- Python-multipart (file uploads)

---

## Running the Application (1 minute)

### Step 2: Start Server

**Linux/macOS/WSL:**
```bash
chmod +x start.sh
./start.sh
```

**Windows:**
```cmd
start.bat
```

You should see:
```
════════════════════════════════════════════
  Starting Graph RAG Server
════════════════════════════════════════════

Access the application at:
  • http://localhost:8000
  • http://YOUR_IP:8000 (from Windows)

Press Ctrl+C to stop
════════════════════════════════════════════
```

---

## Using the Application (2 minutes)

### Step 3: Open Browser

Navigate to: **http://localhost:8000**

### Step 4: Load Sample Graph

1. Click **"📁 Load Graph JSON"** button (top-right)
2. Select `examples/sample_graph.json`
3. Wait 2-3 seconds for visualization

**Success!** You should see:
- 🎨 3D graph visualization with 7 nodes
- ✅ Green success message overlay

### Step 5: Explore!

**3D Visualization:**
- Rotate: Left-click + drag
- Zoom: Scroll wheel
- Pan: Right-click + drag

**Database Explorer** (Press **Ctrl+K**):
- 📊 Overview: See 7 entities, 5 relation types
- 🗂️ Schema: Browse relationships
- 📝 Query: Run Cypher queries
- 🔍 Explorer: Search entities
- 🛤️ Paths: Find connections

---

## Try These Features

### Run a Query

1. Press **Ctrl+K** to open DB Explorer
2. Click **Query** tab
3. Select "Find entity neighbors" template
4. Change `'entity name here'` to `'machine learning'`
5. Click **Execute**

**Result**: See all connected entities!

### Find a Path

1. Click **Paths** tab
2. Source: `artificial intelligence`
3. Target: `computer vision`
4. Click **🔍 Find Shortest Path**

**Result**: See connection path highlighted!

### Search Entities

1. Click **Explorer** tab
2. Type `neural` in search
3. Click on "neural networks"
4. Click **✨ Highlight in Graph**

**Result**: Entity glows gold in 3D graph!

---

## Load Your Own Graph

### Step 6: Prepare Your Data

Your JSON must have this structure:

**📌 Important Notes:**
- Entities are unique by **(id + degree + docs)** combination
- Duplicates are automatically skipped (first occurrence kept)
- Each entity gets a 16-char hash for clean API calls
- Arrays are sorted automatically for consistent matching
```json
{
  "nodes": [
    {
      "id": "entity_1",
      "name": "Entity Name",
      "label": "Category",
      "degree": 5,
      "docs": ["source.pdf"],
      "isAug": false
    }
  ],
  "links": [
    {
      "source": "entity_1",
      "target": "entity_2",
      "predicate": "relates to",
      "confidence": 0.9,
      "source_document": "source.pdf",
      "source_chunk_id": "chunk_1",
      "edge_id": "edge_1"
    }
  ]
}
```

### Step 7: Upload Your Graph

1. Click **"📁 Load Graph JSON"**
2. Select your file
3. Wait for processing

**Done!** Your graph is now loaded and ready to explore.

---

## Keyboard Shortcuts

- **Ctrl+K** (or Cmd+K): Toggle DB Explorer
- **Esc**: Close panels
- **Enter**: Execute search/query (in inputs)

---

## Next Steps

📚 **Read the Full Guide**: See `README.md` for comprehensive documentation

🔧 **Customize**: Edit `template.html` for branding/features

🚀 **Deploy**: See deployment checklist in `README.md`

---

## Troubleshooting

**Port 8000 busy?**
```bash
# Kill existing process
pkill -f graph_rag_server

# Or change port in graph_rag_server.py (near the uvicorn.run(...) call at the end of the file)
```

**Graph won't load?**
- Check JSON is valid: https://jsonlint.com/
- Verify required fields present
- Check browser console (F12) for errors

**Server won't start?**
```bash
# Check Python version
python --version  # Must be 3.8+

# Reinstall dependencies
pip install -r requirements.txt --upgrade
```

---

## Getting Help

- Check `docs/TROUBLESHOOTING.md`
- Read `docs/USER_GUIDE.md`
- Review `docs/API_REFERENCE.md`

---

**You're all set!** Start exploring your knowledge graphs! 🎉

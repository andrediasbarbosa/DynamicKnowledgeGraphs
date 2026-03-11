# Graph-RAG Tool - Deployment Guide

Complete deployment instructions for both standard (local) and corporate (Azure-only) environments.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Standard Deployment (Local Embeddings)](#standard-deployment)
3. [Corporate Deployment (Azure OpenAI Only)](#corporate-deployment)
4. [System Requirements](#system-requirements)
5. [Troubleshooting](#troubleshooting)

---

## Quick Start

### For Windows Users (Recommended)

1. **Open Anaconda Prompt** (not PowerShell or CMD)

2. **Navigate to backend folder:**
   ```bash
   cd C:\Users\homea\Documents\PhD\DynamicKGs\Batch4\src\graph_rag_tool\backend
   ```

3. **Start the server:**
   ```bash
   python graph_rag_server.py
   ```

4. **Open browser:**
   ```
   http://localhost:8000
   ```

   ⚠️ **CRITICAL**: Use `http://localhost:8000` (NOT `file://` protocol)

5. **Load graph data:**
   - Click "📁 Load Graph JSON" button
   - Select your `graph_data.json` file

---

## Standard Deployment (Local Embeddings)

### Installation

**1. Install Python dependencies:**
```bash
pip install -r requirements.txt
```

**Dependencies installed:**
- `fastapi==0.115.0` - Web framework
- `uvicorn[standard]==0.32.0` - ASGI server
- `kuzu==0.7.1` - Graph database
- `openai==1.54.0` - LLM API
- `sentence-transformers==3.3.1` - Local embeddings
- `numpy==1.24.3` - Vector operations
- `pydantic==2.9.0` - Data validation
- `python-multipart==0.0.12` - File uploads

**2. Start the server:**
```bash
cd src/graph_rag_tool/backend
python graph_rag_server.py
```

**3. First run downloads:**
- Sentence transformers model: `all-MiniLM-L6-v2` (~90MB)
- Downloaded from HuggingFace (requires internet)
- Cached locally for subsequent runs

### Configuration

**Embedding Mode:** Local (default)
```python
EMBEDDING_MODE = "local"
EMBEDDING_DIM = 384  # all-MiniLM-L6-v2 dimension
```

**LLM Configuration:**
- Configure via web UI settings panel
- Supports: OpenAI, Azure OpenAI, local models

---

## Corporate Deployment (Azure OpenAI Only)

### Use Case

For environments that:
- Are behind corporate firewall
- Cannot download models from HuggingFace
- Have access to Azure OpenAI API only
- Run on Windows without C compiler

### Installation

**1. Install minimal dependencies:**
```bash
pip install -r requirements-azure.txt
```

**Dependencies (NO sentence-transformers):**
- `fastapi==0.115.0`
- `uvicorn[standard]==0.32.0`
- `kuzu==0.7.1`
- `openai==1.54.0`
- `pydantic==2.9.0`
- `numpy==1.24.3`
- `python-multipart==0.0.12`

**2. Set embedding mode:**

**Windows CMD:**
```cmd
set EMBEDDING_MODE=azure
```

**Windows PowerShell:**
```powershell
$env:EMBEDDING_MODE = "azure"
```

**Linux/Mac:**
```bash
export EMBEDDING_MODE=azure
```

**3. Start server:**
```bash
cd src/graph_rag_tool/backend
python graph_rag_server.py
```

**4. Configure Azure OpenAI in UI:**
- Open settings panel (⚙️)
- Select "Azure OpenAI"
- Enter:
  - API Key
  - Endpoint URL
  - Deployment name
  - API version

**5. Generate embeddings:**
```bash
# After uploading documents, click "Generate Embeddings" in UI
# OR use API endpoint:
curl -X POST http://localhost:8000/api/generate-embeddings \
  -H "Content-Type: application/json" \
  -d '{
    "azure_key": "your-key",
    "azure_endpoint": "https://your-resource.openai.azure.com",
    "azure_deployment": "text-embedding-3-small",
    "azure_version": "2024-05-01-preview"
  }'
```

### Azure Configuration

**Embedding Model:**
- Model: `text-embedding-3-small`
- Dimension: 1536 (vs 384 for local)
- Cost: ~$0.0001 per 1K tokens

**No Downloads Required:**
- Works entirely through Azure API
- No local model files
- No HuggingFace access needed

---

## System Requirements

### Standard Deployment

**Minimum:**
- Python 3.11 (recommended) or 3.12
- 4 GB RAM
- 2 GB disk space (for models)
- Internet connection (first run)

**Recommended:**
- Python 3.11
- 8 GB RAM
- SSD storage
- Multi-core CPU

### Corporate Deployment

**Minimum:**
- Python 3.11 or 3.12
- 2 GB RAM
- 500 MB disk space
- Azure OpenAI access

**Note:** No C compiler needed (numpy pre-built wheels)

---

## Windows Startup Guide

### Using Anaconda Prompt

**1. Open Anaconda Prompt:**
- Start Menu → Anaconda3 → Anaconda Prompt

**2. Navigate to project:**
```bash
cd C:\Users\homea\Documents\PhD\DynamicKGs\Batch4\src\graph_rag_tool\backend
```

**3. Start server:**
```bash
python graph_rag_server.py
```

**4. Server starts on:**
```
http://localhost:8000
```

**5. Open browser:**
- Navigate to: `http://localhost:8000`
- ⚠️ Must use `http://` protocol (NOT `file://`)

### Why Anaconda Prompt?

- ✅ Python environment pre-configured
- ✅ All dependencies accessible
- ✅ No PATH issues
- ✅ Consistent across Windows versions
- ❌ **Don't use:** `.bat` files (deprecated)
- ❌ **Don't use:** Regular CMD (missing environment)

---

## Server Modes

### Development Mode (Current)

```bash
python graph_rag_server.py
```

- Auto-reload on code changes
- Detailed logging
- CORS enabled for all origins
- No graph auto-load (manual load via UI)

### Production Mode

```bash
uvicorn graph_rag_server:app --host 0.0.0.0 --port 8000 --workers 4
```

- Multiple workers
- Optimized performance
- Configure CORS for specific domains
- Auto-load graph on startup (optional)

---

## Database Management

### Database Location

```
src/graph_rag_tool/backend/graph_rag_db/
```

### Reset Database

**If you encounter database corruption:**

**Windows CMD:**
```cmd
cd src\graph_rag_tool\backend
rmdir /s /q graph_rag_db
```

**Linux/Mac:**
```bash
cd src/graph_rag_tool/backend
rm -rf graph_rag_db
```

**Then restart server** - database will be recreated automatically.

### Switching Between WSL and Windows

⚠️ **IMPORTANT:** Kuzu databases are NOT compatible between WSL and Windows!

If you switch environments:
1. Delete `graph_rag_db` folder
2. Restart server
3. Re-load your graph JSON file

---

## Browser Access

### Correct URL

✅ **Correct:**
```
http://localhost:8000
```

❌ **Wrong:**
```
file:///C:/Users/.../template.html
```

### Why This Matters

- **`file://`** protocol → CORS errors, API calls fail
- **`http://`** protocol → Proper server communication

### Verification

Open browser console (F12) and check:
```javascript
// Should see:
fetch('http://localhost:8000/api/...')

// NOT:
fetch('file:///C:/...')
```

---

## Port Configuration

### Default Port: 8000

Change port if needed:

**In code (`graph_rag_server.py`):**
```python
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)  # Change here
```

**Via command line:**
```bash
python -m uvicorn graph_rag_server:app --port 8080
```

### Firewall Rules

If accessing from another machine, ensure port is open:

**Windows Firewall:**
```cmd
netsh advfirewall firewall add rule name="Graph-RAG Server" dir=in action=allow protocol=TCP localport=8000
```

---

## Environment Variables

### Standard Deployment

```bash
# Optional - defaults to "local"
export EMBEDDING_MODE=local
```

### Corporate Deployment

```bash
# Required for Azure mode
export EMBEDDING_MODE=azure
```

### Additional Settings

```bash
# Override embedding dimension
export EMBEDDING_DIM=1536  # For Azure

# Enable debug logging
export LOG_LEVEL=DEBUG
```

---

## Verification Checklist

After deployment, verify:

- [ ] Server starts without errors
- [ ] Browser loads UI at http://localhost:8000
- [ ] Can load graph JSON file
- [ ] 3D graph renders correctly
- [ ] Can execute Cypher queries in Kuzu Explorer
- [ ] Chat interface responds to questions
- [ ] Embeddings generate correctly (check mode)

---

## Next Steps

After successful deployment:

1. **Load Graph Data:**
   - Click "📁 Load Graph JSON"
   - Select your knowledge graph file

2. **Explore Features:**
   - See [FEATURES.md](FEATURES.md) for capabilities

3. **Run Queries:**
   - Use Kuzu Explorer for Cypher queries
   - Use Chat for natural language questions

4. **Troubleshooting:**
   - See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for common issues

---

## Support

- **Issues:** Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Features:** See [FEATURES.md](FEATURES.md)
- **Changes:** See [CHANGELOG.md](CHANGELOG.md)

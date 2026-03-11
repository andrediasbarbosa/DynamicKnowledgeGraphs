# Graph-RAG Tool - Troubleshooting Guide

Solutions to common issues, error messages, and configuration problems.

---

## Table of Contents

1. [Quick Diagnostics](#quick-diagnostics)
2. [Installation Issues](#installation-issues)
3. [Server Problems](#server-problems)
4. [Browser/UI Issues](#browserui-issues)
5. [Database Issues](#database-issues)
6. [RAG Mode Issues](#rag-mode-issues)
7. [Performance Issues](#performance-issues)
8. [Known Limitations](#known-limitations)

---

## Quick Diagnostics

### Checklist

Run through these quick checks:

- [ ] Server running? Check terminal for "Uvicorn running on..."
- [ ] Using `http://localhost:8000` (NOT `file://`)?
- [ ] Browser console shows errors? (Press F12)
- [ ] Database exists? Check `backend/graph_rag_db/` folder
- [ ] Correct Python version? (3.11 or 3.12)
- [ ] Dependencies installed? Run `pip list`

### Log Files

Check console output for:
```bash
# Server startup
✓ Embedding model loaded successfully!
✓ Database initialized

# Requests
INFO: 127.0.0.1:port - "POST /api/query HTTP/1.1" 200 OK
```

---

## Installation Issues

### Issue: `pip install` fails with "No matching distribution"

**Cause:** Python version too old or too new

**Solution:**
```bash
# Check Python version
python --version

# Should be 3.11 or 3.12
# If not, install correct version

# Windows (Anaconda)
conda install python=3.11

# Linux/Mac
pyenv install 3.11
pyenv local 3.11
```

### Issue: sentence-transformers installation fails

**Error:** `ERROR: Failed building wheel for sentence-transformers`

**Cause:** Missing C compiler or incompatible Python version

**Solution 1 - Use pre-built wheels (Python 3.11):**
```bash
pip install --upgrade pip
pip install sentence-transformers==3.3.1
```

**Solution 2 - Use Azure mode (no local embeddings):**
```bash
pip install -r requirements-azure.txt
set EMBEDDING_MODE=azure  # Windows CMD
```

### Issue: numpy compilation fails on Python 3.14

**Error:** `ERROR: No C compiler found`

**Cause:** Python 3.14 too new, no pre-built wheels

**Solution:**
```bash
# Downgrade to Python 3.11 (stable)
conda install python=3.11

# OR use conda to install numpy
conda install numpy=1.24.3
```

### Issue: Kuzu installation fails

**Error:** `Could not find a version that satisfies the requirement kuzu==0.7.1`

**Solution:**
```bash
# Upgrade pip
pip install --upgrade pip

# Try again
pip install kuzu==0.7.1

# If still fails, use conda
conda install -c conda-forge kuzu
```

---

## Server Problems

### Issue: Server won't start - "Address already in use"

**Error:** `OSError: [Errno 48] Address already in use`

**Cause:** Port 8000 already occupied

**Solution 1 - Kill existing process:**
```bash
# Windows
netstat -ano | findstr :8000
taskkill /PID <PID> /F

# Linux/Mac
lsof -ti:8000 | xargs kill -9
```

**Solution 2 - Use different port:**
```bash
# Edit graph_rag_server.py
uvicorn.run(app, host="0.0.0.0", port=8080)  # Change port
```

### Issue: Server starts but can't access from browser

**Cause:** Firewall blocking connections

**Solution:**
```bash
# Windows - Add firewall rule
netsh advfirewall firewall add rule name="Graph-RAG" dir=in action=allow protocol=TCP localport=8000

# Linux - Use ufw
sudo ufw allow 8000

# Mac - Check System Preferences → Security & Privacy → Firewall
```

### Issue: "Module not found" errors on startup

**Error:** `ModuleNotFoundError: No module named 'fastapi'`

**Cause:** Dependencies not installed or wrong environment

**Solution:**
```bash
# Verify you're in correct environment
# Anaconda Prompt should show: (base) or (your-env)

# Install dependencies
pip install -r requirements.txt

# Verify installation
pip list | grep fastapi
pip list | grep kuzu
```

### Issue: Embedding model won't download

**Error:** `Can't reach HuggingFace servers`

**Cause:** Firewall blocking, no internet, or corporate proxy

**Solution 1 - Use Azure mode:**
```bash
set EMBEDDING_MODE=azure
# Use Azure OpenAI for embeddings instead
```

**Solution 2 - Configure proxy:**
```bash
# Set proxy environment variables
export HTTP_PROXY=http://proxy.company.com:8080
export HTTPS_PROXY=http://proxy.company.com:8080

# Then start server
python graph_rag_server.py
```

**Solution 3 - Manual download:**
```bash
# Download model manually from HuggingFace
# Place in: ~/.cache/huggingface/hub/
```

---

## Browser/UI Issues

### Issue: Page loads but shows "file://" in URL

**Problem:** Opening HTML file directly instead of through server

**Visual Indicator:**
- URL bar shows: `file:///C:/Users/.../template.html`
- API calls fail
- Console shows CORS errors

**Solution:**
```
1. Close the browser tab
2. Make sure server is running (python graph_rag_server.py)
3. Open NEW tab
4. Navigate to: http://localhost:8000
5. Verify URL shows "http://" (NOT "file://")
```

### Issue: "500 Internal Server Error" on all queries

**Symptoms:**
- All queries return 500 error
- Console shows: `POST /api/query 500 Internal Server Error`

**Cause 1:** Using `file://` protocol

**Solution:** Use `http://localhost:8000`

**Cause 2:** Database not initialized

**Solution:**
```bash
# Check if database exists
ls backend/graph_rag_db/

# If not, load graph JSON in UI
# Click "📁 Load Graph JSON"
```

**Cause 3:** Database corruption

**Solution:**
```bash
# Delete and recreate database
cd src/graph_rag_tool/backend
rm -rf graph_rag_db  # Linux/Mac
rmdir /s /q graph_rag_db  # Windows

# Restart server and reload graph
```

### Issue: Graph won't render / Black screen

**Causes:**
- WebGL not supported
- GPU drivers outdated
- Browser too old

**Solution 1 - Check WebGL:**
```
Visit: https://get.webgl.org/
Should say: "Your browser supports WebGL"
```

**Solution 2 - Update drivers:**
```
Update GPU drivers from manufacturer:
- NVIDIA: nvidia.com/drivers
- AMD: amd.com/support
- Intel: intel.com/support
```

**Solution 3 - Enable WebGL in browser:**
```
Chrome: chrome://flags/#enable-webgl
Firefox: about:config → webgl.force-enabled = true
```

### Issue: UI buttons don't respond

**Causes:**
- JavaScript errors
- CORS issues
- Missing dependencies

**Solution:**
```
1. Open browser console (F12)
2. Look for errors (red text)
3. Common fixes:
   - Reload page (Ctrl+R)
   - Clear cache (Ctrl+Shift+Delete)
   - Try different browser
```

---

## Database Issues

### Issue: Database corruption after switching WSL/Windows

**Error:** `RuntimeError: invalid unordered_map<K, T> key`

**Cause:** Kuzu databases NOT compatible between WSL and Windows

**Solution:**
```bash
# Delete incompatible database
cd src/graph_rag_tool/backend

# Windows CMD
rmdir /s /q graph_rag_db

# Linux/Mac/WSL
rm -rf graph_rag_db

# Restart server - database recreates automatically
python graph_rag_server.py

# Reload your graph JSON in UI
```

### Issue: "Database locked" error

**Error:** `sqlite3.OperationalError: database is locked`

**Cause:** Another process accessing database

**Solution:**
```bash
# Kill all Python processes
# Windows
taskkill /IM python.exe /F

# Linux/Mac
pkill python

# Restart server
python graph_rag_server.py
```

### Issue: Kuzu LIMIT parameter binding fails

**Error:** `Parameter binding not supported for LIMIT`

**Cause:** Kuzu 0.7.1 limitation

**Solution:** Already fixed in code - LIMIT values injected directly:
```python
# Don't use parameter binding for LIMIT
query = f"MATCH (e:Entity) RETURN e LIMIT {limit}"  # ✓ Correct

# Don't do this:
query = "MATCH (e:Entity) RETURN e LIMIT $limit"  # ✗ Won't work
```

### Issue: RELATIONSHIPS function not found

**Error:** `function RELATIONSHIPS does not exist`

**Cause:** Kuzu 0.7.1 doesn't support `nodes()/relationships()` functions

**Solution:** Already fixed in code - use alternative:
```cypher
# Don't use RELATIONSHIPS(path)
MATCH path = (e1)-[r*1..3]-(e2)
RETURN RELATIONSHIPS(path)  # ✗ Won't work in 0.7.1

# Use this instead:
MATCH path = (e1)-[r*1..3]-(e2)
RETURN e1.name, e2.name, LENGTH(r)  # ✓ Works
```

---

## RAG Mode Issues

### Issue: Document-RAG shows same results as Graph-RAG

**Symptoms:**
- Orange panel not appearing
- Both modes look identical
- No similarity scores shown

**Cause:** Frontend rendering bug (FIXED)

**Verification:**
```javascript
// Open browser console, should see:
🎯 Displaying results for RAG mode: doc-rag
✅ Rendering Document-RAG (Vector Similarity) panel
📈 Similarity results: [...]
```

**If still seeing issue:**
```
1. Hard refresh: Ctrl+Shift+R
2. Clear cache
3. Verify template.html updated
4. Check server logs for correct mode
```

**Visual Check:**
- **Graph-RAG**: Cyan border, shows Cypher query
- **Document-RAG**: Orange border, shows similarity scores
- **Compare**: Both methods side-by-side

### Issue: No similarity scores in Document-RAG

**Cause:** Embeddings not generated

**Solution:**
```
1. Check embedding mode: console should show
   "→ Embedding mode: local" or "→ Embedding mode: azure"

2. Generate embeddings:
   - Local mode: Automatic on first query
   - Azure mode: Click "Generate Embeddings" button

3. Verify embeddings exist:
   - Query: SELECT COUNT(*) FROM Entity WHERE embedding IS NOT NULL
```

### Issue: Graph-RAG generates invalid Cypher

**Symptoms:**
- Query execution fails
- Syntax errors in generated Cypher

**Cause:** LLM hallucination or incorrect schema

**Solution:**
```
1. Check schema in Kuzu Explorer → Schema tab
2. Provide better prompt context
3. Use query templates as examples
4. Simplify question
```

**Prevention:**
```
Good questions:
✓ "Find entities related to Damascus"
✓ "Show connections between Syria and Mosque"

Bad questions:
✗ "Analyze the socio-political implications..." (too complex)
✗ "Run algorithm X on the graph" (not a retrieval task)
```

### Issue: Compare mode shows only one result

**Cause:** Missing compare mode handler (FIXED)

**Verification:**
- Should see: "🔀 Comparison Mode - Both Methods Used" panel
- Two-column layout with both answers

**If still broken:**
```
1. Check RAG mode selector value
2. Look for console errors
3. Verify template.html updated
```

---

## Performance Issues

### Issue: Query takes too long

**Symptoms:**
- Queries timeout
- UI freezes
- "Still loading..." message persists

**Solution 1 - Limit results:**
```cypher
-- Add LIMIT to every query
MATCH (e:Entity)-[r:RELATED]->(t)
RETURN e.name, r.relation_type, t.name
LIMIT 100  -- Add this
```

**Solution 2 - Simplify query:**
```cypher
-- Instead of multi-hop:
MATCH path = (e1)-[*1..5]-(e2)  -- ✗ Slow

-- Use fewer hops:
MATCH path = (e1)-[*1..2]-(e2)  -- ✓ Faster
```

**Solution 3 - Index commonly queried fields:**
```cypher
-- Ensure indexes on name fields
CREATE INDEX ON Entity(name)
```

### Issue: 3D graph rendering is slow

**Symptoms:**
- Laggy rotation/zoom
- Low FPS
- Browser freezes

**Solution:**
```
1. Reduce visible nodes:
   - Use filters in Explorer
   - Hide low-degree nodes
   - Limit search results

2. Disable clustering:
   - Clustering is expensive for large graphs
   - Use pre-clustered data

3. Hardware acceleration:
   - Enable GPU acceleration in browser settings
   - Update graphics drivers
```

### Issue: Embedding generation is slow

**Time Expectations:**
- Local mode: ~1-5 minutes per 1,000 entities
- Azure mode: ~5-15 minutes per 1,000 entities (API rate limits)

**Speed up:**
```
Local mode:
- Use faster machine
- Close other applications
- Use GPU if available (CUDA)

Azure mode:
- Increase API rate limit
- Use batch processing
- Request quota increase from Azure
```

---

## Known Limitations

### Kuzu 0.7.1 Limitations

**1. No parameter binding for LIMIT**
```cypher
# Won't work:
MATCH (e) RETURN e LIMIT $limit

# Must use:
MATCH (e) RETURN e LIMIT 100  # Direct value
```

**2. No nodes()/relationships() functions**
```cypher
# Won't work:
RETURN RELATIONSHIPS(path)

# Must use:
RETURN e1, e2, LENGTH(path)
```

**3. No full-text search**
```cypher
# Won't work:
WHERE e.name CONTAINS 'text'

# Must use:
WHERE LOWER(e.name) CONTAINS LOWER('text')  # Case-insensitive substring
```

### Graph Size Limits

**Tested Limits:**
- Entities: Up to 10,000
- Relationships: Up to 50,000
- 3D render: Optimal < 5,000 nodes

**Beyond limits:**
- Use subgraph queries
- Filter by document
- Export results instead of visualizing

### Browser Compatibility

**Fully Supported:**
- Chrome 90+
- Firefox 88+
- Edge 90+

**Partial Support:**
- Safari 14+ (WebGL may have issues)

**Not Supported:**
- Internet Explorer (any version)
- Mobile browsers (UI not optimized)

### LLM Limitations

**Hallucinations:**
- Graph-RAG may generate invalid Cypher
- Document-RAG may invent relationships
- Always verify critical information

**Context Window:**
- Limited by LLM (4K-128K tokens)
- Very large graphs may exceed context
- Use focused queries

**Cost:**
- API calls cost money
- Monitor usage in Azure/OpenAI dashboard
- Use cheaper models for testing (gpt-4o-mini)

---

## Getting Help

### Before Asking for Help

1. **Check this guide** - Most issues covered here
2. **Check console logs** - Browser (F12) and server terminal
3. **Try fresh start** - Delete database, restart server, reload browser
4. **Check versions** - Python, pip, dependencies

### Providing Information

When reporting issues, include:

```
1. Environment:
   - OS: Windows 11 / Linux / Mac
   - Python version: python --version
   - Installation mode: Standard / Azure

2. Error message:
   - Full error text from console
   - Stack trace if available

3. Steps to reproduce:
   - What did you do?
   - What happened?
   - What did you expect?

4. Logs:
   - Server terminal output
   - Browser console (F12) output

5. Configuration:
   - RAG mode used
   - Query attempted
   - Graph size (entity/relationship count)
```

---

## Emergency Reset

If all else fails, complete reset:

```bash
# 1. Stop server (Ctrl+C)

# 2. Delete database
cd src/graph_rag_tool/backend
rm -rf graph_rag_db  # Linux/Mac
rmdir /s /q graph_rag_db  # Windows

# 3. Clear browser cache
# Chrome: Ctrl+Shift+Delete → Clear browsing data
# Select: Cached images and files

# 4. Reinstall dependencies (optional)
pip uninstall -r requirements.txt -y
pip install -r requirements.txt

# 5. Restart server
python graph_rag_server.py

# 6. Open fresh browser tab
http://localhost:8000

# 7. Load graph JSON
Click "📁 Load Graph JSON"
```

---

## See Also

- **Deployment:** [DEPLOYMENT.md](DEPLOYMENT.md)
- **Features:** [FEATURES.md](FEATURES.md)
- **Changes:** [CHANGELOG.md](CHANGELOG.md)
- **README:** [README.md](README.md)

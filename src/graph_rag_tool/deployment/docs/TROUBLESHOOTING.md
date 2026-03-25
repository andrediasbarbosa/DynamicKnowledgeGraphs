# Troubleshooting Guide

Common issues and solutions for Graph-RAG Kuzu Explorer.

---

## Server Issues

### "Address already in use" Error

**Symptom**:
```
ERROR: [Errno 98] error while attempting to bind on address ('0.0.0.0', 8000): address already in use
```

**Cause**: Port 8000 is occupied by another process

**Solutions**:

**Option 1: Kill existing process**
```bash
# Linux/macOS/WSL
lsof -i :8000
kill -9 <PID>

# Or shorter
pkill -f graph_rag_server

# Windows
netstat -ano | findstr :8000
taskkill /PID <PID> /F
```

**Option 2: Change port**

Edit `graph_rag_server.py` (near the uvicorn.run(...) call at the end of the file):
```python
uvicorn.run(app, host="0.0.0.0", port=8001)  # Changed from 8000
```

Also update `template.html` (search for the API base URL in template.html):
```javascript
const BACKEND_URL = 'http://localhost:8001';  // Changed from 8000
```

---

### Server Won't Start

**Symptom**: Script exits immediately or shows errors

**Possible Causes & Solutions**:

**1. Python Version Too Old**
```bash
# Check version
python --version

# Must be 3.8 or higher
# Update Python if needed
```

**2. Missing Dependencies**
```bash
# Reinstall all dependencies
pip install -r requirements.txt --upgrade

# Or use virtual environment
python -m venv venv
source venv/bin/activate  # Linux/macOS
venv\Scripts\activate     # Windows
pip install -r requirements.txt
```

**3. Permission Issues (Linux/macOS)**
```bash
# Make start script executable
chmod +x start.sh

# Run with explicit python
python graph_rag_server.py
```

---

### Server Crashes After Starting

**Symptom**: Server starts then immediately stops

**Check**:
```bash
# Look for error messages in terminal
# Common issues:

# 1. Import errors
# Solution: pip install -r requirements.txt

# 2. Database corruption
# Solution: rm -rf graph_rag_db

# 3. Syntax errors (if you modified code)
# Solution: Compare with original
```

---

## Graph Upload Issues

### Upload Fails with "Invalid JSON"

**Symptom**: Error message shows "Failed to parse JSON" or similar

**Solutions**:

**1. Validate JSON**
```bash
# Online validator
# Visit: https://jsonlint.com/
# Paste your JSON and check for errors

# Command line (if you have jq)
jq . your_graph.json
```

**2. Check File Encoding**
- Must be UTF-8
- No BOM (Byte Order Mark)
- Unix line endings preferred

**3. Common JSON Errors**:
```json
// ❌ Trailing comma
{"nodes": [1, 2, 3,]}

// ✅ No trailing comma
{"nodes": [1, 2, 3]}

// ❌ Single quotes
{'name': 'value'}

// ✅ Double quotes
{"name": "value"}

// ❌ Comments (not allowed in JSON)
{"name": "value"} // This is a name

// ✅ No comments
{"name": "value"}
```

---

### Upload Times Out

**Symptom**: Loading bar stops, no completion

**Causes & Solutions**:

**1. File Too Large**
```bash
# Check file size
ls -lh your_graph.json

# If >50MB, consider:
# - Splitting into smaller graphs
# - Removing unnecessary fields
# - Compressing data
```

**2. Increase Timeout**

Edit `template.html` (search for the API base URL configuration block):
```javascript
const response = await fetch(`${BACKEND_URL}/api/upload`, {
    method: 'POST',
    body: formData,
    // Add timeout handling
    signal: AbortSignal.timeout(300000)  // 5 minutes
});
```

---

### "Failed to load database statistics"

**Symptom**: Alert or error when opening DB Explorer

**Solutions**:

**1. Wait for Upload to Complete**
- Don't open DB Explorer during upload
- Wait for "✅ Graph Loaded Successfully" message

**2. Refresh Page**
- Hard refresh: `Ctrl+Shift+R` (or `Cmd+Shift+R`)

**3. Check Server Logs**
```bash
# Server terminal should show:
# ✓ Database already initialized with N entities

# If errors appear, restart server
```

---

## Visualization Issues

### 3D Graph Not Showing

**Symptom**: Blank white/black area where graph should be

**Solutions**:

**1. Check WebGL Support**
```javascript
// Open browser console (F12)
// Paste and run:
const gl = document.createElement('canvas').getContext('webgl');
console.log(gl ? 'WebGL supported ✓' : 'WebGL NOT supported ✗');
```

**2. Enable WebGL in Browser**

**Chrome/Edge**:
- Go to `chrome://flags/`
- Search "WebGL"
- Enable if disabled
- Restart browser

**Firefox**:
- Go to `about:config`
- Search `webgl.disabled`
- Set to `false`
- Restart browser

**3. Update Graphics Drivers**
- Update GPU drivers to latest version
- Restart computer

**4. Try Different Browser**
- Chrome (recommended)
- Firefox
- Edge

---

### Graph Renders But Nodes Are Invisible

**Symptom**: Can interact but can't see nodes

**Solutions**:

**1. Check Node Colors**
- Open browser console (F12)
- Look for color-related errors

**2. Reset Camera**
- Click "Reset camera" button in left panel
- Or refresh page

**3. Check Data**
```javascript
// In browser console:
console.log(data.nodes);
// Should show array of nodes
// Each should have x, y, z, color, etc.
```

---

### Performance Issues / Lag

**Symptom**: Visualization is slow or choppy

**Solutions**:

**1. Reduce Max Nodes**
- Use slider in left panel
- Lower from 2500 to 1000 or 500

**2. Enable Auto-Freeze**
- Check "Auto-freeze physics" checkbox
- Graph will freeze after stabilizing

**3. Close Other Tabs**
- WebGL is GPU-intensive
- Close unnecessary browser tabs

**4. Disable Arrows**
- Uncheck "Directional arrows"
- Arrows add rendering overhead

**5. Reduce Graph Complexity**
- Use search to focus on subset
- Don't expand too many hops at once

---

## Database Issues

### "Database not initialized" Error

**Symptom**: Error when trying to query or explore

**Solutions**:

**1. Load a Graph First**
- Click "📁 Load Graph JSON"
- Select and upload a file

**2. Check Database Folder**
```bash
# Should exist after first upload
ls -la graph_rag_db/

# If missing or corrupted, delete and restart
rm -rf graph_rag_db
./start.sh
```

---

### Query Results Show "Error"

**Symptom**: Cypher query fails with error message

**Solutions**:

**1. Check Query Syntax**
```cypher
-- ✅ Correct
MATCH (e:Entity)
WHERE e.name = 'spain'
RETURN e

-- ❌ Wrong (missing quotes)
MATCH (e:Entity)
WHERE e.name = spain
RETURN e

-- ❌ Wrong (wrong node label)
MATCH (e:Node)  -- Should be Entity
RETURN e
```

**2. Check Field Names**
- Use `e.name` not `e.entity_name`
- Use `e.label` not `e.type`
- Use `r.relation_type` not `r.predicate`

**3. Use Templates**
- Click Query tab
- Select template from dropdown
- Modify template for your needs

---

### Search Returns No Results

**Symptom**: Entity search finds nothing

**Possible Causes**:

**1. Graph Not Loaded**
```bash
# Check in Overview tab
# Should show entity count > 0
```

**2. Entity Name Doesn't Match**
```bash
# Search is case-insensitive substring match
# "spain" matches "Spain", "spain", "SPAIN"
# "spai" also matches "spain"

# But "spinach" doesn't match "spain"
```

**3. Database Empty**
```bash
# Refresh page (database auto-cleans on load)
# Upload graph again
```

---

## Browser Issues

### Page Won't Load

**Symptom**: Browser shows "Connection refused" or timeout

**Solutions**:

**1. Check Server is Running**
```bash
# Terminal should show:
# INFO: Uvicorn running on http://0.0.0.0:8000

# If not, start server:
./start.sh
```

**2. Check URL**
- Use `http://localhost:8000` (NOT `https://`)
- Port must match server port

**3. Check Firewall**
- Firewall may block localhost connections
- Temporarily disable to test

---

### "Failed to fetch" Errors

**Symptom**: Console shows fetch errors

**Solutions**:

**1. CORS Issue (if accessing from different domain)**
```bash
# Server must allow origin
# Edit graph_rag_server.py CORS settings
```

**2. Server Not Responding**
```bash
# Check server terminal for errors
# Restart server
```

**3. Network Issue**
```bash
# Check network connectivity
# Try: curl http://localhost:8000/api/health
```

---

### Browser Crashes or Freezes

**Symptom**: Tab or browser becomes unresponsive

**Causes & Solutions**:

**1. Graph Too Large**
- Reduce max nodes (slider in left panel)
- Don't load graphs with >50,000 nodes at once

**2. Memory Leak**
- Close and reopen tab
- Clear browser cache
- Restart browser

**3. Too Many Hops Expanded**
- Don't click "+ 2 hops" repeatedly
- Reset graph between expansions

---

## Data Issues

### Wrong Entity Count Shown

**Symptom**: Overview shows different count than expected

**Solutions**:

**1. Check Original JSON**
```bash
# Count nodes in JSON
grep -c '"id"' your_graph.json

# Or use jq:
jq '.nodes | length' your_graph.json
```

**2. Filtered Nodes**
- Server filters out `isAug: true` nodes
- Only entities (`isAug: false`) are loaded

**3. Refresh Database**
- Reload page
- Upload graph again

---

### Entity Details Not Showing

**Symptom**: Click entity but details panel empty

**Solutions**:

**1. Wait for Load**
- Details may take 1-2 seconds to load
- Watch for "Loading..." message

**2. Check Entity ID**
```javascript
// In console:
console.log('Clicked entity:', entityId);
// ID should match node ID in database
```

**3. Try Different Entity**
- Some entities may have null/missing fields
- Try entity from Overview "Top Entities" list

---

## Performance Optimization

### Slow Queries

**Symptom**: Cypher queries take >5 seconds

**Solutions**:

**1. Add LIMIT**
```cypher
-- ❌ Slow (returns all)
MATCH (e:Entity)
RETURN e

-- ✅ Fast (returns 100)
MATCH (e:Entity)
RETURN e
LIMIT 100
```

**2. Filter Early**
```cypher
-- ❌ Slow (filter after match)
MATCH (e:Entity)-[r:RELATED]->(t:Entity)
WHERE e.name = 'spain'
RETURN t

-- ✅ Fast (filter during match)
MATCH (e:Entity {name: 'spain'})-[r:RELATED]->(t:Entity)
RETURN t
```

**3. Reduce Hop Count**
```cypher
-- ❌ Slow (4 hops = exponential)
MATCH path = ()-[*1..4]->()

-- ✅ Fast (2 hops)
MATCH path = ()-[*1..2]->()
```

---

### Slow Page Load

**Symptom**: Page takes long to initially load

**Solutions**:

**1. Clear Browser Cache**
```
Ctrl+Shift+Delete (or Cmd+Shift+Delete)
Clear cached images and files
```

**2. Check CDN**
- External libraries load from CDN
- Slow network affects load time
- Use faster internet connection

**3. Reduce Initial Data**
- Don't pre-load large graphs
- Load graph after page ready

---

## Getting More Help

### Enable Debug Mode

Add to `graph_rag_server.py`:
```python
import logging
logging.basicConfig(level=logging.DEBUG)
```

### Browser Console

Press `F12` to open developer tools:
- **Console**: See errors and logs
- **Network**: Check API calls
- **Application**: Check local storage

### Collect Debug Info

When reporting issues, include:
```bash
# 1. Python version
python --version

# 2. Dependency versions
pip list | grep -E "(fastapi|uvicorn|kuzu)"

# 3. OS/Platform
uname -a  # Linux/macOS
ver       # Windows

# 4. Browser & version
# Check in browser: Help > About

# 5. Graph size
jq '.nodes | length' graph.json

# 6. Error messages
# Copy from terminal or browser console
```

---

## Entity & Relationship Issues

### "Failed to load relationships" Error

**Symptom**:
- Clicking "Explore Relations" shows error
- Console shows 404 or 500 error

**Causes & Solutions**:

**Cause 1: Using old API format**
```
Solution: Ensure you're using the hash-based API
- Search results should include "composite_hash" field
- Relations endpoint uses hash, not ID: /api/relations/{composite_hash}
```

**Cause 2: Database not loaded**
```bash
Solution: Upload a graph first
1. Click "📁 Load Graph JSON"
2. Select your .json file
3. Wait for "✅ Complete" message
4. Try exploring relations again
```

**Cause 3: Entity has no relations**
```
Expected: Returns empty arrays { "outgoing": [], "incoming": [] }
If you see error instead, check server logs
```

---

### Duplicate Entities During Upload

**Symptom**:
```
⚠️ Skipping duplicate: entity_042 (degree=5, docs count=2)
⚠️ Skipped 150 duplicate entities
```

**This is NORMAL**:
- Entities are unique by combination of (id + degree + docs)
- Duplicates are automatically skipped (first occurrence kept)
- First 10 duplicates shown in console, rest silently skipped

**How to reduce duplicates**:
```bash
# Option 1: Pre-process your JSON to ensure unique combinations
# Option 2: Accept automatic deduplication (recommended)
```

**Entity Uniqueness Rules**:
```python
# These are DUPLICATES (skipped):
entity_1: id="e001", degree=5, docs=["a.pdf","b.pdf"]
entity_2: id="e001", degree=5, docs=["b.pdf","a.pdf"]  # Same (sorted)

# These are UNIQUE (both kept):
entity_1: id="e001", degree=5, docs=["a.pdf"]
entity_2: id="e001", degree=6, docs=["a.pdf"]  # Different degree
entity_3: id="e001", degree=5, docs=["b.pdf"]  # Different docs
```

---

### Old Graph Still Visible After Upload

**Symptom**:
- New graph loads but old nodes still visible
- Mixed visualization from different graphs

**Solution**:
```bash
# This is fixed in current version
# If you still see this:
1. Hard refresh: Ctrl+Shift+R (or Cmd+Shift+R on Mac)
2. Clear browser cache
3. Restart server
4. Re-upload graph
```

**Prevention**:
```javascript
// Modern versions clear 3D graph automatically
// Check console for: "🧹 Cleared 3D graph visualization"
```

---

## Still Having Issues?

1. ✅ Check `README.md` for setup instructions
2. ✅ Review `docs/USER_GUIDE.md` for feature details
3. ✅ Check `docs/API_REFERENCE.md` for API usage
4. ✅ Try the sample graph: `examples/sample_graph.json`
5. ✅ Restart everything (close browser, kill server, restart)

---

**Most issues are resolved by**: Refreshing page + Restarting server + Re-uploading graph 🔄

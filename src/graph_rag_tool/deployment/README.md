# Graph-RAG Kuzu Explorer - Deployment Bundle

**Version**: 1.0
**Last Updated**: February 2026

---

## 📦 What's Included

This is a complete, standalone deployment bundle for the Graph-RAG Kuzu Explorer application. Everything you need to run the application is included in this package.

### Bundle Contents

```
deployment/
├── README.md                    # This file
├── QUICK_START.md              # Fast deployment guide
├── template.html               # Frontend (163KB)
├── graph_rag_server.py         # Backend server
├── requirements.txt            # Python dependencies
├── start.sh                    # Linux/macOS start script
├── start.bat                   # Windows start script
├── graph_rag_db/               # Database directory (auto-created)
├── data/                       # Place your graph files here
├── examples/                   # Sample data
│   └── sample_graph.json       # Example graph (7 entities)
└── docs/                       # Additional documentation
    ├── API_REFERENCE.md
    ├── USER_GUIDE.md
    └── TROUBLESHOOTING.md
```

---

## 🚀 Quick Start (3 Steps)

### 1. Install Python Dependencies

```bash
pip install -r requirements.txt
```

**Required**: Python 3.8 or higher

### 2. Start the Server

**Linux/macOS/WSL:**
```bash
chmod +x start.sh
./start.sh
```

**Windows:**
```cmd
start.bat
```

### 3. Open Browser

Navigate to: **http://localhost:8000**

✅ **Done!** The application is ready to use.

---

## 📋 System Requirements

### Minimum Requirements
- **Python**: 3.8 or higher
- **RAM**: 2GB minimum, 4GB recommended
- **Disk Space**: 500MB (plus space for your graph data)
- **Browser**: Modern browser with WebGL support
  - Chrome 90+
  - Firefox 88+
  - Edge 90+
  - Safari 14+

### Python Dependencies
- **FastAPI**: Web framework
- **Uvicorn**: ASGI server
- **Kuzu**: Graph database
- **Python-Multipart**: File upload support

All dependencies are listed in `requirements.txt`.

---

## 🎯 Usage Workflow

### Step 1: Load Your Graph

1. Open **http://localhost:8000** in browser
2. Click **"📁 Load Graph JSON"** button in header
3. Select your graph JSON file
4. Wait for upload and visualization

**Try the Sample**: Use `examples/sample_graph.json` to test!

### Step 2: Explore with 3D Visualization

- **Rotate**: Left-click + drag
- **Zoom**: Scroll wheel
- **Pan**: Right-click + drag
- **Search**: Use search box on left panel
- **Expand**: Click "📍 Pin" then "+ 1 hop" or "+ 2 hops"

### Step 3: Database Explorer

Press **Ctrl+K** to open the Kuzu Database Explorer panel:

#### 📊 Overview Tab
- View entity count and relation types
- See top connected entities
- Browse source documents

#### 🗂️ Schema Tab
- Explore all relationship types
- View sample relationships
- Filter by type

#### 📝 Query Tab
- Write Cypher queries
- Use pre-built templates
- View query history
- Export results to JSON/CSV

#### 🔍 Explorer Tab
- Search entities by name
- View entity details
- Explore connections
- Highlight entities in 3D graph

#### 🛤️ Paths Tab
- Find shortest paths between entities
- Adjust max hops (1-5)
- Visualize path connections

### Step 4: Chat with Your Graph

Click **"💬 Graph-RAG Chat"** to:
- Ask questions about your knowledge graph
- Get AI-powered insights
- Receive responses with citations

---

## 📄 Graph JSON Format

Your graph data must follow this structure:

```json
{
  "nodes": [
    {
      "id": "unique_id",
      "label": "Category",
      "name": "entity name",
      "type": "entity",
      "degree": 5,
      "docs": ["source1.pdf", "source2.pdf"],
      "isAug": false
    }
  ],
  "links": [
    {
      "source": "source_id",
      "target": "target_id",
      "predicate": "relationship_type",
      "confidence": 0.95,
      "source_document": "document.pdf",
      "source_chunk_id": "chunk_001",
      "edge_id": "edge_001"
    }
  ]
}
```

### Required Fields

**For Nodes:**
- `id`: Entity identifier (string)
- `name`: Display name (string)
- `label`: Category/type (string)
- `degree`: Number of connections (integer)
- `docs`: Array of source documents (string[])
- `isAug`: Whether augmented node (boolean, use `false` for entities)

**⚠️ Entity Uniqueness:**
Entities are uniquely identified by the **combination** of `id`, `degree`, and `docs` (sorted).
- Same ID with different degree/docs = different entities
- Duplicate entities (same id + degree + docs) are automatically skipped during loading
- Arrays are sorted automatically for consistent matching

**For Links:**
- `source`: Source node ID (string)
- `target`: Target node ID (string)
- `predicate`: Relationship type (string)
- `confidence`: Confidence score 0-1 (float)
- `source_document`: Document name (string)
- `source_chunk_id`: Chunk identifier (string)
- `edge_id`: Unique edge ID (string)

---

## 🔑 Hash-Based API

The system uses **16-character hashes** for clean, URL-safe API calls:

### How It Works
Each entity gets a unique hash based on its composite key (id + degree + docs):
```
Entity: id="entity_001", degree=5, docs=["a.pdf","b.pdf"]
   ↓
Composite Key: "5||a.pdf,b.pdf||entity_001"
   ↓
Hash: "a1b2c3d4e5f67890" ← Use this for API calls!
```

### Benefits
- ✅ Clean URLs (no `%` encoding needed)
- ✅ Short identifiers (16 chars)
- ✅ Unique across all entity combinations
- ✅ Works with special characters in IDs/docs

### Example API Usage
```bash
# Search returns entities with hash
GET /api/entities?q=neural
→ Returns: [{ "composite_hash": "a1b2c3...", "id": "entity_003", ... }]

# Use hash for relations (clean URL!)
GET /api/relations/a1b2c3d4e5f67890
→ Returns: { "outgoing": [...], "incoming": [...] }
```

---

## 🔧 Configuration

### Port Configuration

**Default**: Server runs on `http://0.0.0.0:8000`

To change the port, edit `graph_rag_server.py`:

```python
# Line ~2700 (bottom of file)
uvicorn.run(app, host="0.0.0.0", port=8000)  # ← Change port here
```

### Database Location

**Default**: `./graph_rag_db/`

Database is created automatically in the deployment directory.

### Environment Variables

Optional configuration via environment variables:

```bash
# Set graph data path
export GRAPH_JSON_PATH="./data/my_graph.json"

# Start server
./start.sh
```

Note: database path is currently fixed in code to `./graph_rag_db`.

---

## 🌐 Network Access

### Local Access Only (Default)
- Access from same machine: `http://localhost:8000`

### LAN Access (Optional)

To allow access from other devices on your network:

1. Find your IP address:
   ```bash
   # Linux/macOS
   ip addr show
   # or
   ifconfig

   # Windows
   ipconfig
   ```

2. Access from other devices: `http://YOUR_IP:8000`

**Security Note**: Only enable LAN access on trusted networks.

---

## 🐛 Troubleshooting

### "Address already in use" Error

**Problem**: Port 8000 is occupied

**Solutions**:
```bash
# Find process using port 8000
lsof -i :8000          # Linux/macOS
netstat -ano | findstr :8000  # Windows

# Kill the process or change port in graph_rag_server.py
```

### "Module not found" Error

**Problem**: Missing Python dependencies

**Solution**:
```bash
pip install -r requirements.txt --upgrade
```

### Graph Upload Fails

**Problem**: JSON format invalid

**Solutions**:
1. Validate JSON: https://jsonlint.com/
2. Check required fields (see "Graph JSON Format" section)
3. Verify file size (large files may timeout)
4. Check browser console (F12) for errors

### 3D Visualization Not Working

**Problem**: Browser doesn't support WebGL

**Solutions**:
1. Update browser to latest version
2. Enable WebGL in browser settings
3. Try different browser (Chrome recommended)
4. Check GPU drivers are updated

### Database Initialization Fails

**Problem**: Database corruption or permission issues

**Solutions**:
```bash
# Remove database and restart
rm -rf graph_rag_db
./start.sh
```

### Server Won't Start

**Problem**: Python version or dependency issues

**Solutions**:
```bash
# Check Python version (must be 3.8+)
python --version

# Create virtual environment
python -m venv venv
source venv/bin/activate  # Linux/macOS
venv\Scripts\activate     # Windows

# Install dependencies
pip install -r requirements.txt
```

---

## 📚 Additional Documentation

### Included Docs
- **QUICK_START.md**: Fast deployment guide
- **API_REFERENCE.md**: Backend API endpoints
- **USER_GUIDE.md**: Detailed feature walkthrough
- **TROUBLESHOOTING.md**: Common issues and solutions

### Online Resources
- Kuzu Database: https://kuzudb.com/
- FastAPI: https://fastapi.tiangolo.com/
- Cypher Query Language: https://neo4j.com/docs/cypher-manual/

---

## 🔐 Security Notes

### For Production Deployment

⚠️ **This bundle is configured for local/development use**

For production deployment, consider:

1. **Authentication**: Add user authentication
2. **HTTPS**: Use SSL/TLS certificates
3. **CORS**: Configure proper CORS policies
4. **Rate Limiting**: Prevent API abuse
5. **Input Validation**: Enhanced security checks
6. **Database Backup**: Regular backups
7. **Monitoring**: Add logging and monitoring

### API Keys

If using the Graph-RAG chat feature, you'll need:
- **Gemini API Key** (for Google AI)

Set in environment or server configuration.

---

## 🔄 Updates and Maintenance

### Updating Dependencies

```bash
pip install -r requirements.txt --upgrade
```

### Database Maintenance

The database is automatically cleaned on each page load. For manual cleanup:

```bash
# Remove database
rm -rf graph_rag_db

# Restart server (database recreates automatically)
./start.sh
```

### Backup Your Data

Important files to backup:
- Your graph JSON files in `data/`
- Custom configurations
- Any modified code

**Database is recreated from JSON**, so JSON files are your source of truth.

---

## 📊 Performance Tips

### Large Graphs (10,000+ entities)

1. **Increase Memory**: Allocate more RAM to Python
2. **Batch Queries**: Use LIMIT in Cypher queries
3. **Indexed Search**: Database automatically indexes entities
4. **Browser Performance**: Close other tabs, use hardware acceleration

### Optimal Graph Sizes
- **Small**: < 1,000 entities - Instant
- **Medium**: 1,000 - 10,000 entities - Fast
- **Large**: 10,000 - 100,000 entities - Good (may need tuning)
- **Very Large**: 100,000+ entities - May require optimization

---

## 🎨 Customization

### Branding

Edit `template.html` to customize:
- **Header Title**: Search for `<h1>` tag
- **Colors**: Modify CSS variables in `<style>` section
- **Logo**: Add image in header

### Features

Enable/disable features by commenting out sections in:
- **template.html**: Frontend features
- **graph_rag_server.py**: Backend endpoints

---

## 🤝 Support

### Common Tasks

**Load Different Graph**:
1. Click "📁 Load Graph JSON"
2. Select new file
3. Database automatically resets

**Clear Cache**:
- Hard refresh: `Ctrl+Shift+R` (or `Cmd+Shift+R` on Mac)

**Reset Everything**:
1. Close browser
2. Stop server (Ctrl+C)
3. Delete `graph_rag_db/` folder
4. Restart server

---

## 📜 License & Credits

### Technology Stack
- **Frontend**: HTML5, CSS3, JavaScript (ES6+)
- **3D Visualization**: ForceGraph3D (via CDN)
- **Backend**: Python 3.8+, FastAPI, Uvicorn
- **Database**: Kuzu (embedded graph database)

### External Libraries (via CDN)
- ForceGraph3D: https://unpkg.com/3d-force-graph
- Three.js: https://unpkg.com/three
- D3.js: https://unpkg.com/d3

---

## ✅ Deployment Checklist

Before deploying to production:

- [ ] Test with sample data (`examples/sample_graph.json`)
- [ ] Test with your actual graph data
- [ ] Configure port if needed
- [ ] Set up firewall rules (if exposing to network)
- [ ] Configure HTTPS (if public)
- [ ] Add authentication (if needed)
- [ ] Set up monitoring/logging
- [ ] Create backup strategy
- [ ] Document your specific setup
- [ ] Train users on interface

---

## 📞 Getting Started

1. **Install**: `pip install -r requirements.txt`
2. **Start**: `./start.sh` (or `start.bat` on Windows)
3. **Open**: http://localhost:8000
4. **Load**: Click "📁 Load Graph JSON", select `examples/sample_graph.json`
5. **Explore**: Press `Ctrl+K` for Database Explorer

**That's it!** You're ready to explore your knowledge graphs! 🎉

---

**Questions?** Check the docs/ folder for detailed guides.

**Happy Exploring!** 🚀

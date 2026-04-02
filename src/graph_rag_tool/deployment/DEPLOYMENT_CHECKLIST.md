# Deployment Checklist

Use this checklist to ensure smooth deployment of Graph-RAG Kuzu Explorer.

---

## Pre-Deployment

### System Requirements

- [ ] Python 3.8 or higher installed
- [ ] `pip` package manager available
- [ ] Modern web browser installed (Chrome/Firefox/Edge/Safari)
- [ ] Sufficient disk space (500MB + graph data size)
- [ ] Network access (for CDN dependencies)

### File Verification

- [ ] `template.html` present
- [ ] `graph_rag_server.py` present
- [ ] `requirements.txt` present
- [ ] `start.sh` present (Linux/macOS/WSL)
- [ ] `start.bat` present (Windows)
- [ ] `examples/sample_graph.json` present
- [ ] `docs/` folder with all documentation
- [ ] `README.md` present

---

## Installation

### Python Environment

- [ ] Python version verified: `python --version`
- [ ] Virtual environment created (recommended):
  ```bash
  python -m venv venv
  source venv/bin/activate  # Linux/macOS
  venv\Scripts\activate     # Windows
  ```
- [ ] Dependencies installed: `pip install -r requirements.txt`
- [ ] No installation errors reported

### Dependency Verification

Check installed versions:
```bash
pip list | grep -E "(fastapi|uvicorn|kuzu|multipart)"
```

Expected:
- [ ] FastAPI 0.100+ installed
- [ ] Uvicorn 0.20+ installed
- [ ] Kuzu 0.1.0+ installed
- [ ] python-multipart installed

---

## Configuration (Optional)

### Port Configuration

- [ ] Default port 8000 is available
- [ ] OR port changed in `graph_rag_server.py` (near the uvicorn.run(...) call at the end of the file)
- [ ] OR port changed in `template.html` BACKEND_URL (search for the API base URL in template.html)
- [ ] Both files updated if port changed

### Network Configuration

For local-only access (default):
- [ ] No changes needed

For LAN access:
- [ ] Firewall configured to allow port
- [ ] IP address documented for users
- [ ] Security implications understood

For production:
- [ ] HTTPS configured
- [ ] Authentication added
- [ ] CORS properly configured
- [ ] Rate limiting implemented
- [ ] Input validation enhanced

---

## Testing

### Local Testing

- [ ] Server starts without errors: `./start.sh`
- [ ] Terminal shows: "INFO: Uvicorn running on http://0.0.0.0:8000"
- [ ] Browser opens: http://localhost:8000
- [ ] Page loads completely
- [ ] No console errors (F12 → Console)

### Sample Graph Test

- [ ] Click "📁 Load Graph JSON"
- [ ] Select `examples/sample_graph.json`
- [ ] Upload completes successfully
- [ ] See "✅ Graph Loaded Successfully" message
- [ ] 3D graph shows 7 nodes
- [ ] Can rotate/zoom/pan graph

### Database Explorer Test

- [ ] Press Ctrl+K to open DB Explorer
- [ ] Overview tab shows: "7 entities"
- [ ] Schema tab shows relation types
- [ ] Query tab executes template query
- [ ] Explorer tab searches for "machine"
- [ ] Paths tab finds path: "artificial intelligence" → "computer vision"

### Feature Tests

- [ ] Search box finds entities
- [ ] "+ 1 hop" expands neighbors
- [ ] "Reset" returns to full graph
- [ ] Export JSON downloads file
- [ ] Export CSV downloads file
- [ ] Keyboard shortcuts work (Ctrl+K, Esc)

---

## Your Data

### Graph Preparation

- [ ] Graph JSON validated: https://jsonlint.com/
- [ ] Required fields present:
  - [ ] `nodes` array with `id`, `name`, `label`, `degree`, `docs`, `isAug`
  - [ ] `links` array with `source`, `target`, `predicate`, `confidence`, etc.
- [ ] File encoding is UTF-8
- [ ] File size noted (for upload timeout planning)

### First Upload

- [ ] Graph JSON file accessible
- [ ] Click "📁 Load Graph JSON"
- [ ] Select your graph file
- [ ] Wait for complete upload
- [ ] Verify entity count matches expected
- [ ] Verify visualization looks correct

### Data Validation

- [ ] Open DB Explorer → Overview
- [ ] Check entity count
- [ ] Check relation type count
- [ ] Review top entities (make sense?)
- [ ] Check source documents list

---

## User Training

### Documentation Distribution

- [ ] `README.md` shared with users
- [ ] `QUICK_START.md` shared with users
- [ ] `docs/USER_GUIDE.md` shared with users
- [ ] `docs/TROUBLESHOOTING.md` bookmarked for support

### Key Features Demonstrated

- [ ] How to load a graph
- [ ] How to navigate 3D visualization
- [ ] How to open DB Explorer (Ctrl+K)
- [ ] How to search entities
- [ ] How to execute queries
- [ ] How to find paths
- [ ] How to export results

### Common Issues Prepared

- [ ] Port already in use → how to fix
- [ ] Graph won't load → validate JSON
- [ ] Slow performance → reduce max nodes
- [ ] WebGL not supported → update browser

---

## Production Deployment (Optional)

### Security

- [ ] Authentication implemented
- [ ] HTTPS/SSL configured
- [ ] CORS restricted to specific origins
- [ ] Rate limiting enabled
- [ ] Input sanitization enhanced
- [ ] API keys secured (for Graph-RAG chat)

### Infrastructure

- [ ] Server deployed to production machine
- [ ] Firewall rules configured
- [ ] Reverse proxy configured (if using)
- [ ] SSL certificates installed
- [ ] Domain name configured
- [ ] Health monitoring set up
- [ ] Log aggregation configured

### Backup & Recovery

- [ ] Backup strategy for graph JSON files
- [ ] Database backup procedure documented
- [ ] Recovery procedure tested
- [ ] Disaster recovery plan created

### Performance

- [ ] Load testing completed
- [ ] Performance acceptable for expected load
- [ ] Resource limits configured
- [ ] Scaling plan in place (if needed)

---

## Post-Deployment

### Monitoring

- [ ] Server health check automated
- [ ] Error logging configured
- [ ] Usage analytics (if desired)
- [ ] Alert system for failures

### Maintenance Plan

- [ ] Update schedule defined
- [ ] Dependency update procedure
- [ ] Backup schedule automated
- [ ] Support contact established

### Documentation

- [ ] Deployment notes documented
- [ ] Custom configurations documented
- [ ] Network topology documented
- [ ] Access credentials secured

---

## Final Checklist

### Everything Works

- [ ] Server starts reliably
- [ ] Graphs load successfully
- [ ] All features functional
- [ ] Performance acceptable
- [ ] Documentation complete
- [ ] Users trained
- [ ] Support process defined

### Ready to Go

- [ ] Test deployment complete
- [ ] Production deployment complete (if applicable)
- [ ] Monitoring active
- [ ] Backups configured
- [ ] Team informed
- [ ] Launch! 🚀

---

## Rollback Plan (If Issues)

If critical issues occur:

1. **Stop Server**:
   ```bash
   pkill -f graph_rag_server
   # or Ctrl+C in terminal
   ```

2. **Investigate**:
   - Check logs
   - Review error messages
   - Consult troubleshooting guide

3. **Fix or Revert**:
   - Fix configuration
   - Restore from backup
   - Roll back to previous version

4. **Test**:
   - Test with sample graph
   - Verify all features
   - Confirm fix

5. **Redeploy**:
   - Restart server
   - Verify functionality
   - Inform users

---

## Notes

**Date Deployed**: _______________

**Deployed By**: _______________

**Server Location**: _______________

**Access URL**: _______________

**Special Configuration**: _______________

**Issues Encountered**: _______________

**Lessons Learned**: _______________

---

## Support Contacts

**Technical Issues**: _______________

**Graph Data Questions**: _______________

**Server Administration**: _______________

---

**Deployment Complete!** ✅

Keep this checklist for reference and future deployments.

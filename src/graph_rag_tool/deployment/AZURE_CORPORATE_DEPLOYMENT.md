# Graph-RAG Tool - Azure Corporate Deployment Guide

**For corporate environments behind firewall with Azure OpenAI access only**

---

## Overview

This guide is specifically for deploying the Graph-RAG Tool in corporate environments where:
- ✅ Servers are behind a corporate firewall
- ✅ Access to Azure OpenAI endpoints is available
- ❌ No access to HuggingFace or external model downloads
- ❌ No outbound internet access except Azure

**Key Difference from Standard Deployment:**
- **NO** local embedding models (no downloads from HuggingFace)
- **Uses** Azure OpenAI `text-embedding-3-small` for embeddings
- **Fully** firewall-compatible

---

## Prerequisites

### 1. Azure OpenAI Resources

You must have access to an Azure OpenAI deployment with:

**Required Models:**
- ✅ **Chat Model**: `gpt-4o`, `gpt-4`, or `gpt-35-turbo`
- ✅ **Embedding Model**: `text-embedding-3-small`

**Required Information:**
- Azure OpenAI API Key
- Azure OpenAI Endpoint URL (e.g., `https://your-resource.openai.azure.com`)
- Deployment names for chat and embedding models
- API Version (e.g., `2024-05-01-preview`)

### 2. System Requirements

- **Python**: 3.11 or 3.12 (3.14 not recommended - compilation issues)
- **RAM**: 2 GB minimum, 4 GB recommended
- **Disk**: 500 MB + graph data size
- **OS**: Windows 10/11, Windows Server 2016+, or Linux

---

## Installation Steps

### Step 1: Install Dependencies (Azure-Only)

**Windows (Anaconda Prompt - RECOMMENDED):**
```cmd
cd path\to\deployment\folder
pip install -r requirements-azure.txt
```

**What gets installed:**
```
fastapi==0.115.0           # Web framework
uvicorn==0.32.0            # Server
kuzu==0.7.1                # Graph database
openai==1.54.0             # Azure OpenAI client
pydantic==2.9.0            # Data validation
numpy==1.24.3              # Vector operations
python-multipart==0.0.12   # File uploads
```

**IMPORTANT:** NO model downloads occur - all packages install from PyPI only.

---

### Step 2: Configure Azure Mode

Set the embedding mode environment variable:

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

**Verify:**
```cmd
echo %EMBEDDING_MODE%   # Windows CMD
echo $env:EMBEDDING_MODE   # PowerShell
echo $EMBEDDING_MODE    # Linux/Mac
```

Should output: `azure`

---

### Step 3: Start Server

**Windows (Anaconda Prompt):**
```cmd
python graph_rag_server.py
```

**Linux/Mac:**
```bash
python graph_rag_server.py
```

**Expected Output:**
```
Graph-RAG Kuzu Explorer Server
==============================
→ Embedding mode: azure
→ sentence-transformers available: False
⚠️ sentence-transformers not available
  This is EXPECTED in Azure-only mode

✓ Database connection ready
✓ Server initialization complete

Uvicorn running on http://0.0.0.0:8000 (Press CTRL+C to quit)
```

**Critical Check:**
- ✅ `Embedding mode: azure` should be shown
- ✅ `sentence-transformers not available` is EXPECTED and correct
- ✅ Server starts on port 8000

---

### Step 4: Open Browser

Navigate to: **http://localhost:8000**

**⚠️ CRITICAL:** Use `http://localhost:8000` (NOT `file://` protocol)

---

### Step 5: Configure Azure OpenAI

**In the web interface:**

1. Click the **⚙️ Settings** button (top right)

2. **LLM Configuration:**
   - Provider: Select **"Azure OpenAI"**
   - API Key: Enter your Azure OpenAI key
   - Endpoint: `https://your-resource.openai.azure.com`
   - Deployment: Your chat model deployment name (e.g., `gpt-4o`)
   - API Version: `2024-05-01-preview` (or your version)

3. **Embedding Configuration:**
   - Mode: Should show **"Azure"** (from environment variable)
   - Deployment: Your embedding model deployment name (e.g., `text-embedding-3-small`)

4. Click **Save**

---

### Step 6: Load Graph Data

1. Click **"📁 Load Graph JSON"** button
2. Select your graph data file (JSON format)
3. Wait for upload to complete
4. Database auto-initializes

---

### Step 7: Generate Embeddings

**IMPORTANT:** In Azure mode, embeddings are NOT generated automatically.

**Method 1: Via UI (After loading graph):**
1. Open Settings panel
2. Ensure Azure credentials are configured
3. Click **"Generate Embeddings"** button
4. Wait for completion (progress shown in UI)

**Method 2: Via API:**
```bash
curl -X POST http://localhost:8000/api/generate-embeddings \
  -H "Content-Type: application/json" \
  -d '{
    "azure_key": "your-key",
    "azure_endpoint": "https://your-resource.openai.azure.com",
    "azure_deployment": "text-embedding-3-small",
    "azure_version": "2024-05-01-preview"
  }'
```

**Expected Time:**
- ~5-15 minutes for 1,000 entities
- Rate limited by Azure OpenAI quotas

**Verification:**
```bash
# Check embedding status
curl http://localhost:8000/api/schema
# Should show entity_count > 0
```

---

## Verification Checklist

After completing setup, verify:

- [ ] Server starts without errors
- [ ] Embedding mode shows "azure"
- [ ] NO warnings about downloading HuggingFace models
- [ ] Browser loads UI at http://localhost:8000
- [ ] Settings panel accepts Azure credentials
- [ ] Graph loads successfully
- [ ] Embeddings generate successfully
- [ ] Queries return results

---

## Troubleshooting

### Server won't start

**Check Python version:**
```cmd
python --version
```
Should be 3.11 or 3.12

**Reinstall dependencies:**
```cmd
pip uninstall -r requirements-azure.txt -y
pip install -r requirements-azure.txt
```

### Embedding mode is "local" instead of "azure"

**Cause:** Environment variable not set

**Fix:**
```cmd
# Windows CMD
set EMBEDDING_MODE=azure

# Restart server
python graph_rag_server.py
```

**Permanent Fix (Windows):**
```cmd
setx EMBEDDING_MODE azure
```
Then restart terminal and server.

### "Could not download model" errors

**This should NOT happen in Azure mode.**

If you see:
```
Downloading model from HuggingFace...
```

**Problem:** Server is in "local" mode, not "azure" mode

**Fix:**
1. Stop server (Ctrl+C)
2. Set environment variable: `set EMBEDDING_MODE=azure`
3. Restart server
4. Verify output shows: `Embedding mode: azure`

### Azure API errors

**"Unauthorized" or "Invalid API key":**
- Verify API key is correct
- Check endpoint URL format: `https://your-resource.openai.azure.com`
- Ensure no trailing slashes

**"Deployment not found":**
- Verify deployment name matches your Azure OpenAI resource
- Check deployment is active in Azure portal

**"Rate limit exceeded":**
- Azure OpenAI has request quotas
- Wait and retry
- Request quota increase from Azure

### Embeddings generation is slow

**Expected:** ~5-15 minutes per 1,000 entities

**Cause:** Azure OpenAI API rate limits

**Not a problem** - just wait for completion.

**To speed up (requires Azure portal access):**
1. Request higher TPM (Tokens Per Minute) quota
2. Use batch processing (future feature)

---

## Cost Estimation

### Azure OpenAI Costs

**Embedding Generation:**
- Model: `text-embedding-3-small`
- Cost: ~$0.0001 per 1,000 tokens
- 1,000 entities ≈ 50,000 tokens ≈ $0.005 (half a cent)
- 10,000 entities ≈ 500,000 tokens ≈ $0.05 (5 cents)

**Chat Queries:**
- Model: `gpt-4o` or `gpt-4`
- Cost: ~$0.005-0.03 per 1,000 tokens (input) + $0.015-0.06 per 1,000 tokens (output)
- Per query: ~$0.001-0.01 depending on graph size

**Monthly estimate for moderate use:**
- Initial embedding: $0.05 (one-time)
- 100 queries/day: ~$3-10/month

---

## Production Deployment

### Persistent Environment Variable

**Windows (System-wide):**
```cmd
setx EMBEDDING_MODE azure
```

**Linux (add to ~/.bashrc):**
```bash
echo "export EMBEDDING_MODE=azure" >> ~/.bashrc
source ~/.bashrc
```

### Firewall Configuration

**Required Outbound Access:**
- ✅ `*.openai.azure.com:443` (Azure OpenAI endpoints)
- ✅ `pypi.org:443` (for pip install - one-time)
- ❌ NO access to HuggingFace required
- ❌ NO access to other external sites required

### Database Backup

**Important:** Kuzu database is NOT compatible between Windows and WSL/Linux

**Backup strategy:**
```cmd
# Backup database folder
xcopy /E /I graph_rag_db graph_rag_db_backup

# Backup graph JSON files
xcopy /Y data\*.json data_backup\
```

**Restore:**
```cmd
# Stop server first!
rmdir /S /Q graph_rag_db
xcopy /E /I graph_rag_db_backup graph_rag_db
```

---

## Security Considerations

### API Key Storage

**Never:**
- ❌ Commit API keys to version control
- ❌ Share API keys in screenshots
- ❌ Include keys in documentation

**Best practices:**
- ✅ Use environment variables
- ✅ Store in Azure Key Vault (enterprise)
- ✅ Rotate keys periodically
- ✅ Use separate keys for dev/prod

### Network Security

**Recommended:**
- Run on internal network only
- Use VPN for remote access
- Configure firewall to block external access to port 8000
- Use HTTPS in production (configure reverse proxy)

---

## Comparison: Azure vs Local Mode

| Feature | Azure Mode | Local Mode |
|---------|-----------|------------|
| **Model Download** | ❌ None | ✅ ~90MB from HuggingFace |
| **Firewall Friendly** | ✅ Yes | ❌ No (requires HuggingFace) |
| **Embedding Dimension** | 1536 | 384 |
| **Embedding Quality** | Higher | Good |
| **Cost** | ~$0.0001/1K tokens | Free |
| **Speed** | API-limited | Fast |
| **Internet Required** | Azure only | HuggingFace (first run) |
| **Corporate Use** | ✅ Recommended | ❌ Not firewall-safe |

---

## Quick Reference

### Start Server (Azure Mode)
```cmd
set EMBEDDING_MODE=azure
python graph_rag_server.py
```

### Generate Embeddings (API)
```bash
curl -X POST http://localhost:8000/api/generate-embeddings \
  -H "Content-Type: application/json" \
  -d '{"azure_key":"KEY","azure_endpoint":"URL","azure_deployment":"NAME","azure_version":"VER"}'
```

### Check Status
```bash
curl http://localhost:8000/api/health
curl http://localhost:8000/api/schema
```

### Stop Server
```
Ctrl+C in terminal
```

---

## Support

### Documentation
- **General Guide**: README.md
- **User Manual**: docs/USER_GUIDE.md
- **Troubleshooting**: docs/TROUBLESHOOTING.md
- **API Reference**: docs/API_REFERENCE.md

### Common Issues
All covered in docs/TROUBLESHOOTING.md

---

## Summary

**Azure Corporate Deployment Checklist:**

1. ✅ Install dependencies from `requirements-azure.txt`
2. ✅ Set `EMBEDDING_MODE=azure`
3. ✅ Start server
4. ✅ Configure Azure credentials in UI
5. ✅ Load graph data
6. ✅ Generate embeddings via Azure
7. ✅ Verify queries work

**Result:** Fully functional Graph-RAG system running entirely on Azure OpenAI, no external downloads required.

---

**Deployment Date:** February 2026
**Version:** 1.0.0
**Mode:** Azure OpenAI Only (Corporate Firewall Compatible)

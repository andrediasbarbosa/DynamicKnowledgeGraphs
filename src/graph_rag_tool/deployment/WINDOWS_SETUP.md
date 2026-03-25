# Windows Setup Guide

Complete guide for deploying Graph-RAG Kuzu Explorer on **native Windows** (no WSL required).

---

## ✅ Windows Compatibility

**Fully supported on**:
- Windows 10 (64-bit)
- Windows 11 (64-bit)
- Windows Server 2016+

**No WSL needed!** This is a native Windows application.

---

## Prerequisites

### 1. Install Python

**Download**:
- Go to https://www.python.org/downloads/
- Download Python 3.8 or higher (3.11 recommended)
- Run installer

**⚠️ IMPORTANT**: During installation:
- ✅ Check "Add Python to PATH"
- ✅ Check "Install pip"
- Click "Install Now"

**Verify Installation**:
```cmd
python --version
# Should show: Python 3.11.x or similar

pip --version
# Should show: pip 23.x.x or similar
```

### 2. Visual C++ Redistributables (if needed)

Kuzu requires C++ runtime libraries:

**Usually already installed**, but if you get errors:

- Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
- Run installer
- Restart computer

---

## Installation (3 Steps)

### Step 1: Extract Bundle

1. Locate the ZIP file: `graph-rag-kuzu-explorer-v1.0.zip`
2. Right-click → "Extract All..."
3. Choose destination (e.g., `C:\graph-rag\`)
4. Click "Extract"

**Result**: Folder at `C:\graph-rag\deployment\`

### Step 2: Install Dependencies

**Method A: Command Prompt**

1. Press `Win + R`
2. Type `cmd` and press Enter
3. Navigate to deployment folder:
   ```cmd
   cd C:\graph-rag\deployment
   ```
4. Install dependencies:
   ```cmd
   pip install -r requirements.txt
   ```

**Method B: File Explorer**

1. Open `C:\graph-rag\deployment\` in File Explorer
2. Click in address bar
3. Type `cmd` and press Enter
4. Run:
   ```cmd
   pip install -r requirements.txt
   ```

**Expected Output**:
```
Collecting fastapi
  Downloading fastapi-0.xxx...
Collecting uvicorn
  Downloading uvicorn-0.xxx...
Collecting kuzu
  Downloading kuzu-0.xxx...
Collecting python-multipart
  Downloading python_multipart-0.xxx...
Installing collected packages: ...
Successfully installed fastapi-0.xxx uvicorn-0.xxx kuzu-0.xxx python-multipart-0.xxx
```

**⏱️ Takes**: 1-2 minutes

### Step 3: Start Server

**Method A: Double-Click (Easiest)**

1. In File Explorer, navigate to `C:\graph-rag\deployment\`
2. Double-click `start.bat`
3. Server starts in Command Prompt window

**Method B: Command Prompt**

```cmd
cd C:\graph-rag\deployment
start.bat
```

**Expected Output**:
```
════════════════════════════════════════════
  Starting Graph RAG Server
════════════════════════════════════════════

Starting server...

Access the application at:
  • http://localhost:8000
  • http://192.168.1.xxx:8000 (from network)

Press Ctrl+C to stop
════════════════════════════════════════════

INFO:     Started server process [12345]
INFO:     Waiting for application startup.
INFO:     Application startup complete.
INFO:     Uvicorn running on http://0.0.0.0:8000 (Press CTRL+C to quit)
```

**✅ Server is running!**

---

## Using the Application

### Open Browser

1. Open **Chrome**, **Edge**, or **Firefox**
2. Navigate to: **http://localhost:8000**
3. Application loads

### Load Sample Graph

1. Click **"📁 Load Graph JSON"** button (top-right)
2. Navigate to: `C:\graph-rag\deployment\examples\`
3. Select `sample_graph.json`
4. Click "Open"
5. Wait 2-3 seconds
6. See "✅ Graph Loaded Successfully" message

### Explore!

- **3D Graph**: Rotate (left-click drag), Zoom (scroll), Pan (right-click drag)
- **Database Explorer**: Press **Ctrl+K** to open
- **Try all 5 tabs**: Overview, Schema, Query, Explorer, Paths

---

## Windows-Specific Tips

### Startup Options

**Run on Startup** (optional):

1. Press `Win + R`
2. Type `shell:startup` and press Enter
3. Create shortcut to `start.bat`
4. Place shortcut in Startup folder

**Run in Background**:

Use `start_background.bat` (create this file):
```batch
@echo off
start /B pythonw graph_rag_server.py
```

### Firewall Configuration

**First Time**:
- Windows Defender Firewall popup appears
- **✅ Allow access** to "Private networks"

**Manual Configuration**:

1. Open Windows Firewall
2. Click "Allow an app through firewall"
3. Click "Change settings"
4. Find "Python" or add new
5. ✅ Check "Private"
6. Click "OK"

### Port Configuration

Default port: **8000**

**To change**:

1. Edit `graph_rag_server.py` (near the uvicorn.run(...) call at the end of the file):
   ```python
   uvicorn.run(app, host="0.0.0.0", port=8001)  # Changed
   ```

2. Edit `template.html` (search for the API base URL in template.html):
   ```javascript
   const BACKEND_URL = 'http://localhost:8001';  // Changed
   ```

### Network Access

**Access from other Windows PCs** on same network:

1. Find your IP address:
   ```cmd
   ipconfig
   ```
   Look for "IPv4 Address": e.g., `192.168.1.100`

2. On other PC, open browser:
   ```
   http://192.168.1.100:8000
   ```

3. **Configure firewall** to allow incoming connections

---

## Troubleshooting

### "Python not found"

**Problem**: Command Prompt can't find Python

**Solutions**:

1. **Reinstall Python**:
   - Uninstall current Python
   - Download fresh from python.org
   - ✅ Check "Add Python to PATH"
   - Install

2. **Use Python Launcher**:
   ```cmd
   py --version
   py graph_rag_server.py
   ```

3. **Add to PATH manually**:
   - Search "Environment Variables" in Start Menu
   - Click "Environment Variables"
   - Under "System variables", find "Path"
   - Click "Edit"
   - Click "New"
   - Add: `C:\Users\YourName\AppData\Local\Programs\Python\Python311\`
   - Click "OK"
   - Restart Command Prompt

### "pip not found"

**Problem**: pip command not recognized

**Solution**:
```cmd
python -m pip install -r requirements.txt
```

### "Permission denied"

**Problem**: Can't install packages

**Solutions**:

1. **Run as Administrator**:
   - Right-click Command Prompt
   - Click "Run as administrator"
   - Retry installation

2. **Use --user flag**:
   ```cmd
   pip install -r requirements.txt --user
   ```

### "Microsoft Visual C++ required"

**Problem**: Kuzu installation fails with C++ error

**Solution**:

1. Download Visual C++ Redistributables:
   - https://aka.ms/vs/17/release/vc_redist.x64.exe

2. Run installer

3. Restart computer

4. Retry:
   ```cmd
   pip install -r requirements.txt
   ```

### "Port 8000 already in use"

**Problem**: Another application using port 8000

**Solutions**:

1. **Find process**:
   ```cmd
   netstat -ano | findstr :8000
   ```
   Note the PID (last column)

2. **Kill process**:
   ```cmd
   taskkill /PID 12345 /F
   ```
   (Replace 12345 with actual PID)

3. **Or change port** (see "Port Configuration" above)

### "Can't access http://localhost:8000"

**Problem**: Browser can't connect

**Solutions**:

1. **Check server is running**:
   - Look for "Uvicorn running" message
   - Don't close Command Prompt window

2. **Try different URL**:
   - http://127.0.0.1:8000
   - http://[::1]:8000

3. **Check browser**:
   - Try different browser (Chrome, Edge, Firefox)
   - Disable browser extensions
   - Clear cache (Ctrl+Shift+Delete)

4. **Check firewall**:
   - Temporarily disable Windows Firewall to test
   - If works, add exception for Python/port 8000

### Graph Won't Load

**Problem**: Upload fails or times out

**Solutions**:

1. **Validate JSON**:
   - Open https://jsonlint.com/
   - Paste your JSON
   - Fix any errors

2. **Check file size**:
   - Large files (>50MB) may timeout
   - Try smaller graph first

3. **Check file path**:
   - Use Windows file picker (don't type path)
   - Avoid paths with special characters

4. **Try sample graph**:
   - Load `examples\sample_graph.json`
   - If works, issue is with your JSON

### Server Crashes

**Problem**: Command Prompt window closes immediately

**Solutions**:

1. **Run from Command Prompt** (not double-click):
   ```cmd
   cd C:\graph-rag\deployment
   python graph_rag_server.py
   ```
   See actual error message

2. **Check Python version**:
   ```cmd
   python --version
   ```
   Must be 3.8 or higher

3. **Check dependencies**:
   ```cmd
   pip list | findstr "fastapi uvicorn kuzu"
   ```
   All should be installed

4. **Reinstall dependencies**:
   ```cmd
   pip uninstall -y fastapi uvicorn kuzu python-multipart
   pip install -r requirements.txt
   ```

---

## Performance on Windows

### Recommendations

**For Best Performance**:

1. **Close unnecessary applications**
2. **Use SSD** (not HDD) for database
3. **Allocate sufficient RAM**:
   - Small graphs (<1K entities): 2GB RAM
   - Medium graphs (1K-10K): 4GB RAM
   - Large graphs (10K+): 8GB+ RAM

4. **Use modern browser**:
   - Chrome (recommended)
   - Edge (Chromium-based)
   - Firefox

5. **Enable hardware acceleration** in browser:
   - Chrome: Settings → Advanced → System → "Use hardware acceleration"

### Optimize for Large Graphs

**In browser** (left panel):
- Lower "Max nodes" slider (1000-1500)
- Enable "Auto-freeze physics"
- Disable "Directional arrows"

**In queries** (Database Explorer):
- Always use `LIMIT` clause
- Filter early with `WHERE`
- Reduce max hops in path finding

---

## Antivirus Considerations

Some antivirus software may flag Python scripts.

**Safe to Allow**:
- `python.exe`
- `pythonw.exe`
- `graph_rag_server.py`

**If blocked**:
1. Add exception for deployment folder
2. Add exception for Python installation folder
3. Temporarily disable to test

**Recommended**: Windows Defender is sufficient (built-in)

---

## Uninstallation

**To remove**:

1. **Stop server** (Ctrl+C in Command Prompt)

2. **Delete folder**:
   - Delete `C:\graph-rag\deployment\` folder
   - Empty Recycle Bin

3. **Uninstall Python packages** (optional):
   ```cmd
   pip uninstall -y fastapi uvicorn kuzu python-multipart
   ```

4. **Uninstall Python** (optional):
   - Settings → Apps → Python → Uninstall

---

## Advanced: Run as Windows Service

**For production deployment**, run as Windows Service:

### Using NSSM (Non-Sucking Service Manager)

1. **Download NSSM**:
   - https://nssm.cc/download

2. **Install service**:
   ```cmd
   nssm install GraphRAG "C:\Python311\python.exe" "C:\graph-rag\deployment\graph_rag_server.py"
   nssm set GraphRAG AppDirectory "C:\graph-rag\deployment"
   nssm set GraphRAG Start SERVICE_AUTO_START
   ```

3. **Start service**:
   ```cmd
   nssm start GraphRAG
   ```

4. **Service runs automatically** on Windows startup

---

## Quick Reference

### Start Server
```cmd
cd C:\graph-rag\deployment
start.bat
```

### Stop Server
Press `Ctrl+C` in Command Prompt window

### Check Status
```cmd
netstat -ano | findstr :8000
```

### Restart Server
1. Stop server (Ctrl+C)
2. Wait 2 seconds
3. Run `start.bat` again

### View Logs
Check Command Prompt window for live logs

### Update Dependencies
```cmd
pip install -r requirements.txt --upgrade
```

---

## Summary

✅ **Works on native Windows** (no WSL)
✅ **3-step setup**: Install Python → Install dependencies → Run
✅ **Easy startup**: Double-click `start.bat`
✅ **Full features**: Everything works on Windows
✅ **Production ready**: Can run as Windows Service

---

## Getting Help

**Documentation**:
- Main guide: `README.md`
- Quick start: `QUICK_START.md`
- Troubleshooting: `docs\TROUBLESHOOTING.md`

**Common Issues**: See troubleshooting section above

**Test Environment**: Try `examples\sample_graph.json` first

---

**Windows deployment is fully supported!** 🪟✅

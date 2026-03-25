# How to Deploy - Graph-RAG Kuzu Explorer

Step-by-step guide for deploying this bundle to a new location.

---

## Overview

This deployment bundle is **100% portable** and can be deployed to any machine with Python 3.8+.

**No build process, no compilation, no complex setup!**

---

## Deployment Methods

Choose the method that works best for you:

### Method 1: Direct Copy (Same Machine)
1. Copy entire `deployment/` folder to destination
2. Install dependencies
3. Run

### Method 2: ZIP Transfer (Different Machine)
1. Create ZIP of `deployment/` folder
2. Transfer ZIP to destination
3. Extract, install dependencies, run

### Method 3: Git Repository
1. Commit `deployment/` folder to Git
2. Clone on destination machine
3. Install dependencies, run

---

## Method 1: Direct Copy

**Use when**: Deploying on the same machine in a different location

### Steps

1. **Copy folder**:
   ```bash
   cp -r deployment/ /path/to/destination/
   cd /path/to/destination/deployment/
   ```

2. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

3. **Start server**:
   ```bash
   ./start.sh    # Linux/macOS/WSL
   start.bat     # Windows
   ```

4. **Open browser**: http://localhost:8000

✅ **Done!**

---

## Method 2: ZIP Transfer (Recommended)

**Use when**: Moving to different machine or sharing with others

### Creating the ZIP

**Option A: Command Line**

```bash
# From parent directory (src/graph_rag_tool/)
zip -r graph-rag-kuzu-explorer-v1.0.zip deployment/ \
    -x "deployment/graph_rag_db/*" \
    -x "deployment/__pycache__/*" \
    -x "deployment/*.pyc" \
    -x "deployment/.DS_Store"
```

**Option B: GUI**

**Windows**:
1. Right-click `deployment/` folder
2. Send to → Compressed (zipped) folder
3. Rename to: `graph-rag-kuzu-explorer-v1.0.zip`

**macOS**:
1. Right-click `deployment/` folder
2. Compress "deployment"
3. Rename to: `graph-rag-kuzu-explorer-v1.0.zip`

**Linux**:
1. Right-click `deployment/` folder
2. Compress... → .zip
3. Rename to: `graph-rag-kuzu-explorer-v1.0.zip`

### Deploying from ZIP

1. **Transfer ZIP** to destination machine (USB, email, cloud, etc.)

2. **Extract ZIP**:
   ```bash
   # Command line
   unzip graph-rag-kuzu-explorer-v1.0.zip
   cd deployment/

   # Or use GUI: Right-click → Extract All
   ```

3. **Verify extraction**:
   ```bash
   ls -la
   # Should see:
   # template.html
   # graph_rag_server.py
   # requirements.txt
   # start.sh / start.bat
   # README.md
   # docs/
   # examples/
   ```

4. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

5. **Start server**:
   ```bash
   ./start.sh    # Linux/macOS/WSL
   start.bat     # Windows
   ```

6. **Open browser**: http://localhost:8000

✅ **Deployed!**

---

## Method 3: Git Repository

**Use when**: Version control desired or deploying to multiple machines

### Setting Up Repository

1. **Initialize Git** (if not already):
   ```bash
   cd deployment/
   git init
   ```

2. **Add files**:
   ```bash
   git add .
   git commit -m "Initial deployment bundle v1.0"
   ```

3. **Push to remote** (GitHub, GitLab, Bitbucket, etc.):
   ```bash
   git remote add origin https://github.com/your-repo/graph-rag-kuzu.git
   git push -u origin main
   ```

### Deploying from Git

1. **Clone repository**:
   ```bash
   git clone https://github.com/your-repo/graph-rag-kuzu.git
   cd graph-rag-kuzu/
   ```

2. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

3. **Start server**:
   ```bash
   ./start.sh    # Linux/macOS/WSL
   start.bat     # Windows
   ```

4. **Open browser**: http://localhost:8000

✅ **Deployed!**

---

## Platform-Specific Instructions

### Windows

1. **Prerequisites**:
   - Python 3.8+ installed (download from python.org)
   - Added to PATH during installation

2. **Extract/Copy bundle**

3. **Open Command Prompt**:
   ```cmd
   cd path\to\deployment
   ```

4. **Install dependencies**:
   ```cmd
   pip install -r requirements.txt
   ```

5. **Start server**:
   ```cmd
   start.bat
   ```
   Or double-click `start.bat` file

6. **Open browser**: http://localhost:8000

### Linux

1. **Prerequisites**:
   - Python 3.8+ (usually pre-installed)
   - If not: `sudo apt install python3 python3-pip` (Ubuntu/Debian)

2. **Extract/Copy bundle**

3. **Navigate to directory**:
   ```bash
   cd deployment/
   ```

4. **Install dependencies**:
   ```bash
   pip3 install -r requirements.txt
   ```
   Or use virtual environment (recommended):
   ```bash
   python3 -m venv venv
   source venv/bin/activate
   pip install -r requirements.txt
   ```

5. **Make script executable**:
   ```bash
   chmod +x start.sh
   ```

6. **Start server**:
   ```bash
   ./start.sh
   ```

7. **Open browser**: http://localhost:8000

### macOS

1. **Prerequisites**:
   - Python 3.8+ (pre-installed or from python.org)
   - Homebrew (optional): `brew install python`

2. **Extract/Copy bundle**

3. **Open Terminal**:
   ```bash
   cd deployment/
   ```

4. **Install dependencies**:
   ```bash
   pip3 install -r requirements.txt
   ```
   Or use virtual environment (recommended):
   ```bash
   python3 -m venv venv
   source venv/bin/activate
   pip install -r requirements.txt
   ```

5. **Make script executable**:
   ```bash
   chmod +x start.sh
   ```

6. **Start server**:
   ```bash
   ./start.sh
   ```

7. **Open browser**: http://localhost:8000

### WSL (Windows Subsystem for Linux)

1. **Prerequisites**:
   - WSL installed (WSL2 recommended)
   - Ubuntu or other Linux distribution

2. **Copy bundle to WSL**:
   ```bash
   cp -r /mnt/c/path/to/deployment/ ~/
   cd ~/deployment/
   ```

3. **Install dependencies**:
   ```bash
   pip3 install -r requirements.txt
   ```

4. **Start server**:
   ```bash
   chmod +x start.sh
   ./start.sh
   ```

5. **Open browser**: Use Windows browser, navigate to http://localhost:8000

---

## Docker Deployment (Optional)

If you prefer Docker:

### Create Dockerfile

Create `Dockerfile` in deployment folder:

```dockerfile
FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

EXPOSE 8000

CMD ["python", "graph_rag_server.py"]
```

### Build and Run

```bash
# Build image
docker build -t graph-rag-kuzu .

# Run container
docker run -p 8000:8000 graph-rag-kuzu

# Open browser
# http://localhost:8000
```

---

## Cloud Deployment

### AWS EC2

1. **Launch EC2 instance** (Ubuntu 22.04 LTS)
2. **SSH into instance**
3. **Install Python**: `sudo apt update && sudo apt install python3-pip`
4. **Upload bundle**: `scp -r deployment/ user@instance-ip:~/`
5. **Install dependencies**: `pip3 install -r requirements.txt`
6. **Start server**: `./start.sh`
7. **Access**: `http://instance-ip:8000`

**Security**: Configure security group to allow port 8000

### Google Cloud

1. **Create VM** (Ubuntu 22.04)
2. **Upload bundle** via `gcloud scp`
3. **Install dependencies**
4. **Start server**
5. **Access via external IP**

### Azure

1. **Create VM** (Ubuntu 22.04)
2. **Upload bundle** via Azure portal or `scp`
3. **Install dependencies**
4. **Start server**
5. **Configure NSG** to allow port 8000

### Heroku

1. **Add `Procfile`**:
   ```
   web: python graph_rag_server.py
   ```

2. **Deploy**:
   ```bash
   git init
   git add .
   git commit -m "Deploy"
   heroku create
   git push heroku main
   ```

---

## Production Deployment

For production use, consider:

### 1. Use Production Server

Replace Uvicorn with production ASGI server:

**Install**:
```bash
pip install gunicorn
```

**Run**:
```bash
gunicorn graph_rag_server:app --workers 4 --worker-class uvicorn.workers.UvicornWorker --bind 0.0.0.0:8000
```

### 2. Reverse Proxy (Nginx)

**Install Nginx**:
```bash
sudo apt install nginx
```

**Configure** (`/etc/nginx/sites-available/graph-rag`):
```nginx
server {
    listen 80;
    server_name yourdomain.com;

    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

**Enable**:
```bash
sudo ln -s /etc/nginx/sites-available/graph-rag /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl restart nginx
```

### 3. HTTPS (Let's Encrypt)

```bash
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d yourdomain.com
```

### 4. Systemd Service

Create `/etc/systemd/system/graph-rag.service`:

```ini
[Unit]
Description=Graph-RAG Kuzu Explorer
After=network.target

[Service]
Type=simple
User=your-user
WorkingDirectory=/path/to/deployment
ExecStart=/usr/bin/python3 graph_rag_server.py
Restart=always

[Install]
WantedBy=multi-user.target
```

**Enable**:
```bash
sudo systemctl enable graph-rag
sudo systemctl start graph-rag
```

---

## Verification

After deployment, verify everything works:

### 1. Server Health

```bash
curl http://localhost:8000/api/health
# Should return: {"status":"online",...}
```

### 2. Load Sample Graph

1. Open http://localhost:8000
2. Click "📁 Load Graph JSON"
3. Select `examples/sample_graph.json`
4. Verify success

### 3. Test Features

- [ ] 3D visualization renders
- [ ] DB Explorer opens (Ctrl+K)
- [ ] Query execution works
- [ ] Entity search works
- [ ] Path finding works

---

## Troubleshooting

### Server won't start

**Check Python version**:
```bash
python --version  # Must be 3.8+
```

**Check dependencies**:
```bash
pip list | grep -E "(fastapi|uvicorn|kuzu)"
```

**Check port**:
```bash
lsof -i :8000  # Linux/macOS
netstat -ano | findstr :8000  # Windows
```

### Can't access from browser

**Check firewall**:
```bash
# Linux
sudo ufw allow 8000

# Windows
# Add inbound rule for port 8000
```

**Check server logs**: Look for errors in terminal

### Graph won't load

**Validate JSON**: https://jsonlint.com/

**Check file size**: Large files may timeout

**Check browser console**: F12 → Console for errors

---

## Post-Deployment

### Update documentation

- [ ] Note server URL/IP
- [ ] Document any custom configuration
- [ ] Update firewall rules
- [ ] Train users

### Monitor

- [ ] Check server health regularly
- [ ] Monitor resource usage
- [ ] Review logs for errors

### Backup

- [ ] Backup graph JSON files
- [ ] Backup configuration
- [ ] Document recovery procedure

---

## Summary

**Deployment is simple**:

1. ✅ Extract/copy bundle
2. ✅ Install dependencies (`pip install -r requirements.txt`)
3. ✅ Run server (`./start.sh` or `start.bat`)
4. ✅ Open browser (http://localhost:8000)

**That's it!** No build process, no compilation, no complex setup.

---

## Getting Help

- See `README.md` for full documentation
- See `DEPLOYMENT_CHECKLIST.md` for verification steps
- See `docs/TROUBLESHOOTING.md` for issue resolution

---

**Happy Deploying!** 🚀

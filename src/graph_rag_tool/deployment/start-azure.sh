#!/bin/bash
# Graph-RAG Tool - Azure Corporate Deployment Startup Script
# For corporate environments with Azure OpenAI access only
# NO HuggingFace model downloads

echo "========================================"
echo "Graph-RAG Tool - Azure Mode"
echo "Corporate Deployment (Firewall Safe)"
echo "========================================"
echo ""

# Set Azure embedding mode
export EMBEDDING_MODE=azure
echo "Setting embedding mode: AZURE"
echo "- No model downloads from HuggingFace"
echo "- Uses Azure OpenAI embeddings only"
echo "- Firewall compatible"
echo ""

# Check if Python is available
if ! command -v python3 &> /dev/null && ! command -v python &> /dev/null; then
    echo "ERROR: Python not found"
    echo "Please install Python 3.11 or 3.12"
    exit 1
fi

# Use python3 if available, otherwise python
PYTHON_CMD="python3"
if ! command -v python3 &> /dev/null; then
    PYTHON_CMD="python"
fi

echo "Python version:"
$PYTHON_CMD --version
echo ""

# Check if dependencies are installed
echo "Checking dependencies..."
$PYTHON_CMD -c "import fastapi, uvicorn, kuzu, openai, pydantic, numpy" 2>/dev/null
if [ $? -ne 0 ]; then
    echo ""
    echo "WARNING: Some dependencies are missing"
    echo "Installing from requirements-azure.txt..."
    echo ""
    pip install -r requirements-azure.txt
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to install dependencies"
        exit 1
    fi
fi

echo "All dependencies installed"
echo ""

# Display configuration
echo "========================================"
echo "Configuration:"
echo "========================================"
echo "Embedding Mode: AZURE"
echo "Database: Kuzu 0.7.1"
echo "Server Port: 8000"
echo ""
echo "Azure OpenAI Configuration (set in UI):"
echo "- API Key: [Configure in Settings panel]"
echo "- Endpoint: https://your-resource.openai.azure.com"
echo "- Chat Model: gpt-4o or gpt-4"
echo "- Embedding Model: text-embedding-3-small"
echo ""
echo "========================================"

# Start server
echo "Starting server..."
echo ""
echo "Server will be available at:"
echo "  http://localhost:8000"
echo ""
echo "IMPORTANT:"
echo "1. Open browser to http://localhost:8000"
echo "2. Click Settings (gear icon)"
echo "3. Configure Azure OpenAI credentials"
echo "4. Load graph JSON file"
echo "5. Generate embeddings (Settings panel)"
echo ""
echo "Press Ctrl+C to stop server"
echo ""
echo "========================================"
echo ""

# Start Python server
$PYTHON_CMD graph_rag_server.py

# If server exits
echo ""
echo "Server stopped."

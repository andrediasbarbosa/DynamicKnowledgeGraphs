#!/bin/bash
# Simple startup script for Graph RAG (merged frontend + backend)

cd "$(dirname "$0")"

echo "════════════════════════════════════════════"
echo "  Starting Graph RAG Server"
echo "════════════════════════════════════════════"
echo ""

# Check if venv exists
if [ ! -d "venv" ]; then
    echo "✗ Virtual environment not found!"
    echo "  Creating venv and installing dependencies..."
    python3 -m venv venv
    source venv/bin/activate
    pip install --upgrade pip
    pip install -r requirements.txt
else
    source venv/bin/activate
fi

# Get IP address
WSL_IP=$(hostname -I | awk '{print $1}')

echo "Starting server..."
echo ""
echo "⭐ Access the application at (use WSL IP from Windows):"
echo "  • http://$WSL_IP:8000 ← USE THIS from Windows browser!"
echo "  • http://localhost:8000 (WSL only)"
echo ""
echo "⚠️  IMPORTANT: Windows users MUST use the WSL IP address above"
echo "   'localhost' won't work from Windows - use $WSL_IP instead!"
echo ""
echo "Press Ctrl+C to stop"
echo "════════════════════════════════════════════"
echo ""

# Start the server
python graph_rag_server.py

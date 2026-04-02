#!/bin/bash

# Graph RAG with Kuzu Backend - Startup Script
# This script starts both backend (Kuzu + FastAPI) and frontend (HTTP server)

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
BACKEND_DIR="src/graph_rag_tool/backend"
FRONTEND_DIR="src/graph_rag_tool"
BACKEND_PORT=8000
FRONTEND_PORT=8080
WSL_IP=$(hostname -I | awk '{print $1}')

# Print header
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}   Graph RAG with Kuzu Backend - Startup${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
echo ""

# Function to check if a port is in use
check_port() {
    local port=$1
    if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1 || \
       netstat -tuln 2>/dev/null | grep -q ":$port " || \
       ss -tuln 2>/dev/null | grep -q ":$port "; then
        return 0  # Port is in use
    else
        return 1  # Port is free
    fi
}

# Function to wait for backend to be ready
wait_for_backend() {
    local max_wait=30
    local count=0

    echo -e "${YELLOW}⏳ Waiting for backend to initialize...${NC}"

    while [ $count -lt $max_wait ]; do
        if curl -s http://${WSL_IP}:${BACKEND_PORT}/ >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
        if [ $((count % 5)) -eq 0 ]; then
            echo -e "${YELLOW}   Still waiting... ($count/${max_wait}s)${NC}"
        fi
    done

    return 1
}

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down servers...${NC}"

    if [ ! -z "$BACKEND_PID" ]; then
        kill $BACKEND_PID 2>/dev/null || true
        echo -e "${GREEN}✓ Backend stopped${NC}"
    fi

    if [ ! -z "$FRONTEND_PID" ]; then
        kill $FRONTEND_PID 2>/dev/null || true
        echo -e "${GREEN}✓ Frontend stopped${NC}"
    fi

    echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}Goodbye!${NC}"
    echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
    exit 0
}

# Set up cleanup trap
trap cleanup INT TERM

# Check if backend is already running
if check_port $BACKEND_PORT; then
    echo -e "${YELLOW}⚠  Backend already running on port ${BACKEND_PORT}${NC}"
    echo -e "${YELLOW}   Kill it first or use existing instance${NC}"
    read -p "Kill existing backend and restart? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        fuser -k ${BACKEND_PORT}/tcp 2>/dev/null || pkill -f "python.*graph_rag_server" || true
        sleep 2
    else
        echo -e "${BLUE}ℹ  Using existing backend${NC}"
        BACKEND_RUNNING=true
    fi
fi

# Check if frontend is already running
if check_port $FRONTEND_PORT; then
    echo -e "${YELLOW}⚠  Frontend already running on port ${FRONTEND_PORT}${NC}"
    fuser -k ${FRONTEND_PORT}/tcp 2>/dev/null || true
    sleep 1
fi

# Start Backend
if [ "$BACKEND_RUNNING" != "true" ]; then
    echo -e "${BLUE}→ Starting Backend Server...${NC}"
    echo -e "  ${CYAN}Directory: ${BACKEND_DIR}${NC}"
    echo -e "  ${CYAN}Port: ${BACKEND_PORT}${NC}"
    echo -e "  ${CYAN}WSL IP: ${WSL_IP}${NC}"

    cd $BACKEND_DIR

    # Check if venv exists
    if [ ! -d "venv" ]; then
        echo -e "${RED}✗ Virtual environment not found!${NC}"
        echo -e "${YELLOW}  Run: cd ${BACKEND_DIR} && python3 -m venv venv && pip install -r requirements.txt${NC}"
        exit 1
    fi

    # Activate venv and start server in background
    source venv/bin/activate
    nohup python graph_rag_server.py > backend.log 2>&1 &
    BACKEND_PID=$!
    cd - > /dev/null

    # Wait for backend to be ready
    if wait_for_backend; then
        echo -e "${GREEN}✓ Backend started successfully${NC}"

        # Check database status
        schema=$(curl -s http://${WSL_IP}:${BACKEND_PORT}/api/schema)
        entity_count=$(echo $schema | python3 -c "import sys, json; print(json.load(sys.stdin).get('entity_count', 0))" 2>/dev/null || echo "0")

        if [ "$entity_count" -gt 0 ]; then
            echo -e "${GREEN}✓ Database loaded: ${entity_count} entities${NC}"
        else
            echo -e "${YELLOW}⚠  Database is empty (0 entities)${NC}"
            echo -e "${YELLOW}  The backend will initialize it on first startup${NC}"
        fi
    else
        echo -e "${RED}✗ Backend failed to start${NC}"
        echo -e "${YELLOW}  Check logs: tail ${BACKEND_DIR}/backend.log${NC}"
        kill $BACKEND_PID 2>/dev/null || true
        exit 1
    fi
else
    echo -e "${GREEN}✓ Backend already running${NC}"
fi

# Start Frontend
echo ""
echo -e "${BLUE}→ Starting Frontend Server...${NC}"
echo -e "  ${CYAN}Directory: ${FRONTEND_DIR}${NC}"
echo -e "  ${CYAN}Port: ${FRONTEND_PORT}${NC}"

cd $FRONTEND_DIR
python3 -m http.server $FRONTEND_PORT > /dev/null 2>&1 &
FRONTEND_PID=$!
cd - > /dev/null

sleep 2

if check_port $FRONTEND_PORT; then
    echo -e "${GREEN}✓ Frontend started successfully${NC}"
else
    echo -e "${RED}✗ Frontend failed to start${NC}"
    exit 1
fi

# Display summary
echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ All servers running!${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${BLUE}URLs:${NC}"
echo -e "  Frontend:  ${CYAN}http://localhost:${FRONTEND_PORT}/template.html${NC}"
echo -e "  Backend:   ${CYAN}http://${WSL_IP}:${BACKEND_PORT}${NC}"
echo -e "  API Docs:  ${CYAN}http://${WSL_IP}:${BACKEND_PORT}/docs${NC}"
echo -e "  Schema:    ${CYAN}http://${WSL_IP}:${BACKEND_PORT}/api/schema${NC}"
echo ""
echo -e "${YELLOW}Configuration:${NC}"
echo -e "  WSL IP:           ${CYAN}${WSL_IP}${NC}"
echo -e "  Backend URL:      ${CYAN}http://${WSL_IP}:${BACKEND_PORT}${NC}"
echo -e "  Frontend URL:     ${CYAN}http://localhost:${FRONTEND_PORT}${NC}"
echo ""
echo -e "${YELLOW}Next Steps:${NC}"
echo -e "  1. Open ${CYAN}http://localhost:${FRONTEND_PORT}/template.html${NC} in your browser"
echo -e "  2. Configure your LLM API key in the chat settings"
echo -e "  3. Ask questions about your knowledge graph!"
echo ""
echo -e "${YELLOW}Logs:${NC}"
echo -e "  Backend:  tail -f ${BACKEND_DIR}/backend.log"
echo ""
echo -e "${RED}Press Ctrl+C to stop all servers${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
echo ""

# Keep script running and wait for Ctrl+C
wait

#!/bin/bash

echo "🔍 Testing Graph RAG Server..."
echo ""

# Check if server is running
echo "1. Checking if server is running..."
if ps aux | grep -q "[g]raph_rag_server"; then
    echo "   ✅ Server process found"
else
    echo "   ❌ Server NOT running"
    echo "   → Start with: cd backend && ./start.sh"
    exit 1
fi

# Check port 8000
echo ""
echo "2. Checking port 8000..."
if lsof -i :8000 > /dev/null 2>&1 || netstat -tuln 2>/dev/null | grep -q ":8000"; then
    echo "   ✅ Port 8000 is active"
else
    echo "   ❌ Port 8000 not listening"
    exit 1
fi

# Test health endpoint
echo ""
echo "3. Testing health endpoint..."
HEALTH=$(curl -s http://localhost:8000/api/health 2>&1)
if echo "$HEALTH" | grep -q "online"; then
    echo "   ✅ Health check passed"
    echo "   Response: $HEALTH"
else
    echo "   ❌ Health check failed"
    echo "   Response: $HEALTH"
    exit 1
fi

# Test schema endpoint
echo ""
echo "4. Testing schema endpoint..."
SCHEMA=$(curl -s http://localhost:8000/api/schema 2>&1)
if echo "$SCHEMA" | grep -q "entity_count"; then
    echo "   ✅ Schema endpoint working"
    ENTITY_COUNT=$(echo "$SCHEMA" | grep -o '"entity_count":[0-9]*' | cut -d: -f2)
    echo "   Entities in database: $ENTITY_COUNT"
else
    echo "   ⚠️  Schema endpoint responded but may need graph upload"
fi

echo ""
echo "═══════════════════════════════════════"
echo "✅ Server is running correctly!"
echo ""
echo "Open browser: http://localhost:8000"
echo "═══════════════════════════════════════"

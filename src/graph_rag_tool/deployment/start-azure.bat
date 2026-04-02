@echo off
REM Graph-RAG Tool - Azure Corporate Deployment Startup Script
REM For corporate environments with Azure OpenAI access only
REM NO HuggingFace model downloads

echo ========================================
echo Graph-RAG Tool - Azure Mode
echo Corporate Deployment (Firewall Safe)
echo ========================================
echo.

REM Set Azure embedding mode
set EMBEDDING_MODE=azure
echo Setting embedding mode: AZURE
echo - No model downloads from HuggingFace
echo - Uses Azure OpenAI embeddings only
echo - Firewall compatible
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found
    echo Please install Python 3.11 or 3.12
    echo.
    pause
    exit /b 1
)

echo Python version:
python --version
echo.

REM Check if dependencies are installed
echo Checking dependencies...
python -c "import fastapi, uvicorn, kuzu, openai, pydantic, numpy" >nul 2>&1
if errorlevel 1 (
    echo.
    echo WARNING: Some dependencies are missing
    echo Installing from requirements-azure.txt...
    echo.
    pip install -r requirements-azure.txt
    echo.
    if errorlevel 1 (
        echo ERROR: Failed to install dependencies
        pause
        exit /b 1
    )
)

echo All dependencies installed
echo.

REM Display configuration
echo ========================================
echo Configuration:
echo ========================================
echo Embedding Mode: AZURE
echo Database: Kuzu 0.7.1
echo Server Port: 8000
echo.
echo Azure OpenAI Configuration (set in UI):
echo - API Key: [Configure in Settings panel]
echo - Endpoint: https://your-resource.openai.azure.com
echo - Chat Model: gpt-4o or gpt-4
echo - Embedding Model: text-embedding-3-small
echo.
echo ========================================

REM Start server
echo Starting server...
echo.
echo Server will be available at:
echo   http://localhost:8000
echo.
echo IMPORTANT:
echo 1. Open browser to http://localhost:8000
echo 2. Click Settings (gear icon)
echo 3. Configure Azure OpenAI credentials
echo 4. Load graph JSON file
echo 5. Generate embeddings (Settings panel)
echo.
echo Press Ctrl+C to stop server
echo.
echo ========================================
echo.

REM Start Python server
python graph_rag_server.py

REM If server exits
echo.
echo Server stopped.
pause

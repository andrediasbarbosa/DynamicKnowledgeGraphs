@echo off
REM Windows batch file to start Graph RAG server natively on Windows
REM No WSL required!

cd /d "%~dp0"

echo ================================================
echo   Starting Graph RAG Server (Native Windows)
echo ================================================
echo.

REM Check if Python is available
python --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Python not found!
    echo.
    echo Please install Python 3.8 or higher from:
    echo https://www.python.org/downloads/
    echo.
    echo Make sure to check "Add Python to PATH" during installation
    echo.
    pause
    exit /b 1
)

REM Check if dependencies are installed
python -c "import fastapi" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Installing dependencies...
    echo.
    pip install -r requirements.txt
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo ERROR: Failed to install dependencies
        echo.
        pause
        exit /b 1
    )
)

echo Starting server...
echo.
echo Access the application at:
echo   * http://localhost:8000
echo.
echo Press Ctrl+C to stop the server
echo ================================================
echo.

REM Start the server
python graph_rag_server.py

pause

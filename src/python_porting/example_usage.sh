#!/bin/bash
# Example usage of kg_extractor.py
#
# This script demonstrates how to use the Python knowledge graph extractor

echo "========================================="
echo "Knowledge Graph Extractor - Example Usage"
echo "========================================="
echo ""

# Check if Azure credentials are configured in the script
if ! grep -q 'AZURE_OPENAI_KEY = ".*[^"]' kg_extractor.py; then
    echo "Error: Azure OpenAI credentials not configured in kg_extractor.py"
    echo ""
    echo "Please edit kg_extractor.py and set:"
    echo "  AZURE_OPENAI_KEY = \"your-api-key\""
    echo "  AZURE_OPENAI_ENDPOINT = \"https://your-resource.openai.azure.com/\""
    echo ""
    exit 1
fi

# Check if a PDF file was provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <input.pdf>"
    echo ""
    echo "Example:"
    echo "  $0 ../../tests/sample.pdf"
    echo ""
    exit 1
fi

INPUT_PDF="$1"

# Check if file exists
if [ ! -f "$INPUT_PDF" ]; then
    echo "Error: File not found: $INPUT_PDF"
    exit 1
fi

echo "Input PDF: $INPUT_PDF"
echo "Output directory: ./output"
echo ""

# Run extraction
python3 kg_extractor.py "$INPUT_PDF" \
    --output ./output \
    --chunk-size 800 \
    --verbose

# Check if successful
if [ $? -eq 0 ]; then
    echo ""
    echo "========================================="
    echo "Extraction successful!"
    echo "========================================="
    echo ""
    echo "To view the results:"
    echo "  cd output"
    echo "  python3 -m http.server 8080"
    echo "  # Then open http://localhost:8080/graph.html"
    echo ""
else
    echo ""
    echo "Error: Extraction failed"
    exit 1
fi

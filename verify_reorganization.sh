#!/bin/bash

echo "=== Module Organization Verification ==="
echo ""

# Find line numbers of category headers
comb_line=$(grep -n "// COMBINATORIAL INSIGHTS SECTION" src/discovery/report_generator.cpp | cut -d: -f1)
expl_line=$(grep -n "// EXPLORATORY INSIGHTS SECTION" src/discovery/report_generator.cpp | cut -d: -f1)
trans_line=$(grep -n "// TRANSFORMATIONAL INSIGHTS SECTION" src/discovery/report_generator.cpp | cut -d: -f1)
rec_line=$(grep -n "// Recommendations" src/discovery/report_generator.cpp | head -1 | cut -d: -f1)

echo "📍 Section boundaries:"
echo "  Combinatorial: line $comb_line"
echo "  Exploratory:   line $expl_line"
echo "  Transformational: line $trans_line"
echo "  Recommendations: line $rec_line"
echo ""

echo "🧩 COMBINATORIAL modules ($comb_line - $expl_line):"
sed -n "${comb_line},${expl_line}p" src/discovery/report_generator.cpp | grep "auto& .* = by_type\[InsightType::" | sed 's/.*InsightType::/  - /' | sed 's/\];.*//'

echo ""
echo "🔍 EXPLORATORY modules ($expl_line - $trans_line):"
sed -n "${expl_line},${trans_line}p" src/discovery/report_generator.cpp | grep "auto& .* = by_type\[InsightType::" | sed 's/.*InsightType::/  - /' | sed 's/\];.*//'

echo ""
echo "🔄 TRANSFORMATIONAL modules ($trans_line - $rec_line):"
sed -n "${trans_line},${rec_line}p" src/discovery/report_generator.cpp | grep "auto& .* = by_type\[InsightType::" | sed 's/.*InsightType::/  - /' | sed 's/\];.*//'

echo ""
echo "✓ Reorganization complete!"

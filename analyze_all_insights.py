#!/usr/bin/env python3
import json
import sys
from pathlib import Path
from collections import defaultdict

# List of deprecated types (already removed)
DEPRECATED = {'analogical_transfer', 'co_authorship', 'mechanism_consolidation', 'intervention_bottleneck'}

def analyze_insights(run_dirs):
    """Analyze insights across multiple runs"""

    # Track statistics per insight type
    type_stats = defaultdict(lambda: {
        'count': 0,
        'total_score': 0.0,
        'min_score': float('inf'),
        'max_score': float('-inf'),
        'empty_witnesses': 0,
        'empty_evidence': 0,
        'runs_present': set(),
        'scores': []
    })

    total_runs = 0
    for run_dir in run_dirs:
        insights_file = Path(run_dir) / "Step_5_Discovery" / "insights.json"
        if not insights_file.exists():
            continue

        total_runs += 1
        run_name = Path(run_dir).name

        try:
            with open(insights_file, 'r') as f:
                data = json.load(f)
                insights = data.get('insights', [])

                for insight in insights:
                    itype = insight.get('type', 'unknown')

                    # Skip deprecated types
                    if itype in DEPRECATED:
                        continue

                    score = insight.get('score', 0.0)
                    witness_nodes = insight.get('witness_nodes', [])
                    evidence = insight.get('evidence_chunk_ids', [])

                    # Update type stats
                    stats = type_stats[itype]
                    stats['count'] += 1
                    stats['total_score'] += score
                    stats['scores'].append(score)
                    stats['min_score'] = min(stats['min_score'], score)
                    stats['max_score'] = max(stats['max_score'], score)
                    stats['runs_present'].add(run_name)

                    if not witness_nodes:
                        stats['empty_witnesses'] += 1
                    if not evidence:
                        stats['empty_evidence'] += 1

        except Exception as e:
            print(f"Error reading {insights_file}: {e}", file=sys.stderr)

    return type_stats, total_runs

def categorize_by_value(type_stats, total_runs):
    """Categorize insight types by value"""

    categories = {
        'high_value': [],      # Frequent, high quality, present in most runs
        'medium_value': [],    # Moderate frequency, good quality
        'low_value': [],       # Infrequent or low quality
        'very_low_value': []   # Very rare or broken
    }

    for itype, stats in type_stats.items():
        count = stats['count']
        avg_score = stats['total_score'] / count if count > 0 else 0.0
        empty_pct = (stats['empty_witnesses'] / count * 100) if count > 0 else 0.0
        runs = len(stats['runs_present'])
        run_coverage = runs / total_runs if total_runs > 0 else 0.0

        # Calculate value score
        value_score = 0

        # Frequency component (40%)
        if count >= 50:
            value_score += 40
        elif count >= 20:
            value_score += 30
        elif count >= 10:
            value_score += 20
        elif count >= 5:
            value_score += 10
        else:
            value_score += 0

        # Quality component (30%)
        if avg_score >= 0.9:
            value_score += 30
        elif avg_score >= 0.8:
            value_score += 25
        elif avg_score >= 0.7:
            value_score += 15
        else:
            value_score += 5

        # Run coverage component (20%)
        if run_coverage >= 0.7:
            value_score += 20
        elif run_coverage >= 0.5:
            value_score += 15
        elif run_coverage >= 0.3:
            value_score += 10
        else:
            value_score += 0

        # Connectivity component (10%)
        if empty_pct == 0:
            value_score += 10
        elif empty_pct < 10:
            value_score += 5
        else:
            value_score += 0

        entry = {
            'type': itype,
            'count': count,
            'avg_score': avg_score,
            'runs': runs,
            'run_coverage': run_coverage,
            'empty_pct': empty_pct,
            'value_score': value_score
        }

        # Categorize
        if value_score >= 70:
            categories['high_value'].append(entry)
        elif value_score >= 50:
            categories['medium_value'].append(entry)
        elif value_score >= 30:
            categories['low_value'].append(entry)
        else:
            categories['very_low_value'].append(entry)

    # Sort each category by value_score descending
    for cat in categories:
        categories[cat].sort(key=lambda x: x['value_score'], reverse=True)

    return categories

def print_report(categories, total_runs):
    """Print comprehensive value analysis report"""

    print("="*100)
    print("COMPREHENSIVE INSIGHT VALUE ANALYSIS - 54 ACTIVE TYPES")
    print("="*100)
    print()

    total_types = sum(len(cat) for cat in categories.values())
    total_insights = sum(entry['count'] for cat in categories.values() for entry in cat)

    print(f"Total Active Types: {total_types}")
    print(f"Total Insights Analyzed: {total_insights}")
    print(f"Total Runs: {total_runs}")
    print()

    # Summary by category
    print("="*100)
    print("VALUE DISTRIBUTION")
    print("="*100)
    print()

    for cat_name, entries in categories.items():
        cat_count = sum(e['count'] for e in entries)
        cat_pct = (cat_count / total_insights * 100) if total_insights > 0 else 0
        print(f"{cat_name.upper().replace('_', ' ')}: {len(entries)} types, {cat_count} insights ({cat_pct:.1f}%)")

    print()

    # Detailed breakdown
    for cat_name, entries in categories.items():
        if not entries:
            continue

        print("="*100)
        print(f"{cat_name.upper().replace('_', ' ')} INSIGHTS ({len(entries)} types)")
        print("="*100)
        print()

        print(f"{'Type':<35} {'Count':>8} {'Score':>8} {'Runs':>8} {'Empty%':>8} {'Value':>8}")
        print("-"*100)

        for entry in entries:
            print(f"{entry['type']:<35} "
                  f"{entry['count']:>8} "
                  f"{entry['avg_score']:>8.2f} "
                  f"{entry['runs']:>8}/{total_runs} "
                  f"{entry['empty_pct']:>7.1f}% "
                  f"{entry['value_score']:>8}")

        print()

    # Recommendations
    print("="*100)
    print("RECOMMENDATIONS")
    print("="*100)
    print()

    print("✅ HIGH VALUE - Keep and prioritize:")
    for entry in categories['high_value'][:10]:
        print(f"   • {entry['type']}: {entry['count']} insights, {entry['avg_score']:.2f} avg score")

    print()
    print("⚠️  LOW VALUE - Monitor closely:")
    for entry in categories['low_value']:
        print(f"   • {entry['type']}: {entry['count']} insights, {entry['runs']}/{total_runs} runs")

    print()
    print("🔴 VERY LOW VALUE - Consider deprecating:")
    for entry in categories['very_low_value']:
        reasons = []
        if entry['count'] < 5:
            reasons.append(f"only {entry['count']} insights")
        if entry['run_coverage'] < 0.3:
            reasons.append(f"only {entry['runs']}/{total_runs} runs")
        if entry['empty_pct'] > 50:
            reasons.append(f"{entry['empty_pct']:.0f}% empty witnesses")

        reason_str = ", ".join(reasons)
        print(f"   • {entry['type']}: {reason_str}")

    print()

    # Statistics
    print("="*100)
    print("SUMMARY STATISTICS")
    print("="*100)
    print()

    high_count = sum(e['count'] for e in categories['high_value'])
    medium_count = sum(e['count'] for e in categories['medium_value'])
    low_count = sum(e['count'] for e in categories['low_value'])
    very_low_count = sum(e['count'] for e in categories['very_low_value'])

    print(f"High Value Types: {len(categories['high_value'])}/{total_types} ({len(categories['high_value'])*100/total_types:.0f}%)")
    print(f"  → Generating: {high_count}/{total_insights} insights ({high_count*100/total_insights:.1f}%)")
    print()
    print(f"Medium Value Types: {len(categories['medium_value'])}/{total_types} ({len(categories['medium_value'])*100/total_types:.0f}%)")
    print(f"  → Generating: {medium_count}/{total_insights} insights ({medium_count*100/total_insights:.1f}%)")
    print()
    print(f"Low Value Types: {len(categories['low_value'])}/{total_types} ({len(categories['low_value'])*100/total_types:.0f}%)")
    print(f"  → Generating: {low_count}/{total_insights} insights ({low_count*100/total_insights:.1f}%)")
    print()
    print(f"Very Low Value Types: {len(categories['very_low_value'])}/{total_types} ({len(categories['very_low_value'])*100/total_types:.0f}%)")
    print(f"  → Generating: {very_low_count}/{total_insights} insights ({very_low_count*100/total_insights:.1f}%)")
    print()

    print("="*100)
    print("POTENTIAL SAVINGS FROM REMOVING VERY LOW VALUE TYPES")
    print("="*100)
    print()

    if categories['very_low_value']:
        print(f"Types to consider removing: {len(categories['very_low_value'])}")
        print(f"Insights eliminated: {very_low_count} ({very_low_count*100/total_insights:.1f}%)")
        print(f"Compute time saved: ~{len(categories['very_low_value'])*1.8:.0f}%")
    else:
        print("✅ No very low value types found. Pipeline is well optimized!")

if __name__ == '__main__':
    run_dirs = [
        'runs/run_20260329_155622',
        'runs/run_20260328_205849',
        'runs/run_20260328_191905',
        'runs/run_20260328_004433',
        'runs/run_20260326_173330',
        'runs/run_20260326_143012',
        'runs/run_20260325_215039',
        'runs/run_20260325_102032',
        'runs/run_20260311_000205',
        'runs/run_20260322_230605'
    ]

    type_stats, total_runs = analyze_insights(run_dirs)
    categories = categorize_by_value(type_stats, total_runs)
    print_report(categories, total_runs)

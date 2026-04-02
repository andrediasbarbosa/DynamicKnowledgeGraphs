#!/usr/bin/env python3
import json
import sys
from pathlib import Path
from collections import defaultdict

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

    run_summaries = {}

    for run_dir in run_dirs:
        insights_file = Path(run_dir) / "Step_5_Discovery" / "insights.json"
        if not insights_file.exists():
            continue

        run_name = Path(run_dir).name

        try:
            with open(insights_file, 'r') as f:
                data = json.load(f)
                insights = data.get('insights', [])

                run_summary = defaultdict(int)

                for insight in insights:
                    itype = insight.get('type', 'unknown')
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

                    run_summary[itype] += 1

                run_summaries[run_name] = {
                    'total': len(insights),
                    'by_type': dict(run_summary)
                }
        except Exception as e:
            print(f"Error reading {insights_file}: {e}", file=sys.stderr)

    return type_stats, run_summaries

def print_report(type_stats, run_summaries):
    """Print comprehensive analysis report"""

    total_runs = len(run_summaries)

    print("="*80)
    print("INSIGHT TYPE ANALYSIS - LAST 10 RUNS")
    print("="*80)
    print()

    # Sort by count (descending)
    sorted_types = sorted(type_stats.items(), key=lambda x: x[1]['count'], reverse=True)

    print(f"{'Type':<30} {'Count':>8} {'Avg Score':>10} {'Min':>8} {'Max':>8} {'Runs':>6} {'Empty%':>8}")
    print("-"*80)

    for itype, stats in sorted_types:
        count = stats['count']
        avg_score = stats['total_score'] / count if count > 0 else 0.0
        min_score = stats['min_score'] if stats['min_score'] != float('inf') else 0.0
        max_score = stats['max_score'] if stats['max_score'] != float('-inf') else 0.0
        runs = len(stats['runs_present'])
        empty_pct = (stats['empty_witnesses'] / count * 100) if count > 0 else 0.0

        print(f"{itype:<30} {count:>8} {avg_score:>10.2f} {min_score:>8.2f} {max_score:>8.2f} {runs:>6}/{total_runs} {empty_pct:>7.1f}%")

    print()
    print("="*80)
    print("LOW-VALUE INSIGHT CANDIDATES FOR REMOVAL")
    print("="*80)
    print()

    candidates = []

    for itype, stats in sorted_types:
        count = stats['count']
        avg_score = stats['total_score'] / count if count > 0 else 0.0
        empty_pct = (stats['empty_witnesses'] / count * 100) if count > 0 else 0.0
        runs = len(stats['runs_present'])

        reasons = []
        priority = 0

        # Criteria for low value
        if count == 0:
            reasons.append("Zero insights generated")
            priority = 5
        elif count < 5:
            reasons.append(f"Very low count ({count} total across {total_runs} runs)")
            priority = max(priority, 4)
        elif avg_score < 0.5:
            reasons.append(f"Low average score ({avg_score:.2f})")
            priority = max(priority, 3)
        elif empty_pct > 80:
            reasons.append(f"High empty witness rate ({empty_pct:.1f}%)")
            priority = max(priority, 3)
        elif runs < total_runs * 0.3:
            reasons.append(f"Rarely produces results ({runs}/{total_runs} runs)")
            priority = max(priority, 2)

        if reasons:
            candidates.append({
                'type': itype,
                'priority': priority,
                'count': count,
                'avg_score': avg_score,
                'empty_pct': empty_pct,
                'runs': runs,
                'reasons': reasons
            })

    # Sort by priority (descending)
    candidates.sort(key=lambda x: (x['priority'], -x['count']), reverse=True)

    if not candidates:
        print("✓ No low-value insights identified. All insight types are producing useful results.")
    else:
        print(f"Found {len(candidates)} insight type(s) that may be candidates for removal:\n")

        for i, c in enumerate(candidates, 1):
            priority_label = ['', 'LOW', 'MEDIUM', 'HIGH', 'VERY HIGH', 'CRITICAL'][c['priority']]
            print(f"{i}. {c['type']}")
            print(f"   Priority: {priority_label}")
            print(f"   Count: {c['count']} | Avg Score: {c['avg_score']:.2f} | Empty: {c['empty_pct']:.1f}% | Runs: {c['runs']}/{total_runs}")
            print(f"   Reasons:")
            for reason in c['reasons']:
                print(f"     • {reason}")
            print()

    print("="*80)
    print("RUN-BY-RUN SUMMARY")
    print("="*80)
    print()

    for run_name in sorted(run_summaries.keys(), reverse=True):
        summary = run_summaries[run_name]
        print(f"{run_name}: {summary['total']} total insights")

        # Show top 5 insight types for this run
        sorted_types_run = sorted(summary['by_type'].items(), key=lambda x: x[1], reverse=True)[:5]
        for itype, count in sorted_types_run:
            print(f"  • {itype}: {count}")
        print()

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

    type_stats, run_summaries = analyze_insights(run_dirs)
    print_report(type_stats, run_summaries)

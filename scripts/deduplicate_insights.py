#!/usr/bin/env python3
"""
Deduplicate and simplify insight labels in insights.json

This script cleans up verbose and duplicate augmented nodes by:
1. Normalizing seed labels (remove filler words)
2. Deduplicating insights with similar labels
3. Shortening overly long labels

Usage:
    python deduplicate_insights.py input.json output.json

Example:
    python deduplicate_insights.py \
      runs/run_XXX/Step_4_Discovery/insights.json \
      runs/run_XXX/Step_4_Discovery/insights_clean.json
"""

import json
import sys
from collections import defaultdict
from typing import Dict, List, Any

def normalize_label(label: str) -> str:
    """
    Normalize a label by removing filler words and limiting length

    Args:
        label: Original label string

    Returns:
        Normalized label
    """
    if not label:
        return label

    normalized = label.lower().strip()

    # Remove trailing filler words
    fillers = [
        ' technique', ' techniques',
        ' method', ' methods',
        ' approach', ' approaches',
        ' algorithm', ' algorithms',
        ' system', ' systems',
        ' framework', ' frameworks',
        ' process', ' processes',
        ' procedure', ' procedures',
        ' strategy', ' strategies',
        ' mechanism', ' mechanisms'
    ]

    for filler in fillers:
        if normalized.endswith(filler):
            normalized = normalized[:-len(filler)]
            break  # Only remove one filler

    # Limit length (keep original case for actual label)
    if len(normalized) > 50:
        # Try to cut at word boundary
        cutoff = label[:47].rfind(' ')
        if cutoff > 30:
            return label[:cutoff] + '...'
        return label[:47] + '...'

    return label  # Return original case

def create_canonical_key(insight: Dict[str, Any]) -> str:
    """
    Create a canonical key for grouping similar insights

    Args:
        insight: Insight dictionary

    Returns:
        Canonical key string
    """
    parts = [insight.get('type', 'unknown')]

    # Add normalized seed labels (sorted for consistency)
    seed_labels = insight.get('seed_labels', [])
    if seed_labels:
        normalized_labels = sorted([
            normalize_label(label).lower()
            for label in seed_labels
        ])
        parts.extend(normalized_labels)

    return ':'.join(parts)

def deduplicate_insights(insights_data: Dict[str, Any]) -> Dict[str, Any]:
    """
    Remove duplicate insights, keeping the highest-scored one

    Args:
        insights_data: Full insights.json data

    Returns:
        Cleaned insights data
    """
    groups = defaultdict(list)

    # Group insights by canonical key
    for i, insight in enumerate(insights_data.get('insights', [])):
        key = create_canonical_key(insight)
        groups[key].append((i, insight))

    # Select best insight from each group
    keep_indices = set()
    merge_stats = {'groups': 0, 'removed': 0}

    for group in groups.values():
        if len(group) == 1:
            # No duplicates
            keep_indices.add(group[0][0])
        else:
            # Multiple similar insights - keep highest scored
            merge_stats['groups'] += 1
            merge_stats['removed'] += len(group) - 1

            best_idx, best_insight = max(
                group,
                key=lambda x: x[1].get('score', 0)
            )
            keep_indices.add(best_idx)

            # Optionally merge evidence from duplicates
            # (Uncomment to aggregate evidence)
            # for idx, insight in group:
            #     if idx != best_idx:
            #         best_insight['evidence_chunk_ids'] = list(set(
            #             best_insight.get('evidence_chunk_ids', []) +
            #             insight.get('evidence_chunk_ids', [])
            #         ))

    # Filter to kept insights
    insights_data['insights'] = [
        insights_data['insights'][i]
        for i in sorted(keep_indices)
    ]

    print(f"  Merged {merge_stats['groups']} groups of duplicates")
    print(f"  Removed {merge_stats['removed']} duplicate insights")

    return insights_data

def simplify_labels(insights_data: Dict[str, Any]) -> Dict[str, Any]:
    """
    Simplify all seed labels in insights

    Args:
        insights_data: Full insights.json data

    Returns:
        Insights with simplified labels
    """
    simplified_count = 0

    for insight in insights_data.get('insights', []):
        if 'seed_labels' in insight:
            original_labels = insight['seed_labels']
            simplified_labels = [normalize_label(label) for label in original_labels]

            # Count how many actually changed
            for orig, simp in zip(original_labels, simplified_labels):
                if orig != simp:
                    simplified_count += 1

            insight['seed_labels'] = simplified_labels

    print(f"  Simplified {simplified_count} labels")

    return insights_data

def print_statistics(before_data: Dict[str, Any], after_data: Dict[str, Any]):
    """Print before/after statistics"""
    before_count = len(before_data.get('insights', []))
    after_count = len(after_data.get('insights', []))
    removed = before_count - after_count

    print(f"\n{'='*60}")
    print(f"Statistics:")
    print(f"{'='*60}")
    print(f"Before:  {before_count:6d} insights")
    print(f"After:   {after_count:6d} insights")
    print(f"Removed: {removed:6d} insights ({100*removed/before_count:.1f}%)")
    print(f"{'='*60}\n")

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        print("\nError: Expected 2 arguments")
        print("Usage: python deduplicate_insights.py <input.json> <output.json>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    print(f"\nLoading insights from: {input_path}")

    try:
        with open(input_path, 'r') as f:
            data = json.load(f)
    except FileNotFoundError:
        print(f"Error: File not found: {input_path}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON: {e}")
        sys.exit(1)

    # Keep copy for statistics
    import copy
    before_data = copy.deepcopy(data)

    print(f"\nProcessing...")

    # Step 1: Simplify labels
    print("\nStep 1: Simplifying labels")
    data = simplify_labels(data)

    # Step 2: Deduplicate
    print("\nStep 2: Deduplicating insights")
    data = deduplicate_insights(data)

    # Print statistics
    print_statistics(before_data, data)

    # Save cleaned data
    print(f"Writing cleaned insights to: {output_path}")
    with open(output_path, 'w') as f:
        json.dump(data, f, indent=2)

    print(f"\n✓ Done! Use cleaned insights with:")
    print(f"  ./batch4 render -i graph.json --insights {output_path} -o viz/\n")

if __name__ == '__main__':
    main()

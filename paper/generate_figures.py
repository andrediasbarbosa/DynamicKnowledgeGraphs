#!/usr/bin/env python3
"""
Figure Generation Script for Publication
Generates figures 3, 6, 7, and 8 automatically using matplotlib/seaborn

Usage:
    python generate_figures.py [--output-dir figures]

Requirements:
    pip install matplotlib seaborn numpy
"""

import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import argparse
import os
from pathlib import Path


def setup_plot_style():
    """Set consistent plot style for all figures"""
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams.update({
        'font.size': 11,
        'axes.labelsize': 12,
        'axes.titlesize': 13,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.titlesize': 14,
        'font.family': 'serif',
        'font.serif': ['Computer Modern Roman'],
        'text.usetex': False,  # Set to True if you have LaTeX installed
    })


def generate_figure3_bias_audit(output_dir):
    """
    Figure 3: Bias Audit Results
    Two-panel figure: bar chart + Lorenz curve
    """
    print("Generating Figure 3: Bias Audit Results...")

    # Sample data (replace with actual data)
    sources = [
        'Relational_Data', 'KG_Reasoning', 'Geranium_Platform',
        'KG_Construction', 'Evolvable_KGs', 'NeuroSymbolic',
        'KG_Querying', 'Embedding_Uncertainty', 'KG_Curation',
        'MultiHop_QA', 'KGs_and_LLMs', 'Paths_over_KG',
        'AI_Model_Valid', 'Semantic_AI', 'Visual_Construct',
        'Rule_Guided', 'Robust_RAG', 'Sheaf_KGs',
        'Research_KGs', 'DataBook', 'ProfWilson', 'SettledStatus'
    ]

    # Citation counts (sorted descending)
    counts = np.array([18, 14, 12, 10, 8, 7, 6, 6, 5, 5, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1])
    mean_count = counts.mean()

    # Determine colors based on representation
    colors = []
    for c in counts:
        if c > 2 * mean_count:
            colors.append('#F44336')  # Red - over-represented
        elif c < mean_count / 2:
            colors.append('#2196F3')  # Blue - under-represented
        else:
            colors.append('#9E9E9E')  # Gray - normal

    # Create figure with two panels
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 12))

    # Top panel: Bar chart
    y_pos = np.arange(len(sources))
    ax1.barh(y_pos, counts, color=colors, alpha=0.8)
    ax1.axvline(mean_count, ls='--', color='black', linewidth=2, label=f'Mean ({mean_count:.1f})')
    ax1.axvline(2 * mean_count, ls='--', color='red', linewidth=2, label=f'2× Mean ({2*mean_count:.1f})')

    ax1.set_yticks(y_pos)
    ax1.set_yticklabels(sources, fontsize=9)
    ax1.set_xlabel('Number of Citations', fontweight='bold')
    ax1.set_ylabel('Source Document', fontweight='bold')
    ax1.set_title('Source Citation Distribution', fontweight='bold', pad=20)
    ax1.legend(loc='lower right')
    ax1.grid(axis='x', alpha=0.3)

    # Add count labels
    for i, count in enumerate(counts):
        ax1.text(count + 0.3, i, str(count), va='center', fontsize=9)

    # Bottom panel: Lorenz curve
    sorted_counts = np.sort(counts)
    cumsum = np.cumsum(sorted_counts)
    cumsum_pct = cumsum / cumsum[-1]
    x_pct = np.linspace(0, 1, len(counts))

    # Calculate Gini coefficient
    n = len(counts)
    sum_of_ranks = sum((i + 1) * count for i, count in enumerate(sorted_counts))
    gini = (2 * sum_of_ranks) / (n * sum(sorted_counts)) - (n + 1) / n

    # Plot equality line and Lorenz curve
    ax2.plot([0, 1], [0, 1], 'k--', linewidth=2, label='Perfect Equality', alpha=0.7)
    ax2.plot(np.concatenate([[0], x_pct]), np.concatenate([[0], cumsum_pct]),
             'b-', linewidth=3, label='Actual Distribution')
    ax2.fill_between(np.concatenate([[0], x_pct]),
                      np.concatenate([[0], x_pct]),
                      np.concatenate([[0], cumsum_pct]),
                      alpha=0.3, color='orange', label='Inequality Area')

    # Annotations
    ax2.text(0.6, 0.25, f'Gini Coefficient = {gini:.2f}',
             fontsize=14, fontweight='bold',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    ax2.text(0.6, 0.15, f'Fairness Score = {1-gini:.2f}',
             fontsize=12, style='italic')

    ax2.set_xlabel('Cumulative % of Sources', fontweight='bold')
    ax2.set_ylabel('Cumulative % of Citations', fontweight='bold')
    ax2.set_title('Lorenz Curve (Representation Inequality)', fontweight='bold', pad=20)
    ax2.legend(loc='upper left')
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim([0, 1])
    ax2.set_ylim([0, 1])

    plt.tight_layout()
    output_path = os.path.join(output_dir, 'bias_audit_results.pdf')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def generate_figure6_efficiency(output_dir):
    """
    Figure 6: Development Efficiency Comparison
    Horizontal bar chart with grouped bars
    """
    print("Generating Figure 6: Development Efficiency Chart...")

    features = ['Future Work\nUpdates', 'Pattern\nLibrary',
                'Bridge\nAnalogies', 'Bias\nAudit', 'Community\nRecs']

    # Time in hours
    estimated_mid = np.array([20, 28, 32, 48, 48])  # Mid-point of ranges
    actual = np.array([2, 3, 4, 2, 2])
    errors = np.array([4, 4, 8, 8, 8])  # Half of range

    y_pos = np.arange(len(features))

    fig, ax = plt.subplots(figsize=(12, 7))

    # Bars
    bar1 = ax.barh(y_pos - 0.2, estimated_mid, 0.4,
                   xerr=errors, capsize=5,
                   label='Estimated Time', color='#9E9E9E', alpha=0.7,
                   error_kw={'linewidth': 2, 'ecolor': '#424242'})
    bar2 = ax.barh(y_pos + 0.2, actual, 0.4,
                   label='Actual Time', color='#4CAF50', alpha=0.9)

    # Efficiency multiplier annotations
    for i, (est, act) in enumerate(zip(estimated_mid, actual)):
        mult = est / act
        # Position annotation to the right of the longer bar
        x_pos = max(est + errors[i], act) + 3
        ax.text(x_pos, y_pos[i], f'{mult:.0f}×',
                va='center', fontsize=14, fontweight='bold',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='yellow', alpha=0.7))

    ax.set_yticks(y_pos)
    ax.set_yticklabels(features, fontsize=12)
    ax.set_xlabel('Time (hours)', fontweight='bold', fontsize=13)
    ax.set_title('Development Efficiency: Estimated vs. Actual Implementation Time',
                 fontweight='bold', fontsize=14, pad=20)
    ax.legend(loc='lower right', fontsize=12)
    ax.grid(axis='x', alpha=0.3)
    ax.set_xlim([0, max(estimated_mid + errors) + 10])

    # Add vertical line at 0 for clarity
    ax.axvline(0, color='black', linewidth=0.8)

    plt.tight_layout()
    output_path = os.path.join(output_dir, 'efficiency_chart.pdf')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def generate_figure7_pattern_distribution(output_dir):
    """
    Figure 7: Pattern Distribution
    Stacked bar chart by size and type
    """
    print("Generating Figure 7: Pattern Distribution...")

    sizes = ['2-node', '3-node', '4-node']
    motifs = np.array([40, 5, 0])
    k_truss = np.array([25, 1, 1])
    meta = np.array([0, 0, 1])

    x = np.arange(len(sizes))
    width = 0.6

    fig, ax = plt.subplots(figsize=(10, 7))

    # Stacked bars
    p1 = ax.bar(x, motifs, width, label='Motifs', color='#2196F3', alpha=0.9)
    p2 = ax.bar(x, k_truss, width, bottom=motifs,
                label='K-Truss', color='#4CAF50', alpha=0.9)
    p3 = ax.bar(x, meta, width, bottom=motifs+k_truss,
                label='Meta-Pattern', color='#FF9800', alpha=0.9)

    ax.set_ylabel('Count', fontweight='bold', fontsize=13)
    ax.set_xlabel('Pattern Size', fontweight='bold', fontsize=13)
    ax.set_title('Pattern Distribution by Size and Type (Total: 73 patterns)',
                 fontweight='bold', fontsize=14, pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(sizes, fontsize=12)
    ax.legend(loc='upper right', fontsize=12)
    ax.grid(axis='y', alpha=0.3)

    # Add total counts on top of bars
    totals = motifs + k_truss + meta
    for i, total in enumerate(totals):
        ax.text(i, total + 2, f'{int(total)}',
                ha='center', fontweight='bold', fontsize=14,
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.8))

    # Add percentages
    percentages = (totals / totals.sum() * 100)
    for i, pct in enumerate(percentages):
        ax.text(i, -5, f'({pct:.0f}%)',
                ha='center', fontsize=11, style='italic')

    ax.set_ylim([0, max(totals) + 8])

    plt.tight_layout()
    output_path = os.path.join(output_dir, 'pattern_distribution.pdf')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def generate_figure8_recommendation_quality(output_dir):
    """
    Figure 8: Recommendation Quality Heatmap
    Shows distribution of recommendations by category and similarity
    """
    print("Generating Figure 8: Recommendation Quality Heatmap...")

    # Data: rows=categories, cols=similarity ranges
    data = np.array([
        [20, 40, 52, 20],  # Within-community
        [30, 28, 10, 0]     # Cross-community
    ])

    # Calculate percentages for annotations
    row_totals = data.sum(axis=1)
    percentages = (data.T / row_totals * 100).T

    fig, ax = plt.subplots(figsize=(12, 5))

    # Create heatmap
    im = ax.imshow(data, cmap='YlOrRd', aspect='auto', alpha=0.8)

    # Set ticks
    ax.set_xticks(np.arange(4))
    ax.set_yticks(np.arange(2))
    ax.set_xticklabels(['0.3-0.5', '0.5-0.7', '0.7-0.9', '0.9-1.0'], fontsize=12)
    ax.set_yticklabels(['Within-Community\n(Familiar)', 'Cross-Community\n(Novel)'], fontsize=12)

    # Labels
    ax.set_xlabel('Similarity Score Range', fontweight='bold', fontsize=13)
    ax.set_ylabel('Recommendation Category', fontweight='bold', fontsize=13)
    ax.set_title('Recommendation Quality Distribution (Total: 200 recommendations)',
                 fontweight='bold', fontsize=14, pad=20)

    # Annotate cells with counts and percentages
    for i in range(2):
        for j in range(4):
            count = data[i, j]
            pct = percentages[i, j]
            text_color = 'white' if data[i, j] > 30 else 'black'
            ax.text(j, i, f'{count}\n({pct:.1f}%)',
                    ha='center', va='center',
                    color=text_color, fontsize=13, fontweight='bold')

    # Add total counts on the right
    for i, total in enumerate(row_totals):
        pct = total / data.sum() * 100
        ax.text(4.2, i, f'Total: {total}\n({pct:.0f}%)',
                va='center', fontsize=12, fontweight='bold',
                bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.7))

    # Colorbar
    cbar = plt.colorbar(im, ax=ax, pad=0.15)
    cbar.set_label('Count', rotation=270, labelpad=20, fontweight='bold')

    plt.tight_layout()
    output_path = os.path.join(output_dir, 'recommendation_quality.pdf')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(description='Generate publication figures')
    parser.add_argument('--output-dir', default='figures',
                        help='Output directory for figures (default: figures)')
    args = parser.parse_args()

    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True, parents=True)
    print(f"\nGenerating figures in: {output_dir.absolute()}\n")

    # Set plot style
    setup_plot_style()

    # Generate figures
    try:
        generate_figure3_bias_audit(str(output_dir))
        generate_figure6_efficiency(str(output_dir))
        generate_figure7_pattern_distribution(str(output_dir))
        generate_figure8_recommendation_quality(str(output_dir))

        print(f"\n✅ Successfully generated 4 figures!")
        print(f"\nRemaining figures to create manually:")
        print("  • Figure 1: System Architecture (use Draw.io or TikZ)")
        print("  • Figure 2: Bridge Analogy Example (use Gephi or NetworkX)")
        print("  • Figure 4: Pattern Library Structure (use Draw.io)")
        print("  • Figure 5: Community Structure (use Gephi or NetworkX)")
        print("  • Figure 9: Example Subgraph (use Gephi or NetworkX)")
        print(f"\nSee figure_specifications.md for detailed instructions.")

    except Exception as e:
        print(f"\n❌ Error generating figures: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == '__main__':
    exit(main())

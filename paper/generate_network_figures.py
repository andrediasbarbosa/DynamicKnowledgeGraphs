#!/usr/bin/env python3
"""
Network Figure Generation Script for Publication
Generates figures 2, 5, and 9 using NetworkX for graph visualization

Usage:
    python generate_network_figures.py [--output-dir figures]

Requirements:
    pip install networkx matplotlib numpy
"""

import networkx as nx
import matplotlib.pyplot as plt
import numpy as np
import argparse
from pathlib import Path


def setup_plot_style():
    """Set consistent plot style"""
    plt.rcParams.update({
        'font.size': 10,
        'font.family': 'serif',
        'font.serif': ['Computer Modern Roman'],
    })


def generate_figure2_bridge_analogy_example(output_dir):
    """
    Figure 2: Bridge-Mediated Analogy Example
    Shows two communities connected by a bridge entity
    """
    print("Generating Figure 2: Bridge Analogy Example...")

    G = nx.Graph()

    # Left community (ML)
    ml_nodes = ['neural_network', 'optimization', 'gradient_descent', 'transformer']
    G.add_nodes_from(ml_nodes, community='ML')
    G.add_edges_from([
        ('neural_network', 'optimization'),
        ('optimization', 'gradient_descent'),
        ('transformer', 'optimization'),
    ])

    # Right community (Symbolic AI)
    symbolic_nodes = ['reasoning', 'logic', 'inference', 'theorem_proving']
    G.add_nodes_from(symbolic_nodes, community='Symbolic')
    G.add_edges_from([
        ('reasoning', 'logic'),
        ('logic', 'inference'),
        ('inference', 'theorem_proving'),
    ])

    # Bridge entity
    G.add_node('knowledge_graph', community='bridge')

    # Bridge connections
    G.add_edges_from([
        ('neural_network', 'knowledge_graph'),
        ('transformer', 'knowledge_graph'),
        ('knowledge_graph', 'reasoning'),
        ('knowledge_graph', 'logic'),
    ])

    # Layout
    pos = {}

    # Position ML community on left
    ml_y_positions = np.linspace(0, 1, len(ml_nodes))
    for i, node in enumerate(ml_nodes):
        pos[node] = (0.2, ml_y_positions[i])

    # Position Symbolic community on right
    sym_y_positions = np.linspace(0, 1, len(symbolic_nodes))
    for i, node in enumerate(symbolic_nodes):
        pos[node] = (0.8, sym_y_positions[i])

    # Position bridge in center
    pos['knowledge_graph'] = (0.5, 0.5)

    # Adjust positions with spring layout for better aesthetics
    pos = nx.spring_layout(G, pos=pos, iterations=50, k=0.3)

    # Create figure
    fig, ax = plt.subplots(figsize=(14, 10))

    # Define colors
    node_colors = []
    node_sizes = []
    for node in G.nodes():
        comm = G.nodes[node]['community']
        if comm == 'ML':
            node_colors.append('#4CAF50')  # Green
            node_sizes.append(2000)
        elif comm == 'Symbolic':
            node_colors.append('#2196F3')  # Blue
            node_sizes.append(2000)
        else:  # bridge
            node_colors.append('#FFC107')  # Gold/Yellow
            node_sizes.append(4000)

    # Draw edges
    nx.draw_networkx_edges(G, pos, ax=ax, width=2, alpha=0.5, edge_color='#424242')

    # Draw nodes
    nx.draw_networkx_nodes(G, pos, ax=ax,
                           node_color=node_colors,
                           node_size=node_sizes,
                           alpha=0.9,
                           edgecolors='black',
                           linewidths=2)

    # Draw labels
    labels = {node: node.replace('_', '\n') for node in G.nodes()}
    nx.draw_networkx_labels(G, pos, labels, ax=ax,
                           font_size=11,
                           font_weight='bold',
                           font_family='sans-serif')

    # Add community labels
    ax.text(0.15, 0.95, 'ML Community',
            fontsize=16, fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='#4CAF50', alpha=0.3))
    ax.text(0.75, 0.95, 'Symbolic AI Community',
            fontsize=16, fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='#2196F3', alpha=0.3))
    ax.text(0.45, 0.05, 'Bridge Entity',
            fontsize=14, fontweight='bold', style='italic',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='#FFC107', alpha=0.3))

    # Add relation annotations
    ax.annotate('uses', xy=pos['neural_network'], xytext=pos['optimization'],
                fontsize=9, ha='center', style='italic', color='gray')
    ax.annotate('uses', xy=pos['reasoning'], xytext=pos['logic'],
                fontsize=9, ha='center', style='italic', color='gray')

    ax.set_title('Bridge-Mediated Analogy: Cross-Domain Transfer via "knowledge graph"',
                 fontsize=16, fontweight='bold', pad=20)
    ax.axis('off')
    plt.tight_layout()

    output_path = output_dir / 'bridge_analogy_example.pdf'
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def generate_figure5_community_structure(output_dir):
    """
    Figure 5: Community Structure with Bridges
    Shows full knowledge graph with communities and bridges highlighted
    """
    print("Generating Figure 5: Community Structure...")

    # Create a larger graph with 3 communities
    G = nx.Graph()

    # Community 1: ML/Embeddings (30 nodes)
    ml_topics = ['neural_network', 'transformer', 'embedding', 'llm', 'gnn',
                 'attention', 'bert', 'gpt', 'word2vec', 'glove',
                 'fasttext', 'elmo', 'convnet', 'rnn', 'lstm',
                 'gradient_descent', 'backprop', 'adam', 'sgd', 'optimization',
                 'loss_function', 'regularization', 'dropout', 'batch_norm', 'activation',
                 'relu', 'softmax', 'cross_entropy', 'learning_rate', 'epoch']
    G.add_nodes_from(ml_topics, community=0)

    # Add some edges within ML community
    for i in range(len(ml_topics) - 1):
        G.add_edge(ml_topics[i], ml_topics[i + 1])
        if i < len(ml_topics) - 2:
            G.add_edge(ml_topics[i], ml_topics[i + 2])

    # Community 2: Symbolic Reasoning (25 nodes)
    symbolic_topics = ['reasoning', 'logic', 'inference', 'theorem_proving', 'ontology',
                       'semantic_web', 'rdf', 'owl', 'sparql', 'description_logic',
                       'first_order_logic', 'predicate', 'axiom', 'entailment', 'satisfiability',
                       'constraint_solving', 'planning', 'search', 'heuristic', 'alpha_beta',
                       'minimax', 'resolution', 'unification', 'substitution', 'skolemization']
    G.add_nodes_from(symbolic_topics, community=1)

    for i in range(len(symbolic_topics) - 1):
        G.add_edge(symbolic_topics[i], symbolic_topics[i + 1])
        if i < len(symbolic_topics) - 2:
            G.add_edge(symbolic_topics[i], symbolic_topics[i + 2])

    # Community 3: Data Management (20 nodes)
    data_topics = ['database', 'sql', 'nosql', 'graph_db', 'neo4j',
                   'rdf_store', 'triple_store', 'index', 'query', 'join',
                   'aggregation', 'transaction', 'acid', 'consistency', 'replication',
                   'sharding', 'partitioning', 'cache', 'storage', 'persistence']
    G.add_nodes_from(data_topics, community=2)

    for i in range(len(data_topics) - 1):
        G.add_edge(data_topics[i], data_topics[i + 1])
        if i < len(data_topics) - 2:
            G.add_edge(data_topics[i], data_topics[i + 2])

    # Add bridge nodes
    bridges = ['knowledge_graph', 'kg', 'entity', 'relation', 'triple', 'graph', 'network']
    G.add_nodes_from(bridges, community='bridge')

    # Connect bridges across communities
    G.add_edges_from([
        # Bridges to ML
        ('knowledge_graph', 'embedding'),
        ('knowledge_graph', 'gnn'),
        ('kg', 'transformer'),
        ('entity', 'embedding'),
        ('triple', 'llm'),

        # Bridges to Symbolic
        ('knowledge_graph', 'reasoning'),
        ('knowledge_graph', 'ontology'),
        ('kg', 'semantic_web'),
        ('triple', 'rdf'),
        ('relation', 'logic'),

        # Bridges to Data
        ('knowledge_graph', 'graph_db'),
        ('kg', 'neo4j'),
        ('triple', 'rdf_store'),
        ('entity', 'database'),
        ('graph', 'storage'),
    ])

    # Calculate layout
    pos = nx.spring_layout(G, k=0.3, iterations=50, seed=42)

    # Create figure
    fig, ax = plt.subplots(figsize=(16, 12))

    # Assign colors
    node_colors = []
    node_sizes = []
    for node in G.nodes():
        comm = G.nodes[node]['community']
        if comm == 0:
            node_colors.append('#4CAF50')  # Green - ML
            node_sizes.append(300)
        elif comm == 1:
            node_colors.append('#2196F3')  # Blue - Symbolic
            node_sizes.append(300)
        elif comm == 2:
            node_colors.append('#FF9800')  # Orange - Data
            node_sizes.append(300)
        else:  # bridge
            node_colors.append('#FFC107')  # Gold - Bridge
            node_sizes.append(800)

    # Draw edges
    nx.draw_networkx_edges(G, pos, ax=ax, width=0.5, alpha=0.3)

    # Draw regular nodes
    regular_nodes = [n for n in G.nodes() if G.nodes[n]['community'] != 'bridge']
    regular_colors = [node_colors[i] for i, n in enumerate(G.nodes()) if n in regular_nodes]
    regular_sizes = [node_sizes[i] for i, n in enumerate(G.nodes()) if n in regular_nodes]
    regular_pos = {n: pos[n] for n in regular_nodes}

    nx.draw_networkx_nodes(G, regular_pos, nodelist=regular_nodes,
                           node_color=regular_colors,
                           node_size=regular_sizes,
                           alpha=0.8,
                           ax=ax)

    # Draw bridge nodes with star markers
    bridge_pos = {n: pos[n] for n in bridges}
    nx.draw_networkx_nodes(G, bridge_pos, nodelist=bridges,
                           node_color='#FFC107',
                           node_size=800,
                           node_shape='*',
                           alpha=1.0,
                           edgecolors='black',
                           linewidths=2,
                           ax=ax)

    # Draw labels for bridges only
    bridge_labels = {n: n.replace('_', '\n') for n in bridges}
    nx.draw_networkx_labels(G, bridge_pos, bridge_labels, ax=ax,
                           font_size=10, font_weight='bold')

    # Add legend
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor='#4CAF50', label='ML/Embeddings (30 nodes)', alpha=0.8),
        Patch(facecolor='#2196F3', label='Symbolic Reasoning (25 nodes)', alpha=0.8),
        Patch(facecolor='#FF9800', label='Data Management (20 nodes)', alpha=0.8),
        Patch(facecolor='#FFC107', label='Bridge Entities (7 nodes)', alpha=1.0, edgecolor='black'),
    ]
    ax.legend(handles=legend_elements, loc='upper left', fontsize=12, framealpha=0.9)

    ax.set_title('Knowledge Graph Community Structure (3 communities, 82 entities, 7 bridges)',
                 fontsize=16, fontweight='bold', pad=20)
    ax.axis('off')
    plt.tight_layout()

    output_path = output_dir / 'community_structure.pdf'
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def generate_figure9_example_subgraph(output_dir):
    """
    Figure 9: Example Knowledge Subgraph
    Detailed view of one specific analogy
    """
    print("Generating Figure 9: Example Subgraph...")

    G = nx.DiGraph()  # Directed graph for relation arrows

    # Left community nodes (ML)
    ml_nodes = {
        'neural_network': (0.1, 0.6),
        'optimization': (0.3, 0.7),
        'gradient_descent': (0.3, 0.5),
        'transformer': (0.1, 0.4),
    }

    # Right community nodes (Symbolic)
    symbolic_nodes = {
        'reasoning': (0.7, 0.6),
        'logic': (0.9, 0.7),
        'inference': (0.9, 0.5),
        'theorem_proving': (0.7, 0.4),
    }

    # Bridge
    bridge = {'knowledge_graph': (0.5, 0.55)}

    # Add nodes with positions
    for node, pos in {**ml_nodes, **symbolic_nodes, **bridge}.items():
        if node in ml_nodes:
            G.add_node(node, pos=pos, community='ML')
        elif node in symbolic_nodes:
            G.add_node(node, pos=pos, community='Symbolic')
        else:
            G.add_node(node, pos=pos, community='bridge')

    # Add directed edges with relation labels
    edges_with_labels = [
        ('neural_network', 'optimization', 'uses'),
        ('optimization', 'gradient_descent', 'employs'),
        ('transformer', 'optimization', 'uses'),
        ('reasoning', 'logic', 'uses'),
        ('logic', 'inference', 'enables'),
        ('inference', 'theorem_proving', 'applies'),
        ('neural_network', 'knowledge_graph', 'represented_in'),
        ('transformer', 'knowledge_graph', 'represented_in'),
        ('knowledge_graph', 'reasoning', 'enables'),
        ('knowledge_graph', 'logic', 'formalizes'),
    ]

    G.add_edges_from([(u, v) for u, v, _ in edges_with_labels])

    # Get positions
    pos = nx.get_node_attributes(G, 'pos')

    # Create figure
    fig, ax = plt.subplots(figsize=(14, 10))

    # Node colors and sizes
    node_colors = []
    node_sizes = []
    for node in G.nodes():
        comm = G.nodes[node]['community']
        if comm == 'ML':
            node_colors.append('#4CAF50')
            node_sizes.append(2500)
        elif comm == 'Symbolic':
            node_colors.append('#2196F3')
            node_sizes.append(2500)
        else:
            node_colors.append('#FFC107')
            node_sizes.append(4000)

    # Draw edges with arrows
    nx.draw_networkx_edges(G, pos, ax=ax,
                           width=2,
                           alpha=0.6,
                           edge_color='#424242',
                           arrows=True,
                           arrowsize=20,
                           arrowstyle='->')

    # Draw nodes
    nx.draw_networkx_nodes(G, pos, ax=ax,
                           node_color=node_colors,
                           node_size=node_sizes,
                           alpha=0.9,
                           edgecolors='black',
                           linewidths=2)

    # Draw node labels
    labels = {node: node.replace('_', '\n') for node in G.nodes()}
    nx.draw_networkx_labels(G, pos, labels, ax=ax,
                           font_size=11,
                           font_weight='bold')

    # Draw edge labels
    edge_labels = {(u, v): label for u, v, label in edges_with_labels}
    nx.draw_networkx_edge_labels(G, pos, edge_labels, ax=ax,
                                 font_size=9,
                                 font_color='red')

    # Add analogy annotation
    ax.annotate('',
                xy=(0.7, 0.6), xytext=(0.3, 0.7),
                arrowprops=dict(arrowstyle='<->', lw=3, color='purple', alpha=0.5))
    ax.text(0.5, 0.75, 'ANALOGY:\nuses → method',
            fontsize=13, fontweight='bold', ha='center',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='purple', alpha=0.2))

    # Community labels
    ax.text(0.15, 0.85, 'ML Domain',
            fontsize=14, fontweight='bold',
            bbox=dict(boxstyle='round', facecolor='#4CAF50', alpha=0.3))
    ax.text(0.80, 0.85, 'Symbolic AI Domain',
            fontsize=14, fontweight='bold',
            bbox=dict(boxstyle='round', facecolor='#2196F3', alpha=0.3))

    # Hypothesis box
    hypothesis_text = (
        "Hypothesis: Gradient-based optimization methods\n"
        "may be applicable to symbolic reasoning tasks\n"
        "via differentiable knowledge graph representations"
    )
    ax.text(0.5, 0.15, hypothesis_text,
            fontsize=11, ha='center', style='italic',
            bbox=dict(boxstyle='round,pad=0.7', facecolor='yellow', alpha=0.4))

    ax.set_title('Example: Bridge-Mediated Analogy Between Neural Network Optimization and Symbolic Reasoning',
                 fontsize=15, fontweight='bold', pad=20)
    ax.set_xlim([-0.05, 1.05])
    ax.set_ylim([0.05, 0.95])
    ax.axis('off')
    plt.tight_layout()

    output_path = output_dir / 'example_subgraph.pdf'
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved to {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(description='Generate network figures for publication')
    parser.add_argument('--output-dir', default='figures',
                        help='Output directory for figures (default: figures)')
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True, parents=True)
    print(f"\nGenerating network figures in: {output_dir.absolute()}\n")

    setup_plot_style()

    try:
        generate_figure2_bridge_analogy_example(output_dir)
        generate_figure5_community_structure(output_dir)
        generate_figure9_example_subgraph(output_dir)

        print(f"\n✅ Successfully generated 3 network figures!")
        print(f"\nRemaining figures to create:")
        print("  • Figure 1: System Architecture (use Draw.io or TikZ)")
        print("  • Figure 4: Pattern Library Structure (use Draw.io)")
        print(f"\nOr run: python generate_figures.py --output-dir {args.output_dir}")
        print("  to generate figures 3, 6, 7, 8")

    except Exception as e:
        print(f"\n❌ Error generating network figures: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == '__main__':
    exit(main())

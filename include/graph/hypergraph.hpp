#ifndef HYPERGRAPH_HPP
#define HYPERGRAPH_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace kg {

/**
 * @brief Represents a node in the hypergraph
 *
 * Nodes are entities extracted from the corpus. Each node can participate
 * in multiple hyperedges and maintains references to all its incident edges.
 */
struct HyperNode {
    std::string id;                                    // Unique identifier
    std::string label;                                 // Human-readable label
    std::map<std::string, std::string> properties;     // Additional metadata
    std::vector<std::string> incident_edges;           // IDs of hyperedges containing this node
    int degree = 0;                                    // Number of incident hyperedges

    // Optional: for embedding-based deduplication
    std::vector<float> embedding;

    /**
     * @brief Convert node to JSON representation
     */
    nlohmann::json to_json() const;

    /**
     * @brief Create node from JSON
     */
    static HyperNode from_json(const nlohmann::json& j);
};

/**
 * @brief Represents a directed hyperedge connecting multiple source nodes to multiple target nodes
 *
 * Hyperedges represent higher-order relationships where multiple entities
 * jointly interact with multiple other entities through a named relation.
 * Example: [chitosan, PCL] --"compose"--> [nanofiber, scaffold]
 */
struct HyperEdge {
    std::string id;                                    // Unique identifier
    std::vector<std::string> sources;                  // Source node IDs
    std::string relation;                              // Relation type/name
    std::vector<std::string> targets;                  // Target node IDs
    std::map<std::string, std::string> properties;     // Additional metadata

    // Provenance information
    std::string source_document;                       // Document this edge came from
    std::string source_chunk_id;                       // Chunk ID within document
    int source_page = -1;                              // Page number (if applicable)

    double confidence = 1.0;                           // Confidence score [0, 1]

    /**
     * @brief Get all nodes involved in this hyperedge (sources + targets)
     */
    std::set<std::string> get_all_nodes() const;

    /**
     * @brief Get the size of this hyperedge (number of nodes)
     */
    size_t size() const { return sources.size() + targets.size(); }

    /**
     * @brief Check if this hyperedge contains a specific node
     */
    bool contains_node(const std::string& node_id) const;

    /**
     * @brief Compute intersection with another hyperedge
     * @return Set of node IDs that appear in both hyperedges
     */
    std::set<std::string> intersection(const HyperEdge& other) const;

    /**
     * @brief Check if this is a self-loop (sources and targets are identical)
     */
    bool is_self_loop() const;

    /**
     * @brief Convert hyperedge to JSON representation
     */
    nlohmann::json to_json() const;

    /**
     * @brief Create hyperedge from JSON
     */
    static HyperEdge from_json(const nlohmann::json& j);
};

/**
 * @brief Statistics about the hypergraph structure
 */
struct HypergraphStatistics {
    size_t num_nodes = 0;
    size_t num_edges = 0;
    size_t num_duplicate_edges = 0;

    double avg_edge_size = 0.0;
    size_t max_edge_size = 0;
    size_t min_edge_size = 0;

    double avg_node_degree = 0.0;
    size_t max_node_degree = 0;
    size_t min_node_degree = 0;

    size_t max_edge_intersection = 0;
    size_t num_pairs_overlap_1 = 0;
    size_t num_pairs_overlap_2 = 0;
    size_t num_pairs_overlap_3 = 0;

    // Power law fit for degree distribution (if applicable)
    std::optional<double> power_law_exponent;
    std::optional<double> power_law_r_squared;

    nlohmann::json to_json() const;
};

/**
 * @brief Result of a path search query
 */
struct PathSearchResult {
    std::vector<std::vector<HyperEdge>> paths;         // List of paths (each path is a sequence of hyperedges)
    std::vector<std::string> intersection_nodes;       // Nodes used for intersection at each step
    bool found = false;
    size_t num_paths_explored = 0;

    nlohmann::json to_json() const;
};

/**
 * @brief Main Hypergraph class implementing higher-order knowledge representation
 *
 * This class implements the hypergraph data structure as described in the paper:
 * "Higher-Order Knowledge Representations for Agentic Scientific Reasoning"
 *
 * Key features:
 * - Directed hyperedges with multiple sources and targets
 * - Efficient node-to-edge indexing for fast traversal
 * - s-connected path finding (paths where adjacent edges share ≥s nodes)
 * - k-shortest path algorithms (Yen's algorithm adapted for hypergraphs)
 * - Node deduplication using embedding similarity
 * - Multiple export formats (JSON, DOT, incidence matrix)
 */
class Hypergraph {
public:
    Hypergraph() = default;

    // ==========================================
    // Node and Edge Management
    // ==========================================

    /**
     * @brief Add a hyperedge to the graph
     * @param edge The hyperedge to add
     * @return ID of the added edge
     *
     * This will automatically create nodes if they don't exist
     */
    std::string add_hyperedge(const HyperEdge& edge);

    /**
     * @brief Add a hyperedge with source/target lists
     */
    std::string add_hyperedge(
        const std::vector<std::string>& sources,
        const std::string& relation,
        const std::vector<std::string>& targets,
        const std::string& source_chunk_id = ""
    );

    /**
     * @brief Add or update a node
     */
    void add_node(const HyperNode& node);

    /**
     * @brief Remove a hyperedge by ID
     */
    bool remove_hyperedge(const std::string& edge_id);

    /**
     * @brief Remove a node and all incident edges
     */
    bool remove_node(const std::string& node_id);

    /**
     * @brief Get a node by ID
     */
    const HyperNode* get_node(const std::string& node_id) const;
    HyperNode* get_node(const std::string& node_id);

    /**
     * @brief Get a hyperedge by ID
     */
    const HyperEdge* get_hyperedge(const std::string& edge_id) const;
    HyperEdge* get_hyperedge(const std::string& edge_id);

    /**
     * @brief Get all hyperedges incident to a node
     */
    std::vector<HyperEdge> get_incident_edges(const std::string& node_id) const;

    /**
     * @brief Get all nodes in the graph
     */
    std::vector<HyperNode> get_all_nodes() const;

    /**
     * @brief Get all hyperedges in the graph
     */
    std::vector<HyperEdge> get_all_edges() const;

    /**
     * @brief Check if a node exists
     */
    bool has_node(const std::string& node_id) const;

    /**
     * @brief Check if an edge exists
     */
    bool has_edge(const std::string& edge_id) const;

    // ==========================================
    // Graph Operations
    // ==========================================

    /**
     * @brief Merge similar nodes based on embedding similarity
     * @param similarity_threshold Cosine similarity threshold [0, 1]
     * @param merge_frequency How often to perform merging (every N documents)
     *
     * Implements the node merging algorithm from the paper (Algorithm 1, lines 19-35)
     */
    void merge_similar_nodes(double similarity_threshold = 0.95);

    /**
     * @brief Remove self-loop hyperedges where source == target
     */
    size_t remove_self_loops();

    /**
     * @brief Find duplicate hyperedges
     * @return Map of canonical edge ID to list of duplicate edge IDs
     */
    std::map<std::string, std::vector<std::string>> find_duplicate_edges() const;

    /**
     * @brief Merge duplicate hyperedges
     * @return Number of edges removed
     */
    size_t merge_duplicate_edges();

    /**
     * @brief Get the degree of a node (number of incident hyperedges)
     */
    int get_node_degree(const std::string& node_id) const;

    /**
     * @brief Compute degree for all nodes
     */
    std::map<std::string, int> compute_node_degrees() const;

    /**
     * @brief Get the k highest-degree nodes (hubs)
     */
    std::vector<std::pair<std::string, int>> get_top_hubs(size_t k = 20) const;

    /**
     * @brief Compute statistics about the hypergraph
     */
    HypergraphStatistics compute_statistics() const;

    // ==========================================
    // Path Finding Algorithms
    // ==========================================

    /**
     * @brief Find shortest s-connected path between two nodes
     * @param start Starting node ID
     * @param end Ending node ID
     * @param min_intersection_size Minimum number of shared nodes (s parameter)
     * @return Path as sequence of hyperedges, or empty if no path exists
     *
     * Uses BFS to find shortest path where adjacent hyperedges share ≥s nodes.
     * Implements the traversal algorithm from section 4.4 of the paper.
     */
    std::vector<HyperEdge> find_shortest_path(
        const std::string& start,
        const std::string& end,
        int min_intersection_size = 1
    ) const;

    /**
     * @brief Find k shortest s-connected paths using Yen's algorithm
     * @param start Starting node ID
     * @param end Ending node ID
     * @param k Number of paths to find
     * @param min_intersection_size Minimum number of shared nodes (s parameter)
     * @return PathSearchResult containing up to k shortest paths
     *
     * Adapts Yen's k-shortest paths algorithm to hypergraphs.
     */
    PathSearchResult find_k_shortest_paths(
        const std::string& start,
        const std::string& end,
        int k = 3,
        int min_intersection_size = 1
    ) const;

    /**
     * @brief Find all hyperedges that form s-connected components
     * @param min_intersection_size Minimum intersection size (s parameter)
     * @return List of connected components (each is a set of edge IDs)
     *
     * Implements s-connected component extraction as described in Table 4 of the paper.
     */
    std::vector<std::set<std::string>> find_s_connected_components(
        int min_intersection_size = 1
    ) const;

    /**
     * @brief Get the h-hop neighborhood of a node
     * @param node_id Starting node
     * @param hops Number of hops
     * @param min_intersection_size Minimum intersection for connectivity
     * @return Set of node IDs reachable within h hops
     */
    std::set<std::string> get_neighborhood(
        const std::string& node_id,
        int hops = 1,
        int min_intersection_size = 1
    ) const;

    /**
     * @brief Extract induced subhypergraph containing specified nodes
     * @param node_ids Set of node IDs to include
     * @return New Hypergraph containing only hyperedges within the node set
     */
    Hypergraph extract_subgraph(const std::set<std::string>& node_ids) const;

    // ==========================================
    // Graph Projections
    // ==========================================

    /**
     * @brief Project to clique expansion (pairwise graph)
     *
     * Each hyperedge is replaced by a complete graph (clique) over its nodes.
     * This creates a traditional pairwise graph representation.
     */
    // StandardGraph to_clique_expansion() const;  // Forward declaration, implement later

    /**
     * @brief Project to star expansion (bipartite graph)
     *
     * Hyperedges become nodes, original nodes connect to hyperedge nodes.
     */
    // StandardGraph to_star_expansion() const;  // Forward declaration, implement later

    // ==========================================
    // Analysis and Metrics
    // ==========================================

    /**
     * @brief Compute degree distribution
     * @return Map of degree -> frequency
     */
    std::map<int, int> compute_degree_distribution() const;

    /**
     * @brief Fit power law to degree distribution
     * @return Exponent and R² value
     */
    std::pair<double, double> fit_power_law() const;

    /**
     * @brief Compute rich-club coefficient at degree threshold
     * @param degree_threshold Minimum degree for rich club membership
     * @return Rich club coefficient
     *
     * Measures tendency of high-degree nodes to connect to each other.
     */
    double compute_rich_club_coefficient(int degree_threshold) const;

    /**
     * @brief Compute hub integration scores
     * @return Map of node ID to integration score (co-occurrences with other hubs)
     */
    std::map<std::string, int> compute_hub_integration_scores(int top_k_hubs = 20) const;

    // ==========================================
    // Import/Export
    // ==========================================

    /**
     * @brief Export hypergraph to JSON format
     * @param include_metadata Include provenance and properties
     */
    nlohmann::json to_json(bool include_metadata = true) const;

    /**
     * @brief Export to JSON file
     */
    void export_to_json(const std::string& filename, bool include_metadata = true) const;

    /**
     * @brief Export to Graphviz DOT format for visualization
     *
     * Hyperedges are represented as diamond-shaped relation nodes
     * connecting source nodes to target nodes.
     */
    void export_to_dot(const std::string& filename) const;

    /**
     * @brief Export incidence matrix (nodes × hyperedges)
     * @return JSON representation of the incidence matrix
     */
    nlohmann::json to_incidence_matrix() const;

    /**
     * @brief Export to interactive HTML visualization
     *
     * Creates a self-contained HTML file with D3.js-based force-directed
     * graph visualization. Hyperedges are shown as colored rectangles
     * connecting source and target nodes.
     *
     * @param filename Output HTML file path
     * @param title Optional title for the visualization
     */
    void export_to_html(const std::string& filename,
                        const std::string& title = "Hypergraph Visualization") const;

    /**
     * @brief Load hypergraph from JSON
     */
    static Hypergraph from_json(const nlohmann::json& j);

    /**
     * @brief Load hypergraph from JSON file
     */
    static Hypergraph load_from_json(const std::string& filename);

    // ==========================================
    // Merge Operations
    // ==========================================

    /**
     * @brief Merge another hypergraph into this one
     * @param other The hypergraph to merge
     * @param deduplicate Whether to deduplicate nodes and edges after merge
     *
     * Implements the incremental merge operation from Algorithm 1 (lines 14-17)
     */
    void merge(const Hypergraph& other, bool deduplicate = true);

    // ==========================================
    // Utility Methods
    // ==========================================

    /**
     * @brief Get total number of nodes
     */
    size_t num_nodes() const { return nodes_.size(); }

    /**
     * @brief Get total number of hyperedges
     */
    size_t num_edges() const { return hyperedges_.size(); }

    /**
     * @brief Check if hypergraph is empty
     */
    bool empty() const { return nodes_.empty() && hyperedges_.empty(); }

    /**
     * @brief Clear all nodes and edges
     */
    void clear();

    /**
     * @brief Generate a unique edge ID
     */
    static std::string generate_edge_id();

    /**
     * @brief Compute cosine similarity between two embedding vectors
     */
    static double cosine_similarity(
        const std::vector<float>& vec1,
        const std::vector<float>& vec2
    );

private:
    // ==========================================
    // Internal Data Structures
    // ==========================================

    std::map<std::string, HyperNode> nodes_;           // node_id -> node
    std::map<std::string, HyperEdge> hyperedges_;      // edge_id -> hyperedge
    std::map<std::string, std::vector<std::string>> node_to_edges_;  // node_id -> [edge_ids]

    // Counter for generating unique IDs
    static inline size_t edge_id_counter_ = 0;

    // ==========================================
    // Internal Helper Methods
    // ==========================================

    /**
     * @brief Update internal indices after adding an edge
     */
    void update_indices(const HyperEdge& edge);

    /**
     * @brief Remove edge from internal indices
     */
    void remove_from_indices(const std::string& edge_id);

    /**
     * @brief Check if two hyperedges are duplicates
     */
    bool are_duplicate_edges(const HyperEdge& e1, const HyperEdge& e2) const;

    /**
     * @brief BFS helper for path finding
     */
    std::vector<HyperEdge> bfs_path_search(
        const std::string& start,
        const std::string& end,
        int min_intersection_size,
        const std::set<std::string>& excluded_edges = {}
    ) const;

    /**
     * @brief Check if two hyperedges share at least s nodes
     */
    bool are_s_connected(
        const HyperEdge& e1,
        const HyperEdge& e2,
        int min_intersection_size
    ) const;

    /**
     * @brief Get all hyperedges that are s-connected to a given edge
     */
    std::vector<std::string> get_s_connected_neighbors(
        const std::string& edge_id,
        int min_intersection_size
    ) const;

    /**
     * @brief Merge two nodes (used in deduplication)
     */
    void merge_nodes(const std::string& keep_id, const std::string& remove_id);

    /**
     * @brief Find connected components in similarity graph
     */
    std::vector<std::vector<std::string>> find_similarity_components(
        const std::map<std::string, std::vector<std::string>>& similarity_graph
    ) const;
};

} // namespace kg

#endif // HYPERGRAPH_HPP

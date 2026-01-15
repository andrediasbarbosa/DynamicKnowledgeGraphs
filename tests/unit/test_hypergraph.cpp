#include <gtest/gtest.h>
#include "graph/hypergraph.hpp"

using namespace kg;

class HypergraphTest : public ::testing::Test {
protected:
    Hypergraph graph;

    void SetUp() override {
        // Create a simple test graph
        graph.add_hyperedge({"A", "B"}, "rel1", {"C"});
        graph.add_hyperedge({"C"}, "rel2", {"D", "E"});
        graph.add_hyperedge({"B", "E"}, "rel3", {"F"});
    }
};

// ==========================================
// Basic Operations Tests
// ==========================================

TEST_F(HypergraphTest, AddHyperedge) {
    EXPECT_EQ(graph.num_edges(), 3);
    EXPECT_EQ(graph.num_nodes(), 6);  // A, B, C, D, E, F
}

TEST_F(HypergraphTest, NodeExistence) {
    EXPECT_TRUE(graph.has_node("A"));
    EXPECT_TRUE(graph.has_node("F"));
    EXPECT_FALSE(graph.has_node("Z"));
}

TEST_F(HypergraphTest, GetNode) {
    const auto* node = graph.get_node("A");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->id, "A");
    EXPECT_EQ(node->label, "A");
}

TEST_F(HypergraphTest, RemoveHyperedge) {
    auto edges = graph.get_all_edges();
    ASSERT_FALSE(edges.empty());

    std::string edge_id = edges[0].id;
    EXPECT_TRUE(graph.remove_hyperedge(edge_id));
    EXPECT_EQ(graph.num_edges(), 2);
    EXPECT_FALSE(graph.has_edge(edge_id));
}

TEST_F(HypergraphTest, RemoveNode) {
    EXPECT_TRUE(graph.remove_node("C"));
    EXPECT_FALSE(graph.has_node("C"));

    // Node C was involved in 2 edges, so they should be removed
    EXPECT_LT(graph.num_edges(), 3);
}

// ==========================================
// Degree Tests
// ==========================================

TEST_F(HypergraphTest, NodeDegree) {
    // Node C appears in 2 edges
    int degree_c = graph.get_node_degree("C");
    EXPECT_EQ(degree_c, 2);

    // Node A appears in 1 edge
    int degree_a = graph.get_node_degree("A");
    EXPECT_EQ(degree_a, 1);
}

TEST_F(HypergraphTest, ComputeDegrees) {
    auto degrees = graph.compute_node_degrees();
    EXPECT_EQ(degrees.size(), 6);
    EXPECT_GT(degrees["C"], 0);
}

TEST_F(HypergraphTest, GetTopHubs) {
    auto hubs = graph.get_top_hubs(3);
    EXPECT_LE(hubs.size(), 3);

    if (!hubs.empty()) {
        // Hubs should be sorted by degree descending
        for (size_t i = 1; i < hubs.size(); ++i) {
            EXPECT_GE(hubs[i-1].second, hubs[i].second);
        }
    }
}

// ==========================================
// Path Finding Tests
// ==========================================

TEST_F(HypergraphTest, ShortestPathExists) {
    auto path = graph.find_shortest_path("A", "F", 1);
    EXPECT_FALSE(path.empty());
}

TEST_F(HypergraphTest, ShortestPathNonExistent) {
    auto path = graph.find_shortest_path("A", "NonExistent", 1);
    EXPECT_TRUE(path.empty());
}

TEST_F(HypergraphTest, KShortestPaths) {
    auto result = graph.find_k_shortest_paths("A", "F", 3, 1);

    if (result.found) {
        EXPECT_GT(result.paths.size(), 0);
        EXPECT_LE(result.paths.size(), 3);
    }
}

TEST_F(HypergraphTest, Neighborhood) {
    auto neighbors = graph.get_neighborhood("A", 1, 1);
    EXPECT_GT(neighbors.size(), 0);

    // A should not be in its own neighborhood
    EXPECT_EQ(neighbors.count("A"), 0);
}

// ==========================================
// HyperEdge Tests
// ==========================================

TEST(HyperEdgeTest, GetAllNodes) {
    HyperEdge edge;
    edge.sources = {"A", "B"};
    edge.targets = {"C", "D"};

    auto all_nodes = edge.get_all_nodes();
    EXPECT_EQ(all_nodes.size(), 4);
    EXPECT_EQ(all_nodes.count("A"), 1);
    EXPECT_EQ(all_nodes.count("D"), 1);
}

TEST(HyperEdgeTest, ContainsNode) {
    HyperEdge edge;
    edge.sources = {"A", "B"};
    edge.targets = {"C"};

    EXPECT_TRUE(edge.contains_node("A"));
    EXPECT_TRUE(edge.contains_node("C"));
    EXPECT_FALSE(edge.contains_node("Z"));
}

TEST(HyperEdgeTest, Intersection) {
    HyperEdge edge1;
    edge1.sources = {"A", "B"};
    edge1.targets = {"C"};

    HyperEdge edge2;
    edge2.sources = {"B", "C"};
    edge2.targets = {"D"};

    auto intersection = edge1.intersection(edge2);
    EXPECT_EQ(intersection.size(), 2);  // B and C
    EXPECT_EQ(intersection.count("B"), 1);
    EXPECT_EQ(intersection.count("C"), 1);
}

TEST(HyperEdgeTest, SelfLoop) {
    HyperEdge edge1;
    edge1.sources = {"A", "B"};
    edge1.targets = {"A", "B"};
    EXPECT_TRUE(edge1.is_self_loop());

    HyperEdge edge2;
    edge2.sources = {"A"};
    edge2.targets = {"B"};
    EXPECT_FALSE(edge2.is_self_loop());
}

// ==========================================
// Graph Operations Tests
// ==========================================

TEST_F(HypergraphTest, RemoveSelfLoops) {
    graph.add_hyperedge({"X"}, "rel", {"X"});
    EXPECT_GT(graph.num_edges(), 3);

    size_t removed = graph.remove_self_loops();
    EXPECT_EQ(removed, 1);
}

TEST_F(HypergraphTest, FindDuplicates) {
    // Add duplicate edge
    graph.add_hyperedge({"A", "B"}, "rel1", {"C"});

    auto duplicates = graph.find_duplicate_edges();
    EXPECT_GT(duplicates.size(), 0);
}

TEST_F(HypergraphTest, MergeDuplicates) {
    size_t initial_count = graph.num_edges();
    graph.add_hyperedge({"A", "B"}, "rel1", {"C"});

    size_t removed = graph.merge_duplicate_edges();
    EXPECT_GT(removed, 0);
    EXPECT_LT(graph.num_edges(), initial_count + 1);
}

// ==========================================
// Statistics Tests
// ==========================================

TEST_F(HypergraphTest, ComputeStatistics) {
    auto stats = graph.compute_statistics();

    EXPECT_EQ(stats.num_nodes, 6);
    EXPECT_EQ(stats.num_edges, 3);
    EXPECT_GT(stats.avg_edge_size, 0.0);
    EXPECT_GT(stats.avg_node_degree, 0.0);
}

TEST_F(HypergraphTest, DegreeDistribution) {
    auto dist = graph.compute_degree_distribution();
    EXPECT_GT(dist.size(), 0);

    int total_nodes = 0;
    for (const auto& [degree, count] : dist) {
        total_nodes += count;
    }

    EXPECT_EQ(total_nodes, graph.num_nodes());
}

// ==========================================
// Connected Components Tests
// ==========================================

TEST_F(HypergraphTest, ConnectedComponents) {
    auto components = graph.find_s_connected_components(1);
    EXPECT_GT(components.size(), 0);

    // Sum of component sizes should equal total edges
    size_t total_edges = 0;
    for (const auto& comp : components) {
        total_edges += comp.size();
    }
    EXPECT_EQ(total_edges, graph.num_edges());
}

// ==========================================
// Subgraph Tests
// ==========================================

TEST_F(HypergraphTest, ExtractSubgraph) {
    std::set<std::string> nodes = {"A", "B", "C"};
    auto subgraph = graph.extract_subgraph(nodes);

    // Subgraph should only contain specified nodes
    for (const auto& node : subgraph.get_all_nodes()) {
        EXPECT_EQ(nodes.count(node.id), 1);
    }

    // Should have fewer edges (only edges with all nodes in set)
    EXPECT_LE(subgraph.num_edges(), graph.num_edges());
}

// ==========================================
// Merge Tests
// ==========================================

TEST_F(HypergraphTest, MergeGraphs) {
    Hypergraph graph2;
    graph2.add_hyperedge({"X"}, "rel", {"Y"});

    size_t initial_nodes = graph.num_nodes();
    size_t initial_edges = graph.num_edges();

    graph.merge(graph2, false);

    EXPECT_GT(graph.num_nodes(), initial_nodes);
    EXPECT_GT(graph.num_edges(), initial_edges);
    EXPECT_TRUE(graph.has_node("X"));
    EXPECT_TRUE(graph.has_node("Y"));
}

// ==========================================
// Export/Import Tests
// ==========================================

TEST_F(HypergraphTest, ExportToJSON) {
    auto json = graph.to_json();

    EXPECT_TRUE(json.contains("nodes"));
    EXPECT_TRUE(json.contains("hyperedges"));
    EXPECT_TRUE(json.contains("metadata"));

    EXPECT_EQ(json["nodes"].size(), graph.num_nodes());
    EXPECT_EQ(json["hyperedges"].size(), graph.num_edges());
}

TEST_F(HypergraphTest, ExportImportRoundtrip) {
    // Export
    auto json = graph.to_json();

    // Import
    auto loaded = Hypergraph::from_json(json);

    // Should be identical
    EXPECT_EQ(loaded.num_nodes(), graph.num_nodes());
    EXPECT_EQ(loaded.num_edges(), graph.num_edges());
}

TEST_F(HypergraphTest, IncidenceMatrix) {
    auto matrix = graph.to_incidence_matrix();

    EXPECT_TRUE(matrix.contains("nodes"));
    EXPECT_TRUE(matrix.contains("edges"));
    EXPECT_TRUE(matrix.contains("matrix"));

    EXPECT_EQ(matrix["nodes"].size(), graph.num_nodes());
    EXPECT_EQ(matrix["edges"].size(), graph.num_edges());
}

// ==========================================
// Utility Tests
// ==========================================

TEST(UtilityTest, CosineSimilarity) {
    std::vector<float> vec1 = {1.0, 0.0, 0.0};
    std::vector<float> vec2 = {1.0, 0.0, 0.0};
    std::vector<float> vec3 = {0.0, 1.0, 0.0};

    double sim_identical = Hypergraph::cosine_similarity(vec1, vec2);
    EXPECT_NEAR(sim_identical, 1.0, 1e-6);

    double sim_orthogonal = Hypergraph::cosine_similarity(vec1, vec3);
    EXPECT_NEAR(sim_orthogonal, 0.0, 1e-6);
}

TEST(UtilityTest, GenerateEdgeID) {
    std::string id1 = Hypergraph::generate_edge_id();
    std::string id2 = Hypergraph::generate_edge_id();

    EXPECT_NE(id1, id2);
    EXPECT_TRUE(id1.find("edge_") == 0);
}

// ==========================================
// Edge Cases
// ==========================================

TEST(EdgeCaseTest, EmptyGraph) {
    Hypergraph empty;

    EXPECT_EQ(empty.num_nodes(), 0);
    EXPECT_EQ(empty.num_edges(), 0);
    EXPECT_TRUE(empty.empty());
}

TEST(EdgeCaseTest, SingleNode) {
    Hypergraph graph;
    graph.add_hyperedge({"A"}, "rel", {"B"});

    EXPECT_EQ(graph.num_nodes(), 2);
    EXPECT_EQ(graph.num_edges(), 1);
}

TEST(EdgeCaseTest, LargeHyperedge) {
    Hypergraph graph;

    std::vector<std::string> sources;
    for (int i = 0; i < 10; ++i) {
        sources.push_back("S" + std::to_string(i));
    }

    std::vector<std::string> targets;
    for (int i = 0; i < 10; ++i) {
        targets.push_back("T" + std::to_string(i));
    }

    graph.add_hyperedge(sources, "large_rel", targets);

    EXPECT_EQ(graph.num_nodes(), 20);
    EXPECT_EQ(graph.num_edges(), 1);

    auto edges = graph.get_all_edges();
    EXPECT_EQ(edges[0].size(), 20);
}

// ==========================================
// Main
// ==========================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

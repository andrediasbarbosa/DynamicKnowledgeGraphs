/**
 * Graph-RAG Multi-Hop Reasoning Agent
 * Implements ReAct-style (Reasoning + Acting) iterative graph navigation
 *
 * Usage: Replace handleQuery() in graph_rag.html with handleQueryMultiHop()
 */

// ═══════════════════════════════════════════════════════════════════
// GRAPH OPERATIONS
// ═══════════════════════════════════════════════════════════════════

class GraphOperations {
    constructor(graphData, insights) {
        this.data = graphData;
        this.insights = insights;
        this.nodeMap = new Map();
        this.data.nodes.forEach(n => this.nodeMap.set(n.gid, n));
    }

    /**
     * SEARCH: Find nodes by keywords (with centrality-based ranking)
     */
    search(keywords, maxResults = 10) {
        const kws = keywords.toLowerCase().split(/\s+/).filter(w => w.length > 2);
        const scores = new Map();

        this.data.nodes.forEach(n => {
            const label = (n.label || '').toLowerCase();
            const props = n.properties ?
                Object.values(n.properties).join(' ').toLowerCase() : '';

            let score = 0;
            kws.forEach(kw => {
                if (label === kw) score += 5;
                else if (label.startsWith(kw)) score += 3;
                else if (label.includes(kw)) score += 2;
                if (props.includes(kw)) score += 1;
            });

            if (score > 0) {
                // Boost by centrality (degree-based importance)
                const degree = this.getNeighborIds(n.gid).length;
                const centralityBoost = 1 + Math.log(degree + 1) * 0.2;
                score *= centralityBoost;

                scores.set(n.gid, score);
            }
        });

        return Array.from(scores.entries())
            .sort((a, b) => b[1] - a[1])
            .slice(0, maxResults)
            .map(([gid, score]) => {
                const degree = this.getNeighborIds(gid).length;
                return {
                    gid,
                    label: this.nodeMap.get(gid).label,
                    score: score.toFixed(2),
                    degree,
                    node: this.nodeMap.get(gid)
                };
            });
    }

    /**
     * EXPAND: Get neighbors of a node
     */
    expand(nodeId, maxNeighbors = 10) {
        const node = this.nodeMap.get(nodeId);
        if (!node) return { error: `Node ${nodeId} not found` };

        const neighbors = [];

        // Find edges where this node is source or target
        this.data.links.forEach(link => {
            if (link.source === nodeId || link.source?.gid === nodeId) {
                const targetId = typeof link.target === 'object' ? link.target.gid : link.target;
                const target = this.nodeMap.get(targetId);
                if (target) {
                    neighbors.push({
                        direction: 'outgoing',
                        relation: link.relation || 'related_to',
                        neighbor: target.label,
                        neighbor_id: targetId
                    });
                }
            }

            if (link.target === nodeId || link.target?.gid === nodeId) {
                const sourceId = typeof link.source === 'object' ? link.source.gid : link.source;
                const source = this.nodeMap.get(sourceId);
                if (source) {
                    neighbors.push({
                        direction: 'incoming',
                        relation: link.relation || 'related_to',
                        neighbor: source.label,
                        neighbor_id: sourceId
                    });
                }
            }
        });

        // Sort by importance (degree, type, etc.)
        const sorted = neighbors.slice(0, maxNeighbors);

        return {
            node: node.label,
            degree: neighbors.length,
            neighbors: sorted
        };
    }

    /**
     * PATH: Find shortest path between two nodes
     */
    findPath(startId, endId, maxDepth = 5) {
        if (!this.nodeMap.has(startId) || !this.nodeMap.has(endId)) {
            return { error: 'One or both nodes not found' };
        }

        // BFS to find shortest path
        const queue = [[startId]];
        const visited = new Set([startId]);

        while (queue.length > 0) {
            const path = queue.shift();
            const current = path[path.length - 1];

            if (current === endId) {
                // Found path! Format it
                return {
                    found: true,
                    length: path.length - 1,
                    path: path.map(id => this.nodeMap.get(id).label),
                    path_ids: path,
                    edges: this.getEdgesInPath(path)
                };
            }

            if (path.length >= maxDepth + 1) continue;

            // Explore neighbors
            const neighbors = this.getNeighborIds(current);
            for (const neighbor of neighbors) {
                if (!visited.has(neighbor)) {
                    visited.add(neighbor);
                    queue.push([...path, neighbor]);
                }
            }
        }

        return { found: false, message: `No path found within ${maxDepth} hops` };
    }

    /**
     * INSIGHTS: Get relevant insights
     */
    getInsights(nodeIds, maxInsights = 5) {
        if (!this.insights || this.insights.length === 0) {
            return { insights: [] };
        }

        const relevant = this.insights.filter(insight => {
            const seedNodes = insight.seed_nodes || [];
            return seedNodes.some(seed => nodeIds.includes(seed));
        });

        return {
            count: relevant.length,
            insights: relevant.slice(0, maxInsights).map(i => ({
                type: i.type,
                title: i.title || i.type,
                description: i.description,
                nodes: i.seed_nodes
            }))
        };
    }

    /**
     * SUMMARY: Get node details
     */
    getNodeDetails(nodeId) {
        const node = this.nodeMap.get(nodeId);
        if (!node) return { error: 'Node not found' };

        return {
            label: node.label,
            type: node.type,
            properties: node.properties || {},
            degree: this.getNeighborIds(nodeId).length
        };
    }

    // Helper methods
    getNeighborIds(nodeId) {
        const neighbors = new Set();

        this.data.links.forEach(link => {
            const sourceId = typeof link.source === 'object' ? link.source.gid : link.source;
            const targetId = typeof link.target === 'object' ? link.target.gid : link.target;

            if (sourceId === nodeId) neighbors.add(targetId);
            if (targetId === nodeId) neighbors.add(sourceId);
        });

        return Array.from(neighbors);
    }

    getEdgesInPath(path) {
        const edges = [];
        for (let i = 0; i < path.length - 1; i++) {
            const from = path[i];
            const to = path[i + 1];

            const link = this.data.links.find(l => {
                const sourceId = typeof l.source === 'object' ? l.source.gid : l.source;
                const targetId = typeof l.target === 'object' ? l.target.gid : l.target;
                return (sourceId === from && targetId === to) ||
                       (sourceId === to && targetId === from);
            });

            edges.push({
                from: this.nodeMap.get(from).label,
                to: this.nodeMap.get(to).label,
                relation: link?.relation || 'related_to'
            });
        }
        return edges;
    }

    /**
     * BRIDGE: Find bridge nodes connecting different domains/components
     * Queries insights data for nodes that bridge disconnected subgraphs
     */
    findBridges(keywords = '') {
        // Query insights for bridge nodes
        if (!this.insights || this.insights.length === 0) {
            return { found: 0, bridges: [], message: 'No insights data available' };
        }

        let bridges = this.insights.filter(i => i.type === 'bridge');

        // If keywords provided, filter bridges by relevance
        if (keywords) {
            const kws = keywords.toLowerCase().split(/\s+/).filter(w => w.length > 2);
            bridges = bridges.filter(b => {
                const searchText = (
                    (b.seed_labels || []).join(' ') + ' ' +
                    (b.description || '') + ' ' +
                    (b.seed_nodes || []).join(' ')
                ).toLowerCase();
                return kws.some(kw => searchText.includes(kw));
            });
        }

        // Sort by novelty score
        bridges.sort((a, b) => (b.score || 0) - (a.score || 0));

        return {
            found: bridges.length,
            bridges: bridges.slice(0, 10).map(b => ({
                bridge_node: (b.seed_labels || [])[0] || (b.seed_nodes || [])[0],
                description: b.description,
                novelty_score: (b.score || 0).toFixed(3),
                connects_components: b.novelty_tags || [],
                source_documents: (b.source_documents || []).slice(0, 3),
                evidence_count: (b.evidence_chunk_ids || []).length
            }))
        };
    }
}

// ═══════════════════════════════════════════════════════════════════
// REACT AGENT
// ═══════════════════════════════════════════════════════════════════

class GraphReActAgent {
    constructor(graphOps, llmCaller, options = {}) {
        this.ops = graphOps;
        this.callLLM = llmCaller;
        this.maxSteps = options.maxSteps || 6;
        this.verbose = options.verbose || false;
        this.trace = [];
    }

    /**
     * Execute query with multi-hop reasoning
     */
    async query(question) {
        this.trace = [];

        const systemPrompt = this.getSystemPrompt();
        let context = [];
        let finalAnswer = null;

        for (let step = 0; step < this.maxSteps; step++) {
            // Build messages for LLM
            const messages = [
                { role: 'system', content: systemPrompt },
                { role: 'user', content: this.formatContext(context) },
                { role: 'user', content: `Question: ${question}\n\nWhat is your next step?` }
            ];

            // Get LLM response
            const response = await this.callLLM(messages);

            // Parse action
            const action = this.parseAction(response);

            // Record trace
            this.trace.push({
                step: step + 1,
                thought: action.thought,
                action: action.action,
                params: action.params
            });

            // Check if done
            if (action.action === 'ANSWER') {
                finalAnswer = action.params;
                this.trace[this.trace.length - 1].observation = 'Final answer provided';
                break;
            }

            // Execute graph operation
            const observation = await this.executeAction(action);
            this.trace[this.trace.length - 1].observation = observation;

            // Add to context
            context.push({
                step: step + 1,
                thought: action.thought,
                action: action.action,
                params: action.params,
                observation: observation
            });

            // Stop if error
            if (observation.error) {
                finalAnswer = `Unable to complete reasoning: ${observation.error}`;
                break;
            }
        }

        if (!finalAnswer) {
            finalAnswer = 'Could not find answer within step limit. Try rephrasing your question.';
        }

        return {
            answer: finalAnswer,
            trace: this.trace,
            steps: this.trace.length
        };
    }

    getSystemPrompt() {
        return `You are a graph reasoning agent navigating a knowledge graph to answer questions.

Available Actions:
1. SEARCH(keywords) - Find nodes matching keywords (ranked by centrality/importance)
2. EXPAND(node_id) - Get neighbors of a specific node
3. PATH(node1_id, node2_id) - Find path between two nodes
4. BRIDGE(keywords) - Find bridge nodes connecting different domains/components
5. INSIGHTS(node_ids) - Get relevant insights about nodes
6. DETAILS(node_id) - Get detailed information about a node
7. ANSWER(text) - Provide the final answer

Instructions:
- Think step-by-step about how to find the answer
- Start with SEARCH to find relevant nodes (returns high-centrality nodes first)
- Use BRIDGE to discover cross-domain connections and analogies
- Use EXPAND to explore neighbors
- Use PATH to find connections between specific nodes
- Use INSIGHTS for discovered patterns
- End with ANSWER when you have enough information

Format your response as:
THOUGHT: [Your reasoning about what to do next]
ACTION: [ACTION_NAME]([parameters])

Example:
THOUGHT: I need to find nodes related to "machine learning"
ACTION: SEARCH(machine learning)

When you have enough information to answer, use:
THOUGHT: I now have enough context to answer the question
ACTION: ANSWER(Your detailed answer here)

Important:
- Be concise in your thoughts
- Only take one action per step
- Use information from previous observations
- If a search returns no results, try different keywords
- When answering, be comprehensive and cite the graph structure you discovered`;
    }

    parseAction(response) {
        // Extract thought
        const thoughtMatch = response.match(/THOUGHT:\s*(.+?)(?=ACTION:|$)/is);
        const thought = thoughtMatch ? thoughtMatch[1].trim() : 'Continuing reasoning...';

        // Extract action
        const actionMatch = response.match(/ACTION:\s*(\w+)\s*\(([^)]*)\)/i);

        if (!actionMatch) {
            // No valid action found, try to extract answer
            if (response.toLowerCase().includes('answer:')) {
                return {
                    thought,
                    action: 'ANSWER',
                    params: response
                };
            }
            return {
                thought,
                action: 'ERROR',
                params: 'Could not parse action from response'
            };
        }

        const action = actionMatch[1].toUpperCase();
        const params = actionMatch[2].trim();

        return { thought, action, params };
    }

    async executeAction(action) {
        try {
            switch (action.action) {
                case 'SEARCH':
                    const searchResults = this.ops.search(action.params, 10);
                    return {
                        action: 'SEARCH',
                        query: action.params,
                        found: searchResults.length,
                        results: searchResults.map(r => ({
                            id: r.gid,
                            label: r.label,
                            score: r.score
                        }))
                    };

                case 'EXPAND':
                    const expandResults = this.ops.expand(action.params, 10);
                    return {
                        action: 'EXPAND',
                        node_id: action.params,
                        ...expandResults
                    };

                case 'PATH':
                    const [start, end] = action.params.split(',').map(s => s.trim());
                    const pathResults = this.ops.findPath(start, end);
                    return {
                        action: 'PATH',
                        from: start,
                        to: end,
                        ...pathResults
                    };

                case 'INSIGHTS':
                    const nodeIds = action.params.split(',').map(s => s.trim());
                    const insightResults = this.ops.getInsights(nodeIds);
                    return {
                        action: 'INSIGHTS',
                        node_ids: nodeIds,
                        ...insightResults
                    };

                case 'DETAILS':
                    const detailResults = this.ops.getNodeDetails(action.params);
                    return {
                        action: 'DETAILS',
                        node_id: action.params,
                        ...detailResults
                    };

                case 'BRIDGE':
                    const bridgeResults = this.ops.findBridges(action.params);
                    return {
                        action: 'BRIDGE',
                        keywords: action.params,
                        ...bridgeResults
                    };

                case 'ERROR':
                    return {
                        error: action.params
                    };

                default:
                    return {
                        error: `Unknown action: ${action.action}`
                    };
            }
        } catch (err) {
            return {
                error: `Error executing ${action.action}: ${err.message}`
            };
        }
    }

    formatContext(context) {
        if (context.length === 0) {
            return 'No previous steps. Start by searching for relevant nodes.';
        }

        let formatted = 'Previous Steps:\n\n';

        context.forEach(step => {
            formatted += `Step ${step.step}:\n`;
            formatted += `THOUGHT: ${step.thought}\n`;
            formatted += `ACTION: ${step.action}(${step.params})\n`;
            formatted += `OBSERVATION: ${JSON.stringify(step.observation, null, 2)}\n\n`;
        });

        return formatted;
    }

    getTraceHTML() {
        let html = '<div class="reasoning-trace">';

        this.trace.forEach(step => {
            const obs = typeof step.observation === 'object'
                ? JSON.stringify(step.observation, null, 2)
                : step.observation;

            html += `
                <div class="trace-step">
                    <div class="trace-header">
                        <strong>Step ${step.step}</strong>
                    </div>
                    <div class="trace-thought">
                        <span class="trace-label">💭 Thought:</span> ${step.thought}
                    </div>
                    <div class="trace-action">
                        <span class="trace-label">⚡ Action:</span> ${step.action}(${step.params})
                    </div>
                    <div class="trace-observation">
                        <span class="trace-label">👁️ Observation:</span>
                        <pre>${obs}</pre>
                    </div>
                </div>
            `;
        });

        html += '</div>';
        return html;
    }
}

// ═══════════════════════════════════════════════════════════════════
// INTEGRATION FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

/**
 * Enhanced query handler with multi-hop reasoning
 * Use this to replace the existing handleQuery() function
 */
async function handleQueryMultiHop(graphData, insights, callLLM, uiCallbacks) {
    const query = uiCallbacks.getQuery();
    if (!query) return;

    uiCallbacks.clearQuery();
    uiCallbacks.disableSend();
    uiCallbacks.appendUserMessage(query);

    const thinkingDiv = uiCallbacks.appendThinking();

    try {
        // Initialize agent
        const graphOps = new GraphOperations(graphData, insights);
        const agent = new GraphReActAgent(graphOps, callLLM, {
            maxSteps: 6,
            verbose: true
        });

        // Execute query
        const result = await agent.query(query);

        // Remove thinking indicator
        thinkingDiv.remove();

        // Show reasoning trace (collapsible)
        const traceHTML = agent.getTraceHTML();
        uiCallbacks.appendReasoningTrace(traceHTML);

        // Show final answer
        const entities = parseHighlightedEntities(result.answer);
        const cleanAnswer = cleanResponse(result.answer);

        uiCallbacks.appendAssistantMessage(cleanAnswer, entities);

        // Highlight entities if present
        if (entities.length > 0) {
            uiCallbacks.applyHighlights(entities);
        }

    } catch (err) {
        thinkingDiv.remove();
        uiCallbacks.appendAssistantMessage(
            `Error: ${err.message}\n\nCheck your API key and provider settings.`
        );
    } finally {
        uiCallbacks.enableSend();
    }
}

// Helper functions
function parseHighlightedEntities(text) {
    const match = text.match(/HIGHLIGHTED_ENTITIES:\s*(.+?)(?:\n|$)/i);
    return match ? match[1].split(',').map(s => s.trim()).filter(Boolean) : [];
}

function cleanResponse(text) {
    return text.replace(/\n?HIGHLIGHTED_ENTITIES:.*$/im, '').trim();
}

// Export for use in graph_rag.html
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        GraphOperations,
        GraphReActAgent,
        handleQueryMultiHop
    };
}

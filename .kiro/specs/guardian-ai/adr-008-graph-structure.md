# ADR-008: Adjacency List for Policy Graph Representation

**Status:** Accepted  
**Date:** 2026-03-07  
**Context:** Guardian AI's policy graph engine needs an in-memory data structure to represent directed graphs of tool transitions. The structure must support fast neighbor lookups (for transition validation), cycle detection, and path traversal.

## Decision

**Use an adjacency list** (`std::unordered_map<string, vector<PolicyEdge>>`) for the policy graph representation.

## Alternatives Considered

| Data Structure | Neighbor Lookup | Edge Existence | Memory | Best For |
|---------------|----------------|---------------|--------|----------|
| **Adjacency List** | O(degree) | O(degree) | O(V + E) | Sparse graphs |
| **Adjacency Matrix** | O(1) | O(1) | O(V²) | Dense graphs |
| **Edge List** | O(E) | O(E) | O(E) | Simple iteration |
| **CSR (Compressed Sparse Row)** | O(degree) | O(degree) | O(V + E) | Static graphs, cache-friendly |

## Rationale

### Why adjacency list

**1. Policy graphs are sparse**

A typical policy has 5–50 tools with 10–100 transitions. An adjacency matrix for 50 nodes wastes 2,500 cells when only ~100 contain edges. The adjacency list stores only existing edges.

**2. Transition validation is the hot path**

The most frequent operation is: "given the current tool, is the next tool a valid neighbor?" This is O(degree) with an adjacency list, and the average degree in policy graphs is 2–5 — effectively O(1) in practice:

```cpp
bool PolicyGraph::has_edge(const std::string& from, const std::string& to) const {
    auto it = adjacency_list_.find(from);
    if (it == adjacency_list_.end()) return false;
    return std::any_of(it->second.begin(), it->second.end(),
                       [&](const PolicyEdge& e) { return e.to_node_id == to; });
}
```

For graphs with high-degree nodes, the inner vector can be replaced with `std::unordered_set` for O(1) lookup.

**3. Easy serialization**

The adjacency list maps directly to the JSON format:
```json
{
  "nodes": [...],
  "edges": [{"from": "A", "to": "B"}, ...]
}
```

**4. Supports all required algorithms**

- **Cycle detection**: Iterate edges from each node ✅
- **Path validation**: Follow edges sequentially ✅
- **Exfiltration detection**: BFS/DFS from sensitive sources ✅
- **Neighbor suggestions**: Direct lookup from adjacency list ✅

### Why not adjacency matrix?

O(V²) memory for a 200-node graph = 40,000 entries. While this fits in memory, it wastes cache lines traversing mostly-empty rows. The adjacency list keeps related edges contiguous.

### Why not CSR?

CSR (Compressed Sparse Row) is more cache-friendly for static graphs, but policy graphs need to support `add_node()` and `add_edge()` during programmatic construction. CSR requires rebuilding the entire structure on modification.

## Consequences

- `std::unordered_map<std::string, std::vector<PolicyEdge>>` as the core structure
- Neighbor lookup is O(degree), practically O(1) for typical policy graphs
- Thread safety via immutability after construction (no locks needed for reads)
- Memory usage scales linearly with graph size: O(V + E)
- For graphs exceeding 200 nodes, consider switching inner vector to `unordered_set` for O(1) edge existence checks

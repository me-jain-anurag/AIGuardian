// include/guardian/policy_graph.hpp
// Owner: Dev A
// Policy graph engine with adjacency list representation
// Phase 3: Performance optimizations (string interning, LRU cache, fast lookup)
#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <unordered_set>
#include <set>
#include <list>
#include <mutex>

namespace guardian {

struct PolicyNode {
    std::string id;
    std::string tool_name;
    RiskLevel risk_level = RiskLevel::LOW;
    NodeType node_type = NodeType::NORMAL;
    std::map<std::string, std::string> metadata;
    std::string wasm_module;
    SandboxConfig sandbox_config;
};

struct PolicyEdge {
    std::string from_node_id;
    std::string to_node_id;
    std::map<std::string, std::string> conditions;
    std::map<std::string, std::string> metadata;
};

// ============================================================================
// String Interning Pool (Phase 3 performance optimization)
// ============================================================================
// Reduces memory for repeated tool names / node IDs across large graphs
class StringPool {
public:
    // Returns a reference to the interned string (same pointer for same content)
    const std::string& intern(const std::string& s);
    size_t size() const { return pool_.size(); }
    void clear() { pool_.clear(); }

private:
    std::unordered_set<std::string> pool_;
};

// ============================================================================
// LRU Cache for validation/lookup results (Phase 3)
// ============================================================================
template <typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t max_size = 1000) : max_size_(max_size) {}

    std::optional<Value> get(const Key& key) {
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) return std::nullopt;
        // Move to front (most recently used)
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
        return it->second->second;
    }

    void put(const Key& key, const Value& value) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        }
        cache_list_.push_front({key, value});
        cache_map_[key] = cache_list_.begin();
        // Evict LRU if over capacity
        if (cache_map_.size() > max_size_) {
            auto last = cache_list_.end();
            --last;
            cache_map_.erase(last->first);
            cache_list_.pop_back();
        }
    }

    void clear() { cache_map_.clear(); cache_list_.clear(); }
    size_t size() const { return cache_map_.size(); }
    size_t capacity() const { return max_size_; }

private:
    size_t max_size_;
    std::list<std::pair<Key, Value>> cache_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
};

// ============================================================================
// PolicyGraph
// ============================================================================

class PolicyGraph {
public:
    PolicyGraph() = default;
    ~PolicyGraph() = default;

    // Node operations
    void add_node(const PolicyNode& node);
    void remove_node(const std::string& node_id);
    bool has_node(const std::string& node_id) const;
    std::optional<PolicyNode> get_node(const std::string& node_id) const;

    // Edge operations
    void add_edge(const PolicyEdge& edge);
    void remove_edge(const std::string& from_id, const std::string& to_id);
    bool has_edge(const std::string& from_id, const std::string& to_id) const;
    std::vector<PolicyEdge> get_neighbors(const std::string& node_id) const;

    // Accessors
    size_t node_count() const;
    size_t edge_count() const;
    std::vector<std::string> get_all_node_ids() const;

    // Serialization
    std::string to_json() const;
    static PolicyGraph from_json(const std::string& json_str);
    std::string to_dot() const;
    static PolicyGraph from_dot(const std::string& dot_str);

    // Phase 3: Performance features
    StringPool& string_pool() { return string_pool_; }
    LRUCache<std::string, bool>& edge_cache() { return edge_cache_; }
    void clear_caches();

    // Phase 3: Fast lookup — O(1) node-by-tool-name
    std::optional<PolicyNode> get_node_by_tool_name(const std::string& tool_name) const;

    // Phase 3: Graph analysis
    std::vector<std::string> find_path(const std::string& from_id,
                                        const std::string& to_id) const;
    bool is_reachable(const std::string& from_id, const std::string& to_id) const;

private:
    std::unordered_map<std::string, PolicyNode> nodes_;
    std::unordered_map<std::string, std::vector<PolicyEdge>> adjacency_list_;
    // Phase 3: Performance internals
    StringPool string_pool_;
    LRUCache<std::string, bool> edge_cache_{2000};
    std::unordered_map<std::string, std::string> tool_name_to_id_; // fast lookup index
};

} // namespace guardian

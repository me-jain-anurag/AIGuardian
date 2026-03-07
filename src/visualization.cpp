// src/visualization.cpp
// Owner: Dev D (stub implementation by Dev A for integration)
// Policy graph and action sequence visualization
#include "guardian/visualization.hpp"
#include "guardian/policy_graph.hpp"

#include <sstream>

namespace guardian {

// ============================================================================
// Helpers
// ============================================================================

static std::string node_fill_color(NodeType type) {
    switch (type) {
        case NodeType::SENSITIVE_SOURCE:     return "red";
        case NodeType::EXTERNAL_DESTINATION: return "orange";
        case NodeType::DATA_PROCESSOR:       return "lightgreen";
        default:                             return "lightblue";
    }
}

static std::string node_type_abbr(NodeType type) {
    switch (type) {
        case NodeType::SENSITIVE_SOURCE:     return "SRC";
        case NodeType::EXTERNAL_DESTINATION: return "DEST";
        case NodeType::DATA_PROCESSOR:       return "PROC";
        default:                             return "NORM";
    }
}

static std::string risk_level_str(RiskLevel level) {
    switch (level) {
        case RiskLevel::HIGH:   return "HIGH";
        case RiskLevel::MEDIUM: return "MED";
        case RiskLevel::LOW:    return "LOW";
        default:                return "NONE";
    }
}

// ============================================================================
// render_graph
// ============================================================================

std::string VisualizationEngine::render_graph(
    const PolicyGraph& graph,
    const VisualizationOptions& options) const {

    if (options.output_format == VisualizationOptions::JSON) {
        return graph.to_json();
    }
    if (options.output_format == VisualizationOptions::ASCII) {
        return render_ascii_graph(graph);
    }

    // DOT format — custom generation with type-based colours and optional metadata
    std::ostringstream dot;
    dot << "digraph PolicyGraph {\n";
    dot << "    rankdir=LR;\n";
    dot << "    node [style=filled, fontname=\"Arial\"];\n\n";

    auto ids = graph.get_all_node_ids();
    for (const auto& id : ids) {
        auto node = graph.get_node(id);
        if (!node) continue;

        std::string color = node_fill_color(node->node_type);
        std::string label = node->tool_name;
        if (options.show_metadata) {
            label += "\\n[" + node_type_abbr(node->node_type) +
                     " | " + risk_level_str(node->risk_level) + "]";
        }

        dot << "    \"" << id << "\" [label=\"" << label
            << "\", fillcolor=\"" << color << "\"];\n";
    }
    dot << "\n";

    for (const auto& id : ids) {
        auto edges = graph.get_neighbors(id);
        for (const auto& e : edges) {
            dot << "    \"" << id << "\" -> \"" << e.to_node_id << "\";\n";
        }
    }

    dot << "}\n";
    return dot.str();
}

// ============================================================================
// render_sequence
// ============================================================================

std::string VisualizationEngine::render_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& results,
    const VisualizationOptions& options) const {

    std::ostringstream dot;
    dot << "digraph ActionSequence {\n";
    dot << "    rankdir=LR;\n";
    dot << "    node [style=filled, fontname=\"Arial\"];\n\n";

    for (size_t i = 0; i < sequence.size(); ++i) {
        bool approved = (i >= results.size()) || results[i].approved;
        std::string color = node_fill_color(NodeType::NORMAL);
        auto node = graph.get_node_by_tool_name(sequence[i].tool_name);
        if (node) color = node_fill_color(node->node_type);

        if (!approved && options.highlight_violations) {
            dot << "    \"step_" << i << "\" [label=\""
                << sequence[i].tool_name << "\\n(step " << (i + 1) << ")"
                << "\", fillcolor=\"red\", shape=octagon];\n";
        } else {
            dot << "    \"step_" << i << "\" [label=\""
                << sequence[i].tool_name << "\\n(step " << (i + 1) << ")"
                << "\", fillcolor=\"" << color << "\"];\n";
        }
    }
    dot << "\n";

    for (size_t i = 0; i + 1 < sequence.size(); ++i) {
        bool next_approved = (i + 1 >= results.size()) || results[i + 1].approved;
        if (!next_approved && options.highlight_violations) {
            dot << "    \"step_" << i << "\" -> \"step_" << (i + 1)
                << "\" [style=dashed, color=red];\n";
        } else {
            dot << "    \"step_" << i << "\" -> \"step_" << (i + 1)
                << "\" [color=darkgreen];\n";
        }
    }

    dot << "}\n";
    return dot.str();
}

// ============================================================================
// render_ascii_graph
// ============================================================================

std::string VisualizationEngine::render_ascii_graph(const PolicyGraph& graph) const {
    std::ostringstream out;
    out << "=== Policy Graph ===\n";
    auto ids = graph.get_all_node_ids();
    for (const auto& id : ids) {
        auto node = graph.get_node(id);
        if (node) {
            out << "[" << node_type_abbr(node->node_type) << "] "
                << node->tool_name << " (" << id << ")\n";
            auto neighbors = graph.get_neighbors(id);
            for (const auto& edge : neighbors) {
                out << "  -> " << edge.to_node_id << "\n";
            }
        }
    }
    out << "\n--- Legend ---\n";
    out << "[SRC]  Sensitive Source\n";
    out << "[PROC] Data Processor\n";
    out << "[DEST] External Destination\n";
    out << "[NORM] Normal\n";
    return out.str();
}

// ============================================================================
// render_ascii_sequence
// ============================================================================

std::string VisualizationEngine::render_ascii_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& results) const {

    std::ostringstream out;
    out << "=== Action Sequence ===\n";

    int approved_count = 0;
    int blocked_count  = 0;

    for (size_t i = 0; i < sequence.size(); ++i) {
        bool approved = (i >= results.size()) || results[i].approved;
        if (approved) {
            out << (i + 1) << ". " << sequence[i].tool_name << " [APPROVED]\n";
            ++approved_count;
        } else {
            std::string reason = (i < results.size()) ? results[i].reason : "";
            out << (i + 1) << ". " << sequence[i].tool_name
                << " [BLOCKED: " << reason << "]\n";
            ++blocked_count;
        }
    }

    out << "\n--- Summary ---\n";
    out << approved_count << " approved, " << blocked_count << " blocked\n";

    return out.str();
}

} // namespace guardian

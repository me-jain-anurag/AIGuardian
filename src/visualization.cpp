// src/visualization.cpp
// Owner: Dev D (stub implementation by Dev A for integration)
// Policy graph and action sequence visualization
#include "guardian/visualization.hpp"
#include "guardian/policy_graph.hpp"

namespace guardian {

std::string VisualizationEngine::render_graph(
    const PolicyGraph& graph,
    const VisualizationOptions& options) const {

    if (options.output_format == VisualizationOptions::JSON) {
        return graph.to_json();
    }
    // Default: DOT format
    return graph.to_dot();
}

std::string VisualizationEngine::render_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& /*results*/,
    const VisualizationOptions& /*options*/) const {

    std::string dot = "digraph ActionSequence {\n";
    dot += "    rankdir=LR;\n";
    dot += "    node [style=filled, fontname=\"Arial\"];\n\n";

    // Show executed tools in sequence
    for (size_t i = 0; i < sequence.size(); ++i) {
        auto node = graph.get_node(sequence[i].tool_name);
        std::string color = "lightblue";
        if (node) {
            switch (node->node_type) {
                case NodeType::SENSITIVE_SOURCE:     color = "red"; break;
                case NodeType::EXTERNAL_DESTINATION: color = "orange"; break;
                case NodeType::DATA_PROCESSOR:       color = "green"; break;
                default: break;
            }
        }
        dot += "    \"step_" + std::to_string(i) + "\" [label=\"" +
               sequence[i].tool_name + "\\n(step " + std::to_string(i + 1) +
               ")\", fillcolor=\"" + color + "\"];\n";
    }
    dot += "\n";

    // Connect sequential steps
    for (size_t i = 0; i + 1 < sequence.size(); ++i) {
        dot += "    \"step_" + std::to_string(i) + "\" -> \"step_" +
               std::to_string(i + 1) + "\";\n";
    }

    dot += "}\n";
    return dot;
}

} // namespace guardian

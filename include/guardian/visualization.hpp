// include/guardian/visualization.hpp
// Owner: Dev D
// Policy graph and action sequence visualization
#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <map>

namespace guardian {

// Forward declaration
class PolicyGraph;

struct VisualizationOptions {
    enum Format { DOT, SVG, PNG, ASCII, JSON };
    Format output_format = DOT;
    bool highlight_violations = true;
    bool show_metadata = true;
    std::map<std::string, std::string> style_overrides;
};

class VisualizationEngine {
public:
    VisualizationEngine() = default;
    ~VisualizationEngine() = default;

    // Graph rendering
    std::string render_graph(const PolicyGraph& graph,
                              const VisualizationOptions& options = {}) const;

    // Sequence rendering
    std::string render_sequence(const PolicyGraph& graph,
                                 const std::vector<ToolCall>& sequence,
                                 const std::vector<ValidationResult>& results,
                                 const VisualizationOptions& options = {}) const;

    // ASCII rendering
    std::string render_ascii_graph(const PolicyGraph& graph) const;
    std::string render_ascii_sequence(const PolicyGraph& graph,
                                       const std::vector<ToolCall>& sequence,
                                       const std::vector<ValidationResult>& results) const;
};

} // namespace guardian

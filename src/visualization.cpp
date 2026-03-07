// src/visualization.cpp
// Owner: Dev D
// Policy graph and action sequence visualization
// Provides DOT, ASCII, and optional SVG/PNG (via Graphviz) rendering
#include "guardian/visualization.hpp"
#include "guardian/policy_graph.hpp"
#include "guardian/types.hpp"

#include <sstream>
#include <algorithm>
#include <cstdlib>    // std::system
#include <filesystem>

#ifdef HAVE_GRAPHVIZ
#include <gvc.h>
#endif

namespace guardian {

// ============================================================================
// ANSI colour helpers (used by ASCII renderers)
// ============================================================================
namespace {

// ANSI escape codes — work on Linux natively and Windows 10+ after
// enable_virtual_terminal() is called in the CLI entry point.
constexpr const char* ANSI_RESET  = "\033[0m";
constexpr const char* ANSI_RED    = "\033[31m";
constexpr const char* ANSI_GREEN  = "\033[32m";
constexpr const char* ANSI_YELLOW = "\033[33m";
constexpr const char* ANSI_BLUE   = "\033[34m";
constexpr const char* ANSI_ORANGE = "\033[33m";  // closest available
constexpr const char* ANSI_BOLD   = "\033[1m";

// Map NodeType → DOT fill-colour string
std::string node_dot_color(NodeType nt) {
    switch (nt) {
        case NodeType::SENSITIVE_SOURCE:     return "red";
        case NodeType::EXTERNAL_DESTINATION: return "orange";
        case NodeType::DATA_PROCESSOR:       return "lightgreen";
        default:                             return "lightblue";
    }
}

// Map NodeType → ANSI colour prefix
const char* node_ansi_color(NodeType nt) {
    switch (nt) {
        case NodeType::SENSITIVE_SOURCE:     return ANSI_RED;
        case NodeType::EXTERNAL_DESTINATION: return ANSI_ORANGE;
        case NodeType::DATA_PROCESSOR:       return ANSI_GREEN;
        default:                             return ANSI_BLUE;
    }
}

// Map RiskLevel → short label
std::string risk_label(RiskLevel rl) {
    switch (rl) {
        case RiskLevel::LOW:      return "LOW";
        case RiskLevel::MEDIUM:   return "MED";
        case RiskLevel::HIGH:     return "HIGH";
        case RiskLevel::CRITICAL: return "CRIT";
        default:                  return "?";
    }
}

// Map NodeType → short label
std::string type_label(NodeType nt) {
    switch (nt) {
        case NodeType::SENSITIVE_SOURCE:     return "SRC";
        case NodeType::EXTERNAL_DESTINATION: return "DST";
        case NodeType::DATA_PROCESSOR:       return "PROC";
        default:                             return "NORM";
    }
}

// DOT-escape a string (escape " and \)
std::string dot_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

} // anonymous namespace

// ============================================================================
// render_graph — DOT format from PolicyGraph
// ============================================================================

std::string VisualizationEngine::render_graph(
    const PolicyGraph& graph,
    const VisualizationOptions& options) const {

    // ASCII format delegates to ASCII renderer
    if (options.output_format == VisualizationOptions::ASCII) {
        return render_ascii_graph(graph);
    }

    // DOT format (base for SVG/PNG via Graphviz too)
    std::ostringstream dot;
    dot << "digraph PolicyGraph {\n";
    dot << "    rankdir=LR;\n";
    dot << "    graph [fontname=\"Arial\" bgcolor=\"white\"];\n";
    dot << "    node  [style=filled fontname=\"Arial\" fontsize=12];\n";
    dot << "    edge  [fontname=\"Arial\" fontsize=10];\n\n";

    // ── Nodes ─────────────────────────────────────────────────────────────────
    for (const auto& id : graph.get_all_node_ids()) {
        auto node_opt = graph.get_node(id);
        if (!node_opt) continue;
        const auto& node = *node_opt;

        std::string color = node_dot_color(node.node_type);

        dot << "    \"" << dot_escape(id) << "\" [\n";
        dot << "        label=\"" << dot_escape(node.tool_name);
        if (options.show_metadata) {
            dot << "\\n[" << type_label(node.node_type)
                << " | " << risk_label(node.risk_level) << "]";
        }
        dot << "\"\n";
        dot << "        fillcolor=\"" << color << "\"\n";

        // Style overrides (from options)
        if (auto it = options.style_overrides.find(id);
            it != options.style_overrides.end()) {
            dot << "        " << it->second << "\n";
        }
        dot << "    ];\n";
    }
    dot << "\n";

    // ── Edges ─────────────────────────────────────────────────────────────────
    for (const auto& id : graph.get_all_node_ids()) {
        for (const auto& edge : graph.get_neighbors(id)) {
            dot << "    \"" << dot_escape(edge.from_node_id)
                << "\" -> \"" << dot_escape(edge.to_node_id) << "\"";

            if (options.show_metadata && !edge.conditions.empty()) {
                std::string cond_label;
                for (const auto& [k, v] : edge.conditions) {
                    if (!cond_label.empty()) cond_label += "\\n";
                    cond_label += k + "=" + v;
                }
                dot << " [label=\"" << dot_escape(cond_label) << "\"]";
            }
            dot << ";\n";
        }
    }

    dot << "}\n";

    // ── Optional SVG/PNG via Graphviz CLI ─────────────────────────────────────
#ifdef HAVE_GRAPHVIZ
    if (options.output_format == VisualizationOptions::SVG ||
        options.output_format == VisualizationOptions::PNG) {

        namespace fs = std::filesystem;
        auto tmp_dot = fs::temp_directory_path() / "guardian_policy.dot";
        auto tmp_out = fs::temp_directory_path() /
                       (options.output_format == VisualizationOptions::SVG
                            ? "guardian_policy.svg"
                            : "guardian_policy.png");

        std::ofstream f(tmp_dot);
        f << dot.str();
        f.close();

        std::string fmt = options.output_format == VisualizationOptions::SVG
                              ? "svg" : "png";
        std::string cmd = "dot -T" + fmt + " " + tmp_dot.string() +
                          " -o " + tmp_out.string();
        std::system(cmd.c_str());

        // Return path to generated file as the result string
        return tmp_out.string();
    }
#endif

    return dot.str();
}

// ============================================================================
// render_sequence — Highlight action sequence path through graph
// ============================================================================

std::string VisualizationEngine::render_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& results,
    const VisualizationOptions& options) const {

    // ASCII format delegates
    if (options.output_format == VisualizationOptions::ASCII) {
        return render_ascii_sequence(graph, sequence, results);
    }

    std::ostringstream dot;
    dot << "digraph ActionSequence {\n";
    dot << "    rankdir=LR;\n";
    dot << "    graph [fontname=\"Arial\"];\n";
    dot << "    node  [style=filled fontname=\"Arial\" fontsize=12];\n";
    dot << "    edge  [fontname=\"Arial\" fontsize=10];\n\n";

    // ── Step nodes ────────────────────────────────────────────────────────────
    for (size_t i = 0; i < sequence.size(); ++i) {
        const auto& call = sequence[i];

        // Determine if this step had a validation result
        bool approved = true;
        if (i < results.size()) {
            approved = results[i].approved;
        }

        // Base colour from node type
        auto node_opt = graph.get_node(call.tool_name);
        std::string base_color = node_opt
            ? node_dot_color(node_opt->node_type)
            : "lightgray";

        // Violations override colour
        std::string fill_color = approved ? base_color : "red";
        std::string border_color = approved ? "black" : "darkred";
        int penwidth = approved ? 1 : 3;

        std::string node_id = "step_" + std::to_string(i);
        dot << "    \"" << node_id << "\" [\n";
        dot << "        label=\"" << dot_escape(call.tool_name)
            << "\\n(step " << (i + 1) << ")";
        if (!approved && i < results.size() && options.highlight_violations) {
            dot << "\\n✗ " << dot_escape(results[i].reason);
        }
        dot << "\"\n";
        dot << "        fillcolor=\"" << fill_color << "\"\n";
        dot << "        color=\"" << border_color << "\"\n";
        dot << "        penwidth=" << penwidth << "\n";
        if (!approved && options.highlight_violations) {
            dot << "        shape=octagon\n";  // distinct violation shape
        }
        dot << "    ];\n";
    }
    dot << "\n";

    // ── Edges between sequential steps ────────────────────────────────────────
    for (size_t i = 0; i + 1 < sequence.size(); ++i) {
        bool next_approved = (i + 1 < results.size()) ? results[i + 1].approved : true;

        std::string from_id = "step_" + std::to_string(i);
        std::string to_id   = "step_" + std::to_string(i + 1);

        dot << "    \"" << from_id << "\" -> \"" << to_id << "\" [\n";
        if (next_approved) {
            dot << "        color=\"darkgreen\" style=solid\n";
        } else {
            dot << "        color=\"red\" style=dashed\n";
        }
        dot << "    ];\n";
    }

    dot << "}\n";
    return dot.str();
}

// ============================================================================
// render_ascii_graph — Box-drawing characters + ANSI colours
// ============================================================================

std::string VisualizationEngine::render_ascii_graph(
    const PolicyGraph& graph) const {

    std::ostringstream out;
    const auto all_ids = graph.get_all_node_ids();

    out << ANSI_BOLD << "╔══════════════════════════════╗\n"
        << "║     POLICY GRAPH             ║\n"
        << "╚══════════════════════════════╝" << ANSI_RESET << "\n\n";

    if (all_ids.empty()) {
        out << "  (empty graph)\n";
        return out.str();
    }

    // ── Print each node ───────────────────────────────────────────────────────
    for (const auto& id : all_ids) {
        auto node_opt = graph.get_node(id);
        if (!node_opt) continue;
        const auto& node = *node_opt;

        const char* color = node_ansi_color(node.node_type);

        out << color << ANSI_BOLD
            << "  ┌─────────────────────────────┐\n"
            << "  │ " << std::left
            << node.tool_name;

        // Pad to 27 chars for the box
        int pad = 27 - static_cast<int>(node.tool_name.size());
        if (pad > 0) out << std::string(pad, ' ');
        out << " │\n";

        std::string meta = "[" + type_label(node.node_type) +
                           " | risk:" + risk_label(node.risk_level) + "]";
        out << "  │ " << meta;
        pad = 27 - static_cast<int>(meta.size());
        if (pad > 0) out << std::string(pad, ' ');
        out << " │\n";

        out << "  └─────────────────────────────┘" << ANSI_RESET << "\n";

        // ── Print outgoing edges ──────────────────────────────────────────────
        auto neighbors = graph.get_neighbors(id);
        for (size_t ei = 0; ei < neighbors.size(); ++ei) {
            const auto& edge = neighbors[ei];
            bool last = (ei + 1 == neighbors.size());
            out << (last ? "          └──→ " : "          ├──→ ")
                << ANSI_BLUE << edge.to_node_id << ANSI_RESET << "\n";
        }
        if (!neighbors.empty()) out << "\n";
    }

    // ── Legend ────────────────────────────────────────────────────────────────
    out << "\n" << ANSI_BOLD << "  Legend:\n" << ANSI_RESET
        << "  " << ANSI_RED    << "■" << ANSI_RESET << " SENSITIVE_SOURCE    "
        << "  " << ANSI_ORANGE << "■" << ANSI_RESET << " EXTERNAL_DESTINATION\n"
        << "  " << ANSI_GREEN  << "■" << ANSI_RESET << " DATA_PROCESSOR      "
        << "  " << ANSI_BLUE   << "■" << ANSI_RESET << " NORMAL\n\n";

    return out.str();
}

// ============================================================================
// render_ascii_sequence — Action sequence with approve/reject markers
// ============================================================================

std::string VisualizationEngine::render_ascii_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& results) const {

    std::ostringstream out;

    out << ANSI_BOLD
        << "╔══════════════════════════════╗\n"
        << "║     ACTION SEQUENCE          ║\n"
        << "╚══════════════════════════════╝" << ANSI_RESET << "\n\n";

    if (sequence.empty()) {
        out << "  (no actions in sequence)\n";
        return out.str();
    }

    for (size_t i = 0; i < sequence.size(); ++i) {
        const auto& call = sequence[i];
        bool approved = (i < results.size()) ? results[i].approved : true;

        auto node_opt = graph.get_node(call.tool_name);
        const char* node_color = node_opt
            ? node_ansi_color(node_opt->node_type)
            : ANSI_RESET;

        // Step header
        out << "  Step " << (i + 1) << ": "
            << node_color << ANSI_BOLD << call.tool_name << ANSI_RESET;

        if (node_opt) {
            out << "  [" << type_label(node_opt->node_type)
                << " | " << risk_label(node_opt->risk_level) << "]";
        }
        out << "\n";

        // Approval status
        if (approved) {
            out << "  " << ANSI_GREEN << "  ✓ APPROVED" << ANSI_RESET << "\n";
        } else {
            out << "  " << ANSI_RED << "  ✗ BLOCKED";
            if (i < results.size() && !results[i].reason.empty()) {
                out << " — " << results[i].reason;
            }
            // If exfiltration detected
            if (i < results.size() && results[i].detected_exfiltration) {
                const auto& exfil = *results[i].detected_exfiltration;
                out << "\n  " << ANSI_RED
                    << "    ⚠ Exfiltration: "
                    << exfil.source_node << " → " << exfil.destination_node;
            }
            // If cycle detected
            if (i < results.size() && results[i].detected_cycle) {
                const auto& cycle = *results[i].detected_cycle;
                out << "\n  " << ANSI_YELLOW
                    << "    ⟳ Cycle detected (len=" << cycle.cycle_length
                    << ", at index=" << cycle.cycle_start_index << ")";
            }
            out << ANSI_RESET << "\n";
        }

        // Arrow to next step
        if (i + 1 < sequence.size()) {
            bool next_approved = (i + 1 < results.size())
                                 ? results[i + 1].approved : true;
            out << "\n  " << (next_approved ? ANSI_GREEN : ANSI_RED)
                << "        ↓" << ANSI_RESET << "\n\n";
        }
    }

    // ── Summary bar ───────────────────────────────────────────────────────────
    size_t approved_count = 0;
    size_t blocked_count  = 0;
    for (size_t i = 0; i < results.size() && i < sequence.size(); ++i) {
        if (results[i].approved) ++approved_count;
        else                     ++blocked_count;
    }

    out << "\n" << ANSI_BOLD << "  ──────────────────────────────\n"
        << "  Summary: "
        << ANSI_GREEN << approved_count << " approved  "
        << ANSI_RED   << blocked_count  << " blocked"
        << ANSI_RESET << "\n\n";

    return out.str();
}

} // namespace guardian

# AIGuardian API Reference

This document covers the public API for the `Logger` and `VisualizationEngine` components.

## `guardian::Logger`

The `Logger` provides a thread-safe, multi-format logging system.

### Methods
- `void debug(const std::string& component, const std::string& message)`
- `void info(const std::string& component, const std::string& message)`
- `void warn(const std::string& component, const std::string& message)`
- `void error(const std::string& component, const std::string& message)`
- `void set_output_file(const std::string& path)`: Redirects output to a file.
- `void set_json_format(bool enable)`: Toggle between structured JSON and human-readable text.
- `std::string export_logs()`: Returns all logged entries as a single JSON string.

## `guardian::VisualizationEngine`

The `VisualizationEngine` turns complex policy graphs into human-understandable visual formats.

### Methods
- `std::string render_graph(const PolicyGraph& graph, const VisualizationOptions& options)`: 
    - Supports `ASCII`, `DOT`, `SVG`, and `PNG`.
- `std::string render_sequence(const PolicyGraph& graph, const std::vector<ToolCall>& sequence, const std::vector<ValidationResult>& results, const VisualizationOptions& options)`:
    - Highlights a specific tool-calling path through the graph.
    - Colors approved transitions green and failed ones red.

### `VisualizationOptions`
- `output_format`: `ASCII`, `DOT`, `SVG`, `PNG`.
- `highlight_violations`: Boolean to toggle red-border highlighting on nodes that failed validation.

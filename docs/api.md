# AIGuardian API Reference

## guardian::Guardian

The main entry point. Owns all internal components and exposes the full API.

### Constructor

```cpp
Guardian(
    const std::string& policy_file_path,
    const std::string& wasm_tools_dir = "",
    const std::string& config_file_path = ""
);
```

Throws `std::runtime_error` if the policy file is missing or contains invalid JSON.

### Validation

```cpp
ValidationResult validate_tool_call(
    const std::string& tool_name,
    const std::string& session_id
);
```

Validates whether `tool_name` is permitted as the next call in `session_id`. Does not execute anything. Returns a `ValidationResult` with `approved`, `reason`, and optional `detected_cycle` / `detected_exfiltration`.

```cpp
std::pair<ValidationResult, std::optional<SandboxResult>> execute_tool(
    const std::string& tool_name,
    const std::map<std::string, std::string>& params,
    const std::string& session_id
);
```

Validates and, if approved, executes `tool_name` inside the WebAssembly sandbox. Returns both the validation result and the optional sandbox result.

### Sessions

```cpp
std::string create_session();
void end_session(const std::string& session_id);
bool is_initialized() const;
```

Each call to `create_session` returns a unique session ID. The session tracks the sequence of tool calls made so far, which is used for transition and cycle validation.

### Visualization

```cpp
std::string visualize_policy(const std::string& format = "dot") const;
```

Returns the policy graph as a string. `format` can be `"dot"` (Graphviz), `"ascii"`, or `"json"`.

### Config

```cpp
void update_config(const Config& config);
Config get_config() const;
```

---

## guardian::PolicyValidator

Validates individual tool calls against the policy graph. Used internally by `Guardian` but also constructable directly.

```cpp
PolicyValidator(const PolicyGraph& graph);
PolicyValidator(const PolicyGraph& graph, const Config& config);
```

### Methods

```cpp
ValidationResult validate(
    const std::string& tool_name,
    const std::vector<ToolCall>& action_sequence
) const;
```

Full validation: transition check, cycle detection, exfiltration detection. Results are cached (LRU, size 200) for repeated calls with the same context.

```cpp
bool check_transition(const std::string& from_tool, const std::string& to_tool) const;
```

Returns true if a direct edge exists from `from_tool` to `to_tool`. Pass an empty string for `from_tool` to check whether `to_tool` is a valid starting tool (i.e., it exists in the graph).

```cpp
std::optional<CycleInfo> detect_cycle(const std::vector<ToolCall>& sequence) const;
```

Returns a `CycleInfo` if any tool appears more than `cycle_threshold` times **consecutively** in `sequence`. Non-consecutive repetitions do not trigger this.

```cpp
std::optional<ExfiltrationPath> detect_exfiltration(const std::vector<ToolCall>& sequence) const;
```

Returns an `ExfiltrationPath` if a `SENSITIVE_SOURCE` node is followed by an `EXTERNAL_DESTINATION` node with no `DATA_PROCESSOR` in between.

```cpp
bool is_valid_path(const std::vector<std::string>& tool_names) const;
```

Returns true if every consecutive pair in `tool_names` has a valid edge, and every single tool name exists in the graph.

```cpp
void set_cycle_threshold(uint32_t threshold);
double get_cache_hit_rate() const;
```

---

## guardian::Logger

Thread-safe structured logger. Use the singleton (`Logger::instance()`) or create a local instance.

```cpp
Logger(LogLevel min_level = LogLevel::INFO);
static Logger& instance();
```

### Logging

```cpp
void debug(const std::string& component, const std::string& message, const std::string& details = "");
void info (const std::string& component, const std::string& message, const std::string& details = "");
void warn (const std::string& component, const std::string& message, const std::string& details = "");
void error(const std::string& component, const std::string& message, const std::string& details = "");
```

### Configuration

```cpp
void set_level(LogLevel level);
void set_output_file(const std::string& path);  // pass "" to disable file logging
void set_json_format(bool enabled);
```

### Export

```cpp
std::string export_logs() const;             // all entries as a JSON array
std::string export_logs(LogLevel min) const; // filtered by minimum level
void clear();
```

Each JSON entry contains: `timestamp`, `level`, `component`, `tool_name`, `session_id`, `message`, `details`.

---

## guardian::VisualizationEngine

Renders policy graphs and tool call sequences.

```cpp
std::string render_graph(const PolicyGraph& graph, const VisualizationOptions& options = {}) const;
```

Formats: `DOT` (default), `ASCII`, `JSON`. With `show_metadata = true`, node labels include type abbreviation (`SRC`, `PROC`, `DEST`, `NORM`) and risk level. Node colours in DOT: `red` (SENSITIVE_SOURCE), `orange` (EXTERNAL_DESTINATION), `lightgreen` (DATA_PROCESSOR), `lightblue` (NORMAL).

```cpp
std::string render_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& results,
    const VisualizationOptions& options = {}
) const;
```

Renders a tool call sequence as a DOT digraph. Approved transitions are drawn in `darkgreen`. Rejected steps get `fillcolor="red"`, `shape=octagon`, and a `style=dashed` edge.

```cpp
std::string render_ascii_graph(const PolicyGraph& graph) const;
std::string render_ascii_sequence(
    const PolicyGraph& graph,
    const std::vector<ToolCall>& sequence,
    const std::vector<ValidationResult>& results
) const;
```

ASCII output includes a legend for node types. Sequence output shows `APPROVED` or `BLOCKED: <reason>` per step, a summary count, and marks any detected cycle.

---

## Key types

```cpp
struct ValidationResult {
    bool approved;
    std::string reason;
    std::vector<std::string> suggested_alternatives;
    std::optional<CycleInfo> detected_cycle;
    std::optional<ExfiltrationPath> detected_exfiltration;
};

struct CycleInfo {
    std::vector<std::string> cycle_tools;
    uint32_t cycle_length;
    size_t cycle_start_index;
};

struct ExfiltrationPath {
    std::string source_node;
    std::string destination_node;
    std::vector<std::string> path;
};

struct ToolCall {
    std::string tool_name;
    std::string session_id;
    std::map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point timestamp;
};

enum class NodeType  { NORMAL, SENSITIVE_SOURCE, EXTERNAL_DESTINATION, DATA_PROCESSOR };
enum class RiskLevel { LOW, MEDIUM, HIGH, CRITICAL };
enum class LogLevel  { DEBUG, INFO, WARN, ERROR };
```

---

## HTTP Gateway API

The gateway (`gateway/main.cpp`) wraps the C++ library in an HTTP server so that any language can use AIGuardian over a local REST interface. Start it with:

```bash
./build/bin/guardian_gateway <policy-file> <port>
# e.g.
./build/bin/guardian_gateway policies/demo.json 8080
```

All responses are `application/json`. All endpoints send CORS headers (`Access-Control-Allow-Origin: *`) so browser clients can call them directly.

---

### `GET /health`

Returns `200 OK` when the gateway is up and the policy is loaded.

```json
{ "status": "ok", "policy": "policies/demo.json" }
```

---

### `POST /session`

Creates a new session and returns its ID.

**Request body:** none

**Response:**

```json
{ "session_id": "a1b2c3d4-..." }
```

---

### `DELETE /session/:id`

Ends the session with the given ID. Broadcasts a `session_killed` event to all SSE subscribers.

**Response on success:** `200` with `{ "ended": true }`

**Response on unknown ID:** `404`

---

### `POST /intercept`

The core enforcement endpoint. Validates whether the requested tool is permitted as the next call in the given session.

**Request body:**

```json
{
  "session_id": "a1b2c3d4-...",
  "tool": "send_email",
  "params": { "to": "user@example.com" }
}
```

`params` is optional. `session_id` and `tool` are required.

**Response — allowed:**

```json
{
  "allowed": true,
  "reason": "Transition approved",
  "alternatives": []
}
```

**Response — blocked:**

```json
{
  "allowed": false,
  "reason": "Transition from read_db to send_email is not permitted (exfiltration detected)",
  "alternatives": ["create_ticket", "request_approval"]
}
```

If a cycle was detected, the response also includes:

```json
"cycle": {
  "tools": ["search_kb", "search_kb", "search_kb"],
  "start_index": 0,
  "length": 3
}
```

If exfiltration was detected, the response also includes:

```json
"exfiltration": {
  "source": "read_db",
  "destination": "send_email",
  "path": ["read_db", "send_email"]
}
```

---

### `GET /audit/:id`

Returns the full call history for a session.

**Response:**

```json
{
  "session_id": "a1b2c3d4-...",
  "calls": [
    { "tool": "read_db",    "allowed": true,  "reason": "...", "timestamp": "..." },
    { "tool": "send_email", "allowed": false, "reason": "...", "timestamp": "..." }
  ]
}
```

---

### `GET /policy`

Returns a human-readable summary of the loaded policy (node count, edge count, node list).

---

### `GET /policy/json`

Returns the raw policy JSON exactly as loaded from disk. Useful for the frontend to render the policy graph without a separate file read.

```json
{
  "nodes": [ { "id": "read_db", "tool_name": "read_db", "risk_level": "HIGH", "node_type": "SENSITIVE_SOURCE" }, ... ],
  "edges": [ { "from": "read_db", "to": "create_ticket" }, ... ]
}
```

---

### `GET /events`

Server-Sent Events stream. Connect once; the gateway pushes a JSON event for every intercept decision and every session kill.

```
Content-Type: text/event-stream
```

**Intercept event:**

```
data: {"type":"intercept","session_id":"a1b2-...","from":"read_db","to":"send_email","allowed":false,"reason":"...","alternatives":["create_ticket"]}
```

**Session killed event:**

```
data: {"type":"session_killed","session_id":"a1b2-..."}
```

A heartbeat comment (`:\n\n`) is sent every 20 seconds to keep the connection alive through proxies.

---

### Error responses

| Status | Body | Meaning |
|---|---|---|
| `400` | `{ "error": "session_id and tool are required" }` | Missing required fields in `/intercept` |
| `404` | `{ "error": "session not found" }` | Unknown session ID |
| `500` | `{ "error": "..." }` | Internal error (e.g. policy parse failure) |


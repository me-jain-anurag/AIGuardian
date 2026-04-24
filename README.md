# AIGuardian

**C++17 | CMake | HTTP Gateway + SOC Dashboard Demo**

A runtime guard for AI agent tool calls — built by Team CodeGeass at a hackathon (2026). AIGuardian enforces safety constraints on tool usage by validating every call against a policy graph before execution.

> **[▶ Live Demo](https://aiguardian.onrender.com)** — click a scenario button and watch the policy graph animate decisions in real time.

---

## Key capabilities

| Capability | How it works |
|---|---|
| **Policy graph validator** | Every tool is a node; every permitted transition is an edge. Calls that don't follow the graph are blocked at runtime, reducing unsafe tool usage. |
| **Cycle & exfiltration detection** | Detects repeated tool-call streaks (agent stuck in a loop) and sensitive-source → external-destination paths with no data-processing step in between. |
| **HTTP gateway + SOC dashboard** | Exposes `/session` and `/intercept` endpoints. A browser-based dashboard renders the policy graph live — nodes pulse green/red as decisions are made. |

---

## How it works

You write a policy as a JSON file. Each node is a tool. Each edge is a permitted transition. Nodes are tagged by type: `SENSITIVE_SOURCE`, `DATA_PROCESSOR`, `NORMAL`, or `EXTERNAL_DESTINATION`.

At runtime, AIGuardian:

1. Checks that the requested tool exists in the policy.
2. Checks that the transition from the last tool to this one is allowed by an edge.
3. Checks whether a sensitive source is about to flow directly to an external destination without passing through a data processor (exfiltration detection).
4. Checks whether the same tool has been called too many times consecutively (cycle/DoS detection).

If any check fails, the call is blocked and a reason is returned. If all pass, the call proceeds into an optional WebAssembly sandbox.

---

## Use cases

**Financial data pipeline with an AI agent**

You have an agent that reads account data, generates reports, and emails them. The policy allows: `read_accounts -> generate_report -> encrypt -> send_email`. If the agent tries to call `send_email` directly after `read_accounts`, AIGuardian blocks it — the data has not been processed or encrypted.

**Code assistant with deployment access**

An agent can pull code, run tests, and request a deployment. The policy only permits `fetch_code -> run_tests -> request_deploy`. The agent cannot call `deploy_production` directly, even if it tries.

**Preventing runaway tool loops**

An agent gets stuck and calls `search_database` 50 times in a row. The policy sets `cycle_threshold: 5`. On the 6th consecutive call, AIGuardian blocks it and returns a cycle-detected error.

**Multi-tenant agent platform**

Each user session gets its own policy graph. Sessions are fully isolated at the `SessionManager` level.

---

## Quick start

### Prerequisites

- CMake 3.16 or higher
- A C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- Git

### Build

```bash
git clone https://github.com/me-jain-anurag/AIGuardian.git
cd AIGuardian
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Run tests

```bash
ctest --output-on-failure
```

---

## Live demo — SOC Dashboard

The fastest way to see AIGuardian in action is the HTTP gateway + browser dashboard.

### Start locally

```bash
# Start the gateway (serves both API and dashboard UI)
./build/bin/guardian_gateway policies/demo.json 8080
```

Open **`http://localhost:8080`** in any modern browser.

You will see a live policy-graph panel on the left and a control panel on the right. The graph shows the seven tools in `policies/demo.json` and the nine permitted transitions between them. Nodes pulse green or red in real time as decisions are made.

### What to try

| Action | What happens |
|---|---|
| Click **Persistent Exfiltration** | `read_db → send_email` is blocked three times in a row. The loop-detection banner fires on the third block, then the agent falls back to `create_ticket`. |
| Click **Shadow Deploy (learns)** | `read_code → deploy_hotfix` is blocked. The sequence recovers via `→ request_approval → deploy_hotfix → send_email`, all approved. |
| Click **Full Incident Response** | Five-step workflow: `search_kb → create_ticket → request_approval → deploy_hotfix → send_email`. Every step is green. |
| **Playbook Builder** — type a prompt | Type *"Read the database and email the results directly"*, click **Interpret**. The tool sequence is inferred from the text and then executed. The dashboard shows the block in real time. |
| **Playbook Builder** — click chips | Pick tools manually (colour-coded by risk level) and click **Run Playbook** to test any arbitrary sequence. |

See [Demo Guide](docs/demo_guide.md) for more worked examples.

---

## Integrating AIGuardian in your code

The main entry point is `guardian::Guardian`. Include it, point it at a policy file, and call `validate_tool_call` or `execute_tool` before each tool invocation.

```cpp
#include "guardian/guardian.hpp"

using namespace guardian;

// Load a policy
Guardian g("policies/financial.json");

// Create a session for this agent run
std::string session = g.create_session();

// Validate before calling a tool
auto result = g.validate_tool_call("read_accounts", session);
if (!result.approved) {
    std::cerr << "Blocked: " << result.reason << "\n";
    return;
}

// Or validate and execute in sandbox in one call
auto [validation, sandbox] = g.execute_tool(
    "generate_report",
    {{"account_id", "12345"}},
    session
);

// Clean up when the agent run is done
g.end_session(session);
```

For complete examples see the `examples/` directory:

- `examples/basic_integration.cpp` — minimal setup
- `examples/concurrent_sessions.cpp` — multiple parallel agent sessions
- `examples/custom_policy.cpp` — building a policy graph in code
- `examples/sandbox_config.cpp` — per-tool sandbox resource limits

---

## Writing a policy

Policies are JSON files. Full reference in [docs/policy_guide.md](docs/policy_guide.md).

```json
{
  "nodes": [
    { "id": "read_accounts",  "tool_name": "read_accounts",  "node_type": "SENSITIVE_SOURCE",     "risk_level": "HIGH" },
    { "id": "generate_report","tool_name": "generate_report","node_type": "DATA_PROCESSOR",        "risk_level": "LOW"  },
    { "id": "send_email",     "tool_name": "send_email",     "node_type": "EXTERNAL_DESTINATION",  "risk_level": "HIGH" }
  ],
  "edges": [
    { "from": "read_accounts",   "to": "generate_report" },
    { "from": "generate_report", "to": "send_email"      }
  ]
}
```

Node types:

| Type | Meaning |
|---|---|
| `NORMAL` | Generic tool with no special classification |
| `SENSITIVE_SOURCE` | Accesses private or protected data |
| `DATA_PROCESSOR` | Transforms or sanitizes data; breaks exfiltration detection |
| `EXTERNAL_DESTINATION` | Sends data outside the system (network, email, file storage) |

A direct path from `SENSITIVE_SOURCE` to `EXTERNAL_DESTINATION` with no `DATA_PROCESSOR` in between is always blocked, regardless of whether an edge exists.

---

## Architecture

```
Your code
    |
    v
Guardian                  main API — wires everything together
    |
    +-- SessionManager    per-session tool call history, thread-safe
    |
    +-- PolicyValidator   graph traversal, cycle and exfiltration checks, LRU cache
    |       |
    |       +-- PolicyGraph   adjacency list, node/edge storage
    |
    +-- ToolInterceptor   coordinates validator + sandbox per call
    |
    +-- SandboxManager    pluggable execution sandbox (MockRuntime / WasmEdge)
    |
    +-- VisualizationEngine   DOT and ASCII rendering of graphs and sequences
    |
    +-- Logger            structured JSON/text logging with level filtering
```

---

## Configuration

Beyond the policy graph, you can tune behaviour via a config file or the `Config` struct:

```json
{
  "cycle_detection": {
    "default_threshold": 5,
    "per_tool_thresholds": {
      "search_database": 3
    }
  },
  "sandbox": {
    "memory_limit_mb": 128,
    "timeout_ms": 5000,
    "network_access": false
  },
  "logging": {
    "level": "INFO",
    "output_file": "guardian.log",
    "json_format": true
  }
}
```

Pass it as a third argument to `Guardian`:

```cpp
Guardian g("policies/financial.json", "", "config.json");
```

---

## Visualizing a policy

```cpp
Guardian g("policies/financial.json");

// DOT format — pipe to Graphviz or paste into https://dreampuf.github.io/GraphvizOnline
std::cout << g.visualize_policy("dot");

// ASCII for terminals
std::cout << g.visualize_policy("ascii");
```

---

## Project structure

```
frontend/            SOC dashboard (single HTML file, no build step)
gateway/             HTTP gateway source (cpp-httplib, connects to Guardian core)
policies/            Example policy JSON files (demo.json, financial.json, …)
include/guardian/    Public API headers
src/                 Implementation
tests/unit/          Unit and property tests (Catch2 + RapidCheck)
tests/performance/   Benchmark tests
examples/            Standalone usage examples
docs/                Documentation
```

---

## Documentation

- [Demo Guide](docs/demo_guide.md) — live dashboard, scenarios, Playbook Builder
- [API Reference](docs/api.md) — C++ library API + HTTP Gateway REST API
- [Policy Authoring Guide](docs/policy_guide.md)
- [Contributing](CONTRIBUTING.md)
- [Roadmap](ROADMAP.md)

---

## Deployment

AIGuardian ships with a `Dockerfile` for one-command deployment:

```bash
docker build -t aiguardian .
docker run -p 8080:8080 aiguardian
```

The container runs the C++ gateway serving both the REST API and the SOC dashboard at `http://localhost:8080`.

Currently deployed on **[Render](https://aiguardian.onrender.com)** (free tier).

---

## License

MIT License. Copyright (c) 2026 CodeGeass Team.

## Team

Built by Team CodeGeass at a hackathon:

- **me-jain-anurag** — policy graph validator, cycle/exfiltration detection
- **Haruto1632**
- **Ao-chuba**
- **No-Reed**

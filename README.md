# AIGuardian

AIGuardian is a C++17 runtime guard for AI agent tool calls. You define which tools an agent is allowed to call, in what order, and under what conditions — AIGuardian enforces those rules at runtime and blocks anything that violates them.

It is not a firewall or a network-level control. It sits between your orchestration code and your tool executors, intercepts every call, validates it against a policy graph, and either approves or blocks it before execution.

---

## What problem does it solve

AI agents that use tools (file access, APIs, databases, code execution) can behave in unexpected ways — not because they are malicious, but because LLM reasoning is non-deterministic. Without a runtime guard:

- An agent can read sensitive data and immediately send it to an external service. No sanitization, no human review.
- An agent stuck in a loop can hammer an internal service thousands of times before anyone notices.
- An agent given broad tool access can chain calls in ways the developer never intended.

AIGuardian addresses this by requiring you to explicitly declare which tool transitions are permitted. Anything not in the policy is blocked.

---

## How it works

You write a policy as a JSON file. Each node is a tool. Each edge is a permitted transition between tools. You also tag each node by type: `SENSITIVE_SOURCE`, `DATA_PROCESSOR`, or `EXTERNAL_DESTINATION`.

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
- A C++17 compiler:
  - **Windows**: MSYS2 UCRT64 — see [Windows Setup](docs/WINDOWS_SETUP.md)
  - **Linux**: GCC 9+ or Clang 10+
- Git

### Build on Linux

```bash
git clone https://github.com/No-Reed/AIGuardian.git
cd AIGuardian
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Build on Windows (MSYS2 UCRT64)

```bash
git clone https://github.com/No-Reed/AIGuardian.git
cd AIGuardian
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
```

### Run tests

```bash
ctest --output-on-failure
```

All 13 test targets should pass. If executables are blocked by Windows Defender, see [Windows Setup](docs/WINDOWS_SETUP.md).

### Run the CLI demo

```bash
./bin/guardian_cli --scenario finance --policy-path ../policies/financial.json
./bin/guardian_cli --scenario dos    --policy-path ../policies/dos_prevention.json
```

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
    +-- SandboxManager    WebAssembly (WasmEdge/WASI) execution and resource limits
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
include/guardian/    Public API headers
src/                 Implementation
tests/unit/          Unit and property tests (Catch2 + RapidCheck)
tests/performance/   Benchmark tests
examples/            Standalone usage examples
policies/            Example policy JSON files
docs/                Documentation
```

---

## Documentation

- [API Reference](docs/api.md)
- [Policy Authoring Guide](docs/policy_guide.md)
- [Demo Scenario Guide](docs/demo_guide.md)
- [Windows Setup Guide](docs/WINDOWS_SETUP.md)
- [Contributing](CONTRIBUTING.md)

---

## License

MIT License. Copyright (c) 2026 CodeGeass Team.

## Team

- **me-jain-anurag**
- **Haruto1632**
- **Ao-chuba**
- **No-Reed**

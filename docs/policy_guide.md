# Policy Authoring Guide

Policies are JSON files that define what tools an agent is allowed to call and in what order. Each policy is a directed graph: nodes are tools, edges are permitted transitions.

---

## Minimal example

```json
{
  "nodes": [
    { "id": "read_file", "tool_name": "read_file", "node_type": "SENSITIVE_SOURCE",    "risk_level": "HIGH" },
    { "id": "process",   "tool_name": "process",   "node_type": "DATA_PROCESSOR",      "risk_level": "LOW"  },
    { "id": "upload",    "tool_name": "upload",     "node_type": "EXTERNAL_DESTINATION","risk_level": "HIGH" }
  ],
  "edges": [
    { "from": "read_file", "to": "process" },
    { "from": "process",   "to": "upload"  }
  ]
}
```

The agent can call `read_file`, then `process`, then `upload`. Any other sequence is blocked.

---

## Node fields

| Field | Required | Description |
|---|---|---|
| `id` | Yes | Unique identifier for this node in the graph. Must be unique within the policy. |
| `tool_name` | Yes | The name used when calling `validate_tool_call`. Usually the same as `id`. |
| `node_type` | Yes | One of `NORMAL`, `SENSITIVE_SOURCE`, `DATA_PROCESSOR`, `EXTERNAL_DESTINATION`. |
| `risk_level` | Yes | One of `LOW`, `MEDIUM`, `HIGH`, `CRITICAL`. Informational — used in visualization and logging. |
| `sandbox_config` | No | Per-tool sandbox resource limits (see below). |

### Node types

**`NORMAL`** — A regular tool. No special security treatment beyond edge enforcement.

**`SENSITIVE_SOURCE`** — A tool that reads private, confidential, or regulated data. Activates exfiltration tracking: once called, the validator watches for a direct path to an external destination.

**`DATA_PROCESSOR`** — A tool that transforms, redacts, encrypts, or otherwise sanitizes data. Calling this resets exfiltration tracking — it counts as a safe break between sensitive data and external output.

**`EXTERNAL_DESTINATION`** — A tool that sends data outside the system: network calls, email, file uploads, external APIs. If reached after a `SENSITIVE_SOURCE` with no `DATA_PROCESSOR` in between, the call is blocked.

---

## Edge fields

| Field | Required | Description |
|---|---|---|
| `from` | Yes | Node `id` of the source tool. |
| `to` | Yes | Node `id` of the destination tool. |

Only explicit edges are permitted. There is no wildcard or implicit "allow all" option.

A tool may have edges to itself (self-loop) to allow repeated calls. Without a self-loop, calling the same tool twice in a row is blocked.

---

## Sandbox config per node

Each node can have its own resource limits applied when executing inside the WebAssembly sandbox:

```json
{
  "id": "generate_report",
  "tool_name": "generate_report",
  "node_type": "DATA_PROCESSOR",
  "risk_level": "LOW",
  "sandbox_config": {
    "memory_limit_mb": 256,
    "timeout_ms": 5000,
    "network_access": false,
    "allowed_paths": ["/tmp/reports"]
  }
}
```

| Field | Default | Description |
|---|---|---|
| `memory_limit_mb` | 128 | Maximum memory the tool module may use. |
| `timeout_ms` | 5000 | Maximum execution time before the sandbox kills the module. |
| `network_access` | false | Whether the module may make outbound network calls. |
| `allowed_paths` | `[]` | Filesystem paths the module may read or write. |

---

## Cycle detection

By default, if the same tool is called more than 3 times **consecutively**, it is blocked. You can change this globally or per tool:

```json
{
  "cycle_detection": {
    "default_threshold": 5,
    "per_tool_thresholds": {
      "search_database": 2,
      "retry_tool": 10
    }
  }
}
```

This goes in the config file, not the policy file. See the main README for how to pass a config file.

Non-consecutive repetitions are not counted. `read_file -> process -> read_file -> process` does not trigger cycle detection for `read_file`.

---

## Exfiltration detection rules

The check is: does the current tool call sequence contain a `SENSITIVE_SOURCE` followed eventually by an `EXTERNAL_DESTINATION`, with no `DATA_PROCESSOR` in between?

- `read_db` (SENSITIVE) → `send_network` (EXTERNAL): **BLOCKED**
- `read_db` (SENSITIVE) → `transform` (PROCESSOR) → `send_network` (EXTERNAL): **ALLOWED**
- `read_db` (SENSITIVE) → `log_tool` (NORMAL) → `send_network` (EXTERNAL): **BLOCKED** — `NORMAL` does not reset the sensitive flag, only `DATA_PROCESSOR` does

---

## Live demo policy (policies/demo.json)

This is the policy loaded by the SOC Dashboard demo. It models an incident-response agent with seven tools across three columns: sources on the left, a gatekeeper in the middle, and destinations on the right.

```json
{
  "nodes": [
    { "id": "read_code",        "tool_name": "read_code",        "node_type": "SENSITIVE_SOURCE",     "risk_level": "MEDIUM"   },
    { "id": "read_db",          "tool_name": "read_db",          "node_type": "SENSITIVE_SOURCE",     "risk_level": "HIGH"     },
    { "id": "search_kb",        "tool_name": "search_kb",        "node_type": "NORMAL",               "risk_level": "LOW"      },
    { "id": "request_approval", "tool_name": "request_approval", "node_type": "DATA_PROCESSOR",       "risk_level": "LOW"      },
    { "id": "create_ticket",    "tool_name": "create_ticket",    "node_type": "NORMAL",               "risk_level": "LOW"      },
    { "id": "deploy_hotfix",    "tool_name": "deploy_hotfix",    "node_type": "EXTERNAL_DESTINATION", "risk_level": "CRITICAL" },
    { "id": "send_email",       "tool_name": "send_email",       "node_type": "EXTERNAL_DESTINATION", "risk_level": "CRITICAL" }
  ],
  "edges": [
    { "from": "read_code",        "to": "request_approval" },
    { "from": "read_db",          "to": "request_approval" },
    { "from": "read_db",          "to": "create_ticket"    },
    { "from": "search_kb",        "to": "create_ticket"    },
    { "from": "search_kb",        "to": "send_email"       },
    { "from": "create_ticket",    "to": "request_approval" },
    { "from": "request_approval", "to": "deploy_hotfix"    },
    { "from": "request_approval", "to": "send_email"       },
    { "from": "deploy_hotfix",    "to": "send_email"       }
  ]
}
```

**Key design decisions:**

- `read_db → send_email` is **not** an edge. Raw database records may never go directly to an external email. This is the exfiltration vector blocked in Scenario 1.
- `search_kb → send_email` **is** an edge. A low-risk knowledge-base summary is safe to send externally without approval. This contrast is intentional and makes a useful talking point.
- `request_approval` is the only `DATA_PROCESSOR` in the graph. Any sensitive source must flow through it before reaching an external destination.
- `create_ticket → request_approval` allows a filed ticket to escalate to human approval, enabling the full incident-response chain: `read_db → create_ticket → request_approval → deploy_hotfix → send_email`.

---

## Full financial policy example

This is the policy used in the finance demo. It models an agent that reads account data, can generate a report or encrypt it, and can then send it via email — but only after encryption.

```json
{
  "nodes": [
    {
      "id": "read_accounts",
      "tool_name": "read_accounts",
      "node_type": "SENSITIVE_SOURCE",
      "risk_level": "HIGH",
      "sandbox_config": { "memory_limit_mb": 64, "timeout_ms": 1000, "network_access": false }
    },
    {
      "id": "generate_report",
      "tool_name": "generate_report",
      "node_type": "DATA_PROCESSOR",
      "risk_level": "LOW",
      "sandbox_config": { "memory_limit_mb": 256, "timeout_ms": 5000, "allowed_paths": ["/tmp/reports"] }
    },
    {
      "id": "encrypt",
      "tool_name": "encrypt",
      "node_type": "DATA_PROCESSOR",
      "risk_level": "MEDIUM",
      "sandbox_config": { "memory_limit_mb": 128, "timeout_ms": 2000 }
    },
    {
      "id": "send_email",
      "tool_name": "send_email",
      "node_type": "EXTERNAL_DESTINATION",
      "risk_level": "CRITICAL",
      "sandbox_config": { "memory_limit_mb": 32, "timeout_ms": 5000, "network_access": true }
    }
  ],
  "edges": [
    { "from": "read_accounts",   "to": "generate_report" },
    { "from": "read_accounts",   "to": "encrypt"         },
    { "from": "generate_report", "to": "encrypt"         },
    { "from": "encrypt",         "to": "send_email"      }
  ]
}
```

The agent **cannot** go from `read_accounts` directly to `send_email` — there is no edge, and even if there were, exfiltration detection would block it.

---

## Tips

- Keep policies narrow. Only add edges you explicitly intend to permit.
- Use `DATA_PROCESSOR` nodes for any step that redacts, encrypts, aggregates, or otherwise transforms sensitive data before it leaves.
- Give external tools (`EXTERNAL_DESTINATION`) tight sandbox limits: low memory, short timeout, no filesystem access.
- Use `risk_level` consistently — it shows up in visualization and logs and helps with auditing.

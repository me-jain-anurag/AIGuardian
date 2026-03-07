# AIGuardian Policy Authoring Guide

Policies in AIGuardian are defined as directed graphs stored in JSON format. Each node represents a tool, and each edge represents a permitted transition between tools.

## JSON Format Example

```json
{
  "nodes": [
    {
      "id": "read_db",
      "tool_name": "read_db",
      "risk_level": "HIGH",
      "node_type": "SENSITIVE_SOURCE"
    },
    {
      "id": "send_network",
      "tool_name": "send_network",
      "risk_level": "HIGH",
      "node_type": "EXTERNAL_DESTINATION"
    }
  ],
  "edges": [
    {
      "from": "read_db",
      "to": "read_db"
    }
  ]
}
```

## Node Types
- `NORMAL`: Generic tool call.
- `SENSITIVE_SOURCE`: Tools that access private or protected data.
- `EXTERNAL_DESTINATION`: Tools that send data out of the system.
- `DATA_PROCESSOR`: Tools that can sanitize or transform data.

## Edge Rules
- A transition is **ALLOWED** only if an edge exists between the current tool and the next tool.
- A path from a `SENSITIVE_SOURCE` to an `EXTERNAL_DESTINATION` is **BLOCKED** unless it passes through at least one `DATA_PROCESSOR`.
- If an agent repeats a tool call in a cycle that exceeds the `cycle_threshold`, it is **BLOCKED** as a potential DoS attack.

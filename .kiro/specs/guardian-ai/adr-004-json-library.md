# ADR-004: nlohmann_json for JSON Processing

**Status:** Accepted  
**Date:** 2026-03-07  
**Context:** Guardian AI needs JSON parsing/serialization for policy graphs, configuration files, audit logs, and Wasm parameter passing.

## Decision

**Use nlohmann/json** (nlohmann_json) for all JSON operations.

## Alternatives Considered

| Library | API Style | Performance | Header-Only | STL Integration | Adoption |
|---------|----------|-------------|-------------|-----------------|----------|
| **nlohmann/json** | Intuitive (STL-like) | Good | ✅ Yes | ✅ Excellent | Most popular C++ JSON lib |
| **RapidJSON** | DOM/SAX | ✅ Fastest | ✅ Yes | ❌ Custom types | Widely used |
| **simdjson** | Read-only | ✅ Fastest (SIMD) | ❌ No | ❌ Custom types | High-perf only |
| **Boost.JSON** | Boost-style | Good | ❌ No (Boost dep) | ⚠️ Boost types | Boost ecosystem |
| **json-c** | C API | Good | ❌ No | ❌ C only | Legacy |

## Rationale

### Why nlohmann/json

**1. Intuitive API matches Guardian AI's data models**

Policy graph serialization becomes natural C++:
```cpp
nlohmann::json j;
j["id"] = node.id;
j["tool_name"] = node.tool_name;
j["risk_level"] = to_string(node.risk_level);
j["node_type"] = to_string(node.node_type);
j["sandbox_config"] = {
    {"memory_limit_mb", config.memory_limit_mb},
    {"timeout_ms", config.timeout_ms}
};
```

RapidJSON equivalent requires significantly more boilerplate with allocators and value manipulation.

**2. Round-trip fidelity (critical requirement)**

Requirements 1.9 and 15.7 demand serialization round-trip correctness. nlohmann/json preserves:
- Key ordering (via `ordered_json`)
- Exact numeric types
- Unicode strings
- Nested structures

**3. Header-only distribution**

Single `#include <nlohmann/json.hpp>` — no additional build steps. Perfect for a library that aims to minimize dependencies.

**4. Excellent error messages**

Policy file parsing errors need to be descriptive (Requirement 10.3). nlohmann/json throws `json::parse_error` with line/column info:
```
[json.exception.parse_error.101] parse error at line 15, column 8: unexpected '}'
```

### Why not RapidJSON?

RapidJSON is 2-5x faster, but Guardian AI's JSON workloads are tiny:
- Policy files: <100KB, parsed once at startup
- Config files: <1KB, parsed once
- Audit logs: serialized periodically, not in hot path

The <10ms validation target is for *graph traversal*, not JSON parsing. API ergonomics matter more here.

### Why not simdjson?

simdjson is read-only — it cannot serialize. Guardian AI needs both parsing and generation for policy export, audit logs, and config round-trips.

## Consequences

- Single header dependency, easy to vendor if needed
- JSON parsing is not in the hot path (policy loaded once at startup)
- `ordered_json` used where key ordering matters (policy files, config)
- Error messages include line/column for invalid policy files

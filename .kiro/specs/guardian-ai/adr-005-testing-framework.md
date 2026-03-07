# ADR-005: Testing Framework Selection

**Status:** Accepted  
**Date:** 2026-03-07  
**Context:** Guardian AI requires both unit testing and property-based testing. The design specifies 49 correctness properties and comprehensive unit test coverage.

## Decision

**Use Catch2 for unit tests and RapidCheck for property-based tests.**

## Alternatives Considered

### Unit Testing Frameworks

| Framework | API Style | Header-Only | CMake Support | Maturity | Features |
|-----------|----------|-------------|---------------|----------|----------|
| **Catch2** | BDD-style | ✅ Yes (v2) | ✅ Excellent | ✅ Very mature | Sections, generators, benchmarks |
| **Google Test** | xUnit-style | ❌ No | ✅ Excellent | ✅ Very mature | Mocking (GMock), parameterized |
| **doctest** | BDD-style | ✅ Yes | ✅ Good | ✅ Mature | Fastest compilation |
| **Boost.Test** | Boost-style | ❌ No | ⚠️ Via Boost | ✅ Very mature | Full Boost ecosystem |

### Property-Based Testing Frameworks

| Framework | Language | Integration | Shrinking | Maturity |
|-----------|----------|------------|-----------|----------|
| **RapidCheck** | C++ | Catch2/GTest | ✅ Automatic | ✅ Mature |
| **CppQuickCheck** | C++ | Standalone | ⚠️ Basic | ⚠️ Less maintained |
| **autocheck** | C++ | Standalone | ✅ Good | ⚠️ Small community |

## Rationale

### Why Catch2 over Google Test

**1. BDD-style sections match Guardian AI's scenarios**

Demo scenarios (financial, developer, DoS) naturally map to Catch2's `SECTION` blocks:
```cpp
TEST_CASE("Financial exfiltration demo") {
    Guardian guardian("policies/financial.json", "./wasm_tools");
    auto session = guardian.create_session();

    SECTION("blocks direct exfiltration") {
        guardian.execute_tool("read_accounts", {{"id", "12345"}}, session);
        auto result = guardian.execute_tool("send_email", {{"to", "evil@example.com"}}, session);
        REQUIRE(!result.success);
    }

    SECTION("allows safe path") {
        guardian.execute_tool("read_accounts", {{"id", "12345"}}, session);
        guardian.execute_tool("generate_report", {}, session);
        guardian.execute_tool("encrypt", {}, session);
        auto result = guardian.execute_tool("send_email", {{"to", "user@example.com"}}, session);
        REQUIRE(result.success);
    }
}
```

Google Test's `TEST_F` fixtures are more verbose for this pattern.

**2. Header-only option simplifies build**

Catch2 v2 can be included as a single header, reducing dependency management. (Catch2 v3 is a compiled library but still easy via CMake `FetchContent`.)

**3. Built-in micro-benchmarking**

Catch2's `BENCHMARK` macro can validate performance requirements (<10ms validation) without adding Google Benchmark as another dependency.

### Why RapidCheck

**Only serious C++ property-based testing library.**

RapidCheck is the only option with:
- ✅ Automatic shrinking (find minimal failing case)
- ✅ Catch2 integration (`rc::check` inside `TEST_CASE`)
- ✅ Custom generators for complex types (PolicyGraph, ToolCall, etc.)
- ✅ 100+ iteration support per property

```cpp
#include <rapidcheck.h>

TEST_CASE("Property 4: Serialization Round-Trip") {
    rc::check("serialize then deserialize produces equivalent graph",
              [](const PolicyGraph& graph) {
        auto json = graph.to_json();
        auto restored = PolicyGraph::from_json(json);
        RC_ASSERT(graphs_equivalent(graph, restored));
    });
}
```

No other C++ library provides this combination.

## Consequences

- Catch2 v3 installed via CMake `FetchContent` or system package
- RapidCheck installed via CMake `FetchContent` (not commonly packaged)
- Custom RapidCheck generators needed for `PolicyGraph`, `ToolCall`, `SandboxConfig`
- Property tests marked with `*` in tasks.md (optional for faster MVP iteration)
- Performance benchmarks use Catch2's `BENCHMARK` macro

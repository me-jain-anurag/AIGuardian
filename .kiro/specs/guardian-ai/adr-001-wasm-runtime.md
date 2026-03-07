# ADR-001: WasmEdge as Wasm Runtime

**Status:** Accepted  
**Date:** 2026-03-07  
**Context:** Guardian AI needs a WebAssembly runtime for sandboxed tool execution.

## Decision

**Use WasmEdge** as the Wasm runtime for Guardian AI's sandbox execution layer.

## Key Insight

Guardian AI *secures* AI agents — it doesn't run AI models. The Wasm runtime is used solely to sandbox tool execution with enforced resource constraints. AI/ML capabilities (WASI-NN, Tensorflow) are irrelevant.

## Alternatives Considered

| Runtime | Language | C++ Integration | Wall-Clock Timeout | WASI | Backing |
|---------|----------|-----------------|-------------------|------|---------|
| **WasmEdge** | C/C++ | ✅ Native C++ SDK | ✅ Built-in | ✅ Full | CNCF |
| **Wasmtime** | Rust | ⚠️ C API wrapper | ⚠️ Epoch-based | ✅ Full | Bytecode Alliance |
| **Wasmer** | Rust | ⚠️ C API wrapper | ⚠️ Metering | ✅ Full | Commercial |
| **WAMR** | C | ✅ Native C | ⚠️ External | ⚠️ Partial | Intel |

## Rationale

### Why WasmEdge over Wasmtime (the closest alternative)

**1. Built-in wall-clock timeout (deal-breaker)**

Guardian AI's `SandboxConfig` specifies `timeout_ms` as wall-clock time. WasmEdge provides this natively:

```cpp
// WasmEdge: one line
configure.set_timeout(5000); // 5s wall-clock, automatic interruption
```

Wasmtime uses fuel/epoch-based mechanisms that measure *instructions*, not wall-clock time. A tool making a slow network call burns few instructions but lots of real time — fuel won't catch it. Building a reliable external timeout (timer thread + VM interrupt) is error-prone in C++.

**2. Native C++ API**

Guardian AI is a pure C++ library. WasmEdge's C++ SDK integrates directly without FFI overhead. Wasmtime requires linking against the Rust standard library (~5MB+ binary bloat) and wrapping a C API.

**3. Better resource monitoring APIs**

WasmEdge exposes memory usage and execution metrics that Guardian needs for audit logging and `SandboxResult` reporting.

**4. Production-ready**

WasmEdge is a CNCF project used in production by Second State, Dapr, and WasmCloud. Security audits have been completed.

### Where Wasmtime is stronger (and why it doesn't matter here)

| Wasmtime Advantage | Impact on Guardian AI |
|-------------------|----------------------|
| Stricter standards compliance | Low — WASI preview 1 (both support) is sufficient |
| Formal verification efforts | Low — Guardian sandboxes simple tools, not adversarial Wasm |
| Larger community | Low — WasmEdge's community is adequate |
| WASI preview 2 / Component Model | None — not needed for tool sandboxing |

## Mitigations

**API instability:** Pin WasmEdge to a specific version (`0.14.x`) in CMake.

**Runtime abstraction:** Implement `IWasmRuntime` interface from day one:

```cpp
class IWasmRuntime {
public:
    virtual ~IWasmRuntime() = default;
    virtual SandboxResult execute(const std::string& module,
                                  const std::map<std::string, std::string>& params,
                                  const SandboxConfig& config) = 0;
};

class WasmEdgeRuntime : public IWasmRuntime { /* production */ };
class MockRuntime : public IWasmRuntime { /* unit testing */ };
```

This enables switching runtimes later and makes unit tests fast (no `.wasm` files needed).

## Consequences

- All Wasm tooling must target WasmEdge's supported WASI features
- Build system requires WasmEdge SDK as a dependency
- CI must install WasmEdge (or use `MockRuntime` for fast tests)
- If WasmEdge's C++ API changes between versions, updates may require code changes

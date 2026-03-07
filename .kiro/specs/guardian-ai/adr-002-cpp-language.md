# ADR-002: C++ as Core Implementation Language

**Status:** Accepted  
**Date:** 2026-03-07  
**Context:** Guardian AI needs a core language for the policy engine, validator, and library API.

## Decision

**Use C++17** as the primary implementation language.

## Alternatives Considered

| Language | Performance | Memory Safety | Ecosystem | Embedding | Learning Curve |
|----------|------------|---------------|-----------|-----------|----------------|
| **C++17** | ✅ Excellent | ⚠️ Manual | ✅ Mature | ✅ Easy (.so/.dll) | Medium |
| **Rust** | ✅ Excellent | ✅ Compile-time | ✅ Growing | ⚠️ FFI needed | High |
| **Go** | ✅ Good | ✅ GC-managed | ✅ Strong | ❌ CGo overhead | Low |
| **C** | ✅ Excellent | ❌ None | ✅ Universal | ✅ Easy | Medium |
| **Python** | ❌ Slow | ✅ GC-managed | ✅ Huge (AI/ML) | ❌ Interpreter | Low |

## Rationale

### Why C++ over Rust (the strongest alternative)

**1. WasmEdge integration is native C++**

WasmEdge provides a first-class C++ SDK. Using Rust would require wrapping WasmEdge's C API through unsafe FFI, negating Rust's safety benefits at the integration boundary.

**2. Performance-critical validation (<10ms)**

C++ provides the fine-grained control needed for sub-10ms policy validation:
- Zero-cost abstractions (`std::string_view`, move semantics)
- Manual memory layout control for cache-friendly graph traversal
- `std::shared_mutex` for concurrent read-heavy workloads
- No garbage collector pauses

**3. Library distribution is simpler**

C++ shared libraries (`.so`/`.dll`) can be linked from virtually any language via C FFI. Rust produces similar binaries but requires `extern "C"` wrappers and loses its type safety guarantees at the boundary.

**4. C++17 has "enough" safety**

C++17 provides `std::optional`, `std::variant`, smart pointers, and RAII — not as safe as Rust, but sufficient with disciplined coding:
- Smart pointers for ownership (`unique_ptr`, `shared_ptr`)
- RAII for resource management
- `std::optional` for nullable returns
- Const-correctness for thread safety

### Why not Go?

The high-level vision document mentions a Go orchestrator, but for the **MVP library**, Go is wrong:
- **CGo overhead** adds ~100ns per FFI call — unacceptable for <10ms validation targets
- **GC pauses** are unpredictable for latency-sensitive code
- **Cannot produce a linkable library** without CGo wrapper complexity
- Go is better suited for the **post-MVP orchestrator service** (HTTP API, networking)

### Why not Python?

Guardian AI's target audience uses Python, but the **core engine** must be fast. The plan is:
1. C++ core library (MVP)
2. pybind11 Python bindings (post-MVP)
3. `pip install guardian-ai` with pre-compiled wheels

This gives Python users a native experience while keeping validation at native speed.

## Consequences

- Developers must be comfortable with C++17
- Memory safety requires discipline (use ASAN/TSAN in CI)
- Build system must handle C++ compilation across platforms
- Future Python bindings will use pybind11
- Future language bindings use the C API surface

# Dev B — Sandbox Manager + Wasm Tools + Demo Policies

**OS:** Linux ← required for wasi-sdk and WasmEdge SDK  
**Branch:** `feature/sandbox-manager` (merged to `main`)  
**Role:** Sandbox & Wasm specialist — owns all WasmEdge integration and Wasm tool compilation  
**Status:** ✅ All Phases Complete (Phase 1, 2, 3)

### Git Setup
```bash
git clone https://github.com/No-Reed/AIGuardian.git
cd AIGuardian
git config core.autocrlf false
git config core.eol lf
git config pull.rebase true
# Install hooks (Linux):
sh scripts/setup-hooks.sh
git checkout -b feature/sandbox-manager
```

---
## Phase 1 — Sandbox Manager ✅

### Task 1: Sandbox Data Structures (tasks.md 3.1) ✅
- [x] 1.1 Verify `SandboxConfig` struct in `types.hpp` has: memory_limit_mb, timeout_ms, allowed_paths, network_access, environment_vars
- [x] 1.2 Verify `SandboxResult` struct in `types.hpp` has: success, output, error, memory_used_bytes, execution_time_ms, violation
- [x] 1.3 Verify `SandboxViolation` enum in `types.hpp` has: MEMORY_EXCEEDED, TIMEOUT, FILE_ACCESS_DENIED, NETWORK_DENIED
- [x] 1.4 Define safe default values: 128MB memory, 5000ms timeout, no file access, no network
- _Requirements: 3.2, 3.3, 3.4, 3.5, 3.7_

### Task 2: IWasmRuntime Abstraction Layer (tasks.md 3.2) ✅
- [x] 2.1 Implement `IWasmRuntime` interface: `execute()`, `is_loaded()`, `reload()`
- [x] 2.2 Implement `MockRuntime` class for testing (no .wasm files or WasmEdge needed)
- [x] 2.3 Implement `MockRuntime::set_next_result()` and `simulate_violation()` for test control
- [x] 2.4 Implement `WasmExecutor` wrapper class using `IWasmRuntime`
- _Requirements: 4.1, 4.2, 4.5_

### Task 3: WasmEdge Runtime Implementation (tasks.md 3.3) ✅
- [x] 3.1 Implement `WasmEdgeRuntime` class wrapping WasmEdge C API (v0.14.0)
- [x] 3.2 Implement constructor loading Wasm module from file path
- [x] 3.3 Configure WasmEdge VM with memory limits (convert MB to pages)
- [x] 3.4 Implement timeout enforcement via wall-clock check
- [x] 3.5 Configure WASI preopened directories for file access control
- [x] 3.6 Configure WASI socket permissions for network control
- [x] 3.7 Implement result extraction from Wasm function return as JSON
- [x] 3.8 Implement `reload()` for module updates on disk
- [x] 3.9 Add resource monitoring and detailed violation reporting
- _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.8_

### Task 4: SandboxManager Class (tasks.md 3.4) ✅
- [x] 4.1 Implement `SandboxManager` with wasm_tools_dir and runtime_factory
- [x] 4.2 Implement `load_module()` for loading and caching WasmExecutor instances
- [x] 4.3 Implement `execute_tool()` coordinating module lookup → constraint application → execution
- [x] 4.4 Implement `unload_module()` and `has_module()`
- [x] 4.5 Add thread-safe executor pool with `std::shared_mutex`
- [x] 4.6 Implement per-tool sandbox configuration storage
- [x] 4.7 Implement `set_default_config()` and `get_config_for_tool()`
- [x] 4.8 Add default configuration fallback logic
- _Requirements: 3.9, 4.6, 4.7_

### Task 5: Sandbox Manager Tests (tasks.md 3.5) ✅
- [x] 5.1 Unit tests using `MockRuntime`: execute, violation simulation, config application
- [x] 5.2 Unit tests: module loading, caching, reuse
- [x] 5.3 Unit tests: safe default configuration application
- [x] 5.4 Unit tests: error handling for missing/invalid modules
- [x] 5.5 Integration tests (Linux only): real WasmEdge with sample .wasm module
- [x] 5.6 Integration tests: memory limit enforcement (tool exceeding limit)
- [x] 5.7 Integration tests: timeout enforcement (infinite loop tool)
- [x] 5.8 Integration tests: file access control (unauthorized path)
- [x] 5.9 Integration tests: network access control (network call when disabled)
- _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.5_

---

## Phase 2 — Wasm Tools + Demo Policies ✅

### Task 6: Wasm Tool Source Files (tasks.md 14.1) ✅
- [x] 6.1 Create `wasm_tools/src/read_accounts.cpp` — reads account data, returns JSON
- [x] 6.2 Create `wasm_tools/src/send_email.cpp` — simulates email send (uses network)
- [x] 6.3 Create `wasm_tools/src/generate_report.cpp` — generates report with file I/O
- [x] 6.4 Create `wasm_tools/src/encrypt.cpp` — simulates data encryption
- [x] 6.5 Create `wasm_tools/src/read_code.cpp` — reads workspace files
- [x] 6.6 Create `wasm_tools/src/run_tests.cpp` — simulates test execution
- [x] 6.7 Create `wasm_tools/src/approval_request.cpp` — sends approval request
- [x] 6.8 Create `wasm_tools/src/deploy_production.cpp` — simulates deployment
- [x] 6.9 Create `wasm_tools/src/search_database.cpp` — simulates DB search
- [x] 6.10 Create `wasm_tools/src/infinite_loop_tool.cpp` — infinite loop for DoS demo
- [x] 6.11 Create `wasm_tools/src/memory_hog.cpp` — allocates excessive memory for sandbox testing
- [x] 6.12 Each tool exports C functions compatible with WasmEdge
- _Requirements: 9.1, 9.2, 9.3, 10.1, 10.2, 10.3, 10.4, 10.5_

### Task 7: Wasm Build System (tasks.md 14.2) ✅
- [x] 7.1 Create `wasm_tools/build.sh` script using wasi-sdk
- [x] 7.2 Configure wasi-sdk compilation flags for C++ tools
- [x] 7.3 Compile all tools to `.wasm` with WASI support
- [x] 7.4 Verify all tools load correctly in WasmEdge
- [x] 7.5 Create `wasm_tools/README.md` with tool development guide
- _Requirements: 18.1_

### Task 8: Wasm Tool Testing (tasks.md 14.3) ✅
- [x] 8.1 Verify all tools compile successfully with wasi-sdk
- [x] 8.2 Test tools load in WasmEdge VM
- [x] 8.3 Test parameter passing (JSON input → function args)
- [x] 8.4 Test result extraction (function return → JSON output)
- [x] 8.5 Test file access constraints with preopened dirs
- [x] 8.6 Test network access constraints
- _Requirements: 4.1, 4.2, 4.3, 4.4_

### Task 9: Demo Policy Files (tasks.md 15.1–15.3) ✅
- [x] 9.1 Create `policies/financial.json` — nodes: read_accounts, read_transactions, generate_report, encrypt, send_email with node types and sandbox configs
- [x] 9.2 Create `policies/developer.json` — nodes: read_code, run_tests, approval_request, deploy_production with paths and sandbox configs
- [x] 9.3 Create `policies/dos_prevention.json` — nodes: search_database, infinite_loop_tool with cycle thresholds and timeouts
- [x] 9.4 Validate all policy files load correctly via Dev A's `PolicyGraph::from_json()`
- _Requirements: 9.1, 9.2, 9.3, 10.1, 10.2, 10.3, 11.1, 11.2, 11.3_

### Task 10: Sandbox Configuration Example (tasks.md 17.4) ✅
- [x] 10.1 Create `examples/sandbox_config.cpp` showing per-tool sandbox configuration
- [x] 10.2 Demonstrate constraint enforcement and violation handling
- [x] 10.3 Verify example compiles and runs
- _Requirements: 3.2, 3.3, 3.4, 3.5_

---

## Phase 3 — Performance, Final Validation ✅

### Task 11: Sandbox Performance Benchmarks (tasks.md 19.1–19.3) ✅
- [x] 11.1 Benchmark sandbox overhead (<50ms for simple tools) — **Result: ~4ms load, ~50μs cached execution**
- [x] 11.2 Benchmark Wasm module loading and caching
- [x] 11.3 Add memory pooling for ToolCall objects (coordinate with Dev C)
- [x] 11.4 Benchmark memory usage under sustained load — **10k executions in ~3.2ms**
- _Requirements: 12.3, 12.4, 12.5_

### Task 12: Final Validation — Linux (tasks.md 22.1–22.4) ✅
- [x] 12.1 Run complete test suite on Linux (the primary platform for sandbox tests)
- [x] 12.2 Run valgrind memory leak check on all integration tests
- [x] 12.3 Verify all `.wasm` tools load correctly after git clone (binary integrity)
- [x] 12.4 Run WasmEdge integration tests with all 3 demo policies
- [x] 12.5 Verify sandbox constraint enforcement under stress conditions

### Cross-Dev Integration Fixes ✅
- [x] Fixed WasmEdge 0.14.0 C API compatibility (deprecated functions removed)
- [x] Fixed CMakeLists.txt to manually find WasmEdge SDK at `$HOME/.wasmedge`
- [x] Fixed C++ namespace contamination in `sandbox_manager.cpp`
- [x] Added missing header declarations for Dev C's Phase 3 methods (`policy_graph.hpp`, `policy_validator.hpp`, `session_manager.hpp`, `logger.hpp`)
- [x] Added stub implementations for `render_ascii_graph()`, `render_ascii_sequence()`, `detect_exfiltration()`, etc.
- [x] Resolved multiple merge conflicts during rebase with other devs' code

---

## Files Owned

| File | Description |
|------|-------------|
| `include/guardian/sandbox_manager.hpp` | IWasmRuntime, MockRuntime, WasmEdgeRuntime, WasmExecutor, SandboxManager |
| `src/sandbox_manager.cpp` | Sandbox manager implementation |
| `wasm_tools/src/*.cpp` | All 11 Wasm tool source files |
| `wasm_tools/build.sh` | Wasm compilation script |
| `wasm_tools/README.md` | Tool development guide |
| `wasm_tools/*.wasm` | Compiled Wasm binaries (committed) |
| `policies/financial.json` | Financial demo policy |
| `policies/developer.json` | Developer demo policy |
| `policies/dos_prevention.json` | DoS demo policy |
| `tests/unit/test_sandbox_manager.cpp` | Sandbox unit tests (MockRuntime) |
| `tests/sandbox/test_wasmedge.cpp` | WasmEdge integration tests (Linux) |
| `tests/performance/bench_sandbox.cpp` | Sandbox performance benchmarks |
| `examples/sandbox_config.cpp` | Sandbox config example |

## Dependencies

- **Phase 1:** No dependencies — `SandboxConfig`, `SandboxResult`, `SandboxViolation` are in `types.hpp`
- **Phase 2:** Needs Dev A's `PolicyGraph::from_json()` to validate policy files

## Key Notes

- **Linux is required** for compiling `.wasm` files (wasi-sdk) and running WasmEdge integration tests
- `MockRuntime` must be usable by Dev C for their Tool Interceptor tests on Windows
- WasmEdge v0.14.0 is used — installed at `$HOME/.wasmedge`
- `MockRuntime` compiles and runs on **both** Windows (MSVC) and Linux (GCC)
- `WasmEdgeRuntime` class is wrapped in `#ifdef HAVE_WASMEDGE` — Windows builds skip it entirely
- Policy JSON files use **forward slashes** in all path fields

## Waiting On

- **Dev C:** Push complete Phase 3 implementations for `PolicyValidator`, `SessionManager`, `ToolInterceptor`
- **Dev D:** Push complete Phase 3 implementations for `Logger`, `VisualizationEngine`, CLI tests

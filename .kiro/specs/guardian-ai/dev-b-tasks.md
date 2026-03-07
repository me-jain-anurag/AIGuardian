# Dev B — Sandbox Manager + Wasm Tools + Demo Policies

**OS:** Linux ← required for wasi-sdk and WasmEdge SDK  
**Branch:** `feature/sandbox-manager`  
**Role:** Sandbox & Wasm specialist — owns all WasmEdge integration and Wasm tool compilation

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
<!--
---
## Phase 1 — Sandbox Manager

### Task 1: Sandbox Data Structures (tasks.md 3.1)
- [ ] 1.1 Verify `SandboxConfig` struct in `types.hpp` has: memory_limit_mb, timeout_ms, allowed_paths, network_access, environment_vars
- [ ] 1.2 Verify `SandboxResult` struct in `types.hpp` has: success, output, error, memory_used_bytes, execution_time_ms, violation
- [ ] 1.3 Verify `SandboxViolation` enum in `types.hpp` has: MEMORY_EXCEEDED, TIMEOUT, FILE_ACCESS_DENIED, NETWORK_DENIED
- [ ] 1.4 Define safe default values: 128MB memory, 5000ms timeout, no file access, no network
- _Requirements: 3.2, 3.3, 3.4, 3.5, 3.7_

### Task 2: IWasmRuntime Abstraction Layer (tasks.md 3.2)
- [ ] 2.1 Implement `IWasmRuntime` interface: `execute()`, `is_loaded()`, `reload()`
- [ ] 2.2 Implement `MockRuntime` class for testing (no .wasm files or WasmEdge needed)
- [ ] 2.3 Implement `MockRuntime::set_next_result()` and `simulate_violation()` for test control
- [ ] 2.4 Implement `WasmExecutor` wrapper class using `IWasmRuntime`
- _Requirements: 4.1, 4.2, 4.5_

> **📌 COMMIT 1 — Runtime abstraction + MockRuntime**
> ```bash
> git add include/guardian/sandbox_manager.hpp src/sandbox_manager.cpp
> git commit -m "feat(sandbox): implement IWasmRuntime interface and MockRuntime
>
> - IWasmRuntime abstraction for testability
> - MockRuntime with set_next_result/simulate_violation
> - WasmExecutor wrapper class"
> git push -u origin feature/sandbox-manager
> ```
> ⚠️ **Push MockRuntime early** — Dev C needs it for interceptor tests on Windows.

### Task 3: WasmEdge Runtime Implementation (tasks.md 3.3)
- [ ] 3.1 Implement `WasmEdgeRuntime` class wrapping WasmEdge C++ SDK
- [ ] 3.2 Implement constructor loading Wasm module from file path
- [ ] 3.3 Configure WasmEdge VM with memory limits (convert MB to pages)
- [ ] 3.4 Implement timeout enforcement via `configure.set_timeout(ms)`
- [ ] 3.5 Configure WASI preopened directories for file access control
- [ ] 3.6 Configure WASI socket permissions for network control
- [ ] 3.7 Implement result extraction from Wasm function return as JSON
- [ ] 3.8 Implement `reload()` for module updates on disk
- [ ] 3.9 Add resource monitoring and detailed violation reporting
- _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.8_

> **📌 COMMIT 2 — WasmEdge runtime**
> ```bash
> git add include/guardian/sandbox_manager.hpp src/sandbox_manager.cpp
> git commit -m "feat(sandbox): implement WasmEdgeRuntime with constraint enforcement
>
> - Memory limits, timeout, file access, network control
> - Wrapped in #ifdef HAVE_WASMEDGE for Windows compat"
> ```

### Task 4: SandboxManager Class (tasks.md 3.4)
- [ ] 4.1 Implement `SandboxManager` with wasm_tools_dir and runtime_factory
- [ ] 4.2 Implement `load_module()` for loading and caching WasmExecutor instances
- [ ] 4.3 Implement `execute_tool()` coordinating module lookup → constraint application → execution
- [ ] 4.4 Implement `unload_module()` and `has_module()`
- [ ] 4.5 Add thread-safe executor pool with `std::shared_mutex`
- [ ] 4.6 Implement per-tool sandbox configuration storage
- [ ] 4.7 Implement `set_default_config()` and `get_config_for_tool()`
- [ ] 4.8 Add default configuration fallback logic
- _Requirements: 3.9, 4.6, 4.7_

> **📌 COMMIT 3 — SandboxManager class**
> ```bash
> git add include/guardian/sandbox_manager.hpp src/sandbox_manager.cpp
> git commit -m "feat(sandbox): implement SandboxManager with thread-safe executor pool
>
> - Module loading/caching, per-tool config, default fallbacks"
> ```

### Task 5: Sandbox Manager Tests (tasks.md 3.5)
- [ ] 5.1 Unit tests using `MockRuntime`: execute, violation simulation, config application
- [ ] 5.2 Unit tests: module loading, caching, reuse
- [ ] 5.3 Unit tests: safe default configuration application
- [ ] 5.4 Unit tests: error handling for missing/invalid modules
- [ ] 5.5 Integration tests (Linux only): real WasmEdge with sample .wasm module
- [ ] 5.6 Integration tests: memory limit enforcement (tool exceeding limit)
- [ ] 5.7 Integration tests: timeout enforcement (infinite loop tool)
- [ ] 5.8 Integration tests: file access control (unauthorized path)
- [ ] 5.9 Integration tests: network access control (network call when disabled)
- _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.5_

> **📌 COMMIT 4 — Sandbox tests**
> ```bash
> git add tests/unit/test_sandbox_manager.cpp tests/sandbox/test_wasmedge.cpp
> git commit -m "test(sandbox): add unit tests (MockRuntime) and integration tests (WasmEdge)
>
> - Constraint enforcement, module caching, error handling"
> git push origin feature/sandbox-manager
> ```

> **🔀 PHASE 1 PR**
> ```bash
> # Create PR: feature/sandbox-manager → main
> # Title: "Phase 1: Sandbox Manager with MockRuntime + WasmEdge"
> # Reviewer: Dev D
> ```

---

## Phase 2 — Wasm Tools + Demo Policies

> **Sync with main before Phase 2:**
> ```bash
> git checkout main && git pull origin main
> git checkout feature/sandbox-manager && git rebase main
> ```

### Task 6: Wasm Tool Source Files (tasks.md 14.1)
- [ ] 6.1 Create `wasm_tools/src/read_accounts.cpp` — reads account data, returns JSON
- [ ] 6.2 Create `wasm_tools/src/send_email.cpp` — simulates email send (uses network)
- [ ] 6.3 Create `wasm_tools/src/generate_report.cpp` — generates report with file I/O
- [ ] 6.4 Create `wasm_tools/src/encrypt.cpp` — simulates data encryption
- [ ] 6.5 Create `wasm_tools/src/read_code.cpp` — reads workspace files
- [ ] 6.6 Create `wasm_tools/src/run_tests.cpp` — simulates test execution
- [ ] 6.7 Create `wasm_tools/src/approval_request.cpp` — sends approval request
- [ ] 6.8 Create `wasm_tools/src/deploy_production.cpp` — simulates deployment
- [ ] 6.9 Create `wasm_tools/src/search_database.cpp` — simulates DB search
- [ ] 6.10 Create `wasm_tools/src/infinite_loop_tool.cpp` — infinite loop for DoS demo
- [ ] 6.11 Create `wasm_tools/src/memory_hog.cpp` — allocates excessive memory for sandbox testing
- [ ] 6.12 Each tool exports C functions compatible with WasmEdge
- _Requirements: 9.1, 9.2, 9.3, 10.1, 10.2, 10.3, 10.4, 10.5_

> **📌 COMMIT 5 — Wasm tool sources**
> ```bash
> git add wasm_tools/src/
> git commit -m "feat(wasm-tools): add 11 Wasm tool source files
>
> - Financial: read_accounts, send_email, generate_report, encrypt
> - Developer: read_code, run_tests, approval_request, deploy_production
> - DoS: search_database, infinite_loop_tool, memory_hog"
> ```

### Task 7: Wasm Build System (tasks.md 14.2)
- [ ] 7.1 Create `wasm_tools/build.sh` script using wasi-sdk
- [ ] 7.2 Configure wasi-sdk compilation flags for C++ tools
- [ ] 7.3 Compile all tools to `.wasm` with WASI support
- [ ] 7.4 Verify all tools load correctly in WasmEdge
- [ ] 7.5 Create `wasm_tools/README.md` with tool development guide
- _Requirements: 18.1_

> **📌 COMMIT 6 — Compiled .wasm binaries**
> ```bash
> git add wasm_tools/build.sh wasm_tools/README.md wasm_tools/*.wasm
> git commit -m "build(wasm): add build script and compiled .wasm binaries
>
> - build.sh using wasi-sdk
> - All 11 tools compiled to .wasm (committed as binary)"
> ```

### Task 8: Wasm Tool Testing (tasks.md 14.3)
- [ ] 8.1 Verify all tools compile successfully with wasi-sdk
- [ ] 8.2 Test tools load in WasmEdge VM
- [ ] 8.3 Test parameter passing (JSON input → function args)
- [ ] 8.4 Test result extraction (function return → JSON output)
- [ ] 8.5 Test file access constraints with preopened dirs
- [ ] 8.6 Test network access constraints
- _Requirements: 4.1, 4.2, 4.3, 4.4_

> **📌 COMMIT 7 — Wasm tool tests**
> ```bash
> git add tests/sandbox/
> git commit -m "test(wasm-tools): verify tool loading, params, constraints"
> ```

### Task 9: Demo Policy Files (tasks.md 15.1–15.3) ✅
- [x] 9.1 Create `policies/financial.json` — nodes: read_accounts, read_transactions, generate_report, encrypt, send_email with node types and sandbox configs
- [x] 9.2 Create `policies/developer.json` — nodes: read_code, run_tests, approval_request, deploy_production with paths and sandbox configs
- [x] 9.3 Create `policies/dos_prevention.json` — nodes: search_database, infinite_loop_tool with cycle thresholds and timeouts
- [x] 9.4 Validate all policy files load correctly via Dev A's `PolicyGraph::from_json()`
- _Requirements: 9.1, 9.2, 9.3, 10.1, 10.2, 10.3, 11.1, 11.2, 11.3_

> **📌 COMMIT 8 — Demo policies**
> ```bash
> git add policies/
> git commit -m "feat(policies): add 3 demo policy files (financial, developer, dos)"
> ```

-->

### Task 10: Sandbox Configuration Example (tasks.md 17.4) ✅
- [x] 10.1 Create `examples/sandbox_config.cpp` showing per-tool sandbox configuration
- [x] 10.2 Demonstrate constraint enforcement and violation handling
- [x] 10.3 Verify example compiles and runs
- _Requirements: 3.2, 3.3, 3.4, 3.5_

> **📌 COMMIT 9 — Sandbox example**
> ```bash
> git add examples/sandbox_config.cpp
> git commit -m "docs(examples): add sandbox configuration example"
> git push origin feature/sandbox-manager
> ```

> **🔀 PHASE 2 PR — Merge FIRST in Phase 2 (pure additions, no conflicts)**
> ```bash
> # Create PR: feature/sandbox-manager → main
> # Title: "Phase 2: Wasm Tools + Demo Policies + Sandbox Example"
> # Reviewer: Dev D
> ```

---

## Phase 3 — Performance, Final Validation (tasks.md 19, 22)

> **Sync with main before Phase 3:**
> ```bash
> git checkout main && git pull origin main
> git checkout feature/sandbox-manager && git rebase main
> ```

### Task 11: Sandbox Performance Benchmarks (tasks.md 19.1–19.3) ✅
- [x] 11.1 Benchmark sandbox overhead (<50ms for simple tools)
- [x] 11.2 Benchmark Wasm module loading and caching
- [x] 11.3 Add memory pooling for ToolCall objects (coordinate with Dev C)
- [x] 11.4 Benchmark memory usage under sustained load
- _Requirements: 12.3, 12.4, 12.5_

> **📌 COMMIT 10 — Perf benchmarks**
> ```bash
> git add tests/performance/
> git commit -m "test(perf): add sandbox performance benchmarks"
> ```

### Task 12: Final Validation — Linux (tasks.md 22.1–22.4) ✅
- [x] 12.1 Run complete test suite on Linux (the primary platform for sandbox tests)
- [x] 12.2 Run valgrind memory leak check on all integration tests
- [x] 12.3 Verify all `.wasm` tools load correctly after git clone (binary integrity)
- [x] 12.4 Run WasmEdge integration tests with all 3 demo policies
- [x] 12.5 Verify sandbox constraint enforcement under stress conditions

> **📌 COMMIT 11 — Final fixes**
> ```bash
> git add -A
> git commit -m "fix(sandbox): address issues found during final validation"
> git push origin feature/sandbox-manager
> ```

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
| `examples/sandbox_config.cpp` | Sandbox config example |

## Dependencies

- **Phase 1:** No dependencies — `SandboxConfig`, `SandboxResult`, `SandboxViolation` are in `types.hpp`
- **Phase 2:** Needs Dev A's `PolicyGraph::from_json()` to validate policy files

## Key Notes

- **Linux is required** for compiling `.wasm` files (wasi-sdk) and running WasmEdge integration tests
- `MockRuntime` must be usable by Dev C for their Tool Interceptor tests on Windows
- Pin WasmEdge to version `0.14.x` in CMake (`find_package(WasmEdge 0.14.0 EXACT REQUIRED)`)
- Provide `MockRuntime` early so Dev C can test interceptor → sandbox flow without real Wasm

## Cross-Platform Responsibilities (Dev B is the Linux/Wasm specialist)

- **Commit compiled `.wasm` files** to the repo as binary artifacts — Windows devs cannot compile them
- `.gitattributes` already marks `*.wasm` as binary — verify this works on your Linux machine
- The `MockRuntime` class must compile and run on **both** Windows (MSVC) and Linux (GCC):
  - No Linux-only headers (`<unistd.h>`, `<sys/...>`)
  - Use `std::filesystem::path` for all paths in `SandboxManager`
  - Use `std::filesystem::temp_directory_path()` instead of hardcoded `/tmp`
- `WasmEdgeRuntime` class is wrapped in `#ifdef HAVE_WASMEDGE` — Windows builds skip it entirely
- `build.sh` is Linux-only (fine — .wasm compilation only happens on Linux)
- Policy JSON files must use **forward slashes** in all path fields (e.g., `wasm_tools/read_accounts.wasm`)
- Test that your `.wasm` binaries load correctly after being committed and cloned (git may corrupt if `.gitattributes` is wrong)

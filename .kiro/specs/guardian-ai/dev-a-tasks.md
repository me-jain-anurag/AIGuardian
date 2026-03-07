# Dev A — Policy Graph Engine + Guardian API + Config

**OS:** Windows  
**Branch:** `feature/policy-graph`  
**Role:** Integration Lead — owns the shared contract (`types.hpp`) and wires all components together

### Git Setup
```bash
git clone https://github.com/No-Reed/AIGuardian.git
cd AIGuardian
git config core.autocrlf false
git config core.eol lf
git config pull.rebase true
# Install hooks (PowerShell):
Copy-Item scripts/hooks/pre-commit .git/hooks/pre-commit -Force
Copy-Item scripts/hooks/commit-msg .git/hooks/commit-msg -Force
git checkout -b feature/policy-graph
```

---
<!-->
## Phase 1 — Core Foundation

### Task 1: Project Setup and Build System
- [x] 1.1 Create `.gitignore` for C++ CMake project (already created in repo)
- [x] 1.2 Create root `CMakeLists.txt` with C++17 standard
- [x] 1.3 Configure dependencies: nlohmann_json (required), WasmEdge SDK (**optional** — use `find_package(QUIET)` so Windows builds don't fail), Graphviz (optional)
- [x] 1.4 Create directory structure: `include/guardian/`, `src/`, `cli/`, `tests/`, `policies/`, `examples/`, `wasm_tools/`
- [x] 1.5 Create `include/guardian/types.hpp` with all shared enums and structs (THE SHARED CONTRACT)
- [x] 1.6 Create empty header files with include guards for all components
- [x] 1.7 Configure test framework (Catch2 + RapidCheck via FetchContent)
- [x] 1.8 Commit `.gitattributes`, `.editorconfig`, `.gitignore`
- [x] 1.9 Add cross-platform CMake flags: `if(MSVC) /W4 /permissive-` else `-Wall -Wextra -Wpedantic`
- [x] 1.10 Add `#ifdef HAVE_WASMEDGE` guard so Windows builds compile without WasmEdge
- [x] 1.11 Handle `std::filesystem` linking for older GCC (`target_link_libraries(... stdc++fs)`)
- _Requirements: 16.1, 16.6_
- _Cross-platform: See `collaboration-plan.md` Section 9 (CMake patterns)_

> **📌 COMMIT 1 — Project skeleton**
> ```bash
> git add .
> git commit -m "build(cmake): initial project skeleton with CMake, types.hpp, empty headers
>
> - CMakeLists.txt with C++17, cross-platform flags
> - types.hpp shared contract
> - Empty headers for all 7 components
> - .gitignore, .gitattributes, .editorconfig
> - Catch2 + RapidCheck via FetchContent"
> git push -u origin feature/policy-graph
> ```

> **📌 MERGE TO MAIN — Push skeleton so all devs can start**
> ```bash
> # Create PR: feature/policy-graph → main
> # Title: "Project skeleton: CMake + types.hpp + empty headers"
> # After approval and merge:
> git checkout main
> git pull origin main
> git checkout feature/policy-graph
> git rebase main
> ```

### Task 2: Policy Graph Core Data Structures (tasks.md 2.1)
- [x] 2.1 Implement `PolicyNode` struct with id, tool_name, risk_level, node_type, metadata, wasm_module, sandbox_config
- [x] 2.2 Implement `PolicyEdge` struct with from_node_id, to_node_id, conditions, metadata
- [x] 2.3 Implement `PolicyGraph` class with adjacency list (`std::unordered_map<string, vector<PolicyEdge>>`)
- [x] 2.4 Implement `add_node()`, `add_edge()`, `remove_node()`, `remove_edge()`
- [x] 2.5 Implement `has_node()`, `has_edge()`, `get_neighbors()`, `get_node()`
- _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6_

> **📌 COMMIT 2 — Policy graph core**
> ```bash
> git add include/guardian/policy_graph.hpp src/policy_graph.cpp
> git commit -m "feat(policy-graph): implement PolicyGraph with adjacency list
>
> - PolicyNode, PolicyEdge structs
> - add_node/add_edge/remove/query methods
> - Adjacency list representation"
> ```

### Task 3: Policy Graph Serialization (tasks.md 2.3, 2.4)
- [x] 3.1 Implement `to_json()` using nlohmann_json — include nodes, edges, sandbox configs, metadata
- [x] 3.2 Implement `from_json()` with descriptive error messages (line/column info)
- [x] 3.3 Implement `to_dot()` for Graphviz compatibility — include node types, risk levels
- [x] 3.4 Implement `from_dot()` with DOT parsing and error handling
- [x] 3.5 Handle node types, sandbox configs, and metadata in both formats
- _Requirements: 1.7, 1.8, 1.9, 10.1, 10.2, 10.3, 10.7_

> **📌 COMMIT 3 — Serialization**
> ```bash
> git add include/guardian/policy_graph.hpp src/policy_graph.cpp
> git commit -m "feat(policy-graph): add JSON and DOT serialization
>
> - to_json/from_json with nlohmann_json
> - to_dot/from_dot for Graphviz
> - Error handling with descriptive messages"
> ```

### Task 4: Configuration Management (tasks.md 10.1–10.2)
- [x] 4.1 Create `Config` struct with cycle_detection, sandbox, performance, logging, policy sections
- [x] 4.2 Implement `load_config()` from JSON file with validation
- [x] 4.3 Define safe default values for all settings
- [x] 4.4 Apply safe defaults for invalid/missing values, log warnings
- [x] 4.5 Implement config serialization round-trip
- _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7_

> **📌 COMMIT 4 — Config management**
> ```bash
> git add include/guardian/config.hpp src/config.cpp
> git commit -m "feat(config): implement Config struct with JSON load/save
>
> - Safe defaults, validation, round-trip serialization"
> ```

### Task 5: Policy Graph Tests (tasks.md 2.2, 2.5, 2.6, 10.3–10.4)
- [x] 5.1 Unit tests: empty graph, node addition, edge addition, error handling
- [x] 5.2 Unit tests: JSON parsing with malformed input, DOT parsing with syntax errors
- [x] 5.3 Property tests: node addition, edge addition, metadata preservation
- [x] 5.4 Property test: JSON serialization round-trip
- [x] 5.5 Property test: DOT serialization round-trip
- [x] 5.6 Unit tests: config loading, defaults, round-trip
- _Requirements: 1.2, 1.3, 1.9, 10.3, 10.6, 10.7, 15.1, 15.6, 15.7_

> **📌 COMMIT 5 — Phase 1 tests**
> ```bash
> git add tests/unit/test_policy_graph.cpp tests/unit/test_config.cpp
> git commit -m "test(policy-graph): add unit + property tests for graph and config
>
> - Graph operations, JSON/DOT round-trip, error handling
> - Config loading, defaults, serialization"
> git push origin feature/policy-graph
> ```

> **🔀 INTEGRATION: Push `policy_graph.hpp` interface to unblock Dev C and Dev D**
> ```bash
> # Create PR: feature/policy-graph → main
> # Title: "Phase 1: Policy Graph Engine + Config"
> # Reviewer: Dev C
> # After merge, notify Dev C and Dev D to rebase
> ```

---

## Phase 2 — Guardian API (Integration)

> **🔀 INTEGRATION: Before starting Phase 2, sync with all Phase 1 merges**
> ```bash
> git checkout main
> git pull origin main
> git checkout feature/policy-graph
> git rebase main
> # Resolve any conflicts (especially CMakeLists.txt)
> ```

### Task 6: Guardian API Class (tasks.md 12.1–12.5)
- [x] 6.1 Implement `Guardian` constructor accepting policy file path and wasm_tools_dir
- [x] 6.2 Initialize all components: PolicyGraph, PolicyValidator, SessionManager, ~~SandboxManager~~(Dev B), VisualizationEngine
- [x] 6.3 Implement `execute_tool()` — validate then sandbox execute
- [x] 6.4 Implement `validate_tool_call()` — policy-only validation
- [x] 6.5 Implement `create_session()` and `end_session()` — delegate to SessionManager
- [x] 6.6 Implement `load_tool_module()` and `set_default_sandbox_config()`
- [x] 6.7 Implement `visualize_policy()` and `visualize_session()`
- [x] 6.8 Implement `update_config()` and `get_config()`
- [x] 6.9 Error handling: throw on construction errors, return codes on runtime errors
- _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5, 16.8, 16.9_

> **📌 COMMIT 6 — Guardian API core**
> ```bash
> git add include/guardian/guardian.hpp src/guardian.cpp
> git commit -m "feat(guardian-api): implement Guardian class wiring all components
>
> - execute_tool, validate_tool_call, session management
> - Visualization, config, error handling
> - Initializes PolicyGraph + Validator + Session + Sandbox + Viz"
> ```

### Task 7: Guardian API Tests (tasks.md 12.6–12.7)
- [x] 7.1 Test initialization with valid/invalid policy
- [x] 7.2 Test `execute_tool()` end-to-end flow
- [x] 7.3 Test `validate_tool_call()` without execution
- [x] 7.4 Test session management through API
- [x] 7.5 Test error handling for API misuse
- [x] 7.6 Property tests: initialization, result structure, error messages
- _Requirements: 16.3, 16.4, 16.5, 16.8_

> **📌 COMMIT 7 — Guardian API tests**
> ```bash
> git add tests/unit/test_guardian.cpp
> git commit -m "test(guardian-api): add unit + property tests for Guardian class"
> ```

### Task 8: Integration Examples (tasks.md 17.1–17.2)
- [x] 8.1 Create `examples/basic_integration.cpp` — Guardian init, validate, session mgmt
- [x] 8.2 Create `examples/custom_policy.cpp` — programmatic graph creation, serialization
- [x] 8.3 Verify examples compile and run
- _Requirements: 18.1, 18.4, 18.6_

> **📌 COMMIT 8 — Examples**
> ```bash
> git add examples/basic_integration.cpp examples/custom_policy.cpp
> git commit -m "docs(examples): add basic integration and custom policy examples"
> git push origin feature/policy-graph
> ```

> **🔀 INTEGRATION: Merge Phase 2 Guardian API**
> ```bash
> # Create PR: feature/policy-graph → main
> # Title: "Phase 2: Guardian API + Integration Examples"
> # Reviewer: Dev C
> # IMPORTANT: Merge AFTER Dev B's policies/wasm merge, BEFORE Dev C and Dev D
> ```

---
-->
## Phase 3 — Performance, Final Validation (tasks.md 19, 22)

> **🔀 INTEGRATION: Sync with main before Phase 3**
> ```bash
> git checkout main
> git pull origin main
> git checkout feature/policy-graph
> git rebase main
> ```

### Task 9: Performance Optimization for Policy Graph (tasks.md 19.1)
- [ ] 9.1 Add validation result caching with LRU eviction (coordinate with Dev C for cache key design)
- [ ] 9.2 Implement string interning for tool names in PolicyGraph
- [ ] 9.3 Optimize graph traversal algorithms (adjacency list iteration)
- [ ] 9.4 Add Wasm module caching improvements (coordinate with Dev B)
- _Requirements: 12.1, 12.2, 12.3_

> **📌 COMMIT 9 — Performance optimizations**
> ```bash
> git add src/policy_graph.cpp include/guardian/policy_graph.hpp
> git commit -m "perf(policy-graph): optimize traversal, add string interning and LRU cache"
> ```

### Task 10: Performance Benchmarks (tasks.md 19.2–19.3)
- [ ] 10.1 Create `tests/performance/bench_policy_graph.cpp` — benchmark graph operations
- [ ] 10.2 Benchmark policy validation latency (<10ms for 50 nodes, <50ms for 200 nodes)
- [ ] 10.3 Benchmark throughput (>100 validations/second)
- [ ] 10.4 Benchmark memory usage (<100MB for 1000 tool calls)
- [ ] 10.5 Test with graphs up to 200 nodes
- _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

> **📌 COMMIT 10 — Performance benchmarks**
> ```bash
> git add tests/performance/bench_policy_graph.cpp
> git commit -m "test(perf): add policy graph performance benchmarks"
> ```

### Task 11: Verify All Examples Compile and Run (tasks.md 17.5)
- [ ] 11.1 Build all 4 examples (basic_integration, custom_policy, concurrent_sessions, sandbox_config)
- [ ] 11.2 Run each example and verify output
- [ ] 11.3 Verify examples demonstrate best practices and include error handling
- _Requirements: 18.6_

### Task 12: Final Validation (tasks.md 22.1–22.4)
- [ ] 12.1 Run complete test suite: unit + property + integration + performance
- [ ] 12.2 Verify all demos complete within 90 seconds
- [ ] 12.3 Run static analysis (clang-tidy or MSVC /analyze)
- [ ] 12.4 Check for memory leaks (valgrind on Linux via Dev B, or DrMemory on Windows)
- [ ] 12.5 Verify thread safety (run concurrency tests under TSan if available)
- [ ] 12.6 Check code formatting consistency
- _Requirements: 9.5, 10.5, 11.5, 17.10_

> **📌 COMMIT 11 — Final validation fixes**
> ```bash
> git add -A
> git commit -m "fix(all): address issues found during final validation"
> git push origin feature/policy-graph
> ```

> **🔀 FINAL INTEGRATION: Merge Phase 3 to main**
> ```bash
> # Create PR: feature/policy-graph → main
> # Title: "Phase 3: Performance + Final Validation"
> # Reviewer: All devs
> ```

---

## Files Owned

| File | Description |
|------|-------------|
| `include/guardian/types.hpp` | Shared contract (co-owned, PR required for changes) |
| `include/guardian/policy_graph.hpp` | Policy graph class declaration |
| `src/policy_graph.cpp` | Policy graph implementation |
| `include/guardian/config.hpp` | Config struct and loading |
| `src/config.cpp` | Config implementation |
| `include/guardian/guardian.hpp` | Guardian API class declaration |
| `src/guardian.cpp` | Guardian API implementation |
| `CMakeLists.txt` | Build system (initial setup, then shared) |
| `.gitignore` | Git ignore rules |
| `tests/unit/test_policy_graph.cpp` | Policy graph tests |
| `tests/unit/test_config.cpp` | Config tests |
| `tests/unit/test_guardian.cpp` | Guardian API tests |
| `tests/performance/bench_policy_graph.cpp` | Performance benchmarks |
| `examples/basic_integration.cpp` | Basic usage example |
| `examples/custom_policy.cpp` | Custom policy example |

## Dependencies

- **Phase 1:** No dependencies — Dev A starts first
- **Phase 2:** Needs headers from Dev B (SandboxManager), Dev C (PolicyValidator, SessionManager, ToolInterceptor), Dev D (VisualizationEngine)

## Integration Responsibilities

- Push `types.hpp` and empty headers ASAP to unblock team
- Review PRs from Dev C (shared graph↔validator interface)
- Wire all components together in `Guardian` class
- Resolve merge conflicts on `CMakeLists.txt` and `guardian.hpp`

## Cross-Platform Responsibilities (Dev A is the platform lead)

- **CMakeLists.txt must build on BOTH Windows (MSVC) and Linux (GCC)** — test with `cmake --build build`
- Use `std::filesystem::path` for ALL path operations in `policy_graph.cpp` and `guardian.cpp`
- WasmEdge must be **optional** via `#ifdef HAVE_WASMEDGE` — Windows builds use MockRuntime
- Commit `.gitattributes`, `.editorconfig`, and `.gitignore` with the initial skeleton
- Recommend setting up the GitHub Actions CI workflow (see `collaboration-plan.md` Section 8)

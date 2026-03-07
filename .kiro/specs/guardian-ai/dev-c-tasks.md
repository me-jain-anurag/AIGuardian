# Dev C — Policy Validator + Session Manager + Tool Interceptor

**OS:** Windows  
**Branch:** `feature/validator-session`  
**Role:** Core validation logic — owns the algorithmic heart of Guardian AI

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
git checkout -b feature/validator-session
```

---

## Phase 1 — Session Manager + Policy Validator

### Task 1: Session Manager Data Structures (tasks.md 5.1)
- [x] 1.1 Implement `Session` struct with session_id, action_sequence, created_at, last_activity, metadata
- [x] 1.2 Add UUID generation for session identifiers (use `<random>` — see cross-platform section)
- [x] 1.3 Verify `ToolCall` struct in `types.hpp` has: tool_name, parameters, timestamp, session_id
- _Requirements: 11.1, 2.2_

### Task 2: SessionManager Class (tasks.md 5.2–5.3)
- [x] 2.1 Implement `create_session()` generating unique session IDs
- [x] 2.2 Implement `end_session()` for session lifecycle management
- [x] 2.3 Implement `append_tool_call()` for sequence tracking
- [x] 2.4 Implement `get_sequence()` for sequence retrieval
- [x] 2.5 Implement `has_session()` for validation
- [x] 2.6 Add thread-safe session storage with `std::shared_mutex`
- [x] 2.7 Implement `persist_session()` writing action sequence to JSON file
- [x] 2.8 Implement `get_all_sessions()` for audit queries
- [x] 2.9 Add session timeout logic with configurable expiry
- [x] 2.10 Add configurable maximum concurrent sessions limit
- _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.7, 11.8_

> **📌 COMMIT 1 — Session Manager**
> ```bash
> git add include/guardian/session_manager.hpp src/session_manager.cpp
> git commit -m "feat(session): implement SessionManager with thread-safe storage
>
> - Session lifecycle, persistence, UUID generation
> - Thread-safe with std::shared_mutex
> - Timeout logic and concurrent session limits"
> git push -u origin feature/validator-session
> ```

### Task 3: Session Manager Tests (tasks.md 5.4–5.5)
- [x] 3.1 Unit tests: session creation and ID uniqueness
- [x] 3.2 Unit tests: concurrent session isolation (multi-threaded test)
- [x] 3.3 Unit tests: action sequence growth and retrieval
- [x] 3.4 Unit tests: session persistence to JSON and loading
- [x] 3.5 Unit tests: session timeout behavior
- [x] 3.6 Property tests: ID uniqueness, new session initialization, isolation, persistence, query round-trip
- _Requirements: 11.1, 11.2, 11.3, 11.4, 11.6, 11.7_

> **📌 COMMIT 2 — Session tests**
> ```bash
> git add tests/unit/test_session_manager.cpp
> git commit -m "test(session): add unit + property tests for SessionManager
>
> - Concurrency, persistence, isolation, timeout"
> ```

### Task 4: Validation Result Structures (tasks.md 6.1)
- [x] 4.1 Verify `ValidationResult` in `types.hpp` has: approved, reason, suggested_alternatives, detected_cycle, detected_exfiltration
- [x] 4.2 Verify `CycleInfo` in `types.hpp` has: cycle_start_index, cycle_length, cycle_tools
- [x] 4.3 Verify `ExfiltrationPath` in `types.hpp` has: source_node, destination_node, path
- _Requirements: 5.6, 6.4, 7.6_

### Task 5: Transition Validation (tasks.md 6.2)
- [x] 5.1 Implement `check_transition(from_tool, to_tool)` verifying edge existence in graph
- [x] 5.2 Handle initial tool call (empty previous tool) — allow any tool defined in graph
- [x] 5.3 Return descriptive reasons for rejections
- [x] 5.4 Suggest valid alternatives from graph neighbors in rejection response
- _Requirements: 5.1, 5.2, 5.3, 5.4_

### Task 6: Cycle Detection Algorithm (tasks.md 6.3)
- [x] 6.1 Implement `detect_cycle()` using frequency map approach (see design.md Algorithm 2)
- [x] 6.2 Track consecutive tool call repetitions
- [x] 6.3 Support per-tool cycle thresholds from configuration
- [x] 6.4 Return `CycleInfo` with start index, length, and tools
- [x] 6.5 Distinguish between legitimate repeated calls and malicious loops
- _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_

> **📌 COMMIT 3 — Transition + cycle detection**
> ```bash
> git add include/guardian/policy_validator.hpp src/policy_validator.cpp
> git commit -m "feat(validator): implement transition validation and cycle detection
>
> - check_transition with alternatives suggestion
> - detect_cycle with per-tool thresholds
> - Frequency map approach per design.md Algorithm 2"
> ```

### Task 7: Exfiltration Pattern Detection (tasks.md 6.4)
- [x] 7.1 Implement `detect_exfiltration()` using node type checks (see design.md Algorithm 3)
- [x] 7.2 Identify SENSITIVE_SOURCE → EXTERNAL_DESTINATION direct paths
- [x] 7.3 Check for intermediate DATA_PROCESSOR nodes in path
- [x] 7.4 Approve paths that go through at least one DATA_PROCESSOR
- [x] 7.5 Return `ExfiltrationPath` with source, destination, and path
- [x] 7.6 Identify specific patterns: file_read → network_post
- _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_

> **📌 COMMIT 4 — Exfiltration detection**
> ```bash
> git add include/guardian/policy_validator.hpp src/policy_validator.cpp
> git commit -m "feat(validator): implement exfiltration pattern detection
>
> - SENSITIVE_SOURCE → EXTERNAL_DESTINATION blocking
> - DATA_PROCESSOR intermediate node approval
> - design.md Algorithm 3"
> ```

### Task 8: Main Validate Method + Path Validation (tasks.md 6.5–6.6)
- [x] 8.1 Implement `validate()` coordinating transition, cycle, and exfiltration checks
- [x] 8.2 Implement validation caching with LRU eviction for repeated patterns
- [x] 8.3 Ensure deterministic validation results
- [x] 8.4 Implement `is_valid_path()` using graph traversal
- [x] 8.5 Support arbitrary path lengths
- [x] 8.6 Handle multiple valid paths (approve if any matches)
- [x] 8.7 Return matched path or violation point in results
- _Requirements: 5.5, 5.7, 14.1, 14.2, 14.3, 14.4, 14.6_

> **📌 COMMIT 5 — Main validate + path validation**
> ```bash
> git add include/guardian/policy_validator.hpp src/policy_validator.cpp
> git commit -m "feat(validator): implement main validate() with caching and path validation
>
> - LRU cache, deterministic results, graph traversal
> - Coordinates transition + cycle + exfiltration checks"
> ```

### Task 9: Policy Validator Tests (tasks.md 6.7–6.8)
- [x] 9.1 Unit tests: transition validation with valid/invalid edges
- [x] 9.2 Unit tests: initial tool call handling (empty sequence)
- [x] 9.3 Unit tests: cycle detection with various thresholds (default 10, per-tool custom)
- [x] 9.4 Unit tests: exfiltration pattern detection (direct path blocked, safe path allowed)
- [x] 9.5 Unit tests: validation caching behavior (cache hit returns same result)
- [x] 9.6 Unit tests: deterministic validation
- [x] 9.7 Property tests: transition validation, determinism, result structure, cycle detection, exfiltration blocking, safe path approval, path length support, multiple paths, caching
- _Requirements: 5.1, 5.2, 5.3, 5.4, 5.7, 6.1, 6.2, 6.5, 7.4, 7.5, 14.3, 14.4_

> **📌 COMMIT 6 — Validator tests**
> ```bash
> git add tests/unit/test_policy_validator.cpp
> git commit -m "test(validator): add unit + property tests for PolicyValidator
>
> - Transition, cycle, exfiltration, caching, determinism
> - 14 property tests covering all correctness properties"
> git push origin feature/validator-session
> ```

> **🔀 PHASE 1 PR**
> ```bash
> # Create PR: feature/validator-session → main
> # Title: "Phase 1: Session Manager + Policy Validator"
> # Reviewer: Dev A
> ```

---

## Phase 2 — Tool Interceptor + Integration Tests

> **Sync with main before Phase 2:**
> ```bash
> git checkout main && git pull origin main
> git checkout feature/validator-session && git rebase main
> ```

### Task 10: ToolInterceptor Class (tasks.md 7.1–7.2)
- [x] 10.1 Implement constructor accepting PolicyValidator*, SessionManager*, SandboxManager*
- [x] 10.2 Implement `intercept()` — capture tool name, parameters, timestamp
- [x] 10.3 Query SessionManager for current action sequence
- [x] 10.4 Pass tool call to PolicyValidator for validation
- [x] 10.5 On approval: delegate to SandboxManager for execution, then append to session
- [x] 10.6 On rejection: block execution, return error with reason
- [x] 10.7 Preserve original parameters for approved executions
- [x] 10.8 Implement template-based `execute_if_valid()` wrapper
- _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7_

> **📌 COMMIT 7 — Tool Interceptor**
> ```bash
> git add include/guardian/tool_interceptor.hpp src/tool_interceptor.cpp
> git commit -m "feat(interceptor): implement ToolInterceptor coordinating validator + sandbox
>
> - intercept/execute_if_valid flow
> - Approve → sandbox execute, Reject → block + reason"
> ```

### Task 11: Tool Interceptor Tests (tasks.md 7.3–7.4)
- [x] 11.1 Unit tests: tool call capture with various parameters
- [x] 11.2 Unit tests: action sequence updates after interception
- [x] 11.3 Unit tests: approved execution flow (uses MockRuntime from Dev B)
- [x] 11.4 Unit tests: rejected execution blocking
- [x] 11.5 Unit tests: parameter preservation
- [x] 11.6 Property tests: capture completeness, sequence growth, validator communication, approve/reject, parameter preservation
- _Requirements: 2.1, 2.2, 2.3, 2.5, 2.6, 2.7_

> **📌 COMMIT 8 — Interceptor tests**
> ```bash
> git add tests/unit/test_tool_interceptor.cpp
> git commit -m "test(interceptor): add unit + property tests using MockRuntime"
> ```

### Task 12: Integration Tests (tasks.md 20.1–20.2)
- [x] 12.1 End-to-end: financial exfiltration scenario (read_accounts → send_email BLOCKED)
- [x] 12.2 End-to-end: financial safe path (read_accounts → generate_report → encrypt → send_email ALLOWED)
- [x] 12.3 End-to-end: developer deployment scenario (read_code → deploy_production BLOCKED)
- [x] 12.4 End-to-end: developer safe path (read_code → approval_request → deploy_production ALLOWED)
- [x] 12.5 End-to-end: DoS prevention (cycle detection triggers after threshold)
- [x] 12.6 End-to-end: concurrent session handling (independent validation)
- [x] 12.7 End-to-end: session persistence and recovery
- [x] 12.8 Error handling: policy file not found, invalid format, invalid session ID
- [x] 12.9 Error handling: internal error fail-safe (reject on error)
- _Requirements: 9.1–9.4, 10.1–10.4, 11.1–11.4, 13.6, 16.8_

> **📌 COMMIT 9 — Integration tests**
> ```bash
> git add tests/integration/
> git commit -m "test(integration): add end-to-end tests for all 3 demo scenarios
>
> - Financial exfiltration, developer deployment, DoS prevention
> - Concurrent sessions, error handling, fail-safe"
> ```

### Task 13: Concurrent Sessions Example (tasks.md 17.3)
- [x] 13.1 Create `examples/concurrent_sessions.cpp`
- [x] 13.2 Show multiple concurrent sessions with independent validation
- [x] 13.3 Demonstrate session isolation and persistence
- [x] 13.4 Verify example compiles and runs
- _Requirements: 18.5_

> **📌 COMMIT 10 — Concurrent sessions example**
> ```bash
> git add examples/concurrent_sessions.cpp
> git commit -m "docs(examples): add concurrent sessions example"
> git push origin feature/validator-session
> ```

> **🔀 PHASE 2 PR — Merge THIRD (after Dev B and Dev A)**
> ```bash
> # Create PR: feature/validator-session → main
> # Title: "Phase 2: Tool Interceptor + Integration Tests + Example"
> # Reviewer: Dev A
> ```

---

## Phase 3 — Performance, Final Validation (tasks.md 19, 22)

> **Sync with main before Phase 3:**
> ```bash
> git checkout main && git pull origin main
> git checkout feature/validator-session && git rebase main
> ```

### Task 14: Validation Performance Optimization (tasks.md 19.1)
- [ ] 14.1 Optimize validation caching — tune LRU cache size, profile cache hit rates
- [ ] 14.2 Add memory pooling for ToolCall objects
- [ ] 14.3 Profile and optimize cycle detection for long sequences (>1000 tool calls)
- [ ] 14.4 Profile and optimize exfiltration detection for complex graphs (>100 nodes)
- _Requirements: 12.1, 12.2, 12.3_

> **📌 COMMIT 11 — Performance optimization**
> ```bash
> git add src/policy_validator.cpp src/session_manager.cpp
> git commit -m "perf(validator): optimize caching, cycle detection, memory pooling"
> ```

### Task 15: Validation Performance Tests (tasks.md 19.2–19.3)
- [ ] 15.1 Create `tests/performance/bench_validator.cpp`
- [ ] 15.2 Benchmark validation latency (<10ms for 50 nodes, <50ms for 200 nodes)
- [ ] 15.3 Benchmark throughput (>100 validations/second)
- [ ] 15.4 Benchmark memory usage (<100MB for 1000 tool calls)
- [ ] 15.5 Benchmark concurrent session performance (4+ threads)
- _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

> **📌 COMMIT 12 — Performance benchmarks**
> ```bash
> git add tests/performance/bench_validator.cpp
> git commit -m "test(perf): add validator and session performance benchmarks"
> ```

### Task 16: Final Validation — Integration Tests (tasks.md 22.1–22.2)
- [ ] 16.1 Run complete test suite: all unit + property + integration + performance tests
- [ ] 16.2 Verify all 3 demo scenarios produce correct results
- [ ] 16.3 Verify demos complete within 90 seconds each
- [ ] 16.4 Run concurrency stress test under Thread Sanitizer (if available)
- _Requirements: 9.5, 10.5, 11.5, 17.10_

> **📌 COMMIT 13 — Final fixes**
> ```bash
> git add -A
> git commit -m "fix(validator): address issues found during final validation"
> git push origin feature/validator-session
> ```

---

## Files Owned

| File | Description |
|------|-------------|
| `include/guardian/session_manager.hpp` | Session struct, SessionManager class |
| `src/session_manager.cpp` | Session manager implementation |
| `include/guardian/policy_validator.hpp` | PolicyValidator class |
| `src/policy_validator.cpp` | Validator implementation (transition, cycle, exfiltration, caching) |
| `include/guardian/tool_interceptor.hpp` | ToolInterceptor class |
| `src/tool_interceptor.cpp` | Interceptor implementation |
| `tests/unit/test_session_manager.cpp` | Session manager tests |
| `tests/unit/test_policy_validator.cpp` | Policy validator tests |
| `tests/unit/test_tool_interceptor.cpp` | Tool interceptor tests |
| `tests/integration/` | All end-to-end integration tests |
| `tests/performance/bench_validator.cpp` | Validation benchmarks |
| `examples/concurrent_sessions.cpp` | Concurrent sessions example |

## Dependencies

- **Phase 1:** Needs Dev A's `PolicyGraph` header (interface only — code against the declaration)
- **Phase 2:** Needs Dev B's `MockRuntime`/`SandboxManager` header for interceptor → sandbox flow
- **Phase 2:** Needs Dev A's `Guardian` class and Dev B's policies for integration tests

## Key Notes

- Code against `PolicyGraph` interface from headers — don't wait for Dev A's implementation
- Use `MockRuntime` from Dev B for all sandbox-related testing on Windows
- The validator is the most algorithm-heavy component — reference design.md Algorithms 1–5
- Thread safety is critical for `SessionManager` — test concurrent access
- Validation caching uses a key based on last 5 tools + current tool (see design.md Algorithm 5)

## Cross-Platform Notes (Dev C on Windows)

- **UUID generation:** Do NOT use Windows-specific `UuidCreate` or Linux `uuid_generate`. Use portable `<random>`:
  ```cpp
  #include <random>
  #include <sstream>
  #include <iomanip>
  std::string generate_uuid() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<uint32_t> dist;
      std::stringstream ss;
      ss << std::hex << std::setfill('0')
         << std::setw(8) << dist(gen) << "-"
         << std::setw(4) << (dist(gen) & 0xFFFF) << "-"
         << std::setw(4) << ((dist(gen) & 0x0FFF) | 0x4000) << "-"
         << std::setw(4) << ((dist(gen) & 0x3FFF) | 0x8000) << "-"
         << std::setw(12) << ((uint64_t)dist(gen) << 16 | (dist(gen) & 0xFFFF));
      return ss.str();
  }
  ```
- **Session persistence paths:** Use `std::filesystem::path` in `persist_session()` — never hardcode `/` or `\`
- **Temp directory:** Use `std::filesystem::temp_directory_path()` for session audit files
- **Thread safety:** `std::shared_mutex` works the same on MSVC and GCC — no special handling needed
- **MockRuntime usage:** All interceptor → sandbox tests must use `MockRuntime` (no WasmEdge on Windows)
- **Integration tests:** Write tests that work **without** real `.wasm` files — use `MockRuntime` or policy-only validation

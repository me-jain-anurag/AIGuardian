# Dev D — Visualization Engine + CLI Demo Tool + Logging + Docs

**OS:** Windows  
**Branch:** `feature/viz-cli-docs`  
**Role:** User-facing output — owns everything the user sees (terminal, graphs, logs, documentation)

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
git checkout -b feature/viz-cli-docs
```

---

## Phase 1 — Visualization Engine + Logging System

### Task 1: Visualization Engine Core (tasks.md 9.1–9.2)
- [ ] 1.1 Implement `VisualizationOptions` struct with output_format, highlight_violations, show_metadata, style_overrides
- [ ] 1.2 Implement `VisualizationEngine` class
- [ ] 1.3 Implement `render_graph()` — generate DOT format from PolicyGraph
- [ ] 1.4 Include node metadata: tool names, risk levels, node types (color-coded)
- [ ] 1.5 Include edge metadata: transition conditions
- [ ] 1.6 Apply styling: SENSITIVE_SOURCE=red, EXTERNAL_DESTINATION=orange, DATA_PROCESSOR=green, NORMAL=blue
- [ ] 1.7 Optionally invoke Graphviz CLI for SVG/PNG generation (`#ifdef HAVE_GRAPHVIZ`)
- _Requirements: 8.1, 8.4, 8.5, 8.6_

### Task 2: Sequence Visualization (tasks.md 9.3)
- [ ] 2.1 Implement `render_sequence()` — highlight action sequence path through graph
- [ ] 2.2 Distinguish approved transitions (green solid arrows) from rejected transitions (red dashed arrows)
- [ ] 2.3 Mark violation points with distinct styling (red X, thick border)
- [ ] 2.4 Include validation result information in the output
- _Requirements: 8.2, 8.3_

> **📌 COMMIT 1 — Visualization core + sequences**
> ```bash
> git add include/guardian/visualization.hpp src/visualization.cpp
> git commit -m "feat(viz): implement VisualizationEngine with DOT and sequence rendering
>
> - render_graph with node type styling
> - render_sequence with approve/reject highlighting
> - Optional Graphviz SVG/PNG via #ifdef HAVE_GRAPHVIZ"
> git push -u origin feature/viz-cli-docs
> ```

### Task 3: ASCII Art Rendering (tasks.md 9.4)
- [ ] 3.1 Implement `render_ascii_graph()` for terminal output using box-drawing characters
- [ ] 3.2 Implement `render_ascii_sequence()` for action sequences
- [ ] 3.3 Support colored output using ANSI escape codes (green=approved, red=blocked)
- [ ] 3.4 Handle graphs with varying widths and depths
- [ ] 3.5 Display node types and risk levels in ASCII format
- _Requirements: 8.1_

> **📌 COMMIT 2 — ASCII rendering**
> ```bash
> git add include/guardian/visualization.hpp src/visualization.cpp
> git commit -m "feat(viz): implement ASCII art rendering for terminal output
>
> - Box-drawing characters, ANSI colors
> - Graph and sequence rendering"
> ```

### Task 4: Visualization Tests (tasks.md 9.5–9.6)
- [ ] 4.1 Unit tests: DOT format generation contains correct nodes and edges
- [ ] 4.2 Unit tests: ASCII art rendering produces non-empty output
- [ ] 4.3 Unit tests: sequence highlighting marks the correct path
- [ ] 4.4 Unit tests: violation marking uses distinct styling
- [ ] 4.5 Unit tests: metadata inclusion (tool names, risk levels, conditions)
- [ ] 4.6 Unit tests: rendering performance (<2s for 100-node graph)
- [ ] 4.7 Property tests: output generation, path highlighting, violation highlighting, metadata inclusion
- _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.7_

> **📌 COMMIT 3 — Visualization tests**
> ```bash
> git add tests/unit/test_visualization.cpp
> git commit -m "test(viz): add unit + property tests for visualization engine"
> ```

### Task 5: Logging Infrastructure (tasks.md 11.1)
- [ ] 5.1 Implement `Logger` class with DEBUG, INFO, WARN, ERROR levels
- [ ] 5.2 Implement log level filtering (messages below configured level are suppressed)
- [ ] 5.3 Support JSON log format (structured entries with timestamp, level, message, context)
- [ ] 5.4 Support plain text log format
- [ ] 5.5 Support file output (configurable path) and console output (stdout/stderr)
- [ ] 5.6 Implement `export_logs()` generating valid JSON array of log entries
- [ ] 5.7 Support filtering exports by level, time range, session ID
- _Requirements: 13.3, 13.4, 13.5_

### Task 6: Logging Integration Points (tasks.md 11.2)
- [ ] 6.1 Define log entry format: `{timestamp, level, component, tool_name, session_id, message, details}`
- [ ] 6.2 Create logging macros/helpers for validation decisions (approved/rejected with reason)
- [ ] 6.3 Create logging helpers for sandbox execution (metrics: memory, time)
- [ ] 6.4 Create logging helpers for sandbox violations (constraint type, limit exceeded)
- [ ] 6.5 Create logging helpers for policy/wasm load errors
- [ ] 6.6 Implement fail-safe logging: on internal error, log ERROR and ensure tool call is rejected
- _Requirements: 13.1, 13.2, 13.4, 13.6_

> **📌 COMMIT 4 — Logger**
> ```bash
> git add include/guardian/logger.hpp src/logger.cpp
> git commit -m "feat(logger): implement Logger with level filtering, JSON/text, file/console
>
> - export_logs with filtering
> - Fail-safe error handling
> - Logging helpers for validation, sandbox, errors"
> ```

### Task 7: Logging Tests (tasks.md 11.4–11.5)
- [ ] 7.1 Unit tests: log level filtering (DEBUG suppressed when level=INFO)
- [ ] 7.2 Unit tests: JSON format validity (parse output with nlohmann_json)
- [ ] 7.3 Unit tests: log entry completeness (all fields present)
- [ ] 7.4 Unit tests: fail-safe error handling (internal error → ERROR log + reject)
- [ ] 7.5 Property tests: logging completeness, level filtering, error logging, JSON validity, fail-safe
- _Requirements: 13.1, 13.2, 13.3, 13.5, 13.6_

> **📌 COMMIT 5 — Logger tests**
> ```bash
> git add tests/unit/test_logger.cpp
> git commit -m "test(logger): add unit + property tests for Logger"
> git push origin feature/viz-cli-docs
> ```

> **🔀 PHASE 1 PR**
> ```bash
> # Create PR: feature/viz-cli-docs → main
> # Title: "Phase 1: Visualization Engine + Logging System"
> # Reviewer: Dev B
> ```

---

## Phase 2 — CLI Demo Tool + Documentation

> **Sync with main before Phase 2:**
> ```bash
> git checkout main && git pull origin main
> git checkout feature/viz-cli-docs && git rebase main
> ```

### Task 8: CLI Main Entry Point (tasks.md 16.1)
- [ ] 8.1 Create `cli/main.cpp` with argument parsing
- [ ] 8.2 Support `--scenario=financial|developer|dos` flag
- [ ] 8.3 Support `--policy=<path>` flag for custom policy graphs
- [ ] 8.4 Support `--interactive` flag for manual tool call input
- [ ] 8.5 Support `--output=<path>` flag for visualization output
- [ ] 8.6 Display help text with `--help`
- _Requirements: 17.1, 17.2, 17.3, 17.7, 17.8_

### Task 9: Terminal UI Utilities (tasks.md 16.2)
- [ ] 9.1 Create `cli/terminal_ui.hpp/.cpp` with colored output functions
- [ ] 9.2 Implement green output for approved actions: `✓ APPROVED: tool_name`
- [ ] 9.3 Implement red output for blocked actions: `✗ BLOCKED: tool_name — reason`
- [ ] 9.4 Implement yellow output for warnings and sandbox metrics
- [ ] 9.5 Implement ASCII art policy graph rendering (reuse VisualizationEngine)
- [ ] 9.6 Implement progress indicators for demo execution
- [ ] 9.7 Handle Windows terminal ANSI escape code support
- _Requirements: 17.5, 17.6_

> **📌 COMMIT 6 — CLI entry + terminal UI**
> ```bash
> git add cli/main.cpp cli/terminal_ui.hpp cli/terminal_ui.cpp
> git commit -m "feat(cli): implement CLI entry point with arg parsing and terminal UI
>
> - --scenario, --policy, --interactive, --output, --help
> - Colored output with Windows ANSI support
> - enable_virtual_terminal() + SetConsoleOutputCP(CP_UTF8)"
> ```

### Task 10: Demo Scenario Implementations (tasks.md 16.3)
- [ ] 10.1 Create `cli/scenarios.hpp/.cpp` with scenario runner functions
- [ ] 10.2 Implement `run_financial_demo()`:
  - Show read_accounts → send_email BLOCKED (exfiltration)
  - Show read_accounts → generate_report → encrypt → send_email ALLOWED (safe path)
  - Display before/after comparison
- [ ] 10.3 Implement `run_developer_demo()`:
  - Show read_code → run_tests ALLOWED
  - Show read_code → deploy_production BLOCKED
  - Show read_code → approval_request → deploy_production ALLOWED
- [ ] 10.4 Implement `run_dos_demo()`:
  - Show search_database called 5 times → ALLOWED
  - Show search_database called 15 times → BLOCKED after 10
  - Show infinite_loop_tool → TIMEOUT (sandbox)
  - Visualize detected cycle
- [ ] 10.5 Each scenario displays real-time validation decisions with colored output
- [ ] 10.6 Each scenario shows both policy and sandbox enforcement
- _Requirements: 9.1–9.5, 10.1–10.5, 11.1–11.5_

> **📌 COMMIT 7 — Demo scenarios**
> ```bash
> git add cli/scenarios.hpp cli/scenarios.cpp
> git commit -m "feat(cli): implement 3 demo scenarios (financial, developer, dos)
>
> - Real-time colored validation decisions
> - Policy + sandbox enforcement demonstration"
> ```

### Task 11: Interactive Mode (tasks.md 16.4)
- [ ] 11.1 Accept manual tool call input: `> tool_name param1=value1 param2=value2`
- [ ] 11.2 Display validation results in real-time
- [ ] 11.3 Show current action sequence
- [ ] 11.4 Display policy graph visualization
- [ ] 11.5 Support `help`, `policy`, `sequence`, `quit` commands
- _Requirements: 17.8_

### Task 12: Summary Statistics (tasks.md 16.5)
- [ ] 12.1 Track total calls, approved calls, blocked calls per session
- [ ] 12.2 Track violation types: policy_violation, sandbox_violation (memory, timeout, file, network)
- [ ] 12.3 Track execution metrics: avg/max validation time, avg/max sandbox time
- [ ] 12.4 Display formatted summary at demo completion
- _Requirements: 17.9_

> **📌 COMMIT 8 — Interactive mode + stats**
> ```bash
> git add cli/
> git commit -m "feat(cli): add interactive mode and summary statistics
>
> - Manual tool call input, live validation
> - Execution metrics, violation tracking"
> ```

### Task 13: CLI Tests (tasks.md 16.6–16.7)
- [ ] 13.1 Unit tests: scenario flag parsing
- [ ] 13.2 Unit tests: custom policy loading via --policy
- [ ] 13.3 Unit tests: summary statistics calculation
- [ ] 13.4 Unit tests: colored output generation
- [ ] 13.5 Property tests: CLI scenario parsing, custom policy loading, summary statistics
- _Requirements: 17.2, 17.3, 17.7, 17.9_

> **📌 COMMIT 9 — CLI tests**
> ```bash
> git add tests/unit/test_cli.cpp
> git commit -m "test(cli): add unit + property tests for CLI tool"
> ```

### Task 14: Documentation (tasks.md 21.1–21.5)
- [ ] 14.1 Create main `README.md`: overview, quick start, installation, basic usage, links
- [ ] 14.2 Create API documentation: Guardian class, all public structs/enums, method examples
- [ ] 14.3 Create policy authoring guide: JSON format, DOT format, node types, sandbox configs
- [ ] 14.4 Create demo scenario documentation: what each demo shows, expected output
- [ ] 14.5 Review and link `wasm_tools/README.md` (written by Dev B)
- _Requirements: 16.1, 16.2, 16.7, 16.8, 1.7, 10.1, 10.2, 9.1–9.5, 10.1–10.5, 11.1–11.5_

> **📌 COMMIT 10 — Documentation**
> ```bash
> git add README.md docs/
> git commit -m "docs: add README, API docs, policy guide, demo guide"
> git push origin feature/viz-cli-docs
> ```

> **🔀 PHASE 2 PR — Merge LAST (after all other devs)**
> ```bash
> # Create PR: feature/viz-cli-docs → main
> # Title: "Phase 2: CLI Demo Tool + Documentation"
> # Reviewer: Dev B
> ```

---

## Phase 3 — Performance, Final Validation (tasks.md 19, 22)

> **Sync with main before Phase 3:**
> ```bash
> git checkout main && git pull origin main
> git checkout feature/viz-cli-docs && git rebase main
> ```

### Task 15: Visualization Performance (tasks.md 19.3)
- [ ] 15.1 Benchmark visualization rendering (<2s for 100-node graphs)
- [ ] 15.2 Benchmark ASCII rendering for large graphs
- [ ] 15.3 Profile DOT generation and optimize if needed
- _Requirements: 8.7, 12.6_

> **📌 COMMIT 11 — Viz performance**
> ```bash
> git add tests/performance/
> git commit -m "test(perf): add visualization performance benchmarks"
> ```

### Task 16: Final Validation — Demos & Docs (tasks.md 22.2–22.4)
- [ ] 16.1 Run all 3 CLI demos and verify visual output
- [ ] 16.2 Verify demos complete within 90 seconds each
- [ ] 16.3 Verify all examples compile and run (coordinate with Dev A)
- [ ] 16.4 Run code formatting check across all owned files
- [ ] 16.5 Final documentation review: README, API docs, policy guide, demo docs
- [ ] 16.6 Verify all docs are accurate, up-to-date, and cross-linked
- _Requirements: 9.5, 10.5, 11.5, 17.10, 18.6_

> **📌 COMMIT 12 — Final fixes**
> ```bash
> git add -A
> git commit -m "fix(cli,docs): address issues found during final validation"
> git push origin feature/viz-cli-docs
> ```

---

## Files Owned

| File | Description |
|------|-------------|
| `include/guardian/visualization.hpp` | VisualizationOptions, VisualizationEngine |
| `src/visualization.cpp` | Visualization implementation |
| `include/guardian/logger.hpp` | Logger class |
| `src/logger.cpp` | Logger implementation |
| `cli/main.cpp` | CLI entry point |
| `cli/terminal_ui.hpp` | Terminal UI utilities header |
| `cli/terminal_ui.cpp` | Colored output, ASCII art |
| `cli/scenarios.hpp` | Demo scenario declarations |
| `cli/scenarios.cpp` | Demo scenario implementations |
| `tests/unit/test_visualization.cpp` | Visualization tests |
| `tests/unit/test_logger.cpp` | Logger tests |
| `tests/unit/test_cli.cpp` | CLI tests |
| `README.md` | Main project README |
| `docs/api.md` | API documentation |
| `docs/policy_guide.md` | Policy authoring guide |
| `docs/demo_guide.md` | Demo scenario documentation |

## Dependencies

- **Phase 1:** Needs Dev A's `PolicyGraph` header for visualization input; `ValidationResult` is in `types.hpp`
- **Phase 2:** Needs Dev A's `Guardian` class, Dev B's policies + Wasm tools, Dev C's validator for demo scenarios

## Key Notes

- Windows ANSI escape codes: use `SetConsoleMode` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` — see `collaboration-plan.md` Section 6
- DOT generation is the core output — SVG/PNG via Graphviz is optional (`#ifdef HAVE_GRAPHVIZ`)
- The Logger should be a singleton or passed by reference to avoid global state
- ASCII rendering is critical for the CLI demo — invest time in making it look polished
- Demo scenarios in Phase 2 are the project's "showcase" — make them visually impressive

## Cross-Platform Responsibilities (Dev D owns terminal output)

- **Terminal colors are THE cross-platform pain point for this role.** Add this to `cli/terminal_ui.cpp`:
  ```cpp
  #ifdef _WIN32
  #include <windows.h>
  void enable_virtual_terminal() {
      HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
      DWORD dwMode = 0;
      GetConsoleMode(hOut, &dwMode);
      dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(hOut, dwMode);
  }
  #else
  void enable_virtual_terminal() {} // ANSI works natively on Linux
  #endif
  ```
  Call `enable_virtual_terminal()` in `cli/main.cpp` before any colored output.

- **Graphviz:** Wrap all Graphviz library calls in `#ifdef HAVE_GRAPHVIZ`. The CLI tool's `--output` flag should fall back to writing `.dot` text files when Graphviz is not installed.

- **Log file paths:** Use `std::filesystem::path` in Logger for output file paths. Never hardcode separators.

- **Console encoding:** MSVC's `stderr` defaults to the system locale. Add `std::setlocale(LC_ALL, ".UTF-8")` at startup for proper Unicode in log messages.

- **Temp files in tests:** For visualization tests that write output files, use `std::filesystem::temp_directory_path()` — never hardcode `/tmp`.

- **Box-drawing characters:** ASCII art box-drawing characters (`┌─┐│└─┘→`) display correctly on modern Windows 10+ terminals with UTF-8 codepage. Add this to CLI startup:
  ```cpp
  #ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  #endif
  ```

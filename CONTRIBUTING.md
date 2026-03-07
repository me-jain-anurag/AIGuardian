# Contributing to AIGuardian

## Getting started

### Prerequisites

- CMake 3.16+
- C++17 compiler (see platform notes below)
- Git

### Build

**Linux**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
ctest --output-on-failure
```

**Windows (MSYS2 UCRT64)**

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
mingw32-make -j4
ctest --output-on-failure
```

See [docs/WINDOWS_SETUP.md](docs/WINDOWS_SETUP.md) if Defender blocks executables.

### Code formatting

The project uses clang-format with the config in `.clang-format` (LLVM style, indent width 4, column limit 120). Format before committing:

```bash
clang-format -i src/*.cpp include/guardian/*.hpp
```

---

## Repository structure

```
include/guardian/    Public headers — the API surface
src/                 Implementation files
tests/unit/          Unit tests (Catch2) and property tests (RapidCheck)
tests/performance/   Benchmark tests
examples/            Standalone usage examples
policies/            Example policy JSON files
docs/                Documentation
```

Each header in `include/guardian/` has a corresponding `.cpp` in `src/`. Ownership comments at the top of each file indicate which dev originally wrote it (for context, not strict ownership).

---

## Testing

All changes must keep the full test suite passing:

```bash
ctest --output-on-failure
```

13 test targets:

| Target | What it covers |
|---|---|
| `test_session_manager` | Session isolation, sequence tracking |
| `test_policy_validator` | Transitions, cycles, exfiltration, path validation, caching |
| `test_logger` | Level filtering, JSON export, file output, thread safety |
| `test_visualization` | DOT colours, sequence styling, ASCII legend, summary |
| `test_cli` | Interactive CLI flows |
| `test_sandbox_manager` | Sandbox lifecycle |
| `test_wasmedge` | WebAssembly runtime integration |
| `test_tool_interceptor` | Validator + sandbox coordination |
| `test_integration_demos` | End-to-end scenarios |
| `bench_policy_graph` | Graph operation performance |
| `bench_visualization` | Rendering performance |
| `bench_sandbox` | Sandbox execution performance |
| `bench_validator` | Validation latency, throughput, concurrency |

When adding a new feature, add tests to the relevant unit test file. For new public API, add to `tests/unit/`. For performance-sensitive code, add to the relevant bench file.

---

## Commit style

Use conventional commits:

```
feat(component): short description
fix(component): short description
refactor(component): short description
test(component): short description
docs: short description
build: short description
chore: short description
```

Keep commits atomic — one logical change per commit. Do not bundle unrelated changes.

---

## Adding a new check to PolicyValidator

1. Add the detection method to `include/guardian/policy_validator.hpp`.
2. Implement it in `src/policy_validator.cpp`.
3. Call it from `validate()` in the appropriate order (after transition check, before cache store).
4. Add unit tests in `tests/unit/test_policy_validator.cpp`.

---

## Adding a new policy node type

1. Add the value to the `NodeType` enum in `include/guardian/types.hpp`.
2. Update `PolicyGraph::from_json` / `to_json` in `src/policy_graph.cpp` to handle the new string.
3. Update `node_fill_color` and `node_type_abbr` in `src/visualization.cpp`.
4. Update the policy guide in `docs/policy_guide.md`.

---

## Pull requests

- Target the `main` branch.
- All tests must pass.
- Include a brief description of what changed and why.
- If it fixes a bug, describe how to reproduce the bug before the fix.

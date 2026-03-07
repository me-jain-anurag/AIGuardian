# ADR-003: CMake as Build System

**Status:** Accepted  
**Date:** 2026-03-07  
**Context:** Guardian AI needs a build system for compiling the C++ library, CLI tool, tests, and managing dependencies.

## Decision

**Use CMake 3.15+** as the build system.

## Alternatives Considered

| Build System | C++ Support | Dependency Mgmt | Cross-Platform | IDE Support | Adoption |
|-------------|------------|-----------------|----------------|-------------|----------|
| **CMake** | ✅ Industry standard | ⚠️ find_package | ✅ Excellent | ✅ All IDEs | Dominant |
| **Meson** | ✅ Good | ✅ Wrap system | ✅ Good | ⚠️ Limited | Growing |
| **Bazel** | ✅ Good | ✅ Built-in | ✅ Excellent | ⚠️ Limited | Google-centric |
| **xmake** | ✅ Good | ✅ Built-in | ✅ Good | ⚠️ Limited | Niche |
| **Make** | ✅ Manual | ❌ None | ❌ Unix only | ❌ None | Legacy |

## Rationale

### Why CMake

**1. WasmEdge and nlohmann_json both provide CMake configs**

Both critical dependencies ship `find_package()` support. This means zero custom build logic:
```cmake
find_package(WasmEdge REQUIRED)
find_package(nlohmann_json REQUIRED)
target_link_libraries(guardian PUBLIC WasmEdge::wasmedge nlohmann_json::nlohmann_json)
```

With Meson or Bazel, you'd need custom wrap files or BUILD rules for these dependencies.

**2. Universal IDE integration**

CMake generates native project files for Visual Studio, Xcode, CLion, and VS Code (via CMake Tools). Since development happens on WSL2 with VS Code, CMake provides seamless integration.

**3. Industry standard for C++ libraries**

Users integrating Guardian AI into their projects expect `find_package(GuardianAI)` or `add_subdirectory()`. CMake makes this trivial with `install()` targets.

**4. CTest integration for testing**

CMake's `enable_testing()` + `add_test()` works with Catch2, Google Test, and RapidCheck out of the box.

### Why not Meson?

Meson is arguably *better designed* than CMake, but:
- WasmEdge doesn't ship a Meson wrap file
- Fewer developers are familiar with it
- IDE support is weaker (no native VS Code integration)

### Why not Bazel?

Overkill for a single-library project. Bazel excels at monorepos with many targets, not standalone C++ libraries.

## Consequences

- Project uses CMakeLists.txt with modern CMake practices (targets, not variables)
- C++17 enforced via `CMAKE_CXX_STANDARD 17`
- Dependencies resolved via `find_package()` (users must install globally or use vcpkg/Conan)
- Tests use CTest for discovery and execution

cmake_minimum_required(VERSION 3.16)
project(GuardianAI VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Cross-Platform Compiler Flags
# ============================================================================
if(MSVC)
    add_compile_options(/W4 /permissive-)
    # UTF-8 source and execution character set
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Output Directories
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Keep test executable paths stable across single-config and multi-config generators.
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# ============================================================================
# Dependencies
# ============================================================================

# nlohmann/json (required)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# WasmEdge SDK (optional — Linux only for now)
# The install.sh script doesn't reliably create CMake config files. Look for it directly.
find_path(WasmEdge_INCLUDE_DIRS wasmedge/wasmedge.h PATHS "$ENV{HOME}/.wasmedge/include" "/usr/local/include" "/usr/include")
find_library(WasmEdge_LIBRARIES wasmedge PATHS "$ENV{HOME}/.wasmedge/lib" "/usr/local/lib" "/usr/lib")

if(WasmEdge_INCLUDE_DIRS AND WasmEdge_LIBRARIES)
    set(WasmEdge_FOUND TRUE)
endif()
if(WasmEdge_FOUND)
    message(STATUS "WasmEdge found natively")
    add_compile_definitions(HAVE_WASMEDGE)
    # Give all targets access to the definition
    add_definitions(-DHAVE_WASMEDGE)
else()
    message(STATUS "WasmEdge NOT found — sandbox will use MockRuntime only")
endif()

# Graphviz (optional — for SVG/PNG rendering)
find_program(GRAPHVIZ_DOT dot)
if(GRAPHVIZ_DOT)
    message(STATUS "Graphviz found: ${GRAPHVIZ_DOT}")
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GRAPHVIZ libgvc libcgraph)
    endif()
    
    if(GRAPHVIZ_FOUND)
        add_compile_definitions(HAVE_GRAPHVIZ)
    else()
        message(STATUS "Graphviz headers/libs NOT found, only `dot` executable found. Visualization limited to DOT and ASCII output.")
    endif()
else()
    message(STATUS "Graphviz NOT found — visualization limited to DOT and ASCII output")
endif()

# ============================================================================
# Guardian Library
# ============================================================================
add_library(guardian
    src/policy_graph.cpp
    src/config.cpp
    src/guardian.cpp
    # Dev B adds:
    src/sandbox_manager.cpp
    # Dev C adds:
    src/session_manager.cpp
    src/policy_validator.cpp
    src/tool_interceptor.cpp   # (Phase 2)
    # Dev D adds:
    src/visualization.cpp
    src/logger.cpp
)

target_include_directories(guardian PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(guardian PUBLIC
    nlohmann_json::nlohmann_json
)

# Link std::filesystem for older GCC versions
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(guardian PUBLIC stdc++fs)
endif()

# Link WasmEdge if available
if(WasmEdge_FOUND)
    target_link_libraries(guardian PUBLIC wasmedge)
endif()

# ============================================================================
# Test Framework (Catch2 + RapidCheck)
# ============================================================================
option(GUARDIAN_BUILD_TESTS "Build tests" ON)

if(GUARDIAN_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)

    FetchContent_Declare(
        rapidcheck
        GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(rapidcheck)

    # Unit tests (add as devs contribute)
    # add_executable(test_policy_graph tests/unit/test_policy_graph.cpp)
    # target_link_libraries(test_policy_graph PRIVATE guardian Catch2::Catch2WithMain rapidcheck)
    # add_test(NAME test_policy_graph COMMAND test_policy_graph)

    # Dev C — Session Manager tests
    add_executable(test_session_manager tests/unit/test_session_manager.cpp)
    target_link_libraries(test_session_manager PRIVATE guardian Catch2::Catch2WithMain rapidcheck)
    add_test(NAME test_session_manager COMMAND test_session_manager)

    # Dev C — Policy Validator tests
    add_executable(test_policy_validator tests/unit/test_policy_validator.cpp)
    target_link_libraries(test_policy_validator PRIVATE guardian Catch2::Catch2WithMain rapidcheck)
    add_test(NAME test_policy_validator COMMAND test_policy_validator)

    # Dev D — Logger and Visualization tests
    add_executable(test_logger tests/unit/test_logger.cpp)
    target_link_libraries(test_logger PRIVATE guardian Catch2::Catch2WithMain nlohmann_json::nlohmann_json rapidcheck)
    add_test(NAME test_logger COMMAND test_logger)

    add_executable(test_visualization tests/unit/test_visualization.cpp)
    target_link_libraries(test_visualization PRIVATE guardian Catch2::Catch2WithMain rapidcheck)
    add_test(NAME test_visualization COMMAND test_visualization)

    # Dev D — CLI tests
    add_executable(test_cli tests/unit/test_cli.cpp cli/scenarios.cpp cli/terminal_ui.cpp)
    target_link_libraries(test_cli PRIVATE guardian Catch2::Catch2WithMain)
    add_test(NAME test_cli COMMAND test_cli)
    set_tests_properties(test_cli PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Dev B: MockRuntime + WasmExecutor + SandboxManager tests
    add_executable(test_sandbox_manager tests/unit/test_sandbox_manager.cpp)
    target_link_libraries(test_sandbox_manager PRIVATE guardian Catch2::Catch2WithMain)
    add_test(NAME test_sandbox_manager COMMAND test_sandbox_manager)

    # Dev B: WasmEdge integration tests (skipped when HAVE_WASMEDGE not defined)
    add_executable(test_wasmedge tests/sandbox/test_wasmedge.cpp)
    target_link_libraries(test_wasmedge PRIVATE guardian Catch2::Catch2WithMain)
    add_test(NAME test_wasmedge COMMAND test_wasmedge)

    # Dev D: Performance Benchmarks
    add_executable(bench_visualization tests/performance/bench_visualization.cpp)
    target_link_libraries(bench_visualization PRIVATE guardian Catch2::Catch2WithMain)
    add_test(NAME bench_visualization COMMAND bench_visualization)

    # Dev B: Sandbox performance benchmarks
    add_executable(bench_sandbox tests/performance/bench_sandbox.cpp)
    target_link_libraries(bench_sandbox PRIVATE guardian Catch2::Catch2WithMain)
    add_test(NAME bench_sandbox COMMAND bench_sandbox)

    # Dev C — Tool Interceptor tests
    add_executable(test_tool_interceptor tests/unit/test_tool_interceptor.cpp)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
        # Workaround: GNU ld on Windows can fail for complex C++ executables. Use LLD if available.
        target_compile_options(test_tool_interceptor PRIVATE -g0)
    endif()
    target_link_libraries(test_tool_interceptor PRIVATE guardian Catch2::Catch2WithMain rapidcheck)
    add_test(NAME test_tool_interceptor COMMAND test_tool_interceptor)
    
    # Dev C — Integration Demos (Standalone, no Catch2 to avoid GNU linker bugs)
    add_executable(test_integration_demos tests/integration/test_integration_demos.cpp)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
        target_compile_options(test_integration_demos PRIVATE -g0)
    endif()
    target_link_libraries(test_integration_demos PRIVATE guardian)
    add_test(NAME test_integration_demos COMMAND test_integration_demos)

    # Dev C — Concurrent Sessions Example
    add_executable(concurrent_sessions examples/concurrent_sessions.cpp)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
        target_compile_options(concurrent_sessions PRIVATE -g0)
    endif()
    target_link_libraries(concurrent_sessions PRIVATE guardian)
endif()

# ============================================================================
# Sandbox Configuration Demo (Dev B — Task 10)
# ============================================================================
add_executable(sandbox_config_demo examples/sandbox_config.cpp)
target_link_libraries(sandbox_config_demo PRIVATE guardian)

# ============================================================================
# CLI Demo Tool (Phase 2 — Dev D)
# ============================================================================
add_executable(guardian_cli 
    cli/main.cpp 
    cli/terminal_ui.cpp 
    cli/scenarios.cpp
)
target_include_directories(guardian_cli PRIVATE cli)
target_link_libraries(guardian_cli PRIVATE guardian)

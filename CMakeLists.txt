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
find_package(WasmEdge 0.14.0 QUIET)
if(WasmEdge_FOUND)
    message(STATUS "WasmEdge found: ${WasmEdge_VERSION}")
    add_compile_definitions(HAVE_WASMEDGE)
else()
    message(STATUS "WasmEdge NOT found — sandbox will use MockRuntime only")
endif()

# Graphviz (optional — for SVG/PNG rendering)
find_program(GRAPHVIZ_DOT dot)
if(GRAPHVIZ_DOT)
    message(STATUS "Graphviz found: ${GRAPHVIZ_DOT}")
    add_compile_definitions(HAVE_GRAPHVIZ)
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
    # src/sandbox_manager.cpp
    # Dev C adds:
    # src/session_manager.cpp
    # src/policy_validator.cpp
    # src/tool_interceptor.cpp
    # Dev D adds:
    # src/visualization.cpp
    # src/logger.cpp
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
endif()

# ============================================================================
# CLI Demo Tool (Phase 2 — Dev D)
# ============================================================================
# add_executable(guardian_cli cli/main.cpp cli/terminal_ui.cpp cli/scenarios.cpp)
# target_link_libraries(guardian_cli PRIVATE guardian)

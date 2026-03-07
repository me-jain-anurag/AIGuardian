# Implementation Plan: Guardian AI

## Overview

Guardian AI is a C++ runtime security system for AI agents with two-layer protection: policy-based validation and WasmEdge sandbox execution. This implementation plan breaks down the development into discrete coding tasks, building from core components to integration and demos.

## Implementation Approach

1. Core policy graph and validation engine (foundation)
2. WasmEdge sandbox manager (critical MVP feature)
3. Tool interceptor and session management
4. Visualization engine
5. Guardian API and CLI demo tool
6. Wasm tool implementations
7. Integration and testing

## Tasks

- [ ] 1. Project setup and build system
  - Create CMakeLists.txt with C++17 standard
  - Configure dependencies: nlohmann_json, WasmEdge SDK, optional Graphviz
  - Set up include/ and src/ directory structure
  - Create basic header files for all components
  - Configure test framework (Catch2 or Google Test)
  - Configure RapidCheck for property-based testing
  - _Requirements: 16.1, 16.6_

- [ ] 2. Implement Policy Graph Engine
  - [ ] 2.1 Create core data structures
    - Implement PolicyNode struct with id, tool_name, risk_level, node_type, metadata, wasm_module, sandbox_config
    - Implement PolicyEdge struct with from_node_id, to_node_id, conditions, metadata
    - Implement PolicyGraph class with adjacency list representation
    - Add node/edge addition and removal methods
    - Add graph query methods (has_node, has_edge, get_neighbors, get_node)
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6_


  - [ ]* 2.2 Write property tests for Policy Graph
    - **Property 1: Policy Graph Node Addition**
    - **Validates: Requirements 1.2**
    - **Property 2: Policy Graph Edge Addition**
    - **Validates: Requirements 1.3**
    - **Property 3: Policy Graph Metadata Preservation**
    - **Validates: Requirements 1.4, 1.5**

  - [ ] 2.3 Implement JSON serialization
    - Implement to_json() method using nlohmann_json
    - Implement from_json() static method with error handling
    - Handle node types, sandbox configs, and metadata
    - _Requirements: 1.7, 10.1_

  - [ ] 2.4 Implement DOT format serialization
    - Implement to_dot() method for Graphviz compatibility
    - Implement from_dot() static method with parsing
    - Include node and edge metadata in DOT output
    - _Requirements: 1.7, 10.2_

  - [ ]* 2.5 Write property test for serialization round-trip
    - **Property 4: Policy Graph Serialization Round-Trip**
    - **Validates: Requirements 1.9, 10.6**

  - [ ]* 2.6 Write unit tests for Policy Graph
    - Test empty graph creation
    - Test node addition with various node types
    - Test edge addition between existing nodes
    - Test error handling for invalid operations
    - Test JSON parsing with malformed input
    - Test DOT parsing with syntax errors
    - _Requirements: 1.2, 1.3, 10.3, 10.7_

- [ ] 3. Implement WasmEdge Sandbox Manager
  - [ ] 3.1 Create SandboxConfig and result structures
    - Implement SandboxConfig struct with memory_limit_mb, timeout_ms, allowed_paths, network_access, environment_vars
    - Implement SandboxResult struct with success, output, error, memory_used_bytes, execution_time_ms, violation
    - Implement SandboxViolation enum with MEMORY_EXCEEDED, TIMEOUT, FILE_ACCESS_DENIED, NETWORK_DENIED
    - Define safe default values: 128MB memory, 5000ms timeout, no file access, no network
    - _Requirements: 3.2, 3.3, 3.4, 3.5, 3.7_

  - [ ] 3.2 Implement WasmExecutor class
    - Create WasmExecutor class wrapping WasmEdge::VM
    - Implement constructor loading Wasm module from file path
    - Implement execute() method with parameter passing via JSON
    - Implement result extraction from Wasm function return
    - Add module loading validation and error handling
    - Implement reload() method for module updates
    - _Requirements: 4.1, 4.2, 4.5_

  - [ ] 3.3 Implement constraint enforcement in WasmExecutor
    - Configure WasmEdge VM with memory limits (convert MB to pages)
    - Implement timeout enforcement via VM interruption
    - Configure WASI preopened directories for file access control
    - Configure WASI socket permissions for network control
    - Add resource monitoring during execution
    - Return detailed SandboxViolation on constraint breaches
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.8_

  - [ ] 3.4 Implement SandboxManager class
    - Create SandboxManager with wasm_tools_dir configuration
    - Implement load_module() for loading and caching WasmExecutor instances
    - Implement execute_tool() coordinating module lookup and execution
    - Add thread-safe executor pool with std::shared_mutex
    - Implement per-tool sandbox configuration storage
    - Add default configuration fallback logic
    - _Requirements: 3.9, 4.6, 4.7_

  - [ ]* 3.5 Write unit tests for Sandbox Manager
    - Test Wasm module loading from valid .wasm files
    - Test error handling for missing Wasm files
    - Test error handling for invalid Wasm bytecode
    - Test memory limit enforcement (tool exceeding limit)
    - Test timeout enforcement (infinite loop tool)
    - Test file access control (unauthorized path access)
    - Test network access control (network call when disabled)
    - Test safe default configuration application
    - Test module caching and reuse
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.5_

- [ ] 4. Checkpoint - Core components functional
  - Ensure all tests pass for Policy Graph and Sandbox Manager
  - Verify WasmEdge integration works with sample Wasm module
  - Ask the user if questions arise


- [ ] 5. Implement Session Manager
  - [ ] 5.1 Create Session and ToolCall structures
    - Implement ToolCall struct with tool_name, parameters, timestamp, session_id
    - Implement Session struct with session_id, action_sequence, created_at, last_activity, metadata
    - Add UUID generation for session identifiers
    - _Requirements: 11.1, 2.2_

  - [ ] 5.2 Implement SessionManager class
    - Implement create_session() generating unique session IDs
    - Implement end_session() for session lifecycle management
    - Implement append_tool_call() for sequence tracking
    - Implement get_sequence() for sequence retrieval
    - Add thread-safe session storage with std::shared_mutex
    - Implement has_session() for validation
    - _Requirements: 11.1, 11.2, 11.3, 11.5_

  - [ ] 5.3 Implement session persistence
    - Implement persist_session() writing action sequence to JSON file
    - Include timestamp, tool calls, and metadata in output
    - Implement get_all_sessions() for audit queries
    - Add session timeout and cleanup logic
    - _Requirements: 11.4, 11.7_

  - [ ]* 5.4 Write property tests for Session Manager
    - **Property 27: Session ID Uniqueness**
    - **Validates: Requirements 11.1**
    - **Property 28: New Session Initialization**
    - **Validates: Requirements 11.2**
    - **Property 29: Session Isolation**
    - **Validates: Requirements 11.3, 11.6**
    - **Property 30: Session Persistence**
    - **Validates: Requirements 11.4**
    - **Property 31: Session Query Round-Trip**
    - **Validates: Requirements 11.5**

  - [ ]* 5.5 Write unit tests for Session Manager
    - Test session creation and uniqueness
    - Test concurrent session isolation
    - Test action sequence growth
    - Test session persistence and loading
    - Test session timeout behavior
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.7_

- [ ] 6. Implement Policy Validator
  - [ ] 6.1 Create validation result structures
    - Implement ValidationResult struct with approved, reason, suggested_alternatives, detected_cycle, detected_exfiltration
    - Implement CycleInfo struct with cycle_start_index, cycle_length, cycle_tools
    - Implement ExfiltrationPath struct with source_node, destination_node, path
    - _Requirements: 5.6, 6.4, 7.6_

  - [ ] 6.2 Implement transition validation
    - Implement check_transition() verifying edge existence in graph
    - Handle initial tool call (empty previous tool)
    - Return descriptive reasons for rejections
    - Suggest valid alternatives from graph neighbors
    - _Requirements: 5.1, 5.2, 5.3, 5.4_

  - [ ] 6.3 Implement cycle detection algorithm
    - Implement detect_cycle() using frequency map approach
    - Track consecutive tool call repetitions
    - Support per-tool cycle thresholds from configuration
    - Return CycleInfo with start index and length
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.6_

  - [ ] 6.4 Implement exfiltration pattern detection
    - Implement detect_exfiltration() identifying sensitive source to external destination paths
    - Check for intermediate DATA_PROCESSOR nodes
    - Identify direct exfiltration attempts
    - Return ExfiltrationPath with source, destination, and path
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_

  - [ ] 6.5 Implement main validate() method
    - Coordinate transition, cycle, and exfiltration checks
    - Implement validation caching for repeated patterns
    - Ensure deterministic validation results
    - Return comprehensive ValidationResult
    - _Requirements: 5.5, 5.7, 14.1, 14.4_

  - [ ] 6.6 Implement path validation algorithm
    - Implement is_valid_path() using graph traversal
    - Support arbitrary path lengths
    - Handle multiple valid paths (approve if any matches)
    - Cache path validation results
    - Return matched path or violation point
    - _Requirements: 14.2, 14.3, 14.6_

  - [ ]* 6.7 Write property tests for Policy Validator
    - **Property 11: Transition Validation**
    - **Validates: Requirements 5.1, 5.3, 5.4**
    - **Property 12: Validation Determinism**
    - **Validates: Requirements 5.7**
    - **Property 13: Validation Result Structure**
    - **Validates: Requirements 5.6**
    - **Property 14: Cycle Detection Accuracy**
    - **Validates: Requirements 6.1, 6.6**
    - **Property 15: Cycle Threshold Enforcement**
    - **Validates: Requirements 6.2, 6.5**
    - **Property 16: Cycle Information in Rejection**
    - **Validates: Requirements 6.4**
    - **Property 17: Per-Tool Cycle Thresholds**
    - **Validates: Requirements 6.3**
    - **Property 18: Node Type Assignment**
    - **Validates: Requirements 7.1, 7.2**
    - **Property 19: Direct Exfiltration Blocking**
    - **Validates: Requirements 7.4**
    - **Property 20: Safe Path Approval**
    - **Validates: Requirements 7.5**
    - **Property 37: Arbitrary Path Length Support**
    - **Validates: Requirements 14.2**
    - **Property 38: Multiple Valid Paths**
    - **Validates: Requirements 14.3**
    - **Property 39: Validation Caching**
    - **Validates: Requirements 14.4**
    - **Property 40: Path Information in Results**
    - **Validates: Requirements 14.6**

  - [ ]* 6.8 Write unit tests for Policy Validator
    - Test transition validation with valid and invalid edges
    - Test initial tool call handling
    - Test cycle detection with various thresholds
    - Test exfiltration pattern detection
    - Test safe path approval through DATA_PROCESSOR
    - Test validation caching behavior
    - Test deterministic validation
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.7, 6.1, 6.2, 6.5, 7.4, 7.5, 14.3, 14.4_


- [ ] 7. Implement Tool Interceptor
  - [ ] 7.1 Create ToolInterceptor class
    - Implement constructor accepting PolicyValidator, SessionManager, SandboxManager pointers
    - Implement intercept() method capturing tool calls
    - Extract tool name, parameters, and timestamp
    - Query SessionManager for current action sequence
    - Pass tool call to PolicyValidator for validation
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

  - [ ] 7.2 Implement execution coordination
    - On policy approval, delegate to SandboxManager for execution
    - On policy rejection, block execution and return error
    - Preserve original parameters for approved executions
    - Append successful tool calls to session action sequence
    - Return combined ValidationResult and SandboxResult
    - _Requirements: 2.5, 2.6, 2.7_

  - [ ]* 7.3 Write property tests for Tool Interceptor
    - **Property 5: Tool Call Capture Completeness**
    - **Validates: Requirements 2.1, 2.2**
    - **Property 6: Action Sequence Growth**
    - **Validates: Requirements 2.3**
    - **Property 7: Interceptor-Validator Communication**
    - **Validates: Requirements 2.4**
    - **Property 8: Approved Execution Proceeds**
    - **Validates: Requirements 2.5**
    - **Property 9: Rejected Execution Blocked**
    - **Validates: Requirements 2.6**
    - **Property 10: Parameter Preservation**
    - **Validates: Requirements 2.7**

  - [ ]* 7.4 Write unit tests for Tool Interceptor
    - Test tool call capture with various parameters
    - Test action sequence updates
    - Test approved execution flow
    - Test rejected execution blocking
    - Test parameter preservation
    - Test integration with validator and sandbox
    - _Requirements: 2.1, 2.2, 2.3, 2.5, 2.6, 2.7_

- [ ] 8. Checkpoint - Validation and interception complete
  - Ensure all tests pass for Session Manager, Policy Validator, and Tool Interceptor
  - Verify end-to-end flow: intercept → validate → sandbox execute
  - Ask the user if questions arise

- [ ] 9. Implement Visualization Engine
  - [ ] 9.1 Create VisualizationEngine class
    - Implement VisualizationOptions struct with output_format, highlight_violations, show_metadata, style_overrides
    - Implement render_graph() for policy graph visualization
    - Implement render_sequence() for action sequence visualization
    - Support DOT, SVG, PNG, and ASCII output formats
    - _Requirements: 8.1, 8.6_

  - [ ] 9.2 Implement graph rendering
    - Generate DOT format from PolicyGraph
    - Include node metadata (tool names, risk levels, node types)
    - Include edge metadata (transition conditions)
    - Apply styling for different node types
    - Optionally invoke Graphviz for SVG/PNG conversion
    - _Requirements: 8.1, 8.4, 8.5_

  - [ ] 9.3 Implement sequence visualization
    - Highlight action sequence path through graph
    - Distinguish approved vs rejected transitions with colors
    - Mark violation points distinctly
    - Include validation result information
    - _Requirements: 8.2, 8.3_

  - [ ] 9.4 Implement ASCII art rendering
    - Create render_ascii_graph() for terminal output
    - Create render_ascii_sequence() for action sequences
    - Use box-drawing characters for graph structure
    - Support colored output for CLI tool
    - _Requirements: 8.1_

  - [ ]* 9.5 Write property tests for Visualization Engine
    - **Property 21: Visualization Output Generation**
    - **Validates: Requirements 8.1, 8.6**
    - **Property 22: Sequence Path Highlighting**
    - **Validates: Requirements 8.2**
    - **Property 23: Violation Highlighting**
    - **Validates: Requirements 8.3**
    - **Property 24: Visualization Metadata Inclusion**
    - **Validates: Requirements 8.4, 8.5**

  - [ ]* 9.6 Write unit tests for Visualization Engine
    - Test DOT format generation
    - Test ASCII art rendering
    - Test sequence highlighting
    - Test violation marking
    - Test metadata inclusion
    - Test rendering performance (<2s for 100 nodes)
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.7_

- [ ] 10. Implement Configuration Management
  - [ ] 10.1 Create Config structure
    - Define Config struct with cycle_detection, sandbox, performance, logging, policy sections
    - Implement JSON serialization and deserialization
    - Define safe default values for all settings
    - _Requirements: 15.2, 15.3, 15.4, 15.5_

  - [ ] 10.2 Implement configuration loading
    - Implement load_config() from JSON file
    - Validate configuration values
    - Apply safe defaults for invalid values
    - Log warnings for configuration fallbacks
    - _Requirements: 15.1, 15.6_

  - [ ]* 10.3 Write property tests for Configuration
    - **Property 41: Configuration Application**
    - **Validates: Requirements 15.2, 15.3, 15.4, 15.5**
    - **Property 42: Configuration Fallback**
    - **Validates: Requirements 15.6**
    - **Property 43: Configuration Round-Trip**
    - **Validates: Requirements 15.7**

  - [ ]* 10.4 Write unit tests for Configuration
    - Test configuration loading from valid JSON
    - Test invalid configuration handling
    - Test default value application
    - Test configuration serialization
    - _Requirements: 15.1, 15.6, 15.7_


- [ ] 11. Implement Logging System
  - [ ] 11.1 Create logging infrastructure
    - Implement Logger class with DEBUG, INFO, WARN, ERROR levels
    - Support JSON and text log formats
    - Implement log level filtering
    - Support file and console output
    - _Requirements: 13.3, 13.4_

  - [ ] 11.2 Integrate logging throughout components
    - Add validation decision logging (approved/rejected)
    - Add sandbox execution logging with metrics
    - Add policy load error logging
    - Add internal error logging with fail-safe behavior
    - Include timestamps, tool names, and reasons in all logs
    - _Requirements: 13.1, 13.2, 13.4, 13.6_

  - [ ] 11.3 Implement JSON log export
    - Implement export_logs() generating valid JSON
    - Include all log entries with structured data
    - Support filtering by level, time range, session
    - _Requirements: 13.5_

  - [ ]* 11.4 Write property tests for Logging
    - **Property 32: Validation Logging Completeness**
    - **Validates: Requirements 13.1, 13.2**
    - **Property 33: Log Level Filtering**
    - **Validates: Requirements 13.3**
    - **Property 34: Policy Load Error Logging**
    - **Validates: Requirements 13.4**
    - **Property 35: JSON Log Export Validity**
    - **Validates: Requirements 13.5**
    - **Property 36: Fail-Safe Error Handling**
    - **Validates: Requirements 13.6**

  - [ ]* 11.5 Write unit tests for Logging
    - Test log level filtering
    - Test JSON format validity
    - Test log entry completeness
    - Test fail-safe error handling
    - _Requirements: 13.1, 13.2, 13.3, 13.5, 13.6_

- [ ] 12. Implement Guardian API (Main Entry Point)
  - [ ] 12.1 Create Guardian class
    - Implement constructor accepting policy file path and wasm_tools_dir
    - Initialize PolicyGraph from file
    - Initialize all component instances (validator, session manager, sandbox manager, viz engine)
    - Implement error handling for construction failures
    - _Requirements: 16.1, 16.2, 16.3_

  - [ ] 12.2 Implement main API methods
    - Implement execute_tool() combining validation and sandbox execution
    - Implement validate_tool_call() for policy-only validation
    - Implement create_session() delegating to SessionManager
    - Implement end_session() delegating to SessionManager
    - Return comprehensive results with validation and sandbox information
    - _Requirements: 16.4, 16.5_

  - [ ] 12.3 Implement sandbox management methods
    - Implement load_tool_module() for explicit module loading
    - Implement set_default_sandbox_config() for configuration
    - Support per-tool sandbox configuration
    - _Requirements: 3.9, 4.6_

  - [ ] 12.4 Implement visualization methods
    - Implement visualize_policy() rendering policy graph
    - Implement visualize_session() rendering action sequence
    - Support multiple output formats
    - _Requirements: 8.1, 8.2_

  - [ ] 12.5 Implement configuration methods
    - Implement update_config() for runtime configuration changes
    - Implement get_config() for configuration queries
    - _Requirements: 15.2_

  - [ ]* 12.6 Write property tests for Guardian API
    - **Property 44: Guardian Initialization**
    - **Validates: Requirements 16.3**
    - **Property 45: Validation Result Structure**
    - **Validates: Requirements 16.5**
    - **Property 46: API Misuse Error Messages**
    - **Validates: Requirements 16.8**

  - [ ]* 12.7 Write unit tests for Guardian API
    - Test initialization with valid policy
    - Test initialization with invalid policy
    - Test execute_tool() end-to-end flow
    - Test validate_tool_call() without execution
    - Test session management
    - Test error handling for API misuse
    - _Requirements: 16.3, 16.4, 16.5, 16.8_

- [ ] 13. Checkpoint - Core library complete
  - Ensure all tests pass for Guardian API
  - Verify complete end-to-end flow works
  - Test with sample policy and Wasm modules
  - Ask the user if questions arise

- [ ] 14. Implement Wasm Tools
  - [ ] 14.1 Create Wasm tool source files
    - Create wasm_tools/src/read_accounts.cpp
    - Create wasm_tools/src/send_email.cpp
    - Create wasm_tools/src/generate_report.cpp
    - Create wasm_tools/src/encrypt.cpp
    - Create wasm_tools/src/read_code.cpp
    - Create wasm_tools/src/run_tests.cpp
    - Create wasm_tools/src/approval_request.cpp
    - Create wasm_tools/src/deploy_production.cpp
    - Create wasm_tools/src/search_database.cpp
    - Create wasm_tools/src/infinite_loop_tool.cpp (for DoS demo)
    - Create wasm_tools/src/memory_hog.cpp (for sandbox testing)
    - Each tool exports C functions compatible with WasmEdge
    - _Requirements: 9.1, 9.2, 9.3, 10.1, 10.2, 10.3, 10.4, 10.5_

  - [ ] 14.2 Create Wasm build system
    - Create wasm_tools/build.sh script
    - Configure wasi-sdk for compilation
    - Compile all tools to .wasm with WASI support
    - Add README.md with tool development guide
    - _Requirements: 18.1_

  - [ ] 14.3 Test Wasm tools
    - Verify all tools compile successfully
    - Test tools load in WasmEdge
    - Test parameter passing and result extraction
    - Test file access constraints
    - Test network access constraints
    - _Requirements: 4.1, 4.2, 4.3, 4.4_


- [ ] 15. Create Demo Policy Files
  - [ ] 15.1 Create financial services policy
    - Create policies/financial.json
    - Define nodes: read_accounts, read_transactions, generate_report, encrypt, send_email
    - Mark read_accounts and read_transactions as SENSITIVE_SOURCE
    - Mark send_email as EXTERNAL_DESTINATION
    - Mark generate_report and encrypt as DATA_PROCESSOR
    - Define safe paths: read_accounts → generate_report → encrypt → send_email
    - Include sandbox configs for each tool
    - _Requirements: 9.1, 9.2, 9.3_

  - [ ] 15.2 Create developer assistant policy
    - Create policies/developer.json
    - Define nodes: read_code, run_tests, approval_request, deploy_production
    - Mark deploy_production as EXTERNAL_DESTINATION
    - Define paths: read_code → run_tests (allowed), read_code → approval_request → deploy_production (allowed)
    - Block direct read_code → deploy_production
    - Include sandbox configs for each tool
    - _Requirements: 10.1, 10.2, 10.3, 10.4_

  - [ ] 15.3 Create DoS prevention policy
    - Create policies/dos_prevention.json
    - Define nodes: search_database, infinite_loop_tool
    - Configure cycle detection threshold (10 for search_database)
    - Configure timeout for infinite_loop_tool (5000ms)
    - _Requirements: 11.1, 11.2, 11.3_

- [ ] 16. Implement CLI Demo Tool
  - [ ] 16.1 Create CLI main entry point
    - Create cli/main.cpp with argument parsing
    - Support --scenario flag (financial, developer, dos)
    - Support --policy flag for custom policy files
    - Support --interactive flag for manual mode
    - Support --output flag for visualization output
    - _Requirements: 17.1, 17.2, 17.3, 17.7, 17.8_

  - [ ] 16.2 Implement terminal UI utilities
    - Create cli/terminal_ui.cpp with colored output
    - Implement functions for green (approved) and red (blocked) output
    - Implement ASCII art graph rendering
    - Implement progress indicators
    - _Requirements: 17.5, 17.6_

  - [ ] 16.3 Implement demo scenarios
    - Create cli/scenarios.cpp with scenario implementations
    - Implement run_financial_demo() showing exfiltration blocking
    - Implement run_developer_demo() showing deployment control
    - Implement run_dos_demo() showing cycle detection
    - Each scenario displays real-time validation decisions
    - Each scenario shows both policy and sandbox enforcement
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 10.1, 10.2, 10.3, 10.4, 10.5, 11.1, 11.2, 11.3, 11.4, 11.5_

  - [ ] 16.4 Implement interactive mode
    - Accept manual tool call input from user
    - Display validation results in real-time
    - Show current action sequence
    - Display policy graph visualization
    - _Requirements: 17.8_

  - [ ] 16.5 Implement summary statistics
    - Track total calls, blocked calls, violation types
    - Display summary at demo completion
    - Include execution times and sandbox metrics
    - _Requirements: 17.9_

  - [ ]* 16.6 Write property tests for CLI tool
    - **Property 47: CLI Scenario Parsing**
    - **Validates: Requirements 17.2, 17.3**
    - **Property 48: CLI Custom Policy Loading**
    - **Validates: Requirements 17.7**
    - **Property 49: CLI Summary Statistics**
    - **Validates: Requirements 17.9**

  - [ ]* 16.7 Write unit tests for CLI tool
    - Test scenario flag parsing
    - Test custom policy loading
    - Test summary statistics calculation
    - Test colored output generation
    - _Requirements: 17.2, 17.3, 17.7, 17.9_

- [ ] 17. Create Integration Examples
  - [ ] 17.1 Create basic integration example
    - Create examples/basic_integration.cpp
    - Show Guardian initialization
    - Show tool call validation and execution
    - Show session management
    - Include error handling
    - _Requirements: 18.1, 18.6_

  - [ ] 17.2 Create custom policy example
    - Create examples/custom_policy.cpp
    - Show programmatic policy graph creation
    - Show node and edge addition
    - Show policy serialization
    - _Requirements: 18.4_

  - [ ] 17.3 Create concurrent sessions example
    - Create examples/concurrent_sessions.cpp
    - Show multiple concurrent sessions
    - Demonstrate session isolation
    - Show session persistence
    - _Requirements: 18.5_

  - [ ] 17.4 Create sandbox configuration example
    - Create examples/sandbox_config.cpp
    - Show per-tool sandbox configuration
    - Demonstrate constraint enforcement
    - Show violation handling
    - _Requirements: 3.2, 3.3, 3.4, 3.5_

  - [ ] 17.5 Verify all examples compile and run
    - Test each example with provided build instructions
    - Verify examples demonstrate best practices
    - Ensure examples include proper error handling
    - _Requirements: 18.6_

- [ ] 18. Checkpoint - Demos and examples complete
  - Ensure all CLI demos run successfully
  - Verify all integration examples work
  - Test with all three demo scenarios
  - Ask the user if questions arise


- [ ] 19. Performance Optimization and Testing
  - [ ] 19.1 Implement performance optimizations
    - Add validation result caching with LRU eviction
    - Implement string interning for tool names
    - Add memory pooling for ToolCall objects
    - Optimize graph traversal algorithms
    - Add Wasm module caching
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5_

  - [ ] 19.2 Create performance benchmarks
    - Benchmark policy validation latency (<10ms for 50 nodes)
    - Benchmark sandbox overhead (<50ms for simple tools)
    - Benchmark throughput (>100 validations/second)
    - Benchmark memory usage (<100MB for 1000 tool calls)
    - Test with graphs up to 200 nodes
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

  - [ ]* 19.3 Write performance tests
    - Test validation completes within 10ms for 50-node graphs
    - Test validation completes within 50ms for 200-node graphs
    - Test sandbox overhead is <50ms for simple tools
    - Test memory usage stays under 100MB for 1000 tool calls
    - Test throughput exceeds 100 validations/second
    - Test visualization completes within 2s for 100-node graphs
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 8.7_

- [ ] 20. Integration Testing
  - [ ]* 20.1 Write end-to-end integration tests
    - Test complete financial exfiltration scenario (policy blocks direct path)
    - Test complete developer deployment scenario (requires approval)
    - Test complete DoS prevention scenario (cycle detection)
    - Test sandbox blocks unauthorized file access
    - Test sandbox blocks network access when disabled
    - Test sandbox enforces memory limits
    - Test sandbox enforces timeouts
    - Test concurrent session handling
    - Test session persistence and recovery
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 10.1, 10.2, 10.3, 10.4, 11.1, 11.2, 11.3, 3.1, 3.2, 3.3, 3.4, 3.5, 11.3, 11.4_

  - [ ]* 20.2 Write error handling integration tests
    - Test policy file not found
    - Test invalid policy format
    - Test Wasm module not found
    - Test invalid Wasm bytecode
    - Test invalid session ID
    - Test internal error fail-safe behavior
    - _Requirements: 13.4, 13.6, 16.8_

- [ ] 21. Documentation and Polish
  - [ ] 21.1 Create main README.md
    - Overview of Guardian AI
    - Quick start guide
    - Installation instructions
    - Basic usage examples
    - Link to examples and demos
    - _Requirements: 16.1, 16.7_

  - [ ] 21.2 Create API documentation
    - Document Guardian class public API
    - Document all public structs and enums
    - Include usage examples for each method
    - Document error handling patterns
    - _Requirements: 16.1, 16.2, 16.7, 16.8_

  - [ ] 21.3 Create Wasm tool development guide
    - Create wasm_tools/README.md
    - Explain how to write Wasm tools
    - Document parameter passing conventions
    - Explain sandbox constraints
    - Include example tool implementations
    - _Requirements: 18.1, 18.2_

  - [ ] 21.4 Create policy authoring guide
    - Document JSON policy format
    - Document DOT policy format
    - Explain node types and their meanings
    - Explain sandbox configuration options
    - Include example policies
    - _Requirements: 1.7, 10.1, 10.2_

  - [ ] 21.5 Create demo scenario documentation
    - Document each demo scenario
    - Explain what each scenario demonstrates
    - Include expected output
    - Explain policy enforcement decisions
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 10.1, 10.2, 10.3, 10.4, 10.5, 11.1, 11.2, 11.3, 11.4, 11.5_

- [ ] 22. Final Testing and Validation
  - [ ]* 22.1 Run complete test suite
    - Run all unit tests
    - Run all property-based tests (100+ iterations each)
    - Run all integration tests
    - Run all performance benchmarks
    - Verify all tests pass

  - [ ] 22.2 Verify demo scenarios
    - Run financial demo and verify output
    - Run developer demo and verify output
    - Run DoS demo and verify output
    - Verify demos complete within 90 seconds
    - _Requirements: 9.5, 10.5, 11.5, 17.10_

  - [ ] 22.3 Verify examples compile and run
    - Build all examples
    - Run each example and verify output
    - Verify examples follow best practices
    - _Requirements: 18.6_

  - [ ] 22.4 Code quality checks
    - Run static analysis (clang-tidy)
    - Check for memory leaks (valgrind)
    - Verify thread safety
    - Check code formatting

- [ ] 23. Final checkpoint - Project complete
  - All tests passing
  - All demos working
  - All examples functional
  - Documentation complete
  - Ready for release

## Notes

- Tasks marked with `*` are optional testing tasks that can be skipped for faster MVP
- Each task references specific requirements for traceability
- Property tests validate universal correctness properties using RapidCheck
- Unit tests validate specific examples and edge cases
- Checkpoints ensure incremental validation throughout development
- The implementation prioritizes core functionality first, then demos and polish
- WasmEdge sandbox integration is a critical MVP feature tested throughout
- All code should follow C++17 standards and best practices
- Thread safety is required for SessionManager and SandboxManager
- Error handling follows fail-safe principles (reject on error)

## Build Instructions

```bash
# Install dependencies
# - CMake 3.15+
# - C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
# - nlohmann_json
# - WasmEdge SDK
# - wasi-sdk (for compiling Wasm tools)
# - Optional: Graphviz (for visualization)

# Build library and CLI tool
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run demos
./guardian-demo --scenario=financial
./guardian-demo --scenario=developer
./guardian-demo --scenario=dos

# Build Wasm tools
cd ../wasm_tools
./build.sh
```

## Testing Strategy

Guardian AI uses a dual testing approach:

1. **Unit Tests**: Verify specific examples, edge cases, and error conditions
2. **Property-Based Tests**: Verify universal properties across randomized inputs using RapidCheck

Property tests are marked with `*` and can be skipped for faster iteration, but should be run before release to ensure comprehensive correctness validation.

## Performance Targets

- Policy validation: <10ms for 50-node graphs, <50ms for 200-node graphs
- Sandbox overhead: <50ms for simple tools (module loading + VM instantiation)
- Total latency: Validation + tool execution time (tool-dependent)
- Memory: <100MB base + per-tool Wasm module size
- Throughput: >100 validations/second
- Visualization: <2s for 100-node graphs

## Security Considerations

- All tool execution happens in isolated WasmEdge VMs
- Sandbox constraints are enforced at runtime (memory, timeout, file access, network)
- Policy validation uses fail-safe defaults (reject on error)
- Session isolation prevents cross-session interference
- Comprehensive logging enables security auditing
- Policy graphs should be reviewed by security administrators

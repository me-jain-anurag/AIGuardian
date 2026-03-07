# Requirements Document

## Introduction

Guardian AI is a runtime security copilot for AI agents that intercepts and validates tool call sequences against policy graphs to prevent dangerous action patterns. The system addresses the critical gap in AI agent security where individual tool calls may be safe, but their sequences can be exploited for data exfiltration, denial of service, or unauthorized operations. Guardian AI implements a two-layer security model: policy validation ensures only approved tool sequences execute, while WasmEdge sandboxing provides execution isolation to contain damage even when policy allows a tool.

## Deployment Model
    
Guardian AI is designed as a **C++ library** that can be integrated into AI agent applications, with a **CLI demonstration tool** for showcasing capabilities. The library provides a clean API for intercepting and validating tool calls, while the CLI tool demonstrates the security capabilities through interactive scenarios.

**Primary Distribution:** C++ header-only or shared library
**Integration Pattern:** Developers embed Guardian into their AI agent code (LangChain, AutoGen, custom agents)
**Demo Format:** Command-line tool that runs pre-configured attack scenarios
**Future Vision:** 
- HTTP API service for language-agnostic integration (post-MVP)
- Python bindings via pybind11 for seamless Python integration (post-MVP)
- Integration adapters for popular frameworks (post-MVP)

**Two-Layer Security Model:**
1. **Policy Validation Layer**: Graph-based sequence validation ensures only approved tool call patterns execute
2. **Execution Sandboxing Layer**: WasmEdge runtime isolation contains tool execution with configurable memory, timeout, file access, and network constraints

**Security Positioning:** Guardian AI is a **defense-in-depth layer** that complements existing security tools:
- Works alongside prompt injection filters (Lakera Guard, NeMo Guardrails)
- Operates above infrastructure isolation (Docker, Firecracker)
- Adds behavioral sequence validation to content-level and access-control security
- Provides execution sandboxing to limit blast radius of approved tools

## Glossary

- **Guardian_AI**: The complete runtime security system for AI agents
- **Policy_Graph**: A directed graph where nodes represent tools and edges represent allowed transitions between tool calls
- **Tool_Interceptor**: Component that captures tool calls before execution
- **Policy_Validator**: Component that evaluates whether an action sequence violates policy rules
- **Sandbox_Manager**: Component that executes approved tools in isolated WasmEdge environments
- **Wasm_Module**: A WebAssembly binary containing tool implementation code
- **Sandbox_Config**: Configuration specifying execution constraints for a tool (memory, timeout, file access, network)
- **Action_Sequence**: An ordered list of tool calls made by an AI agent during a session
- **Tool_Call**: A single invocation of a tool/function by an AI agent
- **Dangerous_Pattern**: A sequence of tool calls that violates security policy
- **Session**: A continuous interaction period with an AI agent tracked by Guardian AI
- **Cycle**: A repeating sequence of tool calls that may indicate denial of service
- **Exfiltration_Path**: A sequence of tool calls that moves sensitive data to external destinations
- **Safe_Path**: An allowed sequence of tool calls defined in the policy graph
- **Visualization_Engine**: Component that renders policy graphs and highlights violations

## Requirements

### Requirement 1: Policy Graph Definition

**User Story:** As a security administrator, I want to define allowed tool call sequences as a directed graph, so that I can specify which action transitions are permitted.

#### Acceptance Criteria

1. THE Policy_Graph SHALL represent tools as nodes and allowed transitions as directed edges
2. WHEN a Policy_Graph is created, THE Policy_Graph SHALL support adding tool nodes with unique identifiers
3. WHEN a Policy_Graph is created, THE Policy_Graph SHALL support adding directed edges between tool nodes
4. THE Policy_Graph SHALL store metadata for each node including tool name and risk level
5. THE Policy_Graph SHALL store metadata for each edge including transition conditions
6. THE Policy_Graph SHALL support node types including NORMAL, SENSITIVE_SOURCE, EXTERNAL_DESTINATION, and DATA_PROCESSOR
7. WHEN a Policy_Graph is serialized, THE Policy_Graph SHALL output to a standard format
8. WHEN a Policy_Graph is deserialized, THE Policy_Graph SHALL reconstruct from the standard format
9. FOR ALL valid Policy_Graph objects, serializing then deserializing SHALL produce an equivalent graph (round-trip property)

### Requirement 2: Tool Call Interception

**User Story:** As a security system, I want to intercept tool calls before execution, so that I can validate them against policy before allowing execution.

#### Acceptance Criteria

1. WHEN an AI agent invokes a tool, THE Tool_Interceptor SHALL capture the tool call before execution
2. THE Tool_Interceptor SHALL extract tool name, parameters, and timestamp from each tool call
3. THE Tool_Interceptor SHALL maintain the Action_Sequence for the current Session
4. WHEN a tool call is intercepted, THE Tool_Interceptor SHALL pass it to the Policy_Validator
5. WHEN the Policy_Validator approves a tool call, THE Tool_Interceptor SHALL pass it to the Sandbox_Manager for execution
6. WHEN the Policy_Validator rejects a tool call, THE Tool_Interceptor SHALL block execution and return an error
7. THE Tool_Interceptor SHALL preserve the original tool call parameters for approved executions

### Requirement 3: WasmEdge Sandbox Execution

**User Story:** As a security system, I want to execute approved tools in isolated WasmEdge sandboxes, so that even if policy allows a tool, it cannot cause damage beyond its sandbox constraints.

#### Acceptance Criteria

1. WHEN a tool call is approved by the Policy_Validator, THE Sandbox_Manager SHALL execute the tool in a WasmEdge VM
2. THE Sandbox_Manager SHALL apply memory limits specified in the Sandbox_Config
3. THE Sandbox_Manager SHALL apply timeout limits specified in the Sandbox_Config
4. THE Sandbox_Manager SHALL apply file access controls specified in the Sandbox_Config
5. THE Sandbox_Manager SHALL apply network access policies specified in the Sandbox_Config
6. WHEN a sandbox constraint is violated during execution, THE Sandbox_Manager SHALL terminate the execution and log the violation
7. WHEN a sandbox constraint is violated, THE Sandbox_Manager SHALL return an error to the AI agent
8. WHEN tool execution completes successfully, THE Sandbox_Manager SHALL return the result to the AI agent
9. THE Sandbox_Manager SHALL isolate each tool execution in a separate WasmEdge VM instance

### Requirement 4: Sandbox Configuration

**User Story:** As a security administrator, I want to configure sandbox constraints per tool, so that I can enforce fine-grained execution policies.

#### Acceptance Criteria

1. WHEN a Policy_Graph node is created, THE Policy_Graph SHALL support including a Sandbox_Config
2. THE Sandbox_Config SHALL support specifying memory limits in megabytes
3. THE Sandbox_Config SHALL support specifying timeout limits in milliseconds
4. THE Sandbox_Config SHALL support specifying allowed file paths as a list of path patterns
5. THE Sandbox_Config SHALL support specifying network access as allow or deny
6. WHEN a Sandbox_Config is invalid or missing, THE Sandbox_Manager SHALL use safe default values
7. THE safe default values SHALL be: 128MB memory, 5000ms timeout, no file access, no network access
8. THE Sandbox_Config SHALL be serialized and deserialized with the Policy_Graph

### Requirement 5: Wasm Tool Loading

**User Story:** As a system integrator, I want to load tools as Wasm modules, so that they execute in isolated sandboxes.

#### Acceptance Criteria

1. WHEN a Policy_Graph node specifies a Wasm_Module path, THE Sandbox_Manager SHALL load the .wasm file
2. WHEN a Wasm_Module fails to load, THE Sandbox_Manager SHALL log the error and reject tool execution
3. THE Sandbox_Manager SHALL support passing parameters to Wasm_Module functions as JSON
4. THE Sandbox_Manager SHALL support receiving results from Wasm_Module functions as JSON
5. WHEN a Wasm_Module contains invalid bytecode, THE Sandbox_Manager SHALL return a descriptive error
6. THE Sandbox_Manager SHALL cache loaded Wasm_Module instances for performance
7. WHEN a Wasm_Module is updated on disk, THE Sandbox_Manager SHALL reload it on the next execution

### Requirement 6: Sequence Validation

**User Story:** As a security system, I want to validate action sequences against the policy graph, so that I can detect and block dangerous patterns.

#### Acceptance Criteria

1. WHEN a tool call is intercepted, THE Policy_Validator SHALL check if the transition from the previous tool to the current tool exists in the Policy_Graph
2. WHEN no previous tool exists in the Session, THE Policy_Validator SHALL allow any tool call defined in the Policy_Graph
3. WHEN a transition is not defined in the Policy_Graph, THE Policy_Validator SHALL reject the tool call
4. WHEN a transition is defined in the Policy_Graph, THE Policy_Validator SHALL approve the tool call
5. THE Policy_Validator SHALL maintain the current position in the Action_Sequence
6. WHEN validating a sequence, THE Policy_Validator SHALL return a boolean approval status and a reason message
7. FOR ALL Action_Sequences, validation SHALL be deterministic given the same Policy_Graph

### Requirement 7: Cycle Detection

**User Story:** As a security administrator, I want to detect infinite loops in tool call sequences, so that I can prevent denial of service attacks.

#### Acceptance Criteria

1. WHEN an Action_Sequence contains repeated tool calls, THE Policy_Validator SHALL detect cycles
2. WHEN a cycle is detected with length exceeding the configured threshold, THE Policy_Validator SHALL reject the tool call
3. THE Policy_Validator SHALL support configurable cycle detection thresholds per tool type
4. WHEN a cycle is detected, THE Policy_Validator SHALL include cycle information in the rejection reason
5. THE Policy_Validator SHALL distinguish between legitimate repeated calls and malicious loops
6. FOR ALL Action_Sequences with cycles, detection SHALL identify the cycle start and length

### Requirement 8: Exfiltration Pattern Detection

**User Story:** As a security administrator, I want to block direct data exfiltration paths, so that sensitive data cannot be sent to external destinations without proper processing.

#### Acceptance Criteria

1. WHEN a Policy_Graph is defined, THE Policy_Graph SHALL support marking nodes as sensitive data sources
2. WHEN a Policy_Graph is defined, THE Policy_Graph SHALL support marking nodes as external destinations
3. WHEN a Policy_Graph is defined, THE Policy_Graph SHALL support marking nodes as data processors (DATA_PROCESSOR type) that represent safe intermediate processing steps
4. WHEN an Action_Sequence attempts a direct path from a sensitive source to an external destination, THE Policy_Validator SHALL reject the tool call
5. WHEN an Action_Sequence follows a Safe_Path through data processing nodes (DATA_PROCESSOR type), THE Policy_Validator SHALL approve the tool call
6. THE Policy_Validator SHALL identify Exfiltration_Path patterns including file_read followed by network_post
7. THE Policy_Validator SHALL allow safe alternatives such as file_read → data_process → encrypt → network_post

### Requirement 9: Policy Graph Visualization

**User Story:** As a security administrator, I want to visualize the policy graph and action sequences, so that I can understand policy enforcement and debug violations.

#### Acceptance Criteria

1. THE Visualization_Engine SHALL render the Policy_Graph as a directed graph diagram
2. WHEN an Action_Sequence is provided, THE Visualization_Engine SHALL highlight the path taken through the Policy_Graph
3. WHEN a violation occurs, THE Visualization_Engine SHALL highlight the rejected transition in a distinct color
4. THE Visualization_Engine SHALL display node metadata including tool names and risk levels
5. THE Visualization_Engine SHALL display edge metadata including transition conditions
6. THE Visualization_Engine SHALL output visualizations in a standard image format
7. WHEN a visualization is generated, THE Visualization_Engine SHALL complete rendering within 2 seconds for graphs with up to 100 nodes

### Requirement 10: Demo Scenario - Financial Data Exfiltration

**User Story:** As a product demonstrator, I want to show Guardian AI blocking bulk account data exfiltration, so that stakeholders understand the security value.

#### Acceptance Criteria

1. THE Guardian_AI SHALL include a demo scenario with a financial services policy graph
2. WHEN the demo executes read_accounts followed by send_email, THE Policy_Validator SHALL reject the sequence
3. WHEN the demo executes read_accounts → generate_report → encrypt → send_email, THE Policy_Validator SHALL approve the sequence
4. THE demo SHALL display before and after comparisons showing the blocked attack
5. THE demo SHALL complete execution within 60 seconds

### Requirement 11: Demo Scenario - Developer Assistant Safety

**User Story:** As a product demonstrator, I want to show Guardian AI allowing code operations while blocking unauthorized production deployment, so that stakeholders see practical developer use cases.

#### Acceptance Criteria

1. THE Guardian_AI SHALL include a demo scenario with a developer assistant policy graph
2. WHEN the demo executes read_code → run_tests, THE Policy_Validator SHALL approve the sequence
3. WHEN the demo executes read_code → deploy_production without approval_request, THE Policy_Validator SHALL reject the sequence
4. WHEN the demo executes read_code → approval_request → deploy_production, THE Policy_Validator SHALL approve the sequence
5. THE demo SHALL display the policy enforcement in real-time

### Requirement 12: Demo Scenario - Infinite Loop Prevention

**User Story:** As a product demonstrator, I want to show Guardian AI detecting and blocking infinite tool call loops, so that stakeholders understand DoS prevention capabilities.

#### Acceptance Criteria

1. THE Guardian_AI SHALL include a demo scenario demonstrating cycle detection
2. WHEN the demo executes a tool call repeated more than 10 times consecutively, THE Policy_Validator SHALL reject the tool call
3. WHEN the demo executes legitimate repeated calls below the threshold, THE Policy_Validator SHALL approve the calls
4. THE demo SHALL visualize the detected cycle in the action sequence
5. THE demo SHALL show the cycle detection threshold being enforced

### Requirement 13: Policy Graph Parsing and Serialization

**User Story:** As a security administrator, I want to define policies in a human-readable format, so that I can easily create and modify security rules.

#### Acceptance Criteria

1. THE Policy_Graph SHALL support parsing from a JSON format
2. THE Policy_Graph SHALL support parsing from a DOT graph format
3. WHEN an invalid policy file is provided, THE Policy_Graph SHALL return descriptive error messages
4. THE Policy_Graph SHALL support serializing to JSON format
5. THE Policy_Graph SHALL support serializing to DOT graph format
6. FOR ALL valid Policy_Graph objects, parsing then serializing then parsing SHALL produce an equivalent graph (round-trip property)
7. WHEN a policy file contains syntax errors, THE Policy_Graph SHALL identify the error location

### Requirement 14: Session Management

**User Story:** As a security system, I want to track AI agent sessions independently, so that I can maintain separate action sequences for concurrent agents.

#### Acceptance Criteria

1. THE Guardian_AI SHALL support creating new Session instances with unique identifiers
2. WHEN a Session is created, THE Guardian_AI SHALL initialize an empty Action_Sequence
3. THE Guardian_AI SHALL maintain separate Action_Sequences for each active Session
4. WHEN a Session ends, THE Guardian_AI SHALL persist the Action_Sequence for audit purposes
5. THE Guardian_AI SHALL support querying Action_Sequences by Session identifier
6. WHEN concurrent Sessions exist, THE Guardian_AI SHALL validate each independently without interference
7. THE Guardian_AI SHALL support configurable session timeout, after which inactive Sessions are automatically ended and persisted
8. THE Guardian_AI SHALL enforce a configurable maximum number of concurrent active Sessions to prevent resource exhaustion
9. THE Sandbox_Manager SHALL enforce a configurable maximum number of concurrent WasmEdge VM instances to prevent host resource exhaustion

### Requirement 15: Performance Requirements

**User Story:** As a system integrator, I want Guardian AI to validate tool calls with minimal latency, so that it doesn't degrade AI agent responsiveness.

#### Acceptance Criteria

1. WHEN a tool call is intercepted, THE Policy_Validator SHALL complete validation within 10 milliseconds for Policy_Graphs with up to 50 nodes
2. WHEN a tool call is intercepted, THE Policy_Validator SHALL complete validation within 50 milliseconds for Policy_Graphs with up to 200 nodes
3. THE Tool_Interceptor SHALL add no more than 5 milliseconds of overhead per tool call
4. THE Guardian_AI SHALL support validating at least 100 tool calls per second on standard hardware
5. WHEN memory usage is measured, THE Guardian_AI SHALL consume no more than 100MB for a Session with 1000 tool calls
6. WHEN tool execution includes WasmEdge sandboxing, THE Sandbox_Manager SHALL add no more than 50 milliseconds of overhead for simple tools
7. THE total latency for policy validation plus sandbox execution SHALL depend on tool complexity and configured constraints

### Requirement 16: Error Handling and Logging

**User Story:** As a security administrator, I want detailed logs of policy violations and sandbox violations, so that I can audit security incidents and refine policies.

#### Acceptance Criteria

1. WHEN a tool call is rejected by policy, THE Guardian_AI SHALL log the violation with timestamp, tool name, and reason
2. WHEN a tool call is rejected by sandbox constraints, THE Guardian_AI SHALL log the violation with timestamp, tool name, constraint type, and limit exceeded
3. WHEN a tool call is approved, THE Guardian_AI SHALL log the approval with timestamp and tool name
4. THE Guardian_AI SHALL support configurable log levels including DEBUG, INFO, WARN, and ERROR
5. WHEN a Policy_Graph fails to load, THE Guardian_AI SHALL log descriptive error messages
6. WHEN a Wasm_Module fails to load, THE Guardian_AI SHALL log descriptive error messages
7. THE Guardian_AI SHALL support exporting logs in JSON format
8. WHEN an internal error occurs, THE Guardian_AI SHALL log the error and fail safely by rejecting the tool call

### Requirement 17: Path Validation Algorithm

**User Story:** As a security system, I want to efficiently validate if a sequence follows allowed paths, so that I can enforce complex multi-step policies.

#### Acceptance Criteria

1. WHEN validating an Action_Sequence, THE Policy_Validator SHALL use graph traversal to check path validity
2. THE Policy_Validator SHALL support validating paths of arbitrary length within the Policy_Graph
3. WHEN multiple valid paths exist, THE Policy_Validator SHALL approve if any valid path matches
4. THE Policy_Validator SHALL cache validation results for repeated sequence patterns
5. FOR ALL Action_Sequences, path validation SHALL have time complexity no worse than O(n*m) where n is sequence length and m is average node degree
6. WHEN a path validation completes, THE Policy_Validator SHALL return the matched path or the violation point

### Requirement 18: Configuration Management

**User Story:** As a system administrator, I want to configure Guardian AI behavior through configuration files, so that I can customize it for different deployment environments.

#### Acceptance Criteria

1. THE Guardian_AI SHALL load configuration from a JSON configuration file
2. THE Guardian_AI SHALL support configuring cycle detection thresholds
3. THE Guardian_AI SHALL support configuring performance limits including timeout values
4. THE Guardian_AI SHALL support configuring log levels and output destinations
5. THE Guardian_AI SHALL support configuring policy graph file paths
6. WHEN an invalid configuration is provided, THE Guardian_AI SHALL use safe default values and log warnings
7. FOR ALL valid configuration objects, parsing then serializing then parsing SHALL produce equivalent configuration (round-trip property)

### Requirement 19: Library API Design

**User Story:** As an AI agent developer, I want a clean C++ API to integrate Guardian AI into my application, so that I can add runtime security with minimal code changes.

#### Acceptance Criteria

1. THE Guardian_AI SHALL provide a C++ header file exposing the public API
2. THE Guardian_AI SHALL provide a `Guardian` class as the main entry point
3. THE `Guardian` class SHALL support initialization with a policy graph file path
4. THE `Guardian` class SHALL provide a `validate_tool_call()` method accepting tool name, parameters, and session ID
5. THE `validate_tool_call()` method SHALL return a validation result containing approval status, reason message, and suggested alternatives
6. THE Guardian_AI SHALL support both header-only and shared library distribution modes
7. THE Guardian_AI SHALL provide example integration code demonstrating typical usage patterns
8. WHEN the library is used incorrectly, THE Guardian_AI SHALL provide clear error messages at compile time or runtime
9. THE Guardian_AI SHALL maintain API stability with semantic versioning

### Requirement 20: CLI Demo Tool

**User Story:** As a product demonstrator, I want a command-line tool that runs Guardian AI demos, so that I can showcase security capabilities without requiring code integration.

#### Acceptance Criteria

1. THE Guardian_AI SHALL include a CLI executable named `guardian-demo`
2. THE CLI tool SHALL support running pre-configured demo scenarios via command-line flags
3. THE CLI tool SHALL support `--scenario=financial`, `--scenario=developer`, and `--scenario=dos` options
4. WHEN a demo scenario runs, THE CLI tool SHALL display real-time output showing tool calls, validation decisions, and policy enforcement
5. THE CLI tool SHALL use colored terminal output to highlight approved (green) and blocked (red) actions
6. THE CLI tool SHALL display ASCII art visualization of the policy graph and action sequence
7. THE CLI tool SHALL support a `--policy` flag to load custom policy graphs
8. THE CLI tool SHALL support a `--interactive` mode allowing manual tool call input
9. WHEN a demo completes, THE CLI tool SHALL display summary statistics including total calls, blocked calls, and violation types
10. THE CLI tool SHALL complete each demo scenario within 90 seconds

### Requirement 21: Integration Examples

**User Story:** As an AI agent developer, I want example code showing how to integrate Guardian AI, so that I can quickly add it to my existing agent application.

#### Acceptance Criteria

1. THE Guardian_AI SHALL include example code for integrating with a generic AI agent
2. THE Guardian_AI SHALL include example code for wrapping tool execution functions
3. THE Guardian_AI SHALL include example code for handling validation results and blocked calls
4. THE Guardian_AI SHALL include example code for creating custom policy graphs programmatically
5. THE Guardian_AI SHALL include example code for session management with concurrent agents
6. THE example code SHALL compile and run successfully with the provided build instructions
7. THE example code SHALL demonstrate best practices for error handling and logging


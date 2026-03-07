# AIGuardian — Zero-Trust Security for AI Agents

AIGuardian is a security middleware designed to enforce strict, verifiable policy graphs on AI agents. It ensures that agents follow approved sequential tool-calling paths and remain within a secure, resource-constrained sandbox.

## 🚀 Key Features

- **Policy Graph Enforcement**: Define tool-calling rulebooks as directed graphs.
- **Zero-Trust Sandbox**: Execute third-party tools in a secure WasmEdge (WASI) environment.
- **Intelligent Validation**: Real-time detection of exfiltration paths and DoS cycles.
- **Observability**: Rich logging (JSON/Text) and ASCII/Graphviz policy visualization.
- **Cross-Platform**: Built in C++17 for Windows and Linux.

## 🛠 Quick Start

### Prerequisites

- CMake 3.16+
- C++17 compatible compiler (GCC 9+, MSVC 2019+, Clang 10+)
- Git

### Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Run Tests
```bash
ctest --output-on-failure
```

### Run the Interactive Demo
```bash
./bin/guardian_cli --interactive --policy-path ../policies/financial.json
```

### Windows Setup

If you encounter "blocked by your organization's Device Guard policy" errors on Windows, see **[Windows Setup Guide](docs/WINDOWS_SETUP.md)** for Defender exclusion instructions.

## 🏗 Architecture

AIGuardian consists of four primary components:
1. **Guardian Core**: The main API that wires everything together.
2. **Policy Validator**: The algorithmic heart that enforces graph rules.
3. **Sandbox Manager**: Handles secure execution of tool modules.
4. **Visualization & Logging**: Provides transparency into agent behavior.

## 📂 Project Structure

- `include/guardian/` - Public headers
- `src/` - Implementation files
- `tests/unit/` - Unit tests (Catch2 + RapidCheck)
- `tests/integration/` - Integration test scenarios
- `examples/` - Usage examples
- `policies/` - Demo policy files

## 📚 Documentation

- [API Reference](docs/api.md)
- [Policy Authoring Guide](docs/policy_guide.md)
- [Demo Scenario Guide](docs/demo_guide.md)

## ⚖️ License
MIT License - Copyright (c) 2026 CodeGeass Team.

## CodeGeass Team Members
- **me-jain-anurag**
- **Haruto1632**
- **Ao-chuba**
- **No-Reed**

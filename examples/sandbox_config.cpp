#include <fstream>
// examples/sandbox_config.cpp
// Owner: Dev B
// Task 10 — Demonstrates SandboxManager with per-tool configuration,
// constraint enforcement, and violation handling using MockRuntime.
//
// Build:
//   cmake --build build --target sandbox_config_demo
// Run:
//   ./build/bin/sandbox_config_demo

#include "guardian/sandbox_manager.hpp"
#include <iostream>
#include <iomanip>
#include <string>

using namespace guardian;

// ============================================================================
// Helpers
// ============================================================================

static const char* violation_type_str(SandboxViolation::Type t) {
    switch (t) {
        case SandboxViolation::MEMORY_EXCEEDED:    return "MEMORY_EXCEEDED";
        case SandboxViolation::TIMEOUT:            return "TIMEOUT";
        case SandboxViolation::FILE_ACCESS_DENIED: return "FILE_ACCESS_DENIED";
        case SandboxViolation::NETWORK_DENIED:     return "NETWORK_DENIED";
    }
    return "UNKNOWN";
}

static void print_result(const std::string& label, const SandboxResult& r) {
    std::cout << "\n--- " << label << " ---\n";
    std::cout << "  success:    " << (r.success ? "true" : "false") << "\n";
    std::cout << "  output:     " << r.output << "\n";
    if (r.error.has_value()) {
        std::cout << "  error:      " << r.error.value() << "\n";
    }
    std::cout << "  memory:     " << r.memory_used_bytes << " bytes\n";
    std::cout << "  exec time:  " << r.execution_time_ms << " ms\n";
    if (r.violation.has_value()) {
        std::cout << "  VIOLATION:  " << violation_type_str(r.violation->type)
                  << " — " << r.violation->details << "\n";
    }
}

static void print_config(const std::string& label, const SandboxConfig& cfg) {
    std::cout << "\n[Config: " << label << "]\n";
    std::cout << "  memory_limit_mb: " << cfg.memory_limit_mb << "\n";
    std::cout << "  timeout_ms:      " << cfg.timeout_ms << "\n";
    std::cout << "  network_access:  " << (cfg.network_access ? "true" : "false") << "\n";
    std::cout << "  allowed_paths:   ";
    if (cfg.allowed_paths.empty()) {
        std::cout << "(none)";
    } else {
        for (const auto& p : cfg.allowed_paths) std::cout << p << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    try {
        std::cout << std::string(60, '=') << "\n";
        std::cout << "  Guardian AI — Sandbox Configuration Demo\n";
        std::cout << std::string(60, '=') << "\n";

        // -----------------------------------------------------------------------
        // 1. Create a SandboxManager with a MockRuntime factory.
        //    In production you would use WasmEdgeRuntime; MockRuntime lets us
        //    run this demo without WasmEdge installed.
        // -----------------------------------------------------------------------

        // The factory creates a fresh MockRuntime for each loaded tool.
        // We capture raw pointers so we can queue results for the demo.
        MockRuntime* encrypt_mock   = nullptr;
        MockRuntime* email_mock     = nullptr;
        MockRuntime* report_mock    = nullptr;
        MockRuntime* compute_mock   = nullptr;

        RuntimeFactory factory = [&](const std::string& /*path*/,
                                     const SandboxConfig& /*cfg*/)
            -> std::unique_ptr<IWasmRuntime>
        {
            return std::make_unique<MockRuntime>();
        };

        SandboxManager mgr("wasm_tools", factory);

        // -----------------------------------------------------------------------
        // 2. Set a global default sandbox config (safe defaults).
        // -----------------------------------------------------------------------

        SandboxConfig defaults = SandboxConfig::safe_defaults();
        mgr.set_default_config(defaults);

        std::cout << "\n>> Step 1: Global defaults";
        print_config("global default", mgr.get_config_for_tool("any_tool"));

        // -----------------------------------------------------------------------
        // 3. Override config per-tool for specialised requirements.
        // -----------------------------------------------------------------------

        // encrypt: strict memory (32 MB), short timeout (2 s), no file/network.
        SandboxConfig encrypt_cfg;
        encrypt_cfg.memory_limit_mb = 32;
        encrypt_cfg.timeout_ms      = 2000;
        encrypt_cfg.network_access  = false;
        mgr.set_tool_config("encrypt", encrypt_cfg);

        // send_email: needs network, moderate timeout.
        SandboxConfig email_cfg;
        email_cfg.memory_limit_mb = 64;
        email_cfg.timeout_ms      = 3000;
        email_cfg.network_access  = true;
        mgr.set_tool_config("send_email", email_cfg);

        // generate_report: needs file access to /data and /output.
        SandboxConfig report_cfg;
        report_cfg.memory_limit_mb = 256;
        report_cfg.timeout_ms      = 10000;
        report_cfg.allowed_paths   = {"/data", "/output"};
        mgr.set_tool_config("generate_report", report_cfg);

        // heavy_compute: high memory, long timeout.
        SandboxConfig compute_cfg;
        compute_cfg.memory_limit_mb = 1024;
        compute_cfg.timeout_ms      = 30000;
        mgr.set_tool_config("heavy_compute", compute_cfg);

        std::cout << "\n>> Step 2: Per-tool configs set";
        print_config("encrypt",         mgr.get_config_for_tool("encrypt"));
        print_config("send_email",      mgr.get_config_for_tool("send_email"));
        print_config("generate_report", mgr.get_config_for_tool("generate_report"));
        print_config("heavy_compute",   mgr.get_config_for_tool("heavy_compute"));

        // A tool with no per-tool config falls back to global default:
        print_config("unknown_tool (fallback)", mgr.get_config_for_tool("unknown_tool"));

        // -----------------------------------------------------------------------
        // 4. Load modules and execute — demonstrating successful runs.
        //    We reload the factory to capture mock pointers for queuing results.
        // -----------------------------------------------------------------------

        // Re-set factory so we can capture pointers for result queuing.
        mgr.set_runtime_factory(
            [&](const std::string& /*path*/, const SandboxConfig& /*cfg*/)
                -> std::unique_ptr<IWasmRuntime>
            {
                auto mock = std::make_unique<MockRuntime>();
                // Capture the raw pointer before moving ownership.
                // We identify which tool by checking what hasn't been set yet.
                if (!encrypt_mock) { encrypt_mock = mock.get(); }
                else if (!email_mock) { email_mock = mock.get(); }
                else if (!report_mock) { report_mock = mock.get(); }
                else if (!compute_mock) { compute_mock = mock.get(); }
                return mock;
            });

        mgr.load_module("encrypt",         "encrypt.wasm");
        mgr.load_module("send_email",      "send_email.wasm");
        mgr.load_module("generate_report", "generate_report.wasm");
        mgr.load_module("heavy_compute",   "heavy_compute.wasm");

        std::cout << "\n" << std::string(60, '-') << "\n";
        std::cout << ">> Step 3: Successful executions\n";

        // Queue successful results.
        {
            SandboxResult ok;
            ok.success = true;
            ok.output  = R"({"encrypted":"base64data..."})";
            ok.memory_used_bytes = 4 * 1024 * 1024;  // 4 MB
            encrypt_mock->set_next_result(ok);
        }
        print_result("encrypt (success)",
                     mgr.execute_tool("encrypt", R"({"data":"secret123"})"));

        {
            SandboxResult ok;
            ok.success = true;
            ok.output  = R"({"sent":true,"recipient":"user@example.com"})";
            ok.memory_used_bytes = 2 * 1024 * 1024;  // 2 MB
            email_mock->set_next_result(ok);
        }
        print_result("send_email (success)",
                     mgr.execute_tool("send_email", R"({"to":"user@example.com"})"));

        // -----------------------------------------------------------------------
        // 5. Demonstrate violation handling — all four violation types.
        // -----------------------------------------------------------------------

        std::cout << "\n" << std::string(60, '-') << "\n";
        std::cout << ">> Step 4: Constraint violations\n";

        // 5a. TIMEOUT violation
        encrypt_mock->simulate_violation(
            SandboxViolation::TIMEOUT,
            "execution exceeded 2000 ms limit");
        print_result("encrypt (TIMEOUT violation)",
                     mgr.execute_tool("encrypt", R"({"data":"huge_payload"})"));

        // 5b. MEMORY_EXCEEDED violation
        compute_mock->simulate_violation(
            SandboxViolation::MEMORY_EXCEEDED,
            "used 1200 MB, limit 1024 MB");
        print_result("heavy_compute (MEMORY_EXCEEDED violation)",
                     mgr.execute_tool("heavy_compute", R"({"iterations":999999})"));

        // 5c. FILE_ACCESS_DENIED violation
        report_mock->simulate_violation(
            SandboxViolation::FILE_ACCESS_DENIED,
            "path not in allowed_paths: /etc/passwd");
        print_result("generate_report (FILE_ACCESS_DENIED violation)",
                     mgr.execute_tool("generate_report", R"({"path":"/etc/passwd"})"));

        // 5d. NETWORK_DENIED violation
        encrypt_mock->simulate_violation(
            SandboxViolation::NETWORK_DENIED,
            "network access is disabled in sandbox config");
        print_result("encrypt (NETWORK_DENIED violation)",
                     mgr.execute_tool("encrypt", R"({"exfil":"true"})"));

        // -----------------------------------------------------------------------
        // 6. Demonstrate module management (load/unload/has_module).
        // -----------------------------------------------------------------------

        std::cout << "\n" << std::string(60, '-') << "\n";
        std::cout << ">> Step 5: Module management\n";

        std::cout << "  has_module('encrypt'):    "
                  << (mgr.has_module("encrypt") ? "YES" : "NO") << "\n";
        std::cout << "  has_module('not_loaded'): "
                  << (mgr.has_module("not_loaded") ? "YES" : "NO") << "\n";

        mgr.unload_module("encrypt");
        std::cout << "  After unload — has_module('encrypt'): "
                  << (mgr.has_module("encrypt") ? "YES" : "NO") << "\n";

        // Attempt to execute an unloaded module:
        auto missing = mgr.execute_tool("encrypt", "{}");
        std::cout << "  Execute unloaded tool → error: "
                  << missing.error.value_or("(none)") << "\n";

        // -----------------------------------------------------------------------
        // Done
        // -----------------------------------------------------------------------

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "  Demo complete — all sandbox features demonstrated.\n";
        std::cout << std::string(60, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

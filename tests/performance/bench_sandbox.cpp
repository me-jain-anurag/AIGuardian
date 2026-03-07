// tests/performance/bench_sandbox.cpp
// Owner: Dev B
// Performance benchmarks for SandboxManager and Runtime engines

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "guardian/sandbox_manager.hpp"
#include <vector>
#include <string>

using namespace guardian;

TEST_CASE("SandboxManager Performance Benchmarks", "[benchmark][sandbox]") {
    
    // 11.1 Benchmark sandbox overhead (<50ms for simple tools)
    // 11.2 Benchmark Wasm module loading and caching
    // 11.4 Benchmark memory usage under sustained load

    // Factory using MockRuntime
    RuntimeFactory mock_factory = [](const std::string& /*path*/,
                                     const SandboxConfig& /*cfg*/) {
        auto mock = std::make_unique<MockRuntime>();
        // Return a tiny JSON output for minimum overhead simulation
        SandboxResult ok;
        ok.success = true;
        ok.output = "{}";
        mock->set_next_result(ok);
        return mock;
    };

    SECTION("Overhead and Execution") {
        SandboxManager mgr("../wasm_tools", mock_factory);
        mgr.load_module("dummy_tool", "dummy.wasm");

        BENCHMARK("Execute cached MockRuntime module (Overhead)") {
            return mgr.execute_tool("dummy_tool", R"({"test":1})", SandboxConfig::safe_defaults());
        };
    }

    SECTION("Module Loading and Caching") {
        BENCHMARK_ADVANCED("Load module (MockRuntime creation)")
        (Catch::Benchmark::Chronometer meter) {
            SandboxManager mgr("../wasm_tools", mock_factory);
            meter.measure([&] {
                mgr.load_module("dyn_tool", "dyn.wasm");
                mgr.unload_module("dyn_tool");
            });
        };
        
        BENCHMARK_ADVANCED("Check if module is loaded (has_module)")
        (Catch::Benchmark::Chronometer meter) {
            SandboxManager mgr("../wasm_tools", mock_factory);
            mgr.load_module("test_tool", "test.wasm");
            meter.measure([&] {
                return mgr.has_module("test_tool");
            });
        };
    }

    SECTION("Sustained Load (10,000 executions)") {
        SandboxManager mgr("../wasm_tools", mock_factory);
        mgr.load_module("stress_tool", "stress.wasm");
        SandboxConfig cfg = SandboxConfig::safe_defaults();

        // 11.3 Memory pooling (concept check) - reusing config/name strings
        // In real WasmEdge, memory growth is managed by the WasmEdge VM,
        // but this bench ensures our C++ wrapper layer scales nicely.
        BENCHMARK("Execute 10k times (Sustained Load)") {
            int success_count = 0;
            for (int i = 0; i < 10000; ++i) {
                // Since MockRuntime resets its queue or falls back to default success,
                // we just hammer execute_tool
                auto res = mgr.execute_tool("stress_tool", "{}", cfg);
                if (res.success) success_count++;
            }
            return success_count;
        };
    }

#ifdef HAVE_WASMEDGE
    SECTION("Real WasmEdge Module Benchmarks") {
        // Factory using real WasmEdgeRuntime
        RuntimeFactory wasm_factory = [](const std::string& path,
                                         const SandboxConfig& cfg) {
            return std::make_unique<WasmEdgeRuntime>(path, cfg);
        };
        
        SandboxManager wasm_mgr("../wasm_tools", wasm_factory);
        
        BENCHMARK_ADVANCED("Load real .wasm file (read_accounts.wasm)")
        (Catch::Benchmark::Chronometer meter) {
            meter.measure([&] {
                wasm_mgr.load_module("read_accounts", "read_accounts.wasm");
                wasm_mgr.unload_module("read_accounts");
            });
        };
        
        wasm_mgr.load_module("encrypt", "encrypt.wasm");
        BENCHMARK("Execute cached real .wasm file (encrypt.wasm overhead)") {
            return wasm_mgr.execute_tool("encrypt", R"({"data":"1234567890"})");
        };
    }
#endif
}

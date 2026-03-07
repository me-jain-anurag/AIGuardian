// src/guardian.cpp
// Owner: Dev A
// Guardian API — wires PolicyGraph + Validator + Session + Sandbox + Viz
#include "guardian/guardian.hpp"
#include "guardian/policy_graph.hpp"
#include "guardian/config.hpp"
#include "guardian/session_manager.hpp"
#include "guardian/policy_validator.hpp"
// #include "guardian/sandbox_manager.hpp"  // TODO: enable after Dev B's WasmEdge integration
#include "guardian/visualization.hpp"
#include "guardian/logger.hpp"

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <iostream>

namespace fs = std::filesystem;

namespace guardian {

// ============================================================================
// Construction / Destruction
// ============================================================================

Guardian::Guardian(const std::string& policy_file_path,
                   const std::string& wasm_tools_dir,
                   const std::string& config_file_path) {
    // 1. Load config (safe defaults if file missing)
    if (!config_file_path.empty()) {
        config_ = Config::load(config_file_path);
    }

    // 2. Load policy graph from JSON file
    fs::path policy_path(policy_file_path);
    if (!fs::exists(policy_path)) {
        throw std::runtime_error(
            "Guardian: policy file not found: " + policy_file_path);
    }

    std::ifstream file(policy_path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "Guardian: could not open policy file: " + policy_file_path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    try {
        policy_graph_ = PolicyGraph::from_json(content);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Guardian: failed to parse policy file — ") + e.what());
    }

    Logger::instance().log(LogLevel::INFO, "Guardian",
        "Loaded policy graph: " + std::to_string(policy_graph_.node_count()) +
        " nodes, " + std::to_string(policy_graph_.edge_count()) + " edges");

    // 3. Initialize SessionManager
    session_mgr_ = std::make_unique<SessionManager>();

    // 4. Initialize PolicyValidator
    validator_ = std::make_unique<PolicyValidator>(policy_graph_);
    validator_->set_cycle_threshold(config_.cycle_detection.default_threshold);

    // 5. Initialize VisualizationEngine
    viz_ = std::make_unique<VisualizationEngine>();

    // 6. Initialize SandboxManager (after Dev B merge)
    // sandbox_mgr_ = std::make_unique<SandboxManager>();
    // if (!wasm_tools_dir.empty()) {
    //     // Scan and load wasm modules from directory
    //     for (const auto& entry : fs::directory_iterator(wasm_tools_dir)) {
    //         if (entry.path().extension() == ".wasm") {
    //             sandbox_mgr_->load_module(
    //                 entry.path().stem().string(), entry.path().string());
    //         }
    //     }
    // }

    Logger::instance().log(LogLevel::INFO, "Guardian",
        "Initialization complete (sandbox pending Dev B merge)");
    initialized_ = true;
}

Guardian::~Guardian() = default;

Guardian::Guardian(Guardian&&) noexcept = default;
Guardian& Guardian::operator=(Guardian&&) noexcept = default;

// ============================================================================
// Main API
// ============================================================================

std::pair<ValidationResult, std::optional<SandboxResult>>
Guardian::execute_tool(const std::string& tool_name,
                        const std::map<std::string, std::string>& params,
                        const std::string& session_id) {
    if (!initialized_) {
        return {ValidationResult{false, "Guardian not initialized", {}, {}, {}},
                std::nullopt};
    }

    // Verify session exists
    if (!session_mgr_->has_session(session_id)) {
        return {ValidationResult{false, "Invalid session ID: " + session_id, {}, {}, {}},
                std::nullopt};
    }

    // Step 1: Get current action sequence
    auto sequence = session_mgr_->get_sequence(session_id);

    // Step 2: Validate using PolicyValidator
    ValidationResult validation = validator_->validate(tool_name, sequence);

    Logger::instance().log(
        validation.approved ? LogLevel::INFO : LogLevel::WARN,
        "Guardian",
        "validate(" + tool_name + "): " + validation.reason);

    if (!validation.approved) {
        return {validation, std::nullopt};
    }

    // Step 3: Execute in sandbox (once Dev B's SandboxManager is linked)
    std::optional<SandboxResult> sandbox_result = std::nullopt;
    // auto node = policy_graph_.get_node(tool_name);
    // if (node && sandbox_mgr_) {
    //     nlohmann::json params_json(params);
    //     sandbox_result = sandbox_mgr_->execute_tool(
    //         tool_name, params_json.dump(), node->sandbox_config);
    // }

    // Step 4: Append to session on success
    ToolCall call;
    call.tool_name = tool_name;
    call.parameters = params;
    call.timestamp = std::chrono::system_clock::now();
    call.session_id = session_id;
    session_mgr_->append_tool_call(session_id, call);

    return {validation, sandbox_result};
}

ValidationResult Guardian::validate_tool_call(const std::string& tool_name,
                                                const std::string& session_id) {
    if (!initialized_) {
        return {false, "Guardian not initialized", {}, {}, {}};
    }

    if (!session_mgr_->has_session(session_id)) {
        return {false, "Invalid session ID: " + session_id, {}, {}, {}};
    }

    auto sequence = session_mgr_->get_sequence(session_id);
    return validator_->validate(tool_name, sequence);
}

// ============================================================================
// Session Management
// ============================================================================

std::string Guardian::create_session() {
    if (!initialized_) {
        throw std::runtime_error("Guardian::create_session: not initialized");
    }
    auto id = session_mgr_->create_session();
    Logger::instance().log(LogLevel::INFO, "Guardian", "Session created: " + id);
    return id;
}

void Guardian::end_session(const std::string& session_id) {
    if (!initialized_) {
        throw std::runtime_error("Guardian::end_session: not initialized");
    }
    session_mgr_->end_session(session_id);
    Logger::instance().log(LogLevel::INFO, "Guardian", "Session ended: " + session_id);
}

// ============================================================================
// Sandbox Management
// ============================================================================

void Guardian::load_tool_module(const std::string& tool_name,
                                 const std::string& wasm_path) {
    if (!initialized_) {
        throw std::runtime_error("Guardian::load_tool_module: not initialized");
    }
    // TODO: enable after Dev B merge
    // sandbox_mgr_->load_module(tool_name, wasm_path);
    Logger::instance().log(LogLevel::INFO, "Guardian",
        "load_tool_module('" + tool_name + "', '" + wasm_path + "') — sandbox pending");
}

void Guardian::set_default_sandbox_config(const SandboxConfig& config) {
    config_.sandbox = config;
    // TODO: enable after Dev B merge
    // if (sandbox_mgr_) sandbox_mgr_->set_default_config(config);
}

// ============================================================================
// Visualization
// ============================================================================

std::string Guardian::visualize_policy(const std::string& format) const {
    VisualizationOptions opts;
    if (format == "json") {
        opts.output_format = VisualizationOptions::JSON;
    } else {
        opts.output_format = VisualizationOptions::DOT;
    }
    return viz_->render_graph(policy_graph_, opts);
}

std::string Guardian::visualize_session(const std::string& session_id,
                                          const std::string& format) const {
    if (!session_mgr_->has_session(session_id)) {
        throw std::invalid_argument(
            "Guardian::visualize_session: unknown session " + session_id);
    }
    auto sequence = session_mgr_->get_sequence(session_id);
    VisualizationOptions opts;
    if (format == "json") {
        opts.output_format = VisualizationOptions::JSON;
    }
    return viz_->render_sequence(policy_graph_, sequence, opts);
}

// ============================================================================
// Configuration
// ============================================================================

void Guardian::update_config(const Config& config) {
    config_ = config;
    if (validator_) {
        validator_->set_cycle_threshold(config.cycle_detection.default_threshold);
    }
}

Config Guardian::get_config() const {
    return config_;
}

// ============================================================================
// Accessors
// ============================================================================

const PolicyGraph& Guardian::get_policy_graph() const {
    return policy_graph_;
}

bool Guardian::is_initialized() const {
    return initialized_;
}

} // namespace guardian

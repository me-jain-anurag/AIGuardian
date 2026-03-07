// src/guardian.cpp
// Owner: Dev A
// Guardian API — wires PolicyGraph + Validator + Session + Sandbox + Viz
#include "guardian/guardian.hpp"
#include "guardian/policy_graph.hpp"
#include "guardian/config.hpp"
#include "guardian/session_manager.hpp"
// TODO: uncomment after teammate merges:
// #include "guardian/policy_validator.hpp"
// #include "guardian/sandbox_manager.hpp"
// #include "guardian/visualization.hpp"

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

    // 3. Initialize SessionManager (Dev C's implementation)
    session_mgr_ = std::make_unique<SessionManager>();

    // 4. Initialize other components (after teammates merge)
    // validator_ = std::make_unique<PolicyValidator>(policy_graph_);
    // sandbox_mgr_ = std::make_unique<SandboxManager>();
    // if (!wasm_tools_dir.empty()) {
    //     // Load wasm modules from directory
    // }
    // viz_ = std::make_unique<VisualizationEngine>();

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

    // Verify tool exists in policy graph
    if (!policy_graph_.has_node(tool_name)) {
        // Suggest valid tools
        auto all_ids = policy_graph_.get_all_node_ids();
        return {ValidationResult{false, "Unknown tool: " + tool_name, all_ids, {}, {}},
                std::nullopt};
    }

    // Step 1: Get current action sequence
    auto sequence = session_mgr_->get_sequence(session_id);

    // Step 2: Validate (once Dev C's PolicyValidator is linked)
    // ValidationResult validation = validator_->validate(tool_name, sequence);
    // For now, approve all calls (placeholder until validator is linked)
    ValidationResult validation;
    validation.approved = true;
    validation.reason = "Approved (validator not yet linked — awaiting Dev C merge)";

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

    if (!policy_graph_.has_node(tool_name)) {
        auto all_ids = policy_graph_.get_all_node_ids();
        return {false, "Unknown tool: " + tool_name, all_ids, {}, {}};
    }

    auto sequence = session_mgr_->get_sequence(session_id);

    // Validate (placeholder until Dev C merge)
    // return validator_->validate(tool_name, sequence);
    return {true, "Approved (validator not yet linked)", {}, {}, {}};
}

// ============================================================================
// Session Management
// ============================================================================

std::string Guardian::create_session() {
    if (!initialized_) {
        throw std::runtime_error("Guardian::create_session: not initialized");
    }
    return session_mgr_->create_session();
}

void Guardian::end_session(const std::string& session_id) {
    if (!initialized_) {
        throw std::runtime_error("Guardian::end_session: not initialized");
    }
    session_mgr_->end_session(session_id);
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
    std::cerr << "[INFO] load_tool_module('" << tool_name
              << "', '" << wasm_path << "') — sandbox not yet linked" << std::endl;
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
    if (format == "dot" || format == "DOT") {
        return policy_graph_.to_dot();
    }
    if (format == "json") {
        return policy_graph_.to_json();
    }
    // TODO: enable after Dev D merge
    // if (viz_) {
    //     VisualizationOptions opts;
    //     opts.output_format = (format == "ascii") ?
    //         VisualizationOptions::ASCII : VisualizationOptions::DOT;
    //     return viz_->render_graph(policy_graph_, opts);
    // }
    return policy_graph_.to_dot(); // fallback
}

std::string Guardian::visualize_session(const std::string& session_id,
                                          const std::string& /*format*/) const {
    if (!session_mgr_->has_session(session_id)) {
        throw std::invalid_argument(
            "Guardian::visualize_session: unknown session " + session_id);
    }
    // TODO: enable after Dev D merge
    // auto sequence = session_mgr_->get_sequence(session_id);
    // return viz_->render_sequence(policy_graph_, sequence, {});
    return "Session visualization pending Dev D merge";
}

// ============================================================================
// Configuration
// ============================================================================

void Guardian::update_config(const Config& config) {
    config_ = config;
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

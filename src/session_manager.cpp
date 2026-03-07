// src/session_manager.cpp
// Owner: Dev C
// Thread-safe session management implementation
#include "guardian/session_manager.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace guardian {

// ============================================================================
// Construction
// ============================================================================

SessionManager::SessionManager(uint32_t timeout_seconds, uint32_t max_sessions)
    : timeout_seconds_(timeout_seconds), max_sessions_(max_sessions) {}

// ============================================================================
// UUID v4 generation (portable — uses <random>, no OS-specific APIs)
// ============================================================================

std::string SessionManager::generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;

    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << dist(gen) << "-"
       << std::setw(4) << (dist(gen) & 0xFFFF) << "-"
       << std::setw(4) << ((dist(gen) & 0x0FFF) | 0x4000) << "-"   // version 4
       << std::setw(4) << ((dist(gen) & 0x3FFF) | 0x8000) << "-"   // variant 1
       << std::setw(12) << (static_cast<uint64_t>(dist(gen)) << 16 | (dist(gen) & 0xFFFF));
    return ss.str();
}

// ============================================================================
// Session lifecycle
// ============================================================================

std::string SessionManager::create_session() {
    std::unique_lock lock(mutex_);

    // Clean up expired sessions before checking limits
    if (timeout_seconds_ > 0) {
        cleanup_expired_sessions();
    }

    // Enforce max concurrent sessions
    if (max_sessions_ > 0 && sessions_.size() >= max_sessions_) {
        throw std::runtime_error(
            "Maximum concurrent sessions limit reached (" +
            std::to_string(max_sessions_) + ")");
    }

    Session session;
    session.session_id = generate_uuid();
    session.created_at = std::chrono::system_clock::now();
    session.last_activity = session.created_at;

    std::string id = session.session_id;
    sessions_.emplace(id, std::move(session));
    return id;
}

void SessionManager::end_session(const std::string& session_id) {
    std::unique_lock lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found: " + session_id);
    }
    sessions_.erase(it);
}

bool SessionManager::has_session(const std::string& session_id) const {
    std::shared_lock lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }

    // If timeout is enabled, an expired session is treated as non-existent
    if (timeout_seconds_ > 0 && is_expired(it->second)) {
        return false;
    }
    return true;
}

// ============================================================================
// Action tracking
// ============================================================================

void SessionManager::append_tool_call(const std::string& session_id,
                                       const ToolCall& call) {
    std::unique_lock lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found: " + session_id);
    }

    if (timeout_seconds_ > 0 && is_expired(it->second)) {
        sessions_.erase(it);
        throw std::runtime_error("Session expired: " + session_id);
    }

    it->second.action_sequence.push_back(call);
    it->second.last_activity = std::chrono::system_clock::now();
}

std::vector<ToolCall> SessionManager::get_sequence(
    const std::string& session_id) const {
    std::shared_lock lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found: " + session_id);
    }

    if (timeout_seconds_ > 0 && is_expired(it->second)) {
        throw std::runtime_error("Session expired: " + session_id);
    }

    return it->second.action_sequence;
}

// ============================================================================
// Persistence — write session audit log as JSON
// ============================================================================

void SessionManager::persist_session(const std::string& session_id,
                                      const std::string& output_path) const {
    std::shared_lock lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found: " + session_id);
    }

    const Session& session = it->second;

    // Build JSON
    nlohmann::json j;
    j["session_id"] = session.session_id;

    // Timestamps as ISO 8601
    auto to_iso = [](const std::chrono::system_clock::time_point& tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%FT%TZ");
        return ss.str();
    };
    j["created_at"] = to_iso(session.created_at);
    j["last_activity"] = to_iso(session.last_activity);

    // Metadata
    j["metadata"] = session.metadata;

    // Action sequence
    nlohmann::json actions = nlohmann::json::array();
    for (const auto& call : session.action_sequence) {
        nlohmann::json c;
        c["tool_name"] = call.tool_name;
        c["parameters"] = call.parameters;
        c["timestamp"] = to_iso(call.timestamp);
        c["session_id"] = call.session_id;
        actions.push_back(c);
    }
    j["action_sequence"] = actions;

    // Write to file using std::filesystem::path (cross-platform)
    fs::path file_path(output_path);

    // Create parent directories if needed
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path());
    }

    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " +
                                  output_path);
    }
    ofs << j.dump(2);
}

// ============================================================================
// Queries
// ============================================================================

std::vector<Session> SessionManager::get_all_sessions() const {
    std::shared_lock lock(mutex_);

    std::vector<Session> result;
    result.reserve(sessions_.size());
    for (const auto& [id, session] : sessions_) {
        if (timeout_seconds_ == 0 || !is_expired(session)) {
            result.push_back(session);
        }
    }
    return result;
}

size_t SessionManager::active_session_count() const {
    std::shared_lock lock(mutex_);

    if (timeout_seconds_ == 0) {
        return sessions_.size();
    }

    size_t count = 0;
    for (const auto& [id, session] : sessions_) {
        if (!is_expired(session)) {
            ++count;
        }
    }
    return count;
}

// ============================================================================
// Internal helpers
// ============================================================================

void SessionManager::cleanup_expired_sessions() {
    // Caller must hold unique lock
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (is_expired(it->second)) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

bool SessionManager::is_expired(const Session& session) const {
    if (timeout_seconds_ == 0) return false;

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - session.last_activity)
                       .count();
    return elapsed >= static_cast<int64_t>(timeout_seconds_);
}

} // namespace guardian

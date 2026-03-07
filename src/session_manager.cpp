// src/session_manager.cpp
// Owner: Dev C (stub implementation by Dev A for integration)
// Session lifecycle management
#include "guardian/session_manager.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>

namespace guardian {

// ============================================================================
// UUID-like session ID generator
// ============================================================================

static std::string generate_session_id() {
    static std::mt19937 rng(
        static_cast<unsigned>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << dist(rng) << "-";
    oss << std::setw(4) << (dist(rng) & 0xFFFF) << "-";
    oss << std::setw(4) << (dist(rng) & 0xFFFF) << "-";
    oss << std::setw(4) << (dist(rng) & 0xFFFF) << "-";
    oss << std::setw(8) << dist(rng) << std::setw(4) << (dist(rng) & 0xFFFF);
    return oss.str();
}

// ============================================================================
// SessionManager Implementation
// ============================================================================

std::string SessionManager::create_session() {
    std::string id = generate_session_id();
    Session session;
    session.session_id = id;
    session.start_time = std::chrono::system_clock::now();
    session.active = true;
    sessions_[id] = session;
    return id;
}

void SessionManager::end_session(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::invalid_argument(
            "SessionManager::end_session: unknown session " + session_id);
    }
    it->second.active = false;
}

bool SessionManager::has_session(const std::string& session_id) const {
    auto it = sessions_.find(session_id);
    return it != sessions_.end() && it->second.active;
}

std::vector<ToolCall> SessionManager::get_sequence(
    const std::string& session_id) const {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return {};
    }
    return it->second.tool_calls;
}

void SessionManager::append_tool_call(const std::string& session_id,
                                        const ToolCall& call) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::invalid_argument(
            "SessionManager::append_tool_call: unknown session " + session_id);
    }
    if (!it->second.active) {
        throw std::invalid_argument(
            "SessionManager::append_tool_call: session " + session_id + " is ended");
    }
    it->second.tool_calls.push_back(call);
}

size_t SessionManager::active_session_count() const {
    size_t count = 0;
    for (const auto& [id, session] : sessions_) {
        if (session.active) ++count;
    }
    return count;
}

std::vector<std::string> SessionManager::get_active_session_ids() const {
    std::vector<std::string> ids;
    for (const auto& [id, session] : sessions_) {
        if (session.active) {
            ids.push_back(id);
        }
    }
    return ids;
}

} // namespace guardian

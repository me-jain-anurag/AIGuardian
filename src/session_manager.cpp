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

// Provide the map missing from the CPP file
std::map<std::string, Session> sessions_;

std::string SessionManager::create_session() {
    std::string id = generate_session_id();
    Session session;
    session.session_id = id;
    session.created_at = std::chrono::system_clock::now();
    session.last_activity = session.created_at;
    sessions_[id] = session;
    return id;
}

void SessionManager::end_session(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::invalid_argument(
            "SessionManager::end_session: unknown session " + session_id);
    }
    // No 'active' field natively, just remove from map for stub
    sessions_.erase(it);
}

bool SessionManager::has_session(const std::string& session_id) const {
    auto it = sessions_.find(session_id);
    return it != sessions_.end();
}

std::vector<ToolCall> SessionManager::get_sequence(
    const std::string& session_id) const {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return {};
    }
    return it->second.action_sequence;
}

void SessionManager::append_tool_call(const std::string& session_id,
                                        const ToolCall& call) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::invalid_argument(
            "SessionManager::append_tool_call: unknown session " + session_id);
    }
    it->second.action_sequence.push_back(call);
    it->second.last_activity = std::chrono::system_clock::now();
}
// Helper removed

std::vector<std::string> SessionManager::get_all_sessions() const {
    std::vector<std::string> ids;
    for (const auto& [id, session] : sessions_) {
        ids.push_back(id);
    }
    return ids;
}

} // namespace guardian

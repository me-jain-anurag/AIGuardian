// include/guardian/session_manager.hpp
// Owner: Dev C
// Thread-safe session management with persistence
#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <optional>

namespace guardian {

struct Session {
    std::string session_id;
    std::vector<ToolCall> action_sequence;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activity;
    std::map<std::string, std::string> metadata;
};

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    // Session lifecycle
    std::string create_session();
    void end_session(const std::string& session_id);
    bool has_session(const std::string& session_id) const;

    // Action tracking
    void append_tool_call(const std::string& session_id, const ToolCall& call);
    std::vector<ToolCall> get_sequence(const std::string& session_id) const;

    // Persistence
    void persist_session(const std::string& session_id,
                          const std::string& output_path) const;

    // Queries
    std::vector<std::string> get_all_sessions() const;
};

} // namespace guardian

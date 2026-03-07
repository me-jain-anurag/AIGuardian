// include/guardian/session_manager.hpp
// Owner: Dev C
// Thread-safe session management with persistence, timeout, and concurrency limits
#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <cstdint>

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
    /// Construct with optional timeout (seconds) and max concurrent sessions.
    /// timeout_seconds = 0 means no timeout.
    /// max_sessions = 0 means unlimited.
    explicit SessionManager(uint32_t timeout_seconds = 0,
                            uint32_t max_sessions = 0);
    ~SessionManager() = default;

    // Non-copyable, non-movable (owns mutex)
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

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
    std::vector<Session> get_all_sessions() const;
    size_t active_session_count() const;

private:
    /// Generate a portable UUID v4 using <random>
    static std::string generate_uuid();

    /// Remove sessions that have exceeded the timeout.
    /// Caller must hold a unique lock.
    void cleanup_expired_sessions();

    /// Check if a single session is expired. Not locked — caller must hold lock.
    bool is_expired(const Session& session) const;

    std::unordered_map<std::string, Session> sessions_;
    mutable std::shared_mutex mutex_;

    uint32_t timeout_seconds_;
    uint32_t max_sessions_;
};

} // namespace guardian

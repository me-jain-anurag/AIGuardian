// src/session_manager.cpp
// Owner: Dev C
// Session lifecycle management
#include "guardian/session_manager.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>

namespace guardian {

static std::string generate_session_id() {
  static std::mt19937 rng(static_cast<unsigned>(
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

SessionManager::SessionManager(uint32_t timeout_seconds, uint32_t max_sessions,
                               uint32_t initial_reserve)
    : timeout_seconds_(timeout_seconds), max_sessions_(max_sessions),
      initial_reserve_(initial_reserve) {}

std::string SessionManager::create_session() {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  cleanup_expired_sessions();
  if (max_sessions_ > 0 && sessions_.size() >= max_sessions_) {
    throw std::runtime_error("Max sessions reached");
  }
  std::string id = generate_session_id();
  Session session;
  session.session_id = id;
  session.action_sequence.reserve(initial_reserve_);
  session.created_at = std::chrono::system_clock::now();
  session.last_activity = session.created_at;
  sessions_[id] = session;
  return id;
}

void SessionManager::end_session(const std::string &session_id) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    throw std::runtime_error("SessionManager::end_session: unknown session");
  }
  sessions_.erase(it);
}

bool SessionManager::has_session(const std::string &session_id) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end())
    return false;
  if (is_expired(it->second))
    return false;
  return true;
}

std::vector<ToolCall>
SessionManager::get_sequence(const std::string &session_id) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end() || is_expired(it->second)) {
    return {};
  }
  return it->second.action_sequence;
}

void SessionManager::append_tool_call(const std::string &session_id,
                                      const ToolCall &call) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end() || is_expired(it->second)) {
    throw std::runtime_error(
        "SessionManager::append_tool_call: unknown or expired session");
  }
  it->second.action_sequence.push_back(call);
  it->second.last_activity = std::chrono::system_clock::now();
}

void SessionManager::persist_session(const std::string &session_id,
                                     const std::string &output_path) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    throw std::runtime_error("Unknown session");
  }
  std::ofstream ofs(output_path);
  ofs << "{\n  \"session_id\": \"" << session_id
      << "\",\n  \"action_sequence\": [\n";
  for (size_t i = 0; i < it->second.action_sequence.size(); ++i) {
    ofs << "    {\"tool_name\": \"" << it->second.action_sequence[i].tool_name
        << "\"}";
    if (i + 1 < it->second.action_sequence.size())
      ofs << ",\n";
  }
  ofs << "\n  ]\n}";
}

std::vector<std::string> SessionManager::get_all_sessions() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  std::vector<std::string> ids;
  for (const auto &[id, session] : sessions_) {
    if (!is_expired(session))
      ids.push_back(id);
  }
  return ids;
}

size_t SessionManager::active_session_count() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  size_t count = 0;
  for (const auto &[id, session] : sessions_) {
    if (!is_expired(session))
      count++;
  }
  return count;
}

void SessionManager::cleanup_expired_sessions() {
  if (timeout_seconds_ == 0)
    return;
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (is_expired(it->second)) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

bool SessionManager::is_expired(const Session &session) const {
  if (timeout_seconds_ == 0)
    return false;
  auto now = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                     now - session.last_activity)
                     .count();
  return elapsed >= timeout_seconds_;
}

} // namespace guardian

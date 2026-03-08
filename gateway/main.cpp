// gateway/main.cpp
// AIGuardian HTTP Gateway
//
// Endpoints:
//   POST   /session              → { "session_id": "..." }
//   POST   /intercept            → { "allowed": bool, "reason": "...", "alternatives": [...] }
//   GET    /audit/{session_id}   → JSON array of log entries
//   GET    /policy               → policy graph as DOT
//   GET    /policy/json          → policy graph as JSON { nodes, edges }  (consumed by React UI)
//   GET    /events               → Server-Sent Events stream of real-time intercept decisions
//   DELETE /session/{id}         → operator kill-switch — revoke an active session
//   POST   /demo/run/{scenario}  → launch demo scenario subprocess (1, 2, or 3)
//   GET    /health               → { "status": "ok" }
//
// SSE design note:
//   Each GET /events client gets an independent Subscriber (its own deque + condvar).
//   Every POST /intercept call broadcasts a copy of the event JSON to all active subscribers.
//   A 20-second heartbeat (comment line) keeps the TCP connection alive through proxies.
//
// Demo subprocess note:
//   POST /demo/run/{scenario} uses std::system() to fire-and-forget the Python agent as a
//   background process. The scenario parameter is validated to be exactly "1", "2", or "3"
//   before touching the shell, and the GUARDIAN_PYTHON env-var is checked for unsafe shell
//   metacharacters, preventing command injection.

// Require Windows 10+ for cpp-httplib (needs _WIN32_WINNT >= 0x0A00)
#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00

// Guardian headers MUST come before httplib.h — Windows headers define ERROR/DEBUG/WARN
// as numeric macros that conflict with the LogLevel enum members in guardian/logger.hpp.
#include "guardian/guardian.hpp"
#include "guardian/logger.hpp"
#include <nlohmann/json.hpp>

// Undef the Windows macros that conflict with guardian enum identifiers
#ifdef ERROR
#  undef ERROR
#endif
#ifdef DEBUG
#  undef DEBUG
#endif
#ifdef WARN
#  undef WARN
#endif

// Do NOT define CPPHTTPLIB_OPENSSL_SUPPORT — defining it to 0 still
// triggers #ifdef which pulls in OpenSSL symbols we don't link against.
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#  undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <map>
#include <deque>
#include <list>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <cstdlib>
#include <ctime>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Utility: ISO-8601 UTC timestamp
// ---------------------------------------------------------------------------
static std::string now_iso() {
    auto now  = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

// ---------------------------------------------------------------------------
// SSE Event Broadcaster
//
// Each connected /events client registers a Subscriber.  POST /intercept
// calls broadcast(), which copies the event JSON into every subscriber's
// deque and wakes its condition variable.  The SSE content-provider then
// drains the deque and writes "data: <json>\n\n" chunks to the client.
// ---------------------------------------------------------------------------
struct Subscriber {
    std::deque<std::string> events;
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    closed = false;

    Subscriber() = default;
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
};

struct EventBroadcaster {
    std::list<std::shared_ptr<Subscriber>> subs;
    std::mutex                             list_mtx;

    std::shared_ptr<Subscriber> subscribe() {
        auto s = std::make_shared<Subscriber>();
        std::lock_guard<std::mutex> lk(list_mtx);
        subs.push_back(s);
        return s;
    }

    // Mark a subscriber closed (wakes its content provider so it can return
    // false) and remove it from the broadcast list.
    void unsubscribe(std::shared_ptr<Subscriber> s) {
        {
            std::lock_guard<std::mutex> slk(s->mtx);
            s->closed = true;
        }
        s->cv.notify_all();
        std::lock_guard<std::mutex> lk(list_mtx);
        subs.remove(s);
    }

    void broadcast(const json& event) {
        std::string payload = event.dump();
        std::lock_guard<std::mutex> lk(list_mtx);
        for (auto& s : subs) {
            std::lock_guard<std::mutex> slk(s->mtx);
            s->events.push_back(payload);
            s->cv.notify_one();
        }
    }
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::string policy_file = "policies/demo.json";
    int port = 8080;

    if (argc >= 2) policy_file = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);

    // Python executable used by POST /demo/run/:scenario.
    // Override with GUARDIAN_PYTHON env var (e.g. path to a venv interpreter).
#ifdef _WIN32
    const char* default_python = "demo\\venv\\Scripts\\python.exe";
#else
    const char* default_python = "demo/venv/bin/python";
#endif
    const char* env_python = std::getenv("GUARDIAN_PYTHON");
    std::string python_exe = env_python ? env_python : default_python;

    // -------------------------------------------------------------------------
    // Load Guardian
    // -------------------------------------------------------------------------
    guardian::Guardian g(policy_file);
    std::cout << "[Guardian Gateway] Policy loaded: " << policy_file << "\n";
    std::cout << "[Guardian Gateway] Listening on http://0.0.0.0:" << port << "\n";

    // -------------------------------------------------------------------------
    // Shared state (all lambdas below capture by reference; safe because
    // svr.listen() blocks until the server stops, keeping main() alive).
    // -------------------------------------------------------------------------
    EventBroadcaster broadcaster;

    // Tracks the last tool successfully executed per session so that intercept
    // events can report the (from → to) edge being attempted.
    std::map<std::string, std::string> session_last_tool;
    std::mutex                          session_tool_mtx;

    // -------------------------------------------------------------------------
    // HTTP server
    // -------------------------------------------------------------------------
    httplib::Server svr;

    // CORS — allow the React dev server (localhost:5173) and any other origin.
    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "GET, POST, DELETE, PUT, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });
    // Respond to all OPTIONS preflight requests with 204 No Content.
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // ── GET /health ──────────────────────────────────────────────────────────
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ── POST /session ─────────────────────────────────────────────────────────
    svr.Post("/session", [&g](const httplib::Request&, httplib::Response& res) {
        try {
            std::string sid = g.create_session();
            res.set_content(json{{"session_id", sid}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── POST /intercept ───────────────────────────────────────────────────────
    // Body: { "session_id": "...", "tool": "...", "params": { ... } }
    // After resolving the decision, broadcasts an SSE event to all /events clients.
    svr.Post("/intercept", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            std::string session_id = body.value("session_id", "");
            std::string tool_name  = body.value("tool", "");

            if (session_id.empty() || tool_name.empty()) {
                res.status = 400;
                res.set_content(
                    json{{"error", "session_id and tool are required"}}.dump(),
                    "application/json");
                return;
            }

            std::map<std::string, std::string> params;
            if (body.contains("params") && body["params"].is_object()) {
                for (auto& [k, v] : body["params"].items()) {
                    params[k] = v.is_string() ? v.get<std::string>() : v.dump();
                }
            }

            // Capture from_tool BEFORE the call so we know which edge was attempted.
            std::string from_tool;
            {
                std::lock_guard<std::mutex> lk(session_tool_mtx);
                auto it = session_last_tool.find(session_id);
                if (it != session_last_tool.end()) from_tool = it->second;
            }

            auto [validation, _sandbox] = g.execute_tool(tool_name, params, session_id);

            // Update the tracker only on success (the session now "is at" this tool).
            if (validation.approved) {
                std::lock_guard<std::mutex> lk(session_tool_mtx);
                session_last_tool[session_id] = tool_name;
            }

            json resp = {
                {"allowed",      validation.approved},
                {"reason",       validation.reason},
                {"alternatives", validation.suggested_alternatives}
            };
            if (validation.detected_cycle) {
                resp["cycle"] = {
                    {"tools",       validation.detected_cycle->cycle_tools},
                    {"start_index", validation.detected_cycle->cycle_start_index},
                    {"length",      validation.detected_cycle->cycle_length}
                };
            }
            if (validation.detected_exfiltration) {
                resp["exfiltration"] = {
                    {"source",      validation.detected_exfiltration->source_node},
                    {"destination", validation.detected_exfiltration->destination_node},
                    {"path",        validation.detected_exfiltration->path}
                };
            }
            res.set_content(resp.dump(), "application/json");

            // Broadcast to all connected SSE clients.
            json event = {
                {"type",         "intercept"},
                {"timestamp",    now_iso()},
                {"session_id",   session_id},
                {"from",         from_tool},
                {"to",           tool_name},
                {"allowed",      validation.approved},
                {"reason",       validation.reason},
                {"alternatives", validation.suggested_alternatives}
            };
            broadcaster.broadcast(event);

        } catch (const json::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", std::string("JSON parse error: ") + e.what()}}.dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /audit/{session_id} ───────────────────────────────────────────────
    svr.Get(R"(/audit/(.+))", [](const httplib::Request&, httplib::Response& res) {
        std::string logs = guardian::Logger::instance().export_logs();
        res.set_content(logs, "application/json");
    });

    // ── GET /policy — DOT format ─────────────────────────────────────────────
    svr.Get("/policy", [&g](const httplib::Request&, httplib::Response& res) {
        std::string dot = g.visualize_policy("dot");
        res.set_content(dot, "text/plain");
    });

    // ── GET /policy/json — JSON format for react-flow rendering ──────────────
    // Returns the raw policy file content (nodes + edges arrays).
    // The React frontend uses this to build the initial graph layout.
    svr.Get("/policy/json", [&policy_file](const httplib::Request&, httplib::Response& res) {
        try {
            std::ifstream f(policy_file);
            if (!f.is_open()) {
                res.status = 500;
                res.set_content(json{{"error", "Cannot open policy file"}}.dump(),
                                "application/json");
                return;
            }
            json policy = json::parse(f);
            res.set_content(policy.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /events — Server-Sent Events ─────────────────────────────────────
    // The React dashboard connects here on load.  Each intercept decision is
    // pushed as:   data: {"type":"intercept","from":"...","to":"...","allowed":true,...}\n\n
    // A heartbeat comment is sent every 20 s to keep the connection alive.
    svr.Get("/events", [&broadcaster](const httplib::Request&, httplib::Response& res) {
        auto sub = broadcaster.subscribe();

        res.set_header("Cache-Control",     "no-cache");
        res.set_header("Connection",        "keep-alive");
        res.set_header("X-Accel-Buffering", "no"); // prevent nginx from buffering SSE

        res.set_chunked_content_provider(
            "text/event-stream",
            [sub](size_t /*offset*/, httplib::DataSink& sink) -> bool {
                std::deque<std::string> to_send;
                bool closed = false;

                {
                    std::unique_lock<std::mutex> lk(sub->mtx);
                    sub->cv.wait_for(lk, std::chrono::seconds(20), [&sub] {
                        return !sub->events.empty() || sub->closed;
                    });
                    closed = sub->closed;
                    to_send.swap(sub->events);
                }

                if (closed) return false;

                if (to_send.empty()) {
                    // Heartbeat — SSE comments keep proxies from closing idle connections.
                    const char hb[] = ": heartbeat\n\n";
                    return sink.write(hb, sizeof(hb) - 1);
                }

                for (const auto& payload : to_send) {
                    std::string msg = "data: " + payload + "\n\n";
                    if (!sink.write(msg.c_str(), msg.size())) return false;
                }
                return true;
            },
            [sub, &broadcaster](bool /*success*/) {
                // Client disconnected — remove from broadcaster list.
                broadcaster.unsubscribe(sub);
            }
        );
    });

    // ── DELETE /session/{id} — operator kill-switch ───────────────────────────
    // Ends the session in the C++ engine and broadcasts a session_killed event
    // so the React dashboard can update its UI immediately.
    svr.Delete(R"(/session/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string sid = req.matches[1];
        try {
            g.end_session(sid);
            {
                std::lock_guard<std::mutex> lk(session_tool_mtx);
                session_last_tool.erase(sid);
            }
            broadcaster.broadcast(json{
                {"type",       "session_killed"},
                {"timestamp",  now_iso()},
                {"session_id", sid}
            });
            res.set_content(json{{"killed", sid}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── POST /demo/run/{scenario} ─────────────────────────────────────────────
    // Launches demo/demo.py --scenario N as a fire-and-forget background process.
    // The Python agent calls /intercept, which feeds the SSE stream, which the
    // React dashboard animates in real time.
    //
    // Security: the scenario value is matched by the regex \d+ in the route and
    // additionally validated to be exactly "1", "2", or "3" before shell use.
    // The python_exe path is checked for common shell metacharacters.
    svr.Post(R"(/demo/run/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string scenario = req.matches[1];

        if (scenario != "1" && scenario != "2" && scenario != "3") {
            res.status = 400;
            res.set_content(
                json{{"error", "scenario must be 1, 2, or 3"}}.dump(),
                "application/json");
            return;
        }

        // Reject python_exe values that contain shell metacharacters.
        static const std::string bad_chars = "&|;`$<>(){}!";
        for (char c : python_exe) {
            if (bad_chars.find(c) != std::string::npos) {
                res.status = 500;
                res.set_content(
                    json{{"error", "GUARDIAN_PYTHON contains unsafe characters"}}.dump(),
                    "application/json");
                return;
            }
        }

        // Build a background command.  scenario is validated; python_exe is checked above.
        // The demo script is always at demo/demo.py relative to CWD.
#ifdef _WIN32
        std::string cmd = "start /b \"\" \"" + python_exe + "\""
                          " demo\\demo.py --scenario " + scenario
                          + " > nul 2>&1";
#else
        std::string cmd = "\"" + python_exe + "\""
                          " demo/demo.py --scenario " + scenario
                          + " > /dev/null 2>&1 &";
#endif
        std::system(cmd.c_str()); // fire-and-forget; events arrive via SSE

        res.set_content(
            json{{"started", true}, {"scenario", std::stoi(scenario)}}.dump(),
            "application/json");
    });

    svr.listen("0.0.0.0", port);
    return 0;
}

// gateway/main.cpp
// AIGuardian HTTP Gateway — MVP
// Endpoints:
//   POST /session                  → { "session_id": "..." }
//   POST /intercept                → { "allowed": bool, "reason": "...", "alternatives": [...] }
//   GET  /audit/{session_id}       → JSON array of log entries
//   GET  /policy                   → policy graph as DOT
//   GET  /health                   → { "status": "ok" }

// Require Windows 10+ for cpp-httplib (needs _WIN32_WINNT >= 0x0A00)
#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00

// Guardian headers MUST come before httplib.h — Windows headers define ERROR as
// a numeric macro which conflicts with the LogLevel::ERROR enum member.
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
// triggers #ifdef, which pulls in OpenSSL symbols we don't have.
// Just leave it undefined; httplib plain HTTP is all we need.
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#  undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"

#include <iostream>
#include <string>
#include <stdexcept>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    std::string policy_file = "policies/demo.json";
    int port = 8080;

    if (argc >= 2) policy_file = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);

    // -------------------------------------------------------------------------
    // Load Guardian
    // -------------------------------------------------------------------------
    guardian::Guardian g(policy_file);
    std::cout << "[Guardian Gateway] Policy loaded: " << policy_file << "\n";
    std::cout << "[Guardian Gateway] Listening on http://0.0.0.0:" << port << "\n";

    // -------------------------------------------------------------------------
    // HTTP server
    // -------------------------------------------------------------------------
    httplib::Server svr;

    // Health check
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // POST /session — create a new session
    svr.Post("/session", [&g](const httplib::Request&, httplib::Response& res) {
        try {
            std::string sid = g.create_session();
            json resp = {{"session_id", sid}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // POST /intercept — validate a tool call
    // Body: { "session_id": "...", "tool": "...", "params": { ... } }
    svr.Post("/intercept", [&g](const httplib::Request& req, httplib::Response& res) {
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

            auto [validation, _sandbox] = g.execute_tool(tool_name, params, session_id);

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

        } catch (const json::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", std::string("JSON parse error: ") + e.what()}}.dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // GET /audit/{session_id} — return full log for this session
    svr.Get(R"(/audit/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        // Logger is a global singleton — export everything and return it
        // (session filtering can be added later; for a demo single session is fine)
        (void)req;
        std::string logs = guardian::Logger::instance().export_logs();
        res.set_content(logs, "application/json");
    });

    // GET /policy — policy graph as DOT
    svr.Get("/policy", [&g](const httplib::Request&, httplib::Response& res) {
        std::string dot = g.visualize_policy("dot");
        res.set_content(dot, "text/plain");
    });

    // DELETE /session/{id} — end a session
    svr.Delete(R"(/session/(.+))", [&g](const httplib::Request& req, httplib::Response& res) {
        std::string sid = req.matches[1];
        try {
            g.end_session(sid);
            res.set_content(json{{"ended", sid}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.listen("0.0.0.0", port);
    return 0;
}

# AIGuardian — Feature Roadmap

This document records the roadmap of production-grade features planned after the MVP.
Each entry includes a description, the exact technical risk, and a concrete mitigation strategy.

---

## Completed (MVP)

| Feature | Status |
|---|---|
| PolicyGraph (C++ whitelist enforcement) | ✅ Done |
| PolicyValidator (cycle + exfiltration detection) | ✅ Done |
| SessionManager (per-agent session isolation) | ✅ Done |
| SandboxManager (Wasm tool execution) | ✅ Done |
| HTTP Gateway (cpp-httplib wrapping C++ core) | ✅ Done |
| LangChain + Groq demo (3 verified scenarios) | ✅ Done |

---

## Feature 1 — Python / Container Sandbox (Firecracker or Docker)

### Problem
The current SandboxManager only isolates compiled `.wasm` tools. In practice, 99% of AI agent tools are Python functions — LangChain tools, custom scripts, subprocess calls. These run unsandboxed in the host process.

### Feature
Add a secondary sandbox runner that executes arbitrary Python scripts inside **ephemeral Docker containers** (primary) or **Firecracker microVMs** (stretch). The flow:

1. Tool registered without a `.wasm` path → Guardian detects it is a "native" tool.
2. On each intercepted call, `SandboxManager` spins up a Docker container with a minimal Python image.
3. JSON arguments are passed via `stdin`; the tool script runs; `stdout` is collected as the result.
4. The container is destroyed (`docker rm -f`) in the same call before the response is returned.
5. If the container takes longer than the configured timeout (`sandbox_timeout_ms` in policy JSON), it is killed and the call is treated as BLOCKED.

### Technical Risk
**Latency**: Docker container cold-start is 200–800 ms, which adds overhead to every tool call.

**Mitigation**: Container warm pools. Pre-spawn N containers at gateway startup (configurable), keep them alive but paused, and resume-exec into them. This reduces per-call overhead to ~20 ms (equivalent to a docker exec on a running container). The pool size is tunable (`sandbox_pool_size` in config).

**Security Risk**: If the Docker socket is exposed, a compromised tool could escape the sandbox by spawning sibling containers on the host.

**Mitigation**: Run the gateway inside a rootless Docker context or constrain socket access with a socket proxy (e.g., `tecnativa/docker-socket-proxy`) that allows only `POST /containers/*` and `DELETE /containers/*`. Enable `--security-opt=no-new-privileges` and drop all Linux capabilities except `SETUID` for the container runner.

**Firecracker path**: Firecracker microVMs achieve 125 ms cold-start with a 5 MB kernel snapshot. Requires Linux KVM. This is the stretch goal for deployments where Docker is not acceptable (e.g., multi-tenant cloud).

### Implementation Notes
- New class `ContainerSandbox` in `src/container_sandbox.cpp` implementing the `ISandbox` interface used by `SandboxManager`.
- Gateway configuration key: `"sandbox_backend": "docker" | "firecracker" | "wasm"`.
- Requires `libcurl` or `cpp-httplib` pointed at the Docker Engine API (`unix:///var/run/docker.sock`).

---

## Feature 2 — Parameter-Aware DLP (Data Loss Prevention)

### Problem
The policy graph is binary. `read_db → send_email` is either always allowed or always blocked. This prevents legitimate use cases: an agent _should_ be able to email a user their own invoice, but should _not_ be able to email a database dump of all users.

### Feature
Integrate a **payload scanner** into the `ToolInterceptor` that inspects the parameters being passed to a tool _before_ permitting the transition. The decision becomes:

```
ALLOW transition AND ALLOW payload → execute
ALLOW transition AND DENY payload → BLOCKED (DLP violation)
DENY transition → BLOCKED (policy violation)
```

**DLP rules** are declared per-edge in `policy.json`:

```json
{
  "from": "read_db",
  "to": "send_email",
  "dlp_rules": [
    { "type": "regex", "pattern": "\\b\\d{3}-\\d{2}-\\d{4}\\b", "label": "SSN" },
    { "type": "regex", "pattern": "sk-[A-Za-z0-9]{32,}", "label": "API_KEY" },
    { "type": "presidio", "entities": ["EMAIL_ADDRESS", "CREDIT_CARD", "PERSON"] }
  ]
}
```

When a DLP rule matches, the intercept response includes:
```json
{
  "allowed": false,
  "reason": "DLP violation: SSN detected in parameter 'body'",
  "matched_rule": "SSN",
  "field": "body"
}
```

### Technical Risk
**False positives**: Regex patterns for SSNs and API keys will occasionally match innocent strings (e.g., version numbers like `1.2.3` matching a partial numeric pattern).

**Mitigation**: DLP rules support a `confidence_threshold` (0.0–1.0). Regex matches are always confidence 1.0. Microsoft Presidio (invoked via subprocess or REST) returns confidence scores per entity — only trigger if confidence ≥ threshold. Default threshold is 0.85.

**Performance Risk**: Presidio is a Python process. Spawning it per-call adds ~50 ms.

**Mitigation**: Run Presidio as a persistent sidecar service (Docker Compose) and call it over a local Unix socket. The C++ gateway calls it via HTTP keep-alive. Presidio is opt-in; pure-regex DLP adds zero latency.

**Privacy Risk**: Payload content is logged for audit purposes. If the payload itself contains PII, the audit log becomes a data store of sensitive data.

**Mitigation**: The audit log records the _type_ of matched entity (e.g., `"matched": "SSN"`) but not the matched value. The raw payload is never stored in the audit log.

### Implementation Notes
- New class `DLPScanner` in `src/dlp_scanner.cpp`.
- `ToolInterceptor::intercept()` calls `DLPScanner::scan(edge, params)` after the policy check passes.
- Policy JSON schema extension: `"dlp_rules"` array on each edge definition.

---

## Feature 3 — Dynamic Policy Hot-Reloading

### Problem
The policy JSON is loaded once at gateway startup. If a security team needs to immediately ban a tool mid-incident (e.g., an agent is actively exfiltrating data), they must restart the gateway, dropping all active sessions and losing their audit trail.

### Feature
Add a `PUT /policy/reload` endpoint that atomically hot-swaps the `PolicyGraph` in memory while all active agent sessions continue processing their in-flight requests.

**Endpoint:**
```
PUT /policy/reload
Content-Type: application/json

{ "policy_file": "/etc/guardian/policies/production.json" }
```

**Or inline:**
```
PUT /policy/reload
Content-Type: application/json

{ "policy": { "nodes": [...], "edges": [...] } }
```

**Behavior:**
- Gateway acquires a `std::unique_lock<std::shared_mutex>` on the policy slot (write lock).
- New `PolicyGraph` is constructed and validated in a temporary variable.
- If validation fails (cycles, invalid nodes), the old policy is unchanged and a `400` is returned with the validation error.
- If validation passes, the `std::shared_ptr<PolicyGraph>` is atomically replaced.
- All in-flight requests that already hold a `std::shared_lock` (read lock) complete with the old policy.
- All new requests after the swap use the new policy.
- A reload event is appended to the audit log.

**Emergency kill-switch:**
```
PUT /policy/reload
{ "policy": { "nodes": [], "edges": [], "default_action": "DENY_ALL" } }
```
This instantly blocks every tool call from every agent with zero restart downtime.

### Technical Risk
**Memory safety**: If `PolicyGraph` construction throws mid-reload, the old policy must remain untouched.

**Mitigation**: Construct the new graph in a `try` block with a local variable. Only replace the shared pointer _after_ successful construction and validation. The `std::shared_ptr` swap is itself a single atomic store.

**Consistency Risk**: A session might evaluate the first half of a two-tool transition under old policy and the second half under new policy.

**Mitigation**: The `SessionManager` locks a session's policy reference at session creation time. Hot-reload applies only to _new_ sessions created after the swap. This is the safe, predictable semantic: "existing sessions finish under their original policy; reload takes effect for new sessions." Document this clearly in the API reference.

### Implementation Notes
- `PolicyGraph` stored as `std::shared_ptr<const PolicyGraph>` inside a `PolicySlot` struct.
- `PolicySlot` holds a `std::shared_mutex` and the current shared pointer.
- All request handlers call `slot.read_lock()` to get a shared pointer to the current graph.
- `PUT /policy/reload` calls `slot.write_swap(new_graph)`.

---

## Feature 4 — Real-time Audit & Incident Dashboard (Web UI)

### Status: Next to build (combines with the planned React UI)

### Problem
The audit endpoint returns raw JSON that is useful for machines but not for security teams responding to an incident in real time. There is no visual way to see whether an agent is looping, which paths it has tried, or to intervene.

### Feature
A **React dashboard** that is the primary interface for Guardian:

**Policy Graph Panel**: Renders the policy graph using `react-flow`. Nodes animate:
- **Pulse green** when a tool call is ALLOWED.
- **Flash red** when a tool call is BLOCKED.
- **Blocked edges** are highlighted in red with the block reason shown in a tooltip.

**Live Event Feed**: Subscribes to `GET /events` (Server-Sent Events stream) from the gateway. Each intercept decision is pushed as an SSE event and reflected on the graph within milliseconds.

**Session Panel**: Lists active sessions. Each session shows its call sequence as a timeline. Clicking a session highlights its traversal path on the graph.

**Loop & Anomaly Alerts**: If a session hits the same blocked edge ≥ 3 times within 60 seconds, the dashboard shows a red alert banner: _"Session X is hitting a policy loop. Consider killing it."_

**Kill Session Button**: Calls `DELETE /session/:id` on the gateway. The gateway marks the session as REVOKED in `SessionManager`. All subsequent intercept calls for that session return `{"allowed": false, "reason": "Session revoked by operator"}` immediately.

**Demo Scenario Buttons**: Three buttons trigger the three demo scenarios by calling a `POST /demo/run/:scenario` endpoint on the gateway, which invokes the Python agent as a subprocess and streams its output.

### Technical Risk
**SSE on Windows with cpp-httplib**: Server-Sent Events require chunked transfer encoding and persistent connections. cpp-httplib supports this via `Response::set_chunked_content_provider`.

**Mitigation**: Implement `GET /events` using `set_chunked_content_provider` with a shared event queue (MPSC channel: `std::deque` + `std::mutex` + `std::condition_variable`). Each intercept call pushes an event to the queue. The SSE handler drains the queue and flushes chunks. Tested on Windows before wiring to React.

**CORS**: React runs on `localhost:3000`; gateway on `localhost:8080`. Browsers will reject cross-origin requests without CORS headers.

**Mitigation**: Add `Access-Control-Allow-Origin: *` and `Access-Control-Allow-Headers: Content-Type` to all gateway responses. For `OPTIONS` preflight, return `204` immediately.

### Implementation Notes
- Gateway additions: CORS middleware, `GET /events` (SSE), `DELETE /session/:id`, `GET /policy/json`, `POST /demo/run/:scenario`.
- Frontend: Vite + React + `@xyflow/react` (react-flow v12) + TailwindCSS.
- See `frontend/` directory.

---

## Feature 5 — pybind11 Native Python Package (`pip install aiguardian`)

### Problem
The HTTP gateway adds ~1–5 ms of network overhead per tool call. For high-frequency agents making hundreds of tool calls per second, this latency accumulates. Developers also want a simpler integration path than running a separate gateway process.

### Feature
Wrap the C++ core using **pybind11** to produce a native Python extension module. Usage:

```python
import aiguardian

# Load policy and create a session in-process — no network hop
guardian = aiguardian.Guardian("policies/demo.json")
session_id = guardian.new_session()

# Intercept a tool call at C++ speed (~5 μs vs ~2 ms over HTTP)
result = guardian.intercept(session_id, from_tool="read_db", to_tool="send_email", params={})
if not result.allowed:
    raise PermissionError(result.reason)
```

The extension exposes the same `PolicyGraph`, `SessionManager`, and `ToolInterceptor` classes that the gateway uses internally. The implementation is zero-copy from the Python perspective — the C++ objects live on the C++ heap and Python holds a reference via pybind11's reference semantics.

### Technical Risk
**ABI stability**: pybind11 extensions must be compiled against the exact Python version and ABI they target. Distributing prebuilt wheels for all Python versions (3.9, 3.10, 3.11, 3.12, 3.13) and platforms requires a CI matrix with `cibuildwheel`.

**Mitigation**: Use `cibuildwheel` in GitHub Actions. Publish to PyPI with a manylinux2_28 wheel for Linux (covers most CI/CD environments), an MSVC wheel for Windows, and a universal2 wheel for macOS. Python 3.11 is the primary target (LTS + most popular for LangChain).

**GIL Risk**: If pybind11 functions call back into Python (e.g., to invoke a Python-defined tool), and another thread is executing C++ code, GIL contention will serialize everything.

**Mitigation**: Release the GIL (`py::gil_scoped_release`) inside all pure-C++ `intercept()` calls. Re-acquire only when constructing the Python return value. The Guardian C++ core has no Python callbacks in the hot path, so this is safe.

**Dependency Risk**: Users of `pip install aiguardian` would get the C++ policy engine but not the gateway. They must separately run the gateway if they also want the React dashboard.

**Mitigation**: The Python package ships with a `aiguardian.gateway` submodule that can `subprocess.Popen` the bundled gateway binary. The binary is included in the wheel as a package data file.

### Implementation Notes
- New `python/` directory with `CMakeLists.txt` (pybind11 target), `aiguardian/__init__.py`, `setup.py` using `cmake`.
- `cmake/FindPybind11.cmake` via `pybind11_add_module`.
- CI: `.github/workflows/publish.yml` using `cibuildwheel`.

---

## Feature 6 — Quota and Time-Based Guardrails

### Problem
LLM agents frequently enter infinite loops (the model keeps retrying the same failed tool call). This burns API credits and can consume expensive database or cloud compute resources without the operator's awareness. There is also no mechanism to enforce change-freeze windows (e.g., no deployments on weekends).

### Feature
Extend the `policy.json` schema to support **quota** and **time** constraints on specific tool nodes or edges:

```json
{
  "nodes": [
    {
      "id": "deploy_hotfix",
      "type": "EXTERNAL_DESTINATION",
      "quota": {
        "max_calls_per_hour": 5,
        "max_calls_per_session": 2
      },
      "allowed_time_window": {
        "days": ["Mon", "Tue", "Wed", "Thu", "Fri"],
        "start_utc": "09:00",
        "end_utc": "17:00",
        "timezone": "America/New_York"
      }
    },
    {
      "id": "call_openai_api",
      "quota": {
        "max_calls_per_hour": 50,
        "cost_per_call_usd": 0.002,
        "max_cost_per_hour_usd": 0.10
      }
    }
  ]
}
```

When a quota is exceeded, the intercept response is:
```json
{
  "allowed": false,
  "reason": "Quota exceeded: deploy_hotfix called 5 times this hour (limit: 5)",
  "quota_resets_at": "2026-03-08T15:00:00Z"
}
```

When a time window is violated:
```json
{
  "allowed": false,
  "reason": "Time restriction: deploy_hotfix is not allowed outside Mon–Fri 09:00–17:00 EST",
  "next_allowed_at": "2026-03-09T14:00:00Z"
}
```

### Technical Risk
**Distributed state**: Quota counters (`calls_this_hour`) are stored in-process memory. If the gateway is restarted or runs as multiple instances behind a load balancer, the counters reset and the quota is not enforced across instances.

**Mitigation (single instance)**: Persist quota counters to a flat file (`quotas.json`) on every increment, with a 1-second debounce to avoid per-call I/O. On restart, the gateway reads the file and resumes from the persisted counts.

**Mitigation (multi-instance)**: For distributed deployments, replace the in-process counter with an atomic Redis `INCR` + `EXPIRE` call. The gateway optionally reads a Redis connection string from the environment (`GUARDIAN_REDIS_URL`). If not set, it falls back to in-process counters with a logged warning. This keeps the single-instance deployment zero-dependency.

**Time Zone Risk**: UTC-based windows are simple to implement but security teams think in local time. "No deployments after 5 PM EST" needs accurate UTC conversion.

**Mitigation**: Use Howard Hinnant's `date.h` (already a common C++ time zone library, header-only) to convert window boundaries from named time zones to UTC. The policy schema uses IANA zone names (`"America/New_York"`), not raw offsets. The library handles DST automatically.

**Quota Bypass**: An agent could interleave calls across many sessions to stay under per-session limits while exceeding a desirable global limit.

**Mitigation**: Quotas are enforced at two levels: per-session (tracked by `SessionManager`) and global per-tool (tracked by an `std::atomic<uint64_t>` in `PolicyGraph`, reset by a background thread on the hour boundary). Both checks must pass.

### Implementation Notes
- New class `QuotaEnforcer` in `src/quota_enforcer.cpp`.
- `ToolInterceptor::intercept()` calls `QuotaEnforcer::check_and_increment(node_id, session_id)` after policy check passes.
- Policy JSON schema extension: `"quota"` and `"allowed_time_window"` objects on node definitions.
- Background thread in `QuotaEnforcer` resets hourly counters using `std::this_thread::sleep_until(next_hour)`.

---

## Priority Order

| Priority | Feature | Effort | Impact |
|---|---|---|---|
| **Now** | Feature 4 — React Dashboard | Medium | Demo/visual impact |
| High | Feature 3 — Hot-Reload Policy | Low | Enterprise credibility |
| High | Feature 2 — Parameter DLP | Medium | Security depth |
| Medium | Feature 5 — pybind11 Package | High | Developer adoption |
| Medium | Feature 6 — Quota Guardrails | Medium | Cloud cost angle |
| Stretch | Feature 1 — Container Sandbox | High | Completeness |

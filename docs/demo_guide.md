# AIGuardian Demo Guide

This guide covers the live SOC Dashboard demo: starting the HTTP gateway, opening the browser dashboard, and walking through all available scenarios and the Playbook Builder.

---

## Prerequisites

- Gateway binary compiled (`build/bin/guardian_gateway` or `.exe` on Windows)
- Any static file server for the frontend (Python's built-in is fine)
- A modern browser (Chrome, Firefox, Edge)

No LLM API key is required. All demo traffic is between the browser and the local gateway.

---

## Starting the demo

### 1 — Start the gateway

The gateway loads a policy file at startup. It must be running before the dashboard is opened.

```bash
# From the repo root
./build/bin/guardian_gateway.exe policies/demo.json 8080   # Windows (MSYS2)
./build/bin/guardian_gateway      policies/demo.json 8080   # Linux
```

You should see:

```
[Guardian] Policy loaded: 7 nodes, 9 edges
[Guardian] HTTP server listening on 0.0.0.0:8080
```

### 2 — Serve the frontend

```bash
python -m http.server 3000 --directory frontend
```

Or any equivalent — the frontend is a single static HTML file with no dependencies.

### 3 — Open the dashboard

Navigate to **`http://localhost:3000`**.

The status badge in the top-right corner should show **● Live** within a second, confirming the SSE stream to the gateway is open.

---

## Dashboard layout

```
┌────────────────────────────────────────┬───────────────────┐
│  Policy Graph — Live Enforcement       │  Demo Scenarios   │
│                                        │  Playbook Builder │
│  SVG graph, nodes pulse green/red      │  Active Sessions  │
│  as each decision fires                │  Live Events      │
└────────────────────────────────────────┴───────────────────┘
```

**Policy Graph (left):** Shows all seven tools and the nine permitted transitions. Node colours indicate classification: red = `SENSITIVE_SOURCE`, blue = `DATA_PROCESSOR`, green = `NORMAL`, orange = `EXTERNAL_DESTINATION`. When a tool call is processed, the relevant node flashes and the edge lights up.

**Demo Scenarios (right, top):** Three pre-built sequences. Click to run.

**Playbook Builder (right, middle):** Type a natural-language prompt or click tool chips to build any sequence, then execute it live.

**Active Sessions:** Shows each open session with call count and blocked count.

**Live Events feed:** Timestamped log of every decision (✓ allowed, ✗ blocked).

---

## Demo Scenarios

### Scenario 1 — Persistent Exfiltration 🔴

**Sequence:** `read_db → send_email → send_email → send_email → create_ticket`

**What it shows:** An agent attempts to send raw database records directly to an external email address. The first attempt is blocked because `read_db` (SENSITIVE_SOURCE) → `send_email` (EXTERNAL_DESTINATION) has no DATA_PROCESSOR in between — this is exfiltration. The agent retries twice more, triggering the **loop detection** banner on the third consecutive block. On the fourth attempt, the agent falls back to filing a ticket, which *is* an allowed transition from `read_db`.

**What to watch:**
- `read_db` pulses green (data read succeeds)
- `send_email` double-flashes red (blocked, exfil detected)
- After the third block, the ⚠️ **Policy Loop Detected** banner fires
- `create_ticket` pulses green as the legitimate fallback

---

### Scenario 2 — Shadow Deploy (learns) 🟡

**Sequence:** `read_code → deploy_hotfix → request_approval → deploy_hotfix → send_email`

**What it shows:** An agent tries to deploy directly from source code, bypassing approval. The first deploy is blocked (no edge from `read_code` to `deploy_hotfix`). The agent then correctly routes through `request_approval`, re-attempts the deploy (now allowed via the `request_approval → deploy_hotfix` edge), and sends a post-deploy notification.

**What to watch:**
- `read_code` pulses green, then a dashed red arc appears for the blocked shortcut to `deploy_hotfix`
- `request_approval` pulses green — the correct gatekeeper step
- `deploy_hotfix` pulses green on the second try
- `send_email` completes the chain (allowed via `deploy_hotfix → send_email`)

---

### Scenario 3 — Full Incident Response 🟢

**Sequence:** `search_kb → create_ticket → request_approval → deploy_hotfix → send_email`

**What it shows:** A fully policy-compliant five-step incident response workflow. Every step is approved. This demonstrates the "happy path": consult documentation, file a ticket, escalate for approval, deploy the fix, notify the team.

**What to watch:** Every node pulses green in sequence, every connecting edge lights up. No blocks. The live feed shows five `✓` entries.

---

## Playbook Builder

The Playbook Builder is designed for interactive exploration — useful for demos where you want to show the audience how policy decisions react to arbitrary inputs in real time.

### Natural-language prompt interpreter

Type a sentence describing what an AI agent should do. The interpreter scans the text for keywords associated with each tool and infers the intended sequence based on mention order.

**Example prompts:**

| Prompt | Inferred sequence | Outcome |
|---|---|---|
| *"Read the database and email the results directly to the client"* | `read_db → send_email` | 🔴 BLOCKED — exfiltration |
| *"Search the docs, raise a ticket, get approval, then deploy"* | `search_kb → create_ticket → request_approval → deploy_hotfix` | 🟢 All approved |
| *"Pull the source code and push it straight to production"* | `read_code → deploy_hotfix` | 🔴 BLOCKED — no approval step |
| *"Deploy the hotfix and notify the team by email"* | `deploy_hotfix → send_email` | 🟢 Approved (direct notify after deploy) |

After clicking **✦ Interpret**, a confirmation line shows the detected tool sequence. You can then adjust manually using the chip palette before running.

### Manual chip palette

Click tools directly to add them to the sequence. Each chip is colour-coded:

| Colour | Risk level |
|---|---|
| 🟢 Green border | LOW |
| 🟡 Yellow border | MEDIUM |
| 🟠 Orange border | HIGH |
| 🔴 Red border | CRITICAL |

The contrast between `search_kb → send_email` (both LOW, allowed) and `read_db → send_email` (HIGH → CRITICAL, blocked) is a strong talking point: the same destination tool is allowed from one source and blocked from another, based purely on the data classification of the source.

### Running the playbook

Click **▶ Run Playbook**. The sequence executes with a ~1 second pause between steps, animating the graph in real time. All three preset scenario buttons are disabled during a run to prevent interference.

---

## Kill Session

Each entry in the **Active Sessions** panel has a **Kill** button. Clicking it sends `DELETE /session/:id` to the gateway, which ends the session server-side and broadcasts a `session_killed` SSE event. The session disappears from the panel and an entry appears in the live feed.

The loop-detection banner also has an inline **Kill Session** button that appears when a loop is detected, so the operator can immediately terminate the offending session.

---

## Running demo.py (optional — requires Groq API key)

`demo/demo.py` runs three LangChain + Groq scenarios against the live gateway. It requires a `GROQ_API_KEY` environment variable and calls the real LLM.

```bash
cd demo
source venv/Scripts/activate   # Windows MSYS2
# source venv/bin/activate       # Linux

# Run all scenarios
python demo.py

# Run a specific scenario
python demo.py --scenario 1
python demo.py --scenario 2
python demo.py --scenario 3
```

When running, the SSE stream picks up every intercept call and the dashboard animates them in real time — the same graph, but now driven by an actual LLM agent instead of scripted sequences.

---

## Policy reference (demo.json)

The demo policy encodes these seven tools and nine permitted transitions:

```
read_code  ──────────────────────────────┐
                                         ▼
read_db    ──────────────────────────► request_approval ──► deploy_hotfix ──► send_email
           │                            ▲
           ▼                            │
           create_ticket ───────────────┘

search_kb  ──► create_ticket
search_kb  ──► send_email
read_db    ──► create_ticket
```

| Edge | Interpretation |
|---|---|
| `read_db → request_approval` | DB data must pass through human approval before any action |
| `read_db → create_ticket` | DB data can be used to file an incident ticket (safe path) |
| `read_code → request_approval` | Code changes require approval |
| `request_approval → deploy_hotfix` | Approved changes may be deployed |
| `request_approval → send_email` | Approved actions can trigger notifications |
| `deploy_hotfix → send_email` | Post-deploy notification is allowed |
| `create_ticket → request_approval` | A filed ticket can escalate to approval |
| `search_kb → create_ticket` | KB results can inform ticket creation |
| `search_kb → send_email` | Low-risk KB summary can be emailed directly |

Note: `read_db → send_email` is **intentionally absent**. Raw DB records may never go directly to email.

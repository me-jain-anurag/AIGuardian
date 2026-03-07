# AIGuardian Demo Scenario Guide

The CLI Demo Tool (`guardian_cli.exe`) includes three pre-build scenarios to showcase different security enforcement capabilities.

## 1. Financial Exfiltration (`--scenario finance`)
### The Setup
An agent reads sensitive financial statements from a database and attempts to immediately upload them to a public server.

### The Security Result
**BLOCKED.** AIGuardian detects a direct flow from a `SENSITIVE_SOURCE` to an `EXTERNAL_DESTINATION` without any intervening sanitization, preventing the data leak.

---

## 2. Safe Developer Workflow (`--scenario dev`)
### The Setup
An agent performs a standard coding task: pulling code, running local unit tests, and then requesting approval before deployment.

### The Security Result
**APPROVED.** The sequence follows a valid, pre-defined path in the developer policy graph.

---

## 3. Denial of Service Prevention (`--scenario dos`)
### The Setup
An agent is trapped in a reasoning loop and calls the `search_database` tool repeatedly 100 times.

### The Security Result
**BLOCKED.** AIGuardian's cycle detection engine identifies the repetitive pattern and halts the agent after the 10th call, protecting system resources.

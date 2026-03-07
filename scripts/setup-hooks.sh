#!/bin/sh
# scripts/setup-hooks.sh
# Installs git hooks for all developers
# Run once after cloning: sh scripts/setup-hooks.sh

HOOK_DIR=".git/hooks"
SCRIPT_DIR="scripts/hooks"

echo "Installing git hooks..."

# Copy hooks
cp "$SCRIPT_DIR/pre-commit" "$HOOK_DIR/pre-commit"
cp "$SCRIPT_DIR/commit-msg" "$HOOK_DIR/commit-msg"

# Make executable (needed on Linux, harmless on Windows Git Bash)
chmod +x "$HOOK_DIR/pre-commit"
chmod +x "$HOOK_DIR/commit-msg"

echo "✓ Hooks installed:"
echo "  - pre-commit: whitespace, CRLF, large files, #pragma once"
echo "  - commit-msg: conventional commit format"
echo ""
echo "To bypass hooks for a single commit: git commit --no-verify"

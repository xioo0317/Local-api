#!/usr/bin/env bash
# test_api.sh - API test suite for local_api
#
# All tools are placeholders: tools take no parameters and nothing is executed.
#
# Usage: ./test_api.sh [host] [port]
# Default: host=127.0.0.1, port=8080

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"

PASS=0
FAIL=0
TOTAL=0

check() {
    TOTAL=$((TOTAL + 1))
    if [ "$2" = "1" ]; then
        PASS=$((PASS + 1))
        echo "  [PASS] $1"
    else
        FAIL=$((FAIL + 1))
        echo "  [FAIL] $1${3:+ — $3}"
    fi
}

echo "==================================================="
echo "  local_api Test Suite"
echo "  Target: ${BASE_URL}"
echo "==================================================="
echo ""

# ── 1. POST / action=detect ──────────────────────────────
echo "[1/9] POST / action=detect"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"detect"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "status placeholder" "$(echo "$BODY" | grep -q '"status":"placeholder"' && echo 1 || echo 0)"
check "echoes action" "$(echo "$BODY" | grep -q '"action":"detect"' && echo 1 || echo 0)"

# ── 2. POST / action=version ──────────────────────────────
echo "[2/9] POST / action=version"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"version"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "status ok" "$(echo "$BODY" | grep -q '"status":"ok"' && echo 1 || echo 0)"
check "has server_version" "$(echo "$BODY" | grep -q '"server_version"' && echo 1 || echo 0)"

# ── 3. POST / action=debug ──────────────────────────────
echo "[3/9] POST / action=debug"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"debug"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has tools" "$(echo "$BODY" | grep -q '"tools"' && echo 1 || echo 0)"
check "reports is_root" "$(echo "$BODY" | grep -q '"is_root"' && echo 1 || echo 0)"

# ── 4. GET /debug ────────────────────────────────────────
echo "[4/9] GET /debug"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" "$BASE_URL/debug")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has tools" "$(echo "$BODY" | grep -q '"tools"' && echo 1 || echo 0)"

# ── 5. Unknown action ───────────────────────────────────
echo "[5/9] POST / unknown action"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"nope"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "error response" "$(echo "$BODY" | grep -q '"error"' && echo 1 || echo 0)"
check "lists available_actions" "$(echo "$BODY" | grep -q '"available_actions"' && echo 1 || echo 0)"

# ── 6. Invalid JSON body ─────────────────────────────────
echo "[6/9] POST / invalid JSON"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d 'not-json' "$BASE_URL/")
check "HTTP 400" "$([ "${HTTP_CODE}" = "400" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "error response" "$(cat /tmp/api_body | grep -q '"error"' && echo 1 || echo 0)"

# ── 7. Missing action field ──────────────────────────────
echo "[7/9] POST / missing action"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{}' "$BASE_URL/")
check "HTTP 400" "$([ "${HTTP_CODE}" = "400" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "lists available_actions" "$(cat /tmp/api_body | grep -q '"available_actions"' && echo 1 || echo 0)"

# ── 8. CLI dispatch: install (no server needed) ──────────
echo "[8/9] CLI dispatch: install"
BIN="./build/local_api"
if [ ! -x "$BIN" ]; then BIN="./local_api"; fi
rc=0
out=$("$BIN" install 2>&1) || rc=$?
check "install banner" "$(echo "$out" | grep -q "install" && echo 1 || echo 0)"
check "root detection menu shown (auto-default on non-tty)" "$(echo "$out" | grep -q "Root 环境检测方式" && echo 1 || echo 0)"
check "volume-key menu shown (auto-default on non-tty)" "$(echo "$out" | grep -q '\[menu\]' && echo 1 || echo 0)"
if [ "$rc" -eq 0 ]; then
    check "install ok (complete or already-initialized)" "$(echo "$out" | grep -qE "complete|nothing to do" && echo 1 || echo 0)"
else
    check "install fails cleanly on non-root host (rc=${rc})" "$(echo "$out" | grep -q "error: failed to create" && echo 1 || echo 0)"
fi

# ── 9. CLI dispatch: debug entry (master-switch gated) ───
echo "[9/9] CLI dispatch: debug"
rc=0
out=$("$BIN" debug detector 2>&1) || rc=$?
check "debug banner" "$(echo "$out" | grep -q "debug" && echo 1 || echo 0)"
check "debug detector text or disabled" "$(echo "$out" | grep -qE "detected|kernelsu|disabled" && echo 1 || echo 0)"
check "root mode line in detector output" "$(echo "$out" | grep -qE "mode     :|disabled" && echo 1 || echo 0)"
check "SELinux line in detector output" "$(echo "$out" | grep -qE "SELinux|disabled" && echo 1 || echo 0)"

echo ""
echo "==================================================="
echo "  Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "==================================================="

[ "${FAIL}" = "0" ]

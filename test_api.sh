#!/usr/bin/env bash
# test_api.sh - API test suite for local_api
#
# Usage: ./test_api.sh [host] [port]
# Default: host=127.0.0.1, port=8080

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"
RESULT_FILE="/data/local/tmp/coverRoot/root_detect.json"

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
echo "[1/10] POST / action=detect"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"detect"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "status ok" "$(echo "$BODY" | grep -q '"status":"ok"' && echo 1 || echo 0)"
check "has result" "$(echo "$BODY" | grep -q '"result"' && echo 1 || echo 0)"
check "has detected" "$(echo "$BODY" | grep -q '"detected"' && echo 1 || echo 0)"

# ── 2. POST / action=version ──────────────────────────────
echo "[2/10] POST / action=version"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"version"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has server_version" "$(echo "$BODY" | grep -q '"server_version"' && echo 1 || echo 0)"

# ── 3. POST / action=debug ──────────────────────────────
echo "[3/10] POST / action=debug"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"debug"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has tools" "$(echo "$BODY" | grep -q '"tools"' && echo 1 || echo 0)"
check "has detect_dry_run" "$(echo "$BODY" | grep -q '"detect_dry_run"' && echo 1 || echo 0)"
check "reports is_root" "$(echo "$BODY" | grep -q '"is_root"' && echo 1 || echo 0)"

# ── 4. GET /debug ────────────────────────────────────────
echo "[4/10] GET /debug"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" "$BASE_URL/debug")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has tool_status" "$(echo "$BODY" | grep -q '"tool_status"' && echo 1 || echo 0)"

# ── 5. Unknown action ───────────────────────────────────
echo "[5/10] POST / unknown action"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"nope"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "error response" "$(echo "$BODY" | grep -q '"error"' && echo 1 || echo 0)"
check "lists available_actions" "$(echo "$BODY" | grep -q '"available_actions"' && echo 1 || echo 0)"

# ── 6. POST / action=hide_icon ───────────────────────────
# enabled=false keeps the icon visible during the test.
echo "[6/10] POST / action=hide_icon"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"hide_icon","enabled":false}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has status" "$(echo "$BODY" | grep -q '"status"' && echo 1 || echo 0)"
check "echoes enabled" "$(echo "$BODY" | grep -q '"enabled":false' && echo 1 || echo 0)"
# Missing parameter must be rejected
ERR_BODY=$(curl -s -X POST -H "Content-Type: application/json" -d '{"action":"hide_icon"}' "$BASE_URL/")
check "missing param errors" "$(echo "$ERR_BODY" | grep -q '"status":"error"' && echo 1 || echo 0)"

# ── 7. POST / action=susfs_setup ─────────────────────────
echo "[7/10] POST / action=susfs_setup"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"susfs_setup","paths":["/data/adb/test","/system/xbin"]}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has status" "$(echo "$BODY" | grep -q '"status"' && echo 1 || echo 0)"
check "has configured_paths" "$(echo "$BODY" | grep -q '"configured_paths"' && echo 1 || echo 0)"
# Relative path must be rejected
ERR_BODY=$(curl -s -X POST -H "Content-Type: application/json" -d '{"action":"susfs_setup","paths":["relative/path"]}' "$BASE_URL/")
check "relative path errors" "$(echo "$ERR_BODY" | grep -q '"status":"error"' && echo 1 || echo 0)"

# ── 8. POST / action=hide_app_list ───────────────────────
echo "[8/10] POST / action=hide_app_list"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"hide_app_list","packages":["com.example.app1","com.example.app2"]}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has status" "$(echo "$BODY" | grep -q '"status"' && echo 1 || echo 0)"
check "has hidden_packages" "$(echo "$BODY" | grep -q '"hidden_packages"' && echo 1 || echo 0)"
# Bad package name must be rejected
ERR_BODY=$(curl -s -X POST -H "Content-Type: application/json" -d '{"action":"hide_app_list","packages":["not a package!"]}' "$BASE_URL/")
check "bad package errors" "$(echo "$ERR_BODY" | grep -q '"status":"error"' && echo 1 || echo 0)"

# ── 9. POST / action=update_key ──────────────────────────
echo "[9/10] POST / action=update_key"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"update_key","key":"new_secret_key_123"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has status" "$(echo "$BODY" | grep -q '"status"' && echo 1 || echo 0)"
check "has message" "$(echo "$BODY" | grep -q '"message"' && echo 1 || echo 0)"
# Empty key must be rejected
ERR_BODY=$(curl -s -X POST -H "Content-Type: application/json" -d '{"action":"update_key","key":""}' "$BASE_URL/")
check "empty key errors" "$(echo "$ERR_BODY" | grep -q '"status":"error"' && echo 1 || echo 0)"

# ── 10. POST / action=set_hash ───────────────────────────
echo "[10/10] POST / action=set_hash"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"set_hash","hash":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has status" "$(echo "$BODY" | grep -q '"status"' && echo 1 || echo 0)"
check "echoes hash" "$(echo "$BODY" | grep -q '"hash"' && echo 1 || echo 0)"
# Non-hex hash must be rejected
ERR_BODY=$(curl -s -X POST -H "Content-Type: application/json" -d '{"action":"set_hash","hash":"not-a-hash"}' "$BASE_URL/")
check "invalid hash errors" "$(echo "$ERR_BODY" | grep -q '"status":"error"' && echo 1 || echo 0)"

echo ""
echo "==================================================="
echo "  Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "==================================================="

[ "${FAIL}" -gt 0 ] && exit 1
exit 0

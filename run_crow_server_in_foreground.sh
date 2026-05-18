#!/usr/bin/env bash
set -euo pipefail

# Resolve repo root regardless of where the script is called from
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$REPO_ROOT/build/portfolio_cpp"
PORT="${PORT:-18080}"
BIND_HOST="${BIND_HOST:-0.0.0.0}"

# ── Colour helpers ────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

info()  { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()    { echo -e "${GREEN}[ OK ]${RESET}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
die()   { echo -e "${RED}[ERR ]${RESET}  $*" >&2; exit 1; }

SERVER_PID=""

cleanup() {
    local status=$?

    trap - INT TERM EXIT

    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        warn "Stopping server (pid $SERVER_PID)"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi

    exit "$status"
}

trap cleanup INT TERM EXIT

# ── Pre-flight checks ─────────────────────────────────────────────────────────
info "Repo root : $REPO_ROOT"
info "Binary    : $BINARY"
info "Host      : $BIND_HOST"
info "Port      : $PORT"
echo

if [ ! -f "$BINARY" ]; then
    info "Binary not found — building now"
    echo

    command -v cmake &>/dev/null || die "cmake not found. Install it and try again."

    cmake -S "$REPO_ROOT" -B "$REPO_ROOT/build" || die "CMake configure failed."
    cmake --build "$REPO_ROOT/build" || die "CMake build failed."
    echo
fi

if [ ! -x "$BINARY" ]; then
    warn "Binary is not executable — fixing permissions"
    chmod +x "$BINARY"
fi

# Check if the port is already in use
if command -v lsof &>/dev/null; then
    if lsof -iTCP:"$PORT" -sTCP:LISTEN -t &>/dev/null; then
        die "Port $PORT is already in use. Set a different port:\n\n    PORT=9090 $0"
    fi
elif command -v ss &>/dev/null; then
    if ss -tlnp | grep -q ":$PORT "; then
        die "Port $PORT is already in use. Set a different port:\n\n    PORT=9090 $0"
    fi
fi

ok "All checks passed — starting server"
echo
echo -e "  ${BOLD}Dashboard${RESET}  →  http://$BIND_HOST:$PORT"
echo -e "  ${BOLD}API docs${RESET}   →  http://$BIND_HOST:$PORT/openapi.json"
echo
echo -e "  Press ${BOLD}Ctrl+C${RESET} to stop"
echo

# ── Run (terminal-attached, but tracked so Ctrl+C can clean it up) ────────────
export PORT
export BIND_HOST
"$BINARY" &
SERVER_PID=$!

set +e
wait "$SERVER_PID"
SERVER_STATUS=$?
set -e

SERVER_PID=""
trap - INT TERM EXIT
exit "$SERVER_STATUS"

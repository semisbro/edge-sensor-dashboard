#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRONTEND_DIR="$REPO_ROOT/frontend"
DIST_DIR="$FRONTEND_DIR/dist"
ROOT_INDEX="$REPO_ROOT/index.html"
PAGES_ASSETS_DIR="$REPO_ROOT/assets"
PORT="${PORT:-18080}"
BIND_HOST="${BIND_HOST:-0.0.0.0}"
VITE_CROW_URL="${VITE_CROW_URL:-$BIND_HOST:$PORT}"

info() { printf '[INFO] %s\n' "$*"; }
ok() { printf '[ OK ] %s\n' "$*"; }
die() {
    printf '[ERR ] %s\n' "$*" >&2
    exit 1
}

command -v yarn >/dev/null 2>&1 || die "yarn not found. Install Yarn and try again."
[ -f "$FRONTEND_DIR/package.json" ] || die "frontend/package.json not found."

info "Building frontend for GitHub Pages preview"
(
    cd "$FRONTEND_DIR"
    export VITE_CROW_URL
    yarn build --base ./
)

[ -f "$DIST_DIR/index.html" ] || die "Build did not produce $DIST_DIR/index.html."
[ -d "$DIST_DIR/assets" ] || die "Build did not produce $DIST_DIR/assets."

info "Copying built entry to repo root"
cp "$DIST_DIR/index.html" "$ROOT_INDEX"

info "Copying built assets to repo root"
mkdir -p "$PAGES_ASSETS_DIR"
find "$PAGES_ASSETS_DIR" -maxdepth 1 -type f \( -name 'index-*.js' -o -name 'index-*.css' \) -delete
cp -R "$DIST_DIR/assets/." "$PAGES_ASSETS_DIR/"

ok "GitHub Pages entry ready: $ROOT_INDEX"

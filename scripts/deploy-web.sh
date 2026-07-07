#!/bin/sh
# Assemble the web gallery (rss-sdl2-gles2 pattern): copy each hack's
# emscripten build from build-web/ into web/<hack>/, then generate the
# thumbnail index (scripts/gen_gallery.py -- thumbs come from the
# harness screenshots in /tmp/xss_harness).
#
# Usage: scripts/deploy-web.sh [build-web-dir]
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
WEB=${1:-build-web}

n=0
for f in "$WEB"/*.html; do
  h=$(basename "$f" .html)
  [ "$h" = "index" ] && continue
  mkdir -p "web/$h"
  cp "$WEB/$h.html" "$WEB/$h.js" "$WEB/$h.wasm" "web/$h/" 2>/dev/null || {
    echo "skip $h (incomplete build)"; continue; }
  n=$((n+1))
done
echo "deployed $n hacks into web/"

python3 scripts/gen_gallery.py
echo "Serve with: cd web && python3 -m http.server"

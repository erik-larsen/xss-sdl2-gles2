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

# bundled CC0 image set for the grab hacks (shared across all pages;
# each hack page prefetches one at random -- see src/web/shell.html)
mkdir -p web/images
cp assets/images/*.jpg web/images/ 2>/dev/null || true
( cd web/images && ls *.jpg 2>/dev/null | \
  python3 -c "import json,sys; json.dump([l.strip() for l in sys.stdin], open('index.json','w'))" )

python3 scripts/gen_gallery.py
python3 scripts/gen_options.py
echo "Serve with: python3 scripts/serve-web.py"

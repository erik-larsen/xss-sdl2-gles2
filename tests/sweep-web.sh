#!/bin/sh
# Full-gallery web sweep: run tests/verify-web.js on EVERY built hack
# page (smoke-web.sh only samples 8) and write a TSV verdict sheet.
# This is how the "233/233 web" claim gets actually verified.
#
# Usage: tests/sweep-web.sh [build-web-dir] [out.tsv]
# Requires tests/node_modules (npm ci in tests/) and Chrome
# (CHROME_BIN, or the verify-web.js lookup paths).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
WEB=${1:-$ROOT/build-web}
OUT=${2:-$ROOT/tests/STATUS-web.tsv}
PORT=${PORT:-8078}
JOBS_N=${JOBS_N:-4}

( cd "$WEB" && python3 -m http.server "$PORT" >/dev/null 2>&1 ) &
SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT
sleep 1

: > "$OUT"
JOBS=0
for f in "$WEB"/*.html; do
  h=$(basename "$f" .html)
  [ "$h" = "index" ] && continue
  (
    # watchdog kill: macOS has no timeout(1), and a wedged page load
    # must not hang the whole sweep (anemone once did).
    VJS="$HERE/verify-web.js"
    node "$VJS" "http://localhost:$PORT/$h.html" "/tmp/sweep_$h.png" 9000 \
        > "/tmp/sweep_$h.out" 2>/dev/null & np=$!
    ( sleep 75; kill -9 $np 2>/dev/null ) > /dev/null 2>&1 & wd=$!
    wait $np 2>/dev/null
    kill $wd 2>/dev/null
    out=$(grep -E '^\{' "/tmp/sweep_$h.out" | tail -1)
    [ -z "$out" ] && out='{"nonBlank":false,"error":"no-output"}'
    printf '%s\t%s\n' "$h" "$out" >> "$OUT"
  ) &
  JOBS=$((JOBS+1))
  if [ $((JOBS % JOBS_N)) -eq 0 ]; then wait; fi
done
wait
sort -o "$OUT" "$OUT"
total=$(wc -l < "$OUT" | tr -d ' ')
pass=$(grep -c '"nonBlank":true' "$OUT" || true)
echo "sweep: $pass/$total non-blank -> $OUT"
grep -v '"nonBlank":true' "$OUT" | cut -f1 | tr '\n' ' ' | sed 's/^/blank\/failed: /'
echo ""

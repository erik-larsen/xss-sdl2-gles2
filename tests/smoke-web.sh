#!/bin/sh
# Headless browser smoke test for the emscripten build.
#
# Serves build-web/ over HTTP and loads each hack in headless Chromium
# (SwiftShader WebGL), sampling the canvas twice via compositor
# screenshots to assert non-blank + frame-to-frame change. Mirrors
# tests/smoke.sh (the native suite) for the web target.
#
# Requires: a built build-web/, and a verifier with puppeteer-core +
# pngjs resolvable. Defaults to the committed tests/verify-web.js (run
# `npm ci` in tests/ first, or set CHROME_BIN); override with VERIFY=.
# Usage: tests/smoke-web.sh [build-web-dir]
set -e
WEB=${1:-build-web}
PORT=8077
HERE=$(cd "$(dirname "$0")" && pwd)
VERIFY=${VERIFY:-$HERE/verify-web.js}

( cd "$WEB" && python3 -m http.server "$PORT" >/dev/null 2>&1 ) &
SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT
sleep 1

fail=0
_run() { # name waitMs -> sets $out
  out=$(timeout 180 node "$VERIFY" \
        "http://localhost:$PORT/$1.html" "/tmp/smokeweb_$1.png" "$2" \
        2>/dev/null | grep -E '^\{' | tail -1)
}
check() { # name waitMs -- assert non-blank
  printf '%-14s ' "$1"; _run "$1" "$2"
  case "$out" in
    *'"nonBlank":true'*) echo "PASS $out" ;;
    *) echo "FAIL $out"; fail=1 ;;
  esac
}
check_runonly() { # name waitMs -- must run, but blank is not a failure
  printf '%-14s ' "$1"; _run "$1" "$2"
  case "$out" in
    *'"nonBlank":true'*) echo "PASS $out" ;;
    '')                  echo "FAIL(run) $1 (no verdict)"; fail=1 ;;
    *) echo "PASS(run-only) $out" ;;
  esac
}

check testpattern 9000
check deco        9000
# GL hacks: the gl4es-on-WebGL client-array blank was root-caused and
# FIXED (PATCH(xss-sdl) in third_party/gl4es/src/gl/fpe.c realize_glenv;
# see docs/EMSCRIPTEN.md / src/port/glx-sdl.c). All now render+animate.
check gears       9000
check hypertorus  9000
check polyhedra   9000
check lament      9000
# bouncingcow: M2b known issue -- black on software renderers (CI's
# SwiftShader, and llvmpipe natively), correct on real/ANGLE GPUs. Same
# run-only treatment as the native tests/smoke.sh, so it never blocks the
# Pages deploy over a software-rasteriser quirk.
check_runonly bouncingcow 9000
# glmatrix's rain fills from the top over time -- give it a longer warmup
# so the canvas is comfortably non-blank when sampled.
check glmatrix    12000

exit $fail

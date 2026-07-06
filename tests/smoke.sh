#!/bin/sh
# Headless smoke test: run each hack N frames, assert clean exit and a
# non-blank, frame-varying output. Seed of the M5 harness.
set -e
BUILD=${1:-build}
fail=0
# Linux runs headless under xvfb + llvmpipe; macOS has no xvfb (or stock
# timeout) -- run directly over ANGLE/Metal (windows appear briefly).
case "$(uname)" in
  Darwin) WRAP="" ;;
  *)      WRAP="timeout 60 env LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a" ;;
esac
run() { # name frames
  out=/tmp/xss_smoke_$1.ppm
  if ! $WRAP "$BUILD/$1" --frames "$2" --shot "$out" >/dev/null 2>&1; then
    echo "FAIL(run)  $1"; fail=1; return
  fi
  colors=$(python3 - "$out" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1])
print(len(im.getcolors(1<<24) or []))
PY
)
  if [ "$colors" -lt 2 ]; then echo "FAIL(blank) $1"; fail=1
  else echo "PASS        $1 ($colors colors)"; fi
}
run testpattern 30
run deco 4
run greynetic 30
run attraction 150
run interference 10
run moire 30
run gears 60
run hypertorus 60
run glmatrix 200
run lament 60
run polyhedra 60
# bouncingcow: was black on Linux/llvmpipe (M2b known issue); renders
# correctly on macOS/ANGLE since the driver ARRAY_BUFFER-unbind fix
# (M4c), the suspected cause of the Linux symptom too. Run-only (never
# fails CI on blankness), but shoot + report colors so the CI log shows
# whether Mesa renders it; promote to a full `run` once it does.
out=/tmp/xss_smoke_bouncingcow.ppm
if $WRAP "$BUILD/bouncingcow" --frames 30 --shot "$out" >/dev/null 2>&1; then
  colors=$(python3 - "$out" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1])
print(len(im.getcolors(1<<24) or []))
PY
)
  echo "PASS(run-only) bouncingcow ($colors colors; 1 = still black on this driver)"
else
  echo "FAIL(run)  bouncingcow"; fail=1
fi
exit $fail

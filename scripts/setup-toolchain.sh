#!/bin/sh
# Check (or install) the toolchain this repo is pinned to.
#
#   sh scripts/setup-toolchain.sh            # report what is here vs pinned
#   sh scripts/setup-toolchain.sh --install  # also install the pinned emsdk
#
# The point is that a developer machine and CI build with the same
# emscripten. Everything else (cmake, ninja, a C compiler, SDL2, libpng)
# comes from the system: this reports what it finds so a mismatch is
# visible, but does not try to manage it -- see docs/TOOLCHAIN.md for why.
#
# --install clones/updates emsdk into $EMSDK_DIR (default ~/emsdk) and
# installs + activates the pinned version. It never touches an existing
# emsdk activation unless you ask for it.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
VERSIONS="$ROOT/toolchain.versions"
EMSDK_DIR=${EMSDK_DIR:-$HOME/emsdk}

pinned() { grep -E "^$1=" "$VERSIONS" | head -1 | cut -d= -f2-; }

EMSDK_VERSION=$(pinned EMSDK_VERSION)
NODE_VERSION=$(pinned NODE_VERSION)
CHROME_VERSION=$(pinned CHROME_VERSION)

say() { printf '%-22s %s\n' "$1" "$2"; }

echo "pinned by toolchain.versions"
say "  emscripten" "$EMSDK_VERSION"
say "  node (tests)" "$NODE_VERSION"
say "  chrome (tests)" "$CHROME_VERSION"
echo

echo "found on this machine"
if command -v emcc > /dev/null 2>&1; then
  # emcc reports "4.0.12-git" for a build from an emsdk checkout: same
  # release, extra commits. Treat that as a match, and say so.
  have=$(emcc --version 2>/dev/null | head -1 |
         sed -E 's/.*replacement \+ linker emulating GNU ld\) ([^ ]+).*/\1/')
  base=${have%-git}
  if [ "$base" = "$EMSDK_VERSION" ]; then
    if [ "$have" != "$base" ]; then
      say "  emscripten" "$have (matches $EMSDK_VERSION, built from a checkout)"
    else
      say "  emscripten" "$have"
    fi
  else
    say "  emscripten" "$have  <-- PINNED IS $EMSDK_VERSION"
    MISMATCH=1
  fi
  say "  emcc path" "$(command -v emcc)"
else
  say "  emscripten" "not on PATH  <-- PINNED IS $EMSDK_VERSION"
  MISMATCH=1
fi

for t in cmake ninja python3; do
  if command -v "$t" > /dev/null 2>&1; then
    say "  $t" "$("$t" --version 2>&1 | head -1)"
  else
    say "  $t" "not found"
  fi
done

# node and Chrome do not affect what gets built -- they are the headless
# verification rig (tests/verify-web.js). A mismatch still matters,
# because the sweep is how a toolchain bump gets believed, so it warns
# rather than fails: a different major can change puppeteer's behaviour
# and hand you verdicts CI will not reproduce.
if command -v node > /dev/null 2>&1; then
  have=$(node --version | sed 's/^v//')
  if [ "${have%%.*}" = "${NODE_VERSION%%.*}" ]; then
    say "  node (tests)" "v$have"
  else
    say "  node (tests)" "v$have  <-- PINNED IS $NODE_VERSION (sweep may differ from CI)"
    SOFT=1
  fi
else
  say "  node (tests)" "not found"
fi

CHROME_APP="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
if [ -n "${CHROME_BIN:-}" ] && [ -x "${CHROME_BIN}" ]; then
  say "  chrome (tests)" "$("$CHROME_BIN" --version 2>/dev/null) (CHROME_BIN)"
elif [ -x "$CHROME_APP" ]; then
  have=$("$CHROME_APP" --version 2>/dev/null | sed 's/^Google Chrome //')
  if [ "${have%%.*}" = "${CHROME_VERSION%%.*}" ]; then
    say "  chrome (tests)" "$have"
  else
    say "  chrome (tests)" "$have  <-- PINNED IS $CHROME_VERSION (sweep may differ from CI)"
    SOFT=1
  fi
else
  say "  chrome (tests)" "not found (verify-web.js will look for it)"
fi

# SDL2 and libpng are the two native dependencies CMake resolves from the
# system. Report where they came from -- on macOS a Frameworks install and
# a Homebrew dylib are different products with different version numbers,
# which is exactly the kind of drift worth seeing.
if [ -d /Library/Frameworks/SDL2.framework ]; then
  v=$(defaults read /Library/Frameworks/SDL2.framework/Resources/Info.plist \
        CFBundleShortVersionString 2>/dev/null || echo "?")
  say "  SDL2" "$v (/Library/Frameworks)"
elif command -v sdl2-config > /dev/null 2>&1; then
  say "  SDL2" "$(sdl2-config --version) ($(command -v sdl2-config))"
else
  say "  SDL2" "not found"
fi

if [ "${1:-}" = "--install" ]; then
  echo
  echo "installing emscripten $EMSDK_VERSION into $EMSDK_DIR"
  if [ ! -d "$EMSDK_DIR" ]; then
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
  fi
  (cd "$EMSDK_DIR" && git fetch --tags --quiet &&
   ./emsdk install "$EMSDK_VERSION" && ./emsdk activate "$EMSDK_VERSION")
  echo
  echo "activate it in this shell with:"
  echo "  . $EMSDK_DIR/emsdk_env.sh"
  exit 0
fi

if [ -n "${SOFT:-}" ] && [ -z "${MISMATCH:-}" ]; then
  echo
  echo "note: the build is pinned and matches; only the test rig differs."
  echo "The sweep will still run -- just remember CI verifies with"
  echo "node $NODE_VERSION / chrome $CHROME_VERSION when the two disagree."
fi

if [ -n "${MISMATCH:-}" ]; then
  echo
  echo "emscripten does not match the pin. Either:"
  echo "  sh scripts/setup-toolchain.sh --install   (install the pinned one)"
  echo "  or edit toolchain.versions if the pin should move -- then run"
  echo "  python3 scripts/check-toolchain.py and push, so CI moves with it."
  exit 1
fi

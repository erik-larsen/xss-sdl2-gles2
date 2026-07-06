# Building for the web (emscripten)

The CMake build doubles as the web build via `emcmake`. The only
emscripten-specific code is the main-loop driver, the query-string
argv shim, and a handful of `#ifdef __EMSCRIPTEN__` guards documented
in MILESTONES.md (M3a).

## Prerequisites
- emscripten SDK (tested with 3.1.6) with a writable cache
- SDL2, zlib, libpng emscripten ports (fetched by emcc on first build)

## Build
    emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build-web --target testpattern deco gears

Each hack produces `<name>.html` + `.js` + `.wasm`. The HTML uses a
custom full-window shell (`src/web/shell.html`): a black, edge-to-edge
canvas with no emscripten branding or controls, a brief load veil, and
the tab title set at runtime from `SDL_SetWindowTitle()`. The build is
single-threaded, so the page needs no COOP/COEP headers and hosts from
any static server (including GitHub Pages).

Serve over HTTP
(SharedArrayBuffer is not required; the build is single-threaded to
avoid COOP/COEP):

    cd build-web && python3 -m http.server 8077
    # open http://localhost:8077/deco.html

## CLI options on the web
Native CLI flags map to query-string parameters:

    deco.html?frames=600          # run 600 frames then stop
    gears.html?width=1024&height=768

`xss_web_args()` (src/driver/xss_driver.c) parses `location.search`
into an argv before calling the driver.

## Verification
`tests/smoke-web.sh` loads each hack in headless Chromium and asserts a
non-blank, animating canvas. It needs node with `puppeteer-core`,
`@sparticuz/chromium`, and `pngjs`, plus the verifier script
(`/opt/verify-web.js` in the dev sandbox).

## Notes / optimization
Use `-O2` (RelWithDebInfo). `-O3` is not recommended for the gl4es
objects on this target. See MILESTONES.md M3a for the full list of
emscripten-specific fixes and the open gears known-issue.

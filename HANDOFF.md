# Handoff — read this first

Continuing engineering on **xss-sdl2-gles2**: xscreensaver's screen
hacks ported to SDL2 + OpenGL ES 2.0, native (Linux/macOS/Windows) and
WebAssembly. One executable / one web page per hack, from **unmodified**
upstream hack source.

## Orient yourself

- **`MILESTONES.md`** — the authoritative running log (M0 → M12), every
  decision and fix with rationale. Read the recent entries.
- **`docs/TOOLCHAIN.md`** — how versions are pinned, and what cannot be.
- **`tests/STATUS.md`** — per-hack pass/blank/crash sheet (harness output).
- **`README.md`** — architecture + build/run for all targets.

## Current state (2026-08, commit `759bd18`)

- **278 hacks wired** — every shipping xscreensaver hack whose source
  this port can build. Both suites have run over all of them:
  - native (macOS/ANGLE): **274 pass**, `penetrate` static,
    `dymaxionmap` too slow to sample, `glitchpeg` + `vigilance` blank.
  - web (headless Chromium, 279 pages): **277 render**; same two blank.
- **CI: all five jobs green** (linux, macos, windows, web, pages).
  Windows went green for the first time in M11a.
- **Live gallery**: https://erik-larsen.github.io/xss-sdl2-gles2/ —
  **275 cards** (`gen_gallery.py` cards what `tests/STATUS.csv` marks
  `pass`/`static`).
- Toolchain is pinned in **`toolchain.versions`** (emsdk 4.0.12, node
  20.20.2, chrome 152.0.7977.54, images ubuntu-24.04 / macos-15 /
  windows-2025). Actions are SHA-pinned; `scripts/check-toolchain.py`
  enforces all of it in CI. The emsdk 3.1.6 → 4.0.12 bump is verified by
  the sweep above: no regressions, no clusters.

## Next steps

**1. Re-run the suites after any substantive change** — they are the
user's to launch, not yours (see the `no-automatic-test-runs` memory):

```sh
python3 tests/harness.py                 # -> STATUS.csv/.md + thumbnails
sh tests/sweep-web.sh build-web          # -> STATUS-web.tsv
```

Both are current as of `759bd18`, and committing their output is what
moves the gallery. There is one web build dir (`build-web`, pinned
emsdk); the sweep refuses to run against a foreign server or a stale
tree, and `verify-web.js` finds Chrome on macOS and retries a blank
sample once. All three of those guards exist because the rig produced
three separate false catastrophes in one session — see the gotcha below.

**1a. What the suites flagged, and how much is real:**

- **`dymaxionmap` — FIXED (M13e), was never slow.** Its own `-frames` option (animation frame count) swallowed the harness's `--frames N`, so the run was unbounded and the 10-minute cap looked like slowness. Driver flags are now reserved in their double-dash spelling (screenhack-sdl.c); 30 frames + shot now take 1.5 s. Same fix un-shadows anemone/rdbomb/splitflap's `-width`/`-height` count options from the driver's window geometry.
- **`vigilance` — FIXED (M13c), pending suite re-run.** A gl4es bug: fog calls compiled into a display list all replayed as `GL_FOG_COLOR`, leaving fog mode/density at defaults and fogging the scene to solid black. One-line `PATCH(xss-sdl)` in third_party/gl4es/src/gl/listdraw.c. Verified by hand native and web.
- **`peepers` — FIXED (M13d), pending suite re-run.** glBegin-recorded display lists replayed with client-side attribs while their indices sat in a real element VBO; emscripten's FULL_ES2 then sized every client-attrib upload from *client* indices it couldn't see — zero bytes, zero fragments. `PATCH(xss-sdl)` in third_party/gl4es/src/gl/listdraw.c keeps indices client-side in that case (emscripten only). Web-dim results elsewhere may improve in the next sweep for the same reason.
- **`lightning` — takes ~30 s to draw anything**, on both platforms.
  Already on the native follow-up list; nothing new.
- **`penetrate` — static** natively: renders, but two shots 23 frames
  apart match. Probably a slow attract mode; re-check with a much larger
  frame gap before treating it as a bug.
- `boxfit` and `vfeedback` were *rig* flakiness, now fixed by the blank
  retry. They render.

**2. `glitchpeg` — FIXED (M13), pending suite re-run.** It now pulls raw JPEG bytes from the port's image source (`xss_grab_image_file_bytes` in grabclient-sdl.c) and decodes corrupted JPEGs via an stb fallback in jwxyz-png.c; the popen/Xt pipe is compiled out under `XSS_SDL_PORT`. Verified by hand native (two differing full-frame glitch shots) and web (verify-web.js: nonBlank, 3995 colors). M13b also stages `assets/images/` + `index.json` into `build-web/`, so swept pages get real photos — **the next web sweep will show grab hacks switching from colour bars to photos; that is expected, not a regression.**

**3. Windows runtime bring-up (M4b).** The job builds and packages; no
binary has ever been *run*. Expect ANGLE/EGL surface issues, and note
that gl4es reaches ANGLE through `SDL_GL_GetProcAddress`, so a runtime
failure is more likely in context creation than in symbol resolution.

**4. Vendor SDL2 + libpng (the deferred "tier 3").** This is the last
real longevity gap:
- MSYS2 packages cannot be pinned (no archive), so Windows can break
  with no commit of yours.
- macOS is worse than it looks: your machine links
  `/Library/Frameworks/SDL2.framework` **2.30.3** while CI links
  `brew install sdl2` — different products, neither pinned.
Vendoring them into `third_party/` the way ANGLE already is closes both.

**5. Port the pinning recipe** to `sgi-demos` and
`emscripten-sdl2-gles2` — this was the stated reason for M12.
`docs/TOOLCHAIN.md` ends with the four files + one habit to copy.

**6. Native follow-ups** (from the M9 review, all native parity):
boxfit/halo/lightning/moire2/rocks/strange blank-or-nearly at 30s; goop
renders opaque (jwxyz lacks X11 plane-mask transparency); vermiculate
accumulates very slowly.

## Build / run quick reference

```sh
sh scripts/setup-toolchain.sh              # pins vs what is installed

# native (macOS): brew install cmake ninja libpng; SDL2 from /Library/Frameworks
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j8
ANGLE_DEFAULT_PLATFORM=metal ./build/gears --frames 90 --shot /tmp/x.ppm

# web (emsdk comes from toolchain.versions; setup-toolchain.sh checks it)
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-web --target decayscreen -j8
sh scripts/deploy-web.sh build-web         # assembles web/ + gallery
```

CLI on every hack: `--width/--height/--frames N/--shot out.ppm/--fps`,
plus the hack's own options (`-duration 5`, `-verbose`, …). On web the
same options come from the query string: `?frames=60&shot=out.ppm`.

## Conventions (don't relearn these)

- **Hack source is never edited** except vendored fixes tagged
  `PATCH(xss-sdl)`. Port hooks live in `src/port/` (fonts =
  jwxyz-font.c, text = textclient-sdl.c, image = grabclient-sdl.c).
- **Registration**: `CMakeLists.txt` for 2D, `cmake/batch{2,3}-glhacks.cmake`
  for GL — generated by `scripts/gen_glhack_deps.py` (symbol closure).
  Regenerate, don't hand-edit. `XSCREENSAVER_MODULE_2`'s *middle*
  argument is the table prefix, so plain `xss_add_glhack` is right;
  `_p` is only for b_lockglue/sproingiewrap, where the file name and the
  module name genuinely differ.
- **Embedded assets**: `scripts/png2c.py` (upstream ad2c format, verified
  byte-identical against a vendored header), `scripts/gen_ad2c.py` for
  molecules.h / m6502.h, `scripts/bin2c.py` for fonts.
- **Never edit a version outside `toolchain.versions`.**

## Gotchas that cost time (this session and before)

- **`ANGLE_DEFAULT_PLATFORM=metal` on macOS** — the vendored ANGLE
  defaults to a slow GL-on-Metal path. 50–200× faster with it.
- **A uniform result across every hack is the harness, not the hacks.**
  Three separate rig failures in one session each looked like total
  breakage: `findChrome()` knew only Linux paths (234/234 blank), an
  http.server left over from an aborted run held the port so every page
  404'd (279/279 blank), and a single early sample called slow starters
  blank (6 false blanks). Real regressions cluster by subsystem; they do
  not hit all 279 identically. Each of those now fails loudly in a
  second.
- **Web verdicts: use headless, never the in-app browser pane.** Chrome
  pauses rAF in a hidden page, so the pane freezes on a stale frame and
  "still broken" there proves nothing. Cost an hour chasing a carousel
  "bug" that was the pane being hidden.
- **jwxyz read-back keeps its alpha.** `BlackPixel == alpha_mask`
  (0xFF000000), so a read-back pixel must keep those bits or
  `XGetPixel(im,x,y) == BlackPixelOfScreen` never matches — that is how
  phosphor/apple2 capture their fonts. An earlier patch stripped alpha
  and blamed the resulting blank glyphs on those very hacks; it is
  removed (M11d). Do not reinstate it.
- **Windows: `windows.h` vs jwxyz's X11 macros.** Anything that pulls in
  windows.h *after* jwxyz.h explodes in `processthreadsapi.h`
  (`ULONG ControlMask;` → `ULONG (1<<2);`). Bit twice: gl4es's `gl.h`
  (M11a) and a `<winsock2.h>` include (M11). Declare the call you need
  instead.
- **gl4es name mangling has two halves.** On Apple/emscripten/_WIN32 the
  header mangles callers to `gl4es_gl*` and the alias exports are off;
  elsewhere it is the reverse. Flip one without the other and you get
  duplicate symbols (or undefined ones) — see `attributes.h` + `gl.h`.
- **`glIsEnabled` during display-list compile** returned GL_FALSE in
  gl4es. sphere.c/tube.c gate texture coordinates on it, so glplanet's
  globe came out white. Fixed in M11b; the general lesson is that
  queries must execute during compilation, never be deferred.
- **emscripten `INITIAL_MEMORY` is a link-time floor**, not just a
  runtime hint: the earth.c hacks need 32MB before `ALLOW_MEMORY_GROWTH`
  can do anything.
- **Toolchains differ between your machine and CI in ways that hide
  bugs.** Local emsdk 4.0.12 masked the INITIAL_MEMORY failure; local
  clang 16 masked the `countries.c` initializer failure that clang 15
  (macos-14, and emsdk 3.1.6) rejects. `scripts/setup-toolchain.sh`
  exists to make that visible.
- **Web GL context has alpha:8 + pre-swap alpha clamp** (M8a) — needed
  for no-clear trails hacks; don't undo `clamp_alpha()`.
- **No-clear hacks** (M8): flurry gets a driver-level retained
  framebuffer keyed by progclass in glx-sdl.c. New hacks that composite
  over the previous frame go in that list.
- **gl4es on web needs the pre-swap flush + FULL_ES2 desync patches**
  (M9b/M9c). Keep the `PATCH(xss-sdl)` blocks in third_party/gl4es
  (fpe.c, buffers.c) and the `-sGL_MAX_TEMP_BUFFER_SIZE` /
  `-sSTACK_SIZE` link flags.
- **The driver's own GL calls bypass gl4es** (it includes
  `SDL_opengles2.h`, so plain `gl*` binds to ANGLE/WebGL). Anything the
  driver does behind gl4es's back desyncs its state cache — that is why
  the blit texture and VBO are skipped entirely for GL hacks (M11b).
- **zsh word-splitting**: use `${=VAR}`; macOS has no `timeout(1)`.

## Excluded by design (don't "fix" these)

`mapscroller` and `webcollage` fetch over the network through a helper
process — no subprocess, no network, and no `fork()` on the web.

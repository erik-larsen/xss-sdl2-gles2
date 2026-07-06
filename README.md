# xss-sdl — xscreensaver → SDL2 + OpenGL ES 2.0

Port of [xscreensaver](https://github.com/Zygo/xscreensaver) hacks to
SDL2 + GLES2, building natively for Linux/macOS/Windows and for the web
via Emscripten. One executable (or web page) per hack.

## Architecture

```
            hack .c (unmodified xscreensaver source)
        ┌─────────┴─────────┐
   2D hacks              GL hacks
        │                    │
  jwxyz-image          GL 1.x calls ──► gl4es ─► GLES2
   (CPU RGBA)          GLU calls ──► glues (tess/quadrics/mipmap) ─► gl4es
        │
        ▼
 CPU framebuffer ─► GLES2 textured-quad blit
        └─────────┬─────────┘
   SDL2 driver: single GLES-only context path (sgi-demos pattern)
   emscripten ifdefs: main loop, asset preload, title quirk only
```

## Status (M1a complete)

- [x] SDL2 driver, GLES2-only context (sgi-demos `sdl_framebuffer.c` pattern)
- [x] CPU framebuffer → GLES2 quad blit (NPOT, y-flip, RGBA)
- [x] `tick()`-shaped main loop shared by native + emscripten builds
- [x] Harness primitives: `--frames N`, `--shot out.ppm`, `--fps`
- [x] `testpattern` hack validates orientation/channels/animation headlessly
- [x] M1b: jwxyz-image adapter; real 2D hacks running unmodified:
      deco, greynetic, attraction, interference, moire
      (screenshots in docs/screenshots/, smoke suite in tests/smoke.sh)
- [x] M2a: GL pipeline live (gl4es + glues); gears rendering
- [x] M2b: hypertorus, glmatrix, lament, polyhedra verified; bouncingcow known-issue (gl4es dlist+interleaved-arrays, black render)
- [ ] M3: emscripten builds + per-hack pages
- [ ] M4: Windows/macOS builds + CI
- [ ] M5: full hack rollout + automated harness
- [ ] M6: problem hacks, text/image/font assets

## Porting notes (M1b findings)

- **Platform identity**: the tree's portability gate is
  `HAVE_COCOA || HAVE_ANDROID` -> jwxyz, so this port defines
  `HAVE_ANDROID` (like the Android build) and compiles zero JNI files.
  Introducing a first-class `HAVE_SDL` gate is future cleanup.
- **Symbol hiding is mandatory on Linux**: jwxyz defines real-Xlib
  names; SDL2 links real libX11. Without `-fvisibility=hidden`, SDL's
  own Xlib calls bind to jwxyz and crash (`CMAKE_C_VISIBILITY_PRESET`).
- **Vendored patches** (both in `jwxyz/jwxyz-image.c`, marked
  `PATCH(xss-sdl)`): upstream stubs `draw_arc` and `FillPolygon` in
  image mode; we delegate to raster implementations in
  `src/port/jwxyz-arcs.c` (tessellate -> scanline spans via
  XFillRectangle, outlines via XDrawLines). This unblocked
  `attraction` and every other XFillArc-based hack.
- Fonts are metric-sane stubs until M6; hacks that draw text will run
  but render no glyphs.

## Build & run (Linux)

```sh
apt install cmake libsdl2-dev libgles2-mesa-dev
cmake -B build && cmake --build build
./build/testpattern                  # interactive (Esc/q quits)
```

Headless smoke test (CI-style):

```sh
apt install xvfb
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./build/testpattern --frames 60 --shot shot.ppm --fps
```

Expected `shot.ppm`: SMPTE-ish color bars (top), grayscale ramp (middle),
animated plasma (bottom), 1px red border, green 20px square in the
top-left corner, white crosshair at center. See `docs/testpattern.png`.

## Driver CLI (all hacks)

```
--width N --height N    window size (default 800x600)
--frames N              exit after N frames
--shot PATH.ppm         write final rendered frame (GPU readback)
--fps                   print fps once per second
```

## Layout

```
src/driver/    SDL2/GLES2 driver (xss_driver.[ch])
src/hacks/     per-hack entry points
docs/          reference material, verification screenshots
MILESTONES.md  plan + per-milestone status
```

- [x] M3a: emscripten build; testpattern + deco verified in headless Chromium; gears known-issue (intermittent blank via gl4es-on-WebGL)

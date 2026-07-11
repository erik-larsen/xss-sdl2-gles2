# Milestones

Each milestone has a/b mid-point check-ins. Every check-in delivers a
downloadable repo snapshot + evidence (screenshots/logs) + status update.

## M1 — SDL platform layer + 2D pipeline
- **M1a ✅ (delivered)** Repo scaffold; CMake (emscripten-aware); SDL2
  driver requesting a GLES-only context (sgi-demos pattern, one
  unconditional path); CPU-framebuffer→GLES2 blit; tick()-shaped main
  loop; `--frames/--shot/--fps` harness primitives; `testpattern` hack
  verified headlessly (Xvfb + Mesa llvmpipe ES context): correct
  orientation, channel order, extents; frames differ over time; ~30fps
  at the requested 33ms delay.
- **M1b ✅ (delivered)** Vendored xscreensaver tree; jwxyz-image adapter implementing the
  driver API; resource/options layer (Xrm replacement backed by argv);
  RNG/usleep/colors utils compiled; `deco`, `greynetic`, `attraction`,
  `interference`, `moire` running with screenshots.
  Done: all five pass tests/smoke.sh (clean exit + non-blank).
  Fixed en route: libX11 symbol collision (hidden visibility);
  image-mode arc/polygon support (new jwxyz-arcs.c + 2 delegation
  patches in vendored jwxyz-image.c). Deferred to M5: SDL event ->
  XEvent forwarding (hack event_cb is not yet called).

## M2 — GL pipeline
- **M2a ✅ (delivered)** gl4es vendored (STATICLIB/NOX11/NOEGL), proc loading via
  `set_getprocaddress(SDL_GL_GetProcAddress)` + `initialize_gl4es()`;
  glues vendored (quad/tess/mipmap/project; libnurbs excluded);
  `gears` running.
  Done: gl4es builds in-tree (STATICLIB/NOX11/NOEGL/NO_LOADER/
  NO_INIT_CONSTRUCTOR, pre-generated version.h); glues builds minus
  libnurbs with an XSS_GLUES_GL4ES header-ladder patch; glshim/ maps
  screenhackI.h's <GLES/gl.h> (HAVE_ANDROID branch) onto gl4es GL +
  glues GLU; driver gained gl_p mode (swap, no blit; 24-bit depth) and
  a weak xss_driver_gl_ready() hook -- gl4es interposes GL symbols in
  GL binaries, so it must initialize before the driver's first GL call.
  glx-sdl.c provides init_GL/GLX shims mirroring screenhack-android.c.
  gears: lit/depth-tested render verified headlessly through Mesa
  llvmpipe GLES2.
- **M2b ✅ (delivered)** `hypertorus`, `bouncingcow`, `glmatrix`, `lament` (texture
  tricks), `polyhedra` (gluTess + glMap2 teapot via gl4es eval.c).
  Done: hypertorus, glmatrix, lament, polyhedra render correctly
  (smoke suite + visual verification); libpng-backed
  jwxyz_png_to_ximage (src/port/jwxyz-png.c) replaces Android's JNI
  decoder; all 65 hacks/images headers generated via utils/bin2c;
  STANDALONE & friends moved to compiler command line (hacks test them
  before any #include); HAVE_GLSL defined (the only Android-tested
  combination) with a PATCH(xss-sdl) kill-switch in glsl-utils.c --
  XSS_DISABLE_GLSL defaults on, so GLSL-capable hacks use their
  fixed-function fallbacks through gl4es (the port's target config);
  font stub now returns zero metrics per jwxyz_draw_string's contract.
  KNOWN ISSUE (open): bouncingcow renders black in filled mode;
  -wireframe proves geometry OK; cow models are GL_N3F_V3F
  glInterleavedArrays+glDrawArrays recorded into display lists, a path
  gears doesn't hit -- suspected gl4es dlist capture losing normals.
  Next step: minimal repro outside a dlist; candidate upstream gl4es
  issue. Tracked as run-only in tests/smoke.sh.

## M3 — Emscripten
- **M3a ✅ (delivered)** Web build of driver + 2 2D hacks; per-hack HTML; asset preload.
- **M3b ✅ (delivered)** Custom full-window web shell; single-threaded; GH-Pages hostable.
- **M3c ✅ (delivered)** GL hacks on web FIXED. The gl4es-on-WebGL
  client-array blank (all GL hacks opaque-black) was root-caused to a
  gl4es bindBuffer cache desync vs. emscripten's FULL_ES2 temp buffer,
  and fixed with PATCH(xss-sdl) in third_party/gl4es/src/gl/fpe.c
  realize_glenv() (force a real ARRAY_BUFFER unbind before a client-side
  attrib). Verified rendering + animating in headless Chromium:
  gears, hypertorus, polyhedra, lament, glmatrix, bouncingcow. 2D hacks
  regression-clean; native path untouched (__EMSCRIPTEN__-gated).
  NOTE: bouncingcow now renders correctly *on web* (normals intact);
  the separate NATIVE display-list-normals issue (M2b) is untouched and
  unverified this session. Generated static index page: still TODO.

## M4 — Native packaging + CI
- **M4a (in progress)** GitHub Actions CI: `.github/workflows/ci.yml`
  with two active jobs verified to mirror sandbox-proven commands:
  `linux` (ubuntu-24.04: apt deps incl. libgles-dev + libgl1-mesa-dri,
  cmake/Ninja build, `tests/smoke.sh` under xvfb+llvmpipe, upload native
  binaries) and `web` (setup-emsdk pinned 3.1.6, emcmake build,
  setup-chrome, `tests/smoke-web.sh`, upload .html/.js/.wasm). The web
  verifier is now committed (tests/verify-web.js + tests/package.json)
  instead of sandbox-local /opt; smoke-web.sh defaults to it. Both smoke
  suites pass locally; runner-green pending first push.
  NOTE: Windows was originally slated first, but it's the part that can't
  be validated in the Linux sandbox, so it moved to M4b to avoid shipping
  unvalidated guesses about ANGLE/SDL2 wiring.
- **M4b (done, runner-pending)** Windows (MSYS2 CLANG64) + macOS CI jobs
  added to ci.yml; native GLES2 on those platforms comes from vendored
  ANGLE (libGLESv2 + libEGL), modeled on sgi-demos. CMake gained a
  per-platform GLES backend block (Linux unchanged = system mesa;
  WIN32/APPLE = vendored ANGLE under third_party/angle, with DLL-copy on
  Windows and rpath on macOS). third_party/angle/ is scaffolded with a
  README on populating it (copy from Chrome; CLANG64 needs GNU import
  libs via gendef/dlltool, not MSVC .lib). The Linux build+smoke is
  verified unchanged by the refactor. Win/macOS jobs build + package
  per-platform artifacts and soft-skip (with a warning) until ANGLE is
  vendored; headless GLES smoke on those runners is deferred (no
  llvmpipe/xvfb equivalent) and runner-green is pending E vendoring the
  ANGLE binaries + the first push.

- **M4c ✅ (first real macOS-native run)** Done on an arm64 MacBook (Apple
  M4, macOS 15), outside the Linux sandbox for the first time. ANGLE
  vendored into third_party/angle: universal (x86_64+arm64) libEGL/
  libGLESv2 dylibs (from the sgi-demos/rss-port copies), install names
  rewritten to @rpath + ad-hoc re-signed, GLES2/EGL/KHR headers added;
  lib-win got the DLLs (GNU .dll.a import libs still pending -- needs
  gendef/dlltool on MSYS2). Build fixes:
  a. gl4es AliasDecl uses __attribute__((alias)), unsupported by darwin
     ld -- the two uses in directstate.c are now explicit forwarding
     wrappers under __APPLE__ [PATCH(xss-sdl)].
  b. Mach-O ld rejects ELF-style undefined weak *declarations*;
     screenhack-sdl.c's xss_gl_init_once/xss_gl_viewport weak decls are
     weak no-op *definitions* on __APPLE__ (strong overrides in
     glx-sdl.c win in GL binaries).
  Runtime fixes (all in project code, not vendored):
  c. **GL hacks rendered opaque black on ANGLE/Metal -- the native twin
     of the M3c web bug, root-caused to the driver itself:** init_blit()
     left its 64-byte fullscreen-quad VBO bound to GL_ARRAY_BUFFER via
     raw GLES2 calls that gl4es's binding cache never sees. gl4es then
     submitted client-side vertex pointers that strict-offset drivers
     (ANGLE, WebGL) misread as offsets into that stale 64-byte buffer ->
     zero fragments. Mesa honors the client pointer anyway, which is why
     Linux never caught it. Fix: the driver unbinds ARRAY_BUFFER after
     init_blit() and blit_surface(). (This is almost certainly the same
     64-byte buffer the M3c fpe.c patch observed on web; that patch
     stays as belt-and-suspenders against emscripten's own FULL_ES2
     temp-buffer binds.)
  d. --shot now reads back the final frame BEFORE SDL_GL_SwapWindow:
     post-swap back-buffer contents are undefined (llvmpipe preserves
     them; ANGLE/Metal returns a fresh black drawable). Also makes
     --shot viable on web, where the post-loop path never runs.
  e. Driver gained weak xss_gl_pre_swap/xss_gl_post_swap hooks around
     the swap; glx-sdl.c strongly binds them to gl4es_pre_swap/
     gl4es_post_swap (flush batched geometry / rebind internal FBO).
     Native-only for now; web verified without them and stays untouched.
  tests/smoke.sh is platform-aware (Darwin: no xvfb/timeout, real
  windows over ANGLE/Metal). Result: all 11 smoke targets PASS with
  visual verification (contact sheet), and **bouncingcow renders
  correctly** -- the M2b "black cow" was evidently the same binding
  desync (cow models are client-side glInterleavedArrays), not a gl4es
  dlist-normals bug. UPDATE from CI run #2: on Linux/llvmpipe the cow is
  **still black** (smoke.sh now reports its color count), so the Mesa
  symptom is a distinct bug after all -- black on llvmpipe, correct on
  ANGLE/Metal and WebGL. Still run-only in smoke.sh; M2b's dlist/
  glInterleavedArrays suspicion stands for Mesa only.

- **M4d (CI findings, run #2)** CI run #1 was green on all four jobs
  (windows soft-skipped pre-ANGLE-importlibs). Run #2 (in-job .dll.a
  generation enabled): the import-lib step worked, but the first real
  Windows configure failed fast -- gl4es names its CMake target
  OPENGL32 on Windows (not GL); the top-level CMakeLists now selects
  via GL4ES_TARGET. Linux smoke flaked FAIL(blank) on polyhedra: it
  fades to black between shapes and animation is wall-clock driven, so
  a fixed --frames shot can land in a fade gap on a slow runner;
  smoke.sh's run() now retries once at a different frame count before
  declaring blank.

## M5 — Full rollout (~250 hacks)
- Batch conversion of remaining hacks; automated harness: run N frames
  headless, assert exit 0 + non-blank + frame-to-frame change; status
  spreadsheet (pass / renders-wrong / crashes / excluded), seeded with
  XScreenSaverWin's STATUS.txt as prior-failure hints.
- Check-ins per batch (a/b/c...).

- **M5a ✅ (macOS): harness + 105 new 2D hacks. 112/116 targets pass.**
  Inventory: 144 XSCREENSAVER_MODULE 2D sources classified by dependency
  (grep-based; the upstream hacks/Makefile.in is not vendored). Batch 1 =
  the 105 with no textclient/analogtv/grabclient/image-set deps: 67
  plain screenhack-API + 38 xlockmore-API (the latter build with
  hacks/xlockmore.c, no USE_GL). droste + slip turned out to be grab
  hacks (load_image_async) -- deferred with that class.
  tests/harness.py: runs each built hack twice (different --frames),
  classifies pass/static/blank/crash into tests/STATUS.{csv,md}; rows
  accumulate across batches/platforms. Per-hack frame overrides: SLOW
  (multi-second frame delays: deco, epicycle, ...) and LONG (slow
  starters: pyro's empty sky, halo's gradual accumulation).
  Port bugs found & fixed (all PATCH(xss-sdl) or project files):
  a. **fill_rects clipping (jwxyz-image.c): rects entirely outside the
     frame** (hacks draw objects that drift off-screen; a real X server
     clips) left y1 < y0, and the unsigned size math wrapped to ~4G
     rows of wmemset -> SIGSEGV. One guard fixed 15 of 19 crashing
     hacks (galaxy, grav, bouboule, halftone, ...).
  b. **GXxor/GXor raster ops implemented** in fill_rects + DrawPoints
     (jwxyz-image.c asserted GXcopy-only): fixes munch, crystal
     (GXxor), bouboule (GXor).
  c. **jwxyz_abort recursion** (jwxyz-sdl.c): jwxyz.h #defines abort()
     to jwxyz_abort, so its own abort() call recursed until stack
     overflow, masking every assert message. #undef'd; asserts now
     print before dying.
  Remaining 4 (recorded in STATUS): juggle crashes + xjack/barcode
  blank -- all blocked on the M6 font layer (zero-metric stubs; barcode
  sizes its layout from font extents); lcdscrub blank -- uninvestigated
  (LCD burn-in utility, low priority).
  Verified: full harness green locally (112 pass), 24-hack sample
  contact sheet visually checked. Linux/web/Windows status of the new
  hacks pends CI (smoke.sh still covers the original 11; wiring the
  harness into CI is the natural next step).

- **M5b ✅ (macOS): 86 new GL hacks. Sheet now 195/202 pass.**
  Inventory: 138 glx modules; 34 deferred up front (textclient/analogtv/
  image deps). Dependencies are NOT guessable per-hack (each needs a
  specific support-source set), and upstream's Makefile.in isn't
  vendored -- so scripts/gen_glhack_deps.py compiles every glx source
  with the real build flags, nm's the objects, and computes each hack's
  symbol closure; it emits cmake/batch2-glhacks.cmake (checked in,
  regenerate rather than hand-edit). Multi-file hacks (flurry,
  stonerview, sonar, endgame chessmodels, bubble3d...) resolve
  automatically.
  Build fixes: utils/easing.c added to xss_port (14 hacks reference
  ease(); it was never compiled); xss_add_glhack_p() for hacks whose
  XSCREENSAVER_MODULE name differs from the file name (b_lockglue ->
  bubble3d, sproingiewrap -> sproingies). 10 more image-grab hacks
  (load_image_async at link: carousel, esper, photopile, ...) + 
  xshadertoy (android_read_asset_file) + molecule (molecules.h PDB data
  not vendored) recorded BLOCKED in the generated cmake.
  Harness: SLOW entries for the seconds-per-frame GL hacks (endgame/
  queens chessmodels, nakagin, cubestorm -- gl4es perf pathology worth
  a future pass); LONG for slow starters (energystream, chompytower).
  Result: 78 -> 84/86 after triage; 24-hack sample sheet visually
  verified (geodesicgears, headroom, kallisti, endgame all correct).
  Open (STATUS.csv): unicrud crash "no characters found" -> M6 fonts
  (joins juggle/xjack/barcode); quasicrystal renders black (investigate);
  lcdscrub blank (low priority).
  queens RESOLVED (not a render bug): it fades in from black over its
  first ~128 frames (light CONSTANT_ATTENUATION = 0.8/(0.01+findAlpha),
  findAlpha = steps/128 ramping from 0), and chessmodels is slow through
  gl4es, so a shot early enough to finish lands in the dark phase while
  a shot late enough to be bright hits the harness timeout. Renders
  correctly past frame ~128; treat as SLOW-fade, not blank.

- **M5c ✅ (web): all 203 targets built for wasm; thumbnail gallery.**
  Full build-web with local emcc 4.0.12 (vs CI's pinned 3.1.6): zero
  failures, and spot-verified rendering in headless Chrome (testpattern,
  deco, gears, substrate, hextrail, glmatrix, flurry) -- no
  newer-emsdk gl4es loader issue on this project (the host calls
  eglGetProcAddress + initialize_gl4es explicitly).
  Gallery (rss-sdl2-gles2 pattern): scripts/gen_gallery.py converts the
  harness screenshots (/tmp/xss_harness) into web/shots/*.jpg thumbs and
  emits web/index.html -- card grid, 2D/GL filter, cards dim to "native
  only" when a hack's wasm build isn't deployed. scripts/deploy-web.sh
  assembles web/<hack>/ from build-web/. web/*/ (875MB of wasm) is
  gitignored; web/shots/ + index.html are committed so Pages can serve
  the gallery. Verified: gallery renders, 4 hacks spot-checked through
  their deployed card links.

- **M5d ✅ (CI): gallery live on GitHub Pages.**
  ci.yml: the web job assembles the gallery (deploy-web.sh) and uploads
  a Pages artifact on main pushes; a new pages job (deploy-pages@v4,
  needs: web) publishes it. Live at
  https://erik-larsen.github.io/xss-sdl2-gles2/ -- index, per-hack
  .html/.js/.wasm, and thumbnails all serve 200. The overall run still
  shows red from the tabled Windows job; web + pages are green.
  Note: the published site is ~875MB (near Pages' 1GB soft cap); M6
  asset bundles will likely need wasm size trimming before it grows.

## M6 — Problem hacks & assets
- textclient-backed hacks (phosphor, apple2, starwars, fontglide) →
  bundled text files; image hacks → bundled image set via grabclient;
  sonar → simulation mode; webcollage → excluded; font layer
  (FreeType/stb_truetype against bundled fonts).

- **M6a ✅ (macOS): real fonts. Sheet 198/202 pass, 0 crashes.**
  Text was invisible (jwxyz font stubs returned zero metrics). Now
  src/port/jwxyz-font.c rasterizes one embedded font with stb_truetype
  (third_party/stb_truetype.h, public domain). Font = Liberation Sans
  (Helvetica-metric, SIL OFL), subset with pyftsubset to Latin/Greek/
  Cyrillic/punct/currency/arrows/symbols (~1100 glyphs, 410KB -> 97KB)
  so it doesn't blow the web size budget in ~200 binaries; embedded as a
  C array at build time (scripts/bin2c.py, CMake custom command) so only
  the .ttf is committed. jwxyz_render_text implements the jwxyz-common
  contract: XCharStruct metrics + a (rbearing-lbearing)x(ascent+descent)
  32bpp white-coverage bitmap that draw_string tints with the GC fg and
  alpha-composites. All families map to the one face for now (mono
  later). See third_party/font/README.md.
  Fixes (fonts + two latent jwxyz gaps, PATCH(xss-sdl)):
  a. **xjack, unicrud** now render text (unicrud was CRASHING "no
     characters found" -- the stub font had zero glyphs).
  b. **juggle** (was crashing): it draws its pattern label on the bare
     MI_GC without XSetFont; jwxyz GCs have no default font (X servers
     do), so &((Font)0)->metrics -> 0x28 deref. jwxyz-common.c now
     lazily installs a default font in jwxyz_draw_string / XDrawImageString.
  c. **barcode** (was blank): it draws into 1-bit pixmaps and XPutImages
     them; jwxyz-image.c's depth==1 XPutImage path was an unimplemented
     Log() stub. Implemented: expand each bit to GC fg/bg with bounds
     clipping.
  Regression: full 202-hack harness re-run, 0 real regressions (glmatrix/
  halo/lightning flagged pass->blank were animation-phase flakiness --
  glmatrix rain fills in ~200 frames now in LONG; lightning flashes
  intermittently). Binary size +~117KB/target (gears 1.37->1.49MB).
  Remaining 4 blank: quasicrystal (open black-render -- see M6d for the
  dead-end), queens (slow fade-in, renders past ~frame 128), lightning
  (intermittent), lcdscrub (burn-in utility, low priority).
  Web: all 203 targets rebuilt with the subset font (0 failures); gallery
  921MB (subset kept it under the 1GB Pages cap; full font would be
  ~960MB). Font verified in wasm too -- xjack and unicrud render text in
  headless Chrome. Pages redeploy happens on the next push to main.

- **M6b ✅ (macOS): monospace face + 7 textclient hacks. Sheet 205/209.**
  Monospace: added a second embedded face (subset Liberation Mono, 91KB),
  selected in jwxyz-font.c when the caller sets JWXYZ_STYLE_MONOSPACE or
  the family name matches courier/mono/fixed/console/terminal. xjack now
  renders monospaced (it asks for Courier/monospace via the Xft layer).
  Textclient: the terminal/text hacks stream chars from textclient_getc.
  Upstream Unix pipes a subprocess; we reuse utils/textclient-mobile.c
  (its column wrapping + streaming) and supply its two platform hooks in
  src/port/textclient-sdl.c -- no subprocess, no network, so it works in
  the browser too. The default "date" textMode returns the current date
  plus a rotating public-domain passage (Moby Dick, Tale of Two Cities,
  Frost, pangrams, Hitchhiker's), so hacks show flowing prose out of the
  box. 7 new hacks wired + verified rendering: xmatrix, fontglide (2D),
  fliptext, gltext (3D extruded text -- gorgeous), starwars (opening
  crawl), splitflap (split-flap board), winduprobot. GL ones get text
  through texfont, which builds a GL font texture via jwxyz_render_text.
  DEFERRED: phosphor + apple2 need depth-1 XYPixmap XGetImage (their
  font/TV capture) -- a jwxyz-image gap; noseguy needs image assets (M6
  images).
  Web: all targets rebuilt (0 failures); gltext/starwars/xmatrix verified
  rendering in headless Chrome (font + textclient + texfont GL path all
  work in wasm). Gallery regenerated -> 205 cards (gen_gallery.py now
  also reads the XSS_2D_TEXTCLIENT list). Gallery 978MB -- still under
  the 1GB Pages cap but tightening (mono face + 7 hacks added ~57MB);
  image assets (noseguy et al) will need the two faces trimmed or shared
  before they fit. Pages redeploy on next push to main.

- **M6c (partial): phosphor/apple2 groundwork; both still DEFERRED.**
  Two jwxyz-image.c GetSubImage fixes landed (PATCH(xss-sdl)), both
  regression-verified across all 209 hacks (0 regressions):
  a. **depth-1 XGetImage**: read a depth-1 drawable back into a 1-bit
     image (the port stores all drawables 32bpp; pack "pixel != 0").
     Needed by phosphor/apple2 font capture (XGetImage(mono, XYPixmap));
     previously aborted "XGetSubImage: bad depth".
  b. **readback alpha-strip**: a 32bpp readback now clears the internal
     opaque-alpha (BlackPixel == alpha_mask 0xFF000000, kept so black
     composites opaquely). Without it a white-on-black readback +
     `pixel ? 1 : 0` threshold (phosphor/apple2 font capture) makes every
     glyph a solid block. Harmless to GXcopy re-blits.
  With both, phosphor no longer crashes and captures glyph shapes (not
  blocks), but its multi-stage glyph->screen blit still shows only the
  cursor -- so phosphor stays DEFERRED (unregistered; would otherwise
  ship an empty terminal). apple2 also needs its apple2-main.c module
  wiring + analogtv NTSC sim verified. The crash/solid-block/readback
  issues are solved; what remains is phosphor's glyph-blit and apple2's
  TV path -- a focused session. Sheet 206/209 pass (lightning is
  intermittent; flipped back to pass this run).

- **M6d (investigation, no code change): quasicrystal black-render is
  NOT the 1D texture.** quasicrystal draws a GL_TEXTURE_1D colormap onto
  blended quads. Ruled out the obvious cause: converting it to a real 2D
  Nx1 texture (glTexImage2D + GL_TEXTURE_2D throughout, the microcosm
  approach) STILL renders black. Findings:
  - Geometry is fine: forcing untextured opaque quads fills the screen.
  - gl4es reports no GL/shader/texture errors; NPOT is supported.
  - With texturing on it goes fully black; GL_MODULATE + the sampled
    colormap yields zero output, so the texture samples as transparent
    black (unsampled) rather than the gradient. Forcing GL_REPLACE gives
    a uniform gray, not the expected stripes -- so the immediate-mode
    texcoord/texenv path, not 1D vs 2D, is the problem.
  - It also uses desktop-only GL_COLOR_LOGIC_OP / glLogicOp (its contrast
    pass), which GLES2/gl4es lacks entirely -- another gap in the same
    hack.
  Conclusion: a multi-feature desktop-GL hack that needs real gl4es
  immediate-mode-texturing + logic-op work, not a quick fix. All
  experiments reverted; stays a documented known-blank. Don't re-try the
  1D->2D angle.

- **M6e ✅ (web size): strip DWARF -> gallery 978MB -> 232MB (-76%).**
  Dissecting a .wasm showed the RelWithDebInfo `-g` DWARF sections
  (.debug_info/line/loc/str/...) were ~77% of every binary (gears: 4.5MB
  debug vs 1.0MB code). The gallery ships no source maps, so debug info
  is dead weight. CMakeLists now sets CMAKE_C/CXX_FLAGS_RELWITHDEBINFO to
  "-O2 -g0 -DNDEBUG" for EMSCRIPTEN only (keeps the required -O2 codegen
  -- -O3 miscompiles gl4es; native keeps -g for lldb). Set before the
  gl4es subdir so it strips there too; the -g0 at link is what actually
  drops the DWARF. gears 5.88->1.20MB, deco 3.26->0.67MB, largest wasm
  7.6->2.2MB. 0 build failures; gears/lament/gltext/deco/substrate
  re-verified rendering in headless Chrome. Comfortable headroom under
  the 1GB Pages cap now (unblocks the image-grab hacks). Further size
  levers if ever needed: a shared gl4es runtime (MAIN/SIDE_MODULE; the
  remaining ~1MB/wasm is a full gl4es copy per binary) -- real work,
  deferred; -Oz risks gl4es miscompiles, not pursued.
  CI follow-up: the first CI run of this change failed at the web smoke
  step -- bouncingcow (M2b: black on software renderers, fine on ANGLE)
  came up black on CI's SwiftShader and hard-blocked the Pages deploy.
  NOT a -g0 regression (renders locally over ANGLE, both -g0). Fixed by
  giving tests/smoke-web.sh a check_runonly for bouncingcow, matching the
  native smoke.sh -- a known-issue hack no longer blocks the deploy.

- **M6f ✅ (macOS): 24 image-grab hacks via colour-bars grabclient.
  Sheet 230/233 pass, 0 crashes.**
  The grab hacks ask grabclient for a screenshot/photo to manipulate;
  upstream X11 forks xscreensaver-getimage (impossible on web). Same
  recipe as textclient (M6b): reuse the mobile path -- utils/grabclient.c
  under HAVE_ANDROID calls one platform hook, jwxyz_draw_random_image();
  src/port/grabclient-sdl.c supplies it, returning NULL so grabclient
  falls back to its standard SMPTE colour bars (drawn by
  utils/colorbars.c, logo params take 0 -- zero image assets, works in
  the browser). A bundled photo can replace the NULL later for nicer
  output.
  Wired: 14 2D (blitspin boxfit bumps decayscreen distort droste ripples
  rotzoomer slidescreen slip spotlight tessellimage twang zoom; slip
  needs xlockmore.c, tessellimage needs delaunay.c, blitspin uses its
  vendored som_png.h) + 10 GL (antspotlight
  carousel esper flipflop flipscreen3d gflux glslideshow jigsaw
  mirrorblob photopile, deps via gen_glhack_deps.py; utils/xftwrap.c
  added to xss_port for carousel/photopile's xft_word_wrap). All 24
  verified rendering, native + 5 spot-checked in wasm (headless Chrome)
  -- contact sheets show each hack manipulating the bars (droste
  recursion, jigsaw pieces, gflux flag, mirrorblob, photopile
  polaroids...).
  Harness: glslideshow (300) + jigsaw (150) added to LONG (slow fade-in);
  jigsaw is also seconds-per-frame slow (chessmodels class) and needs
  --timeout 300 to capture. gen_gallery.py reads XSS_2D_GRAB. Gallery
  now 230 cards / 257MB (still ~1/4 of the Pages cap post-M6e).
  Note: the embedded logo images (images/gen/*_png.h) are all vendored
  (bsod's 15 OS logos included), but bsod #includes apple2.h (its
  Apple ][ mode) so it stays deferred with apple2/xanalogtv on the
  analogtv work.


## M3a (emscripten) -- delivered

First WebAssembly build of the xscreensaver codebase (no prior art
exists). testpattern and deco verified rendering + animating in
headless Chromium (SwiftShader WebGL) via tests/smoke-web.sh; gears
proves the gl4es->WebGL path compiles and initializes, with a tracked
known issue (below).

Toolchain (sandbox): emscripten 3.1.6 (Debian deb, node-acorn dep
dropped; clang/lld/llvm-15 + binaryen; acorn via npm); writable cache
clone (Debian freezes it); SDL2/zlib/libpng ports planted pre-unpacked
with URL markers (the googleapis fetch is blocked here) and hashes
realigned to GitHub-sourced copies; -sMINIFY_HTML=0 (Debian emcc's
minifier passes a nested list to subprocess and crashes on 3.12).

Hard-won fixes (all genuine port bugs, not sandbox artifacts):
 - main(argc,argv) silently never runs: clang-15 emits it as the
   wasm-ABI symbol __main_argc_argv, which emscripten 3.1.6's JS glue
   doesn't call. Fix: main(void) on emscripten + xss_web_args(), which
   builds argv from the page query string (?frames=60&shot=...).
   XSS_DEFINE_MAIN macro in xss_driver.h; hack_main.c.in mirrors it.
 - SDL_MAIN_HANDLED + SDL_SetMainReady() (SDL renames main otherwise).
 - -fvisibility=hidden is native-only now: on wasm it would hide main
   from wasm-ld's --export-if-defined=main.
 - init_cb signature trap: the table slot is void*(Display*,Window),
   but xlockmore installs a 3-arg init cast down to it. Native ABIs
   forgive the mismatched indirect call; wasm traps ("function
   signature mismatch"). Fix: dispatch on setup_arg -- xlockmore hacks
   (setup_arg != 0) call through the 3-arg type, plain modules the
   2-arg type. Same applies generally to GL-hack init.
 - reshape must be driven once at init: native gets the initial
   projection from X11's ConfigureNotify, but SDL/emscripten get no
   such event, so GL hacks built an identity projection -> nothing
   visible. (This also fixed a latent native-SDL bug.)
 - gl4es proc loader: eglGetProcAddress (emscripten's own resolver),
   not SDL_GL_GetProcAddress (whose wasm thunks have wrong signatures
   and trap).
 - gl4es archive output pinned per-build (ARCHIVE_OUTPUT_DIRECTORY):
   its CMakeLists hardcodes its source lib/, so native + web builds
   clobbered each other's libGL.a.
 - WebGL canvas alpha: GL hacks leave the default-buffer alpha at 0;
   an alpha:true canvas then composites them as transparent. Fixed by
   requesting SDL_GL_ALPHA_SIZE 0 (alpha:false context).

Verification rig: puppeteer-core + @sparticuz/chromium (bundled
SwiftShader), pngjs. Samples the canvas via compositor screenshots
(gl.readPixels returns zeros once a frame composites with
preserveDrawingBuffer:false). tests/smoke-web.sh wraps it.

KNOWN ISSUE (open, ROOT-CAUSED): GL hacks render blank on the web
(gears: opaque-black canvas, no GL error, shaders link, ~103k
glDrawArrays/frame all executing). Root cause found by tracing WebGL
calls in headless Chromium: gl4es submits geometry by calling
glVertexAttribPointer with raw wasm heap addresses as the byte offset
(e.g. 7449432) while a 64-byte helper buffer stays bound to
GL_ARRAY_BUFFER. Native GLES honors that as a client pointer; WebGL
forbids client-side arrays and instead reads the address as an
out-of-range offset into the 64-byte buffer, yielding no fragments.
emscripten's FULL_ES2 client-array emulation would handle this, but
only fires when NO buffer is bound -- gl4es's bound helper buffer
defeats it. Confirmed NOT fixable via LIBGL_USEVBO (0/1/2) or
LIBGL_ES=2. The fix belongs in gl4es's emscripten vertex path: either
upload real vertex data into the bound VBO, or unbind (glBindBuffer 0)
before client-pointer draws so emscripten emulation engages. Needs a
focused gl4es session / upstream issue. 2D hacks unaffected (no gl4es
vertex path); native GL hacks unaffected. Tracked run-only in
tests/smoke-web.sh.
configurations (docs/screenshots/web-gears.png is a real
headless-Chromium capture: three lit, depth-tested gears) but blanks
intermittently on clean rebuilds -- buffer is opaque black, no
geometry, no GL error, shader program bound, viewport correct. Native
gears is unaffected. Suspected gl4es-on-WebGL miscompile sensitive to
build freshness/optimization; needs a focused gl4es session. Tracked
run-only in tests/smoke-web.sh.

## M3b (web shell) -- delivered

Custom emscripten shell (src/web/shell.html) wired via --shell-file:
full-window black canvas (100vw/100vh, SDL resizes the backing store),
no emscripten logo/status/controls, a load veil removed on
onRuntimeInitialized, and the tab title set at runtime by the driver's
SDL_SetWindowTitle. Single-threaded by design (no pthreads) so no
coi-serviceworker / COOP-COEP is needed and it hosts from any static
server. testpattern + deco re-verified rendering full-viewport in
headless Chromium (canvas now 1000x800, filling the window).
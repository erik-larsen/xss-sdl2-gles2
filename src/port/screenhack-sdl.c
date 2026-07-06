/* screenhack-sdl.c -- runs an unmodified xscreensaver hack (its
 * xscreensaver_function_table) on top of the M1a SDL/GLES2 driver.
 *
 * The SDL analog of hacks/screenhack.c's main() + the per-frame logic
 * of jwxyz-android.c's drawXScreenSaver(), with JNI removed:
 *
 *   argv ── options table ──► resource DB ◄── defaults strings
 *   driver surface ──► Window (struct jwxyz_Drawable, image_data)
 *   jwxyz_image_make_display ──► Display
 *   init_cb / draw_cb / reshape_cb / free_cb  through xss_hack adapter
 */

#include "config.h"
#include "screenhackI.h"
#include "jwxyzI.h"
#include "jwxyz-sdl.h"
#include "xss_driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* glx-sdl.c provides these in GL-hack binaries; weak so 2D binaries
 * (which never take the gl_hack_p branch) link without them -- and so
 * this file needs no GL headers (gl4es renames every GL symbol, which
 * would otherwise leak gl4es_* references into 2D binaries). */
#ifdef __APPLE__
/* Mach-O ld errors on undefined weak *declarations* (unlike ELF), so
 * darwin gets weak no-op *definitions* instead; glx-sdl.c's strong
 * definitions override them in GL binaries. 2D binaries call the
 * no-ops, same net effect as the ELF null-check path. */
__attribute__((weak)) void xss_gl_init_once (void) {}
__attribute__((weak)) void xss_gl_viewport (int w, int h) { (void)w; (void)h; }
#else
__attribute__((weak)) void xss_gl_init_once (void);
__attribute__((weak)) void xss_gl_viewport (int w, int h);
#endif

extern int xss_run_screenhack (struct xscreensaver_function_table *xsft,
                               int argc, char **argv);

typedef struct {
  struct xscreensaver_function_table *xsft;
  struct jwxyz_Drawable win;
  Display *dpy;
  void *closure;
} runner;

static runner R;

/* ---- resource setup ------------------------------------------------ */

static void
load_defaults (const char *const *defaults)
{
  for (; defaults && *defaults; defaults++)
    xss_res_put_default (*defaults);
}

/* Consume hack options (per the hack's XrmOptionDescRec table) from
 * argv, writing them into the resource DB.  Unconsumed args are
 * compacted in place for the driver's parser.  Returns new argc. */
static int
consume_hack_options (const XrmOptionDescRec *opts, int argc, char **argv)
{
  int out = 1;
  for (int i = 1; i < argc; i++) {
    const XrmOptionDescRec *m = NULL;
    for (const XrmOptionDescRec *o = opts; o && o->option; o++)
      if (!strcmp (argv[i], o->option)) { m = o; break; }

    if (!m) {
      argv[out++] = argv[i];
      continue;
    }
    switch (m->argKind) {
    case XrmoptionNoArg:
      xss_res_put (m->specifier, m->value ? (char *) m->value : "True");
      break;
    case XrmoptionSepArg:
      if (i + 1 < argc)
        xss_res_put (m->specifier, argv[++i]);
      else
        fprintf (stderr, "%s: %s requires an argument\n", argv[0], m->option);
      break;
    default:
      fprintf (stderr, "%s: unsupported option kind for %s (ignored)\n",
               argv[0], m->option);
      break;
    }
  }
  return out;
}

/* ---- xss_hack adapter ---------------------------------------------- */

static void
sync_window_to_surface (xss_surface *s)
{
  R.win.image_data   = s->pixels;
  R.win.pitch        = (ptrdiff_t) s->pitch * 4;
  R.win.frame.x      = 0;
  R.win.frame.y      = 0;
  R.win.frame.width  = s->width;
  R.win.frame.height = s->height;
}

static Bool
gl_hack_p (void)
{
  return R.xsft->visual == GL_VISUAL;
}

static void *
sh_init (xss_surface *s)
{
  R.win.type = WINDOW;
  R.win.depth = 32;
  R.win.own_data = False;
  sync_window_to_surface (s);
  xss_main_window = &R.win;

  static const unsigned char rgba_bytes[4] = { 0, 1, 2, 3 }; /* RGBA mem */
  R.dpy = jwxyz_image_make_display (&R.win, rgba_bytes);

  if (gl_hack_p ()) {
    if (xss_gl_init_once)
      xss_gl_init_once ();             /* gl4es: proc loader + init */
    if (xss_gl_viewport)
      xss_gl_viewport (s->width, s->height);
  } else {
    /* Paint the background before init, as screenhack.c/android do. */
    unsigned int bg = get_pixel_resource (R.dpy, 0, "background", "Background");
    XSetWindowBackground (R.dpy, &R.win, bg);
    XClearWindow (R.dpy, &R.win);
  }

  /* Two init shapes share the table's init_cb slot (upstream UB that
   * native ABIs forgive but wasm traps on):
   *   - plain screenhack modules: void *init(Display*, Window)
   *   - xlockmore modules: xlockmore_setup() installs the 3-argument
   *     xlockmore_init(Display*, Window, xlockmore_function_table*),
   *     cast down to the 2-arg slot type; callers must pass setup_arg.
   * setup_arg discriminates: xlockmore sets it (the xlmft), plain
   * modules leave it 0. Each call below goes through a pointer type
   * whose wasm signature matches the callee exactly. */
  if (R.xsft->setup_arg) {
    void *(*init3) (Display *, Window, void *) =
      (void *(*) (Display *, Window, void *)) R.xsft->init_cb;
    R.closure = init3 (R.dpy, &R.win, R.xsft->setup_arg);
  } else {
    R.closure = R.xsft->init_cb (R.dpy, &R.win);
  }

  /* Drive one reshape so the hack sets up its projection/viewport.
   * Native X11 gets this from the initial ConfigureNotify; the SDL and
   * emscripten paths have no such event, so GL hacks (which build their
   * projection matrix in reshape_cb) would otherwise render with an
   * identity transform -- i.e. nothing visible. (This is why gears
   * passed the Xvfb smoke test but was blank in the browser.) */
  if (R.xsft->reshape_cb) {
    if (gl_hack_p () && xss_gl_viewport)
      xss_gl_viewport (s->width, s->height);
    R.xsft->reshape_cb (R.dpy, &R.win, R.closure, s->width, s->height);
  }
  return &R;
}

static unsigned long
sh_draw (xss_surface *s, void *state)
{
  (void) state;
  sync_window_to_surface (s);     /* surface realloc moves the pixels */
  return R.xsft->draw_cb (R.dpy, &R.win, R.closure);
}

static void
sh_reshape (xss_surface *s, void *state)
{
  (void) state;
  sync_window_to_surface (s);
  jwxyz_window_resized (R.dpy);
  if (gl_hack_p () && xss_gl_viewport)
    xss_gl_viewport (s->width, s->height);
  R.xsft->reshape_cb (R.dpy, &R.win, R.closure,
                      s->width, s->height);
}

static void
sh_free (void *state)
{
  (void) state;
  if (R.xsft->free_cb)
    R.xsft->free_cb (R.dpy, &R.win, R.closure);
  if (R.dpy)
    jwxyz_image_free_display (R.dpy);
  xss_res_clear ();
}

/* ---- entry ---------------------------------------------------------- */

int
xss_run_screenhack (struct xscreensaver_function_table *xsft,
                    int argc, char **argv)
{
  memset (&R, 0, sizeof R);
  R.xsft = xsft;

  if (xsft->setup_cb)
    xsft->setup_cb (xsft, xsft->setup_arg);

  xss_res_put ("background", "black");      /* baseline, like screenhack.c */
  xss_res_put ("foreground", "white");
  load_defaults (xsft->defaults);
  argc = consume_hack_options (xsft->options, argc, argv);

  xss_hack h;
  memset (&h, 0, sizeof h);
  h.name    = xsft->progclass;
  h.gl_p    = (xsft->visual == GL_VISUAL);
  h.desc    = "xscreensaver hack";
  h.init    = sh_init;
  h.draw    = sh_draw;
  h.reshape = sh_reshape;
  h.free    = sh_free;

  return xss_driver_run (&h, argc, argv);
}

/* fps support: the function table references fps_free; the doFPS path
 * calls this the same way Android does. */
void
screenhack_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure)
{
  (void) dpy; (void) w; (void) closure;
  fps_compute (fpst, 0, -1);
  fps_draw (fpst);
}

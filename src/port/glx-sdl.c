/* glx-sdl.c -- GL plumbing for GL hacks on the SDL port.
 *
 * Mirrors android/screenhack-android.c's GL half: init_GL(), the GLX
 * no-op shims, and GL error helpers -- plus gl4es bootstrap, which is
 * SDL-flavored: gl4es resolves the underlying GLES2 functions through
 * SDL_GL_GetProcAddress on every platform (native and emscripten).
 */

#include "config.h"
#include "screenhackI.h"
#include "xlockmoreI.h"

/* Under emscripten, SDL_main.h redefines main() and the replacement
 * never invokes ours (silent empty-main). We own the entry point. */
#define SDL_MAIN_HANDLED 1
#include <SDL.h>
#include <stdio.h>

#include "gl4esinit.h"
#ifdef __EMSCRIPTEN__
# include <EGL/egl.h>            /* eglGetProcAddress */
static void *
xss_em_getproc (const char *name)
{
  void *p = (void *) eglGetProcAddress (name);
  if (!p)
    fprintf (stderr, "xss-sdl: GetProcAddress(%s) -> NULL\n", name);
  return p;
}
#endif

static Bool gl4es_ready = False;

/* Strong override of the runner's weak viewport hook. */
void
xss_gl_viewport (int w, int h)
{
  glViewport (0, 0, w, h);
}

/* Strong override of the driver's weak hook: runs right after SDL
 * context creation, before any GL call in this binary. */
void
xss_driver_gl_ready (void)
{
  extern void xss_gl_init_once (void);
  xss_gl_init_once ();
}

/* Called by the driver hook once the SDL GLES2 context exists. */
void
xss_gl_init_once (void)
{
  if (gl4es_ready) return;

  /* GLSL-capable hacks run their fixed-function fallbacks (translated
     by gl4es) -- the configuration this port targets. The GLSL-through-
     gl4es path is a future experiment; run with XSS_DISABLE_GLSL unset
     to try it. (Known: hypertorus GLSL crashes in Mesa llvmpipe.) */
  setenv ("XSS_DISABLE_GLSL", "1", 0 /* don't clobber user's choice */);

#ifdef __EMSCRIPTEN__
  /* GL hacks once rendered blank on the web. Root cause (confirmed by
   * WebGL call tracing + in-gl4es probes): gl4es submits client-side
   * vertex arrays by calling glVertexAttribPointer with a raw wasm heap
   * pointer. WebGL forbids client-side arrays; emscripten's FULL_ES2
   * emulation uploads them for us, but only when NO ARRAY_BUFFER is bound
   * at the glVertexAttribPointer call. gl4es's bindBuffer() caches the
   * binding and skips the GLES call when it thinks nothing is bound --
   * while emscripten's own FULL_ES2 path leaves its temp vertex buffer
   * actually-bound after an earlier client draw (the framebuffer blit).
   * The cache desynced (gl4es: ARRAY_BUFFER==0; WebGL: a stale 64-byte
   * buffer), so the heap pointer was read as an out-of-range offset and
   * every draw produced zero fragments -- opaque-black, no GL error.
   * Neither LIBGL_USEVBO 0/1 nor LIBGL_ES=2 changed it. FIXED in
   * third_party/gl4es/src/gl/fpe.c realize_glenv() [PATCH(xss-sdl)]:
   * force a real (cache-bypassing) ARRAY_BUFFER unbind before pointing a
   * client-side attrib, so FULL_ES2 emulation reliably engages. 2D hacks
   * and native GL hacks are unaffected. */
  /* Resolve via emscripten's WebGL proc table. */
  set_getprocaddress (xss_em_getproc);
#else
  set_getprocaddress ((void *(*)(const char *)) SDL_GL_GetProcAddress);
#endif
  initialize_gl4es ();
  gl4es_ready = True;
}

#ifndef __EMSCRIPTEN__
/* Strong overrides of the driver's weak swap hooks. gl4es batches
 * immediate-mode geometry; pre_swap flushes it (and blits gl4es's
 * internal FBO when in use), post_swap rebinds that FBO. Without the
 * flush, GL hacks render opaque black on ANGLE/Metal (llvmpipe was
 * forgiving, which is why Linux never showed it). Native-only for now:
 * the web path was verified working without these, so it stays
 * untouched until it can be re-verified with them. */
extern void gl4es_pre_swap (void);
extern void gl4es_post_swap (void);
void
xss_gl_pre_swap (void)
{
  if (gl4es_ready) gl4es_pre_swap ();
}
void
xss_gl_post_swap (void)
{
  if (gl4es_ready) gl4es_post_swap ();
}
#endif

/* Does nothing -- the SDL driver owns the (single) context. */
void
glXMakeCurrent (Display *dpy, Window window, GLXContext context)
{
  (void) dpy; (void) window; (void) context;
}

/* The driver swaps after draw_cb; hacks calling this directly get a
 * no-op, same as Android. */
void
glXSwapBuffers (Display *dpy, Window window)
{
  (void) dpy; (void) window;
}

void
clear_gl_error (void)
{
  while (glGetError () != GL_NO_ERROR)
    ;
}

void
check_gl_error (const char *type)
{
  char buf[100];
  const char *e;
  GLenum i;
  switch ((i = glGetError ())) {
    case GL_NO_ERROR: return;
    case GL_INVALID_ENUM:      e = "invalid enum";      break;
    case GL_INVALID_VALUE:     e = "invalid value";     break;
    case GL_INVALID_OPERATION: e = "invalid operation"; break;
    case GL_STACK_OVERFLOW:    e = "stack overflow";    break;
    case GL_STACK_UNDERFLOW:   e = "stack underflow";   break;
    case GL_OUT_OF_MEMORY:     e = "out of memory";     break;
    default:
      e = buf; sprintf (buf, "unknown GL error %d", (int) i); break;
  }
  fprintf (stderr, "xss-sdl: %.50s: %.50s\n", type, e);
}

/* Called by OpenGL savers using the XLockmore API.  Same contract as
 * the Android version: clear, and return a pointer to an opaque blob
 * that callers dereference but never look inside. */
GLXContext *
init_GL (ModeInfo *mi)
{
  (void) mi;
  glClearColor (0, 0, 0, 1);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  static int blort = -1;
  return (void *) &blort;
}

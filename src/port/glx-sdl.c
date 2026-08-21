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
#include <string.h>

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

/* ---- retained framebuffer for no-clear (trails) hacks -------------
 *
 * A few hacks composite frame N atop frame N-1 without ever clearing,
 * assuming an X11-style preserved buffer. Post-swap contents are
 * undefined on ANGLE/Metal and discarded outright by the web canvas,
 * so for those hacks the driver brackets each frame with these hooks:
 * frame_begin redraws a saved copy of the previous frame, frame_end
 * captures the new frame back into the texture. This is exactly the
 * save/restore noof.c implements internally (hacks that self-restore
 * are NOT listed here); doing it through gl4es keeps its state cache
 * coherent, and GL_RGB capture stays legal from any framebuffer. */

static GLuint retain_tex = 0;
static int retain_w = 0, retain_h = 0;     /* window size captured    */
static int retain_tw = 0, retain_th = 0;   /* pow2 texture size       */
static Bool retain_primed = False;         /* texture holds a frame   */

static Bool
retain_hack_p (const char *progclass)
{
  static const char *const list[] = { "Flurry", 0 };
  for (const char *const *p = list; *p; p++)
    if (!strcmp (progclass, *p)) return True;
  return False;
}

static int
to_pow2 (int i)
{
  int p = 1;
  while (p < i) p <<= 1;
  return p;
}

void
xss_gl_frame_begin (const char *progclass, int w, int h)
{
  if (!gl4es_ready || !retain_hack_p (progclass)) return;

  if (!retain_tex || w != retain_w || h != retain_h) {
    if (!retain_tex) glGenTextures (1, &retain_tex);
    retain_w = w;  retain_h = h;
    retain_tw = to_pow2 (w);  retain_th = to_pow2 (h);
    glBindTexture (GL_TEXTURE_2D, retain_tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, retain_tw, retain_th, 0,
                  GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture (GL_TEXTURE_2D, 0);
    retain_primed = False;
    return;                         /* first frame: nothing to restore */
  }
  if (!retain_primed) return;

  glPushAttrib (GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_TRANSFORM_BIT |
                GL_VIEWPORT_BIT);
  glDisable (GL_BLEND);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_ALPHA_TEST);
  glDisable (GL_LIGHTING);
  glDisable (GL_CULL_FACE);
  glDisable (GL_SCISSOR_TEST);
  glEnable (GL_TEXTURE_2D);
  glViewport (0, 0, w, h);
  glMatrixMode (GL_PROJECTION);
  glPushMatrix ();
  glLoadIdentity ();
  glOrtho (0, 1, 0, 1, -1, 1);
  glMatrixMode (GL_MODELVIEW);
  glPushMatrix ();
  glLoadIdentity ();

  glBindTexture (GL_TEXTURE_2D, retain_tex);
  glColor3f (1, 1, 1);
  {
    GLfloat tw = w / (GLfloat) retain_tw;
    GLfloat th = h / (GLfloat) retain_th;
    glBegin (GL_QUADS);
    glTexCoord2f (0,  0);  glVertex3f (0, 0, 0);
    glTexCoord2f (tw, 0);  glVertex3f (1, 0, 0);
    glTexCoord2f (tw, th); glVertex3f (1, 1, 0);
    glTexCoord2f (0,  th); glVertex3f (0, 1, 0);
    glEnd ();
  }

  glPopMatrix ();
  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();
  glPopAttrib ();
  glClear (GL_DEPTH_BUFFER_BIT);
}

void
xss_gl_frame_end (const char *progclass, int w, int h)
{
  if (!gl4es_ready || !retain_hack_p (progclass) || !retain_tex) return;
  if (w != retain_w || h != retain_h) return;   /* mid-resize frame */

  glPushAttrib (GL_ENABLE_BIT | GL_TEXTURE_BIT);
  glEnable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, retain_tex);
  glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
  glPopAttrib ();
  retain_primed = True;
}

/* Strong overrides of the driver's weak swap hooks. gl4es batches
 * immediate-mode geometry; pre_swap flushes it (and blits gl4es's
 * internal FBO when in use), post_swap rebinds that FBO. Without the
 * flush, GL hacks render opaque black on ANGLE/Metal (llvmpipe was
 * forgiving, which is why Linux never showed it).
 *
 * The web build needs this flush just as much: gl4es interposes ALL
 * GL symbols in these binaries (the driver's own calls included), so
 * with no pre-swap flush a hack renders only if its state churn
 * happens to trip gl4es's internal flush heuristics. Steady-state
 * immediate-mode hacks (cube21, papercube, hydrostat, ...) batched
 * forever and stayed blank -- this was previously #ifndef
 * __EMSCRIPTEN__ under a "verified working without" note that only
 * held for the lucky subset. */
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

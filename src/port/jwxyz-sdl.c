/* jwxyz-sdl.c -- SDL2 platform layer for jwxyz (JWXYZ_IMAGE mode).
 *
 * Implements the platform half of jwxyz, mirroring the structure of
 * jwxyz-android.c with JNI/EGL removed:
 *   - pixmap allocation (CPU buffers)
 *   - drawable accessors (frame/depth/data/pitch)
 *   - logging / abort
 *   - resource database (defaults + argv) behind get_string_resource()
 *   - font hooks (M1b: metric-sane stubs; real fonts arrive in M6)
 */

#include "config.h"
#include "jwxyzI.h"
#include "jwxyz-sdl.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct jwxyz_Drawable *xss_main_window = NULL;

const char *progname = "xss-sdl";   /* utils/resources.c wants these   */
const char *progclass = "XScreenSaver";

/* ------------------------------------------------------------------ */
/* Logging                                                             */
/* ------------------------------------------------------------------ */

void
jwxyz_logv (Bool error, const char *fmt, va_list args)
{
  fprintf (stderr, "%s: %s: ", progname, error ? "error" : "log");
  vfprintf (stderr, fmt, args);
  fputc ('\n', stderr);
}

void
jwxyz_abort (const char *fmt, ...)
{
  if (fmt && *fmt) {
    va_list args;
    va_start (args, fmt);
    jwxyz_logv (True, fmt, args);
    va_end (args);
  }
  fflush (stderr);
  /* jwxyz.h #defines abort() to jwxyz_abort -- don't recurse */
# undef abort
  abort ();
}

/* ------------------------------------------------------------------ */
/* Drawables                                                           */
/* ------------------------------------------------------------------ */

Pixmap
XCreatePixmap (Display *dpy, Drawable d,
               unsigned int width, unsigned int height, unsigned int depth)
{
  (void) dpy; (void) d;
  Assert (depth == 1 || depth == 32, "XCreatePixmap: bad depth %d", depth);

  Pixmap p = calloc (1, sizeof (*p));
  p->type = PIXMAP;
  p->frame.x = 0;
  p->frame.y = 0;
  p->frame.width  = width;
  p->frame.height = height;
  p->depth = depth;
  p->pitch = (ptrdiff_t) width * 4;     /* always 32bpp storage */
  p->image_data = calloc ((size_t) height, (size_t) p->pitch);
  p->own_data = True;
  Assert (p->image_data, "XCreatePixmap: out of memory");
  return p;
}

int
XFreePixmap (Display *dpy, Pixmap p)
{
  (void) dpy;
  if (p) {
    if (p->own_data)
      free (p->image_data);
    free (p);
  }
  return 0;
}

ptrdiff_t
jwxyz_image_pitch (Drawable d)
{
  return d->pitch;
}

void *
jwxyz_image_data (Drawable d)
{
  return d->image_data;
}

const XRectangle *
jwxyz_frame (Drawable d)
{
  return &d->frame;
}

unsigned int
jwxyz_drawable_depth (Drawable d)
{
  return (d->type == WINDOW ? visual_depth (NULL, NULL) : d->depth);
}

void
jwxyz_get_pos (Window w, XPoint *xvpos, XPoint *xp)
{
  xvpos->x = 0;
  xvpos->y = 0;
  if (xp) {                    /* mouse position; wired up in M5 events */
    xp->x = w->frame.width  / 2;
    xp->y = w->frame.height / 2;
  }
}

float
jwxyz_scale (Window main_window)
{
  (void) main_window;
  return 1.0f;
}

float
jwxyz_font_scale (Window main_window)
{
  (void) main_window;
  return 1.0f;
}

double
current_device_rotation (void)
{
  return 0.0;
}

Bool
ignore_rotation_p (Display *dpy)
{
  (void) dpy;
  return True;
}

/* ------------------------------------------------------------------ */
/* Resource database                                                   */
/* ------------------------------------------------------------------ */

typedef struct { char *key; char *val; } xss_res;
static xss_res *res_tab = NULL;
static int res_count = 0, res_alloc = 0;

static char *
trim_dup (const char *s, const char *end)
{
  while (s < end && (*s == ' ' || *s == '\t')) s++;
  while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                     end[-1] == '\n' || end[-1] == '\r')) end--;
  char *r = malloc (end - s + 1);
  memcpy (r, s, end - s);
  r[end - s] = 0;
  return r;
}

void
xss_res_clear (void)
{
  for (int i = 0; i < res_count; i++) {
    free (res_tab[i].key);
    free (res_tab[i].val);
  }
  free (res_tab);
  res_tab = NULL;
  res_count = res_alloc = 0;
}

void
xss_res_put (const char *key, const char *val)
{
  /* strip leading "*" / "." / "ProgClass." qualifiers */
  const char *k = key;
  const char *dot = strrchr (k, '.');
  const char *star = strrchr (k, '*');
  if (dot  && dot  >= k) k = dot + 1;
  if (star && star >= k) k = star + 1;

  for (int i = 0; i < res_count; i++)
    if (!strcasecmp (res_tab[i].key, k)) {
      free (res_tab[i].val);
      res_tab[i].val = strdup (val);
      return;
    }
  if (res_count == res_alloc) {
    res_alloc = res_alloc ? res_alloc * 2 : 64;
    res_tab = realloc (res_tab, res_alloc * sizeof (*res_tab));
  }
  res_tab[res_count].key = strdup (k);
  res_tab[res_count].val = strdup (val);
  res_count++;
}

void
xss_res_put_default (const char *spec)         /* "*delay:  10000" */
{
  const char *colon = strchr (spec, ':');
  if (!colon) return;
  char *key = trim_dup (spec, colon);
  char *val = trim_dup (colon + 1, colon + 1 + strlen (colon + 1));
  if (*key)
    xss_res_put (key, val);
  free (key);
  free (val);
}

const char *
xss_res_get (const char *name)
{
  for (int i = 0; i < res_count; i++)
    if (!strcasecmp (res_tab[i].key, name))
      return res_tab[i].val;
  return NULL;
}

char *
get_string_resource (Display *dpy, char *res_name, char *res_class)
{
  (void) dpy; (void) res_class;
  const char *v = xss_res_get (res_name);
  return v ? strdup (v) : NULL;
}

/* ------------------------------------------------------------------ */
/* Fonts -- M1b stubs.                                                 */
/* Sane metrics, no glyph rendering.  Real text lands in M6 via        */
/* FreeType/stb_truetype against xscreensaver's bundled fonts.         */
/* ------------------------------------------------------------------ */

struct xss_stub_font { int size; };

const char *
jwxyz_default_font_family (int require)
{
  switch (require) {
  case JWXYZ_FONT_FAMILY & JWXYZ_STYLE_MONOSPACE: return "Courier";
  default:                                        return "Helvetica";
  }
}

void *
jwxyz_load_native_font (Window main_window,
                        int traits_jwxyz, int mask_jwxyz,
                        const char *font_name_ptr, size_t font_name_length,
                        int font_name_type, float size,
                        char **family_name_ret,
                        int *ascent_ret, int *descent_ret)
{
  (void) main_window; (void) traits_jwxyz; (void) mask_jwxyz;
  (void) font_name_ptr; (void) font_name_length; (void) font_name_type;

  struct xss_stub_font *f = calloc (1, sizeof (*f));
  f->size = (int) (size > 0 ? size : 12);
  if (family_name_ret) *family_name_ret = strdup ("Stub");
  if (ascent_ret)  *ascent_ret  = (int) (f->size * 0.8);
  if (descent_ret) *descent_ret = (int) (f->size * 0.2);
  return f;
}

void
jwxyz_release_native_font (Display *dpy, void *native_font)
{
  (void) dpy;
  free (native_font);
}

void
jwxyz_render_text (Display *dpy, void *native_font,
                   const char *str, size_t len, Bool utf8, Bool antialias_p,
                   XCharStruct *cs_ret, char **pixmap_ret)
{
  (void) dpy; (void) utf8; (void) antialias_p;
  struct xss_stub_font *f = native_font;
  int w = (int) (len * f->size * 0.6);
  (void) w;

  /* Zero-size metrics: jwxyz_draw_string's contract treats w==0/h==0
     as "nothing to draw" and never touches the bitmap. Advancing width
     stays 0 too -- text is simply invisible until real fonts in M6. */
  if (cs_ret)
    memset (cs_ret, 0, sizeof (*cs_ret));
  if (pixmap_ret)
    *pixmap_ret = NULL;
}

char *
jwxyz_unicode_character_name (Display *dpy, Font fid, unsigned long uc)
{
  (void) dpy; (void) fid; (void) uc;
  return strdup ("STUB");
}

/* ------------------------------------------------------------------ */
/* Typed resource getters (under HAVE_ANDROID, resources.c expects     */
/* the platform to provide these; semantics copied from                */
/* android/screenhack-android.c)                                       */
/* ------------------------------------------------------------------ */

int mono_p = 0;

Bool
get_boolean_resource (Display *dpy, char *res_name, char *res_class)
{
  char *s = get_string_resource (dpy, res_name, res_class);
  if (!s) return False;
  Bool r = False;
  const char *p = s;
  while (*p && (*p == ' ' || *p == '\t')) p++;
  if (!strncasecmp (p, "true", 4) || !strncasecmp (p, "yes", 3) ||
      !strncasecmp (p, "on", 2)   || *p == '1')
    r = True;
  free (s);
  return r;
}

int
get_integer_resource (Display *dpy, char *res_name, char *res_class)
{
  char *s = get_string_resource (dpy, res_name, res_class);
  if (!s) return 0;
  int v = 0;
  if (1 != sscanf (s, " %d ", &v) &&
      1 != sscanf (s, " 0x%x ", (unsigned int *) &v))
    fprintf (stderr, "%s: %s: bad integer: \"%s\"\n", progname, res_name, s);
  free (s);
  return v;
}

double
get_float_resource (Display *dpy, char *res_name, char *res_class)
{
  char *s = get_string_resource (dpy, res_name, res_class);
  if (!s) return 0.0;
  double v = 0;
  if (1 != sscanf (s, " %lf ", &v))
    fprintf (stderr, "%s: %s: bad float: \"%s\"\n", progname, res_name, s);
  free (s);
  return v;
}

/* image mode: nothing cached per-size; resize is fully handled by the
 * runner updating the Window's frame/image_data. */
void
jwxyz_window_resized (Display *dpy)
{
  (void) dpy;
}

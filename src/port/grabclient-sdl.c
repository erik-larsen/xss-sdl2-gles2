/* grabclient-sdl.c -- image source for the SDL/web port.
 *
 * The image-grab hacks (jigsaw, antspotlight, decayscreen, distort,
 * ripples, zoom, ...) ask grabclient for a screenshot / random photo to
 * manipulate. Upstream X11 forks xscreensaver-getimage; iOS/Android
 * instead call one platform hook, jwxyz_draw_random_image(), from
 * utils/grabclient.c. We reuse that file and supply the hook here.
 *
 * The hook draws a random image from the bundled CC0 landscape set
 * (assets/images/, provenance in its README.md) scaled-to-cover into
 * the target drawable and returns its (malloc'd) name. Sources:
 *   - native: $XSS_IMAGE_DIR, or ./assets/images
 *   - web:    the shell (src/web/shell.html) prefetches one random
 *             image from ../images/ before the wasm runtime starts and
 *             parks the bytes on Module; we copy them out here.
 * Decoding is stb_image (JPEG/PNG -> RGBA), matching the byte order
 * jwxyz-image uses ({0,1,2,3} RGBA; see jwxyz_png_to_ximage).
 *
 * Returning NULL makes grabclient fall back to SMPTE colour bars (its
 * standard no-image behaviour) -- so hacks still work with no image
 * set present (or when the web fetch fails / is offline).
 */

#include "utils.h"
#include "jwxyz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
# include <emscripten.h>
#else
# ifndef _WIN32
#  include <dirent.h>
#  include <strings.h>
# endif
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

/* Scale src (sw x sh RGBA) to cover dst (dw x dh RGBA): aspect-fill,
 * centered crop, bilinear. */
static void
scale_to_cover (const unsigned char *src, int sw, int sh,
                unsigned char *dst, int dw, int dh)
{
  double scale = (double) dw / sw;
  if ((double) dh / sh > scale) scale = (double) dh / sh;
  double xoff = (sw - dw / scale) / 2.0;
  double yoff = (sh - dh / scale) / 2.0;

  for (int y = 0; y < dh; y++) {
    double fy = yoff + y / scale;
    int y0 = (int) fy; if (y0 > sh - 2) y0 = sh - 2; if (y0 < 0) y0 = 0;
    double ty = fy - y0; if (ty < 0) ty = 0; if (ty > 1) ty = 1;
    for (int x = 0; x < dw; x++) {
      double fx = xoff + x / scale;
      int x0 = (int) fx; if (x0 > sw - 2) x0 = sw - 2; if (x0 < 0) x0 = 0;
      double tx = fx - x0; if (tx < 0) tx = 0; if (tx > 1) tx = 1;
      const unsigned char *p00 = src + 4 * ((size_t) y0 * sw + x0);
      const unsigned char *p10 = p00 + 4;
      const unsigned char *p01 = p00 + 4 * (size_t) sw;
      const unsigned char *p11 = p01 + 4;
      unsigned char *o = dst + 4 * ((size_t) y * dw + x);
      for (int c = 0; c < 4; c++)
        o[c] = (unsigned char)
          (p00[c] * (1 - tx) * (1 - ty) + p10[c] * tx * (1 - ty) +
           p01[c] * (1 - tx) * ty       + p11[c] * tx * ty + 0.5);
    }
  }
}

/* ---- image bytes + name, per platform --------------------------------- */

#ifdef __EMSCRIPTEN__

/* The shell (src/web/shell.html) fetches the bundled image set before
 * main() runs (preRun + addRunDependency) and parks it on
 * Module.xssImages as [{name, bytes}]. Startup only waits for the first
 * one, so the array keeps growing while the hack runs -- we pick from
 * whatever has landed by the time this grab happens, which is what
 * makes repeat-grabbing hacks (carousel) show more than one picture. */

EM_JS (int, xss_web_image_count, (void), {
  return (Module.xssImages && Module.xssImages.length) || 0;
});
EM_JS (int, xss_web_image_len, (int i), {
  return Module.xssImages[i].bytes.length;
});
EM_JS (void, xss_web_image_copy, (int i, unsigned char *ptr), {
  HEAPU8.set(Module.xssImages[i].bytes, ptr);
});

static unsigned char *
grab_image_bytes (size_t *len_ret, char **name_ret)
{
  int n = xss_web_image_count ();
  if (n <= 0) return NULL;
  int i = (int) (random () % n);

  int len = xss_web_image_len (i);
  if (len <= 0) return NULL;
  unsigned char *buf = malloc (len);
  if (!buf) return NULL;
  xss_web_image_copy (i, buf);

  char js[128];
  snprintf (js, sizeof js, "(Module.xssImages[%d].name || '')", i);
  const char *name = emscripten_run_script_string (js);
  *name_ret = strdup (name && *name ? name : "image");
  *len_ret = (size_t) len;
  return buf;
}

#elif defined(_WIN32)

/* No dirent on MSVC; the Windows build is WIP anyway. Colour bars. */
static unsigned char *
grab_image_bytes (size_t *len_ret, char **name_ret)
{
  (void) len_ret; (void) name_ret;
  return NULL;
}

#else  /* native POSIX */

static int
image_file_p (const char *n)
{
  const char *dot = strrchr (n, '.');
  return dot && (!strcasecmp (dot, ".jpg") || !strcasecmp (dot, ".jpeg")
                 || !strcasecmp (dot, ".png"));
}

static unsigned char *
grab_image_bytes (size_t *len_ret, char **name_ret)
{
  const char *dirname = getenv ("XSS_IMAGE_DIR");
  if (!dirname || !*dirname) dirname = "assets/images";

  DIR *d = opendir (dirname);
  if (!d) return NULL;

  char names[64][256];
  int n = 0;
  struct dirent *e;
  while ((e = readdir (d)) && n < 64)
    if (image_file_p (e->d_name)) {
      strncpy (names[n], e->d_name, sizeof names[n] - 1);
      names[n][sizeof names[n] - 1] = 0;
      n++;
    }
  closedir (d);
  if (!n) return NULL;

  const char *pick = names[random () % n];
  char path[1024];
  snprintf (path, sizeof path, "%s/%s", dirname, pick);

  FILE *f = fopen (path, "rb");
  if (!f) return NULL;
  fseek (f, 0, SEEK_END);
  long sz = ftell (f);
  fseek (f, 0, SEEK_SET);
  unsigned char *buf = (sz > 0) ? malloc (sz) : NULL;
  if (!buf || fread (buf, 1, sz, f) != (size_t) sz) {
    free (buf); fclose (f); return NULL;
  }
  fclose (f);
  *len_ret = (size_t) sz;
  *name_ret = strdup (pick);
  return buf;
}

#endif

/* Raw file bytes for hacks that corrupt the encoded image themselves
 * (glitchpeg). Same sources as the draw hook below; caller frees both. */
unsigned char *
xss_grab_image_file_bytes (unsigned long *len_ret, char **name_ret)
{
  size_t len = 0;
  char *name = NULL;
  unsigned char *bytes = grab_image_bytes (&len, &name);
  if (!bytes) return NULL;
  *len_ret = (unsigned long) len;
  *name_ret = name;
  return bytes;
}

/* ---- the hook --------------------------------------------------------- */

char *
jwxyz_draw_random_image (Display *dpy, Drawable drawable, GC gc)
{
  size_t len = 0;
  char *name = NULL;
  unsigned char *bytes = grab_image_bytes (&len, &name);
  if (!bytes) return NULL;   /* -> grabclient draws colour bars */

  int sw = 0, sh = 0, comp = 0;
  unsigned char *rgba = stbi_load_from_memory (bytes, (int) len,
                                               &sw, &sh, &comp, 4);
  free (bytes);
  if (!rgba || sw < 2 || sh < 2) {
    fprintf (stderr, "xss-sdl: image decode failed (%s)\n",
             name ? name : "?");
    free (name);
    stbi_image_free (rgba);
    return NULL;
  }

  Window root;
  int x, y;
  unsigned int dw, dh, bw, depth;
  XGetGeometry (dpy, drawable, &root, &x, &y, &dw, &dh, &bw, &depth);
  if (dw < 1 || dh < 1) {
    free (name);
    stbi_image_free (rgba);
    return NULL;
  }

  char *data = malloc ((size_t) dw * dh * 4);
  if (!data) { free (name); stbi_image_free (rgba); return NULL; }
  scale_to_cover (rgba, sw, sh, (unsigned char *) data, dw, dh);
  stbi_image_free (rgba);

  XImage *img = XCreateImage (dpy, NULL, 32, ZPixmap, 0, data,
                              dw, dh, 32, dw * 4);
  XPutImage (dpy, drawable, gc, img, 0, 0, 0, 0, dw, dh);
  XDestroyImage (img);   /* frees data */

  return name;
}

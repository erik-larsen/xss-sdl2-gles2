/* jwxyz-png.c -- jwxyz_png_to_ximage() for the SDL port.
 *
 * ximage-loader.c's HAVE_JWXYZ branch decodes embedded PNG data through
 * this platform hook (Android does it via a JNI round-trip to
 * android.graphics.Bitmap).  Here: libpng, normalized to 8-bit RGBA,
 * returned as a 32bpp ZPixmap XImage whose pixels are RGBA in memory --
 * the same layout jwxyz-image and the GL texture loaders expect.
 */

#include "config.h"
#include "jwxyzI.h"
#include "jwxyz-sdl.h"

#include <png.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const unsigned char *p; size_t left; } png_src;

static void
read_fn (png_structp png, png_bytep out, png_size_t n)
{
  png_src *src = png_get_io_ptr (png);
  if (n > src->left) { png_error (png, "short PNG data"); return; }
  memcpy (out, src->p, n);
  src->p += n;
  src->left -= n;
}

XImage *
jwxyz_png_to_ximage (Display *dpy, Visual *visual,
                     const unsigned char *png_data, unsigned long data_size)
{
  (void) visual;
  if (data_size < 8 || png_sig_cmp (png_data, 0, 8))
    return NULL;

  png_structp png = png_create_read_struct (PNG_LIBPNG_VER_STRING, 0, 0, 0);
  png_infop info = png ? png_create_info_struct (png) : NULL;
  if (!info) { if (png) png_destroy_read_struct (&png, 0, 0); return NULL; }

  png_bytep *rows = NULL;
  XImage *img = NULL;

  if (setjmp (png_jmpbuf (png))) {
    free (rows);
    if (img) { free (img->data); img->data = 0; XDestroyImage (img); }
    png_destroy_read_struct (&png, &info, 0);
    return NULL;
  }

  png_src src = { png_data, data_size };
  png_set_read_fn (png, &src, read_fn);
  png_read_info (png, info);

  png_uint_32 w, h;
  int depth, color;
  png_get_IHDR (png, info, &w, &h, &depth, &color, 0, 0, 0);

  /* Normalize anything to 8-bit RGBA. */
  if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb (png);
  if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
    png_set_expand_gray_1_2_4_to_8 (png);
  if (png_get_valid (png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha (png);
  if (depth == 16) png_set_strip_16 (png);
  if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb (png);
  png_set_filler (png, 0xFF, PNG_FILLER_AFTER);   /* force alpha channel */
  png_read_update_info (png, info);

  char *data = malloc ((size_t) w * h * 4);
  rows = malloc (h * sizeof (*rows));
  if (!data || !rows) longjmp (png_jmpbuf (png), 1);
  for (png_uint_32 y = 0; y < h; y++)
    rows[y] = (png_bytep) data + (size_t) y * w * 4;

  png_read_image (png, rows);
  png_read_end (png, 0);
  free (rows);
  rows = NULL;
  png_destroy_read_struct (&png, &info, 0);

  img = XCreateImage (dpy, NULL, 32, ZPixmap, 0, data, w, h, 32, w * 4);
  return img;
}

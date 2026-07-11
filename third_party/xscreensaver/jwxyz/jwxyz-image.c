/* xscreensaver, Copyright (c) 1991-2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* JWXYZ Is Not Xlib.

   Pixmaps implemented in CPU RAM, for Android OpenGL hacks.
   Renders into an XImage, basically.

   See the comment at the top of jwxyz-common.c for an explanation of
   the division of labor between these various modules.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef JWXYZ_IMAGE /* entire file */

#include "jwxyzI.h"
#include "jwxyz.h"
#include "jwxyz-timers.h"
#include "pow2.h"

#include <wchar.h>


union color_bytes { // Hello, again.
  uint32_t pixel;
  uint8_t bytes[4];
};

struct jwxyz_Display {
  const struct jwxyz_vtbl *vtbl; // Must come first.

  Window main_window;
  Visual visual;
  struct jwxyz_sources_data *timers_data;

  unsigned long window_background;
};

struct jwxyz_GC {
  XGCValues gcv;
  unsigned int depth;
};


extern const struct jwxyz_vtbl image_vtbl;

Display *
jwxyz_image_make_display (Window w, const unsigned char *rgba_bytes)
{
  Display *d = (Display *) calloc (1, sizeof(*d));
  d->vtbl = &image_vtbl;

  Visual *v = &d->visual;
  v->class      = TrueColor;
  Assert (rgba_bytes[3] == 3, "alpha not last");
  unsigned long masks[4];
  for (unsigned i = 0; i != 4; ++i) {
    union color_bytes color;
    color.pixel = 0;
    color.bytes[rgba_bytes[i]] = 0xff;
    masks[i] = color.pixel;
  }
  v->red_mask   = masks[0];
  v->green_mask = masks[1];
  v->blue_mask  = masks[2];
  v->alpha_mask = masks[3];

  d->timers_data = jwxyz_sources_init (XtDisplayToApplicationContext (d));
  d->window_background = BlackPixel(d,0);
  d->main_window = w;

  return d;
}

void
jwxyz_image_free_display (Display *dpy)
{
  jwxyz_sources_free (dpy->timers_data);

  free (dpy);
}


static jwxyz_sources_data *
display_sources_data (Display *dpy)
{
  return dpy->timers_data;
}


static Window
root (Display *dpy)
{
  return dpy->main_window;
}

static Visual *
visual (Display *dpy)
{
  return &dpy->visual;
}


static void
next_point(short *v, XPoint p, int mode)
{
  switch (mode) {
    case CoordModeOrigin:
      v[0] = p.x;
      v[1] = p.y;
      break;
    case CoordModePrevious:
      v[0] += p.x;
      v[1] += p.y;
      break;
    default:
      Assert (False, "next_point: bad mode");
      break;
  }
}

#define SEEK_DRAWABLE(d, x, y) \
  SEEK_XY (jwxyz_image_data(d), jwxyz_image_pitch(d), x, y)

static int
DrawPoints (Display *dpy, Drawable d, GC gc,
            XPoint *points, int count, int mode)
{
  /* PATCH(xss-sdl): support GXxor/GXor (munch, bouboule) besides GXcopy. */
  int func = gc->gcv.function;
  Assert (func == GXcopy || func == GXxor || func == GXor,
          "XDrawPoints: bad GC function");

  const XRectangle *frame = jwxyz_frame (d);
  short v[2] = {0, 0};
  for (unsigned i = 0; i < count; i++) {
    next_point(v, points[i], mode);
    if (v[0] >= 0 && v[0] < frame->width &&
        v[1] >= 0 && v[1] < frame->height) {
      uint32_t *p = SEEK_DRAWABLE(d, v[0], v[1]);
      switch (func) {
      case GXxor: *p ^= gc->gcv.foreground; break;
      case GXor:  *p |= gc->gcv.foreground; break;
      default:    *p  = gc->gcv.foreground; break;
      }
    }
  }

  return 0;
}


static void
copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
           int src_x, int src_y, unsigned int width, unsigned int height,
           int dst_x, int dst_y)
{
  jwxyz_blit (jwxyz_image_data (src), jwxyz_image_pitch (src), src_x, src_y, 
              jwxyz_image_data (dst), jwxyz_image_pitch (dst), dst_x, dst_y, 
              width, height);
}


static void
draw_line (Drawable d, unsigned long pixel,
           short x0, short y0, short x1, short y1)
{
// TODO: Assert line_Width == 1, line_stipple == solid, etc.

  const XRectangle *frame = jwxyz_frame (d);

  /* PATCH(xss-sdl): clip lines to the frame instead of dropping them.
     A real X server clips; hacks legitimately draw lines that touch or
     cross the edge (lcdscrub's stripes end exactly at width/height and
     were ALL dropped -> solid blank; juggle/phosphor logged this too).
     Cohen-Sutherland endpoint clip; the couple-of-LSB Bresenham phase
     difference vs true X11 clipping is invisible here. */
  {
    int fw = frame->width, fh = frame->height;
# define OUTCODE(x, y) (((x) < 0 ? 1 : (x) >= fw ? 2 : 0) | \
                        ((y) < 0 ? 4 : (y) >= fh ? 8 : 0))
    int c0 = OUTCODE (x0, y0), c1 = OUTCODE (x1, y1);
    while (c0 | c1) {
      int c, nx, ny;
      double t;
      if (c0 & c1)
        return;                        /* fully outside, nothing to draw */
      c = c0 ? c0 : c1;
      if (c & 8)                       /* below */
        t = (fh - 1 - y0) / (double) (y1 - y0), nx = x0 + (int) ((x1 - x0) * t + 0.5), ny = fh - 1;
      else if (c & 4)                  /* above */
        t = (0 - y0) / (double) (y1 - y0),      nx = x0 + (int) ((x1 - x0) * t + 0.5), ny = 0;
      else if (c & 2)                  /* right */
        t = (fw - 1 - x0) / (double) (x1 - x0), ny = y0 + (int) ((y1 - y0) * t + 0.5), nx = fw - 1;
      else                             /* left */
        t = (0 - x0) / (double) (x1 - x0),      ny = y0 + (int) ((y1 - y0) * t + 0.5), nx = 0;
      if (c == c0) { x0 = nx; y0 = ny; c0 = OUTCODE (x0, y0); }
      else         { x1 = nx; y1 = ny; c1 = OUTCODE (x1, y1); }
    }
# undef OUTCODE
  }

  int dx = abs(x1 - x0), dy = abs(y1 - y0);

  unsigned dmod0, dmod1;
  int dpx0, dpx1;
  if (dx > dy) {
    dmod0 = dy;
    dmod1 = dx;
    dpx0 = x1 > x0 ? 1 : -1;
    dpx1 = y1 > y0 ? frame->width : -frame->width;
  } else {
    dmod0 = dx;
    dmod1 = dy;
    dpx0 = y1 > y0 ? frame->width : -frame->width;
    dpx1 = x1 > x0 ? 1 : -1;
  }

  unsigned n = dmod1;
  unsigned mod = n;
  ++n;

  dmod0 <<= 1;
  dmod1 <<= 1;

  uint32_t *px = SEEK_DRAWABLE(d, x0, y0);

  for(; n; --n) {
    *px = pixel;

    mod += dmod0;
    if(mod > dmod1) {
      mod -= dmod1;
      px += dpx1;
    }

    px += dpx0;
  }
}

static int
DrawLines (Display *dpy, Drawable d, GC gc, XPoint *points, int count,
           int mode)
{
  short v[2] = {0, 0}, v_prev[2] = {0, 0};
  unsigned long pixel = gc->gcv.foreground;
  for (unsigned i = 0; i != count; ++i) {
    next_point(v, points[i], mode);
    if (i)
      draw_line (d, pixel, v_prev[0], v_prev[1], v[0], v[1]);
    v_prev[0] = v[0];
    v_prev[1] = v[1];
  }
  return 0;
}


static int
DrawSegments (Display *dpy, Drawable d, GC gc, XSegment *segments, int count)
{
  unsigned long pixel = gc->gcv.foreground;
  for (unsigned i = 0; i != count; ++i) {
    XSegment *seg = &segments[i];
    draw_line (d, pixel, seg->x1, seg->y1, seg->x2, seg->y2);
  }
  return 0;
}


static int
ClearWindow (Display *dpy, Window win)
{
  Assert (win == dpy->main_window, "not a window");
  const XRectangle *wr = jwxyz_frame (win);
  return XClearArea (dpy, win, 0, 0, wr->width, wr->height, 0);
}

static unsigned long *
window_background (Display *dpy)
{
  return &dpy->window_background;
}

static void
fill_rects (Display *dpy, Drawable d, GC gc,
            const XRectangle *rectangles, unsigned long nrectangles,
            unsigned long pixel)
{
  /* PATCH(xss-sdl): support GXxor/GXor (crystal, bouboule) besides
     GXcopy. */
  int func = gc ? gc->gcv.function : GXcopy;
  Assert (func == GXcopy || func == GXxor || func == GXor,
          "fill_rects: bad GC function");

  const XRectangle *frame = jwxyz_frame (d);
  void *image_data = jwxyz_image_data (d);
  ptrdiff_t image_pitch = jwxyz_image_pitch (d);

  for (unsigned i = 0; i != nrectangles; ++i) {
    const XRectangle *rect = &rectangles[i];
    unsigned x0 = rect->x >= 0 ? rect->x : 0, y0 = rect->y >= 0 ? rect->y : 0;
    int x1 = rect->x + rect->width, y1 = rect->y + rect->height;
    if (y1 > frame->height)
      y1 = frame->height;
    if (x1 > frame->width)
      x1 = frame->width;
    /* PATCH(xss-sdl): a rect lying entirely outside the frame (hacks
       draw objects that drift off-screen; a real X server clips them)
       leaves x1 < x0 or y1 < y0 here, and the unsigned subtraction
       below then wraps to ~4G rows: wmemset scribbles past the buffer
       and segfaults (galaxy, grav, bouboule, ... on macOS). Skip. */
    if (x1 <= (int) x0 || y1 <= (int) y0)
      continue;
    unsigned x_size = x1 - x0, y_size = y1 - y0;
    void *dst = SEEK_XY (image_data, image_pitch, x0, y0);
    while (y_size) {
      switch (func) {                           /* PATCH(xss-sdl) */
      case GXxor:
        for(size_t j = 0; j != x_size; ++j)
          ((uint32_t *)dst)[j] ^= (uint32_t) pixel;
        break;
      case GXor:
        for(size_t j = 0; j != x_size; ++j)
          ((uint32_t *)dst)[j] |= (uint32_t) pixel;
        break;
      default:
# if __SIZEOF_WCHAR_T__ == 4
        wmemset (dst, (wchar_t) pixel, x_size);
# else
        for(size_t j = 0; j != x_size; ++j)
          ((uint32_t *)dst)[j] = pixel;
# endif
        break;
      }
      --y_size;
      dst = (char *) dst + image_pitch;
    }
  }
}


static int
FillPolygon (Display *dpy, Drawable d, GC gc,
             XPoint *points, int npoints, int shape, int mode)
{
  /* PATCH(xss-sdl): upstream stubs this; delegate to the SDL port's
     raster implementation (src/port/jwxyz-arcs.c). */
  extern int jwxyz_image_fill_polygon (Display *, Drawable, GC,
                                       XPoint *, int, int, int);
  return jwxyz_image_fill_polygon (dpy, d, gc, points, npoints, shape, mode);
  return 0;
}

static int
draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
                unsigned int width, unsigned int height,
                int angle1, int angle2, Bool fill_p)
{
  /* PATCH(xss-sdl): upstream stubs this; delegate to the SDL port's
     raster implementation (src/port/jwxyz-arcs.c). */
  extern int jwxyz_image_draw_arc (Display *, Drawable, GC, int, int,
                                   unsigned int, unsigned int,
                                   int, int, Bool);
  return jwxyz_image_draw_arc (dpy, d, gc, x, y, width, height,
                               angle1, angle2, fill_p);
}


static XGCValues *
gc_gcv (GC gc)
{
  return &gc->gcv;
}


static unsigned int
gc_depth (GC gc)
{
  return gc->depth;
}


static GC
CreateGC (Display *dpy, Drawable d, unsigned long mask, XGCValues *xgcv)
{
  struct jwxyz_GC *gc = (struct jwxyz_GC *) calloc (1, sizeof(*gc));
  gc->depth = jwxyz_drawable_depth (d);

  jwxyz_gcv_defaults (dpy, &gc->gcv, gc->depth);
  XChangeGC (dpy, gc, mask, xgcv);
  return gc;
}


static int
FreeGC (Display *dpy, GC gc)
{
  if (gc->gcv.font)
    XUnloadFont (dpy, gc->gcv.font);

  free (gc);
  return 0;
}


static int
PutImage (Display *dpy, Drawable d, GC gc, XImage *ximage,
          int src_x, int src_y, int dest_x, int dest_y,
          unsigned int w, unsigned int h)
{
  const XRectangle *wr = jwxyz_frame (d);

  Assert (gc, "no GC");
  Assert ((w < 65535), "improbably large width");
  Assert ((h < 65535), "improbably large height");
  Assert ((src_x  < 65535 && src_x  > -65535), "improbably large src_x");
  Assert ((src_y  < 65535 && src_y  > -65535), "improbably large src_y");
  Assert ((dest_x < 65535 && dest_x > -65535), "improbably large dest_x");
  Assert ((dest_y < 65535 && dest_y > -65535), "improbably large dest_y");

  // Clip width and height to the bounds of the Drawable
  //
  if (dest_x + w > wr->width) {
    if (dest_x > wr->width)
      return 0;
    w = wr->width - dest_x;
  }
  if (dest_y + h > wr->height) {
    if (dest_y > wr->height)
      return 0;
    h = wr->height - dest_y;
  }
  if (w <= 0 || h <= 0)
    return 0;

  // Clip width and height to the bounds of the XImage
  //
  if (src_x + w > ximage->width) {
    if (src_x > ximage->width)
      return 0;
    w = ximage->width - src_x;
  }
  if (src_y + h > ximage->height) {
    if (src_y > ximage->height)
      return 0;
    h = ximage->height - src_y;
  }
  if (w <= 0 || h <= 0)
    return 0;

  /* Assert (d->win */

  if (jwxyz_dumb_drawing_mode(dpy, d, gc, dest_x, dest_y, w, h))
    return 0;

  XGCValues *gcv = gc_gcv (gc);

  Assert (gcv->function == GXcopy, "XPutImage: bad GC function");
  Assert (!ximage->xoffset, "XPutImage: bad xoffset");

  ptrdiff_t
    src_pitch = ximage->bytes_per_line,
    dst_pitch = jwxyz_image_pitch (d);

  const void *src_ptr = SEEK_XY (ximage->data, src_pitch, src_x, src_y);
  void *dst_ptr = SEEK_XY (jwxyz_image_data (d), dst_pitch, dest_x, dest_y);

  if (gcv->alpha_allowed_p) {
    Assert (ximage->depth == 32, "XPutImage: depth != 32");
    Assert (ximage->format == ZPixmap, "XPutImage: bad format");
    Assert (ximage->bits_per_pixel == 32, "XPutImage: bad bits_per_pixel");

    const uint8_t *src_row = src_ptr;
    uint8_t *dst_row = dst_ptr;

    /* Slight loss of precision here: color values may end up being one less
       than what they should be.
     */
    while (h) {
      for (unsigned x = 0; x != w; ++x) {
        // Pixmaps don't contain alpha. (Yay.)
        const uint8_t *src = src_row + x * 4;
        uint8_t *dst = dst_row + x * 4;

        // ####: This is pretty SIMD friendly.
        // Protip: Align dst (load + store), let src be unaligned (load only)
        uint16_t alpha = src[3], alpha1 = 0xff - src[3];
        dst[0] = (src[0] * alpha + dst[0] * alpha1) >> 8;
        dst[1] = (src[1] * alpha + dst[1] * alpha1) >> 8;
        dst[2] = (src[2] * alpha + dst[2] * alpha1) >> 8;
      }

      src_row += src_pitch;
      dst_row += dst_pitch;
      --h;
    }
  } else {
    Assert (ximage->depth == 1 || ximage->depth == 32,
            "XPutImage: depth != 1 && depth != 32");

    if (ximage->depth == 32) {
      Assert (ximage->format == ZPixmap, "XPutImage: bad format");
      Assert (ximage->bits_per_pixel == 32, "XPutImage: bad bits_per_pixel");
      jwxyz_blit (ximage->data, ximage->bytes_per_line, src_x, src_y,
                  jwxyz_image_data (d), jwxyz_image_pitch (d), dest_x, dest_y,
                  w, h);
    } else {
      /* PATCH(xss-sdl): 1-bit XImage -> 32bpp drawable. Expand each bit
         to the GC foreground (1) or background (0), the X semantics for
         XPutImage of a bitmap. barcode draws into 1-bit pixmaps then
         XPutImages them to the screen; without this it was invisible.
         Bits are LSB-first within each byte (jwxyz XImage convention). */
      const XRectangle *frame = jwxyz_frame (d);
      uint32_t fg = (uint32_t) gcv->foreground;
      uint32_t bg = (uint32_t) gcv->background;
      const uint8_t *sdata = (const uint8_t *) ximage->data;
      uint8_t *ddata = (uint8_t *) jwxyz_image_data (d);
      for (int row = 0; row < h; row++) {
        int dy = dest_y + row;
        if (dy < 0 || dy >= frame->height) continue;
        const uint8_t *srow = sdata + (size_t) (src_y + row) * src_pitch;
        uint32_t *drow = (uint32_t *) (ddata + (size_t) dy * dst_pitch);
        for (int col = 0; col < w; col++) {
          int dx = dest_x + col;
          if (dx < 0 || dx >= frame->width) continue;
          int sx = src_x + col;
          int bit = (srow[sx >> 3] >> (sx & 7)) & 1;
          drow[dx] = bit ? fg : bg;
        }
      }
    }
  }

  return 0;
}

static XImage *
GetSubImage (Display *dpy, Drawable d, int x, int y,
             unsigned int width, unsigned int height,
             unsigned long plane_mask, int format,
             XImage *dest_image, int dest_x, int dest_y)
{
  Assert ((width  < 65535), "improbably large width");
  Assert ((height < 65535), "improbably large height");
  Assert ((x < 65535 && x > -65535), "improbably large x");
  Assert ((y < 65535 && y > -65535), "improbably large y");

  if (dest_image->depth == 32 && jwxyz_drawable_depth (d) == 32) {
    Assert (format == ZPixmap, "XGetSubImage: bad format");
    jwxyz_blit (jwxyz_image_data (d), jwxyz_image_pitch (d), x, y,
                dest_image->data, dest_image->bytes_per_line, dest_x, dest_y,
                width, height);
    /* PATCH(xss-sdl): strip the internal opaque-alpha from a read-back
       image. Drawables store black as BlackPixel == alpha_mask
       (0xFF000000) so it composites opaquely, but a returned XImage is
       visible RGB data: leaving alpha set makes "black" non-zero, which
       breaks the common white-on-black readback + `pixel ? 1 : 0`
       threshold (phosphor/apple2 font capture -> every glyph a solid
       block). Zeroing alpha is harmless for GXcopy re-blits (alpha
       ignored) and only matters to alpha_allowed re-blits, which don't
       read pixels back this way. */
    uint32_t amask = (uint32_t)
      DefaultVisualOfScreen (DefaultScreenOfDisplay (dpy))->alpha_mask;
    if (amask) {
      for (unsigned r = 0; r < height; r++) {
        uint32_t *row = (uint32_t *)
          (dest_image->data + (size_t) (dest_y + (int) r)
           * dest_image->bytes_per_line) + dest_x;
        for (unsigned c = 0; c < width; c++)
          row[c] &= ~amask;
      }
    }
    return dest_image;
  }

  /* PATCH(xss-sdl): read back a depth-1 drawable into a 1-bit image.
     Port drawables are stored 32bpp regardless of logical depth (a
     depth-1 pixmap holds 0 / foreground(1) pixels), so pack "pixel != 0"
     into the dest's LSB-first bitmap. phosphor/apple2 capture their font
     glyphs this way (XGetImage(mono_pixmap, ..., XYPixmap)). */
  Assert (dest_image->depth == 1,
          "XGetSubImage: unsupported depth combination");
  {
    const XRectangle *frame = jwxyz_frame (d);
    const uint32_t *sdata = (const uint32_t *) jwxyz_image_data (d);
    ptrdiff_t spitch = jwxyz_image_pitch (d);   /* bytes */
    uint8_t *ddata = (uint8_t *) dest_image->data;
    int dbpl = dest_image->bytes_per_line;
    for (unsigned row = 0; row < height; row++) {
      int sy = y + (int) row;
      const uint32_t *srow =
        (const uint32_t *) ((const uint8_t *) sdata + (size_t) sy * spitch);
      uint8_t *drow = ddata + (size_t) (dest_y + (int) row) * dbpl;
      for (unsigned col = 0; col < width; col++) {
        int sx = x + (int) col;
        int set = (sx >= 0 && sx < frame->width &&
                   sy >= 0 && sy < frame->height && srow[sx] != 0);
        int dx = dest_x + (int) col;
        if (set)
          drow[dx >> 3] |=  (1 << (dx & 7));
        else
          drow[dx >> 3] &= ~(1 << (dx & 7));
      }
    }
  }
  return dest_image;
}


static int
SetClipMask (Display *dpy, GC gc, Pixmap m)
{
  Log ("TODO: No clip masks yet"); // Slip/colorbars.c needs this.
  return 0;
}

static int
SetClipOrigin (Display *dpy, GC gc, int x, int y)
{
  gc->gcv.clip_x_origin = x;
  gc->gcv.clip_y_origin = y;
  return 0;
}


const struct jwxyz_vtbl image_vtbl = {
  root,
  visual,
  display_sources_data,

  window_background,
  draw_arc,
  fill_rects,
  gc_gcv,
  gc_depth,
  jwxyz_draw_string,

  copy_area,

  DrawPoints,
  DrawSegments,
  CreateGC,
  FreeGC,
  ClearWindow,
  SetClipMask,
  SetClipOrigin,
  FillPolygon,
  DrawLines,
  PutImage,
  GetSubImage
};

#endif /* JWXYZ_IMAGE -- entire file */

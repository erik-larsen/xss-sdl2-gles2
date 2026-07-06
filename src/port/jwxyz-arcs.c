/* jwxyz-arcs.c -- XDrawArc/XFillArc (and polygon span fill) for
 * jwxyz's JWXYZ_IMAGE mode, which stubs draw_arc upstream (the GL
 * backend tessellates arcs, but that code is tied to its vertex queue).
 *
 * Strategy: tessellate the (possibly partial, possibly elliptical) arc
 * into a polyline.  Outlines go through XDrawLines; fills become a
 * polygon (pie-slice through the center, as per the X11 default
 * GC arc-mode) rasterized into per-scanline spans emitted as
 * 1-pixel-tall XFillRectangle calls.  Routing through public Xlib
 * calls means GC foreground/clipping behave for free.
 */

#include "config.h"
#include "jwxyzI.h"
#include "jwxyz-sdl.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#define MAX_ARC_PTS 256

static int
arc_points (XPoint *pts, int x, int y,
            unsigned int width, unsigned int height,
            int angle1, int angle2)
{
  double w2 = width * 0.5, h2 = height * 0.5;
  double cx = x + w2, cy = y + h2;

  if (angle2 < 0) { angle1 += angle2; angle2 = -angle2; }
  if (angle2 > 360 * 64) angle2 = 360 * 64;

  double a1 = angle1 * (M_PI / (180.0 * 32.0)) / 2.0;   /* deg64 -> rad */
  double a2 = angle2 * (M_PI / (180.0 * 32.0)) / 2.0;

  int segs = (int) (4.0 * sqrt ((w2 > h2 ? w2 : h2))) + 8;
  segs = (int) (segs * (a2 / (2 * M_PI))) + 2;
  if (segs > MAX_ARC_PTS - 2) segs = MAX_ARC_PTS - 2;

  int n = 0;
  for (int i = 0; i <= segs; i++) {
    double t = a1 + a2 * i / segs;
    /* X11 angles: counterclockwise, 0 = three-o'clock; screen y down. */
    pts[n].x = (short) lrint (cx + w2 * cos (t));
    pts[n].y = (short) lrint (cy - h2 * sin (t));
    n++;
  }
  return n;
}

/* Scanline-fill a polygon by emitting 1px-tall XFillRectangle spans. */
static void
fill_polygon_spans (Display *dpy, Drawable d, GC gc,
                    const XPoint *pts, int npts)
{
  if (npts < 3) return;

  int ymin = pts[0].y, ymax = pts[0].y;
  for (int i = 1; i < npts; i++) {
    if (pts[i].y < ymin) ymin = pts[i].y;
    if (pts[i].y > ymax) ymax = pts[i].y;
  }

  double xs[MAX_ARC_PTS + 4];
  for (int y = ymin; y <= ymax; y++) {
    double yc = y + 0.5;
    int nx = 0;
    for (int i = 0; i < npts; i++) {
      const XPoint *p0 = &pts[i], *p1 = &pts[(i + 1) % npts];
      double y0 = p0->y, y1 = p1->y;
      if ((yc >= y0 && yc < y1) || (yc >= y1 && yc < y0)) {
        double t = (yc - y0) / (y1 - y0);
        xs[nx++] = p0->x + t * (p1->x - p0->x);
      }
    }
    /* insertion sort (nx is tiny) */
    for (int i = 1; i < nx; i++) {
      double v = xs[i]; int j = i - 1;
      while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; }
      xs[j + 1] = v;
    }
    for (int i = 0; i + 1 < nx; i += 2) {
      int x0 = (int) ceil (xs[i] - 0.5);
      int x1 = (int) floor (xs[i + 1] - 0.5);
      if (x1 >= x0)
        XFillRectangle (dpy, d, gc, x0, y, (unsigned) (x1 - x0 + 1), 1);
    }
  }
}

int
jwxyz_image_draw_arc (Display *dpy, Drawable d, GC gc, int x, int y,
                      unsigned int width, unsigned int height,
                      int angle1, int angle2, Bool fill_p)
{
  XPoint pts[MAX_ARC_PTS + 2];
  Bool full = (angle2 >= 360 * 64 || angle2 <= -360 * 64);
  int n = arc_points (pts, x, y, width, height, angle1, angle2);

  if (fill_p) {
    if (!full) {                    /* pie slice through the center */
      pts[n].x = (short) (x + width  / 2);
      pts[n].y = (short) (y + height / 2);
      n++;
    }
    fill_polygon_spans (dpy, d, gc, pts, n);
  } else {
    XDrawLines (dpy, d, gc, pts, n, CoordModeOrigin);
  }
  return 0;
}

int
jwxyz_image_fill_polygon (Display *dpy, Drawable d, GC gc,
                          XPoint *points, int npoints, int shape, int mode)
{
  (void) shape;
  XPoint local[MAX_ARC_PTS];
  if (npoints > MAX_ARC_PTS) npoints = MAX_ARC_PTS;

  /* CoordModePrevious: convert deltas to absolute. */
  for (int i = 0; i < npoints; i++) {
    local[i] = points[i];
    if (mode == CoordModePrevious && i > 0) {
      local[i].x += local[i - 1].x;
      local[i].y += local[i - 1].y;
    }
  }
  fill_polygon_spans (dpy, d, gc, local, npoints);
  return 0;
}

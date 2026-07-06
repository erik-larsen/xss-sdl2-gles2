/* jwxyz-sdl.h -- SDL2 platform layer for jwxyz (JWXYZ_IMAGE mode).
 * The SDL analog of jwxyz-android.h: defines struct jwxyz_Drawable and
 * the handles the port layer shares with the screenhack runner. */

#ifndef __JWXYZ_SDL_H__
#define __JWXYZ_SDL_H__

#include <stddef.h>
#include "jwxyz.h"

struct jwxyz_Drawable {
  enum { WINDOW, PIXMAP } type;
  XRectangle frame;
  void *image_data;             /* 32bpp RGBA, top-down                 */
  ptrdiff_t pitch;              /* bytes per row                        */
  int depth;                    /* pixmaps: 1 or 32; window: 32         */
  Bool own_data;                /* free image_data on XFreePixmap?      */
};

/* Resource database: defaults strings + command line.  Owned by the
 * screenhack runner, queried by get_string_resource(). */
extern void xss_res_clear (void);
extern void xss_res_put (const char *key, const char *val); /* overrides */
extern void xss_res_put_default (const char *spec);  /* "*delay: 10000"  */
extern const char *xss_res_get (const char *name);

/* The window the running hack draws into (port layer needs it for
 * jwxyz_get_pos etc.). */
extern struct jwxyz_Drawable *xss_main_window;

#endif

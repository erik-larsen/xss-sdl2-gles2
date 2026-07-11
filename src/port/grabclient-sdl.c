/* grabclient-sdl.c -- image source for the SDL/web port.
 *
 * The image-grab hacks (decayscreen, distort, ripples, zoom, ...) ask
 * grabclient for a screenshot / random photo to manipulate. Upstream
 * X11 forks xscreensaver-getimage; iOS/Android instead call one platform
 * hook, jwxyz_draw_random_image(), from utils/grabclient.c. We reuse that
 * file and supply the hook here.
 *
 * Returning NULL makes grabclient fall back to SMPTE colour bars (its
 * standard no-image behaviour) drawn into the target drawable -- so the
 * hacks have real content to distort with no bundled photo and no
 * subprocess, which also makes them work in the browser. A bundled image
 * could be decoded here later (jwxyz-png.c + scale-blit) for nicer
 * output; the colour-bars path un-blocks the whole class first.
 */

#include "utils.h"
#include "jwxyz.h"

char *
jwxyz_draw_random_image (Display *dpy, Drawable drawable, GC gc)
{
  (void) dpy; (void) drawable; (void) gc;
  return 0;   /* -> grabclient draws colour bars */
}

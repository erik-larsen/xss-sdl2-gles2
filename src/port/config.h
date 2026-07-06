/* config.h -- SDL2 port of xscreensaver, M1b: 2D (jwxyz-image) hacks only.
 * Modeled on the Android build's define set (android/.../Android.mk),
 * minus Android/JNI/GL.  GL-hack defines (HAVE_GL etc.) arrive in M2. */

#ifndef __XSS_SDL_CONFIG_H__
#define __XSS_SDL_CONFIG_H__

/* STANDALONE etc. are defined on the compiler command line (CMake),
 * because hacks test them before including any header. config.h keeps
 * only what must be textual. */


/* Identifies this port in version strings etc. */
#define XSS_SDL_PORT 1

/* glibc: make struct timezone et al. visible to files that use
 * GETTIMEOFDAY_TWO_ARGS without including <sys/time.h> themselves. */
#include <sys/time.h>

#endif

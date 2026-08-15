/* Stub for Mesa's mesa_wgl.h.
 *
 * The vendored gl.h includes this header unconditionally on MinGW (the
 * `|| defined(__MINGW32__)` arm of its guard), but the file was never
 * part of this gl4es snapshot, which broke the MSYS2 CLANG64 CI build.
 * The wgl* prototypes the real header declares already come from
 * mingw-w64's wingdi.h, pulled in by the windows.h include just above
 * in gl.h -- and this port reaches ANGLE through EGL, so no wgl entry
 * points are used at all.
 */
#ifndef _mesa_wgl_h_
#define _mesa_wgl_h_
/* intentionally empty */
#endif

/* glshim/GLES/gl.h -- xss-sdl shim.
 * screenhackI.h's HAVE_ANDROID branch includes <GLES/gl.h>; we route it
 * to gl4es's full OpenGL 1.x/2.x headers (plus GLU from glues) so the
 * hacks compile their ordinary desktop-GL code paths against gl4es. */
#ifndef __XSS_GLSHIM_GLES_GL_H__
#define __XSS_GLSHIM_GLES_GL_H__
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#endif

/* xss_driver.h -- SDL2/GLES2 driver API for xscreensaver hack ports.
 *
 * The entry-point table deliberately mirrors xscreensaver's
 * struct xscreensaver_function_table (init/draw/reshape/event/free),
 * so that in M1b a jwxyz adapter can wrap real, unmodified hacks
 * behind this same interface.
 *
 * M1a scope: hacks draw into a CPU RGBA framebuffer (the jwxyz-image
 * model); the driver uploads it as a texture and blits one GLES2 quad,
 * following the sgi-demos sdl_framebuffer.c setup pattern.
 */
#ifndef XSS_DRIVER_H
#define XSS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* CPU framebuffer surface handed to 2D hacks each frame.
 * Pixel format: 32bpp, byte order R,G,B,A in memory (GL_RGBA/UNSIGNED_BYTE).
 * Row 0 is the TOP of the window (X11 convention); the blit shader flips. */
typedef struct {
    uint32_t *pixels;     /* width*height pixels, row-major, top-down  */
    int       width;
    int       height;
    int       pitch;      /* in pixels, == width for now               */
} xss_surface;

typedef struct xss_hack {
    const char *name;
    int         gl_p;   /* hack renders via GL directly; skip CPU blit */                            /* e.g. "testpattern"   */
    const char *desc;                            /* one-line description */

    /* Called once after the window/framebuffer exist. Returns hack state. */
    void *(*init)(xss_surface *s);

    /* Called once per frame. Returns requested delay until next frame,
     * in microseconds (same convention as screenhack draw_cb). */
    unsigned long (*draw)(xss_surface *s, void *state);

    /* Window/framebuffer was resized. May be NULL. */
    void (*reshape)(xss_surface *s, void *state);

    /* Free hack state. May be NULL. */
    void (*free)(void *state);
} xss_hack;

/* Run a hack: parses common CLI args, opens window, runs the loop.
 * Common args:
 *   --width N --height N     window size (default 800x600)
 *   --frames N               exit after N frames (testing/harness)
 *   --shot PATH.ppm          write final frame as binary PPM on exit
 *   --fps                    print frames-per-second once per second
 * Returns process exit code. */
int xss_driver_run(const xss_hack *hack, int argc, char **argv);

/* Entry-point helper.
 *
 * On emscripten we must use a no-argument main: clang-15 emits
 * main(argc, argv) under the wasm-ABI name __main_argc_argv, which
 * emscripten 3.1.x's JS glue predates -- it finds no _main export and
 * silently never runs the program. main(void) keeps the classic
 * symbol. CLI-equivalent options come from the page's query string
 * instead (?frames=60&shot=out.ppm&width=800 ...), which
 * xss_web_args() converts into a --key value argv. */
#ifdef __EMSCRIPTEN__
char **xss_web_args(int *argc_ret);   /* parse location.search */
#define XSS_DEFINE_MAIN(hackvar)                                  \
  int main(void) {                                                \
    int argc; char **argv = xss_web_args(&argc);                  \
    return xss_driver_run(&(hackvar), argc, argv);                \
  }
#else
#define XSS_DEFINE_MAIN(hackvar)                                  \
  int main(int argc, char **argv) {                               \
    return xss_driver_run(&(hackvar), argc, argv);                \
  }
#endif

#endif /* XSS_DRIVER_H */

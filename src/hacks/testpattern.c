/* testpattern.c -- pipeline-validation "hack" for the SDL driver.
 *
 * Draws static calibration content (color bars, grayscale ramp, border,
 * center crosshair) so one screenshot verifies geometry, orientation,
 * and channel order -- plus an animated plasma band so the harness's
 * frame-to-frame difference check has something to detect.
 */

#include "xss_driver.h"
#include <math.h>
#include <stdlib.h>

typedef struct { double t; } tp_state;

static inline void put(xss_surface *s, int x, int y,
                       unsigned r, unsigned g, unsigned b)
{
    if (x < 0 || y < 0 || x >= s->width || y >= s->height) return;
    s->pixels[(size_t)y * s->pitch + x] =
        (uint32_t)(0xFFu << 24 | b << 16 | g << 8 | r);   /* RGBA in memory (LE) */
}

static void *tp_init(xss_surface *s)
{
    (void)s;
    tp_state *st = calloc(1, sizeof *st);
    return st;
}

static unsigned long tp_draw(xss_surface *s, void *state)
{
    tp_state *st = state;
    int W = s->width, H = s->height;

    /* SMPTE-ish color bars: top 40% */
    static const unsigned bars[7][3] = {
        {235,235,235},{235,235,16},{16,235,235},{16,235,16},
        {235,16,235},{235,16,16},{16,16,235}
    };
    int bars_h = H * 2 / 5;
    for (int y = 0; y < bars_h; y++)
        for (int x = 0; x < W; x++) {
            int i = x * 7 / W; if (i > 6) i = 6;
            put(s, x, y, bars[i][0], bars[i][1], bars[i][2]);
        }

    /* grayscale ramp: next 20% (verifies no channel swizzle/banding) */
    int ramp_y0 = bars_h, ramp_h = H / 5;
    for (int y = ramp_y0; y < ramp_y0 + ramp_h; y++)
        for (int x = 0; x < W; x++) {
            unsigned v = (unsigned)(255.0 * x / (W - 1));
            put(s, x, y, v, v, v);
        }

    /* animated plasma: bottom 40% (verifies frames change over time) */
    int pl_y0 = ramp_y0 + ramp_h;
    double t = st->t;
    for (int y = pl_y0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double u = (double)x / W * 10.0, v = (double)y / H * 10.0;
            double p = sin(u + t) + sin((v + t) * 0.7)
                     + sin((u + v + t) * 0.5)
                     + sin(sqrt(u * u + v * v) - 2.0 * t);
            unsigned r = (unsigned)(127.5 * (1 + sin(p * M_PI * 0.5)));
            unsigned g = (unsigned)(127.5 * (1 + sin(p * M_PI * 0.5 + 2.094)));
            unsigned b = (unsigned)(127.5 * (1 + sin(p * M_PI * 0.5 + 4.188)));
            put(s, x, y, r, g, b);
        }

    /* 1px red border + center crosshair: verifies extents & orientation
     * (top-left corner gets a 20px green square -- if it shows bottom-left
     * in the window, the y-flip is wrong). */
    for (int x = 0; x < W; x++) { put(s, x, 0, 255,0,0); put(s, x, H-1, 255,0,0); }
    for (int y = 0; y < H; y++) { put(s, 0, y, 255,0,0); put(s, W-1, y, 255,0,0); }
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
            put(s, x, y, 0, 255, 0);
    for (int d = -10; d <= 10; d++) {
        put(s, W/2 + d, H/2, 255,255,255);
        put(s, W/2, H/2 + d, 255,255,255);
    }

    st->t += 0.08;
    return 33333;   /* ~30 fps, in screenhack's microsecond convention */
}

static void tp_free(void *state) { free(state); }

static const xss_hack testpattern_hack = {
    .name = "testpattern",
    .desc = "driver pipeline validation pattern",
    .init = tp_init,
    .draw = tp_draw,
    .reshape = NULL,            /* surface realloc is enough; stateless */
    .free = tp_free,
};

XSS_DEFINE_MAIN(testpattern_hack)

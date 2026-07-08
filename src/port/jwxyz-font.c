/* jwxyz-font.c -- real text rendering for the SDL/jwxyz-image port.
 *
 * Replaces the M1b zero-metric stubs (formerly in jwxyz-sdl.c) with a
 * stb_truetype rasterizer against one embedded font (Liberation Sans,
 * metric-compatible with Helvetica). All font families map to it for
 * now; a monospace face can be added later.
 *
 * The contract (see jwxyz-common.c jwxyz_draw_string): jwxyz_render_text
 * fills an XCharStruct and, when pixmap_ret != 0, allocates a
 * (rbearing-lbearing) x (ascent+descent) array of 32bpp pixels holding
 * the glyph coverage as WHITE (all channels = coverage). jwxyz_draw_string
 * then lifts the green channel into alpha and swaps in the GC foreground,
 * and the image-mode XPutImage alpha-composites it.
 */

#include "config.h"
#include "jwxyz.h"
#include "jwxyzI.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

/* Embedded TTF bytes (generated: cmake/font_data.c from third_party/font). */
extern const unsigned char xss_font_ttf[];
extern const unsigned int  xss_font_ttf_len;

typedef struct {
  stbtt_fontinfo info;
  float scale;
  int   px;                 /* pixel size (nominal em height)            */
  int   ascent, descent;    /* scaled, pixels; descent >= 0 (depth)      */
} xss_font;

/* ---- UTF-8 / Latin-1 decode -------------------------------------- */

/* Decode one codepoint from s[0..len); advance *i. utf8=False treats
   each byte as a Latin-1 codepoint (jwxyz passes pre-decoded bytes). */
static unsigned long
next_codepoint (const unsigned char *s, size_t len, size_t *i, int utf8)
{
  if (*i >= len) return 0;
  unsigned c = s[*i];
  if (!utf8 || c < 0x80) { (*i)++; return c; }

  int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
  unsigned long cp = c & (0x7F >> n);
  (*i)++;
  for (int k = 0; k < n && *i < len && (s[*i] & 0xC0) == 0x80; k++) {
    cp = (cp << 6) | (s[*i] & 0x3F);
    (*i)++;
  }
  return cp;
}

/* ---- API --------------------------------------------------------- */

const char *
jwxyz_default_font_family (int require)
{
  switch (require) {
  case JWXYZ_FONT_FAMILY & JWXYZ_STYLE_MONOSPACE: return "Courier";
  default:                                        return "Helvetica";
  }
}

void *
jwxyz_load_native_font (Window main_window,
                        int traits_jwxyz, int mask_jwxyz,
                        const char *font_name_ptr, size_t font_name_length,
                        int font_name_type, float size,
                        char **family_name_ret,
                        int *ascent_ret, int *descent_ret)
{
  (void) main_window; (void) traits_jwxyz; (void) mask_jwxyz;
  (void) font_name_ptr; (void) font_name_length; (void) font_name_type;

  xss_font *f = calloc (1, sizeof (*f));
  if (!f) return 0;

  if (!stbtt_InitFont (&f->info, xss_font_ttf,
                       stbtt_GetFontOffsetForIndex (xss_font_ttf, 0))) {
    free (f);
    return 0;
  }

  f->px = (int) (size > 0 ? size + 0.5f : 12);
  if (f->px < 1) f->px = 1;
  f->scale = stbtt_ScaleForPixelHeight (&f->info, (float) f->px);

  int a, d, g;
  stbtt_GetFontVMetrics (&f->info, &a, &d, &g);
  f->ascent  = (int) (a * f->scale + 0.5f);
  f->descent = (int) (-d * f->scale + 0.5f);

  if (family_name_ret) *family_name_ret = strdup ("Liberation Sans");
  if (ascent_ret)  *ascent_ret  = f->ascent;
  if (descent_ret) *descent_ret = f->descent;
  return f;
}

void
jwxyz_release_native_font (Display *dpy, void *native_font)
{
  (void) dpy;
  free (native_font);
}

void
jwxyz_render_text (Display *dpy, void *native_font,
                   const char *str, size_t len, Bool utf8, Bool antialias_p,
                   XCharStruct *cs_ret, char **pixmap_ret)
{
  (void) dpy; (void) antialias_p;
  xss_font *f = native_font;
  const unsigned char *s = (const unsigned char *) str;

  /* Pass 1: lay out glyphs on the baseline, accumulate the ink bbox and
     the total advance. Pen position is tracked in floating pixels so
     sub-pixel advances don't drift. */
  float pen = 0;
  int ink_x0 = 0, ink_y0 = 0, ink_x1 = 0, ink_y1 = 0;
  int have_ink = 0;
  size_t i = 0;
  unsigned long prev = 0;
  while (i < len) {
    unsigned long cp = next_codepoint (s, len, &i, utf8);
    if (!cp) break;
    int g = stbtt_FindGlyphIndex (&f->info, (int) cp);

    if (prev)
      pen += stbtt_GetGlyphKernAdvance (&f->info, (int) prev, g) * f->scale;

    int adv, lsb;
    stbtt_GetGlyphHMetrics (&f->info, g, &adv, &lsb);

    int gx0, gy0, gx1, gy1;
    stbtt_GetGlyphBitmapBox (&f->info, g, f->scale, f->scale,
                             &gx0, &gy0, &gx1, &gy1);
    if (gx1 > gx0 && gy1 > gy0) {
      int px0 = (int) (pen) + gx0;
      int px1 = (int) (pen) + gx1;
      if (!have_ink) {
        ink_x0 = px0; ink_x1 = px1; ink_y0 = gy0; ink_y1 = gy1;
        have_ink = 1;
      } else {
        if (px0 < ink_x0) ink_x0 = px0;
        if (px1 > ink_x1) ink_x1 = px1;
        if (gy0 < ink_y0) ink_y0 = gy0;
        if (gy1 > ink_y1) ink_y1 = gy1;
      }
    }
    pen += adv * f->scale;
    prev = cp;
  }

  int width = (int) (pen + 0.5f);

  if (cs_ret) {
    memset (cs_ret, 0, sizeof (*cs_ret));
    cs_ret->width = width;
    if (have_ink) {
      cs_ret->lbearing = ink_x0;
      cs_ret->rbearing = ink_x1;
      cs_ret->ascent   = -ink_y0;
      cs_ret->descent  = ink_y1;
    }
    /* No ink (empty / all-whitespace): leave l/rbearing 0 so
       jwxyz_draw_string's w==0 short-circuit fires, but width (advance)
       is still reported for text-extent queries. */
  }

  if (!pixmap_ret) return;
  *pixmap_ret = 0;
  if (!have_ink) return;

  int W = ink_x1 - ink_x0, H = ink_y1 - ink_y0;
  if (W <= 0 || H <= 0) return;

  unsigned char *cov = calloc ((size_t) W * H, 1);
  if (!cov) return;

  /* Pass 2: rasterize each glyph's coverage into the bbox. */
  pen = 0;
  i = 0;
  prev = 0;
  while (i < len) {
    unsigned long cp = next_codepoint (s, len, &i, utf8);
    if (!cp) break;
    int g = stbtt_FindGlyphIndex (&f->info, (int) cp);

    if (prev)
      pen += stbtt_GetGlyphKernAdvance (&f->info, (int) prev, g) * f->scale;

    int gx0, gy0, gx1, gy1;
    stbtt_GetGlyphBitmapBox (&f->info, g, f->scale, f->scale,
                             &gx0, &gy0, &gx1, &gy1);
    if (gx1 > gx0 && gy1 > gy0) {
      int dx = (int) (pen) + gx0 - ink_x0;
      int dy = gy0 - ink_y0;
      if (dx >= 0 && dy >= 0 && dx + (gx1 - gx0) <= W && dy + (gy1 - gy0) <= H)
        stbtt_MakeGlyphBitmap (&f->info, cov + (size_t) dy * W + dx,
                               gx1 - gx0, gy1 - gy0, W,
                               f->scale, f->scale, g);
    }
    int adv, lsb;
    stbtt_GetGlyphHMetrics (&f->info, g, &adv, &lsb);
    pen += adv * f->scale;
    prev = cp;
  }

  /* Expand coverage -> 32bpp white (every channel = coverage). */
  uint32_t *out = malloc ((size_t) W * H * 4);
  if (!out) { free (cov); return; }
  for (size_t p = 0; p < (size_t) W * H; p++) {
    uint32_t c = cov[p];
    out[p] = c | (c << 8) | (c << 16) | (c << 24);
  }
  free (cov);
  *pixmap_ret = (char *) out;
}

char *
jwxyz_unicode_character_name (Display *dpy, Font fid, unsigned long uc)
{
  (void) dpy; (void) fid;
  char buf[32];
  snprintf (buf, sizeof buf, "U+%04lX", uc);
  return strdup (buf);
}

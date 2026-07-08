/* textclient-sdl.c -- text source for the SDL/web port.
 *
 * The textclient hacks (phosphor, apple2, xmatrix, starwars, fontglide,
 * gltext, ...) stream characters from textclient_getc(). Upstream Unix
 * pipes from a subprocess (xscreensaver-text); iOS/Android instead call
 * two platform hooks from utils/textclient-mobile.c. We reuse that file
 * (its column wrapping and streaming) and provide those two hooks here:
 * no subprocess, no network -- just bundled public-domain text, which is
 * also what makes these hacks work in the browser.
 *
 * textclient_mobile_date_string() is the source for the default "date"
 * textMode; we return the current date/time followed by a rotating
 * public-domain passage, so the hacks show flowing prose out of the box.
 * textclient_mobile_url_string() is unused (URL mode falls back to date).
 */

#include "utils.h"
#include "textclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Short public-domain passages, returned in rotation. */
static const char *const passages[] = {
  "Call me Ishmael. Some years ago -- never mind how long precisely --\n"
  "having little or no money in my purse, and nothing particular to\n"
  "interest me on shore, I thought I would sail about a little and see\n"
  "the watery part of the world.\n",

  "It was the best of times, it was the worst of times, it was the age\n"
  "of wisdom, it was the age of foolishness, it was the epoch of belief,\n"
  "it was the epoch of incredulity, it was the season of Light, it was\n"
  "the season of Darkness.\n",

  "Two roads diverged in a yellow wood,\n"
  "And sorry I could not travel both\n"
  "And be one traveler, long I stood\n"
  "And looked down one as far as I could\n"
  "To where it bent in the undergrowth.\n",

  "The quick brown fox jumps over the lazy dog.\n"
  "Pack my box with five dozen liquor jugs.\n"
  "How vexingly quick daft zebras jump!\n"
  "The five boxing wizards jump quickly.\n",

  "In the beginning the Universe was created. This has made a lot of\n"
  "people very angry and been widely regarded as a bad move. Space is\n"
  "big. Really big. You just won't believe how vastly hugely mind-\n"
  "bogglingly big it is.\n",
};
#define NPASSAGES ((int) (sizeof (passages) / sizeof (passages[0])))

char *
textclient_mobile_date_string (void)
{
  static int which = 0;

  time_t now = time (0);
  char date[64];
  struct tm *tm = localtime (&now);
  if (!tm || !strftime (date, sizeof date, "%a %b %e %H:%M:%S %Y", tm))
    date[0] = 0;

  const char *body = passages[which];
  which = (which + 1) % NPASSAGES;

  size_t n = strlen (date) + 2 + strlen (body) + 2;
  char *s = malloc (n);
  if (!s) return 0;
  snprintf (s, n, "%s\n\n%s\n", date, body);
  return s;
}

char *
textclient_mobile_url_string (Display *dpy, const char *url)
{
  (void) dpy; (void) url;
  return 0;   /* no network; URL mode falls back to date */
}

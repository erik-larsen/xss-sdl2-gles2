
/* Copyright (c) Mark J. Kilgard, 1995. */

/* This program is freely distributable without licensing fees 
   and is provided without guarantee or warrantee expressed or 
   implied. This program is -not- in the public domain. */

#if 0   /* for Mesa */
# include "glutint.h"
#else   /* for xscreensaver */
# include "screenhackI.h"
# undef APIENTRY
# define APIENTRY /**/
#endif

#include "glutstroke.h"

/* CENTRY */
int APIENTRY 
glutStrokeWidth(GLUTstrokeFont font, int c)
{
  StrokeFontPtr fontinfo;
  const StrokeCharRec *ch;

  /* PATCH(xss-sdl): the _WIN32 arm here mapped the font id through
   * GLUT's internal __glutFont(), which lives in glutint.h -- the
   * header this tree does not build (see the #if 0 at the top of
   * glut_swidth.c). In xscreensaver a GLUTstrokeFont already IS the
   * StrokeFontPtr, on every platform. */
  fontinfo = (StrokeFontPtr) font;

  if (c < 0 || c >= fontinfo->num_chars)
    return 0;
  ch = &(fontinfo->ch[c]);
  if (ch)
    return ch->right;
  else
    return 0;
}

int APIENTRY 
glutStrokeLength(GLUTstrokeFont font, const unsigned char *string)
{
  int c, length;
  StrokeFontPtr fontinfo;
  const StrokeCharRec *ch;

  /* PATCH(xss-sdl): the _WIN32 arm here mapped the font id through
   * GLUT's internal __glutFont(), which lives in glutint.h -- the
   * header this tree does not build (see the #if 0 at the top of
   * glut_swidth.c). In xscreensaver a GLUTstrokeFont already IS the
   * StrokeFontPtr, on every platform. */
  fontinfo = (StrokeFontPtr) font;

  length = 0;
  for (; *string != '\0'; string++) {
    c = *string;
    if (c >= 0 && c < fontinfo->num_chars) {
      ch = &(fontinfo->ch[c]);
      if (ch)
        length += ch->right;
    }
  }
  return length;
}

/* ENDCENTRY */

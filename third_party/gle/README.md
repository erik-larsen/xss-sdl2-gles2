# GLE — Tubing and Extrusion Library

Vendored for the `extrusion` hack, which is the only thing in
xscreensaver that uses it. Upstream builds it against a system `libgle`;
this tree has no system GL at all, so GLE is compiled here as a static
library against gl4es + the vendored glues, like everything else.

- Upstream: https://github.com/linas/glextrusion (master, Aug 2026)
- Author: Linas Vepstas
- Files: `src/` (the library proper) and `src/GL/gle.h` (its public
  header — xscreensaver's `extrusion.h` reaches it via `HAVE_GLE3`).
  The build tree, docs, examples and the C++ port are not vendored.

## License

GLE's `COPYING` offers a choice: the source code is under the IBM
standard example source code license (free use, distribution and
modification, disclaimer of warranty — see `COPYING.src`), or, at the
recipient's option, the GPL. This tree takes the former, which is what
lets it sit alongside the Apache-2.0 scaffolding.

## Local changes

Marked `PATCH(xss-sdl)` in the source:

- `src/port.h`: always include `<GL/gl.h>` / `<GL/glu.h>` rather than
  Apple's framework headers on Darwin — here those come from gl4es and
  glues on every platform.

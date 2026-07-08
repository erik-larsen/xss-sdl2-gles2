# Bundled font

`LiberationSans-subset.ttf` is the single font embedded into every hack
binary (native and wasm) for jwxyz text rendering (see
`src/port/jwxyz-font.c`, which rasterizes it with stb_truetype). All font
families that hacks request map to it for now; a monospace face can be
added later.

## Provenance

Derived from **Liberation Sans Regular** 2.1.5 (metric-compatible with
Helvetica/Arial), subset with `pyftsubset` to keep the per-binary size
small — the full face is ~410 KB and lands in all ~200 web binaries:

    pyftsubset LiberationSans-Regular.ttf \
      --output-file=LiberationSans-subset.ttf \
      --unicodes="U+0020-007E,U+00A0-00FF,U+0100-017F,U+0180-024F,\
U+0370-03FF,U+0400-04FF,U+2000-206F,U+20A0-20BF,U+2100-214F,\
U+2190-21FF,U+2200-22FF,U+2500-257F,U+25A0-25FF,U+2600-26FF" \
      --layout-features='*' --glyph-names --no-hinting

That is Basic Latin, Latin-1, Latin Extended-A/B, Greek, Cyrillic,
general punctuation, currency, letterlike symbols, arrows, math
operators, box drawing, geometric shapes, and misc symbols (~1100
glyphs, ~97 KB) — enough for the text hacks and for `unicrud` to show
interesting glyphs. Regenerate from the upstream 2.1.5 release if the
range needs widening.

## License

Liberation fonts are SIL Open Font License 1.1 — see
`LICENSE.liberation`. The OFL permits bundling and redistribution
(including in this GPL project); the subset remains under the OFL.

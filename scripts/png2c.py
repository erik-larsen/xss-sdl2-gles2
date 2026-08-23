#!/usr/bin/env python3
"""Embed a binary file as a C string literal, upstream ad2c style.

  python3 scripts/png2c.py <infile> <symbol> <outfile.h>

xscreensaver's images/gen/*.h headers are produced by utils/ad2c during
its build: one octal-escaped C string per file, wrapped in the __extension__
guard that quiets "string longer than ISO C requires". This is the same
transform, so headers generated here are byte-identical to upstream's and
sit alongside the ones already vendored in hacks/images/gen/.
"""
import sys

# ? is escaped to keep the preprocessor from seeing a trigraph.
LITERAL = {c for c in range(0x20, 0x7f)} - {0x22, 0x5c, 0x3f}


def c_string(data):
    out = []
    for b in data:
        out.append(chr(b) if b in LITERAL else "\\%03o" % b)
    return "".join(out)


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: png2c.py <infile> <symbol> <outfile.h>")
    infile, symbol, outfile = sys.argv[1:4]
    data = open(infile, "rb").read()
    with open(outfile, "w") as f:
        f.write("#ifdef __GNUC__\n__extension__\n#endif\n")
        f.write("static const unsigned char %s[] =\n" % symbol)
        f.write(' "%s";\n' % c_string(data))


if __name__ == "__main__":
    main()

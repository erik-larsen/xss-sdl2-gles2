#!/usr/bin/env python3
"""Embed text files as C string literals, upstream ad2c style.

  python3 scripts/gen_ad2c.py [--strip-asm-comments] <out.h> <in>...

Upstream builds molecules.h (hacks/glx/molecules.sh) and m6502.h
(hacks/m6502.sh) this way: utils/ad2c piped through sed, so that every
line of every input becomes a C string literal, adjacent literals
concatenate into one string per file, and the files are separated by
commas. The hack then says

    static const char * const builtin_pdb_data[] = {
    # include "molecules.h"
    };

and gets one array element per input file. Same output, without the
shell. --strip-asm-comments drops "; ..." comments first, which is what
m6502.sh does to keep the embedded assembly small.
"""
import re
import sys

def c_escape(line):
    return line.replace('\\', '\\\\').replace('"', '\\"')

def main(target, sources, strip_comments=False):
    out = []
    for src in sources:
        with open(src, encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.rstrip('\n').rstrip('\r')
                if strip_comments:
                    line = re.sub(r'[ \t]*;.*$', '', line)
                out.append(' "%s\\n"\n' % c_escape(line))
        out.append(',\n')
    text = ''.join(out)
    try:
        if open(target, encoding='utf-8').read() == text:
            return          # don't touch the timestamp for no reason
    except OSError:
        pass
    with open(target, 'w', encoding='utf-8') as f:
        f.write(text)

if __name__ == '__main__':
    args = sys.argv[1:]
    strip = '--strip-asm-comments' in args
    args = [a for a in args if a != '--strip-asm-comments']
    main(args[0], sorted(args[1:]), strip)

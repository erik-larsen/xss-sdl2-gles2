#!/usr/bin/env python3
"""Build molecules.h -- the built-in PDB data molecule.c embeds.

Upstream does this with hacks/glx/molecules.sh (utils/ad2c piped through
sed): every line of every .pdb becomes a C string literal, adjacent
literals concatenate into one string per file, and the files are
separated by commas so that molecule.c's

    static const char * const builtin_pdb_data[] = {
    # include "molecules.h"
    };

sees one array element per molecule. Same output, without the shell.
"""
import sys

def c_escape(line):
    return line.replace('\\', '\\\\').replace('"', '\\"')

def main(target, sources):
    out = []
    for src in sources:
        with open(src, encoding='utf-8', errors='replace') as f:
            for line in f:
                out.append(' "%s\\n"\n' % c_escape(line.rstrip('\n').rstrip('\r')))
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
    main(sys.argv[1], sorted(sys.argv[2:]))

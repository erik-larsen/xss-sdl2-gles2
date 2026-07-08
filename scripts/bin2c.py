#!/usr/bin/env python3
"""Embed a binary file as a C byte array.

  python3 scripts/bin2c.py <infile> <symbol> <outfile.c>

Emits:
  const unsigned char <symbol>[] = { ... };
  const unsigned int  <symbol>_len = <N>;
"""
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: bin2c.py <infile> <symbol> <outfile.c>")
    infile, symbol, outfile = sys.argv[1], sys.argv[2], sys.argv[3]
    data = open(infile, "rb").read()
    with open(outfile, "w") as f:
        f.write(f"/* generated from {infile} by scripts/bin2c.py -- "
                f"do not edit */\n")
        f.write(f"const unsigned char {symbol}[] = {{\n")
        for i in range(0, len(data), 20):
            f.write("  " + ",".join(str(b) for b in data[i:i+20]) + ",\n")
        f.write("};\n")
        f.write(f"const unsigned int {symbol}_len = {len(data)};\n")
    print(f"{outfile}: {len(data)} bytes -> {symbol}")


if __name__ == "__main__":
    main()

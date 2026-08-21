#!/usr/bin/env python3
"""Generate per-hack option schemas for the web settings panel.

  python3 scripts/gen_options.py [--webdir web]

Parses each shipping hack's upstream config XML
(third_party/xscreensaver/hacks/config/<hack>.xml) into a compact JSON
schema written to <webdir>/<hack>/options.json, which the shell
(src/web/shell.html) renders as a settings drawer.  The drawer emits a
query string (?delay=30000&wireframe) that xss_web_args() turns back
into hack argv.

Schema: {"label": "Gears", "controls": [
  {"t":"slider","key":"delay","label":"Frame rate","low":0,
   "high":100000,"def":30000,"conv":"invert","lowlab":"Low",
   "highlab":"High","int":1},
  {"t":"spin", ... same fields, no lowlab/highlab },
  {"t":"bool","label":"Wireframe","on":["wireframe",null],"off":null,
   "def":0},                       # on/off = [key, value?] emitted in
                                   # that checkbox state; def = initial
  {"t":"sel","label":"Mode","opts":[["Random",null,null],
   ["Ball","mode","ball"], ...]},  # [label, key, value]; null key =
                                   # the default option (no arg)
  {"t":"str","key":"pattern","label":"Pattern"}]}

Controls the port has no use for are skipped: <xscreensaver-image>
(bundled colour-bars grabclient), <xscreensaver-text> (bundled text),
<file>, <command>, <video>, <xscreensaver-updater>.
"""

import argparse, json, os, sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_gallery import registered_hacks  # noqa: E402

# executables named after the source file, XML after the module
XML_ALIAS = {"b_lockglue": "bubble3d", "sproingiewrap": "sproingies"}

SKIP_TAGS = {"command", "video", "xscreensaver-updater",
             "xscreensaver-image", "xscreensaver-text", "file",
             "_description"}


def parse_arg(spec):
    """'--mode ball' -> ('mode', 'ball'); '--wireframe' -> ('wireframe',
    None).  Values may be single-quoted ('--text \\'%A %d\\'')."""
    spec = spec.strip()
    key, _, val = spec.partition(" ")
    key = key.lstrip("-")
    val = val.strip().strip("'") or None
    return key, val


def num_value(s):
    f = float(s)
    return int(f) if f == int(f) else f


def walk(el, out):
    for child in el:
        tag = child.tag
        if tag in SKIP_TAGS:
            continue
        if tag in ("hgroup", "vgroup"):
            walk(child, out)
        elif tag == "number":
            key, _ = parse_arg(child.get("arg", ""))
            if not key or "%" not in child.get("arg", ""):
                continue
            low = num_value(child.get("low"))
            high = num_value(child.get("high"))
            dflt = num_value(child.get("default"))
            c = {"t": "slider" if child.get("type") == "slider" else "spin",
                 "key": key,
                 "label": child.get("_label") or child.get("id") or key,
                 "low": low, "high": high, "def": dflt,
                 "int": 1 if all(isinstance(v, int)
                                 for v in (low, high, dflt)) else 0}
            if child.get("convert"):
                c["conv"] = child.get("convert")
            if child.get("type") == "slider":
                if child.get("_low-label"):
                    c["lowlab"] = child.get("_low-label")
                if child.get("_high-label"):
                    c["highlab"] = child.get("_high-label")
            out.append(c)
        elif tag == "boolean":
            on = parse_arg(child.get("arg-set")) if child.get("arg-set") else None
            off = (parse_arg(child.get("arg-unset"))
                   if child.get("arg-unset") else None)
            if not on and not off:
                continue
            out.append({"t": "bool",
                        "label": child.get("_label")
                        or child.get("id") or "",
                        "on": list(on) if on else None,
                        "off": list(off) if off else None,
                        # arg-unset => the option defaults to enabled
                        "def": 1 if off else 0})
        elif tag == "select":
            opts = []
            for o in child.findall("option"):
                if o.get("arg-set"):
                    k, v = parse_arg(o.get("arg-set"))
                else:
                    k, v = None, None
                opts.append([o.get("_label") or o.get("id") or "", k, v])
            if opts:
                out.append({"t": "sel",
                            "label": child.get("_label")
                            or child.get("id") or "",
                            "opts": opts})
        elif tag == "string":
            key, _ = parse_arg(child.get("arg", ""))
            if key:
                out.append({"t": "str", "key": key,
                            "label": child.get("_label")
                            or child.get("id") or key})


def hack_schema(name):
    path = os.path.join(ROOT, "third_party", "xscreensaver", "hacks",
                        "config", f"{XML_ALIAS.get(name, name)}.xml")
    if not os.path.exists(path):
        return None
    root = ET.parse(path).getroot()
    controls = []
    walk(root, controls)
    return {"label": root.get("_label", name), "controls": controls}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--webdir", default=os.path.join(ROOT, "web"))
    args = ap.parse_args()

    n = 0
    for name in sorted(registered_hacks()):
        schema = hack_schema(name)
        if schema is None:
            print(f"warning: no config XML for {name}", file=sys.stderr)
            continue
        d = os.path.join(args.webdir, name)
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, "options.json"), "w") as f:
            json.dump(schema, f, ensure_ascii=False,
                      separators=(",", ":"))
        n += 1
    print(f"options: {n} schemas -> {args.webdir}/<hack>/options.json")


if __name__ == "__main__":
    sys.exit(main())

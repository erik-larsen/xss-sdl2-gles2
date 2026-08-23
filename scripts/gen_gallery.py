#!/usr/bin/env python3
"""Generate the web gallery: thumbnail grid of all hacks (rss-sdl2-gles2
style) with auto-screenshot thumbs from the harness.

  python3 scripts/gen_gallery.py [--shotdir /tmp/xss_harness]
                                 [--webdir web] [--thumb-width 480]

- Thumbnails: <shotdir>/<hack>-1.ppm (written by tests/harness.py) ->
  <webdir>/shots/<hack>.jpg
- Hack list + 2D/GL class: parsed from CMakeLists.txt +
  cmake/batch2-glhacks.cmake registrations.
- Status: tests/STATUS.csv; only hacks with status 'pass' (or 'static')
  get cards by default.
- Cards link to <hack>/<hack>.html (the emscripten build, deployed by
  scripts/deploy-web.sh). Cards whose build is missing from <webdir>
  are rendered dimmed and unlinked, so the gallery can ship before the
  full wasm build does.
- Card metadata (pretty label, one-line description, author/year):
  parsed from the vendored upstream config XMLs
  (third_party/xscreensaver/hacks/config/<hack>.xml).
"""

import argparse, csv, glob, json, os, re, sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def registered_hacks():
    """-> {name: '2D'|'GL'} from the CMake registrations."""
    hacks = {}
    # Every CMake file that can register a hack: the top-level one plus
    # each generated batch under cmake/ (globbed, so a new batch file
    # shows up in the gallery without editing this script).
    sources = [os.path.join(ROOT, "CMakeLists.txt")]
    sources += sorted(glob.glob(os.path.join(ROOT, "cmake", "*.cmake")))
    for f in sources:
        text = open(f).read()
        for m in re.finditer(r"xss_add_xhack\((\w+)", text):
            hacks[m.group(1)] = "2D"
        for m in re.finditer(r"xss_add_glhack(?:_p)?\((\w+)", text):
            hacks[m.group(1)] = "GL"
    # foreach-list registrations: set(XSS_2D_<anything> a b c)
    text = open(os.path.join(ROOT, "CMakeLists.txt")).read()
    for m in re.finditer(r"set\((XSS_2D_\w+)\s+(.*?)\)", text, re.S):
        # strip inline comments (e.g. "# noseguy needs assets")
        body = re.sub(r"#[^\n]*", "", m.group(2))
        for n in body.split():
            hacks[n] = "2D"
    hacks["polyhedra"] = "GL"          # manual registration
    hacks.pop("name", None)            # function definitions, not hacks
    return hacks


def hack_metadata(name):
    """-> {label, desc, author, year} from the upstream config XML.

    desc is the first prose paragraph of <_description> (URL-only lines
    dropped); author/year come from its trailing "Written by X; YEAR."
    Missing XML or fields -> empty strings.
    """
    meta = {"label": "", "desc": "", "author": "", "year": ""}
    # executables named after the source file, XML after the module
    xmlname = {"b_lockglue": "bubble3d",
               "sproingiewrap": "sproingies"}.get(name, name)
    path = os.path.join(ROOT, "third_party", "xscreensaver", "hacks",
                        "config", f"{xmlname}.xml")
    if not os.path.exists(path):
        return meta
    root = ET.parse(path).getroot()
    meta["label"] = root.get("_label", "")
    dnode = root.find("_description")
    text = (dnode.text or "") if dnode is not None else ""
    paras = [p.strip() for p in re.split(r"\n\s*\n", text) if p.strip()]
    m = re.search(r"Written by\s+(.+?);\s*([0-9][-0-9,; ]*[0-9]|[0-9]{4})\.",
                  text, re.S)
    if m:
        meta["author"] = re.sub(r"\s+", " ", m.group(1)).strip()
        meta["year"] = m.group(2).strip()
    for p in paras:
        p = re.sub(r"\s+", " ", p)
        # skip URL-only lines, the author line, and the "removed from
        # the distribution as of 5.x" boilerplate on retired hacks
        if re.match(r"(https?://|Written by)", p) or \
           "was removed from the XScreenSaver distribution" in p:
            continue
        # drop trailing URLs / "Written by" if glued to the same paragraph
        p = re.split(r"\s*(?:https?://\S+|Written by .*)$", p)[0].strip()
        if p:
            meta["desc"] = p
            break
    if not meta["desc"] and paras:
        # only the retirement boilerplate exists (juggle, thornbird);
        # keep its informative tail ("It has been replaced by ...")
        p = re.sub(r"\s+", " ", paras[0])
        p = re.sub(r"^This screen saver was removed.*?version \d+\.\d+\.\s*",
                   "", p)
        meta["desc"] = re.split(r"\s*Written by .*$", p)[0].strip()
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shotdir", default="/tmp/xss_harness")
    ap.add_argument("--webdir", default=os.path.join(ROOT, "web"))
    ap.add_argument("--thumb-width", type=int, default=480)
    ap.add_argument("--include-failing", action="store_true")
    args = ap.parse_args()

    from PIL import Image

    status = {}
    scsv = os.path.join(ROOT, "tests", "STATUS.csv")
    if os.path.exists(scsv):
        for r in csv.DictReader(open(scsv)):
            status[r["hack"]] = r["status"]

    hacks = registered_hacks()
    shown = []
    os.makedirs(os.path.join(args.webdir, "shots"), exist_ok=True)
    for name, klass in sorted(hacks.items()):
        st = status.get(name, "unknown")
        if st not in ("pass", "static") and not args.include_failing:
            continue
        src = os.path.join(args.shotdir, f"{name}-1.ppm")
        thumb = os.path.join(args.webdir, "shots", f"{name}.jpg")
        if os.path.exists(src):
            im = Image.open(src)
            w = args.thumb_width
            im = im.resize((w, w * im.height // im.width))
            im.convert("RGB").save(thumb, quality=85)
        elif not os.path.exists(thumb):
            print(f"warning: no screenshot for {name}", file=sys.stderr)
        built = os.path.exists(os.path.join(args.webdir, name, f"{name}.html"))
        meta = hack_metadata(name)
        if not meta["label"]:
            print(f"warning: no config XML metadata for {name}",
                  file=sys.stderr)
        shown.append((name, klass, built, meta))

    items = ",\n    ".join(
        json.dumps([n, k, 1 if b else 0, m["label"] or n, m["desc"],
                    m["author"], m["year"]], ensure_ascii=False)
        for n, k, b, m in shown)
    html = TEMPLATE.replace("/*HACKS*/", items) \
                   .replace("/*COUNT*/", str(len(shown)))
    with open(os.path.join(args.webdir, "index.html"), "w") as f:
        f.write(html)
    nb = sum(1 for _, _, b, _ in shown if b)
    print(f"gallery: {len(shown)} hacks ({nb} with web builds) -> "
          f"{args.webdir}/index.html")


TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>xscreensaver &mdash; WebAssembly</title>
<style>
  :root { color-scheme: dark; --bg:#0a0a0f; --card:#14141c; --ink:#e8e8f0; --dim:#8a8a9a; --accent:#6cf; }
  * { box-sizing:border-box; }
  html,body { margin:0; padding:0; background:var(--bg); color:var(--ink);
    font-family:-apple-system,system-ui,"Segoe UI",sans-serif; -webkit-font-smoothing:antialiased; }
  header { padding:48px 24px 8px; text-align:center; }
  h1 { margin:0 0 8px; font-size:28px; font-weight:600; letter-spacing:-.01em; }
  header p { margin:4px auto; color:var(--dim); font-size:14px; line-height:1.5; max-width:640px; }
  header a { color:var(--accent); text-decoration:none; }
  .filters { text-align:center; padding:8px; }
  .filters button { background:var(--card); color:var(--ink); border:1px solid #1f1f2a;
    border-radius:8px; padding:6px 14px; margin:0 4px; font-size:13px; cursor:pointer; }
  .filters button.on { border-color:var(--accent); color:var(--accent); }
  .grid { display:grid; gap:14px; padding:24px; max-width:1400px; margin:0 auto;
    grid-template-columns:repeat(auto-fill,minmax(210px,1fr)); }
  .card { background:var(--card); border-radius:12px; overflow:hidden; text-decoration:none;
    color:inherit; border:1px solid #1f1f2a; transition:transform .15s,border-color .15s;
    display:flex; flex-direction:column; }
  a.card:hover { transform:translateY(-3px); border-color:var(--accent); }
  .card.nobuild { opacity:.45; }
  .thumb { aspect-ratio:4/3; background:#000 center/cover no-repeat; }
  .meta { padding:10px 12px 4px; display:flex; align-items:baseline; justify-content:space-between; }
  .name { font-size:14px; font-weight:600; }
  .tag { font-size:10px; color:var(--dim); border:1px solid #2a2a3a; padding:1px 6px; border-radius:6px; white-space:nowrap; }
  .desc { padding:0 12px; color:var(--dim); font-size:12px; line-height:1.45; flex:1;
    display:-webkit-box; -webkit-line-clamp:2; -webkit-box-orient:vertical; overflow:hidden; }
  .by { padding:6px 12px 10px; color:var(--dim); font-size:11px; opacity:.75;
    white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
  footer { text-align:center; color:var(--dim); font-size:12px; padding:24px; line-height:1.6; }
</style>
</head>
<body>
<header>
  <h1>xscreensaver</h1>
  <p>Jamie Zawinski's <a href="https://www.jwz.org/xscreensaver/">xscreensaver</a> hacks,
     ported to SDL2 + OpenGL&nbsp;ES&nbsp;2 and compiled to WebAssembly.
     2D hacks render via jwxyz; GL hacks via gl4es (OpenGL&nbsp;&rarr;&nbsp;GLES2) on WebGL.</p>
  <p style="font-size:12px;margin-top:10px;">/*COUNT*/ hacks &middot; click to run &middot;
     press <strong>Esc</strong> or <strong>Q</strong> to exit.</p>
</header>
<div class="filters">
  <button data-f="all" class="on">all</button>
  <button data-f="GL">GL</button>
  <button data-f="2D">2D</button>
</div>
<div class="grid" id="grid"></div>
<footer>
  xscreensaver &copy; Jamie Zawinski et al. &middot;
  ported with jwxyz + gl4es + glues + SDL2 + WebGL
</footer>
<script>
  var hacks = [
    /*HACKS*/
  ];
  var g = document.getElementById('grid');
  function esc(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
  hacks.forEach(function(h){
    var name=h[0], klass=h[1], built=h[2], label=h[3], desc=h[4], author=h[5], year=h[6];
    var el=document.createElement(built?'a':'div');
    el.className='card'+(built?'':' nobuild'); el.dataset.k=klass;
    if(built) el.href=name+'/'+name+'.html';
    if(desc) el.title=desc;
    el.innerHTML='<div class="thumb" style="background-image:url(\\'shots/'+name+'.jpg\\')"></div>'+
      '<div class="meta"><div class="name">'+esc(label)+'</div><div class="tag">'+klass+
      (built?'':' &middot; native only')+'</div></div>'+
      (desc?'<div class="desc">'+esc(desc)+'</div>':'')+
      (author?'<div class="by">'+esc(author)+(year?', '+esc(year):'')+'</div>':'');
    g.appendChild(el);
  });
  document.querySelectorAll('.filters button').forEach(function(b){
    b.onclick=function(){
      document.querySelectorAll('.filters button').forEach(function(x){x.className='';});
      b.className='on';
      document.querySelectorAll('.card').forEach(function(c){
        c.style.display=(b.dataset.f==='all'||c.dataset.k===b.dataset.f)?'':'none';
      });
    };
  });
</script>
</body>
</html>
"""

if __name__ == "__main__":
    sys.exit(main())

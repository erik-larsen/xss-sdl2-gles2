#!/usr/bin/env python3
"""M5 rollout harness: run every built hack headless and classify it.

For each hack executable in the build dir (or the names given on the
command line): run it twice with different --frames counts, screenshot
both, and classify:

  pass    exit 0, non-blank, and the two shots differ (it animates)
  static  exit 0, non-blank, but both shots identical
  blank   exit 0 but the screenshot has < 2 colors (retried once,
          different frame count, before declaring -- some hacks fade
          through black legitimately)
  crash   non-zero exit / signal / timeout

Writes tests/STATUS.md (human table) and tests/STATUS.csv (machine).
Existing rows for hacks NOT in this run are preserved, so batches
accumulate. Screenshots land in <shotdir> for contact-sheeting.

Usage: python3 tests/harness.py [--build DIR] [--frames N] [names...]
"""

import argparse, csv, os, platform, subprocess, sys
from datetime import date

HERE = os.path.dirname(os.path.abspath(__file__))
STATUS_CSV = os.path.join(HERE, "STATUS.csv")
STATUS_MD = os.path.join(HERE, "STATUS.md")

# Non-hack executables that may sit in the build dir.
SKIP = {"testpattern"}

# Hacks whose per-frame delay is seconds, not ms: the default 40 frames
# would blow the timeout. Value = frames for the first shot.
SLOW = {"deco": 3, "abstractile": 3, "epicycle": 4, "interaggregate": 5,
        "petri": 5, "cloudlife": 8}
# (The former seconds-per-frame GL entries -- endgame, queens, nakagin,
# cubestorm -- were an ANGLE-backend pathology, fixed by the driver
# forcing ANGLE_DEFAULT_PLATFORM=metal on macOS. They run at full speed
# now.)

# Slow starters: legitimately near-black at 40 frames (pyro's sky is
# empty until the first shell bursts). Value = frames for the first shot.
LONG = {"pyro": 250, "halo": 500, "energystream": 200, "chompytower": 200,
        "glmatrix": 200,   # code-rain fills in gradually
        # grab hacks with a slow fade-in from black:
        "glslideshow": 300, "jigsaw": 150,
        # queens fades in over its first ~128 frames:
        "queens": 200}
# Inherently intermittent (flash/fade between events); a single shot can
# legitimately land dark. Left at default -- treat a lone blank as flaky.
# e.g. lightning.


def wrap(cmd, timeout_wrap):
    """Platform wrapper: Linux runs under xvfb + llvmpipe, mac direct."""
    if platform.system() == "Darwin":
        return cmd
    return ["timeout", str(timeout_wrap),
            "env", "LIBGL_ALWAYS_SOFTWARE=1", "xvfb-run", "-a"] + cmd


def colors(ppm):
    from PIL import Image
    try:
        im = Image.open(ppm)
        return len(im.getcolors(1 << 24) or [])
    except Exception:
        return 0


def shot(exe, frames, out, timeout):
    if os.path.exists(out):
        os.unlink(out)
    cmd = wrap([exe, "--frames", str(frames), "--shot", out], timeout)
    try:
        r = subprocess.run(cmd, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=timeout)
    except subprocess.TimeoutExpired:
        return "timeout", 0
    if r.returncode != 0:
        return f"exit {r.returncode}", 0
    if not os.path.exists(out):
        return "no shot", 0
    return None, colors(out)


def run_hack(name, build, frames, shotdir, timeout):
    frames = LONG.get(name, SLOW.get(name, frames))
    exe = os.path.join(build, name)
    s1 = os.path.join(shotdir, f"{name}-1.ppm")
    s2 = os.path.join(shotdir, f"{name}-2.ppm")

    d_retry, d_next = (1, 2) if name in SLOW else (37, 23)
    err, c1 = shot(exe, frames, s1, timeout)
    if err:
        return "crash", err
    if c1 < 2:  # possible legit black moment: retry at another count
        err, c1 = shot(exe, frames + d_retry, s1, timeout)
        if err:
            return "crash", err
        if c1 < 2:
            return "blank", f"{c1} colors"

    err, c2 = shot(exe, frames + d_next, s2, timeout)
    if err:
        return "crash", f"2nd run: {err}"
    if open(s1, "rb").read() == open(s2, "rb").read():
        return "static", f"{c1} colors, frames {frames} == {frames+d_next}"
    return "pass", f"{c1} colors"


def load_existing():
    rows = {}
    if os.path.exists(STATUS_CSV):
        with open(STATUS_CSV) as f:
            for row in csv.DictReader(f):
                rows[row["hack"]] = row
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build")
    ap.add_argument("--frames", type=int, default=40)
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--shotdir", default="/tmp/xss_harness")
    ap.add_argument("names", nargs="*")
    args = ap.parse_args()

    os.makedirs(args.shotdir, exist_ok=True)
    names = args.names or sorted(
        f for f in os.listdir(args.build)
        if os.access(os.path.join(args.build, f), os.X_OK)
        and os.path.isfile(os.path.join(args.build, f))
        and "." not in f and f not in SKIP)

    rows = load_existing()
    counts = {}
    for i, name in enumerate(names, 1):
        status, note = run_hack(name, args.build, args.frames,
                                args.shotdir, args.timeout)
        counts[status] = counts.get(status, 0) + 1
        rows[name] = {"hack": name, "status": status, "note": note,
                      "platform": platform.system().lower(),
                      "date": date.today().isoformat()}
        print(f"[{i}/{len(names)}] {status:<7} {name} ({note})", flush=True)

    with open(STATUS_CSV, "w", newline="") as f:
        w = csv.DictWriter(f, ["hack", "status", "note", "platform", "date"])
        w.writeheader()
        for name in sorted(rows):
            w.writerow(rows[name])

    with open(STATUS_MD, "w") as f:
        f.write("# Hack status\n\nGenerated by tests/harness.py; "
                "rows accumulate across batches.\n\n")
        total = {}
        for r in rows.values():
            total[r["status"]] = total.get(r["status"], 0) + 1
        f.write(" | ".join(f"**{k}**: {v}" for k, v in sorted(total.items()))
                + f" | total: {len(rows)}\n\n")
        f.write("| hack | status | note | platform | date |\n")
        f.write("|---|---|---|---|---|\n")
        for name in sorted(rows):
            r = rows[name]
            f.write(f"| {r['hack']} | {r['status']} | {r['note']} | "
                    f"{r['platform']} | {r['date']} |\n")

    print("\nThis run:", ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
    print(f"Wrote {STATUS_CSV} and {STATUS_MD}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Assert that .github/workflows/ci.yml agrees with toolchain.versions.

`runs-on:` takes a literal, so the runner images are necessarily spelled
twice; this catches the day someone edits one and not the other. Also
verifies that no version literal has crept back into the workflow where
an ${{ env.* }} reference belongs, and that every action is pinned to a
commit SHA rather than a mutable tag.

Exit status is 0 when everything matches, 1 otherwise. CI runs it; so
can you, any time: python3 scripts/check-toolchain.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSIONS = os.path.join(ROOT, "toolchain.versions")
WORKFLOW = os.path.join(ROOT, ".github", "workflows", "ci.yml")


def read_versions():
    out = {}
    for line in open(VERSIONS):
        m = re.match(r"^([A-Z_]+)=(.*)$", line.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


def main():
    v = read_versions()
    wf = open(WORKFLOW).read()
    errors = []

    # 1. runner images match, and none of them is a floating -latest label
    for key, label in (("RUNNER_LINUX", "linux"),
                       ("RUNNER_MACOS", "macos"),
                       ("RUNNER_WINDOWS", "windows")):
        want = v.get(key)
        if not want:
            errors.append(f"{key} missing from toolchain.versions")
            continue
        if f"runs-on: {want}" not in wf:
            errors.append(f"{key}={want} is not the {label} job's runs-on")

    for m in re.finditer(r"runs-on:\s*(\S+)", wf):
        if m.group(1).endswith("-latest"):
            errors.append(f"runs-on: {m.group(1)} floats -- pin the image")

    # 2. the installed toolchains come from the file, not from literals
    for key, pat in (("EMSDK_VERSION", r"version:\s*\$\{\{\s*env\.EMSDK_VERSION"),
                     ("NODE_VERSION", r"node-version:\s*\$\{\{\s*env\.NODE_VERSION"),
                     ("CHROME_VERSION", r"chrome-version:\s*\$\{\{\s*env\.CHROME_VERSION")):
        if key not in v:
            errors.append(f"{key} missing from toolchain.versions")
        elif not re.search(pat, wf):
            errors.append(f"{key} is not read from toolchain.versions in ci.yml")

    # 3. every action is pinned to a SHA (a tag can be moved under you)
    for m in re.finditer(r"uses:\s*([^\s@]+)@(\S+)", wf):
        action, ref = m.group(1), m.group(2)
        if not re.fullmatch(r"[0-9a-f]{40}", ref):
            errors.append(f"{action}@{ref} is not pinned to a commit SHA")

    if errors:
        print("toolchain check FAILED:")
        for e in errors:
            print("  -", e)
        return 1
    print(f"toolchain check ok: emsdk {v['EMSDK_VERSION']}, "
          f"node {v['NODE_VERSION']}, chrome {v['CHROME_VERSION']}, "
          f"images {v['RUNNER_LINUX']} / {v['RUNNER_MACOS']} / "
          f"{v['RUNNER_WINDOWS']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

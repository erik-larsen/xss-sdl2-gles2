# Toolchain pinning

A wasm build published to the web keeps working for a very long time —
the standards under it move slowly, and nobody deprecates your `.wasm`.
What decays is the ability to *rebuild* it. This file is how that decay
is kept out of this repo, and it is meant to be copied into sibling
projects (`sgi-demos`, `emscripten-sdl2-gles2`) as-is.

## The rule

**Every version lives in [`toolchain.versions`](../toolchain.versions).**
No version literal belongs anywhere else. `scripts/check-toolchain.py`
runs in CI and fails the build if the workflow disagrees with that file,
if a runner image is a floating `-latest` label, or if an action is
pinned to a tag rather than a commit SHA.

To move a version: edit `toolchain.versions`, run the checker, push, read
CI. One place, one commit, one reason.

## The layers, most to least controllable

| Layer | How it is pinned | Rots when |
|---|---|---|
| **Vendored source** (`third_party/`: xscreensaver, gl4es, glues, GLE, stb, ANGLE binaries) | It is *in the repo* | Never |
| **Emscripten** | `EMSDK_VERSION` — `setup-emsdk` installs exactly it | emsdk drops old releases (they stay downloadable for years) |
| **Actions** | Commit SHAs, with the tag in a trailing comment | Never; Dependabot proposes bumps monthly |
| **Runner images** | Literal dated labels (`ubuntu-24.04`, `macos-15`, `windows-2025`) | GitHub retires an image, ~2 years |
| **Node / Chrome** (test rig only) | `NODE_VERSION`, `CHROME_VERSION` | Chrome-for-Testing keeps an archive; node.org keeps everything |
| **apt / Homebrew packages** | Only by the image | Every image refresh |
| **MSYS2 packages** | **Not pinnable** — see below | Continuously |

The web build is the best case and the reason this project pins at all:
`-sUSE_SDL=2 -sUSE_LIBPNG=1` take SDL2 and libpng from *emscripten
ports*, so `EMSDK_VERSION` alone determines the entire web toolchain.
Nothing on the machine leaks in.

## What cannot be pinned, honestly

- **MSYS2 (Windows).** `pacman` serves only current versions and MSYS2
  keeps no long-term package archive, so `mingw-w64-clang-x86_64-*` is
  whatever shipped that week. `update: false` would freeze less than it
  appears to — installing current packages onto an older base is the
  partial-upgrade case MSYS2 warns about, and it breaks in worse ways
  than drift. The durable fix is to vendor SDL2 and libpng for Windows
  the way ANGLE already is; until then, Windows is the platform most
  likely to break with no commit of yours.
- **apt and Homebrew.** Same shape, less severe: the runner image fixes
  them in practice, and both distributions move slowly within a release.
- **The macOS system compiler.** It comes with the image. This is not
  academic: `macos-14` ships clang 15, which rejects the generated
  aggregate initialisers in `hacks/glx/countries.c` that gcc, clang 16
  and emscripten all accept. Hence the `macos-15` pin.

## Keeping a developer machine in step

```sh
sh scripts/setup-toolchain.sh            # what is here vs what is pinned
sh scripts/setup-toolchain.sh --install  # install the pinned emsdk
set -a; . ./toolchain.versions; set +a   # load the pins into your shell
```

It treats `4.0.12-git` (emcc built from an emsdk checkout) as matching
`4.0.12` — same release, extra commits — and reports the native tools and
which SDL2 the build will actually find. On macOS a `/Library/Frameworks`
install and a Homebrew dylib are different products with different
versions, and only one of them is what CI uses; the script makes that
visible rather than letting it surprise you at push time.

The `grep ... >> "$GITHUB_ENV"` line in the workflow is for the runner
only: `GITHUB_ENV` names a file that exists inside a job and nowhere
else, so running that line in your own shell redirects to an empty
filename and the shell reports "no such file or directory". Every
`KEY=VALUE` in `toolchain.versions` is a single token with no spaces so
that both forms work; free-text provenance lives in comments.

## Bumping emscripten

1. Edit `EMSDK_VERSION`, run `python3 scripts/check-toolchain.py`.
2. Rebuild the whole web tree locally and run the full sweep
   (`sh tests/sweep-web.sh build-web`) — a toolchain bump can change
   codegen under gl4es, which is the part of this stack most likely to
   notice.
3. Check whether any version-specific workaround can go. Historically:
   `-sTOTAL_STACK` (renamed `STACK_SIZE` in 3.1.27, already switched) and
   `main(void)` + `xss_web_args()` — that one stays, because it is also
   how query-string options reach a hack, not merely a workaround for
   3.1.6's JS glue not calling `__main_argc_argv`.

## Copying this to another project

Four files and one habit:

- `toolchain.versions` — the single source of truth.
- `scripts/check-toolchain.py` — the guard that keeps it true.
- `scripts/setup-toolchain.sh` — the developer-side mirror.
- `.github/dependabot.yml` — deliberate action bumps.
- The habit: a workflow step that does
  `grep -E '^[A-Z_]+=' toolchain.versions >> "$GITHUB_ENV"`, after which
  every `with:` reads `${{ env.SOMETHING }}` and never a literal.

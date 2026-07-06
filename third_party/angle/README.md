# Vendored ANGLE (native GLES2 backend for Windows & macOS)

On Linux, gl4es calls into the system mesa `libGLESv2`. Windows and macOS
have no system GLES2, so xss-sdl links a vendored copy of ANGLE's
`libGLESv2` + `libEGL` here -- the same approach sgi-demos uses. The
CMake wiring lives in the top-level `CMakeLists.txt` (the native GLES2
backend block). Linux ignores this directory entirely.

## Layout

    third_party/angle/
      include/            ANGLE GLES2/GLES/EGL/KHR headers
        EGL/  GLES/  GLES2/  KHR/
      lib-mac/            libEGL.dylib   libGLESv2.dylib
      lib-win/            libEGL.dll     libGLESv2.dll
                          libEGL.dll.a   libGLESv2.dll.a   (import libs)

These binaries are committed to the repo (the chosen vendoring model).
CMake's `find_library(... REQUIRED)` will fail with a clear message until
they are present, so CI stays red until this directory is populated.

## Where the binaries come from

ANGLE ships inside Chrome/Chromium. Copy `libEGL` + `libGLESv2` from a
Chrome install (matching the target architecture), and take the headers
from the Khronos/ANGLE registry (or from sgi-demos `libs/libgles/include`,
which is exactly these headers).

### macOS (`lib-mac/`)

Copy the two dylibs out of Chrome's framework bundle, then make sure their
install names are rpath-relative so the `-Wl,-rpath` CMake sets can find
them at runtime:

    install_name_tool -id @rpath/libGLESv2.dylib libGLESv2.dylib
    install_name_tool -id @rpath/libEGL.dylib    libEGL.dylib

(If `libGLESv2.dylib` references `libEGL.dylib` by a non-rpath path, fix
that too with `install_name_tool -change ...`.)

### Windows, MSYS2 CLANG64 (`lib-win/`)

Copy `libEGL.dll` + `libGLESv2.dll` from Chrome. The CLANG64 toolchain
links through **GNU import libraries** (`.dll.a`), not the MSVC `.lib`
that sgi-demos' `lib-win/exp.bat` produces (`dumpbin`/`lib` target the
MSVC linker). Only the DLLs are committed; the import libs are derived
artifacts. **CI generates them automatically** (see the "Generate GNU
import libs" step in ci.yml). For a local MSYS2 build, generate them
once with:

    gendef libGLESv2.dll
    dlltool -d libGLESv2.def -D libGLESv2.dll -l libGLESv2.dll.a
    gendef libEGL.dll
    dlltool -d libEGL.def -D libEGL.dll -l libEGL.dll.a

(`gendef` is in the `mingw-w64-clang-x86_64-tools-git` package;
`llvm-dlltool -m i386:x86-64` works where binutils `dlltool` is absent.)

CMake's `find_library(NAMES GLESv2 libGLESv2 ...)` then resolves
`libGLESv2.dll.a`; the matching `.dll` is copied next to each `.exe` at
build time (the `angle_runtime` target).

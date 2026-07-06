# xscreensaver -> SDL2 + OpenGL ES 2.0 port
#
# One build system for native (Linux/macOS/Windows) and Emscripten:
#   native:      cmake -B build && cmake --build build
#   emscripten:  emcmake cmake -B build-web && cmake --build build-web
#
# Each hack is its own executable (native) or .html page (web).

cmake_minimum_required(VERSION 3.16)
project(xss-sdl C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
add_compile_options(-Wall -Wextra)

# jwxyz defines Xlib-named symbols (XQueryPointer, ...). On Linux, SDL2
# dynamically links real libX11; if our executables export those names,
# SDL's Xlib calls bind to jwxyz and crash. Hidden visibility keeps the
# fake Xlib private to each executable.
if(NOT EMSCRIPTEN)
  # Hide jwxyz's Xlib-named symbols so SDL's real libX11 calls don't
  # bind to them (Linux). On wasm there is no libX11 -- and hiding
  # everything would hide main itself from wasm-ld's
  # --export-if-defined=main, so the JS runtime would silently skip it.
  set(CMAKE_C_VISIBILITY_PRESET hidden)
endif()

# ---------------------------------------------------------------- SDL2
if(EMSCRIPTEN)
  add_compile_options(-sUSE_SDL=2 -sUSE_LIBPNG=1)
  add_link_options(-sUSE_SDL=2 -sUSE_LIBPNG=1 -sALLOW_MEMORY_GROWTH=1
                   -sFULL_ES2=1 -sMINIFY_HTML=0
                   --shell-file ${CMAKE_SOURCE_DIR}/src/web/shell.html)
  # NB: emscripten optimization comes from CMAKE_BUILD_TYPE. Use
  # RelWithDebInfo (-O2); -O3 miscompiles gl4es (gears renders blank).
  set(XSS_SDL_LIBS "")
else()
  find_package(SDL2 REQUIRED)
  set(XSS_SDL_LIBS SDL2::SDL2)
endif()

# ----------------------------------------------- native GLES2 backend
# gl4es translates desktop GL -> GLES2; the GLES2 implementation it calls
# into is the system library on Linux, and vendored ANGLE (libGLESv2 +
# libEGL) on Windows/macOS -- see third_party/angle/README.md. The Linux
# path is unchanged (bare GLESv2). PATCH(xss-sdl): M4 Win/macOS support.
if(NOT EMSCRIPTEN)
  if(WIN32 OR APPLE)
    if(APPLE)
      set(ANGLE_LIBDIR ${CMAKE_SOURCE_DIR}/third_party/angle/lib-mac)
    else()
      set(ANGLE_LIBDIR ${CMAKE_SOURCE_DIR}/third_party/angle/lib-win)
    endif()
    find_library(ANGLE_GLESV2 NAMES GLESv2 libGLESv2
                 PATHS ${ANGLE_LIBDIR} NO_DEFAULT_PATH REQUIRED)
    find_library(ANGLE_EGL    NAMES EGL libEGL
                 PATHS ${ANGLE_LIBDIR} NO_DEFAULT_PATH REQUIRED)
    add_library(xss_gles INTERFACE)
    target_include_directories(xss_gles INTERFACE
                               ${CMAKE_SOURCE_DIR}/third_party/angle/include)
    target_link_libraries(xss_gles INTERFACE ${ANGLE_GLESV2} ${ANGLE_EGL})
    set(XSS_GLES xss_gles)
  else()
    set(XSS_GLES GLESv2)   # Linux: system mesa libGLESv2
  endif()
endif()

# Runtime placement of the ANGLE libs (modeled on sgi-demos GLES_INSTALL):
# macOS resolves them via rpath; Windows needs the DLLs next to each .exe.
# All hack executables land in CMAKE_BINARY_DIR, so one copy covers them.
# PATCH(xss-sdl): M4. Untested on a real runner -- see MILESTONES.md M4b.
if(APPLE AND NOT EMSCRIPTEN)
  set(CMAKE_BUILD_RPATH   ${ANGLE_LIBDIR})
  set(CMAKE_INSTALL_RPATH ${ANGLE_LIBDIR})
elseif(WIN32 AND NOT EMSCRIPTEN)
  file(GLOB ANGLE_DLLS ${ANGLE_LIBDIR}/*.dll)
  if(ANGLE_DLLS)
    add_custom_target(angle_runtime ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ANGLE_DLLS} ${CMAKE_BINARY_DIR}
      COMMENT "Copying ANGLE DLLs next to the executables")
  endif()
endif()

# -------------------------------------------------------------- driver
add_library(xss_driver STATIC src/driver/xss_driver.c)
target_include_directories(xss_driver PUBLIC src/driver)
target_link_libraries(xss_driver PUBLIC ${XSS_SDL_LIBS} m)
if(NOT EMSCRIPTEN)
  target_link_libraries(xss_driver PUBLIC ${XSS_GLES})
endif()

# --------------------------------------------------------------- hacks
# xss_add_hack(<name> <sources...>) -> executable (native) / .html (web)
function(xss_add_hack name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE xss_driver)
  if(EMSCRIPTEN)
    set_target_properties(${name} PROPERTIES SUFFIX ".html")
  endif()
endfunction()

xss_add_hack(testpattern src/hacks/testpattern.c)

# ----------------------------------------------------- xscreensaver port
set(XSS  ${CMAKE_SOURCE_DIR}/third_party/xscreensaver)

add_library(xss_port STATIC
  src/port/jwxyz-sdl.c
  src/port/jwxyz-arcs.c
  src/port/jwxyz-png.c
  ${XSS}/hacks/ximage-loader.c
  src/port/screenhack-sdl.c
  ${XSS}/jwxyz/jwxyz-common.c
  ${XSS}/jwxyz/jwxyz-image.c
  ${XSS}/jwxyz/jwxyz-timers.c
  ${XSS}/hacks/fps.c
  ${XSS}/utils/resources.c
  ${XSS}/utils/yarandom.c
  ${XSS}/utils/usleep.c
  ${XSS}/utils/hsv.c
  ${XSS}/utils/colors.c
  ${XSS}/utils/spline.c
  ${XSS}/utils/utf8wc.c
  ${XSS}/utils/pow2.c
  ${XSS}/utils/aligned_malloc.c
  ${XSS}/utils/doubletime.c
  ${XSS}/utils/thread_util.c
  ${XSS}/utils/xshm.c
  ${XSS}/utils/xft.c
  ${XSS}/utils/font-retry.c
  ${XSS}/utils/erase.c
)
target_include_directories(xss_port PUBLIC
  src/port src/driver ${XSS}/jwxyz ${XSS}/utils ${XSS}/hacks
  ${CMAKE_SOURCE_DIR}/third_party/gl4es/include
  ${CMAKE_SOURCE_DIR}/third_party/glues/source
  src/port/glshim)
# Like Android.mk: these must be on the command line, not just config.h,
# because hacks test STANDALONE etc. before their first #include.
target_compile_definitions(xss_port PUBLIC HAVE_CONFIG_H=1 _GNU_SOURCE=1
  STANDALONE=1 HAVE_JWXYZ=1 JWXYZ_IMAGE=1 HAVE_ANDROID=1
  HAVE_XUTF8DRAWSTRING=1 HAVE_UNISTD_H=1 HAVE_INTTYPES_H=1
  GETTIMEOFDAY_TWO_ARGS=1)
if(NOT EMSCRIPTEN)
  # Web builds stay single-threaded: -pthread wasm requires COOP/COEP
  # headers, which breaks simple static hosting. thread_util.c has a
  # no-pthread fallback.
  target_compile_definitions(xss_port PUBLIC HAVE_PTHREAD=1)
endif()
if(NOT EMSCRIPTEN)
  find_package(PNG REQUIRED)
  target_link_libraries(xss_port PUBLIC PNG::PNG)
endif()
target_link_libraries(xss_port PUBLIC xss_driver)
if(NOT EMSCRIPTEN)
  find_package(Threads REQUIRED)
  target_link_libraries(xss_port PUBLIC Threads::Threads)
endif()

# xss_add_xhack(<name> [extra sources...]) -> port a real 2D hack
function(xss_add_xhack name)
  set(HACK_PREFIX ${name})
  configure_file(src/port/hack_main.c.in
                 ${CMAKE_BINARY_DIR}/gen/${name}_main.c @ONLY)
  add_executable(${name}
    ${CMAKE_BINARY_DIR}/gen/${name}_main.c
    ${XSS}/hacks/${name}.c
    ${ARGN})
  target_link_libraries(${name} PRIVATE xss_port)
  if(EMSCRIPTEN)
    set_target_properties(${name} PROPERTIES SUFFIX ".html")
  endif()
endfunction()

# --------------------------------------------------------- gl4es + glues
set(NOX11 ON CACHE BOOL "" FORCE)
set(NOEGL ON CACHE BOOL "" FORCE)
set(STATICLIB ON CACHE BOOL "" FORCE)
set(NO_LOADER ON CACHE BOOL "" FORCE)
set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/gl4es EXCLUDE_FROM_ALL)
# gl4es names its library target GL everywhere except Windows, where it
# is OPENGL32 (src/CMakeLists.txt "some ES simulators used opengl32.dll").
if(WIN32 AND NOT EMSCRIPTEN)
  set(GL4ES_TARGET OPENGL32)
else()
  set(GL4ES_TARGET GL)
endif()
# gl4es's CMakeLists hardcodes its own source lib/ as the archive output
# dir; with two build trees (native + web) they clobber each other's
# libGL.a. Pin the target's output into the current build dir, and link
# the target by name (not -lGL, which finds the stale source-tree copy).
set_target_properties(${GL4ES_TARGET} PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/gl4es-lib)

set(GLUES ${CMAKE_SOURCE_DIR}/third_party/glues/source)
file(GLOB GLUES_SRC ${GLUES}/*.c ${GLUES}/libtess/*.c)
add_library(glues STATIC ${GLUES_SRC})
target_compile_definitions(glues PUBLIC LIBRARYBUILD=1 XSS_GLUES_GL4ES=1)
target_include_directories(glues PUBLIC ${GLUES}
  ${CMAKE_SOURCE_DIR}/third_party/gl4es/include)
target_link_libraries(glues PUBLIC ${GL4ES_TARGET})

# GL side of the port (gl4es bootstrap + GLX shims + init_GL)
add_library(xss_glport STATIC src/port/glx-sdl.c)
target_compile_definitions(xss_glport PUBLIC
  HAVE_CONFIG_H=1 HAVE_GL=1 USE_GL=1 HAVE_GLSL=1)
target_include_directories(xss_glport PUBLIC
  src/port/glshim ${XSS}/hacks/glx)
target_link_libraries(xss_glport PUBLIC xss_port glues ${GL4ES_TARGET})

# xss_add_glhack(<name> [extra glx sources...]) -> port a GL hack
function(xss_add_glhack name)
  set(HACK_PREFIX ${name})
  configure_file(src/port/hack_main.c.in
                 ${CMAKE_BINARY_DIR}/gen/${name}_main.c @ONLY)
  set(extras "")
  foreach(e ${ARGN})
    list(APPEND extras ${XSS}/hacks/glx/${e}.c)
  endforeach()
  add_executable(${name}
    ${CMAKE_BINARY_DIR}/gen/${name}_main.c
    ${XSS}/hacks/glx/${name}.c
    ${XSS}/hacks/xlockmore.c
    ${extras})
  target_link_libraries(${name} PRIVATE xss_glport)
  if(EMSCRIPTEN)
    set_target_properties(${name} PROPERTIES SUFFIX ".html")
  endif()
endfunction()

set(GLCOMMON rotator gltrackball trackball quaternion fps-gl texfont normals glsl-utils)
xss_add_glhack(gears involute tube ${GLCOMMON})
xss_add_glhack(hypertorus ${GLCOMMON})
xss_add_glhack(bouncingcow gllist cow_face cow_hide cow_hoofs cow_horns cow_tail cow_udder ${GLCOMMON})
xss_add_glhack(glmatrix ${GLCOMMON})
xss_add_glhack(lament gllist lament_model ${GLCOMMON})
xss_add_xhack(greynetic)
xss_add_xhack(attraction)
xss_add_xhack(interference)
xss_add_xhack(moire)
xss_add_xhack(deco)

# polyhedra: module prefix "polyhedra", main source polyhedra-gl.c
set(HACK_PREFIX polyhedra)
configure_file(src/port/hack_main.c.in ${CMAKE_BINARY_DIR}/gen/polyhedra_main.c @ONLY)
add_executable(polyhedra
  ${CMAKE_BINARY_DIR}/gen/polyhedra_main.c
  ${XSS}/hacks/glx/polyhedra-gl.c
  ${XSS}/hacks/glx/polyhedra.c
  ${XSS}/hacks/glx/teapot.c
  ${XSS}/hacks/xlockmore.c
  ${XSS}/hacks/glx/rotator.c ${XSS}/hacks/glx/gltrackball.c
  ${XSS}/hacks/glx/trackball.c ${XSS}/hacks/glx/quaternion.c
  ${XSS}/hacks/glx/fps-gl.c ${XSS}/hacks/glx/texfont.c
  ${XSS}/hacks/glx/normals.c ${XSS}/hacks/glx/glsl-utils.c)
target_link_libraries(polyhedra PRIVATE xss_glport)
if(EMSCRIPTEN)
  set_target_properties(polyhedra PROPERTIES SUFFIX ".html")
endif()

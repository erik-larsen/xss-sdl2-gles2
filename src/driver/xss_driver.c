/* xss_driver.c -- SDL2 + OpenGL ES 2.0 driver for xscreensaver hack ports.
 *
 * SDL/GL setup follows the sgi-demos sdl_framebuffer.c pattern verbatim:
 * one unconditional code path that requests an OpenGL ES context via
 *   SDL_HINT_OPENGL_ES_DRIVER + SDL_GL_CONTEXT_EGL + PROFILE_ES,
 * which is known to work on Linux, macOS, Windows and Emscripten.
 *
 * The main loop is structured as a tick() function from day one so the
 * Emscripten build (emscripten_set_main_loop) and the native build
 * (while-loop) share all logic.  The only platform-conditional code in
 * this file is the loop driver itself and a window-title quirk.
 */

#include "xss_driver.h"

/* Under emscripten, SDL_main.h redefines main() and the replacement
 * never invokes ours (silent empty-main). We own the entry point. */
#define SDL_MAIN_HANDLED 1
#include <SDL.h>
#include <SDL_opengles2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
# include <emscripten.h>

/* Build an argv from the page's query string: ?frames=60&shot=o.ppm
 * becomes { prog, "--frames", "60", "--shot", "o.ppm" }. Unknown keys
 * pass through as --key value and hit the driver's normal parser. */
char **
xss_web_args (int *argc_ret)
{
    static char *argv[64];
    static char buf[1024];
    static char keys[32][40];
    int argc = 0, nk = 0;
    argv[argc++] = "hack";

    /* emscripten 3.1.6 predates EM_ASM_PTR; run_script_string returns
     * a static buffer, which we copy immediately. */
    const char *qs = emscripten_run_script_string (
        "(typeof location !== 'undefined' && location.search)"
        " ? location.search.slice(1) : ''");
    size_t len = strlen (qs);
    if (len >= sizeof buf) len = sizeof buf - 1;
    memcpy (buf, qs, len);
    buf[len] = 0;

    char *save = NULL;
    for (char *tok = strtok_r (buf, "&", &save);
         tok && argc < 62 && nk < 32;
         tok = strtok_r (NULL, "&", &save)) {
        char *eq = strchr (tok, '=');
        snprintf (keys[nk], sizeof keys[nk], "--%.*s",
                  eq ? (int)(eq - tok) : (int) strlen (tok), tok);
        argv[argc++] = keys[nk++];
        if (eq && eq[1]) argv[argc++] = eq + 1;
    }
    argv[argc] = NULL;
    *argc_ret = argc;
    return argv;
}
#endif

/* ------------------------------------------------------------------ */
/* Driver state                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    const xss_hack *hack;
    void           *hack_state;

    SDL_Window     *window;
    SDL_GLContext   gl;

    xss_surface     surf;        /* CPU framebuffer the hack draws into */

    GLuint          prog;
    GLuint          tex;
    GLuint          vbo;
    GLint           a_pos, a_uv, u_tex;

    int             win_w, win_h;       /* drawable size (pixels)      */
    unsigned long   delay_us;           /* hack-requested frame delay  */
    Uint64          next_frame_ticks;   /* SDL ticks of next draw      */

    long            frames_done;
    long            frames_limit;       /* --frames N, 0 = unlimited   */
    const char     *shot_path;          /* --shot out.ppm              */
    bool            show_fps;
    bool            running;
    int             exit_code;

    /* fps accounting */
    Uint64          fps_t0;
    long            fps_frames;
} xss_ctx;

static xss_ctx g;   /* single instance; emscripten main loop needs it  */

/* Optional hook: GL-hack builds link glx-sdl.c, whose strong definition
 * initializes gl4es. Must run after context creation and before ANY GL
 * call, because gl4es interposes the GL symbols in those binaries. */
__attribute__((weak)) void xss_driver_gl_ready(void) {}

/* Optional hooks around the buffer swap: glx-sdl.c's strong definitions
 * call gl4es_pre_swap()/gl4es_post_swap(). gl4es holds immediate-mode
 * geometry in an internal batch; without the pre-swap flush the frame's
 * tail (or all of it) never reaches the framebuffer. Mesa/llvmpipe
 * masked this; ANGLE/Metal renders opaque black without it. */
__attribute__((weak)) void xss_gl_pre_swap(void) {}
__attribute__((weak)) void xss_gl_post_swap(void) {}

/* ------------------------------------------------------------------ */
/* GLES2 blit: one textured quad, y-flip done in the texture coords    */
/* ------------------------------------------------------------------ */

static const char *vsrc =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main(){ v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *fsrc =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){ gl_FragColor = vec4(texture2D(u_tex, v_uv).rgb, 1.0); }\n";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof log, NULL, log);
        fprintf(stderr, "ERROR: shader compile failed:\n%s\n", log);
        exit(1);
    }
    return sh;
}

static void init_blit(void)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    g.prog = glCreateProgram();
    glAttachShader(g.prog, vs);
    glAttachShader(g.prog, fs);
    glLinkProgram(g.prog);
    GLint ok = 0;
    glGetProgramiv(g.prog, GL_LINK_STATUS, &ok);
    if (!ok) { fprintf(stderr, "ERROR: program link failed\n"); exit(1); }
    glDeleteShader(vs);
    glDeleteShader(fs);

    g.a_pos = glGetAttribLocation (g.prog, "a_pos");
    g.a_uv  = glGetAttribLocation (g.prog, "a_uv");
    g.u_tex = glGetUniformLocation(g.prog, "u_tex");

    /* Fullscreen quad as triangle strip.  UVs flip y: surface row 0 is
     * the top of the window (X11 convention), GL row 0 is the bottom. */
    static const GLfloat quad[] = {
        /* pos        uv          */
        -1.f, -1.f,   0.f, 1.f,
         1.f, -1.f,   1.f, 1.f,
        -1.f,  1.f,   0.f, 0.f,
         1.f,  1.f,   1.f, 0.f,
    };
    glGenBuffers(1, &g.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    /* Never leave our VBO bound: the driver's raw GLES2 calls are
     * invisible to gl4es's buffer-binding cache. If this 64-byte quad
     * VBO stays bound, gl4es (believing ARRAY_BUFFER==0) submits
     * client-side vertex pointers that strict-offset drivers
     * (ANGLE/Metal, WebGL) misread as offsets into it -- every GL-hack
     * draw then produces zero fragments (opaque-black window). Mesa
     * honors the client pointer anyway, which is why Linux never
     * caught it. Same desync the fpe.c M3c web patch defends against. */
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* NPOT texture is legal in GLES2/WebGL1 iff CLAMP_TO_EDGE + no mips. */
    glGenTextures(1, &g.tex);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void alloc_surface(int w, int h)
{
    free(g.surf.pixels);
    g.surf.width  = w;
    g.surf.height = h;
    g.surf.pitch  = w;
    g.surf.pixels = calloc((size_t)w * h, 4);
    if (!g.surf.pixels) { fprintf(stderr, "ERROR: out of memory\n"); exit(1); }

    glBindTexture(GL_TEXTURE_2D, g.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

static void blit_surface(void)
{
    glViewport(0, 0, g.win_w, g.win_h);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g.prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    g.surf.width, g.surf.height,
                    GL_RGBA, GL_UNSIGNED_BYTE, g.surf.pixels);
    glUniform1i(g.u_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glEnableVertexAttribArray(g.a_pos);
    glEnableVertexAttribArray(g.a_uv);
    glVertexAttribPointer(g.a_pos, 2, GL_FLOAT, GL_FALSE, 16, (void *)0);
    glVertexAttribPointer(g.a_uv,  2, GL_FLOAT, GL_FALSE, 16, (void *)8);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);  /* see init_blit: keep gl4es's
                                        * binding cache in sync */
    /* swap happens in tick(), after the optional --shot readback */
}

/* ------------------------------------------------------------------ */
/* Screenshot (PPM keeps the harness dependency-free)                  */
/* ------------------------------------------------------------------ */

static void write_ppm(const char *path)
{
    /* Read back the actual rendered window, not the CPU buffer: this
     * verifies the full pipeline, which is the point of the harness. */
    int w = g.win_w, h = g.win_h;
    unsigned char *px = malloc((size_t)w * h * 4);
    if (!px) return;
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); free(px); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)            /* GL is bottom-up */
        for (int x = 0; x < w; x++)
            fwrite(px + 4 * ((size_t)y * w + x), 1, 3, f);
    fclose(f);
    free(px);
    printf("INFO: wrote %s (%dx%d)\n", path, w, h);
}

/* ------------------------------------------------------------------ */
/* Events                                                              */
/* ------------------------------------------------------------------ */

static void handle_resize(void)
{
    SDL_GL_GetDrawableSize(g.window, &g.win_w, &g.win_h);
    alloc_surface(g.win_w, g.win_h);
    if (g.hack->reshape)
        g.hack->reshape(&g.surf, g.hack_state);
}

static void poll_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            g.running = false;
            break;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)
                g.running = false;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                handle_resize();
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main loop tick -- shared verbatim by native and emscripten builds   */
/* ------------------------------------------------------------------ */

static void tick(void)
{
    poll_events();
    if (!g.running) return;

    Uint64 now = SDL_GetTicks64();
    if (now < g.next_frame_ticks) {
#ifndef __EMSCRIPTEN__
        SDL_Delay((Uint32)SDL_min(g.next_frame_ticks - now, 5));
#endif
        return;     /* not yet time for the next hack frame */
    }

    g.delay_us = g.hack->draw(&g.surf, g.hack_state);
    g.next_frame_ticks = now + g.delay_us / 1000;
    if (!g.hack->gl_p)
        blit_surface();                /* 2D: upload CPU surface + quad */
    xss_gl_pre_swap();                 /* gl4es: flush batched geometry */
    /* --shot reads back the final frame BEFORE the swap: the post-swap
     * back buffer is undefined (llvmpipe preserves it; ANGLE/Metal
     * returns a fresh black drawable). Pre-swap is also what makes the
     * shot work on web, where the post-loop path never runs (the
     * emscripten main loop never returns) and a composited canvas
     * reads back zeros. */
    if (g.shot_path && g.frames_limit && g.frames_done + 1 >= g.frames_limit)
        write_ppm(g.shot_path);
    SDL_GL_SwapWindow(g.window);
    xss_gl_post_swap();
    g.frames_done++;

    if (g.show_fps) {
        g.fps_frames++;
        if (now - g.fps_t0 >= 1000) {
            printf("INFO: %.1f fps\n", g.fps_frames * 1000.0 / (now - g.fps_t0));
            g.fps_t0 = now;
            g.fps_frames = 0;
        }
    }

    if (g.frames_limit && g.frames_done >= g.frames_limit)
        g.running = false;
}

#ifdef __EMSCRIPTEN__
static void em_tick(void)
{
    tick();
    if (!g.running)
        emscripten_cancel_main_loop();
}
#endif

/* ------------------------------------------------------------------ */
/* Entry                                                               */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [--width N] [--height N] [--frames N] [--shot out.ppm] [--fps]\n",
        prog);
}

int xss_driver_run(const xss_hack *hack, int argc, char **argv)
{
    int req_w = 800, req_h = 600;
    memset(&g, 0, sizeof g);
    g.hack = hack;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--width")  && i+1 < argc) req_w = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--height") && i+1 < argc) req_h = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i+1 < argc) g.frames_limit = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot")   && i+1 < argc) g.shot_path = argv[++i];
        else if (!strcmp(argv[i], "--fps"))                  g.show_fps = true;
        else { usage(argv[0]); return 2; }
    }

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "ERROR: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_version v; SDL_GetVersion(&v);
    printf("INFO: SDL version: %d.%d.%d\n", v.major, v.minor, v.patch);

    /* --- GLES context request: the sgi-demos pattern, one path ----- */
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    /* Make the vendored ANGLE use its native Metal backend. Its default
     * on macOS is the OpenGL backend, which sits on Apple's deprecated
     * GL-on-Metal stack (GLEngine) -- and that path stalls in
     * glMapBufferRange (gldWaitForObject: a full GPU sync) on every
     * client-array draw gl4es submits. Display-list-heavy hacks (queens,
     * endgame, nakagin, cubestorm, jigsaw: chessmodels etc.) ran at
     * SECONDS per frame; on the Metal backend they are 50-200x faster.
     * Users can still override with their own ANGLE_DEFAULT_PLATFORM. */
    setenv("ANGLE_DEFAULT_PLATFORM", "metal", 0 /* don't clobber */);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
    /* 0 alpha bits: on emscripten this makes SDL request a WebGL
     * context with alpha:false, so the canvas composites opaquely
     * regardless of the framebuffer's alpha channel. (GL hacks render
     * through gl4es into an internal FBO and leave the default-buffer
     * alpha at 0, which an alpha:true canvas would treat as fully
     * transparent -> invisible.) Native is unaffected. */
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    char title[256];
    snprintf(title, sizeof title, "xscreensaver: %s", hack->name);
    g.window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, req_w, req_h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
        SDL_WINDOW_SHOWN  | SDL_WINDOW_RESIZABLE);
    if (!g.window) {
        fprintf(stderr, "ERROR: SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }

    g.gl = SDL_GL_CreateContext(g.window);
    if (!g.gl) {
        fprintf(stderr, "ERROR: SDL_GL_CreateContext: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    xss_driver_gl_ready();

    printf("INFO: GL vendor:   %s\n", glGetString(GL_VENDOR));
    printf("INFO: GL renderer: %s\n", glGetString(GL_RENDERER));
    printf("INFO: GL version:  %s\n", glGetString(GL_VERSION));

    glClearColor(0, 0, 0, 1);
    init_blit();
    SDL_GL_GetDrawableSize(g.window, &g.win_w, &g.win_h);
    printf("INFO: drawable: %dx%d\n", g.win_w, g.win_h);
    alloc_surface(g.win_w, g.win_h);

    g.hack_state = hack->init(&g.surf);
    g.running = true;
    g.fps_t0 = SDL_GetTicks64();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(em_tick, 0, 1);    /* runs until cancel   */
#else
    while (g.running)
        tick();
#endif

    if (g.shot_path && !g.frames_limit)
        write_ppm(g.shot_path);   /* fallback (quit w/o --frames); the
                                   * post-swap content is undefined on
                                   * some drivers -- prefer --frames N */

    if (hack->free)
        hack->free(g.hack_state);
    free(g.surf.pixels);
    SDL_GL_DeleteContext(g.gl);
    SDL_DestroyWindow(g.window);
    SDL_Quit();

    printf("INFO: exiting after %ld frames\n", g.frames_done);
    return g.exit_code;
}

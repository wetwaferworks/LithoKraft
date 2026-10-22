/* ILithoCEF.h — the Litho CEF layer interface contract.
 *
 * LithoCEF (JD, 2026-10-20): the host-shell layer of LithoLattice.
 *
 *   "The CEF layer will sit OUTSIDE of the engine and just boot the
 *    engine. And the CEF layer plugs into the hardware layer."
 *
 * Layering, bottom to top:
 *   hardware layer   — display, GPU, input devices, OS windowing
 *   LithoCEF shell   — THIS contract; a backend (CEF, stub, ...) plugs
 *                      into the hardware layer and hosts the engine
 *   the engine       — LithoLattice / LithoKraft, booted inside the
 *                      shell, untouched by how it got there
 *
 * The engine core depends on NOTHING in this header's implementations:
 * this file is plain C (no C++ types, no Chromium/CEF types, no OS
 * headers) so it can be consumed from C, C++, and across DLL borders
 * without anything browser-flavored leaking into engine headers.
 * Backends export exactly one symbol: lithocef_get_api().
 *
 * The living miniature of this contract is the web build's shell page:
 * the page boots the engine, owns the canvas and the GL context, feeds
 * the engine's memory-mapped input ring, and presents frames through
 * the hand-written thin imports in engine/src/render/gl33/lk_webio.js.
 * ILithoCEF names that same shape for native hardware.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LITHOCEF_ABI_VERSION 1u

/* Opaque shell instance. Created, owned, and destroyed by the backend;
 * the engine side only ever holds the pointer. */
typedef struct LithoCefShell LithoCefShell;

typedef enum LithoCefResult {
  LITHOCEF_OK = 0,
  LITHOCEF_ERR_INVALID = -1,     /* bad argument / wrong call order */
  LITHOCEF_ERR_UNSUPPORTED = -2, /* backend cannot do this (yet) */
  LITHOCEF_ERR_NOT_BOOTED = -3,  /* engine not booted (or already shut down) */
  LITHOCEF_ERR_INTERNAL = -4     /* backend failure; see last_error() */
} LithoCefResult;

/* How the engine's rendered output reaches the glass.
 *   NONE              — no surface (headless runs, sealed sim tests).
 *   NATIVE_WINDOW     — the shell owns a native window + GL context and
 *                       the engine renders into it (CEF-as-host shape).
 *   OFFSCREEN_TEXTURE — the shell hands the engine a GL texture name
 *                       (render target) or receives the engine's frames
 *                       as a texture, for compositing into a larger
 *                       scene (web-content-in-world, headset shells). */
typedef enum LithoCefSurfaceKind {
  LITHOCEF_SURFACE_NONE = 0,
  LITHOCEF_SURFACE_NATIVE_WINDOW = 1,
  LITHOCEF_SURFACE_OFFSCREEN_TEXTURE = 2
} LithoCefSurfaceKind;

typedef struct LithoCefSurfaceDesc {
  LithoCefSurfaceKind kind;
  /* Platform-opaque handles; the engine never dereferences them, they
   * are carried so a renderer backend that shares the shell's context
   * can find its bearings. HWND / NSWindow* / X11 Window cast to void*,
   * or a GLFWwindow*. NULL when kind is NONE or OFFSCREEN_TEXTURE. */
  void* native_window;
  /* Platform-opaque GL context (HGLRC / NSOpenGLContext* / GLXContext /
   * EGLContext) the surface was created with, for context sharing.
   * NULL when unknown or not applicable. */
  void* gl_context;
  /* OFFSCREEN_TEXTURE only: GL texture name used for the hand-off.
   * 0 = no texture hand-off (backend presents by its own path). */
  uint32_t texture_name;
  int32_t width;  /* surface size in physical pixels */
  int32_t height;
} LithoCefSurfaceDesc;

typedef struct LithoCefConfig {
  uint32_t abi_version; /* caller sets LITHOCEF_ABI_VERSION */
  const char* application_name; /* window title / app id; may be NULL */
  int32_t surface_width;        /* requested surface size; 0 = default */
  int32_t surface_height;
  LithoCefSurfaceKind preferred_surface;
  bool headless_ok; /* true: a NONE surface is an acceptable outcome */
} LithoCefConfig;

/* What the shell should boot. For a CEF backend, payload_uri is the page
 * or package the shell presents as the engine's first payload (the web
 * build's page today; an engine entry point tomorrow). The engine does
 * not parse this — the shell owns boot policy. */
typedef struct LithoCefEngineBoot {
  const char* payload_uri; /* may be NULL (backend default) */
  const char* working_dir; /* may be NULL */
  int32_t argc;            /* forwarded engine arguments; may be 0 */
  const char* const* argv; /* may be NULL when argc is 0 */
} LithoCefEngineBoot;

/* Input, shell -> engine. The shell translates hardware input (keyboard,
 * pointer, wheel, text, focus) into events and pushes them through the
 * sink the engine side registered. The engine-side consumer is the
 * memory-mapped input ring proven by the web build: the shell FEEDS the
 * ring; it never reaches past it into engine state. Events are stamped
 * at the writer (time_ns, monotonic) — event time, never drain time. */
typedef enum LithoCefInputEventKind {
  LITHOCEF_INPUT_KEY = 1,
  LITHOCEF_INPUT_POINTER_MOVE = 2,
  LITHOCEF_INPUT_POINTER_BUTTON = 3,
  LITHOCEF_INPUT_WHEEL = 4,
  LITHOCEF_INPUT_TEXT = 5,
  LITHOCEF_INPUT_FOCUS = 6
} LithoCefInputEventKind;

typedef struct LithoCefInputEvent {
  uint32_t kind;  /* LithoCefInputEventKind */
  uint32_t code;  /* key code / pointer button index (backend-defined,
                     documented by each backend; the engine maps at its
                     own boundary, as the web build does) */
  int32_t value;  /* KEY/BUTTON: 1 = down, 0 = up. WHEEL: signed delta.
                     FOCUS: 1 = gained, 0 = lost. */
  double x;       /* pointer position in surface pixels (pointer kinds) */
  double y;
  uint32_t text_utf32; /* TEXT kind: one Unicode code point per event */
  uint64_t time_ns;    /* writer-side event time, monotonic nanoseconds */
} LithoCefInputEvent;

typedef struct LithoCefInputSink {
  void* user;
  /* Push one event toward the engine. Returns 1 when the event was
   * consumed (written to the ring), 0 when it was dropped (ring full);
   * the shell counts drops via dropped_inputs(). */
  int (*push_event)(void* user, const LithoCefInputEvent* ev);
} LithoCefInputSink;

/* The backend's whole surface, as one table. Versioned by abi_version;
 * new verbs are appended, never reordered. */
typedef struct LithoCefApi {
  uint32_t abi_version;

  /* Lifecycle. */
  LithoCefShell* (*create)(const LithoCefConfig* cfg);
  void (*destroy)(LithoCefShell* shell);
  /* Boot the engine inside the shell. Callable once per shell. */
  int (*boot_engine)(LithoCefShell* shell, const LithoCefEngineBoot* boot);
  /* Orderly engine stop. The shell (and its surface) stays alive until
   * destroy(); boot_engine may be called again after shutdown. */
  int (*shutdown)(LithoCefShell* shell);

  /* Surface provision. acquire fills *out (kind NONE when headless).
   * The surface stays valid until release_surface or destroy. */
  int (*acquire_surface)(LithoCefShell* shell, LithoCefSurfaceDesc* out);
  void (*release_surface)(LithoCefShell* shell);

  /* Input injection point. Registering replaces any previous sink;
   * pass NULL to detach. The sink must outlive its registration. */
  int (*set_input_sink)(LithoCefShell* shell, const LithoCefInputSink* sink);

  /* Present / frame pacing. present() hands the finished frame to the
   * hardware layer (swap / composite / texture publish, per backend).
   * refresh_millihertz() reports the display cadence the shell is
   * pacing against (60000 = 60 Hz); 0 = unknown / unbounded (stub,
   * headless). The engine's step clock stays the engine's — the shell
   * paces PRESENTATION only, never simulation. */
  int (*present)(LithoCefShell* shell);
  int (*refresh_millihertz)(LithoCefShell* shell);

  /* Counters for the bench record. */
  uint64_t (*frames_presented)(LithoCefShell* shell);
  uint64_t (*dropped_inputs)(LithoCefShell* shell);

  /* Human-readable detail for the most recent error on this shell.
   * Owned by the shell; valid until the next API call on it. */
  const char* (*last_error)(LithoCefShell* shell);
} LithoCefApi;

/* The one symbol every LithoCEF backend exports. */
const LithoCefApi* lithocef_get_api(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

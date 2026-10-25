/* IHwLayer.h — the hardware layer (IHwLayer) base contract.
 *
 * IHwLayer (JD's name, 2026-10-24 16:53 EDT; supersedes ILithoCEF,
 * IHardwareWeb and IHardwareLayer): the layer of LithoLattice that sits
 * outside the engine, boots the engine, plugs into the hardware, and
 * provides hardware resources while the engine never learns what it
 * runs on. JD's law of the shape (verbatim, 2026-10-20):
 *
 *   "The CEF layer will sit OUTSIDE of the engine and just boot the
 *    engine. And the CEF layer plugs into the hardware layer."
 *
 * Layering, bottom to top:
 *   hardware         — display, GPU, input devices, OS windowing
 *   IHwLayer         — THIS contract (the base every tenant sits behind);
 *                      tenants: IHwCef (IHwCef.h, the CEF host, the
 *                      working path) and the stub backend (hw_stub.h)
 *   the engine       — LithoLattice / LithoKraft, booted inside the
 *                      layer, untouched by how it got there
 *
 * The engine core depends on NOTHING in this header's implementations:
 * this file is plain C (no C++ types, no Chromium/CEF types, no OS
 * headers) so it can be consumed from C, C++, and across DLL borders
 * without anything browser-flavored leaking into engine headers.
 * Backends export exactly one symbol: lithohw_get_api().
 *
 * The living miniature of this contract is the web build's shell page:
 * the page boots the engine, owns the canvas and the GL context, feeds
 * the engine's memory-mapped input ring, and presents frames through
 * the hand-written thin imports in engine/src/render/gl33/lk_webio.js.
 * IHwLayer names that same shape for native hardware.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ABI 2 (2026-10-25, IHwLayer phase 2): query_census appended. */
#define LITHOHW_ABI_VERSION 2u

/* Hardware census (JD: "the layer spends what the hardware census says
 * the machine can pay"). Every field is honest: 0 means unknown, never a
 * guess. The census describes the machine, never the tenant: IHwCef,
 * IHwWebkit and the stub all answer through the same struct, so the
 * engine cannot tell which tenant is present. GPU detail lives behind
 * IHwGpu (IHwGpu.h); here only whether one is reachable. */
typedef struct LithoHwCensus {
  uint32_t logical_threads;     /* hardware threads; feeds the Thread Handshake census */
  uint32_t physical_memory_mb;  /* 0 = unknown */
  int32_t refresh_millihertz;   /* display cadence; 0 = unknown / headless */
  uint32_t gpu_reachable;       /* 1 = an IHwGpu seat can answer for this shell */
  uint32_t reserved[4];         /* zero; appended fields take these first */
} LithoHwCensus;

/* Opaque shell instance. Created, owned, and destroyed by the backend;
 * the engine side only ever holds the pointer. */
typedef struct LithoHwShell LithoHwShell;

typedef enum LithoHwResult {
  LITHOHW_OK = 0,
  LITHOHW_ERR_INVALID = -1,     /* bad argument / wrong call order */
  LITHOHW_ERR_UNSUPPORTED = -2, /* backend cannot do this (yet) */
  LITHOHW_ERR_NOT_BOOTED = -3,  /* engine not booted (or already shut down) */
  LITHOHW_ERR_INTERNAL = -4     /* backend failure; see last_error() */
} LithoHwResult;

/* How the engine's rendered output reaches the glass.
 *   NONE              — no surface (headless runs, sealed sim tests).
 *   NATIVE_WINDOW     — the shell owns a native window + GL context and
 *                       the engine renders into it (CEF-as-host shape).
 *   OFFSCREEN_TEXTURE — the shell hands the engine a GL texture name
 *                       (render target) or receives the engine's frames
 *                       as a texture, for compositing into a larger
 *                       scene (web-content-in-world, headset shells). */
typedef enum LithoHwSurfaceKind {
  LITHOHW_SURFACE_NONE = 0,
  LITHOHW_SURFACE_NATIVE_WINDOW = 1,
  LITHOHW_SURFACE_OFFSCREEN_TEXTURE = 2
} LithoHwSurfaceKind;

typedef struct LithoHwSurfaceDesc {
  LithoHwSurfaceKind kind;
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
} LithoHwSurfaceDesc;

typedef struct LithoHwConfig {
  uint32_t abi_version; /* caller sets LITHOHW_ABI_VERSION */
  const char* application_name; /* window title / app id; may be NULL */
  int32_t surface_width;        /* requested surface size; 0 = default */
  int32_t surface_height;
  LithoHwSurfaceKind preferred_surface;
  bool headless_ok; /* true: a NONE surface is an acceptable outcome */
} LithoHwConfig;

/* What the shell should boot. For a CEF backend, payload_uri is the page
 * or package the shell presents as the engine's first payload (the web
 * build's page today; an engine entry point tomorrow). The engine does
 * not parse this — the shell owns boot policy. */
typedef struct LithoHwEngineBoot {
  const char* payload_uri; /* may be NULL (backend default) */
  const char* working_dir; /* may be NULL */
  int32_t argc;            /* forwarded engine arguments; may be 0 */
  const char* const* argv; /* may be NULL when argc is 0 */
} LithoHwEngineBoot;

/* Input, shell -> engine. The shell translates hardware input (keyboard,
 * pointer, wheel, text, focus) into events and pushes them through the
 * sink the engine side registered. The engine-side consumer is the
 * memory-mapped input ring proven by the web build: the shell FEEDS the
 * ring; it never reaches past it into engine state. Events are stamped
 * at the writer (time_ns, monotonic) — event time, never drain time. */
typedef enum LithoHwInputEventKind {
  LITHOHW_INPUT_KEY = 1,
  LITHOHW_INPUT_POINTER_MOVE = 2,
  LITHOHW_INPUT_POINTER_BUTTON = 3,
  LITHOHW_INPUT_WHEEL = 4,
  LITHOHW_INPUT_TEXT = 5,
  LITHOHW_INPUT_FOCUS = 6
} LithoHwInputEventKind;

typedef struct LithoHwInputEvent {
  uint32_t kind;  /* LithoHwInputEventKind */
  uint32_t code;  /* key code / pointer button index (backend-defined,
                     documented by each backend; the engine maps at its
                     own boundary, as the web build does) */
  int32_t value;  /* KEY/BUTTON: 1 = down, 0 = up. WHEEL: signed delta.
                     FOCUS: 1 = gained, 0 = lost. */
  double x;       /* pointer position in surface pixels (pointer kinds) */
  double y;
  uint32_t text_utf32; /* TEXT kind: one Unicode code point per event */
  uint64_t time_ns;    /* writer-side event time, monotonic nanoseconds */
} LithoHwInputEvent;

typedef struct LithoHwInputSink {
  void* user;
  /* Push one event toward the engine. Returns 1 when the event was
   * consumed (written to the ring), 0 when it was dropped (ring full);
   * the shell counts drops via dropped_inputs(). */
  int (*push_event)(void* user, const LithoHwInputEvent* ev);
} LithoHwInputSink;

/* The backend's whole surface, as one table. Versioned by abi_version;
 * new verbs are appended, never reordered. */
typedef struct LithoHwApi {
  uint32_t abi_version;

  /* Lifecycle. */
  LithoHwShell* (*create)(const LithoHwConfig* cfg);
  void (*destroy)(LithoHwShell* shell);
  /* Boot the engine inside the shell. Callable once per shell. */
  int (*boot_engine)(LithoHwShell* shell, const LithoHwEngineBoot* boot);
  /* Orderly engine stop. The shell (and its surface) stays alive until
   * destroy(); boot_engine may be called again after shutdown. */
  int (*shutdown)(LithoHwShell* shell);

  /* Surface provision. acquire fills *out (kind NONE when headless).
   * The surface stays valid until release_surface or destroy. */
  int (*acquire_surface)(LithoHwShell* shell, LithoHwSurfaceDesc* out);
  void (*release_surface)(LithoHwShell* shell);

  /* Input injection point. Registering replaces any previous sink;
   * pass NULL to detach. The sink must outlive its registration. */
  int (*set_input_sink)(LithoHwShell* shell, const LithoHwInputSink* sink);

  /* Present / frame pacing. present() hands the finished frame to the
   * hardware layer (swap / composite / texture publish, per backend).
   * refresh_millihertz() reports the display cadence the shell is
   * pacing against (60000 = 60 Hz); 0 = unknown / unbounded (stub,
   * headless). The engine's step clock stays the engine's — the shell
   * paces PRESENTATION only, never simulation. */
  int (*present)(LithoHwShell* shell);
  int (*refresh_millihertz)(LithoHwShell* shell);

  /* Counters for the bench record. */
  uint64_t (*frames_presented)(LithoHwShell* shell);
  uint64_t (*dropped_inputs)(LithoHwShell* shell);

  /* Human-readable detail for the most recent error on this shell.
   * Owned by the shell; valid until the next API call on it. */
  const char* (*last_error)(LithoHwShell* shell);

  /* ABI 2. Hardware census / capability query. Fills *out (zeroing what
   * the backend cannot know); LITHOHW_OK, or LITHOHW_ERR_INVALID on a
   * null argument. Callable at any point between create and destroy. */
  int (*query_census)(LithoHwShell* shell, LithoHwCensus* out);
} LithoHwApi;

/* The one symbol every LithoHw backend exports. */
const LithoHwApi* lithohw_get_api(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

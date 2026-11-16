# WASM Readiness — LithoKraft Browser Build (recon, 2026-10-18)

Scope: read-only recon of the receive tree (`~/workspace/torch-receive-2026-10-17/tree/`)
plus scaffolding in this folder. No engine sources were modified. Everything below is
grep/verified against the tree as it stands tonight; where something was not verified,
it says so.

## 1. Windowing / presenter seam — the good news: there is exactly one

- **Window creation lives in one file:** `engine/src/platform/window.cpp`, behind
  `engine/src/platform/include/litholattice/platform/window.h`.
  - `class Window`, created via `Window::create(const WindowDesc&)`.
  - Wraps GLFW (`GLFWwindow*` in a pImpl), requests either a **GL 3.3 core** context
    or a **Vulkan** no-API window depending on `WindowDesc.api`.
  - Presentation is `Window::swap()`; close polling is `Window::shouldClose()`.
  - Input is a separate backend: `engine/src/platform/glfw_input_backend.{h,cpp}`
    (`GlfwInputBackend`), constructed inside `window.cpp`.
  - CMake target: `litholattice_platform` (`engine/src/platform/CMakeLists.txt`),
    linking the `glfw` target, which the top-level `engine/CMakeLists.txt` (lines
    28–32) pulls in via **FetchContent from GitHub** (vendored copies also exist under
    `_depsrc/`).

- **Software presenter path exists and is clean:**
  `engine/src/render/include/litholattice/render/software_presenter.h`, implemented in
  `engine/src/render/gl33/software_presenter.cpp`.
  - Interface: `ISoftwarePresenter::present(const uint32_t* pixels, int w, int h, int fbW, int fbH)`
    — takes a finished CPU framebuffer (`0xAARRGGBB`, top row first, the
    `SoftwareFramebuffer` layout) and blits it.
  - Factory: `createSoftwarePresenter(GLProcLoader)`. Used from
    `game/LithoKraft/src/app/main.cpp`.
  - Today the blit goes through a GL 3.3 texture upload into the window back buffer,
    then `Window::swap()`. **Browser mapping:** the same interface can be implemented
    against a `<canvas>` — either as a WebGL texture upload (closest to today's path)
    or a 2D-canvas `ImageData` put. This is the fallback renderer for hardware where
    WebGL is unavailable, and the lowest-risk first present path.

- **What this means for WASM:** the seam is narrow. A browser platform layer needs to
  replace `litholattice_platform`'s two files (window + input backend) with an
  Emscripten/HTML5 implementation exposing the same `Window` interface, and add one
  canvas presenter. Nothing else in the tree calls GLFW directly — `grep` for
  `glfwCreateWindow|glfwInit` across `engine/` and `game/` returns only
  `engine/src/platform/window.cpp`.

- **Vulkan backend** (`engine/src/render_vk/`) has no browser target; WebGL is the
  browser GPU path. Note the GL 3.3 core shaders will need GLSL ES 3.00 translation
  care under WebGL2 — not yet assessed.

## 2. Filesystem surface — inventory only (design comes later)

Pattern counts (files matching, engine + game, overlapping sets):

| Pattern | Files |
|---|---|
| `std::filesystem` | 88 |
| `std::ifstream` | 35 |
| `fopen` | 21 |
| `std::ofstream` | 16 |

Under `engine/src/` + `game/LithoKraft/src/` only: **100 files** match at least one
pattern including selftests/headless tools; **76 files** excluding
selftest/test/headless files. (An earlier hallway estimate of "~31 files" was low —
the real surface is ~76 production files.)

Concentration point: `engine/src/storage/storage.cpp` /
`engine/src/storage/include/litholattice/storage/storage.h` is the storage layer —
the right single place to later hang an IDBFS/MEMFS + export/import bridge.
World permanence in a browser will need: Emscripten FS (MEMFS/IDBFS) under the
storage layer, plus an explicit export/import of world saves, because browser
storage is evictable. **Not designed here — inventoried only, per instructions.**

## 3. Threading surface — why the isolation headers matter

- `std::thread` appears in **27 files** under `engine/` + `game/`. The structural ones:
  - `engine/src/sim/include/litholattice/sim/sim_thread.h` — `class SimThread`, the
    sim thread, fed by the serialized `ControlStream` (`control_stream.h`); lock-free
    triple-buffered stats back to render.
  - `engine/src/core/include/litholattice/core/threeway.h` — the **threeway split**
    (toggle `LL_THREEWAY=1`, default OFF): main thread (window/events/input/logic),
    render thread (GPU work from a finished frame snapshot), sim thread (ticks
    unchanged). Implemented with mutexes today; the async stages sit on
    `engine/src/core/include/litholattice/core/spsc_queue.h` and
    `engine/src/render/include/litholattice/render/sw_upload_ring.h` (lock-free SPSC
    upload ring).
  - Worker threads: `engine/src/input/input_thread.cpp`,
    `engine/src/audio/occlusion_thread.cpp`, `engine/src/audio/streaming.cpp`,
    job system in `engine/src/core/include/litholattice/core/job_system.h`.

- **Browser consequence:** Emscripten pthreads require `SharedArrayBuffer`, which
  browsers only expose on **cross-origin-isolated** pages — hence the COOP/COEP
  headers in `serve.py` in this folder. Without isolation the build must run
  single-threaded (threeway off — which is already the engine's default posture).

## 4. Emscripten toolchain — NOT installed on this workstation

- `which emcc` / `which emcmake`: **not found**. No `~/emsdk` directory. Not checked
  exhaustively beyond PATH and the standard emsdk home location.
- Install route (documented, **not performed** — the SDK is multi-GB):
  ```
  git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
  cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
  source ~/emsdk/emsdk_env.sh   # per-shell; puts emcc/emcmake on PATH
  ```
  (A Debian `emscripten` apt package also exists, but emsdk is the upstream-supported
  route and the one to standardize on. On JD's Windows machine the same emsdk flow
  applies via its `emsdk.bat`.)
- Serving/testing needs only Python 3 (3.12.3 present here) — see `serve.py`.

## 5. CMake — no EMSCRIPTEN branches today

- `grep -rn "EMSCRIPTEN|__EMSCRIPTEN__"` over all `CMakeLists.txt` / `*.cmake` in
  `engine/` and `game/`: **zero hits**. The build has platform conditionals for
  MSVC / UNIX-not-APPLE (e.g. `engine/CMakeLists.txt` lines 13–14, 24, 88) — the
  pattern an `if(EMSCRIPTEN)` branch would follow — but no web branch exists yet.
- The FetchContent GLFW (GitHub) will not be the browser route: Emscripten provides
  GLFW3 via its ports system (`-sUSE_GLFW=3`) or the platform layer can drop GLFW
  entirely (see §1). That decision belongs to the first build milestone.

## 6. Next steps (ordered)

**Status update (2026-10-19, overnight loop):** see
`OVERNIGHT-LOOP-REPORT.md` in this folder for the full checkpoint report.
Step 1 is **done** (emsdk installed at `~/workspace/emsdk`, Emscripten
6.0.12). Step 2 is **at checkpoint**: the tree configures and the full game
links under Emscripten (`build-wasm/LithoKraft.wasm`, 4.8 MB) with the
platform stub in `engine/src/platform/window_web_legacy.cpp`; node smokes
run the sim/content self-tests (122 PASS, 0 FAIL) and the demo format suite
(31/31) up to the no-pthreads thread wall. The frame itself is **not** yet
on canvas — remaining for this step: the `emscripten_set_main_loop`
restructure, asset preloading, and the canvas presenter. Steps 3–7 stand as
written below, with the threading pass (step 5's content) pulled forward as
the immediate next action since the thread wall is now the first blocker.

1. **Install emsdk** (route in §4) on the workstation; verify `emcc --version`. ✅ done 2026-10-18 (note: §4's "not installed" finding predates the install).
2. **Platform stub milestone — "frame on canvas":** an `if(EMSCRIPTEN)` CMake branch
   that swaps `litholattice_platform` for a minimal Emscripten window/input stub and
   presents `SoftwareRenderer` output through a canvas presenter (§1). Success =
   LithoKraft software-renderer frames visible at `localhost` via `serve.py`.
   *This is the first real build milestone.*
3. **Single-threaded first:** threeway OFF, no pthreads — prove the sim + software
   renderer deterministically in-browser before touching SharedArrayBuffer.
4. **GL33 → WebGL2 pass:** bring up the GL backend and the GL presenter path;
   resolve GLSL ES 3.00 shader translation.
5. **Threading pass:** enable Emscripten pthreads behind cross-origin isolation
   (headers already handled by `serve.py`; itch.io hosting supports the same
   isolation flags per-game), threeway stages on.
6. **Persistence pass:** storage layer (§2) onto IDBFS + world export/import design —
   JD decision item, not an implementation default.
7. **Package for itch.io:** static output (`.html/.js/.wasm` + assets) under itch's
   envelope (≤200 MB/file, ≤1000 files) — the native tree already sits far under it.

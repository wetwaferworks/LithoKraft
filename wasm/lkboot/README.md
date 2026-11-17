# lkboot

lkboot is the browser and Node runtime for the LithoKraft wasm build. It has no
Emscripten and no outside JS. The engine's main runs in a worker. Rendering goes
through a transferred OffscreenCanvas (lkgl.mjs, WebGL2). DOM input reaches the
engine's mapped input ring through lkpage.mjs and lkhost.mjs. Files go through
lk_store (lkstore.mjs): OPFS or memory, with the asset pack mounted read-only on top.

## Build (box)

    cmake -S engine -B wbd -DCMAKE_TOOLCHAIN_FILE=cmake/lk-wasm32-toolchain.cmake \
          -DCMAKE_BUILD_TYPE=DebugOpt -DLK_LLVM_HOME=<LLVM 20>
    cmake --build wbd -j4 --target lithokraft          # -> wbd/LithoKraft (the .wasm)
    python3 wasm/lkpack.py ... LithoKraft.lkp2         # engine + game assets, scripts

The toolchain file sets LITHOKRAFT_MODELS_DIR_PATH to /LithoKraft/assets/models (see
the note there).

## Smoke

Serve this directory with COOP same-origin and COEP require-corp. Put LithoKraft.wasm
and LithoKraft.lkp2 in tests/ (both are build outputs: don't commit them), then open
tests/kraft.html. These query flags are available:

- `?store=memory`: keep saves in memory instead of OPFS
- `?nopack`: run with built-ins only
- `?args=--flat-lab`: argv for the game
- `?touch=1`: force the touch overlay (touch.js)
- `?debuginput=1`: log every input event

The page sets window.__kraft (frames, GL calls and errors, the engine error list, and
the ring counters) for a CDP driver to read.

## Checks

    node tests/host_check.mjs
    node tests/pack_check.mjs LithoKraft.lkp2
    node tests/run-node.mjs <thread.wasm> <single.wasm>

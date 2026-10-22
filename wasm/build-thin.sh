#!/bin/sh
# build-thin.sh — the LithoKraft THIN web build (2026-10-20 rip-out).
#
# What changed vs the old web build (recorded in full in the specimen,
# ~/workspace/emscripten-analysis/specimen/build-records/):
#   The old link line (build-wasm2/CMakeCache.txt CMAKE_EXE_LINKER_FLAGS)
#   carried -sPROXY_TO_PTHREAD=1, -sPTHREAD_POOL_SIZE=8,
#   -sOFFSCREENCANVAS_SUPPORT=1 and five --preload-file mappings, and the
#   pages loaded the generated Emscripten glue (LithoKraft.js, 438,525 B).
#   That machinery wedged before first present on Android WebView
#   155.0.8059.43 (diag: transfer attempts=0, engine live=0, t=4s..32s).
#
#   The thin build links the SAME objects/libs with those four things
#   REMOVED, plus -g (keeps import names unminified so our hand-written
#   loader, wasm/full/lkboot.js, can implement the declared import
#   surface; DWARF is stripped back off below). Assets ship in our own
#   pack format (lkpack.py -> LKP2) instead of --preload-file.
#   No CMake configure is run by this script (the recorded object list
#   is replayed verbatim), so this script itself does NOT tick the
#   build counter. The tick comes from the normal-path build of the
#   objects it replays: run `cmake --build build-wasm2 --target
#   lithokraft` first (tools/build_counter.py rolls the ledger there,
#   per JD's ruling of 2026-10-20 that the counter ticks on every
#   build again — the v1.3 thin build was the one allowed exception).
#
# The generated LithoKraft.js from this link is a BYPRODUCT ONLY — it is
# never shipped or served. The pages load lkboot.js.
set -eu
TREE=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$TREE/build-wasm2"
OUT=$(mktemp -d /tmp/lithokraft-thin.XXXXXX)
EMXX=${EMXX:-$HOME/workspace/emsdk/upstream/emscripten/em++}
WASMOPT=${WASMOPT:-$HOME/workspace/emsdk/upstream/bin/wasm-opt}

# The recorded object/lib paths below are relative to the app target's
# binary dir (build-wasm2/game/src/app) — corrected 2026-10-20 (bug-fix
# run): the script previously cd'ed to build-wasm2 root, where none of
# its own relative paths resolve.
cd "$BUILD/game/src/app"
"$EMXX" -pthread -O3 -DNDEBUG -sUSE_PTHREADS=1 \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=536870912 -sTOTAL_STACK=8388608 \
  -sMAX_WEBGL_VERSION=2 \
  --js-library "$TREE/engine/src/render/gl33/lk_webio.js" \
  CMakeFiles/lithokraft.dir/main.cpp.o \
  CMakeFiles/lithokraft.dir/scripts_dir.cpp.o \
  CMakeFiles/lithokraft.dir/selftest.cpp.o \
  CMakeFiles/lithokraft.dir/perf_headless.cpp.o \
  CMakeFiles/lithokraft.dir/noise_preview.cpp.o \
  CMakeFiles/lithokraft.dir/handshake_test.cpp.o \
  CMakeFiles/lithokraft.dir/control_latency.cpp.o \
  CMakeFiles/lithokraft.dir/control_stream_test.cpp.o \
  CMakeFiles/lithokraft.dir/demo_record_test.cpp.o \
  CMakeFiles/lithokraft.dir/vulkan_stub_emscripten.cpp.o \
  -o "$OUT/LithoKraft.js" -g \
  ../game/liblithokraft_game.a \
  ../../../src/audio/liblitholattice_audio_device.a \
  ../content/liblithokraft_content.a \
  ../content/model/liblithokraft_model.a \
  ../../../src/ai/liblitholattice_ai.a \
  ../../../src/sim/liblitholattice_sim.a \
  ../../../src/render/liblitholattice_render.a \
  ../../../src/platform/liblitholattice_platform.a \
  ../../../src/entity/liblitholattice_entity.a \
  ../../../src/mcp/liblitholattice_mcp.a \
  ../../../src/camera/liblitholattice_camera.a \
  ../../../src/console/liblitholattice_console.a \
  ../../../src/script/liblitholattice_script.a \
  ../../../src/mods/liblitholattice_mods.a \
  "$TREE/build-wasm/_deps/angelscript-build/libangelscript.a" \
  ../../../src/input/liblitholattice_input.a \
  ../../../src/ui/liblitholattice_ui.a \
  ../../../src/defrag/liblitholattice_defrag.a \
  ../../../src/storage/liblitholattice_storage.a \
  ../../../src/world/liblitholattice_world.a \
  ../../../src/index/liblitholattice_index.a \
  ../../../src/projectile/liblitholattice_projectile.a \
  ../../../src/player/liblitholattice_player.a \
  ../../../src/sprite/liblitholattice_sprite.a \
  ../../../src/systerian/liblitholattice_systerian.a \
  ../../../src/procgen/liblitholattice_procgen.a \
  ../../../src/assets/liblitholattice_assets.a \
  ../../../src/assets/liblitholattice_stb.a \
  ../../../src/assets/liblitholattice_nanosvg.a \
  ../../../src/subdiv/liblitholattice_subdiv.a \
  ../../../src/crypto/liblitholattice_crypto.a \
  ../../../src/audio/liblitholattice_audio.a \
  ../../../src/core/liblitholattice_core.a \
  ../../../src/audio/liblitholattice_miniaudio_impl.a \
  -lm

# Strip the DWARF/producers sections back off (keeps the unminified
# import names the loader binds to, drops the debug weight). NOTE:
# link-time binaryen optimization passes were limited by -g in the
# recorded run; no extra -O3 pass is applied here either — the bench
# comparison is the arbiter, not a bigger hammer.
"$WASMOPT" "$OUT/LithoKraft.wasm" -o "$OUT/LithoKraft-thin.wasm" \
  --strip-dwarf --strip-producers \
  --enable-threads --enable-bulk-memory --enable-mutable-globals \
  --enable-sign-ext --enable-reference-types --enable-multivalue \
  --enable-nontrapping-float-to-int

# Our own asset pack (deterministic; replaces --preload-file).
python3 "$TREE/wasm/lkpack.py" "$TREE" "$OUT/LithoKraft.data"

# The engine's EM_ASM body table for the thin loader: the toolchain
# extracts those snippets (the engine's own C++ source text) into the
# link's glue byproduct, keyed by address; lkasm.js carries them as
# data for lkboot.js. Regenerated every build so the addresses always
# match the binary just linked.
python3 "$TREE/wasm/extract-asm.py" "$OUT/LithoKraft.js" "$TREE/wasm/full/lkasm.js"

cp "$OUT/LithoKraft-thin.wasm" "$TREE/wasm/full/LithoKraft.wasm"
cp "$OUT/LithoKraft.data" "$TREE/wasm/full/LithoKraft.data"
echo "thin build staged into $TREE/wasm/full/"
sha256sum "$TREE/wasm/full/LithoKraft.wasm" "$TREE/wasm/full/LithoKraft.data"

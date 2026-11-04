# GPU Trace: Step 1 Renderer Side, Next Notes for Elfie (CONFIDENTIAL)
October 10, 2026, 11 AM. From Cortana for JD. Credit: GPU ideas from @t3hSurge, Seth Champion, and Grok.

## Status after importing your package (branch elfie/2026-10-10 on TheWorkstation)
- It builds on Windows (MSVC) after 3 small fixes: added `add_subdirectory(src/entity)` to the root CMakeLists, `__builtin_ctz` changed to `std::countr_zero`, and a `friction` positional init in engine_tests. 334 game self-tests and the engine tests pass.
- Step 2 (bit-packed net marks, CTZ) works.
- Step 1: the CPU TraceStrengthBuffer exists, but **nothing runs on the GPU yet**. Windowed runs match the old numbers: pattern-wall 64 at 60 FPS, 512 TPS, 1.08 ms tick, **1.78 MB/frame upload (unchanged)**. Wire-torture uploads 495 KB per frame and wall-pixel 16 uploads 1.27 MB.

## What's needed to finish Step 1
1. **Mesher:** for Trace (and lamp) faces, write the cell's strength-buffer slot index into the vertex (the existing `value` attribute) instead of baking in strength or color. Keep it stable: a slot is assigned when Trace is placed and freed when it's removed. A strength change must NOT mark the section for remesh anymore. Only placement or removal does.
2. **Engine renderer (both backends, under litholattice::render):**
   - Vulkan: a storage buffer (DEVICE_LOCAL plus staging) sized to the slot capacity. Each frame, copy only `dirtyWordIndices()` words via staging, then call `uploadAck()`. Bind it as a descriptor in the chunk pipeline.
   - OpenGL 4.3+: an SSBO with `glBufferSubData` per dirty word range (coalesce adjacent words). GL 3.3 fallback: a buffer texture (TBO) with `texelFetch`.
3. **Shader (chunk vert or frag):** `uint w = strengths[slot >> 2]; uint s = (w >> ((slot & 3u) * 8u)) & 0xFFu;` Drive the four dots (dot k lit when s >= 16*(k+1)) and the ring from `s`. Lamps work the same way, with the color index.
4. **Afterglow:** move the lamp afterglow to this path (a per-slot last-lit tick or a decay in the shader) so it no longer forces remeshes.
5. **Gate and measure:**
   - Self-test: the strength read back from the buffer equals the sim for every Trace cell after N ticks.
   - Measure upload bytes per frame and p99 frame time on wall-pixel 16, pattern-wall 64, and wire-torture. **Target: upload drops to only the changed words** (KBs, not MBs), and remesh count during animation falls to about 0.
   - Run with `--display maximized --present fifo` for JD's visual runs.

## Then (optional, the enthusiast tier)
- Step 3, compiled chips on GPU compute, as in the first GPU brief (ELL layout, per-level dispatch, double-buffered state, integer-only, tick-for-tick CPU match).

## Other items from the JD test
- The cursor stays locked inside the window in bench and windowed runs. Release it on focus loss and Esc, and don't capture it during bench flyovers. Add a config default for launch mode (maximized).
- The `blue_via` model references undefined faces `side_bottom`, `side_mid`, `side_single` (manifest mismatch).
- Test screens: one-block gap horizontally AND vertically (pixel pitch 2) so the aspect stays square.
- The Pattern ROM WIP was lost in a PC drop and needs a rewrite (Pattern ROM block, Dual Catchor, `--pattern anim`).

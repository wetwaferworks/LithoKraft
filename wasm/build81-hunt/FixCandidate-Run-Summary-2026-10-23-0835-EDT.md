# Flicker-Fix Candidate — Run Summary — 2026-10-23 0835 EDT

**Package:** `LithoKraft-Itch-FixCandidate-2026-10-23-0835-EDT.zip`
**Identity (labeled, separate):** shell — none (web package; shell v1.8/versionCode 9 untouched) · rolling build counter — the engine binary still prints **0.0.0.0.0.0.1.17** (build 81's binary; a true seal with a new counter needs a rebuild and JD's lock) · game Alpha **0.0.3.21**. This is a *candidate*, not a sealed build.

## What changed
Exactly one file vs the verified itch candidate: **lkboot.js** — build-80 driver (80180d17…) → fixed driver (**4cd379ce…**), now also the tree's driver at `wasm/full/lkboot.js` (original backed up in scratch). The fix (frame-integrity, 2026-10-23):
1. **Create/Gen execute in place** — pure producers, no pre-flush of the held frame.
2. **Snapshot-at-decode** for Uniform vector/matrix families, BufferData, BufferSubData: payload copied while the producer is parked, sync reply posts immediately, record held as pure values and executed whole at frame-done — **only while a frame is outstanding** (the VAO rule; with no frame in flight the record executes in place from its copy).
3. True readers (Get family, Is, Check, Fence, ClientWait, ReadPixels) and the Tex readers keep flush-then-execute.

Engine wasm, data, pages, touch, asm: byte-identical to the candidate.

## Why (evidence chain)
- TB1/LKDIAG-2 census, local + JD's itch run (08:27 EDT, ANGLE): **97–99.9% of draws executed in sync-flushed fragments**; Uniform4fv 35k–51k sync executions/s; streaming complete at 4,501/4,501 meshes yet ~9–11 fps standing. JD: flicker present on GL, software page good.
- TB2 prototype (same fix, test-bed line): local census flipped to **flushFD = 100%, flushSync = 0**; its v1 frame-gate bug (held EAB BufferData under the wrong VAO → GL_INVALID_OPERATION → invisible world) was birth-traced and fixed by the outstanding-frame gate, which the production port carries.

## Local validation of THIS package (registrar's bench, SwiftShader)
- GL settled census: **0.0% black, 40,951 uniques, mean 59.3** (reference good scene: 0.0% / ~40.7k / 59.9). Page errors: 0. Texture-array upload error: **absent**.
- SW settled census: **0.0% black, 37,048 uniques, mean 48.3** (candidate SW reference: 0.0% / ~36.8k / 48.4). Page errors: 0.
- tps at target (256.2/256 GL run).

## Awaiting
JD's itch run (ANGLE + human eyes): flicker gone? flying fps? If it holds, this driver is the base for the next sealed build (identity is JD's lock). Archie's consensus at 10 AM still welcome on the design questions in the report addenda.

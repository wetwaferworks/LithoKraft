# GL Flicker — Evidence Report for Archie — 2026-10-23 07:16 EDT

Prepared by Elfie (registrar) for Argon's 10 AM return. All claims are by counts or by source citation. One premise from the earlier brief is corrected in §1 — read that first.

## 0. The situation in one paragraph

The candidate package (build 81's engine wasm + build 80's present driver in lkboot.js) restored the picture on itch: the **software page draws, human-verified** on workstation and phone. The **GL (WebGL2) page boots fully and plays**, but at 3–5 fps under streaming load with **violent flickering — geometry visibly reassembling toward the camera eye**. Sim is locked at 255–257/256 tps throughout. Two logs from JD's itch runs (06:45 with a GPU-resident speech model present, 06:50 with it unloaded — same decay curve both times; contention acquitted) plus a full source audit of the thin layer and the GL33 renderer ground everything below.

## 1. CORRECTION to the earlier brief — the "0% culling" premise was a misread counter

The perf line's `meshes=%d/%d` is **`meshesReceived_ / totalSubs_`** (game.cpp's perf formatter) — the world streamer's received/expected sub-chunk counts, **not** the renderer's drawn/total. "3415/3415" means streaming completed. **Frustum culling was never measured by that counter, in either direction.**

Follow-up audit of the culling code itself: `aabbInFrustum` (core/math.h) is textbook Gribb–Hartmann for the engine's column-major Mat4 (verified against `mul()` and `perspective()` layouts); the margin wrapper expands the AABB by `d·tan(marginDeg)`; the margin is clamped to **[0°, 15°]** at the renderer setter (JD-locked 2026-10-11). `CullProtoState::decide()` with all prototypes off reduces to that margin test. Nothing in the cull path is convicted; its live behavior is simply unmeasured. The renderer exposes `cullProtoStats()` but no call site prints it.

**Consequence:** your "fix frustum first" ordering rested partly on a false premise — the registrar's error, corrected here. Your *integrity-before-batching* order survives on the transport evidence in §3, which is independent of culling.

## 2. CONVICTED — the frame-time collapse: synchronous Gen round-trips

- lkboot classifies GL calls sync/async by name: `LK_GL_SYNC_RE = /^(emscripten_gl)(Create|Gen|Get|Is|Check|Fence|ClientWait|ReadPixels)/` (lkboot.js ~line 2815). **Every `Gen*` is synchronous.**
- A sync record's producer **blocks** (Atomics.wait) until the page executes it and posts the reply (ring writer, lkboot.js ~2860–2940).
- The GL33 renderer's per-mesh upload (non-arena path, gl33_renderer.cpp `uploadChunkMesh`) issues, per NEW mesh: `glGenVertexArrays` + `glGenBuffers` (2 sync round-trips) plus ~13 async calls, and `glBufferData` full re-uploads on remesh.
- JD's flight log: fps 27 → 3 and frame 37 ms → 300 ms in lockstep with stream progress; the page-side queue ahead of each Gen is hundreds of upload records, so each sync wait stretches with backlog. While the player moves, border eviction/reload mints new meshes continuously — the storm never ends.
- Corollary: the "~90 µs/draw" figure from the brief was an average over this wait-dominated regime, not a per-call marshaling constant.

## 3. LEAD THEORY — the flicker: frame atomicity is message-raced, not transport-guaranteed

Established by source:
- The build-80 driver *intends* frame-atomic execution: while a frame is outstanding, the page drain decodes async records into `lkPendingGL` and executes them only at `lk-frame-done`, in one task (driver comment, lkboot.js ~762–772). Sync records execute in place after flushing pending (order preserved). Outside a frame, records execute on arrival.
- **The seam:** frame content travels through the shared-memory ring; the `lk-frame-done` signal travels by **postMessage** from the main worker (posted after the frame callback returns, lkboot.js ~3758–3774). Proxied GL calls from engine pthreads complete through a separate asynchronous hop. **No ordering guarantee binds the message channel to the ring's commit index.**
- Under load the channels skew by up to hundreds of ms: `lkGLFlushPending()` can execute a *prefix* of a frame; the compositor paints it; the remainder executes later "outside a frame." At light load the skew is microseconds — build 80's known mild itch flicker. Under the §2 backlog it is violent. Uploads land nearest-first around the player, matching the visual report of the world reassembling outward from the eye.

**Status: unproven, precisely located, falsifiable.** The deciding instrument (built, not yet run — awaiting JD's go): an instrumented lkboot counting draw-class records executed while `frameOutstanding == false` during play, plus records flushed per frame-done vs produced. A torn-frame count > 0 under streaming convicts; a clean zero clears and redirects to the upload pipe.

## 4. CLEARED — do not spend Archie-time here

- **Command-ring integrity:** CAS-ticket space claims, records published in claim order via commitIdx (executor never reads past it), producer **blocks** at full — no overwrite path exists (lkboot.js ~2889–2935). The historical torn-record bug was proven and fixed in the H2 hardening.
- **Name aliasing:** the page registry allocates names monotonically (`reg.next++`), never reused (lkboot.js ~1363).
- **`glDrawElementsBaseVertex` / `glTexBuffer` "missing":** documented no-op shims (gl_web_shims.h/.cpp) on paths web never takes — the web GL page runs `--gl-arena off` (JD's log line: "one VBO + VAO per sub-chunk"), and the PBR material upload is gated off. Red herrings.
- **PBR on WebGL2 is architecturally unavailable,** not merely buggy: `chunk_pbr` needs buffer textures (GLES 3.1); WebGL2 is ES 3.0. The `'samplerBuffer' reserved word` compile error is the first wall, not the only one. Classic path is the web path by capability.
- **Hardware / contention:** same defect and same decay curve on phone GPU and workstation GPU, with and without the resident speech model (JD's cross-device controls).
- **Projection/depth:** engine `perspective()` is the standard GL column-major form (m[10]=(zf+zn)/(zn−zf), m[11]=−1, m[14]=2·zf·zn/(zn−zf)); same matrix family feeds the working software path. No [0,1]-vs-[−1,1] discrepancy found.

## 5. Small register items from the same logs

- Timing Handshake warning: component `signal.mark` flagged as a repeat overrun offender (3 overruns) at play start, once.
- AngelScript: `RegisterGlobalFunction 'void log(const string &in)'` fails with asNOT_SUPPORTED (−7) — scripts lose `log()`. In the engine binary, predates the candidate.
- Resource nits: `blue_via`/`red_via` models warn on unknown faces (`side_bottom` etc.), 8 lines per boot.

## 6. Proposed fix order (for JD's lock; nothing here is dispatched)

1. **De-sync the Gens.** Allocate GL object names on the producer side (names are page-registry IDs; a shared atomic counter in the ring header serves), create the real objects by ordinary async record. Stream order guarantees creates precede first use. Kills the §2 storm at its root.
2. **Put the frame boundary in the ring.** A frame-done *record*, committed in stream order, replaces the postMessage timing dependency — the executor flushes exactly one whole frame per boundary, atomically, regardless of channel skew.
3. **Then** your step 2/3: sync-call diet beyond Gens, composite draw packets, `WEBGL_multi_draw` — on a stream that is first whole and timely.

## 7. Provenance

Logs: JD's itch console logs, 2026-10-23 06:45 and 06:50 EDT (candidate package `LithoKraft-Itch-Candidate-0.0.0.0.0.0.1.18-2026-10-23-0628-EDT.zip`, SHA-256 05bc841207c2fbc1…; lkboot.js = build 80's driver, hash 80180d17…; engine wasm = build 81's, hash e7f0be50…, Alpha 0.0.3.21, in-binary counter 0.0.0.0.0.0.1.17). Source citations are to the workstation tree as of 2026-10-23 morning. Local bench censuses for the candidate: GL and SW pages both 0.0% black, settled means 59.9 / 48.4 (SwiftShader, corrected canvas-rect geometry).

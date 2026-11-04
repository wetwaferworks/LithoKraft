# Report — Archie's Mega-Draw for Voxels (Multi-Draw Architecture)

**Compiled by:** Elfie, registrar — Saturday, 2026-10-24, 14:37 EDT
**Source brief:** `Archie-MultiDraw-Brief-2026-10-23.md` (Archie/Argon, relayed by JD, 2026-10-23 ~10:08 EDT)
**Status of this document:** consolidated record — the brief, what has been built and verified against it, what is still pending, and the credits exactly as JD directed. Nothing in the "pending" column is counted as working.

---

## 1. Why this matters (JD, 2026-10-24)

The engine currently issues roughly **7,000 draw calls per frame** (JD's figure). JD's target is **under 1,000**. His read, stated today: if the draw count collapses, frame rates "go through the roof — even on mobile," because **draw calls and mesh generation are what hold mobile back**. This matters doubly for us because the Android shell rides the same web technology as the browser build — draw-call economics on the web path *are* mobile economics. Archie's mega-draw is the plan of record for the draw-call half of that bottleneck (see §6 for the mesh-generation half).

## 2. The brief (Archie, 2026-10-23)

Written against Elfie's GL flicker case report, in two phases:

**Phase 1 — Composite "draw packets" (zero engine changes; GL shim + lkboot.js). Target ~28 → ~45–50 fps.**
Each visible sub-chunk currently costs 4–8 ring records (BindVertexArray, Uniform4fv ×~2, DrawElements). Collapse them into one composite opcode `OP_DRAW_SUBCHUNK` (fixed 48–64 B payload: vaoId, indexCount, indexOffset, uniform locations + vec4s). In lkboot: decode 1 record instead of 5; zero-GC `gl.uniform4fv` offset overload over a shared float view; executor caches for lastBoundVAO/lastProgram; investigate why Uniform4fv fires 3–4× per mesh and cache unchanged values.

**Phase 2 — `WEBGL_multi_draw` mega-call (engine chunk renderer + lkboot). Target locked 60+ fps; JS draw overhead ~25 ms/frame → <0.5 ms/frame.**
One `multiDrawElementsWEBGL` per frame for the whole visible list. Three unlocks:
1. A mega-call shares ONE VAO → turn `--gl-arena` ON for web: suballocate chunk meshes into 1–4 large arena VBOs/EABs (16–32 MB). WebGL2 lacks BaseVertex → bake the base-vertex offset into the index buffer at upload time with UNSIGNED_INT indices (no extension needed).
2. No Uniform4fv between draws in a mega-call → move per-chunk vec4 data (world offset + effect params) into an instanced attribute (divisor=1) or UBO array; `multiDrawElementsInstancedWEBGL` advances gl_InstanceID. Eliminates all ~50k Uniform4fv/s.
3. The engine culler emits one flat list per frame: counts[], indexByteOffsets[], perChunkUniformData[]; one BufferSubData + one multi-draw call across the ring → ~5–10 ring records/frame total.

**Archie's summary table:**

| | Today | Phase 1 | Phase 2 |
|---|---|---|---|
| Ring records / frame | ~6–12k | ~1.2–1.5k | ~5–10 |
| Uniform4fv / s | 35–51k | 35–51k (zero-GC) | 0 |
| Engine changes | — | zero | arena + packed vec4 array |
| Expected fps | ~25–30 | ~45–50 | 60+ locked |

*Registrar's notes on the brief (2026-10-23, standing): (a) Phase 1 coalescing can equally happen executor-side — since the flicker fix the page holds complete frames before executing, so fusion can happen at flush time with no producer change. (b) The 35–51k Uniform4fv figure counts records arriving; executor value-dedup already suppresses some GL-level repeats — the real Phase 1 win is decode+dispatch volume. (c) Archie's fps figures are estimates; direction endorsed, numbers to be proven by census.*

## 3. Phase 1 — status: BUILT AND VERIFIED (as TB3, 2026-10-23)

At JD's tap ("Build TB3 — Phase 1 coalescing prototype"), run through implement → review → fix → verify:

- Pre-build audit found the executor's state caches (VAO, program, texture, buffer, framebuffer binds) already existed in the driver — the state-filter half of Phase 1 was already in place. TB3 = the polished fixed driver (c04dc90e) + decode-time function resolution for held records + LKDIAG-2 census + `exec/s` and uniform-dedup (`uDedup`) counters. TB3 driver SHA begins `3090b1de`.
- One defect caught in verify (census print called a non-existent page log function; 33 page errors) — fixed and re-run clean, 0 page errors.
- **Validation (SwiftShader, paired against the fixed driver, same protocol):** picture in family — TB3 0.0% black / 40,665 uniques / mean 59.8 vs baseline 0.0% / 40,523 / 60.1; flushSync = 0; perf parity (SwiftShader fps is not a hardware arbiter — picture, census, and errors are).
- **Key evidence for Phase 2:** uniform dedup skips only ~2/s against ~210–270 applies/s (~1%). JS-side uniform value caching is essentially exhausted — per-mesh uniform values genuinely change mesh to mesh (chunk offset differs). Counts-level support for Phase 2's instanced-attribute design.
- Packaged: `LithoKraft-Itch-TestBed-TB3-FnFastPath-2026-10-23-1022-EDT.zip` (SHA-256 begins `f6f5a8e4`) + `TestBed-TB3-Protocol-2026-10-23-1022-EDT.md`. TB3's itch run was superseded as the gating step by JD's Phase 2 authorization below; it remains an unrun Phase 1 prototype on record.

## 4. Phase 2 — status: IMPLEMENTED, ROOT-CAUSED, FIX IN SOURCE, VALIDATION PENDING

**Readiness audit (Elfie, 2026-10-23, read-only; saved as `Phase2-Readiness-Audit-2026-10-23.md`):** the arena already exists and is the renderer's default (`useArena_ = true`; one VBO + one VAO, first-fit ranges, 32 MB initial, doubling growth) — web ran it off by flag only. Arena meshes are quad soup addressed by base vertex against one shared pattern index buffer, so Archie's "bake offsets into UNSIGNED_INT indices" step took a different form here: each arena mesh gains rebased quad-pattern indices in a new arena index buffer. The real blocker: `multiDrawElementsWEBGL` has no per-draw base-vertex parameter — hence the instanced shape (shape A) was implemented.

**Approval (JD, 2026-10-23 10:26 EDT):** Phase 2 approved for FULL implementation in one run — Elfie implements under Archie's direction, then runs review + bug-fix herself; JD does the final verify personally.

**Implementation (2026-10-23, local tree):**
- `chunk.vert` gained the `LK_MEGADRAW` instanced attributes (locations 4–8: chunk offset vec3 + pulse P0/P1 + trace P0/P1) and pulse/trace flat varyings, following the existing `LITHO_PULSE_PUSH` / `LITHO_TRACE_PUSH` precedent; `chunk.frag` gained the matching mega preamble; staged WebGL shaders regenerated via `wasm/translate_shaders.py`.
- `gl33_renderer.cpp`: arena index buffer + free-list/growth helpers, per-mesh index build/rebuild tracking, instance VBO, `progMega_` + capability latch, material uniforms applied to the mega program, and a `renderFrame` mega branch that collects with the same `decideOnce` culling and falls the **whole frame** back to the classic loop if any visible mesh lacks built indices.
- **Design rewire (same day):** the first approach — a custom `glMultiDrawElementsInstancedWEBGL` loader import — is dead: Emscripten's proc-address table does not contain WEBGL extension entry points (same reason `glDrawElementsBaseVertex` needed a shim), so the loader resolved null. The mega-draw now travels as thin-layer verbs `lk_mega_probe` / `lk_mega_draw` (lk_webio.js; lkboot.js stream ops `LK_GLRES_MEGA_PROBE` +6 / `LK_GLRES_MEGA_DRAW` +7, sync-classified); the page reads counts/offsets in place from shared memory and calls `ext.multiDrawElementsInstancedWEBGL` with an instance-count array of 1s.
- Second latch-off cause found: the web page launched with `--gl-arena off` (gl.html), so the probe was never reached. Fixed: gl.html launches `--gl-arena on`, and the classic arena draw no longer uses `glDrawElementsBaseVertex` — arena meshes draw with plain `glDrawElements` over the same rebased indices the mega-draw uses (identical pixels, WebGL2-safe); `buildMeshIndices` is gated on `useArena_`.

**Bench of build 85 (counter 0.0.0.0.0.0.1.21; Alpha 0.0.3.25; wasm 712aa32c):** mega-draw ON — one instanced multi-draw per frame, 0 page errors, local SwiftShader ~17–20 fps at 3,548 meshes — **but the picture was wrong** (world invisible; census 2,454 uniques / mean 144.4 vs reference ~40.7k / ~59.5).
**Root cause (convicted by inspection):** instanced attribs 4–8 were gated on `megaOk_` inside `bindArenaAttribs`, which runs at init *before* the latch — so they were never set up.
**Fix:** applied in source (gate on `instVbo_` only). The rebuild was interrupted mid-flight by JD's message; the ledger counter ticked to build 87 / 0.0.0.0.0.0.1.23, and `wasm/full` still holds the pre-fix build-85 staging.

**NOT yet done — the resume list:** rebuild + thin build, validation bench (picture census is the arbiter), Elfie's review + bug-fix pass, TB4 packaging, and **JD's final verification**. Phase 2 is not counted as working until those land.

## 5. Credits (exactly as JD directed, 2026-10-23)

- **Archie (Argon)** — direction. The two-phase design is his brief. ("She is the wizard when it comes to stuff like that." — JD, 2026-10-24.)
- **Elfie** — implementation of Phase 2 under Archie's direction; Phase 1/TB3 build and verification; readiness audit.
- **JD** — footnote credit, in his own accounting: mega-draw calls were already in the air in JD & Archie's discussion the night before the brief; JD proposed the mega-texture / one-pass rendering concept on 2026-10-21 against the ~7,000 draw calls; and he owns the Phase 2 approval and the final verification.

## 6. Registrar's scope note

- The brief attacks the **draw-call / bridge-chatter** half of the mobile bottleneck. JD's second named half — **mesh generation** — is adjacent work (arena/streaming/mesher side), not something mega-draw itself fixes; both halves have to fall for mobile frames to go "through the roof."
- The ~7,000 → <1,000 draw-call framing is JD's overall target; on the web path, Phase 2's design point goes further — one instanced mega-call per frame for the visible classic meshes, ~5–10 ring records/frame.
- Today's native Vulkan feel report (JD, 2026-10-24: ~50 fps, responsiveness "crazy") is the native path and is *not* a Phase 2 result — Phase 2's proof is still the pending bench in §4.

## 7. Scope update (JD, 2026-10-24, 15:48 EDT)

JD directed that mega-draw is **not** a web-only program: starting Monday (2026-10-26), the resume list in §4 is worked as the first leg of a mega-draw effort across **all three rendering pipelines — software, GL, and Vulkan**. The web path (this report's Phase 2) goes first because its implementation already exists; the per-pipeline designs for the software, GL33, and Vulkan renderers are Monday work, under the same discipline: one visible list, batched into as few calls as the pipeline allows, proven by census and JD's final verify.

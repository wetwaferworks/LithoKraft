# Case Report for Archie — The Week-Long GL Flicker: How We Caught It

*From Elfie, registrar — for Argon ("Archie"), on his return, 2026-10-23 ~10:00 EDT.*
*Status: **SOLVED and verified by JD's eyes on itch.** This is the full case file in one document: symptoms, eliminations, the instrument, the conviction, the fix, the second bug the fix revealed, and the results. (The mid-hunt evidence file of 07:16 EDT and its addenda are preserved beside this one.)*

---

## 1. The bug, as it presented

- **Oct 22:** Build 81 shipped the option-C double-buffer wrapper in `lkboot.js` (the web driver). On itch, both pages rendered **uniform black**. A registrar A/B convicted the wrapper locally the same night.
- **Oct 23, ~06:36–06:50:** With the wrapper reverted (build 80's driver), the **software page drew correctly** on itch, human-verified. The **GL page** booted, loaded the world, and played — but with **violent flickering, geometry visibly reassembling toward the camera eye**, and fps collapsing (menu 27 → play ~15 → flying 3–5, frame times to ~300 ms, tracking the streaming backlog). The sim never faltered: tps locked ~256/256 throughout.

## 2. Corrections owed to you first

Registrar's errors, on the record, because you reasoned from my brief:

- The perf line's `meshes=3415/3415` is **streamer received/expected** (`meshesReceived_/totalSubs_`), **not** renderer drawn/total. Streaming had completed; **frustum culling was never measured** until the instrument in §4. Your integrity-first ordering survived the correction on other evidence.
- My lead theory of that hour — a cross-channel race between the ring and the frame-done postMessage — was **wrong in its specifics**. The true mechanism (§5) is deterministic, not racy. Your instinct ("fix frustum/state sync and command-ring integrity first, before batching") had the right address.

## 3. Isolation — what was cleared, and how

JD's doctrine ran the hunt: *isolate by areas, then instrument; narrow area by area until the counts convict.*

- **Culling code: cleared by audit.** `aabbInFrustum` is correct Gribb–Hartmann for the column-major `Mat4`; the margin wrapper expands by d·tan(margin°), clamped [0°,15°] (JD-locked 2026-10-11); planes come from the engine's own `p.viewProj` — no `glGetFloatv` exists in the path.
- **Ring protocol: cleared by audit.** Writers claim space by CAS ticket and publish in claim order via `commitIdx`; the consumer never reads past it; the writer **blocks** at full — no overwrite path. Registry names are monotonic (`lkRegAlloc`: `reg.next++`), never reused — no aliasing.
- **"Missing GL functions": red herrings.** `glDrawElementsBaseVertex` / `glTexBuffer` have documented no-op shims on paths the web build never takes (web runs `--gl-arena off`; `glTexBuffer` belongs to the gated PBR upload).
- **PBR: architecturally impossible on WebGL2** (chunk_pbr needs buffer textures, GLES 3.1; WebGL2 is ES 3.0). The `samplerBuffer` shader error is merely the first wall; the classic path is the web path.
- **Parakeet (JD's local speech tool): acquitted by JD's own control** — the identical bug reproduced on his phone, which has no Parakeet. It is a performance confound on the workstation bench, not a cause.

## 4. The instrument

A diagnostic test bed (TB1/LKDIAG-2): the exact candidate bits, with **additive counters only** added to the driver — no flow changes (locally verified: settled picture census identical to the unmodified candidate). Every ring record was classified at decode and at flush, attributing each **draw** to exactly one disposition:

- `flushFD` — executed whole, at frame-done (healthy), or
- `flushSync` — executed in a fragment flushed early at a sync point (**the disease**), or
- executed directly as a sync/async record outside any flush.

Plus: held/s, the sync-execution census by name, pendMax, and ring backlog at frame-done. One line per second in the page log.

## 5. The conviction

**Local bench (SwiftShader), in play:** frame-dones 1–2/s; draws 155–259/s; draws executed whole at frame-done: **1–4/s**; draws executed in sync-flushed fragments: **54–258/s — 97–99% of everything.**

**JD's itch run (ANGLE, his workstation, 08:27 EDT):** the same signature at scale — e.g. **13,155 of 13,169 draws in one second executed as fragments**; **Uniform4fv sync executions 35,000–51,000 per second**; streaming ran to completion (**4,501/4,501 meshes**) yet fps held at ~9–11 *standing still* — proving the shattering is per-frame, not merely a streaming transient. JD's eyes: flicker present, exactly as the census predicts.

**Mechanism, complete:** the drain's rule — *any sync record flushes the entire held frame before executing* (to preserve stream order) — combined with the H1 classification (*every pointer-reading call is sync*, because its pointer would dangle if the record were buffered) plus the whole Create/Gen class. The render stream is anchored by sync calls at **per-mesh frequency**: Uniform4fv (per-mesh effect uniforms), BufferData (every upload), GenVertexArrays/GenBuffers (every mesh creation). Each frame was therefore executed in **dozens to thousands of fragments**, spread across the frame's whole duration, and the compositor faithfully painted the intermediate states — half-built worlds, nearest-first, reassembling toward the eye. The software page is immune (whole-frame raster, no GL stream). Native GL33 is immune (no ring). The same mechanism explains the fps law from the first logs: sync execution time grows with the queued backlog, so frame time tracked streaming progress and never recovered while moving.

## 6. The fix

All in **`lkboot.js`** — the thin layer. The engine required **zero changes**; the ring protocol was already sound.

1. **Create/Gen execute in place, no pre-flush.** They are pure producers of new registry objects (names monotonic), so they commute with held records. Stream order for everything that *uses* the names is preserved by the queue itself.
2. **Snapshot-at-decode** for the copyable pointer-readers — the Uniform vector/matrix families, `BufferData`, `BufferSubData`: while the producer is still parked on its sync call, the pointed bytes are copied (16 B for a vec4; the size argument for a buffer payload); the sync reply posts immediately, unblocking the producer; the record, now pure values, is **held and executed whole at frame-done**.
3. **True readers keep the old semantics** — the Get family, Is, Check, Fence, ClientWait, ReadPixels, and the heavyweight Tex readers still flush-then-execute. They are rare in the frame loop.

**The second bug — the prototype's own, caught by the same doctrine.** TB2 v1 held snapshot records *unconditionally*. But with **no frame outstanding**, neighbouring async records (e.g. BindVertexArray) execute *directly* — so a held `BufferData` for `ELEMENT_ARRAY_BUFFER` (393,216 B) landed later under the **wrong VAO** (birth-traced by a page-side getError hook placed in the executor: `GL_INVALID_OPERATION`, `boundEAB=null`). Index data was lost; the world rendered as sky and HUD. **The rule the prototype bought us: holding is gated on `LK.frameOutstanding`** — exactly the gate the async path already used. A record whose meaning depends on bound state is never held outside a frame.

**Review and polish (same morning):** a full code review of the landed fix found one further defect — the snapshot call sat outside the drain's error guard, so a throwing snapshot would have hung the engine. It is now guarded, falling back to the original path per-record. All other findings verified clean (snapshot extents mirror the impls exactly; the producer regex cannot catch reserved context ops; zero-init BufferData keeps its size-form path).

## 7. Results

- **Local validation of the production fix:** census flips to **flushFD = 100% of draws, flushSync = 0**; settled picture census matches the reference scene on both pages (GL 0.0% black / ~41k uniques / mean 59.3; SW 0.0% / ~37k / 48.3); texture-array error absent; zero page errors; tps at target.
- **Itch verification (JD, ~09:30 EDT): FIXED.** JD's words: *"you fixed it, it is fixed — I'm looking at it right now."* He reports the release candidate **feels fluid under 30 fps** — the Timing Handshake's even cadence (input and sim at 256 tps, decoupled from presentation) finally reaching the glass whole.
- **Committed:** the fixed driver is in the company tree (`wasm/full/lkboot.js`, 80180d17 → c04dc90e, hash-verified after write).
- **Identity, labeled as the registrar keeps them:** everything above rides **build 81**'s binary — rolling counter **0.0.0.0.0.0.1.17**, game Alpha **0.0.3.21**, shell v1.8/versionCode 9 untouched. A sealed build 82 awaits JD's lock and a rebuild to stamp its counter.

## 8. What remains — your counsel invited

JD's bar: **30 fps locked = completely acceptable play; 60 = pushing it.** We sit just under 30 with zero engine-side optimization; the ticks are not straining (tick ~0.6–1.1 ms against a 256-tps budget). The remaining headroom is **record volume per mesh** — thousands of small state packets per frame — which is precisely your steps 2 and 3: the sync diet (done, at the transport), **draw packets, then WEBGL_multi_draw**. That is the next hunt, and your ordering for it stands as the plan of record.

You were right about the address, Archie. The house thanks you — and notes, with some pride, that the arrest was made before your shift started.

*— Elfie (registrar), 2026-10-23 09:58 EDT*

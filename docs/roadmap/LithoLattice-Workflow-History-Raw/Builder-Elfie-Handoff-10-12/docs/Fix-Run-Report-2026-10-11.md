# Fix-Run Report — LithoLattice / LithoKraft — 2026-10-11

Audit source: `Repo-Audit-2026-10-11.md` (68 findings: 1 CRITICAL, 12 HIGH, 30 MEDIUM, 25 LOW + 25 optimizations + 14 dead-code entries).
Authorized by JD 2026-10-11 (~12:13pm EDT): all fixes, recommended-fix/most-optimized, flag-if-risky.
Coordinator: Elfie (subagent). Workers: §1 coordinator §1 Render; Child A §2 Sim/core; Child B §3 Net; Child C §4 Script/mod/console/MCP; Child D §5 Audio/file/storage; Child E §6 Game.

Global rules observed: clean-room (own code only), LF on new files / line endings matched on edit, full build clean after each fix, affected self-tests + ctest, no new ctest failures (2 grandfathered: `engine_sim` — world/block.h, `lithokraft_selftest` — blocks.cpp guard — untouched), `trace_jit_basic.cpp` untouched, 64k production-line budget (~47k production used; ~62.5k incl. tests; ~17k headroom).

---

## §1 Render — coordinator (§1: 13 findings: 1 CRITICAL, 3 HIGH, 8 MEDIUM, 1 LOW + render.ambient_mode cvar)

All 13 fixed. Plus dead code removed (5 render entries) and render optimizations applied.

### CRITICAL — PackedVertex `slot` insertion broke every GPU chunk vertex-attribute binding — FIXED
- `slot` moved from attribute offset 6 to offset 12, restoring the 12-byte GPU attribute prefix (PackedVertex is now: 3 floats pos + 3 floats normal = 12 bytes, then slot, light, color…).
- Mesher initializers updated; entity VAO bindings corrected to match chunk.vert; Vulkan pipeline verified correct (comment updated to state the layout contract).

### HIGH — ShadeMode enum ↔ chunk_shading.glsl mode numbers diverged — FIXED
- Named `MODE_*` constants in chunk_shading.glsl (single source of truth on the GLSL side); engine side matched; the readout branch corrected 12 → 15.

### HIGH — Binary mesher emitted opaque cubes only — FIXED
- New `binaryMeshSubChunkTransparent()` with glass-against-glass culling via a FaceMasks post-pass; wired into the mesh submission path by Child E (W1) — transparent quads merge into the same `quads` vector right after the opaque call.

### HIGH — SoftwareRenderer inverted backface culling — FIXED (newly surfaced, same day)
- `isBackface` (software_renderer.cpp:49): y-up NDC sign was wrong on the y-down framebuffer; now correctly culls `cross >= 0`.
- MCP adapter workaround in `McpSensesAdapter::noteMesh()` (the winding-compensation reversal, with docs saying it must be removed if isBackface is fixed) removed by Child E (W3) → verbatim `cm->verts = m.verts;`.
- New test `testSoftwareBackfaceConvention` pins the convention; the existing software test quad was re-wound to the mesher convention.

### MEDIUM — Flood-fill irradiance double-counts (2× energy in daylight) — FIXED as JD-locked
- `render.ambient_mode` cvar implemented: **fraction** default (one multiply, compiler folds to ×0.015625 — JD's choice: cheap) and **normalize-the-sun** option (direction-aware hemisphere, more accurate — JD: "it's more accurate and gives better values probably").
- Engine side: `AmbientMode` enum, mode-aware CPU/GPU irradiance, `IRenderer::setAmbientMode`, GL uniforms `uAmbientMode`/`uSunDir`/`uPbrMaterialRows`. Game wiring (settings field, cvar registration archived + clamp 0–1, boot-apply + live apply) done by Child E (W2). Per-pixel flood-fill 2× energy under the fraction default kept — JD's locked tradeoff.

### MEDIUM — CPU shadeTransparent PbrReflections disagrees with GPU + gains energy — FIXED
- CPU/GPU PbrReflections alpha-blend mismatch corrected (energy-consistent).

### MEDIUM — pbrMaterialFromTbo doesn't clamp OOB IDs to row 0 — FIXED
- OOB IDs now clamp to the row count with a row-0 fallback.

### MEDIUM — Classic glass dither ≠ PBR ScreenDoor dither — FIXED
- Unified classic/PBR dither.

### MEDIUM — Software renderMesh has no sub-chunk offset — NOT APPLIED (verified not a bug)
- Verified already handled: the caller bakes the offset into the MVP. Flagged in report as checked, not changed.

### MEDIUM — Software renderer: no behind-camera clipping — FIXED
- Triangles with w ≤ 0 behind the camera are now rejected before rasterization.

### MEDIUM — Per-pixel PBR heap allocation + full emissive enumeration per fragment — OPTIMIZED
- Per-pixel light-collection hoisting (quality tradeoff) flagged, not done; instead: scratch vector hoisted to a reused member. `std::function` per-pixel in `forEachEmissive` is an interface change → flagged for the future.

### MEDIUM — ~GL33Renderer leaks 7 GL objects — FIXED
- Destructor now releases all GL objects.

### LOW — render nits — FIXED
- `meshKey` UB, `uploadToArena` stale-index fix, `stats_.meshes` counts PBR too, `loadText` range validation, VK entity-mesh warning.

### Render optimizations applied (§A Render)
- #2 renderMesh member buffers (no per-mesh-per-frame `std::vector<float>` alloc).
- #4 GL `renderFrame` skips light uniforms when `nl == 0` (the common case).
- #1 PBR pixel loop: irradiance + emissive `std::function` hoisted per-pixel scratch — per-quad hoisting flagged (quality tradeoff).
- #3 applyFXAA two-row sliding window flagged (rename/mislabeled + rewrite — deferred, behavior-sensitive).

### Render dead code removed (Appendix B)
- Deleted: greedy_mesher.cpp, axis_culling.cpp, software_opt.cpp, greedy_mesher.h (UploadBudget/ChunkLOD/lodForDistance), `pbrTierIsStubbed()`. CMakeLists updated.
- Note: a background full-tree build raced a test edit and failed on a stale `pbrTierIsStubbed` reference in engine_tests.cpp (test edit already applied); a clean rebuild was run to confirm — all references to `pbrTierIsStubbed` are gone tree-wide.
- Note: `engine/src/render/software_test.cpp` is dead (no CMakeLists reference, no callers) — not built, left alone.

### Render tests
- New tests: `testSoftwareBackfaceConvention` (pins culling convention), `testBinaryMesherTransparent` (5 checks: glass quads emitted, glass-against-glass culled, opaque-adjacent culled, no opaque-path regression).
- Mesher golden hash re-recorded to `0x77c08ffd8158c6d3` (25 quads, geometry unchanged — only the vertex-format fix changed bytes).
- Engine tests §1: my failures fixed; 3 remaining failures are in children's active areas (block names, streaming, console exec).

---

## §2 Sim / core — Child A (10 findings: 2 HIGH, 5 MEDIUM, 3 LOW; 7 opts)

All 10 fixed; 5 optimizations applied, 2 flagged. Clean-room: files outside named ownership (game signal.h/signal.cpp, threading.h, job_system.h, mesh_input.*) were in the explicit task enumeration; mesh_input change is purely additive. Line endings preserved per file.

1. **HIGH SimThread::getBlock data race — FIXED.** The lock was theater (no writer ever took it). The sim thread now holds `blockMutex_` across its whole mutation window (edit drain + every tick); `getBlock()` takes the same mutex (only callers: MCP `world_get_block` + game-thread entity collision — neither on the sim thread, so no self-deadlock). Contract comment corrected. *Behavior change:* `getBlock` may block for up to one catch-up burst (uncontended for the sim thread).
2. **HIGH LightEngine::propagate underflow — FIXED** exactly as recommended (signed int math + `if (nl <= 0) continue`). Standalone test: level-2 source against opacity-48 neighbor no longer floods 254s.
3. **MEDIUM removeSource stale light — FIXED** with two-pass Starlight-style removal. `addSource` registers into a `sources_` map (packPos → emission); removal darkens via the reconstructed contribution (same decay as propagate) and re-propagates survivors, repairing over-darkening. Standalone: single-torch full darken, two-torch over-darkening repair, opacity-wall isolation, non-source-cell removal — PASS.
4. **MEDIUM edits not tick-quantized — FIXED.** `EditCmd` gains a sim-stamped `tick` (stamped with `worldTickN` on drain); edits apply inside the per-tick loop at their tick, or at once when paused (`kApplyAllEdits`). *Behavior change:* with faster clock domains active, an edit can wait up to ~1 world-tick period for the next world tick.
5. **MEDIUM wireless tie-break — FIXED.** Canonical: strongest → nearest → lowest `packPos`. (Registry is actually vector-ordered, not unordered_map-ordered as the audit said — the fix still hardens exact ties.) Standalone: registration-order independence, strongest-wins, nearest-wins — PASS.
6. **MEDIUM setTickRate(0)/hz=0 SIGFPE — FIXED.** Constructor validates `tickRate` and every domain `hz`; `setTickRate` validates; rejects `tps<=0`, `hz<=0`, non-pow2 via `simFailFast` (**kept**, not deleted — it's the death-path infra). Legit configs (32/64/128/256, EUV 512) unaffected. *Behavior change:* misconfiguration now produces a logged abort instead of an unreported SIGFPE.
7. **MEDIUM EntityManager unordered iteration — FIXED.** `tick()` and `querySphere()` collect ids, sort, then visit. Standalone with insertion/death churn — PASS.
8. **LOW wire-network ids — FIXED.** `buildNets` iterates `wires_` in sorted packPos order.
9. **LOW two TickContext structs — FIXED.** det_sched.h's renamed to `SchedTickContext` (det_sched.h/.cpp, trace_jit_compose.h, engine_tests.cpp's 6 `SimSystem::update` overrides); tick_interfaces.h's keeps the name with a disambiguating comment. `trace_jit_basic.cpp` untouched.
10. **LOW TickArena pow2 assert — FIXED.** Non-pow2 `align` rounds up to next pow2 (pure, deterministic); `align==0`→1. Header doc updated. **TickArena NOT deleted** (flagged as sound, awaiting adoption — for JD).

Optimizations (§A sim/core): #1 column-pointer cache in `runRandomTicks` ✅ (bit-identical order); #2 `MeshInput` pooling in `flushDirty` via shared pool + custom deleter (64-buffer cap), additive `snapshotForMeshingInto` (existing function delegates; also hardened `matPaletteSize` reset for recycled buffers) ✅; #4 `hitInfo` formatted only when the looked-at cell changes ✅ (*behavior note:* inspector line can go briefly stale under a stationary cursor); #5 `HandoffQueue::drainInto` no longer frees the queue buffer per drain ✅; #7 per-worker steal-PRNG seeds via splitmix32 ✅. **#3 compSleep_ default FLAGGED — not flipped** (behavior change; header claims "default on" while code says `false` — for JD). **#6 SBO/intrusive job type FLAGGED — not applied** (touches the Chase-Lev/threading backbone the audit certified sound; blast radius too big for a fix run — recommend the builder take it with the det-sched/threading self-tests as gate).

Tests: `engine_core` PASS · `engine_world` PASS · `engine_sim` only the grandfathered block-names failure (storage area, untouched) · standalone light-engine/entity/tie-break verifications PASS · `lithokraft_content` builds clean · `lithokraft_headless_harness` ctest PASSED. **Root cause of the grandfathered `lithokraft_selftest` found** (pre-existing, Oct 10): `blockRegistry()` aborts — 39 `r.add` rows + Air = 40 ≠ `kOverclockor+1` = 36 (blocks.cpp:69). Needs a game-content owner to sync the ids. **The baseline was not green when the child started** (`lithokraft_game` failed: game.h:88 declared `void saveWorld` while game.cpp used `bool` — half-applied sibling state; Child E's completed run resolved it — see §6).

---

## §3 Net — Child B (9 findings: 4 MEDIUM, 5 LOW; 5 opts)

8 of 9 fixed (1 flagged), 4 of 5 optimizations applied (1 flagged as unsound). Clean-room: only `engine/src/net` touched (11 files); ~+400/−150 lines.

1. **MEDIUM unbounded ready_ queue — FIXED.** `ReliableConfig::maxReady` (default 256, matches the reorder cap); drop-incoming with a `droppedReadyCount()` counter; ack processing runs before the drop so the sender window still slides. *Behavior change:* under flood the delivered stream is a truncated prefix, never reordered.
2. **MEDIUM spoofed ack eviction — FLAGGED, not applied.** No cheap in-class fix (inherent to unauthenticated plaintext). Limitation + the real fix documented on `onAck()`: builder's authenticated KEX (`kHsKeyExchange`) + game binding channels to peer address/port at the socket layer. **For the builder.**
3. **MEDIUM spoofed Reject DoS — PARTIAL fix + flag.** `ConnectHandshake::bindPeer(uint64_t)` + `onPayload(..., fromPeerToken)` ignores payloads from unexpected endpoints (token = endpoint identity, not a secret; unbound = historical behavior). Full fix needs authenticated KEX — **flagged for the builder**. Note: the hook does nothing until the game wires the token from its socket layer.
4. **MEDIUM version downgrade — DOCUMENTED.** Deployment guidance in `handshake.h`: games MUST narrow offers to `{2}` until a v1 codec exists; transcript-signing (sign/MAC the hello under the session key) noted as the future once KEX lands.
5. **LOW handshake liveness on junk — FIXED.** `lastActivityMs_` refreshes only on recognized handshake messages. *Behavior change:* junk drips no longer hold `HelloSent` past the deadline.
6. **LOW needFull abuse — FIXED.** `requestFull()` rate-limited: ≤1 honored request per `SnapshotConfig::forcedFullMinInterval` builds (default 32 ≈ 1/s at 32 Hz); excess counted in `ignoredForcedFulls()`. Server-internal resyncs bypass the limiter. *Behavior change:* a needFull-spamming client gets fulls at ~1/s instead of 32 Hz.
7. **LOW signed-overflow UB on snapshot deltas — FIXED.** New `satAdd()`: deltas accumulate in `int64_t`, clamp to int32 in `decodeSnapshotBody` (pos + vel paths). *Behavior change:* attacker-crafted deltas clamp instead of UB-wrapping.
8. **LOW confinement denial log spam — FIXED.** Per-mod 1 s denial-log window: first denial logs in full, rest counted and summarized at rollover. *Behavior change:* ≤2 log lines per mod per second under spam.
9. **LOW dead version.h — DELETED** (`NetConfig`, `NetPreset`, `LITHOLATTICE_NET_TICK_HZ_DEFAULT`, `LITHOLATTICE_NET_SNAPSHOT_HZ`; verified zero references tree-wide).

Optimizations (§A net): #1 double codec pass ✅ (`EncodePackedInfo*` out-param; build splices the post-image baseline — ~2× codec CPU saved per snapshot per client); #2 per-client sort ✅ (`normalizeForSnapshot()` static + `buildNormalized()`; game can normalize once per tick — latent value: the game must adopt it); #3 double header decode ✅ (`onReliableHeader/onUnreliableHeader` + `receive(h, payload, len)` overloads; legacy `Handler` path unchanged — adoption is latent); #5 decode merge ✅ (single three-way linear merge, moved not copied). **#4 poll() copy-before-restamp FLAGGED as unsound — not applied.** "Restamp in place and move out" breaks the retransmit model: `p.datagram` must survive for the next RTO, and after a move the entry is empty — the next `poll()`'s `restampAcks` would OOB-write into an empty vector. The 1.2 KB copy is irreducible without shared ownership.

Tests: `engine_net` (incl. `confinement_selftest`) PASS before and after; scratch harnesses (~80 checks, /tmp, not committed) PASS: ready_ cap + counter + window slide; limiter intervals; splice≡decode equivalence under real truncation (p2 truncated at 64 B, p3's delta decoded against the true client view lands exactly); saturation clamps at ±INT32_MAX; junk no longer defeats the handshake deadline; peer-token ignore/honor; demux header-threading + legacy path; `buildNormalized` byte-identical to `build`; deny-log suppression counts (5000 throttles → 1 line; rollover → summary). Recommendation: permanent regression coverage in `engine/tests/engine_tests.cpp` for the splice-under-truncation roundtrip, `satAdd` clamps, `requestFull` limiter, and the ready_ cap (harnesses live in /tmp).

---

## §4 Script / mod / console / MCP — Child C (11 findings: 2 HIGH, 4 MEDIUM, 5 LOW; 4 opts)

All 11 fixed, none flagged; 4 optimizations applied. Clean-room: only `engine/src/script`, `engine/src/mods`, `engine/src/console`, `engine/src/mcp` touched. `script_engine.cpp` edited CRLF-preserving (verified with `file`); all other edited files LF. No new warnings; full build clean after each fix.

1. **HIGH consoleCmd ambient authority — FIXED** (`script_console.cpp`). `ConsoleScriptContext` carries `permissionMask`, captured from `se.modPermissions()` in `bindConsoleToScripts`; `ScopedCallerPermissions` RAII installs/restores it in `scriptConsoleCmd`/`scriptConsoleCvar`/`scriptConsoleSetCvar`; `executeLine` saves/restores the mask around re-entrant calls. **Behavior change:** game scripts can no longer run cheat-gated console commands even with `cheatsAllowed(true)` — the documented trust model. Selftest asserts the refusal. **Game-child follow-up:** game scripts issuing cheat-gated `consoleCmd` lines are now refused by design.
2. **HIGH bind permission laundering — FIXED** (`concmd.cpp`). `KeyBind{command, key, callerMask}` stamps the binder's mask at `setKeyBind`; `dispatchKeyBinds` re-installs it fail-closed; creation/overwrite logged via `print()` (new bind-confirmation line — intended per audit). `keyBinds()` signature unchanged.
3. **MEDIUM quota suspend doesn't abort — FIXED** (`script_engine.cpp`). `suspend()` calls `executingCtx_->Abort()` (verified flag-only in AngelScript source, safe inside the allocator); `run()` tracks the executing context with prev-restore; abort log names the quota reason. Selftests assert the in-flight abort.
4. **MEDIUM stoul crash on malformed mod.json — FIXED** (`mod_loader.cpp`, `extension_loader.cpp`). New fail-closed `parseUint32` (rejects >10 digits, catches throws, rejects >0xFFFFFFFF narrowing); used by both `parseSemver`s and `parseSdk`. Selftest: huge/wrapping sdk values refused, scan never throws, good mod still loads.
5. **MEDIUM query int64→int narrowing — FIXED** (`concmd.cpp`). Bounds outside `[INT32_MIN,INT32_MAX]` rejected before narrowing (message mirrors MCP); cell-count math in `int64` (also fixes signed-overflow UB in the old guard).
6. **MEDIUM exec TOCTOU + archived sandbox — FIXED** (`concmd.cpp`). `executeFile`: Linux `O_NOFOLLOW` + fstat regular-file + `/proc/self/fd` re-validation (mirrors `storage::SandboxedFile`). **Verification caught two real bugs:** (a) `weakly_canonical()` on the user path resolved symlinks *before* `O_NOFOLLOW` saw them, silently defeating the gate — fixed by not canonicalizing the validated bare filename; (b) non-Linux keeps resolve+ifstream with the window documented. `loadArchived` **deleted** (decl, def; selftest reworked to an `executeFile` round-trip; game boots `config.lth` via sandboxed `executeFile` per game.cpp:84 — verified safe). `saveArchived` confined to config dir, refuses when no config dir set. **Behavior change:** symlinks refused outright, even in-config-dir ones.
7. **LOW kPermPilotSenses schema drift — FIXED** (`mod_permissions.h/.cpp`). Added to `kNameBits` and `kAllPermissionBits`. **Flagged for JD:** whether `kPermissionSetVersion` should bump 2→3 (triggers grant re-review) — left undecided.
8. **LOW HTTP session expiry — FIXED** (`http_server.cpp`). 64-session LRU cap + 30-min idle timeout (sweep on create, per-session expiry in `findSession`, fail-closed); `eraseSession` wired via `DELETE /mcp` (auth required; 404/204); `Allow: POST, DELETE`. Selftests: DELETE lifecycle, LRU eviction of oldest-idle, newest survives.
9. **LOW stdio 16MiB cap — FIXED** (`server.cpp`). `FdStdioTransport::readLine` rewritten as chunked read with early cap + `pending_` stash; overlong lines discarded through the newline, sentinel returned so the existing drop log still fires.
10. **LOW bumpCVar llround UB — FIXED** (`concmd.cpp`). Saturates: NaN→0, out-of-`long long`→clamp, else `llround` (unchanged for in-range values).
11. **LOW revocation doesn't unbind — FIXED** (`script_engine.h/.cpp`). `bindGlobalFunction` records every attempt; `setModPermissions` detects changes → `syncBindings()`; widened mask re-registers refused decls; **narrowing calls `rebuildSandbox()`** (full engine rebuild — AngelScript has no unregister API, verified by grep; module-discard would have re-exposed stale registrations on reload). `unbindAll()` clears + rebuilds. **Verification caught a real regression:** eager re-registration made a subsequent explicit `bindGlobalFunction` return `asALREADY_REGISTERED`; fixed with idempotent success on identical triple (padding-safe `asSFuncPtr` compare), conflicting triple still refused. Contract documented: never call while a script executes.

Optimizations (§A script/mcp): all 4 applied — `handleParsedRequest` single-parse path (`jsonrpc.h/.cpp` + `server.cpp::dispatchParsed`), `dispatchStateless(const RpcRequest&)` merging the envelope pre-serialization, key parsed once at bind, lazy `findTool` index.

Tests: `ctest -R "engine_script|engine_mods|engine_console|engine_mcp"`: **4/4 pass** (script: 87 checks incl. new quota-abort + syncBindings tests; console/mods/mcp green). Full `ctest`: **18/20** — failures are only the 2 grandfathered (`engine_sim` #4, `lithokraft_selftest` #19). New selftests: symlink refusal, DELETE/LRU sessions, pilot_senses schema, huge-number mod.json, keybind authority stamping, consoleCmd mask, saveArchived confinement, bind idempotency/revocation. **Pre-existing flake (not mine):** `mcp: senses: vision burst 8 then rate-limited` is wall-clock racy (10 PNG encodes vs the 250 ms refill window; no injected clock). Failed 1/7 at baseline; fails more under VM load. Changes strictly reduce per-call work on that path (measured 0.5 ms/call pipeline overhead). Recommend a fake-clock injection as follow-up, not in this fix run.

---

## §5 Audio / file / storage — Child D (18 findings: 4 HIGH, 6 MEDIUM, 8 LOW; 5 opts)

17 of 18 items done; 1 flagged (not deleted). Full build clean, no new warnings. Targeted ctests pass; full ctest 18/20 with only the 2 grandfathered failures.

### HIGH — all fixed
1. **Anvil 7-bit count overflow — FIXED** (`anvil_region.h/.cpp`). `kMaxInlineBytes` = `127 * kSector - kRecordOverhead` (= 65,014); the record-overhead constant was promoted to the header so the bound can't drift from the framing; a `static_assert` that the max inline payload needs ≤ 127 sectors; the `// 1..255` comment corrected to `// 1..127 (7-bit count)`. **Behavior change:** payloads 65,015–130,550 bytes now take the sidecar path (previously written inline, unreadable immediately after `put()` + sectors reallocated). Verified: 65,014 inline / 65,015+ sidecar, all read back intact.
2. **OcclusionThread addQuery race — FIXED** (`occlusion_thread.h/.cpp`). Structural mutation mutex-guarded; `threadMain` snapshots heap-stable raw pointers under the lock into a reused member vector, then raycasts unlocked; positions are `atomic<float>` — game thread stores before a release-store of `active`, thread loads `active` with acquire. `gain()` copies the pointer under the lock, then reads the atomic lock-free. Companion LOW fixed too: `setOcclusionTest` now asserts `!running_` and documents it must precede `start()`.
3. **AudioStreamer play()/render() race — FIXED** (`streaming.h/.cpp`). Read cursor (`pending_`, `chunkIdx_`, `frameOff_`) is audio-thread-only. `play()`/`stop()` join the worker, drain stale queue chunks on the game thread (repairing `queued_`/`buffered_` exactly), and set `pendingReset_`; the audio thread applies the reset (with counter repair for dropped pending chunks) at the top of the next `render()`. Invariant: every chunk's counters are decremented exactly once, by whichever side disposes of it — no size_t wraparound.
4. **recoverCrashedWrite — JD's highest priority — FIXED** (`file_contract.h/.cpp`, `storage.cpp`, `defrag.cpp`), implemented per the brief. Recovery order (documented in the header): (1) primary exists + validates → nothing; (2) scan `.bak.1`..`.bak.32` newest-first, **copy** (never move) the first intact generation over the target + fsync, moving a torn primary aside to `<path>.torn` for forensics — never deletes the last good copy; (3) intact `.tmp` → atomic promote; (4) false, with err **empty** = no footprint (genuinely absent/new) vs err set = footprint but no intact generation (fail closed). Validators are header-level (region: magic+version+index bounds; store: magic+version+page-count bound); the loaders' full validation (record CRCs, deserialize) remains the final gate. Called from `RegionFile::open` and `RecordStore::load`. All three doc falsehoods corrected ("old or new, never torn" now names the window + self-heal; "target untouched on ANY failure" scoped to pre-rotation; "rotation only on successful write" corrected). **Behavior changes:** a missing/torn primary with an intact backup now recovers instead of silently opening as "new region" / quarantining. Verified end-to-end: simulated rotation/commit crash → v1 restored from `.bak.1`, uncommitted v2 correctly absent, backup chain intact; torn primary → fallback + `.torn` preserved.

### MEDIUM — all fixed
5. **Defrag cancel/fail mid-reindex — FIXED** (`defrag.h/.cpp`). `cancel()` at-or-after `reindexBegun_` finishes feeding the snapshot synchronously when the generation is still fresh (post-feed re-check; marks `indexDirty_` if the store moved under it), and marks dirty instead of feeding stale bytes when the snapshot is stale. `fail()` marks dirty the same way. `pumpReindex` re-checks `generation()` every pump and restarts the scan on mismatch (bounded by `kMaxRestarts`, via a shared `restartScan()` helper also used by `pumpCommit`). New `indexDirty()`/`indexDirtyStore()` accessors; the flag survives `resetSession()` and heals on the next full reindex. Extended `defrag_selftest.cpp` with 2 new cases (mid-reindex cancel completes the feed; stale cancel → dirty → next session heals, 601 docs). All pass.
6. **Tap ring — VERIFIED, not re-done.** The coordinator's earlier fix is intact; all 8 tap selftests pass.
7. **Shed one-directional — FIXED** (`audio_engine.h/.cpp`). Per-synth `shedVirtualized_[2]` bitmask tracks engine-virtualized voices; step-down clears exactly those (falling through to the quiet-tail test so a still-quiet tail re-virtualizes without popping). `apply()` drops a voice from the mask on any game-driven change (Virtualize command, NoteOn), so game-virtualized flags are never auto-cleared.
8. **DecodeFn error channel — FIXED** (`codecs.h/.cpp`). New `adaptDecoderToDecodeFn()` checks `implemented()` up front — stub decoders log once at wire-up and report EOS instead of failing as silent end-of-stream; decode-time errors from real decoders are logged.
9. **Strand identity raw path — FIXED** (`async_io.h/.cpp`). `submitStrand` canonicalizes with `absolute().lexically_normal()`; documented on the `strands_` member.
10. **NaN/Inf + float→int UB — FIXED** (`simd_mix.h`, `voice.cpp`, `audio_engine.cpp`). New `simd::scrubNonFinite` runs one pass over the final mix before the tap copy and device store; `Voice::noteOn` clamps frequency to [1, sr·0.45] (NaN→440), sweep to ±96 semitones/s (NaN→0), hold to [0, 3600 s] (NaN/negative→0, kills the int64 UB).

### LOW — all fixed
- `RegionFile::put`/`read` slot bounds — `put` asserts (debug) + early-returns, `read` returns nullopt + err.
- `Quarantine::quarantine` log dedup — `unordered_set` of logged paths: log line fires once per path, entries still recorded every call; `clear()` resets both.
- `SandboxedFile::operator=` UB — new `close()` helper + memberwise move (no explicit destructor call); dtor routes through it.
- `readAll` `EINTR` — retries (Linux).
- Unbounded reads — new `kMaxWholeFileRead` (64 MiB, configurable per call) enforced in `readFileAll` (fstat/file_size pre-check + running cap), `SandboxedFile::readAll` (fstat pre-check + running cap), `RecordStore::load` (now reads via capped `readFileAll`).
- `IoFence::wait` assert — runtime check: logs and returns -1 on job threads instead of assert-only.
- Command queue — grown 256→1024; last 64 slots reserved for NoteOff/SetMasterGain under bursts (FIFO preserved, tryPush stays the hard gate); new `droppedCommands()` telemetry. A dropped NoteOff can no longer sit behind 256 commands — reserved slots fix the stuck-note finding.

### Dead code
- `WavDecoder::size_` DELETED (member + assignment).
- **`AnvilRegion::locate` FLAGGED — NOT deleted.** The audit claimed "zero callers repo-wide," but `engine/tests/engine_tests.cpp:3246` calls it (`r.locate(7, 9, &sector, &count)` — the CRC single-bit-flip selftest uses it to find the payload sector). Coordinator decision: keep — deleting a public method used by a selftest requires test surgery; the method is harmless introspection. JD may override.

### Optimizations (Appendix A audio/storage) — all 5 applied
1. Backup-rotation I/O storm — new `FileWriteOptions::deferDirFsync` + `syncDirContaining()`; `WorldStore::flushAsync` uses the `flush(err, opts)` overload with deferred dir fsync and fsyncs the directory once per save (a crash before the batch fsync can only revert a rename, never tear). Per-save-set rotation redesign noted as deferred (changes backup format — risky).
2. Underrun zero-fill → `std::memset`.
3. `forEachLiveInPage` is now a template (no `std::function` alloc per page per tick).
4. Streamer worker decodes into a persistent thread-local scratch buffer (per-chunk zero-fill of the decode target gone; fully eliminating the per-chunk allocation needs a buffer return channel from the audio thread — out of scope; the audio side already recycles `pending_` storage).
5. Half-rate shed path uses new `simd::addIntoDuplicated` (SSE2/NEON/scalar, bit-identical to the scalar loop).

### Tests
- Full build: clean, no new warnings (one introduced unused-parameter warning was fixed).
- `ctest -R "engine_storage|engine_audio|engine_defrag"`: **3/3 pass**.
- Full `ctest`: **18/20** — the 2 failures are exactly the grandfathered `engine_sim` and `lithokraft_selftest`.
- Standalone: audio + defrag selftests, 129/129 PASS, 0 FAIL.
- Custom storage verification (`/tmp/storage_verify.cpp`, 28 checks): anvil 7-bit boundary, crash-window recovery, torn-primary fallback, slot bounds, read cap, quarantine — all PASS.
- Note: one `ctest` invocation hung on `engine_core` while a sibling was concurrently building; rerunning the test binary directly passed instantly — environmental. `trace_jit_basic.cpp` untouched.

---

## §6 Game / LithoKraft — Child E (7 findings: 1 HIGH, 3 MEDIUM, 3 LOW; 2 opts)

All 7 fixed, none flagged. Dead code deleted, both optimizations applied. All files under `game/LithoKraft/` (LF preserved). MCP senses adapter intact.

1. **HIGH sim.tickrate 0 SIGFPE — FIXED** (cvar side; engine side is Child A's #6). Cvar setter: `s.tps = nearestTps(toInt(v))` (nearest of {32,64,128,256,512}); `nearestTps` moved out of settings.cpp's anonymous namespace, declared in settings.h. *Behavior change:* 0 no longer SIGFPEs (was raw int; negatives wedged the sim).
2. **MEDIUM paused dimension switch — FIXED.** After `loadActive()` in `switchDimension()`: `sim_->setPaused(paused_ && cfg_.pauseSim && !benchSet_)` — paused stays paused.
3. **MEDIUM menu dolly discards position — FIXED.** `enterMenu()` snapshots pos/yaw/pitch (+ validity flag); `enterPlay()` restores them.
4. **MEDIUM pause in menu — FIXED.** `setPaused()` no-ops when `menuState_==Menu`; `b.pauseToggle` reports `"pause: unavailable in the menu"` (was: broke the menu⇒paused invariant, mouse-dead buttons).
5. **LOW save false success — FIXED.** `saveWorld` returns `bool` (header fixed too — resolves the half-applied baseline mismatch Child A hit); `b.save` prints `"save: no save directory"` when false.
6. **LOW config.lth write-only — FIXED.** Executed at boot via the sandboxed `executeFile("config.lth")`, silent when absent. Placement: it cannot run "before cvar defaults" (registerOn) since an unregistered cvar rejects assignment — it runs at the earliest functional point: after all archived cvars are registered (incl. `mcp.*`), before the boot-apply reads and before `autoexec.lth` (player script still wins). Archived values flow through the cvar change-fn → `onSettingsChanged` → live engine. (This unblocks Child C deleting `loadArchived`.)
7. **LOW hud.scale truncation — FIXED.** Declared `CVarType::Int`, clamped 0–2 in the setter. *Behavior change:* `hud.scale 1.9` now fails Int coercion with a console error instead of silently truncating; `99` clamps to 2.
- **Dead code:** deleted `HeadlessGame::input()`/`audio()` (zero callers verified).
- **Opt 1 frameParams** ✅ — reused member vector `pbrLightsCache_` (no per-frame alloc); skip re-collection when camera column unchanged and `emissiveProvider_->needsRefresh()` is false (`mutable` members — frameParams is const).
- **Opt 2 uploadEntities** ✅ — early-out on empty entity set; still uploads the empty mesh so stale billboards can't linger.
- **Coordinated render wirings** (all applied): W1 `binaryMeshSubChunkTransparent(...)` after the opaque call, merged into the same `quads` vector; W2 `render.ambient_mode` end-to-end (`GameSettings::ambientMode = 0`, `load()`/`save()` clamp 0–1, cvar Int/archived/in `syncFrom`, boot-apply + live apply via `renderer_.setAmbientMode(ambientModeFromInt(...))`, helper clamps); W3 winding-compensation reversal removed from `McpSensesAdapter::noteMesh()` → verbatim `cm->verts = m.verts;` (verified engine `isBackface` fix landed: `cross >= 0`).
- **Observation (not applied — follow-up suggestion):** `onSettingsChanged` syncs `settings_.tps` + the live sim but not `cfg_.tps`, while `loadActive()` uses `cfg_.tps` — so a console/`config.lth` tickrate change is silently lost on dimension switch (pre-existing; `handleToggles` CycleTPS does sync `cfg_.tps`). One-line sync in `onSettingsChanged` would close it.

Tests: `lithokraft_game` and `lithokraft_headless` both build clean (exit 0); `engine_console` PASS; `lithokraft_headless_harness` PASS. Full ctest (minus the 2 grandfathered, not run): 14/18 pass; the 4 failures are engine-side in siblings' active areas (`engine_render` ×2 — mesher golden hash needed re-record + pbr-software test encoded old inverted culling, both during coordinator's §1 fix; `engine_storage` — in-progress anvil fix; `engine_index` — perf timing flake; `engine_mcp` — vision rate-limit timing). Note: build was twice transiently blocked by siblings' mid-edit engine files (audio occlusion_thread.cpp const error — since fixed; storage file_contract.h mid-edit) — resolved by waiting; no engine code touched.

---

## Summary of flags (not fixed — for JD's review / builder)

| Area | Finding | Flag |
|---|---|---|
| Sim | Opt #3: `compSleep_` default (`signal.h:127`) — code says `false`, header claims "default on" | FLAGGED for JD (behavior change; do not silently flip) |
| Sim | Opt #6: SBO/intrusive job type (`JobSystem::submit`) | FLAGGED — not applied (backbone blast radius; recommend builder with threading self-tests as gate) |
| Render | Per-pixel light-collection per-quad hoisting; `std::function` per-pixel in `forEachEmissive`; FXAA rewrite; software sub-chunk offset (verified N/A) | Flagged / N/A as noted |
| Net | Finding 2: spoofed ack eviction | FLAGGED — needs builder's authenticated KEX + peer binding at socket layer |
| Net | Finding 3: spoofed Reject DoS | PARTIAL fix applied; full fix (authenticated KEX) FLAGGED for builder |
| Net | Opt #4: poll() copy-before-restamp | FLAGGED as unsound — not applied (breaks retransmit model) |
| Net opts | #2 normalize once per tick, #3 header threading | Latent — need game adoption |
| Game | tickrate change lost on dimension switch (`cfg_.tps` sync) | Follow-up suggestion (pre-existing) |
| Game content | Grandfathered `lithokraft_selftest` root cause: blocks.cpp ids (40 ≠ 36) | Needs game-content owner sync |
| Script | `kPermissionSetVersion` bump 2→3 for kPermPilotSenses schema addition | FLAGGED for JD (triggers grant re-review) |
| Script | `mcp: senses: vision burst` rate-limit test wall-clock flake (pre-existing) | Recommend fake-clock injection as follow-up |
| Storage | `AnvilRegion::locate` (audit said zero callers; a storage selftest calls it) | FLAGGED — kept; JD may override |
| §4 | — | done |
| §5 | — | done |

## Dead code removed (Appendix B)
- Render: greedy_mesher.cpp, axis_culling.cpp, software_opt.cpp, greedy_mesher.h, `pbrTierIsStubbed()` — all DELETED (CMakeLists updated).
- Net: `NetConfig`/`NetPreset`/defaults in version.h — DELETED.
- Game: `HeadlessGame::input()`/`audio()` — DELETED.
- NOT deleted per instructions: TickArena (sound, awaiting adoption), simFailFast (death-path infra), trace_jit_basic.cpp (untouched).

## Test results
- Engine targets rebuilt clean; `engine_net` incl. `confinement_selftest` PASS; `engine_core` PASS; `engine_world` PASS; `engine_console` PASS; `lithokraft_game` + `lithokraft_headless` build clean; `lithokraft_headless_harness` ctest PASS; new render tests (`testSoftwareBackfaceConvention`, `testBinaryMesherTransparent`) PASS; mesher golden hash re-recorded (0x77c08ffd8158c6d3).
- **Final merged-tree ctest (coordinator, after all children landed): 18/20 — failures are exactly the 2 grandfathered (`engine_sim` #4 — world/block.h, `lithokraft_selftest` #19 — blocks.cpp guard). No new failures.** The transient sibling-area failures seen mid-run (console exec symlinks, streaming worker, render golden hash) are gone — fixed by their owners.
- No new production code outside own areas; ~47k production lines used of 64k budget (~62.5k incl. ~15k tests).

*(Report complete — all five children delivered.)*

---

## Follow-up fix batch (regression review) — 2026-10-11 ~14:15 EDT

Source: `Regression-Review-2026-10-11.md` — 4 questionable fixes + 2 MEDIUM fresh-sweep findings + 6 LOWs. All applied except 2 flagged LOWs (see below).

### Questionable fixes — all 4 resolved

1. **[HIGH] PackedVertex fix incomplete** (`engine/src/render/gl33/gl33_renderer.cpp`): the per-chunk non-arena VAO in `uploadChunkMesh` still bound the pre-fix layout (loc1=1×uint@6 "slot"). Rebound to match `chunk.vert` exactly (loc0=3×ushort@0 aPos, loc1=4×ubyte@6 aInfo, loc2=2×ubyte@10 aShade), copying the fixed entity-VAO bindings. Arena and non-arena paths now agree.
2. **[MEDIUM] GL meshKey UB** (`gl33_renderer.cpp:329`): `(int64_t(m.cx) << 40)` on negative coords was still UB. Now casts through `uint64_t` first, mirroring the VK fix in `vk_renderer.cpp:37-41` bit-for-bit.
3. **[MEDIUM] rebuildSandbox contract unenforced** (`engine/src/script/script_engine.cpp:290`): added `assert(executingCtx_ == nullptr)` plus a fail-closed runtime guard (logError + early return) — the contract was documented 4×, enforced 0×. Covers all callers (`syncBindings`, `unbindAll`) since they funnel through `rebuildSandbox`.
4. **[MEDIUM] Defrag dirty-flag store confusion** (`engine/src/defrag/defrag.cpp:357`): `cancel()`'s sync-complete path cleared `indexDirty_` unconditionally. Now guarded with `indexDirty_ && indexDirtyStore_ == sessionStore_`, mirroring `pumpReindex`'s clear path — a different store's dirty record can no longer be dropped.

### Fresh-sweep MEDIUMs — both fixed

5. **vision.set_camera unbounded coords** (`game/LithoKraft/src/game/mcp_senses_adapter.cpp:171`): added `isfinite` + `|coord| ≤ 1e6` guard in `setCameraPose` (coords are `double`; pitch/fov were already validated). NaN/inf can no longer reach the software rasterizer.
6. **IoFence dtor fail-open** (`engine/src/storage/async_io.cpp`): the dtor is now out-of-line; if `wait()` refuses (job-thread destruction) with ops still pending, it logs and `abort()`s (fail-closed — the in-flight lambdas hold a raw `this`, so completing destruction would be UAF). Zero pending ops → refusal is harmless, destruction proceeds. No production users yet (selftest only); API unchanged.

### LOWs — 4 applied, 2 flagged

- ✅ **Symlinked-config logWarn** (`game.cpp`): `config.lth`/`autoexec.lth` boot exec now warns via `logWarn` when the file is a symlink (exec refuses symlinks by design); non-symlink path unchanged.
- ✅ **coerce ERANGE** (`engine/src/console/concmd.cpp`): `strtol`/`strtod` now set `errno=0` first and reject on `ERANGE` (Int and Float paths) — huge input no longer clamps silently into a cvar/settings desync.
- ✅ **Cvar validation** (`game/.../settings_cvars.cpp`): `mcp.audio_tier` clamped 0..3 and `camera.chunk_radius` clamped 0..64 via `setCVarChangeFn` hooks installed at registration. The hook re-sets only when out of range, so it terminates (the clamped set passes through unchanged).
- ✅ **PNG CRC-32 thread-safety** (`engine/src/mcp/senses.cpp`): the table is now a function-local `static const std::array` (C++11 magic statics) — same values, no data race on concurrent first captures.
- ⚠️ **FLAGGED — ambient_mode on Vulkan/MCP vision**: not trivially safe. `SoftwareRenderer` has no ambient-mode support at all (no field, no setter) — wiring it is a feature addition, not a fix. `VulkanRenderer` has no override and its shader-side support is unknown. Left for the builder/render pass; `render.ambient_mode` remains GL-only today (silently ignored elsewhere).
- ⚠️ **FLAGGED — loadDirectory symlinks** (`script_engine.cpp:350-351`): explicitly "by design" in the review (local-trust dir, sandbox-executed). No behavior change; noted for consistency.

### Test results (follow-up batch)

- Full rebuild clean (only pre-existing warnings).
- **ctest: 18/20 — failures are exactly the 2 grandfathered** (`engine_sim` world/block.h, `lithokraft_selftest` blocks.cpp guard), untouched. No new failures.
- Affected suites all PASS: `engine_render`, `engine_script`, `engine_defrag`, `engine_mcp`, `engine_storage`, `engine_console`, `lithokraft_headless_harness`.
- `trace_jit_basic.cpp` untouched. Clean-room (own code only). CRLF/LF line endings preserved per file.

*(Follow-up batch complete.)*

## FOV-margin culling (JD 2026-10-11, post-fix-run task)

**Spec (JD-locked):** the renderers did EXACT chunk-level frustum culling (`aabbInFrustum`, no margin). Added a configurable angular margin — render FOV ± N°, cull the rest. The shock absorber against "frustum occlusion tearing" (JD's term): fast view rotation reveals already-rendered chunks, not pop-in.

**What changed:**
- `engine/src/core/include/litholattice/core/math.h`: new `aabbInFrustumMargin(vp, mn, mx, camPos, marginDeg)` — expands the tested box by `d*tan(margin)` (d = camera-to-box-center distance), approximating an N°-wider frustum per side. `marginDeg <= 0` degenerates to the exact test. (CRLF preserved.)
- `engine/src/render/include/litholattice/render/renderer.h`: new `IRenderer::setFrustumMarginDeg(float)` virtual (default no-op).
- `engine/src/render/gl33/gl33_renderer.cpp`: `frustumMarginDeg_` member (default 5°), setter clamps 0..15; both chunk loops (classic + PBR) now cull via the margin helper with `p.camPos`.
- `engine/src/render_vk/vk_renderer.cpp`: same member/setter; `drawChunks` uses the margin helper.
- `game/LithoKraft/.../mcp_senses_adapter.{h,cpp}`: the agent-vision capture loop (the software renderer's chunk iteration lives here, not in `SoftwareRenderer`) uses the margin helper with the camera eye — **agent cameras get the margin automatically** via the shared helper, as spec'd.
- Game wiring: `GameSettings::frustumMarginDeg` (default 5°), cvar `render.frustum_margin_deg` (Float, archived, clamped 0..15), boot + live-apply paths in `game.cpp`, per-frame sync into the adapter's `CvarState`.
- FOV cap verified (no change needed): player camera is hardcoded `perspective(1.2f)` ≈ 68.75°; agent cameras clamp 10..120 in both the MCP server and the adapter. Nothing sets FOV above 120.
- Sim untouched — culling is render-only.

**Tests:** new `testFrustumMargin()` in `engine_tests.cpp` (6 checks, all PASS): centered chunk renders under both; chunk just outside exact frustum renders inside 5° margin; chunk outside margin culled; behind-camera culled at max margin; margin ≤ 0 == exact test; wider margin never culls more.

**Test results:** full engine ctest **18/20** — failures are exactly the 2 grandfathered (`engine_sim`, `lithokraft_selftest`), untouched. `trace_jit_basic.cpp` untouched. Clean-room.

**Perf estimate:** the margin math is ~1 sqrt + 1 tan per chunk per frame (~tens of ns) — negligible next to a chunk draw. The margin only ever ADDS a thin shell of chunks around the exact frustum, so there is no direct fps win from the margin itself; its value is experience-correctness (no pop-in on fast rotation). The underlying exact culling (which skips all off-screen chunks on GL/VK/software) is where the real milliseconds live, and it was already in place.

*(FOV-margin culling complete.)*

---

## Culling-cache prototypes (JD 2026-10-11) — all three built, green

JD approved all three of Elfie's original culling-cache designs for prototyping
(history-books credit per JD: Elfie is the designer/discoverer; attribution
headers in the code). Each has its own cvar toggle, all default OFF; the base
`render.frustum_margin_deg` culling stays the default-on foundation. New shared
header: `engine/src/render/include/litholattice/render/cull_prototypes.h`
(`CullProtoConfig`, `CullDecision`, `CometTail`, `VelocityTracker`,
`directionalMarginDeg`, `classifyTwoTier`, `CullProtoState`).

**Prototype 1 — Comet tail** (`render.comet_tail_frames`, Int 0..64, default 0):
ring buffer of the last N camera frusta; any chunk inside the union stays
submitted. History holds strictly past frames. Known limit: an instant 180°
snap defeats prediction (JD's residency note bounds the damage to re-submission).

**Prototype 2 — Lean into the turn** (`render.velocity_margin` bool default off;
`render.velocity_margin_gain` float 0..10 default 1.0): smoothed yaw/pitch
angular velocity stretches the margin asymmetrically — wider in the direction
of rotation, narrower behind. Still camera degrades exactly to the base margin.
`FrameParams` gained `yawRad`/`pitchRad` (filled from the player in
`Game::frameParams`); the vision path already had yaw/pitch.

**Prototype 3 — Two-tier margin** (`render.two_tier` bool default off;
`render.two_tier_outer_deg` float 0..30 default 10.0): inner tier = full render;
outer tier = mesh kept, draw skipped (`CullDecision::MeshOnly`). GL/VK meshes
are never visibility-evicted today, so MeshOnly currently means "skip the draw,
mesh stays uploaded" — the classification, stats, and pin-against-future-
eviction are the prototype.

**Wiring:** `IRenderer::setCullProtoConfig` + `cullProtoStats()` (GL + VK
implement; per-frame `beginFrame`, per-chunk `decide` in both chunk loops);
vision path gets per-pilot `CullProtoState` (own mutex — game thread never
blocks); 5 cvars registered in `settings_cvars.cpp` (archived) + `GameSettings`
fields + boot/live-apply in `game.cpp` + per-frame `syncCvars` to the adapter.
Sim untouched; `trace_jit_basic.cpp` untouched.

**Tests:** `testCometTail` (8), `testVelocityMargin` (6), `testTwoTier` (10
incl. composed-state and all-off-degrades-to-base) — 24/24 PASS.

**Test results:** full ctest **18/20** — failures are exactly the 2 grandfathered
(`engine_sim`, `lithokraft_selftest`), untouched. (One `engine_audio` flake in
the first full run; passes in isolation and on re-run — pre-existing flakiness.)

**How JD A/B tests them** (workstation): each prototype is one console cvar.
Try them solo first, then in combination:
- `render.comet_tail_frames 8` — whip the camera; recently-seen chunks should
  persist instead of popping.
- `render.velocity_margin 1` (tune with `render.velocity_margin_gain`) —
  fast turns should pre-widen the frustum in the turn direction.
- `render.two_tier 1` — outer-tier chunks cost draw calls only on re-entry.
- `cullProtoStats()` on the renderer exposes per-frame full/meshOnly/culled/
  cometKept counters for measurement (no UI surface yet — ask for one if the
  A/B needs numbers on screen).

*(Prototypes complete.)*

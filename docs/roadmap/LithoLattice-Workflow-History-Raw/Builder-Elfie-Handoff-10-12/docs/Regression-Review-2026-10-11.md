# Regression Review — LithoLattice / LithoKraft — 2026-10-11

**Method:** 4 parallel read-only reviewers over `~/workspace/litholattice`. Three verified every fix from `Fix-Run-Report-2026-10-11.md` against the actual code (§1+§2, §3+§4, §5+§6); one traced every behavior change for regressions and ran a fresh adversarial sweep. No files modified.

**Audit source:** `Repo-Audit-2026-10-11.md` (68 findings). **Fix source:** `Fix-Run-Report-2026-10-11.md` (66 fixed, 1 partial, 2 flagged).

---

## Executive summary

| | Count |
|---|---|
| Fixes verified clean (with file:line evidence) | **~62** |
| Fixes QUESTIONABLE (incomplete or unenforced — none broken on a live path) | **4** |
| Fixes BROKEN | **0** |
| New findings from fresh sweep | **2 MEDIUM, 6 LOW** |
| New CRITICALs | **0** |

**The 4 questionable fixes:**
1. **[HIGH] CRITICAL PackedVertex fix incomplete** — per-chunk non-arena VAO (`gl33_renderer.cpp:339-345`) still binds the pre-fix slot@6 layout. Dormant by default (arena is the default path), but corrupts every chunk if the arena is ever disabled. One-line fix: copy the entity-VAO bindings.
2. **[MEDIUM] GL `meshKey` UB only half-fixed** — VK twin fixed (`vk_renderer.cpp:37-41`); GL inline key (`gl33_renderer.cpp:329`) still left-shifts signed `int64_t` (UB on negative chunk coords).
3. **[MEDIUM] `rebuildSandbox` contract unenforced** — "never call while a script executes" documented 4×, enforced 0× (`script_engine.cpp:290,261,255`). No production callers today; bites when the builder wires per-mod engines. Needs `assert(executingCtx_ == nullptr)` or fail-closed guard.
4. **[MEDIUM] Defrag `indexDirty_` per-service vs per-store** — flag is per-service, stores are per-map; `cancel()`'s sync-complete path (`defrag.cpp:357-364`) clears it without the store guard that `pumpReindex`'s clear path has. Multi-store defrag can lose one store's dirty record.

**Fresh sweep:** 2 MEDIUM (`vision.set_camera` unbounded coords → NaN into rasterizer; `IoFence` dtor fail-open → potential UAF), 6 LOW. No fix-induced regressions on any live path; all 12 behavior changes traced clean (2 with caveats noted below).

---

## (a) Fix verification table

Verdicts: ✅ VERIFIED · ⚠️ QUESTIONABLE · ❌ BROKEN. Evidence is the reviewer's check, condensed.

### §1 Render (13 findings + ambient_mode + dead code)

| Fix | Verdict | Evidence |
|---|---|---|
| CRITICAL PackedVertex slot → offset 12 | ⚠️ | Struct/mesher/entity-VAO/arena-VAO/VK all match `chunk.vert`; **non-arena per-chunk VAO (`gl33_renderer.cpp:339-345`) still binds old layout** — loc1=1×uint@6 vs shader's uvec4 aInfo. Dormant (arena default), corrupts if arena disabled. |
| HIGH ShadeMode ↔ GLSL MODE_* | ✅ | Named constants 1,2,3,4,8,9,10,11,15 match enum; readout branch 15; 12/13/14 are blend-state modes, no collision. |
| HIGH binary mesher transparent | ✅ | `binary_greedy_mesher.cpp:201-224`; glass-against-glass cull `:129-151`; wired `game.cpp:193` after opaque call; tests pass. |
| HIGH isBackface fix + adapter workaround removal | ✅ | `cross >= 0` (`software_renderer.cpp:57-60`); `noteMesh` verbatim copy (`mcp_senses_adapter.cpp:63-66`); `testSoftwareBackfaceConvention` passes. Both halves consistent. |
| MEDIUM `render.ambient_mode` | ✅* | Enum default Fraction; CPU path truly normalizes (`pbr_lights.h:151-163`); GL uniforms uploaded; game wiring complete. *Note: software renderer's `ambientMode` field not wired to cvar — MCP vision stays Fraction even at mode=1. |
| MEDIUM software behind-camera clip | ✅ | `quadVisible` rejects w≤0 quads before rasterization, both paths. |
| MEDIUM PbrReflections energy | ✅ | CPU `refl*alpha + bg*(1-alpha)` matches GPU; energy-conserving. |
| MEDIUM pbrMaterialFromTbo OOB clamp | ✅ | `chunk_pbr.frag:70-76`: OOB → row 0. |
| MEDIUM dither unification | ✅ | All three paths: `floor(alpha*255) ≤ bayer*17`, identical matrices. |
| MEDIUM GL33 dtor leaks | ✅ | All 7 named objects released (`gl33_renderer.cpp:110-124`). |
| LOW meshKey UB | ⚠️ | VK fixed; **GL (`gl33_renderer.cpp:329`) still `(int64_t(m.cx) << 40)` — signed shift of negatives is UB**. |
| LOW uploadToArena / stats_.meshes / loadText / VK warning | ✅ | All four check out at the cited lines. |
| Dead code (greedy_mesher, axis_culling, software_opt, pbrTierIsStubbed) | ✅ | Zero tree-wide references; mid-run `engine_tests.cpp` race resolved. |
| Render opts (member buffers, nl==0 skip, scratch hoist) | ✅ | All three verified in code. |

### §2 Sim / core (10 findings + opts)

| Fix | Verdict | Evidence |
|---|---|---|
| HIGH getBlock race → blockMutex_ | ✅ | Sim holds mutex across drain+ticks (`sim_thread.cpp:262-318`); callers are game thread + MCP thread only — no self-deadlock path; blocking bounded by ≤16-step iteration. |
| HIGH LightEngine underflow | ✅ | Signed math + `nl <= 0` continue; no off-by-one (level-64 → 63 cells, level-1 → none). |
| MEDIUM removeSource two-pass | ✅ | `sources_` written only by `addSource` — sole seeding path; pass-2 repairs over-darkening. |
| MEDIUM tick-quantized edits | ✅ | Drain stamps current tick; `kApplyAllEdits` when paused; `resize(kept)` — no double-apply. |
| MEDIUM wireless tie-break | ✅ | Strict total order (strongest → nearest → packPos); registration-order independent. |
| MEDIUM setTickRate SIGFPE → simFailFast | ✅ | `isPow2Rate` correct; 32–512 pass; 0/negative/non-pow2 abort. |
| MEDIUM EntityManager sorted iteration | ✅ | Both `tick()` and `querySphere()` sort ids first. |
| LOW wire-network sort / TickContext rename / TickArena align | ✅ | All three verified; no stale references. |
| Sim opts (5 applied) | ✅ | Column cache bit-identical; MeshInput pool bounded; hitInfo staleness documented in-code. |

### §3 Net (8 fixed, 1 flagged + opts)

| Fix | Verdict | Evidence |
|---|---|---|
| MEDIUM ready_ cap (256, drop-incoming) | ✅ | Both insertion points capped (`channels.cpp:171,200`); `onAck()` runs before drop (`:154`); counter `uint64_t`, no UB. |
| MEDIUM spoofed Reject (partial) | ✅ | Single choke point (`handshake.h:304`); bound-only; "does nothing until game wires token" accurately documented. |
| LOW handshake liveness | ✅ | Recognized ids only refresh; no legit type starves. |
| LOW requestFull limiter | ✅ | ≤1/32 builds; server resyncs bypass at `:83,:145`; client can't reset (private state). |
| LOW satAdd int64+clamp | ✅ | Both pos and vel paths clamp (not wrap). |
| LOW confinement log suppression | ✅ | Rollover emits summary before reset; first denial of new window never lost. |
| LOW version.h dead code | ✅ | Zero references tree-wide. |
| Opts #1 splice / #2 normalize / #3 headers / #5 merge | ✅ | Splice handles packed/cut/spawn/removal; `build()`=normalized by construction; legacy path unchanged; no use-after-move. |
| Flagged: spoofed-ack, opt #4 | ✅ | Correctly NOT applied; `poll()` copy intact. |

### §4 Script / mod / console / MCP (11 findings + opts)

| Fix | Verdict | Evidence |
|---|---|---|
| HIGH consoleCmd ambient authority | ✅ | RAII first-statement + dtor restore; `concmd.cpp` has zero `throw`; mask is the mod's; nesting composes as a stack; `dispatchKeyBinds`/`tick()` also save/restore. |
| HIGH bind permission laundering | ✅ | Stamp at `setKeyBind`, fail-closed reinstall at dispatch; mask-less entries unreachable (private `keyBinds_`, always explicit stamp). |
| MEDIUM quota abort | ✅ | `suspend()` null-guarded `Abort()`; `run()` is the only `Execute()` site and tracks with prev-restore. |
| MEDIUM parseUint32 | ✅ | Identical fail-closed copies; narrowing check rejects `"9999999999"` (digit-count alone wouldn't). |
| MEDIUM query narrowing | ✅ | INT32 bounds before any cast, console + MCP paths; cell math in 64-bit. |
| MEDIUM exec TOCTOU | ✅ | O_NOFOLLOW + fstat-regular + `/proc/self/fd` re-validation; validated filename deliberately not canonicalized; `loadArchived` deleted; `saveArchived` confined. Minor: interior slashes pass the "bare filename" comment (containment still holds via fd re-validation on Linux). |
| LOW kPermPilotSenses schema | ✅ | `1<<11`, unique; in `kNameBits` + `kAllPermissionBits`; version correctly left at 2 (JD's call). |
| LOW HTTP sessions | ✅ | 64 LRU + 30-min idle, sweep on create, fail-closed expiry, auth'd DELETE. Minor: exact-timestamp-tie victim theoretically the new session (practically unreachable). |
| LOW stdio 16MiB cap | ✅ | Chunked reads, cap trips mid-read; `pending_` can't accumulate across calls (≤8 KiB tail). |
| LOW bumpCVar saturation | ✅ | NaN→0, LLONG clamps, `llround` unchanged in-range; no UB gap. |
| LOW revocation → rebuildSandbox | ⚠️ | Idempotent triple compare IS padding-safe; **but the "never call while a script executes" contract is documented 4× and enforced 0×** — `rebuildSandbox`/`syncBindings`/`setModPermissions` have no assert/guard. No production callers today; builder must add `assert(executingCtx_ == nullptr)` before per-mod engines land. |
| Opts (single-parse, dispatchStateless, key-once, lazy findTool) | ✅ | No behavioral change; lazy index sound (tools_ only grows in ctor-called register). |

### §5 Audio / file / storage (17 fixed, 1 flagged + opts)

| Fix | Verdict | Evidence |
|---|---|---|
| HIGH Anvil 7-bit overflow | ✅ | `static_assert` non-tautological (fires if overhead→11); sidecar write/read/delete complete; single header constexpr source of truth. |
| HIGH OcclusionThread race | ✅ | All structural mutations under `queriesMu_`; acquire/release pairing correct (positions before `active` release-store); `gain()` safe (no move/erase). |
| HIGH AudioStreamer race | ✅ | All 3 disposal paths enumerated — exactly-once invariant holds; double `pendingReset_` harmless (idempotent). |
| HIGH recoverCrashedWrite | ✅ | Order: primary validates → nothing; `.bak.1`..`.bak.32` newest-first **COPY** (`copy_file`, no rename of backup); torn primary → `.torn` (never deleted); fsync restored file + dir; intact `.tmp` promoted. Header-level validators + loader full validation as final gate (header-valid/body-corrupt can't be served as good). err empty=set semantics exact. Both `RegionFile::open` and `RecordStore::load` call it first on the absent path — no early-return skips. All 3 doc claims corrected. Newest-first can't regress a world (torn primary can't hold committed data newer than `.bak.1` under atomic-rename writes). |
| MEDIUM defrag mid-reindex | ⚠️ | `indexDirty_` survives `resetSession()`; heal path exists; `kMaxRestarts=3` bounds restarts. **But the flag is per-service while stores are per-map: `cancel()`'s sync-complete path (`defrag.cpp:357-364`) clears it without the store guard `pumpReindex` uses — multi-store defrag can lose one store's dirty record.** |
| MEDIUM shed one-directional | ✅ | Bitmask tracks engine-virtualized voices; `apply()` drops on every game-driven change; step-down clears exactly masked. |
| MEDIUM DecodeFn error channel | ✅ | Wire-up-time check + single log; worker breaks on first short chunk (≤1 error log/stream). |
| MEDIUM strand identity | ✅ | `absolute().lexically_normal()` at `submitStrand` entry; all insert/lookup/erase use normalized path. Symlink caveat documented. |
| MEDIUM scrubNonFinite + clamps | ✅ | Runs after mix, before tap copy + device store; `freq_` written only by `noteOn` (NaN→440, clamped). |
| LOWs (8) | ✅ | All eight verified: slot bounds unconditional early-return; quarantine `clear()` resets both; `operator=` fixed; EINTR Linux-only (non-Linux fail-closed); 64MiB caps routed; IoFence runtime check; command queue 1024+64 reserve with telemetry. |
| Flagged: `AnvilRegion::locate` kept | ✅ | Correct call — `engine_tests.cpp:3246` uses it; audit's "zero callers" was wrong. |
| Opts (deferDirFsync, memset, template, scratch, addIntoDuplicated) | ✅ | deferDirFsync's POSIX-rename-atomicity assumption documented, not over-claimed; addIntoDuplicated bit-identical. |

### §6 Game (7 findings + W1/W2/W3 + opts)

| Fix | Verdict | Evidence |
|---|---|---|
| HIGH sim.tickrate snap | ✅ | `nearestTps` boundaries correct (40→32, 384→256); cvar can only produce engine-acceptable values — no cvar→simFailFast path. |
| MEDIUM paused dimension switch | ✅ | Re-apply after `loadActive()`; both entry paths covered. |
| MEDIUM menu dolly | ✅ | Fresh snapshot in `enterMenu`, restore in `enterPlay`; no stale-restore path (exitToMenu comes from pause dialog, not launch menu). |
| MEDIUM pause no-op in menu | ✅ | Double-guarded (`b.pauseToggle` message + `setPaused` guard); LithoScript `pause` routes through `b.pauseToggle` — no bypass. |
| LOW saveWorld bool | ✅ | Header/impl both `bool` — baseline mismatch resolved; `b.save` handles false. |
| LOW config.lth boot exec | ✅ | After all archived cvars (incl. `mcp.*`), before boot-apply and `autoexec.lth`; sandboxed path; permissions = game's own console context (same as what the player can type — no escalation). |
| LOW hud.scale Int | ✅ | `CVarType::Int`, clamp 0–2 in setter and `load()`. |
| W1 transparent merge / W2 ambient_mode / W3 adapter reversal | ✅ | Opaque-then-transparent order; cvar clamp + boot/live apply via `ambientModeFromInt`; `noteMesh` verbatim copy, cross-checked with §1 backface fix — no double-inversion. |
| Game opts (frameParams, uploadEntities) | ✅ | Both verified. |
| Observation: `cfg_.tps` sync | ✅ | Confirmed still open, not half-fixed. |

---

## (b) New bugs introduced by the fixes

**None broken on a live path.** The 4 questionable items above are the complete list — two are incomplete fixes (same "one twin missed" pattern), two are unenforced contracts:

1. **[HIGH] Non-arena VAO still binds pre-fix layout** (`engine/src/render/gl33_renderer.cpp:339-345`). The CRITICAL fix covered struct, mesher, entity VAO, arena VAO, and Vulkan — but missed the per-chunk non-arena VAO in `uploadChunkMesh`, which still binds loc1=1×uint@6 ("slot") vs the shader's uvec4 aInfo. Dormant by default (`useArena_` true; game passes `glArena=true`), but disabling the arena corrupts every chunk (all normals +X, wrong UVs/layer/AO). Fix: copy the entity-VAO bindings (one line each).
2. **[MEDIUM] GL `meshKey` signed-shift UB** (`engine/src/render/gl33_renderer.cpp:329`). `(int64_t(m.cx) << 40)` with negative coords is still UB; the VK twin was fixed. Fix: cast through `uint64_t` first, mirroring `vk_renderer.cpp:37-41`.
3. **[MEDIUM] `rebuildSandbox` re-entrancy contract unenforced** (`engine/src/script/script_engine.cpp:290,261,255`). Documented in 4 places, guarded in 0. A mid-execution call would `ShutDownAndRelease()` under a running context. No production callers today (selftests only; not a script binding) — must be fixed before the builder wires per-mod engines. Fix: `assert(executingCtx_ == nullptr)` or fail-closed early-return + log.
4. **[MEDIUM] Defrag dirty-flag store confusion** (`engine/src/defrag/defrag.cpp:357-364`). `indexDirty_`/`indexDirtyStore_` are per-service; `stores_` is a map. `cancel()`'s sync-complete path clears the flag unconditionally after feeding the current store — can clear a dirty record belonging to a different store (the `pumpReindex` clear path at `:528` is store-guarded; `cancel()` isn't). Low-probability trigger today (multi-store defrag with two dirty indexes), real bookkeeping hole. Fix: guard the clear with `indexDirtyStore_ == current store`, mirroring `:528`.

---

## (c) Fresh sweep — new findings (ranked)

### MEDIUM
5. **`vision.set_camera` accepts unbounded coordinates → NaN into the software rasterizer.** `reqNumber("x"/"y"/"z")` has no range check (`engine/src/mcp/server.cpp:1683-1685`); `setCameraPose` validates only pitch/fov (`mcp_senses_adapter.cpp:171-180`). `1e308` parses (JSON rejects only ERANGE) → `float()` → inf → NaN through `lookDir`/`perspective` → `int(std::floor(NaN))` is UB in `rasterizeTriangle` (benign on x86 → `INT_MIN` → empty frame). Agent-facing (`kPermPilotSenses`), self-DoS only. Fix: `isfinite` + `|coord| ≤ 1e6` guard in `setCameraPose`.
6. **`IoFence` dtor fail-open on job threads → potential UAF.** The fix changed assert→log+return -1 in `wait()` (`engine/src/storage/async_io.cpp:172-185`), but `~IoFence()` calls `wait()` — a fence destroyed on a job thread with pending ops returns immediately while the in-flight op later calls `onOpDone` on the destroyed object (raw `IoFence*` captured at `:63,75,97,104`). Before: debug assert / release deadlock; now: log + potential UAF. Narrow (requires fence misuse on a job thread), but the new path is less safe than a loud abort. Fix: detach semantics or `shared_ptr` self-ownership for in-flight ops.

### LOW
7. **`render.ambient_mode` silently ignored on Vulkan.** `VulkanRenderer` has no `setAmbientMode` override (`IRenderer::setAmbientMode` defaults to no-op) — the cvar does nothing on VK. Same gap on the MCP vision path (software `ambientMode` field never wired from the cvar — vision frames always Fraction). Fix: VK override (or document GL-only) + wire `syncCvars` → `sw_.setAmbientMode`.
8. **Symlinked player configs silently don't load at boot.** `exec` now refuses symlinks even in the config dir; `game.cpp` ignores `executeFile`'s return — a dotfiles-style setup loses `config.lth`/`autoexec.lth` with only a debug-console line. Deliberate security behavior; recommend a `logWarn` at boot when the file exists but is refused.
9. **`coerce` Int lacks ERANGE check** (`engine/src/console/concmd.cpp:231-236`). `strtol` clamps huge input to `LONG_MAX` without failing; the settings change-fn's `stoi` then throws → caught → silent no-op with cvar/settings desync. Fix: reject on `errno == ERANGE`.
10. **`mcp.audio_tier` / `camera.chunk_radius` unvalidated for future wiring.** Both are write-only today (nothing reads them); when the builder wires them, raw `std::stoi` values (negative, 2³¹) flow straight in. `mcp.vision_width/height` are clamped at use — these two aren't. Fix: clamp at registration, one line each.
11. **PNG CRC-32 table init not thread-safe** (`engine/src/mcp/senses.cpp:13-21`). Idempotent writes (benign in practice) but formal UB on concurrent first captures. Unreachable today (single-threaded transports). Fix: `std::call_once` or function-local static.
12. **`ScriptEngine::loadDirectory` follows symlinks** (`engine/src/script/script_engine.cpp:350-351`). Local-trust dir, sandbox-executed — by design; noted for consistency with the hardened console `exec`.

### Clean bills (checked, no findings)
Completer (no path escape, no symlink deref) · text indexer (UTF-8 safe) · `RecordStore::deserialize` (bounds validated, no overflow) · anvil read path · audio tap ring · all MCP tool-arg validation (`lithoscript_exec` runs at the agent's mask, 64 KiB cap) · IExtensible (unwired stub) · deadlock audit on all fix-introduced mutexes (`blockMutex_`, occlusion `queriesMu_`, senses mutexes, `IoFence`) · vision PNG encoder bounds · boot surface (`config.lth`/`autoexec.lth`/scripts/mod dirs all local-trust; no Fragment/mod write path into config dir).

---

## Behavior-change regression verdicts (12 traced)

| # | Change | Verdict |
|---|---|---|
| 1 | `sim.tickrate` snap + `simFailFast` | ✅ CLEAN — no legitimate runtime path can reach the abort; saves don't store tickrate. |
| 2 | Cheat-gated `consoleCmd` refused for game scripts | ✅ CLEAN — no in-tree script issues cheat-gated commands. |
| 3 | `config.lth` executes at boot | ✅ CLEAN by design — same trust as console input; planting requires config-dir write (mods lack file bindings). Latent LOW: `escapeValue` doesn't escape `\n` (unexploitable today — no String cvars registered). |
| 4 | `pause` no-op in menu | ✅ CLEAN — old behavior was pure bug; no menu→pause→save dependency. |
| 5 | `exec` refuses symlinks | ✅ CLEAN in-tree; caveat #8 above for dotfiles-style players. |
| 6 | Backface fix + adapter reversal removal | ✅ CONSISTENT — GL, software, and MCP vision agree; `engine_render` passes. |
| 7 | `render.ambient_mode` added | ⚠️ PARTIAL GAP — ignored on Vulkan and MCP vision (finding #7). No vtable issue (default no-op). |
| 8 | `recoverCrashedWrite` on open path | ✅ CLEAN — fast path is one `exists()` + header validation; 32-stat scan only on missing/torn primary. |
| 9 | `getBlock` takes `blockMutex_` | ✅ CLEAN — no self-deadlock (callers: game thread, MCP thread; sim uses `chunks_` directly). |
| 10 | Tick-quantized edits | ✅ CLEAN — ≤1 world-tick added latency; nothing spins on edit landing. Observation: `saveWorld` doesn't drain the edit queue (pre-existing; one-line follow-up). |
| 11 | Mid-run transients | ✅ ALL RESOLVED — fresh rebuild clean; `engine_console`/`engine_storage`/`engine_audio`/`engine_render`/`engine_core` pass; full ctest 18/20 (2 grandfathered only). |
| 12 | `hud.scale` Int coercion | ✅ CLEAN — no in-tree fractional uses; loud error beats silent truncation. |

## Flags carried forward (unchanged from fix-run)
Net spoofed-ack eviction (builder: authenticated KEX) · spoofed Reject DoS full fix (builder: KEX) · `compSleep_` default (JD) · SBO job type (builder) · `kPermissionSetVersion` 2→3 (JD) · per-save-set backup rotation redesign (deferred: format change) · `cfg_.tps` sync on dimension switch (one-line follow-up) · vision rate-limit test wall-clock flake (fake-clock follow-up).

---

*Report complete — 4 reviewers, read-only, no tree modifications. JD's doctrine held: no blind spots; clean bills given with evidence, questionable items named with file:line.*

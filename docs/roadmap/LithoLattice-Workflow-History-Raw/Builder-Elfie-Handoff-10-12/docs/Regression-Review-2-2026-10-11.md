# Regression Review #2 — LithoLattice / LithoKraft — 2026-10-11

**Method:** 5 parallel read-only reviewers over `~/workspace/litholattice`. Four verified each fix group from the follow-up batch (§render, §script+defrag, §senses+storage, §console+game LOWs) against the actual code; one ran a fresh adversarial sweep of everything the follow-up batch touched, tracing behavior changes. No files modified, nothing built.

**Scope:** the 10 items in "Follow-up fix batch (regression review) — 2026-10-11 ~14:15 EDT" at the end of `Fix-Run-Report-2026-10-11.md`, which addressed the 4 questionable fixes + 8 new findings from Regression Review #1 (`Regression-Review-2026-10-11.md`).

---

## Executive summary

| | Count |
|---|---|
| Fixes VERIFIED (with file:line evidence) | **8** |
| Fixes QUESTIONABLE (incomplete — none broken on a live path) | **2** |
| Fixes BROKEN | **0** |
| New bugs introduced by the follow-up batch | **1 MEDIUM (latent), 3 LOW** |
| Fresh-sweep new findings (batch-adjacent) | **1 MEDIUM, 1 LOW (+ pre-existing notes)** |
| Flagged LOWs confirmed correctly NOT applied | **2 / 2** |

**The 2 questionable fixes:**
1. **[MEDIUM] GL meshKey UB fix incomplete — again.** The fix at `gl33_renderer.cpp:331` is bit-identical to the VK twin, but `uploadPbrChunkMesh` at `gl33_renderer.cpp:204` still packs with `(int64_t(m.cx) << 40)` — the exact signed-shift UB this fix was meant to eliminate, one function away. Same "one twin missed" pattern as review #1. Latent in practice (two's-complement; insert/erase consistent), but formally the same MEDIUM.
2. **[LOW→MEDIUM] Cvar clamps incomplete.** `mcp.audio_tier` (0..3) and `camera.chunk_radius` (0..64) got hooks, but `mcp.vision_width`/`mcp.vision_height` (`settings_cvars.cpp:144-148`) document 16..512 ranges in help text with NO clamp hooks — the exact hazard class the fix's own comment warns about.

---

## (a) Per-fix verification

Verdicts: ✅ VERIFIED · ⚠️ QUESTIONABLE · ❌ BROKEN.

### 1. [HIGH] PackedVertex non-arena VAO rebound — ✅ VERIFIED
- `engine/src/render/gl33/gl33_renderer.cpp:345-350` (non-arena `uploadChunkMesh`): loc0 = 3×ushort@0, loc1 = 4×ubyte@6, loc2 = 2×ubyte@10 — matches the claim.
- `:315-320` (entity VAO) and `:554-559` (arena VAO, `bindArenaAttribs`): byte-identical binding sequences.
- `engine/assets/shaders/chunk.vert:4-6`: `in uvec3 aPos` (loc0), `in uvec4 aInfo` (loc1), `in uvec2 aShade` (loc2) — integer attributes, matching `glVertexAttribIPointer`. Struct offsets (`renderer.h:36-38`, `#pragma pack(1)`) confirm aInfo bytes @6..9, aShade @10..11.
- No other PackedVertex VAO retains the old layout (the PBR VAO at `:215-222` binds `PbrQuadVertex` to its own shader — offsets match that struct).

### 2. [MEDIUM] GL meshKey UB — ⚠️ QUESTIONABLE (incomplete)
- `gl33_renderer.cpp:331`: `int64_t((uint64_t(uint32_t(m.cx)) << 40) ^ (uint64_t(uint32_t(m.sy)) << 32) ^ uint64_t(uint32_t(m.cz)))` — bit-for-bit identical to VK twin `render_vk/vk_renderer.cpp:40`.
- **But** `gl33_renderer.cpp:204` (`uploadPbrChunkMesh`): `(int64_t(m.cx) << 40) ^ (int64_t(m.sy) << 32) ^ int64_t(uint32_t(m.cz))` — signed shift of possibly-negative coords, the same UB. Repo-wide sweep (`<< 40` / `<< 32`): every other hit is unsigned-safe; line 204 is the only remaining signed-shift of coordinates.
- Mitigating: PBR insert (`:209`) and erase (`:205`) use the same expression, and two's-complement targets produce identical bits — latent, not live.

### 3. [MEDIUM] rebuildSandbox guard — ✅ VERIFIED
- `engine/src/script/script_engine.cpp:301`: `assert(executingCtx_ == nullptr)` — first statement, before `ShutDownAndRelease` at `:308`.
- `:302-305`: fail-closed runtime guard — `logError` + early return. Correctly ordered (debug fail-loud, release fail-closed).
- Only callers of `rebuildSandbox` in the tree: `syncBindings` (`:282`) and `unbindAll` (`:287`); `syncBindings` only called from `setModPermissions` (`:259`). No script-reachable path (tree-wide grep; only boot at `game.cpp:257` and selftests, all host-side).
- Early return leaves the engine exactly as it was (no mutation before the guard) — sane state; the documented fail-closed tradeoff (revocation deferred, not lost).

### 4. [MEDIUM] Defrag cancel() store guard — ✅ VERIFIED
- `engine/src/defrag/defrag.cpp:361-364`: `if (indexDirty_ && indexDirtyStore_ == sessionStore_) { indexDirty_ = false; indexDirtyStore_.clear(); }` — guard runs before `resetSession()` clears at `:374`. Correct ordering.
- `pumpReindex` clear path (`:534-537`): character-for-character identical. Only two clear sites in the tree; all other touches are sets (`:366-367, :370-371, :400-401` in `fail()`), correctly tagging the current store.
- Types: both `std::string` (`defrag.h:218, :233`) — proper identity comparison.

### 5. vision.set_camera guard — ✅ VERIFIED
- `game/LithoKraft/src/game/mcp_senses_adapter.cpp:177-181`: `!isfinite` on x/y/z → reject; `fabs > 1e6` (`kMaxCameraCoord`) on all three → reject. Pitch (−90..90) / fov (10..120) checked at `:172-173`.
- Runs before the mutex (`:182`) and before pose store (`:186`); float narrowing happens later at render (`:127`) on the already-guarded pose. Rejection returns `false` with zero state mutation; server maps to `ToolResult::error` (`engine/src/mcp/server.cpp:1691-1692`).
- 1e6 ≈ 3.8× above the largest locked world extent (±262,144, Space). No legit pose clipped.
- Server side (`server.cpp:1683-1689`) is a straight JSON→adapter pass; the adapter guard is the single chokepoint. `reqNumber` requires `isNumber()`; `1e309` → inf via `asDouble()` is exactly what `isfinite` catches.

### 6. IoFence dtor fail-closed — ✅ VERIFIED
- `engine/src/storage/async_io.cpp:151-166`: out-of-line dtor; on `wait() < 0` (job-thread refusal) with `pending_ > 0` → log + `std::abort()`; zero pending → destruction proceeds. Header documents it (`async_io.h:58`).
- `pending_` incremented under the fence lock in `submit()` (`:62`) and `submitStrand()` (`:95`) before enqueue, decremented under the same lock in `onOpDone()` (`:184-188`), read under the same lock in the dtor (`:160`) — no undercount path to a missed UAF.
- Job-thread detection sound (`thread_local t_onJobThread`, `job_system.h:245,263,288,300,493`).
- No production users (tree-wide: only `async_io.{h,cpp}` + `engine_tests.cpp` selftests). No existing test destroys a fence on a job thread with pending ops — the abort is a genuine tripwire, not a latent crash.

### 7. Symlinked-config logWarn — ✅ VERIFIED
- `game/LithoKraft/src/game/game.cpp:89-92`: `std::filesystem::is_symlink(ce, slEc)` → `logWarn("game: '%s' is a symlink; console exec refuses symlinks, config.lth not loaded", ...)`; `:93` non-symlink path unchanged (`else consoleRegistry_.executeFile(...)`). Same pattern for autoexec at `:173-177`.
- Checks the symlink condition explicitly pre-call (the previously-ignored `executeFile` return is no longer the failure channel). `logWarn` is real (`log.h:17`, included at `game.cpp:16`).

### 8. coerce ERANGE — ✅ VERIFIED
- `engine/src/console/concmd.cpp:238-240` (Int): `errno = 0` before `strtol`, reject on `ERANGE`. `:248-250` (Float): same with `strtod`.
- Rejection is loud: `setCVar` sets `err = "value '<raw>' is not a <type>"` (`:374-377`; default-value path `:334-335`). No silent no-op.
- Neighboring parse paths all fail-closed or deliberately loud: `parseInt64` (`:88`) manual digit loop with overflow reject; `parseDouble` (`:95`) `stod` + `isfinite` + catch; `wait`'s strtol (`:585`) clamps LOUDLY; `bumpCVar`'s strtod (`:628`) reads canonicalized stored values.

### 9. Cvar clamps — ⚠️ QUESTIONABLE (incomplete)
- Hooks exist with right ranges: `settings_cvars.cpp:186-187` — `clampHook(0, 3)` on `mcp.audio_tier`, `clampHook(0, 64)` on `camera.chunk_radius`.
- Termination sound: `:181-183` only re-sets when `clamped != v` — set(999) → set(64) → passes through unchanged. Type-consistent (Int cvars, `stoi` on canonicalized values, try/catch backstop).
- **Miss:** `mcp.vision_width` / `mcp.vision_height` (`:144-148`) document ranges 16..512 in help text but have NO clamp hooks — raw `2^31` flows into future capture code. Same-file, same hazard class the fix's own comment (`:166-170`) warns about.

### 10. PNG CRC-32 static table — ✅ VERIFIED
- `engine/src/mcp/senses.cpp:19-30`: function-local `static const std::array<uint32_t, 256>` — C++11 magic statics, thread-safe init by construction.
- Spot-checked against standard IEEE CRC-32: t[0]=0x0, t[1]=0x77073096, t[2]=0xee0e612c, t[128]=0xedb88320, t[255]=0x2d02ef8d — all canonical (table generated by the standard reflected loop, poly `0xEDB88320`, `:24`).
- Read-only after init (`crc32` at `:33-38`); no writes, no `const_cast` in the file.

### Flagged LOWs — confirmed correctly NOT applied
- **ambient_mode on Vulkan/MCP vision:** no `VulkanRenderer` class exists in the tree (zero hits under `engine/src/render/`); `SoftwareRenderer` has no `ambientMode` field and no `setAmbientMode` override; the only override engine-wide is `GL33Renderer::setAmbientMode` (`gl33_renderer.cpp:159`), base default at `renderer.h:158` is a no-op; zero "ambient" hits under `engine/src/mcp/`. Still effectively GL-only — scope respected.
- **loadDirectory symlinks:** `script_engine.cpp:350-354` still uses `fs::directory_iterator` + `e.is_regular_file()` (follows symlinks), zero "symlink" mentions in the file. Unchanged, by design.

---

## (b) New bugs introduced by the follow-up batch

1. **[MEDIUM] PBR meshKey shift-UB missed (fix 2's twin).** `engine/src/render/gl33/gl33_renderer.cpp:204` (`uploadPbrChunkMesh`): `(int64_t(m.cx) << 40) ^ (int64_t(m.sy) << 32) ^ ...` — left-shifting negative signed values is UB on negative chunk coords. The batch's stated goal ("mirroring vk_renderer.cpp:37-41 bit-for-bit") is unfulfilled on the PBR path. Latent on two's-complement targets (insert `:209` / erase `:205` use the same expression), but formally the same MEDIUM the batch just fixed, one function away. Fix: apply the identical `uint64_t(uint32_t(...))` cast.
2. **[LOW] Fail-closed debris in unbindAll/syncBindings (fix 3's guard).** Only triggerable by violating the documented contract (unreachable from scripts; all host callers are boot/selftests outside `run()`). On refusal: `unbindAll()` clears `bindings_` *before* `rebuildSandbox()`, leaving registrations with destroyed records — orphaned until restart. In `syncBindings`, revoked records are marked `b.bound=false` before the rebuild; on refusal a later same-mask `syncBindings` computes `revoked=false` and never retries. Reordering to rebuild-then-clear would make refusal state-neutral.
3. **[LOW] Spurious error log in ~IoFence (fix 6).** The dtor calls `wait()` unconditionally, which logs "wait() called from a job thread; refusing" even when `pending_==0` and destruction proceeds normally. Cosmetic log noise only.
4. **[LOW] Subnormal float inputs now rejected (fix 8).** `ERANGE` conflates overflow and underflow, so `"1e-320"` is rejected alongside `"1e999"`. No game cvar plausibly needs subnormals; `"inf"`/`"nan"` literals still parse (strtod doesn't set ERANGE), preserving the `incrementvar`-relied "float cvar can hold inf" behavior.

---

## (c) Fresh sweep — new findings (batch-adjacent)

### MEDIUM
5. **`yaw` unvalidated on the vision.set_camera JSON path.** `engine/src/mcp/server.cpp:1687`: `pose.yaw = float(r.reqNumber("yaw"))` with no range/finite check; `yaw = 1e309` → inf float → `std::cos(inf)` = NaN in the forward vector (`mcp_senses_adapter.cpp:129-130`) → NaN reaches `lookDir` the same way coordinates used to. Fix 5 closed x/y/z; yaw is the same hole one field over. Fix: `isfinite` + range guard on yaw in `setCameraPose`.

### LOW
6. **`pitch` NaN passes the range check.** `mcp_senses_adapter.cpp:172`: `NaN < -90 || NaN > 90` is false, so NaN pitch survives validation. Currently unreachable (JSON `reqNumber` requires `isNumber()`), but fragile if the interface is ever called from non-JSON code. Fix: add `isfinite` to the pitch check.

### Pre-existing notes (not introduced by the batch; carried for the builder)
- **Defrag single-slot dirty record:** `indexDirty_`/`indexDirtyStore_` hold one store's record; if store X's session fails then store Y's fails, `fail()` (`defrag.cpp:400-401`) overwrites X's record — X's dirty mark silently lost. The new guard protects clears, not sets. A per-store map would eliminate it.
- **`run()` exception safety:** `script_engine.cpp:414-417` restores `executingCtx_ = prevCtx` as a plain statement after `ctx->Execute()`; any throw between set and restore leaves a stale value, permanently arming the new guard. Consider RAII if exceptions are ever adopted.
- **`chunk.vert:2` stale comment:** still says "12-byte PackedVertex"; the struct is 16 bytes (`renderer.h:38` static_assert). Cosmetic, but the kind of comment that misleads a future VAO fix.
- **Pack-override shader risk:** `chunk.vert` is overridable per-pack (`assets/packs/<pack>/shaders/chunk.vert`) with no VAO-vs-shader validation — a mismatched override silently reintroduces the corruption class.
- **Arena VAO bind-order fragility:** `bindArenaAttribs()` (`:552`) runs from `growArena()` after `glGenVertexArrays` (`:573`) — correct today, but attrib setup is split from VAO creation; a future caller binding `arenaVao_` first would bind VAO 0's state.
- **`incrementvar` strtod without errno reset** (`concmd.cpp:1019/1024/1029`): `1e999` delta → HUGE_VAL; lands in `bumpCVar`'s explicit clamp design (no desync), but loudness is inconsistent with the coerce fix.

### Clean bills (checked, no findings)
`chunk.h:66` shift (non-negative operands) · defrag selftests (fresh-cancel, stale-cancel paths) · `concmd.cpp` other strtol/strtod sites (`cmdWait`, `incrementvar` — deliberate, test-pinned) · all `setModPermissions` callers (boot + selftests, none in script context) · `setCVarChangeFn` replace-semantics (no hook stacking) · clamp-hook callers (none set from inside a change callback; no test asserts out-of-range raw values) · boot ordering (clamp hooks installed `game.cpp:72` before `config.lth` exec `:93`) · `erase(key)` in `uploadChunkMesh` uses the new key consistently.

---

## (d) Behavior-change regression verdicts (fixes 1–10)

| # | Change | Verdict |
|---|---|---|
| 1 | VAO rebound (non-arena) | ✅ CLEAN — all three VAO paths bind exactly what `chunk.vert` declares; GL state change only; mesher golden hash unaffected. |
| 2 | meshKey uint64_t cast | ⚠️ CAVEAT — fixed site bit-identical to real-world output of the old UB (no key-space change, no stale-map risk); PBR twin still UB (finding b.1). |
| 3 | rebuildSandbox guard | ⚠️ CAVEAT (LOW) — assert can't fire in any existing selftest; runtime guard is the real enforcement in Release; refusal leaves the debris in b.2, contract-violation-only. |
| 4 | Defrag cancel() store guard | ✅ CLEAN — mirrors `pumpReindex` exactly; preserved flag is genuine dirty state with a tested heal path; early-return branch doesn't touch the flag. |
| 5 | setCameraPose guard | ✅ CLEAN — rejections return before mutex/state mutation (prior pose intact); MCP tool layer returns a clean error. |
| 6 | IoFence abort | ✅ CLEAN — no existing test destroys a fence on a job thread with pending ops; zero-pending destruction proceeds (modulo LOW log noise b.3). |
| 7 | Symlink logWarn | ✅ CLEAN — `is_symlink(p, ec)` can't misfire on regular files; `O_NOFOLLOW` + `/proc/self/fd` re-validation remains the real gate (worst case: warn fires, swapped-in regular file executes legitimately). |
| 8 | coerce ERANGE | ✅ CLEAN (modulo LOW b.4) — no test feeds huge values expecting success; previously "accepted" huge configs were silent no-ops with desync (now loud rejections — strictly better). |
| 9 | Cvar clamps | ✅ CLEAN — `setCVarChangeFn` replaces (no stacking); recursion terminates after one level; no in-callback sets; hooks installed before `config.lth` executes. (Completeness gap noted in a.9.) |
| 10 | PNG CRC table | ✅ CLEAN — `static const`, no `const_cast`, read-only use; magic statics guarantee thread-safe init. |

---

## Flags carried forward (unchanged)

From review #1, still open: net spoofed-ack eviction (builder: authenticated KEX) · spoofed Reject DoS full fix (builder: KEX) · `compSleep_` default (JD) · SBO job type (builder) · `kPermissionSetVersion` 2→3 (JD) · per-save-set backup rotation redesign (deferred: format change) · `cfg_.tps` sync on dimension switch (one-line follow-up) · vision rate-limit test wall-clock flake (fake-clock follow-up).
From review #2, new: **PBR meshKey UB** (`gl33_renderer.cpp:204`, one-line cast — recommend folding into the builder's render pass) · **`vision.set_camera` yaw guard** (same-file follow-up to fix 5) · **vision_width/height clamps** (`settings_cvars.cpp:144-148`, same one-line pattern) · ambient_mode Vulkan/MCP vision (builder/render pass, previously flagged).

---

*Report complete — 5 reviewers, read-only, no tree modifications. JD's doctrine held: no blind spots; clean bills given with evidence, questionable items named with file:line.*

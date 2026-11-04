# LithoLattice / LithoKraft — Adversarial Repo Audit
**2026-10-11 · read-only · six subsystem reviewers, attacker mindset**

Scope: `~/workspace/litholattice` — engine (`engine/src/*`) + game (`game/LithoKraft`). ~62.5k lines (C++17). Nothing was modified; no fixes applied. Every finding cites an exact file:line, states concrete impact, and proposes a fix. Reviewers were instructed to verify before claiming and to say honestly when an area was clean.

Known issues deliberately NOT re-reported: `blocks.cpp` 40-vs-36 guard (builder's fix), `engine_sim` block-names narrowing, `lithokraft_selftest` SIGABRT (both grandfathered), `trace_jit_basic.cpp` (builder-owned).

## Executive summary

| Severity | Count |
|---|---|
| CRITICAL | 1 |
| HIGH | 12 |
| MEDIUM | 30 |
| LOW | 25 |
| **Total** | **68** |

Plus: 25 optimization items and 14 dead-code entries across the appendices.

### Top 5 (read these first)
1. **[CRITICAL] PackedVertex `slot` broke every GPU vertex binding** — `engine/src/render/gl33/gl33_renderer.cpp:528`, `engine/src/render_vk/vk_pipelines.cpp:185`. Every chunk renders corrupted on any real GPU (wrong normals, UVs, layers, AO). The VM has no GPU, so nothing caught it. The CPU golden hash was re-recorded; the GPU side never was.
2. **[HIGH] `sim.tickrate 0` in the console SIGFPE-crashes the process** — `game/LithoKraft/src/game/settings_cvars.cpp:66`. Non-cheat cvar, any player can type it, integer divide-by-zero on the sim thread. Same crash reachable via `SimThread::setTickRate(0)` with no validation.
3. **[HIGH] AnvilRegion loses payloads ≥ 65,015 bytes and then frees their sectors** — `engine/src/storage/anvil_region.cpp:320,403`. `put()` returns success; the data is unreadable immediately after, and the sectors get reallocated → silent corruption on top of loss.
4. **[HIGH] `writeFileAtomic` crash window leaves the world file *absent*** — `engine/src/storage/file_contract.cpp:114-130`. Crash between backup rotation and commit: target missing, old at `.bak.1`, new in `.tmp`, nothing recovers either. The "old or new, never torn" contract is false with backups enabled.
5. **[HIGH] Two audio-thread data races** — `engine/src/audio/streaming.cpp:24` (`AudioStreamer::play` vs `render`: vector clear-during-iteration → heap corruption) and `engine/src/audio/occlusion_thread.cpp:49` (`addQuery` vs iteration: heap-use-after-free).

### Alpha blockers (proposed — JD decides)
The following should be fixed before the 10/23 alpha, in rough priority order:
1. GPU vertex binding (CRITICAL) — the alpha runs on real GPUs; everything renders wrong otherwise.
2. `sim.tickrate 0` SIGFPE (+ engine-side `setTickRate` validation) — any console user can kill the game.
3. AnvilRegion sector overflow — silent chunk-data loss.
4. `writeFileAtomic` crash window — world loss on power failure during save.
5. AudioStreamer + OcclusionThread races — nondeterministic audio-thread crashes.
6. ShadeMode enum divergence — catchor readout renders wrong (visible).
7. Binary mesher opaque-only — PBR tiers 2/3 have no transparent geometry (either fix or gate the tier UI).
8. Software backface culling inverted — software renderer draws back faces.
9. `getBlock` cross-thread race (reachable today via MCP `world_get_block`).
10. Menu dolly discarding saved player position + paused-dimension-switch unpausing the sim (game feel / correctness).

NOT alpha-blocking (fix before mods / builder wiring): the two permission HIGHs (latent until per-mod engines are wired), LightEngine bugs (unwired), MCP session expiry, net plaintext attacks (documented, KEX is the plan).

---

## 1. Render — 1 CRITICAL, 3 HIGH, 8 MEDIUM, 1 LOW

### [CRITICAL] PackedVertex `slot` insertion broke every GPU chunk vertex-attribute binding
- **File:** `engine/src/render/gl33/gl33_renderer.cpp:528,530` (arena path, the default), also `:308-315` (non-arena/entity path); `engine/src/render_vk/vk_pipelines.cpp:185-186`; shaders `engine/assets/shaders/chunk.vert:4-5`, `engine/assets/shaders/vk/chunk.vert:8-11`
- **Impact:** `PackedVertex` is now 16 bytes (`renderer.h:28`: x,y,z @0, `slot` uint32 @6, u,v,faceAo,layer @10, value,mode @14), but all three binding sites still use the pre-`slot` 12-byte offsets, and neither shader declares a `slot` input. GL arena (live default, `glArena=true` in `game/.../main.cpp:243`) feeds `aInfo`=(slot bytes) @6 and `aShade`=(u,v) @10; Vulkan feeds `R8G8B8A8`@6 / `R8G`@10 identically. On any real GPU: wrong face normals (`N[slotByte&7]`), wrong UVs, wrong texture layer, wrong AO, wrong shade mode/value — every chunk corrupted. The CPU golden hash was re-recorded after the `slot` addition; the GPU side never was; the dev VM has no GPU. Software renderer immune (C++ field access).
- **Fix:** decide the contract once — move `slot` to the end of `PackedVertex` (keeping the 12-byte prefix stable), or update all three binding sites to @10/@14 *and* add the slot input to both chunk shaders (currently written by the mesher, consumed by nothing).

### [HIGH] ShadeMode enum ↔ chunk_shading.glsl mode numbers diverged
- **File:** `engine/assets/shaders/chunk_shading.glsl:102` (`else if (vMode == 12)` = 7-segment readout) vs `engine/src/render/include/litholattice/render/renderer.h:50` (`kShadeReadout` = 15; `kShadeAlphaBlend`=12, `kShadeWeightedOIT`=13, `kShadeAdditive`=14). Shared by GL and VK.
- **Impact:** the game's catchor block uses `kShadeReadout` (15) (`game/.../appearance.cpp:88`) → hits no shader branch → renders plain textured opaque; readout digits never appear. Landmine: anything wired to `kShadeAlphaBlend` (12) renders as a 7-segment display; 13/14 fall through to opaque.
- **Fix:** renumber shader branches to the enum, or stop hardcoding numbers in GLSL — emit constants from one shared source.

### [HIGH] Binary mesher emits opaque cubes only — PBR path cannot draw any transparent material
- **File:** `engine/src/render/binary_greedy_mesher.cpp:157-158` (`emit16 = (opaque && shape == Cube) ? 1 : 0`)
- **Impact:** glass/water/etc. produce zero quads on the BinaryQuad/PBR path; PBR transparency tiers 2–3 have no transparent geometry — transparent blocks are invisible holes. The tier UI offers options that render nothing.
- **Fix:** transparent-voxel emit pass in the binary mesher (separate quad stream), or restrict PBR tier selection until it exists.

### [HIGH] Software renderer backface culling is inverted
- **File:** `engine/src/render/software_renderer.cpp:49-53` (`isBackface`: `cross <= 0` → cull) + Y-flip in `transformVertex:44-45`
- **Impact:** with GL-style y-up NDC MVP, mesher CCW-from-normal quads land CW on screen (flip is orientation-reversing), so `cross <= 0` culls *front* faces. `software_opt.cpp` agrees with the wrong convention; `software_test.cpp` never rasterizes, so nothing pins it.
- **Fix:** flip the cull test to `cross >= 0`, or document y-down viewProj requirement and assert it.

### [MEDIUM] Flood-fill irradiance double-counts: up to 2× energy gain in daylight
- **File:** `engine/src/render/include/litholattice/render/pbr_lights.h:146-153`; consumed at `pbr_software_shaders.h:130`, `engine/assets/shaders/pbr_shading.glsl:169`
- **Impact:** full daylight → diffuse=1.0 *and* ambient=1.0 → `albedo × 2.0` → bright areas clip to white, destroying shading variation. Contradicts the energy-conservation claim in `pbr_brdf.h`. CPU and GPU agree — design-wide.
- **Fix:** make ambient a fraction of the sky term or normalize the sum.

### [MEDIUM] CPU `shadeTransparent` PbrReflections tier disagrees with GPU and gains energy
- **File:** `engine/src/render/include/litholattice/render/pbr_transparency_tiers.h:385-390` vs `engine/assets/shaders/chunk_pbr.frag:105-107`
- **Impact:** CPU computes full-PBR-refl + background×(1−alpha) (energy gain; ignores the documented Fresnel split 40 lines above); GPU alpha-blends sRGB(PBR) over background. Same tier, different images; the CPU "reference" is less physical.
- **Fix:** mirror the GPU branch in linear, or apply the Fresnel split properly; delete or use `splitReflectionTransmission`.

### [MEDIUM] `pbrMaterialFromTbo` doesn't clamp OOB IDs to row 0 despite claiming to
- **File:** `engine/assets/shaders/chunk_pbr.frag:62`
- **Impact:** stale/foreign material ID (< 0xFFFF but ≥ table size, e.g. older registry generation) reads past the buffer texture → undefined texels → black alpha-0 fragments (misbehaves in tier-3 pass). Software path falls back; GPU doesn't.
- **Fix:** pass table row count as a uniform; clamp id to `[0, count)`, fall back to row 0.

### [MEDIUM] Classic glass dither ≠ PBR ScreenDoor dither — coexistence incoherence
- **File:** `engine/assets/shaders/chunk_shading.glsl:99-101` vs `chunk_pbr.frag:87` + `transparency_method.h:31-34`
- **Impact:** same glass dithers to different pattern/density on classic vs PBR meshes in the same frame (m=0: classic discards a≤0.031, PBR discards a≈0).
- **Fix:** classic branch uses the same `alpha*255 > m*17` rule as `screenDoorTest`.

### [MEDIUM] Software `renderMesh` has no sub-chunk offset — every chunk rasterizes at the origin
- **File:** `engine/src/render/software_renderer.cpp:28-47,208-211`; `software_renderer.h:26-27`
- **Impact:** unlike GL (`uOffset`) and VK (push constant), no per-mesh origin — all sub-chunks overlap at local coords. World-space varyings also local → PBR view vectors and analytic-light attenuation wrong for any non-origin chunk.
- **Fix:** offset parameter on `renderMesh` (or bake into MVP per mesh; offset the varyings).

### [MEDIUM] Software renderer: no behind-camera clipping
- **File:** `engine/src/render/software_renderer.cpp:40-47`
- **Impact:** w<0 vertices project mirrored and can pass the depth test → phantom geometry; camera-plane-spanning triangles smear.
- **Fix:** clip/reject triangles with any w ≤ 0 before rasterization.

### [MEDIUM] Per-pixel PBR does heap allocation + full emissive enumeration per fragment
- **File:** `engine/src/render/include/litholattice/render/pbr_software_shaders.h:139-141` → `pbr_lights.h:83-118`
- **Impact:** O(pixels × emissive-candidates) + malloc/free per pixel in the render hot path — dominates frame time before rasterization does.
- **Fix:** hoist collection out of the pixel loop (per quad/tile); caller-provided scratch vector instead of return-by-value.

### [MEDIUM] `~GL33Renderer` leaks 7 GL objects
- **File:** `engine/src/render/gl33/gl33_renderer.cpp:97-102` — `ebo_`, `lineVao_`, `lineVbo_`, `entityVao_`, `entityVbo_`, `hudVao_`, `hudVbo_` never deleted.
- **Impact:** small one-time leak per renderer lifetime (process-lifetime → low in practice; real on error/re-init path).
- **Fix:** delete them in the destructor.

### [LOW] Miscellaneous render nits
- `meshKey` (`gl33_renderer.cpp:317`, `vk_renderer.cpp:38`): `(int64_t(cx) << 40)` left-shifts negative `cx` — technically UB. Cast through `uint64_t` first.
- GL arena `uploadToArena` (`gl33_renderer.cpp:582-596`): allocation failure after old range freed leaves stale `indexCount`/`first` → draws garbage from freed range. Set `indexCount = 0` on the failure path.
- Software per-frame allocations: `screen` vector per `renderMesh` (`software_renderer.cpp:213`); full-framebuffer copy per `applyFXAA` (`:263`). Reuse member buffers.
- `MaterialRegistry::loadText` (`pbr_materials.cpp:93-106`): no range validation — roughness 5, alpha −1, ior 0 load silently.
- Vulkan `uploadEntityMesh` (`vk_renderer.cpp:106`) silent no-op TODO — entities/items invisible on Vulkan with no log.
- GL `stats_.meshes` (`gl33_renderer.cpp:354`) counts classic meshes only, not PBR meshes.

## 2. Sim / core — 2 HIGH, 5 MEDIUM, 3 LOW

### [HIGH] Data race: `SimThread::getBlock` reads the live ChunkMap while the sim thread writes it
- **File:** `engine/src/sim/include/litholattice/sim/sim_thread.h:92-95` (and `:120`)
- **Impact:** `getBlock` takes `blockMutex_`, but no sim-thread ChunkMap writer ever takes it — the lock is theater. Live cross-thread caller: `engine/src/mcp/server.cpp:964` (MCP `world_get_block`, served on MCP server threads) calls `b.world->getBlock(...)` concurrently with the running sim. Torn `BlockId` reads, plus a genuine race on `unique_ptr<SubChunk>` objects — `ChunkMap::set` (`chunk_map.h:69`) writes the `unique_ptr` while `get` reads it when a block is placed into a null sub-chunk. UB, TSan-flaggable, reachable **today** via the MCP agent surface.
- **Fix:** take `blockMutex_` in `ChunkMap::set`/`setState` (contention cost at 512 TPS — likely unacceptable), route `getBlock` through the immutable `snapshotForMeshing` mechanism, or a seqlock/generation counter on sub-chunks. Minimum: correct the false "Thread-safe single-block read" contract comment.

### [HIGH] Integer underflow in `LightEngine::propagate` floods phantom light (latent — engine unwired)
- **File:** `engine/src/world/light_engine.cpp:63`
- **Impact:** `const uint8_t newLevel = cur.level - 1 - (opacity_[ni] / 16);` — `cur.level`=2 with `opacity/16`=3 gives `2-1-3 = -2` → wraps to **254**, which then satisfies `254 > light[ni]` and cascades across the whole volume. After the flood, real lights (≤64) can never win again — lighting permanently broken, mob spawning (below light 16) misbehaves. Zero production callers today (only a comment in `pbr_lights.h:24`) — latent until the lighting engine is wired, but it ships as *the* lighting engine.
- **Fix:** `const int nl = int(cur.level) - 1 - int(opacity_[ni]) / 16; if (nl <= 0) continue; const uint8_t newLevel = uint8_t(nl);`

### [MEDIUM] `removeSource` leaves permanent stale light
- **File:** `engine/src/world/light_engine.cpp:44-56`
- **Impact:** only the source cell is zeroed; neighbors re-queued with stale levels re-spread nowhere — the removed torch's light field is frozen forever. Removing a torch never darkens the room. Same latency caveat (unwired).
- **Fix:** two-pass removal (darken influenced region, re-propagate from surviving sources), Starlight-style.

### [MEDIUM] Player edits are not tick-quantized — replays diverge across machines
- **File:** `engine/src/sim/sim_thread.cpp:199`
- **Impact:** `applyEdits(ray_.read())` drains once per loop iteration, before the 0–16 tick catch-up loop. Which `worldTickN` an edit lands on depends on wall-clock frame pacing. Identical edit sequences at different frame rates attach to different tick indices → "same inputs → same outputs" fails for player input. The sim tick is wall-clock-free; the input boundary is not.
- **Fix:** stamp each `EditCmd` with a target tick (or drain inside the per-tick loop) so edit application is a pure function of tick index.

### [MEDIUM] Wireless tie-break is nondeterministic: equidistant equal-strength broadcasts
- **File:** `game/LithoKraft/src/content/include/lithokraft/content/signal.h:81-95` (`RadioRegistry::query`)
- **Impact:** on equal strength *and* equal distance², first broadcast in vector order wins — and that order comes from `unordered_map` bucket order (insertion/edit history). Two symmetric transmittors, same channel/strength, different colours → receivor input colour is edit-history-dependent → downstream logic diverges between runs. Genuine (if narrow) sim-state nondeterminism; the engine's own `signal_propagate.h` design doc warns about this class.
- **Fix:** canonical tie-break (lowest `packPos` wins on ties), or sort `next_` by position in `flip()`.

### [MEDIUM] `setTickRate(0)` / `ClockDomain{hz=0}` → SIGFPE on the sim thread, unreported
- **File:** `engine/src/sim/sim_thread.cpp:184-185`, `:212`, `:214`
- **Impact:** no validation. `setTickRate(0)` → `worldDiv = masterHz/0`; `ClockDomain{hz=0}` → `masterHz / d.hz`. Integer divide-by-zero → SIGFPE, no report. Negative tps → 100% busy-spin. Unreachable via shipped config paths today (CLI validates; game pushes 512 Hz only) — but one misconfigured line from an unreported hard crash, exactly what the fail-fast policy exists to replace with a logged abort. (Compounds the game's `sim.tickrate 0` cvar finding — same crash, two entry points.)
- **Fix:** validate in constructor and `setTickRate` (reject `tps <= 0`, `hz <= 0`, non-power-of-two); convert to `simFailFast` reports.

### [MEDIUM] `EntityManager` iterates `unordered_map` in tick and queries (fragile determinism)
- **File:** `engine/src/entity/include/litholattice/entity/entity.h:100-101`; `engine/src/entity/entity.cpp:36-62`
- **Impact:** `tick()` and `querySphere()` run in bucket order — a function of insertion/erase history. Reproducible within one STL build for identical op sequences (not run-to-run nondeterminism today), but fragile: any order-dependent consumer ("nearest entity", cross-entity damage) silently depends on it; differs across STL implementations and edit histories; save/load and net sync inherit the fragility.
- **Fix:** iterate in `id` order (sort keys, or dense `vector<Entity>` sorted by id) in `tick()` and `querySphere()`.

### [LOW] Wire-network ids assigned in `unordered_set` iteration order — violates canonical-id contract
- **File:** `game/LithoKraft/src/content/signal.cpp:353`
- **Impact:** network id assignment depends on edit history; `signal_propagate.h` documents ids must be canonical/stable. Benign today (floods order-independent, `exportState` sorts) — only `inspect()` shows unstable ids. Future order-sensitive consumers break without warning.
- **Fix:** iterate `wires_` in sorted `packPos` order, or assign from canonical key.

### [LOW] Two different `litholattice::TickContext` structs — latent ODR violation
- **File:** `engine/src/sim/include/litholattice/sim/tick_interfaces.h:17` vs `engine/src/sim/include/litholattice/sim/det_sched.h:91`
- **Impact:** same namespace+name, different layouts. No TU includes both today — compiles; first TU that does gets a hard error or silent confusion.
- **Fix:** rename one (`SchedTickContext` / `BlockTickContext`).

### [LOW] `TickArena::allocate` power-of-two assert is debug-only
- **File:** `engine/src/core/tick_arena.cpp:74`
- **Impact:** release builds with non-pow2 `align` silently misalign (`(raw + (align-1)) & ~(align-1)` wrong for non-pow2). All current callers safe — latent.
- **Fix:** round up or assert+fallback; `static_assert` at template call sites.

## 3. Net — 4 MEDIUM, 5 LOW

### [MEDIUM] Unbounded `ready_` queue in ReliableChannel — memory exhaustion via in-order reliable flood
- **File:** `engine/src/net/channels.cpp:162,185`; `engine/src/net/include/litholattice/net/channels.h:244`
- **Impact:** `receive()` pushes every in-order datagram into `ready_` with **no cap** (`reorder_` capped at 256, `inflight_` at 64). Attacker with raw socket sends valid reliable datagrams (version byte 1–2, channel bit, sequential seq, ≤1200 B payload) → ~12 MB/s growth per connection at 10k datagrams/s; nothing sheds load. Reachable pre-handshake (accepted version set `{1,2}` during negotiation, no source binding).
- **Fix:** cap `ready_` like `reorder_` (`maxReady` in `ReliableConfig`, shed-oldest or drop-incoming with counter); document the drop policy.

### [MEDIUM] Spoofed ack/heartbeat silently evicts in-flight reliable packets
- **File:** `engine/src/net/channels.cpp:108` (`onAck`); `:150-175`
- **Impact:** ack fields ride plaintext in the 14-byte header. Attacker observing one datagram learns seq N, sends spoofed ack (`seq=0`, `ack=N`, `ackBits=0xFFFF`) → `onAck` evicts the entire in-flight window. Sender stops retransmitting; actually-lost datagrams silently dropped on the "ordered, acked" channel. Eviction math is window-bounded (RFC-1982 correct), but eviction *within* the window is attacker-controlled.
- **Fix:** no cheap in-class fix — inherent to unauthenticated plaintext. Real fix: builder's authenticated KEX + game binding channels to peer address/port at the socket layer. Until then, the reliable guarantee holds only against non-adversarial loss.

### [MEDIUM] Handshake killed by a 3-byte spoofed Reject — targeted connection-prevention DoS
- **File:** `engine/src/net/handshake.cpp:131-137`
- **Impact:** client in `HelloSent` accepts any well-formed `Reject` (`[3, reason≠0, maxProto]`) → `Rejected`. Attacker spoofing the server prevents handshake completion, one tiny datagram per attempt.
- **Fix:** authenticate the handshake (KEX/signatures); bind to peer address at socket layer. Partial: ignore handshake payloads from unexpected endpoints.

### [MEDIUM] Version negotiation unauthenticated — on-path attacker forces downgrade or refusal
- **File:** `engine/src/net/handshake.cpp:14-24`, `:111-160`
- **Impact:** ClientHello offers `{1,2}` in plaintext; on-path attacker rewrites to `{1}` → server agrees on v1 → `CodecKind::V1Baseline`, a codec that **does not exist in the tree**; fail-closed burden entirely on unseen game code. Downgrade protection needs handshake integrity.
- **Fix:** sign the transcript once KEX lands (or MAC the hello under the session key); document games must narrow to `{2}` until v1 has a real codec.

### [LOW] Client handshake deadline defeated by junk packets (liveness)
- **File:** `engine/src/net/handshake.cpp:111` — `lastActivityMs_ = nowMs` runs on *any* ≥1-byte payload before msg-id dispatch. Spoofed 1-byte drips keep the client in `HelloSent` forever; `pollTimeout()` never fires.
- **Fix:** refresh liveness only after the payload parses as a recognized handshake message.

### [LOW] `needFull` abuse forces full snapshots — bounded bandwidth/CPU inflation
- **File:** `engine/src/net/snapshot.cpp` (`requestFull` ← `kFlagNeedFull`, `protocol.h:73`) — malicious client sets the bit on every ack → full instead of delta snapshots at 32 Hz. ~3–4× bandwidth + encode cost per malicious client (bounded, not amplification).
- **Fix:** rate-limit forced fulls (one per N snapshots, or require ack advance).

### [LOW] Signed-overflow UB on attacker-controlled snapshot deltas
- **File:** `engine/src/net/protocol.cpp:276-278`, `:288` — wire varints (full int32 range) added to int32 baselines. Malicious server/MitM → UB (practically wraps to garbage positions).
- **Fix:** accumulate in `int64_t`, saturate/clamp to int32.

### [LOW] Confinement denial logging is attacker-amplifiable (log spam → disk fill)
- **File:** `engine/src/net/confinement.cpp:47,52,67,117` — every denied/throttled mod send logs; a buggy/malicious mod with `kPermNet` fills the log volume.
- **Fix:** rate-limit log lines (once per window per mod, count the rest).

### [LOW] Dead config surface in `version.h`
- **File:** `engine/src/net/include/litholattice/net/version.h:31-44` — `NetConfig`, `NetPreset`, `LITHOLATTICE_NET_TICK_HZ_DEFAULT`, `LITHOLATTICE_NET_SNAPSHOT_HZ` referenced nowhere. Live knobs are `SnapshotConfig`/`ReliableConfig`.
- **Fix:** wire up or delete.

## 4. Script / mod / console / MCP — 2 HIGH, 4 MEDIUM, 5 LOW

### [HIGH] AngelScript `consoleCmd` runs at ambient authority, not the mod's — documented trust model unimplemented
- **File:** `engine/src/console/script_console.cpp:18` (`scriptConsoleCmd` → `executeLine(line)`, no permission scoping)
- **Impact:** `concmd.h` (SCRIPT TRUST MODEL) promises mod-issued lines run with the mod's granted permissions. The implementation installs no mask — inherits ambient authority, and the game sets `setCheatsAllowed(true)` (`game.cpp:54`). Latent today (only first-party `lithokraft` bound, `game.cpp:206`); goes live when the builder wires per-mod ScriptEngines — then any third-party mod with `kPermConsole` runs cheat-gated commands (`give`, `kill`, cheat cvars) with no additional grant. Accidental discovery likely (mod author tests `consoleCmd("give ...")`, finds it works, ships it).
- **Fix:** install the owning mod's mask via `setCallerPermissions()` for the call duration in `scriptConsoleCmd`/`scriptConsoleCvar`/`scriptConsoleSetCvar` (mirror the `IConsoleExec` adapter contract); store the mask in `ConsoleScriptContext` at bind time. Also save/restore `callerPermissions_` around re-entrant `executeLine` calls (currently only `tick()` does).

### [HIGH] Keybind permission laundering: `bind` plants cheat commands that fire under the player's authority
- **File:** `engine/src/console/concmd.cpp:355` (`setKeyBind` stores raw string, no authority captured) + `:570` (`dispatchKeyBinds` → `executeLine(cmd)` under ambient authority)
- **Impact:** `bind` is a non-cheat builtin. An MCP agent with only `kPermConsole` (cheats revoked) runs `bind F5 "give diamond_sword"` — the cheat check fires at *dispatch* under the player's full authority, not the binder's. `setKeyBind` silently overwrites existing binds (no prompt, no log). The `setCallerPermissions` enforcement is bypassed across time via the persistent bind table. Same laundering via semi-trusted Fragment/ReadOR scripts.
- **Fix:** stamp the binder's `callerPermissions_` into each bind entry at `setKeyBind`; re-install the stored mask in `dispatchKeyBinds` (fail-closed: restricted-planted binds can never gain cheat authority). Log bind creation/overwrite.

### [MEDIUM] Memory-quota exceed suspends the mod but doesn't abort the in-flight script
- **File:** `engine/src/script/script_engine.cpp:36` (`allocShim` → `suspend(...)`, then returns normally and the allocation succeeds)
- **Impact:** CPU overruns abort via the line-callback deadline, but a memory-quota trip only sets `kSuspended` — the running `ctx->Execute()` continues (up to the 500 ms hook budget), and every further allocation succeeds. A `string +=` loop can grab tens of MB in 500 ms — the quota is enforced only against *future* hooks.
- **Fix:** abort the running context on quota trip (plumb `asIScriptContext*` into the suspend path, or flag the `lineCallback` every 256 lines).

### [MEDIUM] Malformed `mod.json` crashes the engine at startup via uncaught `std::stoul`
- **File:** `engine/src/mods/mod_loader.cpp:32` (`parseSemver`), `:61` (`parseSdk`), `engine/src/mods/extension_loader.cpp:37` (`parseSemver`)
- **Impact:** `std::stoul(tok)` throws on huge/non-numeric strings; nothing catches. `"sdk": "99999999999999999999"` propagates out of `parseModJson` → `scan` → `load` → process crash. A single bad download DoSes every launch until the folder is manually removed. Accidental or malicious; trivially triggerable.
- **Fix:** try/catch (or pre-check `tok.size() > 19`); treat as bad sdk/version (fail-closed, mod refused).

### [MEDIUM] Console `query` narrows int64 bounds to `int` without a range check (MCP twin has the check)
- **File:** `engine/src/console/concmd.cpp:1075-1080`
- **Impact:** `parseInt64` accepts up to `INT64_MAX`, then `static_cast<int>` silently wraps; the `kMaxQueryCells` guard is computed on wrapped values (signed-overflow UB), and the backend gets garbage coordinates. The MCP `world_query_blocks` tool correctly rejects out-of-int32 coords — the console path was missed.
- **Fix:** mirror the MCP check — reject bounds outside `[INT32_MIN, INT32_MAX]` before narrowing.

### [MEDIUM] `exec` file open has a TOCTOU symlink race; `loadArchived` has no sandbox at all
- **File:** `engine/src/console/concmd.cpp:624` (`executeFile`); `:742` (`loadArchived`)
- **Impact:** `executeFile` canonicalizes and verifies the path under the config dir, then opens with `std::ifstream(full)` — a symlink swap between check and open escapes the sandbox (needs config-dir write; LOW-MEDIUM). `loadArchived` takes an arbitrary path with zero validation and executes its lines — and `mod_interface.h:48` claims "exec/saveArchived stay sandboxed," which is false for both `saveArchived` (`concmd.cpp:714`, arbitrary write path) and `loadArchived`. No production callers of `loadArchived` — latent.
- **Fix:** open with `O_NOFOLLOW` + re-validate via `/proc/self/fd`; confine `loadArchived`/`saveArchived` to the config dir, or document as engine-only APIs.

### [LOW] `kPermPilotSenses` missing from the mod permission manifest schema
- **File:** `engine/src/mods/include/litholattice/mods/mod_permissions.h` — bit exists in `mod_interface.h` and `permBitName()`, but `kNameBits`/`kAllPermissionBits` stop at `kPermPlayerControl`. A mod can never declare `pilot_senses`; `resolve()` masks it out. Schema drift.
- **Fix:** add `{"pilot_senses", kPermPilotSenses}` to `kNameBits` and the bit to `kAllPermissionBits`.

### [LOW] Handshake-era HTTP sessions never expire; `eraseSession` is dead code
- **File:** `engine/src/mcp/http_server.cpp:295` (zero callers); `createSession`
- **Impact:** every `initialize` mints a session living until server stop; no DELETE, timeout, or cap. Authenticated client grows the session table unbounded. Localhost-only + bearer auth → LOW.
- **Fix:** cap sessions (LRU eviction) and/or idle-timeout; wire `eraseSession` to session-close.

### [LOW] stdio transport reads the whole line before enforcing the 16 MiB cap
- **File:** `engine/src/mcp/server.cpp` (`runStdio` → `FdStdioTransport::readLine` → unbounded `std::getline`) — the `kMaxStdioLineBytes` check fires *after* the full line is in memory. One giant line OOMs first.
- **Fix:** chunked read with an early cap.

### [LOW] `bumpCVar` `std::llround` UB on unclamped `++` near INT64_MAX
- **File:** `engine/src/console/concmd.cpp:544` — `cvar++` path calls `bumpCVar(..., clamp=false)`; `nv` can be inf/out-of-`long` range → `std::llround` UB.
- **Fix:** saturate — clamp to the bound instead of `llround` when out of range.

### [LOW] Mod permission revocation doesn't unbind script functions
- **File:** `engine/src/script/include/litholattice/script/script_engine.h` — `bindGlobalFunction` has no unregister; `setModPermissions` checked at bind time only. Later grant reductions leave bindings live for the process lifetime.
- **Fix:** `unbindAll()`/`syncBindings(mask)` on mask changes.

## 5. Audio / file / storage — 4 HIGH, 6 MEDIUM, 8 LOW

### [HIGH] AnvilRegion: inline sector count overflows the 7-bit count field — payloads ≥ 65,015 bytes become unreadable immediately after `put()`
- **File:** `engine/src/storage/anvil_region.cpp:320` (need computation), `:403` (`le[3] = s.sectors`), `:409` (`counts_[sl] = s.sectors`), `:521`; cap at `anvil_region.h:29` (`kMaxInlineBytes = 255 * kSector - 10`)
- **Impact:** the count byte steals bit `0x80` for the external-sidecar flag → only counts 1–127 valid inline. But `stage()` allows up to 130,550 bytes → `need` up to 255. Payloads 65,015–130,550 bytes get count byte `0x80`–`0xFF`; `read()` misclassifies as external → "external sidecar missing" → chunk data lost. Worse, `rebuildIndex()`/`rebuildBitmap()` (`:185-194`) skip `kExternalBit` entries → the record's sectors are marked *free* and reallocated — silent corruption on top of loss. `put()` returns success; failure surfaces only on read.
- **Fix:** one line — `kMaxInlineBytes = 127 * kSector - kRecordOverhead` (= 65,014), or clamp in `stage()`: `if (need > 127)` → sidecar path. Correct the `// 1..255` comment at `:320`.

### [HIGH] OcclusionThread: game-thread `addQuery` races the occlusion thread's vector iteration (heap-use-after-free)
- **File:** `engine/src/audio/occlusion_thread.cpp:49` (`queries_.push_back`) vs `:87` (`for (auto& q : queries_)` in `threadMain`)
- **Impact:** the header says the game registers (listener, sound) pairs *each frame* while the thread runs at 32 Hz. `push_back` during iteration = data race; reallocation dangles the thread's iterators → crash/UB. Separately, the position fields are plain floats written after the `active` CAS and read with no release/acquire pairing — torn/stale positions.
- **Fix:** mutex-guard structural mutation; `threadMain` snapshots raw pointers under the lock, then raycasts unlocked; make position fields `std::atomic<float>` (or release/acquire around the CAS).

### [HIGH] AudioStreamer: `play()` on the game thread races `render()` on the audio thread
- **File:** `engine/src/audio/streaming.cpp:24-25` vs `:59`, `:85-86`
- **Impact:** the header blesses `play()` as "replaces any previous source" (mid-playback), but `pending_` (a `std::vector`), `chunkIdx_`, `frameOff_` are plain members mutated on both threads, no sync. Vector clear-during-iteration → heap corruption/crash on the audio thread.
- **Fix:** audio-thread-only mutation — `play()` sets `atomic<bool> resetRequested_`; `render()` performs the reset at the top before draining. Or a generation counter checked in `render()`.

### [HIGH] `writeFileAtomic`: crash between backup rotation and commit leaves the target *absent* — the "old or new, never torn" promise is false with backups enabled
- **File:** `engine/src/storage/file_contract.cpp:114-130` (rotation `:114`, `rename(path → .bak.1)` `:118`, commit `rename(tmp → path)` `:130`)
- **Impact:** crash/power loss between `:118` and `:130` → target missing, old at `.bak.1`, new in `.tmp`. Nothing recovers: `RegionFile::open` (`storage.cpp:145-147`) treats missing file as "new region" → chunks silently dropped; `RecordStore::load` (`defrag.cpp`) just fails. The header claims "a crash leaves either the old or the new version, never a torn one" — with `keepBackups > 0` the crash leaves *neither* at the target. Two companion doc falsehoods: "on ANY failure returns false + err with the target untouched" (false if `:130` fails after `:118`), "rotation happens only on a successful write" (rotation runs *before* commit).
- **Fix:** `storage::recoverCrashedWrite(path)` — called from `RegionFile::open` and `RecordStore::load` when the target is absent: if `<path>.bak.1` exists, rename it back (loses at most the uncommitted write — always a known-good image); else if `<path>.tmp` validates, promote it. Best-effort restore on commit-rename failure. Correct the three doc claims.

### [MEDIUM] Defrag: cancel/fail during Reindex leaves the TextIndexer partially fed; mutation during Reindex feeds stale bytes
- **File:** `engine/src/defrag/defrag.cpp:312` (`cancel()` → `resetSession()`), `:433-447` (`pumpReindex`), `:398` (`pumpCommit`)
- **Impact:** `reindexBegin()` (`:440`) calls `indexer_->clear()`, then re-feeds 256/tick. `cancel()` mid-reindex just idles — index stays cleared-but-partial, no repair; searches silently miss documents. The generation guard (`:402`) covers only Scan/Copy/Commit: a store mutation *during* Reindex is never re-checked → stale snapshot bytes indexed while store and file hold new bytes → permanent index/store divergence. The selftest only checks cancel idles the session, not index consistency.
- **Fix:** on cancel/fail at-or-after `reindexBegun_`, run reindex to completion synchronously or record "index dirty" and force full reindex on next `beginDefrag`; re-check `generation()` in `pumpReindex`, restart on mismatch (bounded by `kMaxRestarts`).

### [MEDIUM] AudioEngine tap ring: `tapTail_` lost update between the audio thread's overflow trim and the MCP thread's `drainTap`
- **File:** `engine/src/audio/audio_engine.cpp:70-72` vs `:86-90`
- **Impact:** two threads plain-store the same atomic, no ordering — updates lost: reader's drain progress rewound (duplicate tap frames) or writer's trim clobbered (stale frames served). Genuine lock-free bug; impact limited to the agent-audio tap (duplicate/stale audio for the MCP pilot), not a crash.
- **Fix:** remove the writer-side trim — overflow handling only in `drainTap` (advance `tail` to `head - capacity` before draining). Single writer for `tapTail_`.

### [MEDIUM] Degradation shed is one-directional: engine-virtualized voices never auto-recover
- **File:** `engine/src/audio/audio_engine.cpp:28-47` (`applyShedActions`); `engine/src/audio/synth.cpp:40`
- **Impact:** at shed level ≥ 2 the engine virtualizes the upper half of voices; the header documents "the engine only ever ADDS virtualization here; the game thread clears it" — but no game-side code sends `Virtualize` commands, and `setRateDivider` *does* auto-recover while voice virtualization doesn't. After one CPU spike, upper-half voices stay silent indefinitely even at shed level 0 — the game can't distinguish "virtualized because inaudible" from "virtualized because shed."
- **Fix:** track engine-virtualized voices in a bitmask; clear exactly those when the shedder steps down; leave game-virtualized flags untouched.

### [MEDIUM] Codec stub boundary: `DecodeFn` has no error channel, so a stub decoder wired into `AudioStreamer` fails as silent EOS
- **File:** `engine/src/audio/include/litholattice/audio/streaming.h:30`; `engine/src/audio/include/litholattice/audio/codecs.h`
- **Impact:** `implemented()` correctly returns false for FLAC/Opus/Vorbis and `decode()` sets `err`, but the streaming contract can't carry the error — a caller skipping the `implemented()` check gets instant end-of-stream: silence, no diagnostic. Only the selftest consumes codecs today — latent trap for the builder wiring music playback.
- **Fix:** `adaptDecoderToDecodeFn()` helper checking `implemented()` up front and surfacing the stub error via log/callback — or extend `DecodeFn` with an error-out parameter.

### [MEDIUM] IoScheduler strand identity is the raw path string — same file via different spellings gets concurrent writers sharing one `.tmp`
- **File:** `engine/src/storage/async_io.cpp:96` (`strands_[path]`); `file_contract.cpp` (fixed `<path>.tmp`)
- **Impact:** `"world/r.0.0.llr"` vs `"./world/r.0.0.llr"` hash to different strands → two `writeFileAtomic`s run concurrently, both truncating/writing the *same* `<path>.tmp` with independent stdio buffers → interleaved "frankenfile" then atomically committed. CRC-framed readers catch it on load; raw readers get silent corruption. `WorldStore` doesn't use `IoScheduler` (own serial queue) — nothing serializes a scheduler writer against `WorldStore::flush` to the same path.
- **Fix:** `lexically_normal()` (ideally absolute) the path in `submitStrand`; document that all writers to a path go through one scheduler with canonical spelling.

### [MEDIUM] No NaN/Inf scrubbing on the audio path; float→int UB on untrusted `NoteSpec` values
- **File:** `engine/src/audio/include/litholattice/audio/simd_mix.h`; `engine/src/audio/voice.cpp:56`, `:75`
- **Impact:** float WAV samples pass NaN/Inf through `WavDecoder::decode` into the mix; the final clamp deliberately doesn't remove NaN → NaN reaches the device and the MCP tap. `Voice::noteOn` uses `note.frequency`/`note.hold` unclamped: NaN/negative/huge frequency = UB float→`uint32_t` on first `advancePhase`; huge frequency makes the noise-clock loop iterate ~`freq/48000*16` times — effectively hanging the audio thread. Game-controlled inputs today; one malformed `NoteSpec` away from UB.
- **Fix:** scrub non-finite samples to 0 in the final output stage (one 512-frame pass); clamp `frequency` to `[1, sampleRate*0.45]`, `hold` to a sane max in `Voice::noteOn`.

### [LOW] `RegionFile::put`/`read` public with no slot bounds validation
- **File:** `engine/src/storage/include/litholattice/storage/storage.h:107`; `storage.cpp` `read()`/`flush()`
- **Impact:** out-of-range `lx`/`lz` → OOB vector access in `flush()` (`crcs_[size_t(s)]` write = heap corruption). Current callers safe (`floorDiv` → `[0,32)`); public API unguarded.
- **Fix:** assert/early-return on `lx`/`lz` range in `put`/`read`.

### [LOW] `Quarantine::quarantine` logs on every call despite the "logged once" contract
- **File:** `engine/src/storage/file_contract.cpp:186-189` — no dedup; a corrupt file re-read per frame spams the log unboundedly.
- **Fix:** `unordered_set` of already-logged paths.

### [LOW] `SandboxedFile::operator=` invokes the destructor explicitly on `*this`
- **File:** `engine/src/storage/sandboxed_io.cpp:48` — formally UB (works in practice). Fragile idiom in security-critical code.
- **Fix:** `close()` helper + memberwise move.

### [LOW] `SandboxedFile::readAll` doesn't retry on `EINTR`
- **File:** `engine/src/storage/sandboxed_io.cpp` — one-line robustness fix.

### [LOW] Unbounded whole-file reads — OOM on hostile large files
- **File:** `engine/src/storage/file_contract.cpp` (`readFileAll`), `engine/src/defrag/defrag.cpp` (`RecordStore::load` slurps *before* the `pageCount > 1M` check), `engine/src/storage/sandboxed_io.cpp` (`readAll`)
- **Impact:** `readSandboxedFile` is the mod path — a multi-GB mod asset fully loaded → OOM crash. No size cap on the read side.
- **Fix:** `fstat`/size pre-check with configurable cap (fail closed), or bounded streaming reader.

### [LOW] `IoFence::wait()` relies on `assert` for the no-wait-on-job-thread rule
- **File:** `engine/src/storage/async_io.cpp:161` — NDEBUG removes the check → violating caller deadlocks the pool silently.
- **Fix:** runtime check (log + return) or documented hard requirement.

### [LOW] `OcclusionThread::setOcclusionTest` is a data race if called after `start()`
- **File:** `engine/src/audio/occlusion_thread.cpp:~75` — assigning `std::function` while `threadMain` reads it is UB. Honored by convention; fragile.
- **Fix:** require before `start()` (assert `!running_`) or atomic-shared.

### [LOW] Dropped `AudioCommand`s when the 256-deep queue fills — a dropped `NoteOff` is a stuck note
- **File:** `engine/src/audio/include/litholattice/audio/audio_engine.h` — under event bursts the game silently loses commands; losing `NoteOff` leaves a voice droning until stolen.
- **Fix:** grow the queue and/or prioritize `NoteOff`/`SetMasterGain` on overflow.

## 6. Game / LithoKraft — 1 HIGH, 3 MEDIUM, 3 LOW

### [HIGH] Console `sim.tickrate 0` crashes the process (SIGFPE on the sim thread)
- **File:** `game/LithoKraft/src/game/settings_cvars.cpp:66` (unvalidated setter), `game.cpp:528` (applies raw), `engine/src/sim/sim_thread.cpp:184`
- **Impact:** `sim.tickrate` is a **non-cheat** cvar — any player with the `~` console types `sim.tickrate 0` → `s.tps = toInt(v)` → `sim_->setTickRate(0)` → `worldDiv = 512/0` → SIGFPE, process dies. Negative values silently wedge the world tick; non-powers-of-two silently mistune the rate (violates the pow2 invariant). UI dropdown and F7 are safe — only the cvar path bypasses validation. (Bad value persists to settings.json; `load()` heals via `nearestTps` next boot.)
- **Fix:** clamp in the cvar setter (`s.tps = nearestTps(toInt(v))`) **and** guard `SimThread::setTickRate` (reject `tps <= 0` — the engine divides by it).

### [MEDIUM] Dimension switch while paused restarts the sim unpaused — world ticks behind the pause menu
- **File:** `game/LithoKraft/src/game/game.cpp:354` (`switchDimension`), `:349` (`sim_->start()`)
- **Impact:** F6 (`SwitchDim`) is live in the Debug context even with the pause dialog open. `switchDimension` → `loadActive()` builds a fresh `SimThread` with `paused_ = false` (`sim_thread.h:152`); nothing re-applies the game's `paused_`. Pause menu displayed, sim ticking — the user thinks the world is frozen; it isn't.
- **Fix:** after `loadActive()`, `sim_->setPaused(paused_ && cfg_.pauseSim && !benchSet_)`.

### [MEDIUM] Menu cinematic dolly permanently discards the restored player position
- **File:** `game/LithoKraft/src/game/game.cpp:504` (`applyMenuCamera`), `:496` (`enterPlay`)
- **Impact:** the ctor restores `player_.pos/yaw/pitch` from `player.dat`, then `main.cpp` calls `enterMenu()` before the first frame. Every menu frame, `applyMenuCamera()` overwrites `player_.pos` with the dolly path; `enterPlay()` never restores. Save/restore is 100% defeated whenever the launch menu is used (always except `--bench`): on Play you start wherever the dolly happened to be.
- **Fix:** snapshot pos/yaw/pitch in `enterMenu()`, restore in `enterPlay()`.

### [MEDIUM] Console `pause` command in the menu breaks the menu⇒paused invariant
- **File:** `game/LithoKraft/src/game/game.cpp:91` (`b.pauseToggle`), `:459` (`setPaused`)
- **Impact:** the `~` console is reachable in the menu. Typing `pause` calls `setPaused(false)`: pops the opaque `Inventory` context, sets `captured_ = true` — menu overlay still up, but Gameplay actions live (WASD/mouse move the player behind the menu), sim ticking, and `captured_ = true` makes `routeUiInput` drop all pointer events to the menu → Play/Settings/Exit buttons mouse-dead until keyboard-activate. `onFocusChanged` guards on `menuState_ == Playing`; the console path doesn't.
- **Fix:** no-op `b.pauseToggle` (and/or `setPaused`) when `menuState_ == MenuState::Menu`.

### [LOW] `save` console command reports success when nothing was saved
- **File:** `game/LithoKraft/src/game/game.cpp:87` — `b.save` prints `"save: world saved"` unconditionally, but `saveWorld` early-returns when `save_` is null (`--no-save`, `--bench`, headless). Misleading.
- **Fix:** `saveWorld` returns `bool`; print `"save: no save directory"` when false.

### [LOW] `config.lth` archived-cvar persistence is write-only — the archive feature is dead
- **File:** `game/LithoKraft/src/game/game.cpp:275` (`~Game` → `saveArchived`) — nothing ever executes `config.lth` (ctor runs only `autoexec.lth`; zero references in `engine/src/console`). `settings.json` is the real persistence.
- **Fix:** execute `config.lth` at boot (before cvar defaults) or drop `saveArchived` and the "archived" claims.

### [LOW] `hud.scale` cvar truncates floats into an int field with no range check
- **File:** `game/LithoKraft/src/game/settings_cvars.cpp:56` — declared `CVarType::Float`, setter truncates (`hud.scale 1.9` → `1`); no range validation (`hud.scale 99` persists until `load()` clamps next boot).
- **Fix:** declare `CVarType::Int`, clamp 0–2 in the setter.

---

## Appendix A — Optimizations (perf left on the table)

### Sim / core (hot tick paths)
1. `runRandomTicks` does (2r+1)² `unordered_map` lookups per tick (`engine/src/sim/random_ticks.cpp:20-22`): radius 8 → 289 hash lookups × 512 TPS ≈ 148k/s. Cache the ticking column pointer list per area change.
2. `MeshInput` (~24 KB) `make_shared` per dirty sub-chunk per remesh pass (`sim_thread.cpp` → `flushDirty` → `snapshotForMeshing`, `mesh_input.cpp:8`), up to ~120 passes/s under signal churn. Pool/recycle `MeshInput` objects.
3. `SignalSim::compSleep_ = false` (`signal.h:127`) → component/torch scans walk the entire unordered containers every tick at up to 512 TPS. The quiescent-skip machinery exists — consider defaulting it on, and/or dense vectors for cache-friendly scans.
4. `SimThread::run` formats the look-at inspector string every loop iteration (`sim_thread.cpp:256-260`), even zero-tick iterations. Gate on `hit_` change or actual consumption.
5. `HandoffQueue::drainInto` allocates a `std::vector` per call (`threading.h:125-129`); `publishEvents` calls it every loop iteration. Reuse a member scratch vector.
6. `JobSystem::submit` heap-allocates a `std::function` per job (plus a second wrap in `JobFence::submit`) on every stage of every scheduler tick. Small-buffer-optimized or intrusive job type removes per-tick churn.
7. Steal-victim PRNG: all workers seed identically (`job_system.h`: `rngState_.assign(workers, 0x9E3779B9u)`) → thundering herd on first contention. Seed per worker (splitmix of seed + index).

### Render
1. PBR software pixel loop (`pbr_software_shaders.h:117-164`): `irradianceFromLightLevel` + emissive `std::function` construction per pixel — hoist both to per-quad.
2. `renderMesh` (`software_renderer.cpp:208-211`): `std::vector<float> screen(vertCount*3)` allocates per mesh per frame — reused member sized to max mesh.
3. `applyFXAA` (`software_renderer.cpp:261-280`): full-framebuffer copy + 9-tap box per pixel, mislabeled (it's a blur, not FXAA). Two-row in-place sliding window removes the copy.
4. GL `renderFrame` (`gl33_renderer.cpp:400-406`): analytic-light stack arrays + 3 `glUniform*v` calls even when `nl == 0` — skip when the game supplies no lights (the common case).

### Net (hot path = `SnapshotClient::build` at 32 Hz × clients, plus per-datagram receive)
1. Double codec pass per snapshot: `snapshot.cpp` `build()` runs `encodeSnapshot` then `decodeSnapshotBody` on its own output to reconstruct the post-image baseline (~2× codec CPU per snapshot per client). Record packed record byte-ranges during encode and splice the post-image without re-parsing.
2. Per-client sort+normalize: `build()` copies the full entity vector and sorts per client per snapshot (`O(E log E)` × clients at 32 Hz). Share one normalized/sorted copy across clients for the same tick.
3. Double header decode: `ChannelDemux::receive()` parses the header to route, then the channel's `receive()` parses it again. Thread the parsed `PacketHeader` through.
4. `ReliableChannel::poll()` copies before restamp (`channels.cpp:~84`): `d = p.datagram` then `restampAcks(d)` — restamp in place and move out, saving ~1.2 KB per retransmit.
5. `decodeSnapshotBody` builds `result.entities` by carry-over push_backs, then merges into a second vector and moves — a single three-way merge would do.

### Script / mod / console / MCP
1. `McpServer::dispatchStateless` (`server.cpp`): serializes the response, then `parseJson(*resp)` re-parses it just to merge the stateless envelope. Build the envelope into the `JsonValue` *before* serializing — saves a full parse+reserialize per stateless request (the hot path).
2. `handleRequest` step 5b + `handleMessage` + `dispatchStateless`: the request body is JSON-parsed three times (era classification, `parseRpcRequest`, envelope merge). Parse once, pass the DOM down.
3. `ConsoleRegistry::dispatchKeyBinds` (`concmd.cpp:570`): `keyFromName(keyName)` per bind every frame. Parse once at `setKeyBind`, store the key alongside the command string.
4. `onToolsCall`: linear scan over `tools_` per call — fine at ~20; index by name if the tool count grows.

### Audio / storage
1. Backup-rotation I/O storm on region flush: `RegionFile::flush` → `writeFileAtomic` (default `keepBackups=8`): every dirty region on every save does ~7 chain renames + 1 target rename + file fsync + **directory fsync** (`file_contract.cpp:114-135`). ~9N renames + 2N fsyncs per save for N regions; the per-file dir fsync is the expensive part. Hoist `fsyncParentDir` into a batch "flush directory once after all regions" API; reconsider per-region-file rotation vs per-save-set.
2. Underrun zero-fill scalar loop (`streaming.cpp:80-83`, audio thread) → `std::memset`.
3. Defrag scan allocates per page per tick: `forEachLiveInPage` takes `std::function` (`defrag.cpp` `pumpScan`); each live record gets its own vector in `live_` — 64+ heap allocs/tick on the tick thread during scan. Templated callback instead of `std::function`.
4. Streamer worker allocates per chunk (`streaming.cpp` `workerMain`: `c.pcm.resize(chunkFrames_ * 2)` per chunk). Reuse a thread-local scratch buffer.
5. Half-rate shed path scalar (`synth.cpp` `mixInto`) — trivial at 512 frames, but it runs exactly when CPU is scarce; SIMD pass if level 3 is ever hit.

### Game
1. Per-frame heap allocation + full-cache walk in `frameParams()` (`game.cpp` ~`frameParams`, `LightCollector::collect` at `pbr_lights.h:68`): fresh `std::vector<AnalyticLight>` every frame + full emissive-cache walk through a `std::function` sink per candidate. In lamp-dense scenes, thousands of indirect calls/frame on the render hot path. Hoist the vector to a reused member; skip re-collection when camera column + provider dirty flag unchanged.
2. `uploadEntities()` scans unconditionally (`game.cpp`): 64-block `querySphere` + billboard build every frame even with zero item entities. Early-out on empty entity set.

---

## Appendix B — Dead code

| File | What | Note |
|---|---|---|
| `engine/src/render/greedy_mesher.cpp` | Entire file: `greedyMeshSubChunk` stub (never appends to `out`), `ChunkVisibility::compute` | Zero callers |
| `engine/src/render/axis_culling.cpp` | Entire file: `splitByAxis`/`visibleFaces` | Zero callers (renderers use `aabbInFrustum` + GPU culling) |
| `engine/src/render/software_opt.cpp` | Entire file: `rasterizeTriangleFast`/`rasterizeTiled` | Zero callers (`rasterizeTiled` is a TODO no-op) |
| `engine/src/render/include/.../greedy_mesher.h` | `UploadBudget`, `ChunkLOD`, `lodForDistance` | Zero users (game rolled its own `kUploadBudget`) |
| `engine/src/render/include/.../pbr_transparency_tiers.h` | `pbrTierIsStubbed()` | Always-false, no callers |
| `engine/src/mcp/http_server.cpp:295` | `McpHttpServer::eraseSession` | Zero callers (compounds the session-exhaustion finding) |
| `engine/src/console/concmd.cpp:742` | `ConsoleRegistry::loadArchived` | No production callers (only selftest); game never loads `config.lth` |
| `engine/src/net/include/litholattice/net/version.h:31-44` | `NetConfig`, `NetPreset`, `LITHOLATTICE_NET_TICK_HZ_DEFAULT`, `LITHOLATTICE_NET_SNAPSHOT_HZ` | Referenced nowhere |
| `engine/src/audio/codecs.cpp:73` | `WavDecoder::size_` | Assigned, never read |
| `engine/src/storage/anvil_region.cpp:525` | `AnvilRegion::locate()` | Zero callers repo-wide |
| `engine/src/sim/*` | `TickArena` (whole subsystem) | Zero production users — sound implementation, waiting for adoption or removal |
| `engine/src/sim/*` | `simFailFast` / `simFailFastFromException` | Zero production callers — death path exists, nothing invokes it |
| `engine/src/world/light_engine.cpp` | `addSource`/`setOpacity`/`propagate`/`removeSource` | Zero production callers (latent until wired) |
| `game/LithoKraft/src/headless/headless_harness.h:44-45` | `HeadlessGame::input()`, `HeadlessGame::audio()` | Zero callers |

---

## Appendix C — Verified clean (threat-model areas with no finding)

Reviewers checked these honestly, not just skimmed:
- **Sim:** Chase-Lev deque, JobSystem (no missed wakeups, clean shutdown), TripleBuffer/SpscQueue/HandoffQueue, det_sched determinism, `random_ticks` splitmix seeding, no wall-clock/`rand()` in sim tick paths, IEEE/SSE2 floats, bounded tick loops, TickArena implementation soundness.
- **Render:** Cook-Torrance core (no NaN paths), Snell/TIR CPU↔GLSL identical, 48-byte material rows match std430, ScreenDoor dither CPU↔GPU equivalent, binary mesher merge math + winding, emissive voxel decode, Vulkan RAII/fencing/staging/descriptor/push-constant/swapchain handling.
- **Net:** varint/codec strictness, framing exactness, handshake/KEX parser strictness, encryption-stub boundaries (NullCipher refuses, fail-closed, `isPlaintext()` loud gate), ReplayWindow, no hash-flood surface, bounded queues, snapshot transport-only scoping.
- **Script/mod/MCP:** MCP tool dispatch gating (every tool, central check), JSON depth caps + strict ArgReader, HTTP framing caps, lithoscript budget interlock (wait/alias/exec/step/total-wait verified terminating), IExtensible depth/cycle/rollback, quota attribution design, text indexer.
- **Audio/storage:** WAV/FLAC/Opus/Vorbis parser bounds, `unframeRecord` length check ordering, `writeFileAtomic` phase-1 (short-write detection, fclose error check, temp cleanup), SandboxedFile 3 gates, AnvilRegion commit protocol (timestamp-before-location, payload-before-index, torn-tail truncation), defrag scan/copy staleness handling, region quarantine, IoScheduler strand mechanics.
- **Game:** menu click-to-grab, Esc-vs-modals, focus-loss behavior, context push/pop balance, `saveWorld` null-guards + dtor order, mesh epoch guards, `give` with bogus ids, `settings.json` load clamping.

---

## Method notes
- Six parallel reviewers, each owning one subsystem; all read-only, no tree modifications.
- Severity is by *exploitability × blast radius*: CRITICAL = corrupts rendering/data or crashes for all users on a normal path; HIGH = crash/data-loss/security bypass reachable in practice; MEDIUM = real bug, narrower trigger; LOW = robustness/hygiene.
- Findings marked "latent" are in code that ships but has no production callers *yet* — they become live the moment the builder wires the owning system. They are not false positives.
- Game-side call sites the reviewers could not see (the game hasn't wired net/MCP adapters yet) are flagged as open questions, not findings.

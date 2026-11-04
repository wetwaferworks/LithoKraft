# Full-Tree Code Review — Consolidated Report (2026-10-12)

Three reviewers swept the entire tree read-only: engine core/sim (R1), generation/trust/presentation (R2), game layer (R3). Deduped and ranked below.

Two cross-reviewer corrections applied during consolidation: R2's "hot_swap/sealed flags never consumed" is **refuted** — R3 confirmed `main.cpp` consumes them in the boot seal decision (engine parses, game consumes; fine as designed). R1's "definitionEpoch() has zero callers in engine/src" is expected — it's a game-layer API, not a bug.

## Top 10 must-fix

**1. CRITICAL — `SimThread::run()` has no exception boundary** — `engine/src/sim/sim_thread.cpp:231`
An exception escaping a game tick hook propagates out of the `std::thread` callable → silent `std::terminate` with no corruption report logged, and a half-updated world may subsequently be saved. This is exactly the scenario the fail-fast doctrine was built for. Fix: wrap the per-tick step → `simFailFastFromException(e, "sim_thread", tick)`.

**2. CRITICAL — test private signing key committed in repo; nothing stops shipping the test baked key** — `engine/src/assets/assets_selftest.cpp:325`, `mod_signature.h`
The private seed matching the baked-in test public key is in the tree in plaintext. Anyone with repo access can mint "Verified" mods under it, git history retains the secret forever (so the test key can never be trusted in *any* shipped build), and no build flag or boot check refuses to ship it — the REPLACE-BEFORE-BETA marker is comment-only. Fix: fail-closed guard (refuse to boot/build-with-ship-flag when the baked key equals the test key); make the selftest generate an ephemeral keypair per run instead of hardcoding the seed.

**3. HIGH — `scanMods()` early-returns before the trust anchor AND the seal decision when `mods/` is missing** — `game/LithoKraft/src/app/main.cpp:106`
Both `setTrustedPublicKeyHex` and the entire seal decision sit below `if (!is_directory(mods)) return {};`. A stock install without a `mods/` dir gets no trust anchor (JD-signed structures can never verify `Trusted`) and **no seal at all** — the registry stays open, the exact case the spec says must fail closed. Silent: no log line on this path. Fix: hoist the trust-anchor call above the return; run the seal decision over the empty set (release → seals, per spec); log the no-mods case.

**4. HIGH — console `tick()` permission laundering across `wait`** — `engine/src/console/concmd.cpp`
On re-suspend, `queueSuspended()` stamps `callerPermissions_` after it was restored to the ambient mask instead of the resumption's `resume.callerMask`. A restricted agent script (`wait 1; wait 1; cheat_command`) resumes with full ambient authority — breaks the agent-permissions invariant. Fix: stamp `ns.callerMask = resume.callerMask` in the re-suspend path (one line).

**5. HIGH — `refreshStaged()` races concurrent readers; no mesh-invalidation contract** — `engine/src/world/include/litholattice/world/block.h`
It overwrites `blockDefs_[id]` in place and bumps a plain `uint64_t epoch_`. `runBetweenTicks` only excludes the sim's mutation window — the sim's own `flushDirty()` → `snapshotForMeshingInto()` and mesh-worker/UI threads read defs outside any lock (game.cpp:336/340/355 read `blockRegistry()` on pool threads). Torn `BlockDef` reads = UB. Additionally `definitionEpoch()`/`MeshInput::epoch` have no consumer: post-refresh snapshots stay "fresh" → stale meshes. Fix: `shared_mutex` around def storage + atomic epoch, and document/bump the mesh epoch on `refreshStaged()` before the live-ops path is ever exercised.

**6. HIGH — `findBlock()` alias recursion has no cycle guard** — `engine/src/world/include/litholattice/world/block.h`
A mod-crafted alias cycle → unbounded recursion → stack overflow. (Known queued H1, still present.) Fix: iterative lookup with a visited set; validate in `addBlockAlias`.

**7. HIGH — IK solvers have no NaN guards; NaN input poisons the whole skeleton** — `engine/src/anim/iksolve.cpp`
`solveTwoBone`/`solveCcd`/`solveFabrik`: NaN in target/pole/rootPos flows through NaN-unsafe comparisons into quaternion math → NaN model matrices for the entire skeleton. Inputs come from render-rate entity positions; one degenerate entity NaNs every skinned vertex. Fix: early-out to bind pose on non-finite input.

**8. HIGH — `computeSkinningMatrices` parent cycle = infinite recursion; no `validateRig`** — `engine/src/anim/iksolve.cpp:223`
`done[i]` is set only after the recursive parent call, so a parent cycle never terminates → stack overflow. Authored rig templates are a supported input and nothing validates them; caps are enforced only in `riggen.cpp`, `kMaxIkChains` has zero consumers. Fix: iterative traversal + real `validateRig()` (acyclic parents, cap enforcement, influence bounds).

**9. HIGH — net encode/decode integer UB, attacker-triggerable** — `engine/src/net/protocol.cpp`, `engine/src/net/quantize.cpp`
`encodeRecord` int32 overflow on `cur.x - base->x`; `getVarint` shift-overflow on the 5th byte. The 2026-10-11 audit hardened decode but missed encode; the varint path needs >32 bits. Fix: int64 deltas + saturate; accumulate varints in uint64_t then range-check.

**10. MEDIUM — JSON parser has no depth cap → crafted-input stack overflow** — `engine/src/core/json.cpp`
`Parser::val()` recurses per nesting level; a deeply nested mod JSON/config crashes the process (DoS). Fix: depth counter, cap ~128, fail closed.

## Medium findings

- **Permission/UI:** `ui_show` binding bypasses all mod-UI hardening — `game/LithoKraft/src/game/ui_script_bindings.cpp:73`: calls `ui::buildWidget(json)` directly with no depth pre-scan (recursive `build()` → stack overflow via one JSON string), skips widget budget, reserved-style checks, `ModPanel` confinement, MOD badge. Fix: route through the same pre-scan + budget + badging.
- **Unbounded config → UB/hang:** `diversity.cpp` `RockFormationFeature::place` — `max_radius`/`max_height` unclamped → `dx*dx` int overflow + `(2r+1)²` loop hangs the gen thread; `stages.cpp` `StructureFeature` — unbounded blueprint `dx/dz` → `reach_` explosion, `std::abs(INT_MIN)` UB, unbounded `spacing`. Fix: clamp in the loader.
- **Overview maps lie** — `engine/src/procgen/stages.cpp:1180`: `basesInRect` replays only biome gate + footprint, not the height/slope/water/surface/anchor gates `place()` enforces; header claims maps mark "exactly the structures chunk gen places". Fix: replay gates or weaken the doc.
- **Margin-replay disagreement at chunk borders** — `stages.cpp:1075`, `diversity.cpp`: new solver gates read clamped-column data for out-of-chunk bases → owning chunk and replaying neighbor can disagree → seams. Fix: document the caveat; long-term probe true columns by world position.
- **Resource-pack symlink escape** — `engine/src/assets/resource_pack.cpp:31-42`: `isSafePackPath` rejects `..`/absolute/`:` but `has()` uses `fs::is_regular_file` which follows symlinks → hostile pack reads arbitrary local files as textures/JSON. Fix: reject symlinks via `symlink_status`.
- **PNG decompression bomb** — `engine/src/assets/image.cpp`: 64 MB input cap doesn't bound decoded output → gigapixel PNG → multi-GB alloc → OOM. Fix: cap decoded w/h before pixel copy (SVG path already clamps at 2048).
- **Biome-transition in-place mutation** — `engine/src/procgen/diversity.cpp`: in-chunk neighbors read already-transitioned columns while out-of-chunk go through the un-transitioned probe → border asymmetry + cascade, contradicting the header. Fix: snapshot the biome array before the loop.
- **AI tick deadlock on throwing path job** — `engine/src/ai/ai_world.cpp:82-88`: `--left` never runs if a job throws (JobPool catches) → `cv.wait` hangs forever. Fix: decrement in the lambda regardless.
- **WalkCache thread-contract races** — `engine/src/ai/nav.h:37`: "thread-safe" claim is false — no snapshot exists, `ChunkMap::get()` requires the owner-thread contract; plus a stale-reinsert race. Fix: document the contract; generation counter to poison in-flight computes.
- **IoScheduler dtor UAF** — `engine/src/storage/include/litholattice/storage/async_io.h`: implicit dtor doesn't quiesce in-flight ops capturing `this`. Fix: drain in dtor or assert.
- **Truncated block-name table still renamed over the good one** — `engine/src/storage/block_names.cpp`: stream checked before close/flush; deferred write error (disk full) swallowed → `.tmp` renamed over good table. World-permanence adjacent. Fix: check after explicit close.
- **Anvil in-place shrink leaks sectors** — `engine/src/storage/anvil_region.cpp`: tail of shrunk rewrite stays marked used forever. Fix: `freeRun(oldSector + newCount, oldC - newCount)`.
- **Game manifest `sealed:true` ignored by seal decision** — `main.cpp:96-104` vs `121-159`: decision only iterates `set.ordered`; spec says "sealed:true in *any* manifest". Fix: fold the game manifest's flag in.
- **StructureRegistry never sealed** — `struct_registry.h:8-10`: header promises "sealed before worldgen, then read-only"; no code calls `seal()`. In-game SAVE can register structures at runtime in a "sealed" release. Fix: seal at boot alongside the content registry, or document the carve-out.
- **Hot-swap path is wired but inert** — `game.cpp:166-175`: `stageBlockUpdate` has zero callers anywhere; `hotswap_refresh` always prints "no staged updates". The release-open carve-out enables runtime mutation for nobody while carrying the item-5 race. Fix: build the staging producer with synchronization, or narrow the carve-out until it exists.
- **Throwing console command corrupts transient state** — `engine/src/console/concmd.cpp`: `execDepth_`/`currentCtx_`/`callerPermissions_`/`ticking_` not restored on throw. Fix: RAII-guard the invocation.
- **ScriptEngine::run() exception skips context restore** — `engine/src/script/script_engine.cpp`: C++ exception through `ctx->Execute()` → stale context pointer + leak. Fix: try/catch.
- **runBetweenTicks not exception-safe** — `sim_thread.cpp:55`: `fn()` throws → sim paused forever. RAII pause guard.
- **Script quota UAF on mod unload** — `engine/src/script/script_memory_hook.cpp`: dangling `owner` (ScriptEngine*) in quota headers; allocations escaping hook execution freed after engine destruction → `quotaCredit` writes freed memory. Fix: generation-counted handle/weak ref.

## Low findings (notable)

- `ScriptSignalNode` declared, never defined, no callers — first use = link error.
- `ITraceJit::execute` takes no tick context — the seam is currently decorative.
- Two `kMaxLight` constants, same namespace, different types — including both headers = hard compile error; latent landmine. Also stale "4-bit nibble" comment vs 1-64 scale.
- `software_renderer.cpp` `|w|` clamp destroys sign → defeats near-plane rejection.
- `struct_block.cpp:captureRegion` — no region-size cap; `maxY == INT32_MAX` signed-overflow UB.
- `voice.h:dutyFraction` indexes 4-element array with unvalidated enum — OOB read.
- `verifyModSignature`: I/O error degrades to `Unsigned` (fail-open on transient errors).
- `riggen` thread-local cache: failed `extractRig` → later `computeSkinning` silently leaves influences empty; NaN vertex → `int(floor(NaN))` UB.
- `TripleBuffer::read()` lifetime undocumented — game must copy `simStats()` immediately; `Game::simStats()` also derefs `sim_` unchecked.
- `give 0 N` silently eats items (stoul wraps `-1`, id 0 reads as empty slot).
- Engine doc comments now false under the live-ops carve-out (`mod_manifest.h:45`, `block.h:252-253`: "release always seals").
- `litho_sign` keygen: `.priv` created with umask perms then chmod 0600 — tiny wider-perm window.
- `resource_loader.cpp:crc32Table()` lazy init is data-racy (benign but formal UB).
- `net/quantize.cpp`: `packQuat` NaN → UB float-to-int; `posToFixed` overflow UB.
- `net/confinement.cpp`: `windowBytes + bytes` can wrap.
- `chase_lev.h`: throwing `std::function` copy in `steal()` escapes `findJob`'s try/catch → worker death.
- `ai/senses.cpp:7`: zero-length segment walks 512 cells instead of returning 0.
- `world/raycast.cpp`: DDA never tests the origin cell.
- `core/math.h`: `normalize()` of zero vector → NaN.
- `core/tick_arena.cpp:88`: size-wraparound on absurd sizes vs "never returns nullptr" contract.
- `storage/anvil_region.cpp`: `readHeader` silent normalize; no dir fsync after commit (durability gap).
- `storage/file_contract.cpp`: second torn primary overwrites forensic copy; AnvilRegion has no production callers.
- `main.cpp`: unchecked `*ModManifest::parse` deref, `--resolution` strtol→int before bounds check, seal reason doesn't name the mod + CWD `pack.json` pickup, `--selftest` never calls `scanMods()`.
- `diversity.cpp`: `posRoll2` dead code; `loadBiomeDensity` defaults 0 → silently inert features (footgun); `worleyF1` correlated jitter axes (quality nit); transparent non-cube shapes get no quads on BinaryQuad path.
- Copy-pasted doxygen debris on `kFusor`; `rand()` for item-toss velocities adjacent to sim events.

## Coverage

- **R1:** all of core/, world/, sim/, entity/, ai/, storage/, net/, console/, script/.
- **R2:** all of procgen/, anim/, crypto/, assets/, render/, audio/, ui/; render_vk scan-only (no GPU); skipped re-verifying NIST/RFC vectors, vendored nanosvg/stb_image.
- **R3:** app/main.cpp (full), content/blocks.*, game/game.cpp + game_commands.*, world_save.cpp, inventory.h; engine headers for contract verification. Skipped: headless/, model/vegetation/components internals, docs/.
- **Determinism audits (sim/world/procgen):** clean overall — fixed neighbor orders, ordered maps, seeded RNGs; no sim-hot-path hazards beyond listed items.

## Recommended sequencing for the builder

1. Items 1–3 (fail-fast boundary, key hygiene, seal early-return) — correctness of the safety story itself.
2. Items 4–6 (permission laundering, refresh race, alias cycle) — security/determinism invariants.
3. Items 7–8 (IK NaN + cycle guards) — crash-class, cheap fixes.
4. Items 9–10 + mediums (net UB, JSON cap, ui_show, symlink, PNG bomb) — beta hardening.
5. The medium tier in any order; lows as drive-bys.

# LithoLattice / LithoKraft — Full Code Review
**2026-10-11 · read-only · four area reviewers · post-audit follow-up**

Scope: `~/workspace/litholattice` — full tree, with EMPHASIS on everything that landed since the morning audit (Repo-Audit-2026-10-11.md: 68 findings — 1 CRITICAL, 12 HIGH, 30 MEDIUM, 25 LOW). The audit's fix program landed all day; this review verifies those fixes and hunts for new issues in the post-audit landings: sub-block subdivision modeling, Wafer biome procgen, Fragment markdown, delimiter pairing, ProcGen vegetation, terrain expansion.

Nothing was modified. Every finding cites an exact file:line, states concrete impact, and proposes a fix. Audit findings already fixed are NOT re-reported (listed as verified in §5); audit findings still present are noted only where still relevant.

## Executive summary

| Severity | New findings |
|---|---|
| CRITICAL | 0 |
| HIGH | 1 |
| MEDIUM | 5 |
| LOW | 19 |
| INFO | 1 |
| **Total** | **26** |

Plus: the audit's fix program verified — the overwhelming majority of its 68 findings are fixed and verified in-tree (details in §5). The few remaining audit items are documented/accepted (net plaintext attacks awaiting builder KEX; MCP session cap partial).

**Bottom line:** the post-audit landings are high quality — deterministic, fail-closed, well-tested (the cross-cutting reviewer calls them "the best test suites in the tree"). One new HIGH must be fixed before alpha. Two verification actions should happen before alpha. With those done, the tree is alpha-safe.

## Top 3 issues JD must see

1. **[HIGH] Restricted scripts escalate to ambient authority on double-`wait`** — `engine/src/console/concmd.cpp:850`. `tick()` restores the ambient mask *before* the re-suspension branch, so `queueSuspended()` stamps the script with ambient authority on its second suspension. A restricted MCP agent or mod chaining `wait 1; wait 1; <cheat command>` gets full authority — this **re-opens the keybind-permission-laundering hole the fix program just closed**. Surgical fix: preserve `resume.callerMask` across the re-suspend branch + a selftest. **Alpha blocker.**
2. **`lithokraft_selftest` was never re-run after its root cause was fixed.** The audit-era abort (blockRegistry 40≠36) is fixed in-tree (verified arithmetically: 54 `r.add(` + Air = 55 = `kSilicaTilledSoil`(54)+1), but the suite's current status is unverified — and it's the ONLY passing coverage for: the shipped `organic.json` with the now-live ravine carver, and save/load round-trips of worldgen'd chunks. The main `lithokraft` binary isn't in the build dir, so this needs a game-target build + one run. **Biggest verification hole; close before alpha.**
3. **Ground traces are dead on arrival.** The Subdivision Report claims a GLSL mirror (`groundTraceColor()`), GL33 uniforms, Vulkan push constants, and a live `render.ground_traces` cvar. None exists in the tree: the uniforms resolve to -1 (silent no-ops), the cvar is never registered, the CPU mirror has zero consumers. The feature renders nothing. Either land the shader mirror or stop claiming the feature. (No crash/corruption — MEDIUM.)

---

## 1. New findings

### HIGH

**[HIGH] `engine/src/console/concmd.cpp:850` — re-suspended scripts escalate to ambient authority.**
`tick()` installs `resume.callerMask` (the script's restricted mask), runs the commands, then restores the tick-loop ambient mask at :843 *before* the re-suspension branch at :845-851. `queueSuspended()` (:742) stamps `s.callerMask = callerPermissions_` — now the ambient mask, not the script's. A script that suspends twice (`wait 1; wait 1; give diamond_sword`) gets its authority *upgraded* on the second suspension. This defeats the just-landed keybind-laundering fix: a restricted context (MCP agent via `lithoscript_exec`, mod via `consoleCmd`, restricted-binder keybind) chains two non-cheat `wait`s; everything after the second `wait` executes under full authority. Existing selftests (console_selftest.cpp:838-873) cover only single-suspension authority — untested.
*Fix:* in `tick()`'s re-suspend branch, reinstall `resume.callerMask` around the `queueSuspended` call (or move the mask restore below the branch). Add a selftest: restricted mask + `wait 1; wait 1; dbg_cheatdrop` must print the cheat-refused message, not CHEAT_FIRED. (`executeLine`:600 and `executeFile`:993 stamp correctly — `tick()` is the only broken site.)
**→ Alpha blocker.**

### MEDIUM

**[MEDIUM] `engine/assets/shaders/chunk_shading.glsl` — ground traces are dead on arrival: the claimed shader mirror doesn't exist.**
The Subdivision Report claims additive shader changes, a GLSL `groundTraceColor()`, GL33 uniforms + Vulkan push constants, and a live `render.ground_traces` cvar. Reality: the GLSL contains no `groundTraceColor` and no `uGroundTraceTime`/`uGroundTraceOn` uniforms; no Vulkan support exists; `render.ground_traces` is referenced only in comments (`renderer.h:138`, `ground_traces.h:25`) and never registered as a cvar; the CPU mirror `groundTraceEmissive()` has zero consumers outside its selftest. The GL33 renderer (`gl33_renderer.cpp:283,398-399`) queries and sets both uniforms — locations resolve to -1, silent no-ops. The feature renders nothing; toggling the (nonexistent) cvar changes nothing. No crash or corruption.
*Fix:* implement the GLSL mirror + uniform wiring (and Vulkan push constants) per the header's HOW section, or mark the feature as not-yet-landed so the report's in-game claims stop being false.

**[MEDIUM] `engine/src/procgen/water.cpp` — the ≥60-solid-below-ocean invariant breaks when world height < 256.**
`OceanFillRule::apply` clamps sea to the world top at runtime (`const int sea = clampLevel(s_.seaLevel, top)`), but `loadOceanSettings` (`generator_loader.cpp`) validates `sea_level - max_depth >= kMinSolidBelowOcean` against the *configured* sea level. With a short world (e.g. height 128) and default `sea_level=256`, the loader accepts (256−196=60), yet the actual surface is at 127 and `floorTarget` clamps to ≥0 — the ocean floor can sit near y=0 with fewer than 60 (even 0) solid layers below the liquid, and the water surface silently differs from the configured value. Latent: no shipped dimension wires oceans/lakes yet; tall worlds unaffected.
*Fix:* validate `sea_level` against the actual world height in the loader (plumb the height in), or compute the check with `min(seaLevel, worldTop)`. Must land before water is wired into any dimension shorter than 256.

**[MEDIUM] `water_selftest.cpp:20,75` — the "matches fluid.h" check cannot catch the drift it claims to guard.**
The test pins `kStillWaterLevel == kFluidMaxLevel`, but `kFluidMaxLevel` is a **local copy** (`inline constexpr uint8_t kFluidMaxLevel = 16; // mirrors litholattice/world/fluid.h`) rather than the real `litholattice::kMaxFluidLevel` from `engine/src/world/include/litholattice/world/fluid.h:9`. If fluid.h changes to 15 or 32, this test still passes green while the worldgen↔fluid-seam contract silently breaks. The check's name ("worldgen water level matches fluid.h kMaxFluidLevel") gives false confidence on a cross-module contract.
*Fix:* `#include "litholattice/world/fluid.h"` and compare against the real constant.

**[MEDIUM] `engine/src/procgen/trees.cpp:162-167` — the two-colour tip alternation is a documented contract with zero direct tests.**
The extremity-index alternation rule (`tipMaterial`/`leafMaterial` by `ei % 2`) is implemented and documented in three places (trees.h, trees.cpp, vegetation.h), and it's vegetation decision #1 ("Decisions for JD to lock"). No test verifies alternation itself — suites only assert both materials *appear somewhere* (`vegetation_selftest.cpp`: `seen[kTraceRed] && seen[kTraceBlue]`). A silent change to the rule (e.g. the very alternative JD is considering) passes the entire suite.
*Fix:* add a check that extremity tip cells contain both materials in alternating order, or drop the rule from the contract until JD locks it.

**[MEDIUM] `game/LithoKraft/assets/worldgen/organic.json:37` — the now-LIVE ravine carver has no passing end-to-end coverage.**
`{"type": "ravine"}` in organic.json used to be a placeholder; per the terrain report it now carves real canyons (1/16 chance, 64–128 long, 16–32 deep) with default params. `engine_procgen` tests the ravine *logic* against synthetic JSONs and passes, but **no passing suite loads the shipped organic.json end-to-end**: `lithokraft_scout_selftest` only loads wafer.json, the headless harness generates wafer columns only, and `lithokraft_selftest` (the suite that loads both dimensions via `createDimensions()`) is grandfathered. A malformed organic.json, loader-config mismatch, or ravine-with-real-config crash is invisible to CI.
*Fix:* re-run `lithokraft_selftest` (see §4), or add an organic.json load+generate smoke check to the scout selftest.

### LOW

**Engine core:**
- `[LOW] engine/src/subdiv/octree.cpp` — `SparseOctree::empty()`/`leafCount()` build the full leaf vector (O(n) allocation) just to test emptiness; called per subdivided block in the chunk-build loop. The collapse invariant in `set()` guarantees `!root_ || root_->cells.empty()` ⟺ empty — implement `empty()` directly from `root_`.
- `[LOW] engine/src/procgen/stages.cpp` (`RiverCarver::carve`) — `ctx.maps.height[col] = cutY` assigned unconditionally after carving, but if `cutY` coincides with pre-existing cave air (bed guard skips placement when the cell is air), `maps.height` claims a solid top that is actually air. Downstream stages key off it. Narrow trigger: river+cave overlap. *Fix:* only assign when the floor cell is solid after bed placement, or scan down for the true top.
- `[LOW] engine/src/procgen/water.cpp` (`ShorelineRule` ctor) — duplicated lake-settings normalization re-implements `LakeFillRule`'s clamp block to reproduce the lake mask; if the two drift, shoreline rims won't match lake fills. *Fix:* share one `normalizeLakeSettings()` helper.
- `[LOW] Unnamed tuning literals vs the "no nonsense" doctrine` — water constants are properly named (`kMaxOceanDepth` etc.), but the growth code has ~15 unnamed tuning literals (`trees.cpp`: 0.62f radius falloff, 0.30f upward bias, 2.2f/4.0f blob radii, 0.7f shard radius, 0.9f branch-length scale; `shrubs.cpp`; `stages.cpp` `kDeflect` table is documented inline as ±30/60/90° — acceptable). Hygiene only.

**Engine systems:**
- `[LOW] engine/src/console/concmd.cpp:590` — unterminated `/*` on an interactive line is silent. `executeLine` uses the error-dropping `tokenize(line)` overload, so `echo hi /* oops` executes nothing with no feedback. Fail-closed (nothing runs), but the exec-file path prints the reason while the console doesn't. *Fix:* use the `err` overload in `executeLine` and print the message.
- `[LOW] engine/src/console/concmd.cpp:746-768` (`bumpCVar`) — NaN delta silently zeroes an int cvar. The llround-UB fix maps NaN → `q = 0`, so `incrementvar myint nan` sets the cvar to 0 instead of refusing (delta parsed with raw `strtod`, which accepts nan/inf); for float cvars NaN flows into `%.9g`. *Fix:* fail closed — return false with a printed reason when `delta` is non-finite.

**Game:**
- `[LOW] fragment_markdown.cpp:329` — signed-int overflow (UB) in `numberedItem` on huge numbers. `number = number * 10 + (l.text[k] - '0')` accumulates an unbounded digit run into an `int`; `99999999999999999999. x` overflows → garbage `start`. Reachable by any player typing a big number in a fragment comment. *Fix:* bail or saturate after ~9 digits.
- `[LOW] fragment_markdown.cpp` (splitLines/trim) — `\r` retained in Rendered view. `trim` strips only space/tab, so CRLF-sourced fragments leave a trailing `\r` in paragraph/heading/quote text (header documents keeping `\r` only for Source view). Cosmetic; the future Fragment UI would render a stray glyph. *Fix:* strip `\r` in render paths, keep in Source.
- `[LOW] screenshot.cpp:127` — double `fclose` UB in `writeFileBytes`. `ok = n == bytes.size() && std::fclose(f) == 0`; if the first `fclose` fails (e.g. disk-full flush error), the `!ok` branch calls `std::fclose(f)` again on an already-closed `FILE` → UB. *Fix:* close exactly once.
- `[LOW] wafer_biomes.h:83-84`, `wafer_biomes.cpp:35` — stale pre-renumber block IDs in comments (`kScribeSilicon/49`, `kDenseSilicon/50`, `kFleckedSilicon/51`, `kDeadSilicon/46` vs actual 47/48/49/44). Code uses named constants (behavior correct), but these are pre-renumber leftovers — the wrong-number trap the renumber was meant to kill. *Fix:* correct the comments.
- `[LOW] game.cpp:911,243` — `screenshot <path>` / `genmap <path>` write to unsandboxed paths. `writeFileBytes` takes the raw console arg (+ `create_directories` on the parent). Console is reachable via MCP agents and mod `consoleCmd` — same trust class as the keybind-laundering finding. Content is PNG bytes only (limited blast radius), but no path confinement at all. *Fix:* confine to a screenshots dir under the save/config dir, or reject `..`/absolute paths.
- `[LOW] settings_cvars.cpp:63,77,85,89` — unclamped int cvar setters. `client.display_mode`, `sim.tickradius`, `render.mesher`, `render.pbr_tier` take raw `toInt(v)` while `settings.json` `load()` clamps the same fields. `render.pbr_tier 9` flows into `setPbrTier(PbrTransparencyTier(9))` — matches no shader branch (misrendered transparency; no crash). Same class as the fixed `hud.scale`/`sim.tickrate`. *Fix:* mirror the `load()` clamps in the setters.

**Cross-cutting:**
- `[LOW] procgen_selftest.cpp:2375-2383` — the "chunk-pure" river test is tautological: `solo = gen->generate(info(5,5))`, then the loop only compares when `dx==0 && dz==0` — chunk (5,5) compared **to itself**. Can only fail if determinism breaks (tested separately); doesn't test reach/replay coverage. *Fix:* delete or make meaningful (mutation check: reduce reach, assert the seam test fails).
- `[LOW] shrubs_selftest.cpp:186` — "texture bytes stable" is near-tautological (`textureHash(i)` re-fetches and re-hashes — only proves two successive calls agree); `:72,:74,:196` — golden `if (kGolden != 0)` skip pattern means zeroing a constant silently disables the check. *Fix:* assert goldens unconditionally (remove the guards).
- `[LOW] delimiter_pairing.cpp:179-216` — `balancePastedDelimiters` is not idempotent on multiple lone closers; header overclaims. Doc: "The returned text is balanced as a standalone unit." Counter-case: `/* a */ */` → `/*/* a */ */`, still ends with a lone `*/`; repeated pastes accumulate `/*` prefixes. Harmless in practice (editor nicety, common cases correct, fail-closed caps in place). *Fix:* soften the doc to "best-effort pairing; balanced input returned byte-identical."
- `[LOW] OceanFillRule clamps silently in C++ but the loader rejects loudly — two behaviors for one condition.** `OceanSettings{maxDepth: 500}` → constructor clamps to 196; same value in JSON → loader rejects with error. Both fail-closed, both tested, but the split ("liberal API, strict config") is undocumented in water.h. *Fix:* one doc line.
- `[LOW] fragment_state.h` — `upgradeFragment()` thread-safety is undocumented. No atomics, no locks, header-inline. Zero production callers today (Spark event doesn't exist; only tests call it) — convention-only risk. *Fix:* one doc line ("call from the game thread only") before the Spark wiring lands.
- `[LOW] Water no-water rule no-op is untested behaviorally.** `clampChecks` verifies `OceanFillRule(noWater).settings().water == kAirMaterial` but never generates with it to prove no water is written. Minor. *Fix:* one generate-and-assert-no-water-cells check.
- `[LOW] Trees have no pinned golden hashes.** Unlike shrubs (5 pinned goldens), trees rely on same-process hash equality; a change altering *all* tree outputs uniformly wouldn't trip `engine_trees`. Partially mitigated: `vegetation_selftest` compares re-authored models against checked-in `.lthm` files byte-for-byte. *Fix:* pin one golden `hashLeaves` per preset in `engine_trees`.

### INFO

- `[INFO] Cross-platform bit-identical determinism caveat.` Trees/shrubs/carvers/traces use libm transcendental functions (`sin`/`cos`/`sqrt`/`pow`), which can differ by 1 ULP across libm implementations. Same-build determinism is verified by the tests; cross-platform (multiplayer/save portability) bit-identity isn't strictly guaranteed. Pre-existing pattern across all procgen, not introduced by these landings.

---

## 2. Design tensions — verdicts (game area)

1. **Markdown subset boundaries: VERIFIED graceful.** Tables/setext/HTML/strikethrough/footnotes → plain paragraphs (no false structure); `![a](b)` → Link span (degrades to a link, not an image); nested lists → flattened (documented); multi-level quotes → joined single-level (documented); unclosed fence → CodeBlock to end (documented CommonMark rule); unterminated `/*` → err + partial render (documented decision). No crash/hang paths: `parseInline` recursion bounded by input length, all delimiter scans linear, worst case O(n²) — no exponential blowup. Pure functions, no RNG. The only boundary warts are the two LOWs above (int overflow, `\r`). **Test gap:** `fragment_markdown_selftest` has no pinned checks for the unsupported boundaries — worth adding so the boundary can't silently drift.
2. **LFS1 vs LBT: NO real inconsistency in the tree today.** `FragmentState::serialize()/deserialize()` and `markdown_unlocked()` have **zero production call sites** — nothing saves or loads it outside the selftest (verified by repo-wide grep; also true of `renderFragment` and the `FragmentGallery` LFG1 blob). It's a designed-for-persistence unit awaiting the Spark wiring. **Recommendation when wired:** embed the 8-byte LFS1 blob as an LBT byte-array value inside the canonical save document rather than a separate save file, so backup/migration/recovery tooling stays unified. Minor forward-compat note: `deserialize()` silently masks unknown flag bits — forward-compat is by version bump only.
3. **`upgradeFragment()`: VERIFIED clean.** Idempotent (repeat call is a no-op, tier untouched); tier +1 only per newly applied upgrade; saturates at 255, never wraps; `deserialize` fail-closed (bad magic / wrong version / any truncation / trailing bytes → `false` + err, object unchanged). 37/37 selftest battery covers double-upgrade, tier suite, all 8 truncation lengths, v1 rejection.

## 3. Alpha blockers (proposed — JD decides)

1. **[HIGH] `concmd.cpp:850` re-suspension permission escalation** — fix + selftest. Without it, a restricted MCP agent or mod reaches cheat authority, contradicting the trust model the fix program just implemented.
2. **Re-run `lithokraft_selftest`** (needs a game-target build; the main `lithokraft` binary isn't in build-linux) — closes the organic-ravine and save/load-of-worldgen coverage gaps in one move. If it fails elsewhere, we learn the new failure.
3. **Ground traces: land the shader mirror or stop claiming the feature** — the Subdivision Report's in-game claims are false today. Blocks the *claim*, not alpha itself.
4. **Ocean loader world-height check** — must land before water is wired into any dimension shorter than 256 (latent until wired; nothing ships water yet).
5. **Water fluid.h mirror test + tip-alternation pinning** — fix before alpha; both are false-confidence risks on shipped contracts.

NOT alpha-blocking: the 19 LOWs (ride the normal fix flow); the documented net plaintext attacks (builder KEX is the plan, per the audit); the MCP session-table cap (partially fixed, localhost-only).

## 4. Grandfathered failures — blast-radius assessment

- **`engine_sim`** = `litholattice_tests sim` → `testSim, testChangedColumns, testRandomTicks, testDetSched, testTraceJitCompose, testSignalPropagate, testBlockNames, testAttachables`. The known failure is in **`testBlockNames`** (storage block-name remap narrowing). **No new code touches those paths.** Closest overlap (silica random-ticks) dispatches game-side via `signal_world.cpp:56` → `silicaOnRandomTick`, covered by the passing `lithokraft_silica_selftest` (13/13, separate binary). **Blast radius on new code: none.**
- **`lithokraft_selftest`** = `lithokraft --selftest` → signal/content/audio/input/assets selftests, `runProcgenSelfTest`, then **wafer + organic dimension load → SignalSim::scan → save/load round-trips → worldgen determinism checks**. The audit-era root cause (blockRegistry abort, 40≠36) is **fixed in the current tree** — verified arithmetically: 54 real `r.add(` calls + Air (row 0, added by the `BlockRegistry()` constructor at `engine/src/world/include/litholattice/world/block.h:90`) = 55 = `kSilicaTilledSoil`(54)+1, matching the guard at blocks.cpp:102. Corroborated: the passing `lithokraft_scout_selftest` (90+ checks) calls `blockRegistry()` at scout_selftest.cpp:52. **However: no report re-ran it — current status is unverified.** Its unique unreplicated coverage: full wafer+organic dimension load (only path loading shipped organic.json with the live ravine), save/load round-trips of worldgen'd chunks, full content selftest against all 55 block rows. **Bottom line:** new code doesn't depend on *known-broken* paths, but the organic-dimension live config and game save/load-of-worldgen sit on ground *unverified since the fix* — covered by nothing passing today.

## 5. Audit fix-program verification

The fix program closed the overwhelming majority of the audit's 68 findings. Verified fixed in this review:

**Render/sim core:** PackedVertex CRITICAL (slot moved to struct end @12, 12-byte prefix stable; all three GL binding sites + VK pipeline bind @0/@6/@10 matching both chunk shaders); `sim.tickrate 0`/`setTickRate(0)`/`ClockDomain{hz=0}` SIGFPE (game clamps via `nearestTps`, engine fail-fasts via `simFailFast`); player edits now tick-quantized (`applyEdits(ray, worldTickN)` inside the per-tick loop); wireless tie-break canonical (`(strength, dist², packPos)`); `EntityManager` iterates in id order; wire-network ids from sorted `packPos`; `TickContext` renamed to `SchedTickContext`; `TickArena` rounds up via `nextPow2`; `SimThread::getBlock` race (sim thread holds `blockMutex_` across the mutation window); `LightEngine::propagate` uint8 underflow (signed math); `removeSource` two-pass.

**Console/mods/scripting/MCP:** `consoleCmd` ambient authority (permission mask captured at bind, RAII install); keybind permission laundering (binder's mask stamped + logged) — *caveat: the new HIGH re-opens it for double-wait scripts*; `mod.json` `std::stoul` crash (fail-closed `parseUint32`); `exec` TOCTOU (`O_NOFOLLOW` + `/proc/self/fd` re-validation); `loadArchived` removed (archived cvars boot via sandboxed `executeFile`); `saveArchived` confined; AnvilRegion 7-bit overflow (`kMaxInlineBytes = 127 * kSector - kRecordOverhead` + `static_assert`); `writeFileAtomic` crash window (`recoverCrashedWrite()` wired into `RegionFile::open` and `RecordStore::load`); IoScheduler strand identity (path canonicalized); `query` int64 narrowing (int32 check first); `bumpCVar` llround UB (saturation); `SandboxedFile::operator=` UB; `readAll` EINTR + `maxBytes` caps; quarantine log spam; `RegionFile` bounds; `ready_` cap (`maxReady` 256 + drop policy); handshake junk liveness; `needFull` rate limit; snapshot delta `satAdd`; confinement log rate limit; `version.h` dead config removed; `kPermPilotSenses` schema; memory-quota suspend now aborts the in-flight script; revocation unbinds via `syncBindings()`; stdio 16 MiB chunked cap; both audio races (atomic `pendingReset_`, `queriesMu_` mutex + snapshot).

**Game:** all 7 audit items — `sim.tickrate 0` cvar, menu dolly position, paused-dimension-switch, console `pause` in menu, `save` honesty, `config.lth` boot execution, `hud.scale` clamping.

**Still present (documented/accepted, none new):** spoofed ack/heartbeat and spoofed Reject / version downgrade (inherent to unauthenticated plaintext; partial `bindPeer` endpoint-token mitigation; documented with the builder-KEX plan — not alpha-blocking per the audit); MCP HTTP session table (authenticated `DELETE /mcp` now wired to `eraseSession`, but still no idle-timeout or cap — localhost-only, LOW).

**Post-audit landings verified sound:** block-comment tokenizer (state machine correct across all three implementations, fail-closed unterminated, cross-check tests); delimiter pairing (16 MiB caps, auto-inserted-only skip-over, provably underflow-free arithmetic); FragmentState LFS1 v2 (exact 8-byte blob, fail-closed on all 8 truncation lengths, bad magic, wrong version, trailing bytes); octree/subdiv (deep-copy semantics, strict `LTHS` deserialization rejecting 7 malformation classes, `.lthm` CRC32 + bounds-checked cursor); procgen trees/shrubs (seeded xoshiro256**, fail-closed `sanitize()`/`validShrubParams()`, no unordered containers in output paths); terrain (chunk-pure carvers with per-origin-chunk local Rngs, loader fail-closed validation, named depth constants); biome procgen (seeded hashes, weight-aware blending, `genmap` size refusal tiers).

## 6. Test-quality verdict

**Genuinely strong overall — the best test suites in the tree**, with the specific weaknesses listed above. Verified real, not ceremony: cave replica tests re-derive expected carved sets over 3×3 chunks and compare *exactly* (double-entry bookkeeping — caught two real bugs mid-flight: worm density double-roll, branch RNG snapshot); mountain exact node math + 6 known-seed spot heights + byte-identical determinism across independent loads; water exact-98 lake cap in a 200-deep pit, ≥60-solid invariant, byte-identical buffers across independent loads, 7 loader accept/reject cases; tree/shrub fail-closed degenerate batteries (0/65/−3 size, NaN density, zero materials), segment-budget termination, golden leaf/texture hashes; vegetation save→load→`modelEquals` round-trips, re-author byte-identical files cross-process, checked-in `.lthm` files matching authoring code byte-for-byte; fragment markdown/fragment/delimiter malformed-input batteries with object-unchanged-on-failure; tokenizer fail-closed behavior with 81 console checks. **Fix before alpha:** the water fluid.h mirror test (false confidence on a cross-module contract) and the tip-alternation pinning (untested documented contract). The rest of the weaknesses are LOW hygiene.

## 7. Thread-safety verdict

**Clean — no new data races introduced.** All new carvers are chunk-pure replay with per-origin-chunk local `Rng`s (no shared mutable RNG — `seed.h` documents "one per task"), `const` carve paths, immutable pipelines, per-call `GraphContext`s, `biomeAt` documented thread-safe; multithreaded wafer generation in `perf_headless` exercises this in practice. Trees/shrubs are pure functions of params (function-local-static texture set = thread-safe init). Delimiter pairing is stateless pure logic. `FragmentState` has no production callers yet.

## 8. Overall verdict

The post-audit landings are **alpha-safe with the HIGH fixed and the two verification actions done** (fix `concmd.cpp:850` + selftest; re-run `lithokraft_selftest`). The tree is in genuinely good shape: deterministic, fail-closed throughout, named constants where JD's doctrine demands them, and tested by the strongest suites in the project's history. The 19 LOWs ride the normal fix flow. The ground-traces claim needs correcting (land the mirror or unclaim it). Nothing else stands between this tree and alpha.

---
*Method: four parallel area reviewers (engine core, engine systems, game, cross-cutting), all read-only; suites re-run where cited (engine_subdiv, engine_trees, engine_shrubs, engine_water, engine_procgen, engine_storage, engine_net, engine_script, engine_mods, engine_console, engine_mcp, lithokraft_fragment_selftest, lithokraft_markdown_selftest — all green). Severity rubric: CRITICAL = corrupts data/render or crashes on a normal path; HIGH = crash/data-loss/security bypass reachable in practice; MEDIUM = real bug, narrower trigger; LOW = robustness/hygiene.*

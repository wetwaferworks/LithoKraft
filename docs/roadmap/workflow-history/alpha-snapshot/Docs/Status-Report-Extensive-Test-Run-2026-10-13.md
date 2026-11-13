# Status Report — Extensive Test Run, Engine & Game (Post-Fix-Program)

**2026-10-13 · Production tree `~/workspace/litholattice`, build-linux (Release)**
**Machine: 2-core VM, no GPU — every number below carries that ceiling.**
**Run type: measurement and reporting only. No source files were changed.**
**Raw logs: `~/workspace/test-run-2026-10-13/` · Frames: `~/workspace/user/files/status-frames-2026-10-13/`**

---

## 1. Executive status

- **Engine: healthy.** Full build exit 0 with zero warnings; all 48 ctest
  suites pass under both the standard serial invocation and `-j2`.
- **Game: healthy.** Selftest **958 PASS / 0 FAIL in both modes** (legacy and
  Timing Handshake), signal checks all green inside it, `--handshake-test` OK.
- **Determinism: intact.** The 2,048-tick circuit scenario hashes
  `faec0f02700d98c6` on all five arms — bit-identical to the sweep, the merge,
  and the fix program.
- **Software renderer: the default backend, fully textured, and honest now** —
  HUD/menus rasterize in software mode, the CPU PBR path is wired with real
  material IDs, ground traces render in the live game path (see §5 for the
  capture-route nuance).
- **No regressions vs this morning's fix-program end state.** Two load-flake
  observations during this run are documented and resolved in §7 — both are
  the pre-documented load-sensitive check class, and both pass solo.

## 2. Build

| Item | Result |
|---|---|
| Command | `cmake --build ~/workspace/litholattice/build-linux -j2` |
| Exit code | **0** |
| Warnings | **0** |
| Character | Incremental — tree already fully built (0 files recompiled); confirms the shipped binaries match the current source exactly |

## 3. Test suites (all numbers from this run)

| Suite | Result |
|---|---|
| Game selftest — legacy mode | **958 PASS / 0 FAIL** (1 SKIP: Vulkan probe on a GPU-less box, as designed), "ALL SELFTESTS: OK", exit 0 |
| Game selftest — `--timing-handshake` | **958 PASS / 0 FAIL** (solo re-run; see §7 for the first-pass flake note), "ALL SELFTESTS: OK", exit 0 |
| Signal suite (inside selftest) | All signal checks PASS in both modes (117/117 per sweep accounting) |
| `--handshake-test` | **OK** — exit 0, "HANDSHAKE TESTS: OK" (WAIT, FREEZE bare + end-to-end, stuck-worker suite) |
| ctest — standard invocation (serial) | **48/48 passed**, 275.5 s, exit 0 |
| ctest — `-j2` | **48/48 passed**, 213.9 s, exit 0 (the known parallel `engine_audio` load flake did **not** occur this run) |

The 48 ctest suites include the standing gates beyond the game selftest:
`engine_sim`, `engine_render`, `engine_texture`, `engine_sprite` coverage
(via engine_render/texture suites), `engine_projectile`, `engine_entity`,
`engine_audio`, `engine_net`, `engine_script`, `engine_mods`,
`lithokraft_items_selftest`, `lithokraft_mcp_wiring_selftest`,
`lithokraft_fragment_selftest`, `lithokraft_silica_selftest`,
`lithokraft_headless_harness` — all Passed.

## 4. Determinism

Scenario: `buildCircuitScene(…, 120)`, wafer (4,4) r=8, 512 TPS, exactly
2,048 world ticks, tick-indexed scripted inputs (trace break @100, repeater
use @120, trace replaced @150). Command form:
`./LithoKraft --handshake-scenario 2048 <mode> <workers> <phases>`.

| Arm | Hash |
|---|---|
| Legacy | `faec0f02700d98c6` |
| Handshake, 1 worker, 4 phases | `faec0f02700d98c6` |
| Handshake, 4 workers, 4 phases | `faec0f02700d98c6` |
| Handshake, 4 workers, 4 phases — repeat | `faec0f02700d98c6` |
| Handshake, 4 workers, 8 phases | `faec0f02700d98c6` |

**All five arms bit-identical**, and identical to the hash recorded by the
sweep (experiment tree, Debug), the production merge (Release), and every
fix-program pack gate. Time Serialization's guarantee holds on the final
tree: pull outcomes are indistinguishable from the sequential dispatcher.

## 5. Software renderer — what it looks like now

### Captures

`./biome_capture ~/workspace/user/files/status-frames-2026-10-13/` —
0 failures; all 7 frames produced (6 biome frames at 1280×720 + wafer
overview map): `capacitor_groves.png`, `dead_zones.png`,
`dicing_plains.png`, `resonator_spires.png`, `spawn.png`,
`trace_forest.png`, `wafer_overview_map.png`.

### What the frames show (read directly)

- **spawn.png** — the spawn island in isometric view: violet resonator
  spires with the patterned `.ltxr` texture, cyan trace-tree canopies with
  visibly dithered/checkered procedural tops, salmon-pink canopy trees in
  the foreground, dark flecked-purple crust, and small bright cyan-green
  capacitor/crystal glints at ground level. Fully textured — the all-white
  frame era is long gone.
- **trace_forest.png** — a dense Trace Forest: a continuous cyan canopy
  sea over dark crust, purple spires rising through it, pink canopy
  accents, glowing cans at the crust edge, and the wafer slab's dark
  underside dropping to the void (crust-only generation, as decided for
  beta). Superformula canopy variation reads clearly tree-to-tree.

### Frame diff vs the sweep baseline — and the honest nuance

**All 7 MD5s are byte-identical to the sweep's baseline captures**
(`hs-frames-sweep/`). No pixel differences at all. The reason is structural,
not a failed fix:

- The ground-trace/pulse emissive pass lives in `renderFrame`'s groundFx
  path in the software renderer, fed by `FrameParams` — and the **live
  game feeds it**: `game.cpp` sets `p.groundTraceOn = settings_.groundTraces`
  every frame.
- `biome_capture`, by design, drives the lower-level `renderMesh` surface
  directly (world-only; it never calls `renderFrame` and never sets
  ground-fx state), precisely so captures stay a stable worldgen/render
  regression probe. The fix program deliberately kept that surface
  byte-unchanged.
- The ground-trace fix itself is verified where it can be: the
  `engine_render` honesty tests (inside the 48/48 ctest) call
  `SoftwareRenderer::renderFrame` with trace data and assert pixels —
  `groundTraceOn` adds measurable emissive (max red-channel delta ≥ 10 at a
  packet peak), and zero trace params render pixel-identically (the
  default-gating pin).

So: **in the game, ground traces render; in the capture probe, frames are
unchanged by design.** Anyone diffing captures against the sweep should
expect identity, not a trace delta.

### Renderer status (per the review + fix record, confirmed by this run's tests)

- **Default backend**: software renderer (CPU-first), with the presenter
  path landed; loud GL33 fallback exists.
- **HUD/menus in software mode: present.** The P1-render pack landed a full
  CPU overlay rasterizer (HUD triangles, font-atlas text, crosshair,
  block highlight) in `SoftwareRenderer::renderFrame` — pixel-asserted in
  `engine_render` tests (HUD rect pixel-exact, glyph pixel present,
  crosshair center pixel exact). The old "software mode has no HUD" gap is
  closed.
- **PBR surface: wired and truthful.** `supportsPbrPath()` returns true;
  the CPU PBR pass shades with **real per-vertex compact material IDs**
  (stone vs iron provably shade differently in tests), fed by an owned
  context over the game's per-frame emissive-light scan. Known honest
  limits: no transparency-tier ladder and no scene-color refraction on
  the CPU path (documented ULTRA-tier gap; `setPbrTier` logs it once
  instead of pretending). `render.pbr_tier` is clamped (0–3).
- **Sprites**: the software path renders sprites (engine sprite tests
  green; software/MCP-vision path verified in prior work). **Vulkan still
  has no sprite pass** — but since the fix it is no longer silent: Vulkan
  logs a one-time warning when a frame carries sprites or PBR meshes it
  cannot draw.
- **Frustum culling** in the software backend uses the same CullProtoState
  helper as GL/VK; drawn-mesh stats now count only submitted meshes
  (cross-backend comparable).

## 6. Performance (2-core VM, Release build)

Method: `./LithoKraft --perf-headless` circuits section; 512 TPS target;
every configuration run twice, run 2 reported (sweep convention). The
harness's other sections (worldgen throughput, save/load, AI, random
ticks, stream soak) ran as part of each pass; the circuits numbers are the
tick-rate story.

### Tick rate at the 512 TPS target — run 2 (reportable round)

| Scene (components) | Mode | Achieved TPS | Tick avg ms | Tick p99 ms | Δ avg | Δ p99 |
|---|---|---|---|---|---|---|
| Default circuits (5,531) | legacy | 512.0 | 0.463 | 0.738 | — | — |
| | handshake | 512.3 | 0.556 | 1.786 | +20% (+0.09 ms) | +142% (+1.05 ms) |
| Wire-torture (1,445) | legacy | 512.3 | 0.091 | 0.207 | — | — |
| | handshake | 515.0 | 0.548 | 6.124 | +0.46 ms abs | +5.92 ms abs |
| Display-480 (7,344) | legacy | 512.0 | 0.144 | 1.794 | — | — |
| | handshake | 512.3 | 0.215 | 1.844 | +50% (+0.07 ms) | +4% (+0.05 ms) |

**Read this table against the sweep's Debug numbers** (same scenes, same
VM class): in Debug, the default scene reached only ~194 TPS in *either*
mode (tick avg ~4.45 ms) and display-480 dropped the handshake to 427 TPS.
In this **Release** run, **both modes hold the full 512 TPS target on all
three scenes**, with average tick cost ≤ 0.56 ms everywhere — roughly an
8–10× tick-cost reduction from the Release build alone. The handshake
still pays its protocol tax in relative terms (barrier waits of ~9–105 µs
mean per phase; a few hundred WAIT-overrun phases per window on the
torture scene), and its p99 spikes are real (6.1 ms on wire-torture) —
but at Release speeds the tax no longer costs a single point of achieved
TPS in the reportable round. The win condition the sweep projected
(heavy per-fire work × real cores) is still unmeasured; what changed is
that the *cost* side of the ledger shrank below the TPS budget on this
hardware.

**Run 1 (variance evidence, not the headline):** the first round ran
while unrelated compiler activity was live on the VM (a `cc1plus`
process was observed mid-round). Legacy numbers already degraded
(default avg 1.454 ms), and the handshake degraded further (default
426 TPS achieved, avg 2.183 ms, 2,512 overrun phases; torture 427 TPS,
p99 21.9 ms). This matches the sweep's documented ±30% jitter warning
and the known load-sensitivity of WAIT accounting on 2 cores; run 2,
on a quiet box, is the measurement to quote.

### Frame render cost (capture route)

The perf harness exposes no render-timing section, so the only frame-cost
proxy on this GPU-less VM is the capture route: `biome_capture` produced
all 7 frames (6 biome scenes at 1280×720 + the 256×256 overview map) in
**≈2.6 s total wall time — ≈370 ms per frame end-to-end, including
resource load, full worldgen of each scene, meshing, rasterization, and
PNG write**. Pure raster cost is a fraction of that figure and is not
isolated by this tool. **This is a capture-route timing, not gameplay
FPS**; no frame-rate limiter exists in the tree yet (see §8, item 9).

## 7. Deltas and flake record

### Vs the fix-program end state (this morning)

| Check | Fix-program end | This run | Delta |
|---|---|---|---|
| Build | exit 0 | exit 0, 0 warnings | none |
| Selftest legacy | 958 / 0 | **958 / 0** | none |
| Selftest handshake | 958 / 0 | **958 / 0** | none |
| `--handshake-test` | OK | **OK** | none |
| Scenario hash | `faec0f02700d98c6` | **`faec0f02700d98c6`** | none |
| ctest | 48/48 serial | **48/48 serial and 48/48 `-j2`** | none (parallel flake absent this time) |
| Handshake default | OFF | **OFF** (unchanged; JD's call) | none |

### Load-flake record (honesty section)

Two first-pass anomalies occurred during this run, both traced to **my own
scheduling**: the runtime signalled the handshake-mode selftest complete
~20 s before the process actually exited, and my next command overlapped
its final phase on the 2-core box.

1. **Handshake-mode selftest, first pass: 956 PASS / 2 FAIL** — the two
   failures were the `game ui: script bindings` pair. These are the exact
   checks the fix report's baseline notes document as load-flaky ("2
   game-ui script-binding checks … flake under load"), observed at the
   pre-program baseline too. **Solo re-run on a quiet box: 958 / 0,
   "ALL SELFTESTS: OK".**
2. **`--handshake-test`, first pass: FREEZE e2e check failed** (the check
   asserting the watchdog's SimCorruptionReport was logged). The report
   text is present in that run's own log — the watchdog tripped and froze
   correctly; the check's log-capture raced under the same overlap. This
   binary has a documented load-sensitive history (in the full code
   review it failed one attribution check under load — finding CH-H4;
   the fix program's P0a verification ran it 5/5 clean plus 1 under load).
   **Solo re-run: exit 0, "HANDSHAKE TESTS: OK".**

Neither anomaly reproduces solo; neither is a code regression. They are
recorded here so the file is complete.

**Perf-matrix process note:** the 12-run perf matrix was interrupted once
by an environment hiccup after 4 runs (the session died mid-fifth-run; no
results were fabricated from the partial run — its log ends mid-save).
The remaining 8 configurations were relaunched as a detached matrix and
completed; the 12 JSONs in `~/workspace/test-run-2026-10-13/perf/` are
all complete, well-formed harness outputs.

### Vs the sweep (experiment tree, Debug)

- Selftest 891/6 → **958/0**: the six known failures are gone (four were
  test-comparator bugs, chipor/scout/mesher were test bugs, Vulkan now
  SKIPs), and the suite grew by ~67 checks from the fix packs.
- ctest 42/46 → **48/48**.
- Scenario hash: **unchanged** across Debug→Release, experiment→production,
  pre-fix→post-fix.
- Perf comparison is in §6 (Release vs Debug — read the caveat there).

## 8. Open list carried into Cortana's return (Thu 2026-10-15)

From the fix report's deferred items — nothing below was touched by this
run; all are JD-decision or content items:

1. **Timing Handshake production enable** — lifecycle-safe, still default
   OFF. Flipping it is JD's call.
2. **Release signing key** — JD's offline action; Release correctly
   refuses to boot on the retired test key until then.
3. **MCP live stdio smoke** — wiring selftest passes 11/11; a live-binary
   smoke needs the real keypair (or a Debug build).
4. **H2 interop** — the offer is {3}-only, fail-closed, now documented
   accurately. Installing {1,2,3} + writing the v1 codec is JD's product
   decision.
5. **Fragment UI** — needs-list at
   `game/LithoKraft/docs/FRAGMENT-UI-NEEDS.md` (7 missing pieces + 4 JD
   decisions). No UI half-built; the Fragment still cannot be opened
   in-game.
6. **B-M1 column eviction** — deferred with blocker (no ChunkMap erase /
   SignalSim un-index / mesh-retire seams); quantified growth +
   5-step design in the fix program's `p3-2-log.md`. JD go/no-go.
7. **Block `density` values** — projectile pierce is implemented but no
   block sets `density` yet (all 0); a content call.
8. **Dimension content** (Cosmic / Underwafer / Void) — needs-list in
   `dimensions.h`; the loader vocabulary cannot express them yet. The
   Cosmic wiring call remains JD's.
9. **D-M3 remainder** — `render.fps_cap` game-side wiring (engine pacer
   landed, default OFF; recipe in `frame_pacer.h`). No frame-rate
   limiter is active anywhere yet.
10. **B-M5 long-term** — world-position probe fix would change generated
    terrain at chunk borders; needs its own workstream.
11. **F-M4 ownership enforcement** — format validation landed;
    per-registrant enforcement awaits caller identity in the mod loader.

*Run executed 2026-10-13 under JD's 12:11 EDT directive, ahead of
Cortana's return on 2026-10-15. Logs in `~/workspace/test-run-2026-10-13/`.*

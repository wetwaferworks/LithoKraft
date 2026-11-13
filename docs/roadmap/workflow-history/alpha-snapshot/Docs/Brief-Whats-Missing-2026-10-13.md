# Brief 2 — What's Missing / Not Wired: The Dotted-Dash Manifest

**LithoLattice / LithoKraft — 2026-10-13**
**Audience:** JD (mobile review) and Cortana (Thursday 2026-10-15 session).
**Purpose:** This brief is the **work order** for the two 2-person wiring teams JD is about to green-light. It is the definitive inventory of everything that is verified-but-unported, declared/stub/unwired in production, or blocked on a JD decision. If it is not listed here, it is not known to be missing; if it is listed here, it is missing in exactly the way described — no more, no less.
**Convention (from the 2026-10-13 UML set):** solid = implemented and wired; **dashed = declared / stub / not built**. This brief is the text form of every dashed box in `UML-Update-Notes-2026-10-13.md`.

**Sources cited by file name throughout:** `UML-Update-Notes-2026-10-13.md` (missing-callouts table — the backbone of this brief); `Status-Report-Extensive-Test-Run-2026-10-13.md` §8; `Code-Review-Fix-Program-2026-10-13.md` (deferred list, 12 items); `Control-Serialization-2026-10-13.md` §10; `IDemoRecord-2026-10-13.md` §8–§9; `Model-Dithering-2026-10-13.md` §6; `Software-Renderer-AA-2026-10-13.md` §7; `Ascii-Systerian-Renderer-2026-10-13.md` §6; `Worldgen-Completeness-2026-10-13.md` §10; `game/LithoKraft/docs/FRAGMENT-UI-NEEDS.md`; column-eviction design in `~/workspace/fix-program-2026-10-13/p3-2-log.md` (item B-M1).

## Summary — counts per group

| Group | Meaning | Count |
|---|---|---|
| **(A) Verified-but-unported** | Complete, gated green in an experiment tree; production untouched; awaiting JD's port word | **9 port units** under 4 headings: 1 combined control + demo port decision (18 files), 3 independent render phases, 4 worldgen groups (a)–(d) |
| **(B) Declared / stub / unwired in production** | Code, design, or plumbing exists in the production tree, but no production caller, registration, pass, or content completes the circuit | **14 items** (B1–B14) |
| **(C) Decision-blocked** | Cannot be wired by anyone except JD deciding; wiring teams must not self-decide any of these | **6 items** (C1–C6) |

Groups A + B are the wiring teams' scope once JD green-lights them. Group C is explicitly **out of their authority** (see closing note). Several items appear in two groups by design — e.g. worldgen water (A(d) ports the parameters; B13 is the game-content dependency that makes them live), and column eviction (B2 is the engineering item; C6 is the go/no-go that gates it).

---

## (A) VERIFIED-BUT-UNPORTED — experiment builds awaiting JD's port word

All items in this group share the same discipline: built and verified **only** in an experiment copy; the production tree at `~/workspace/litholattice` was verified byte-identical against the fix-program final manifest (`~/workspace/fix-program-2026-10-13/manifest-final.md5`) at the end of each run. "Port" means copy the listed files to the same paths in production, rebuild `build-linux`, and re-run that phase's gates in production before calling it landed. No item in this group requires design work — it requires a port decision and a careful copy.

| # | Item | Experiment location | Verification evidence | Port shape |
|---|---|---|---|---|
| A1 | **Control serialization** — serialized control stream coupled PRE/POST to the Timing Handshake; every sim-affecting input (Edit / Ray / PlayerChunk / Locomotion / ControlFrame) sequenced and stamped at the world tick | `~/workspace/experiments/control-serialization/`; report `Control-Serialization-2026-10-13.md` §10 | **978 PASS / 0 FAIL in both modes** (958 standing + 20 new `control:` checks); determinism hash **`5041cf1795c33a8c` across modes** (legacy, handshake 2-worker, handshake 4-worker — identical); standing scenario untouched at `faec0f02700d98c6`; latency **bounded to the phase boundary** — paced p50 ≈ 0.99–1.05 ms (≈½ tick at 512 TPS), burst of 64 fully applied within one tick; `--control-test` OK (36 checks) | **13 files: 6 new, 7 modified** — full list below |
| A2 | **IDemoRecord** — demo recording / replay / analysis (LLDM container) on top of A1 | Same experiment tree as A1; report `IDemoRecord-2026-10-13.md` §8 (manifest), §9 (not built) | **1009 PASS / 0 FAIL** (978 + 31 new `demo:` checks); **four-arm hash proof** — recorded session, no-recorder session, legacy replay, and handshake replay all **`5041cf1795c33a8c`**; recording is non-perturbing and replay is mode-independent; `--demo-test` OK; production untouched (1,618 files, 0 mismatches) | **Combined with A1 as ONE port decision: 18 files total (10 new + 8 modified)** — full list below. Do not port A2 alone |
| A3a | **Render Phase 1 — Model dithering** — near-camera screen-door fade (IGN pattern), band default 2–8 blocks; cvars `render.model_dither` (setting default on), `render.model_dither_near`, `render.model_dither_far` | `~/workspace/experiments/model-dither/`; report `Model-Dithering-2026-10-13.md` §6 | 958 PASS / 0 FAIL both modes; scenario `faec0f02700d98c6` ×4 arms; ctest 48/48 effective; stock biome frames MD5-identical to production baseline (nothing in band at biome-camera distance). GL33 path is source-complete but runtime-unverified (no GL context on the VM — standing limitation) | Independent file set (§6 of the report) — can port phase-by-phase |
| A3b | **Render Phase 2 — Software-renderer AA suite** — `render.aa` = `off \| fxaa \| cmaa \| ssaa2x`; `cmaa` (coverage restoration, CMAA-lineage) is the flagship; **default stays `off`** until JD picks | Same tree as A3a; report `Software-Renderer-AA-2026-10-13.md` §7 | 958/0 both modes; scenario hash unchanged; engine checks pin fxaa/cmaa edge softening with flat interiors bit-untouched, cmaa blending nothing on an exactly aligned edge, ssaa resolve bit-exact; `off` frame is the untouched raster | Independent file set (§7 of the report) — can port phase-by-phase |
| A3c | **Render Phase 3 — ASCII + Systerian renderer** — glyph-grid post stage, two glyph sets; cvars `render.ascii` (default off), `render.ascii_cols` (40–400, default 160), `render.ascii_set` (`ascii \| systerian`) | Same tree as A3a; report `Ascii-Systerian-Renderer-2026-10-13.md` §6 | 958/0 both modes; scenario hash unchanged; ctest 48/48 effective; engine checks pin grid geometry, ramp mapping, and the exact Systerian value-grid mapping (mid-grey → `58`, white → `64`, per the located map in `engine/src/systerian/include/litholattice/systerian/isysterian.h`) | Independent file set (§6 of the report) — can port phase-by-phase |
| A4a | **Worldgen (a) Parameterization** — ocean/lake caps (196/98) demoted from engine law to settings defaults; `oceans.min_solid_below`, `oceans.min_depth` as real fields | `~/workspace/experiments/worldgen-completeness/src`; report `Worldgen-Completeness-2026-10-13.md` §10(a) | Whole worldgen run: **1020 PASS / 0 FAIL both modes** (958 baseline + 62); scenario `faec0f02700d98c6` ×4 arms; water module 73 PASS / 0 FAIL; explicit-196/98 generator byte-identical to engine-default generator; `max_depth: 300` loads and generates strictly deeper water | Engine water files + loader hunks + `PROCGEN.md` + game data `wafer.json` / `organic.json` (inert sections — see B13) |
| A4b | **Worldgen (b) Carvers** — CheeseCarver (3D density-field), SpaghettiCarver + NoodleCarver (two-noise boundary family), RuinsCarver open mode | Same tree as A4a; report §10(b) | Included in the 1020/0 gates above; ruins default-mode hash pinned to pre-change golden `8d0168dd575353a1` and passing — shipped pipelines deliberately unchanged | `stages.h` / `stages.cpp` + carver hunks of `generator_loader.cpp` + carver hunks of `procgen_selftest.cpp`. No game-data changes |
| A4c | **Worldgen (c) Noise** — noise-library completion (cellular noise, graph nodes, modifiers) | Same tree as A4a; report §10(c) | Included in the 1020/0 gates above (38 of the new checks are Item 3) | `modifiers.h/.cpp`, `noise.h`, `cellular_noise.cpp`, `graph_nodes.cpp`, `graph.h` + noise/graph hunks of `procgen_selftest.cpp` |
| A4d | **Worldgen (d) Credits / docs / tooling** | Same tree as A4a; report §10(d) | Credits ledger at `~/workspace/experiments/worldgen-completeness/credits-ledger.md` (experiment tree) | `game/LithoKraft/CREDITS.md` (game data); optionally the capture tool `worldgen_capture.cpp` (new) + its app `CMakeLists.txt` hunk. The Backrooms design spec (`~/workspace/worldgen-completeness-2026-10-13/backrooms-design.md`) is a program document, **not** a port item |

### A1 — Control serialization: the 13-file port list (`Control-Serialization-2026-10-13.md` §10)

Copy from `~/workspace/experiments/control-serialization/` to the same paths in `~/workspace/litholattice/`, rebuild, re-run §6 gates (production selftest becomes 978; scenario stays `faec0f…`).

**New (6):**

- `engine/src/sim/include/litholattice/sim/control_stream.h`
- `engine/src/sim/control_stream.cpp`
- `game/LithoKraft/src/app/control_stream_test.h`
- `game/LithoKraft/src/app/control_stream_test.cpp`
- `game/LithoKraft/src/app/control_latency.h`
- `game/LithoKraft/src/app/control_latency.cpp`

**Modified (7 listed entries, 8 files — `handshake_test.*` is two files):**

- `engine/src/sim/CMakeLists.txt` — `control_stream.cpp` added to `litholattice_sim`.
- `engine/src/sim/include/litholattice/sim/sim_thread.h` — `setPlayerChunk` / `setRay` / `edit` become stream producers; `controlStream()` accessor; `applyEdits` → `applyControlEvents` + `applyOneEdit`.
- `engine/src/sim/sim_thread.cpp` — producers, PRE/POST wiring, paused-path stamp semantics (§1.3 of the report — the one semantic reviewers must double-check at port time).
- `game/LithoKraft/src/app/CMakeLists.txt` — the two harness `.cpp` files join the `lithokraft` target.
- `game/LithoKraft/src/app/main.cpp` — `--control-latency`, `--control-test`, `--control-scenario TICKS legacy|handshake|baseline` flags.
- `game/LithoKraft/src/app/selftest.cpp` — `fails += runControlSelfTest();` after the handshake section.
- `game/LithoKraft/src/app/handshake_test.h` / `handshake_test.cpp` — exports `ControlScenarioWorld` / `buildControlScenarioWorld()` / `hashControlWorld()` wrappers; no behaviour change.

**Conflict risk (per §10): low.** The sim files were last touched by the fix program (P0a/P1); no other crew is working in `engine/src/sim` concurrently.

### A2 — IDemoRecord: the combined 18-file manifest (`IDemoRecord-2026-10-13.md` §8)

**Port A1 + A2 together, in a single decision.** The demo system is meaningless without the stream beneath it. **Four files are shared and must be ported once, with both lanes' additive hunks:** `engine/src/sim/CMakeLists.txt`, `game/LithoKraft/src/app/CMakeLists.txt`, `game/LithoKraft/src/app/main.cpp`, `game/LithoKraft/src/app/selftest.cpp`. Deduplicated totals: **10 new + 8 modified = 18 files.**

**New (10)** — the 6 control files above, plus 4 IDemoRecord files:

- `engine/src/sim/include/litholattice/sim/idemo_record.h` — `IDemoRecord` / `DemoRecorder` / `DemoPlayer`, LLDM format contract, `DemoError`, `parseDemo`, analysis queries.
- `engine/src/sim/demo_record.cpp` — recorder/player implementation, fail-closed parser with torn-tail salvage, trailer build/verify.
- `game/LithoKraft/src/app/demo_record_test.h`
- `game/LithoKraft/src/app/demo_record_test.cpp` — 31 selftest checks, 12 `--demo-test` SimThread proofs, `--demo-dump` / `--demo-verify` CLI.

**Modified (8)** — the 8 control-modified files above; the last four also carry IDemoRecord hunks: `engine/src/sim/CMakeLists.txt` (adds `demo_record.cpp` beside `control_stream.cpp`); game app `CMakeLists.txt` (adds `demo_record_test.cpp`); `main.cpp` (adds `--demo-test`, `--demo-dump FILE [--ticks LO HI]`, `--demo-verify FILE`); `selftest.cpp` (adds `fails += runDemoSelfTest();` after `runControlSelfTest()`).

After the copy: rebuild and re-run §6 gates in production (selftest becomes **1009**; `--demo-test` OK; scenario stays `faec0f…`; the proof demo's four hashes stay `5041cf1795c33a8c`).

**Explicitly not in A2** (per §9): no playback UI (no in-game player, transport controls, or timeline — replay is programmatic via `submitInto` and inspectable via CLI); no demo compression (segments are raw LLCS bytes; the 2,048-tick proof record is 398 bytes, so none is needed at proof scale). The game-side taps gap is B12 below.

### A3 — Render suite: port rules (Phases 1–3)

- Each phase's report carries **independent production-port instructions and an independent file set** — the phases **can port phase-by-phase**, in any order JD chooses. The shared test wiring serves all three phases and is ported **once**: `engine/src/render/include/litholattice/render/software_test.h` (new), `engine/src/render/software_test.cpp`, `engine/tests/CMakeLists.txt`, `engine/tests/engine_tests.cpp`. Note `renderer.h` is touched by all three phases in adjacent hunk regions (Phase 1: `setModelDither`; Phase 2: `setAaMode`; Phase 3: `setAsciiMode` + `setAsciiGlyphSet`) — if porting all three, take the whole file once; if porting one, take only that phase's virtuals.
- **The `biome_capture` lab does NOT port in any phase.** It is experiment-only evidence tooling; production's capture tool stays stock. This is stated in all three phase reports and is not optional.
- Vulkan is not plumbed for dithering (no push-constant/descriptor path for the band uniforms yet; the interface default is an honest no-op) — porting Phase 1 does not change that; see also B5.
- After any phase ports, re-run the standard gates (build, selftest both modes, ctest, scenario hash, stock captures) before calling it landed.

### A4 — Worldgen: port rules (groups (a)–(d))

- Each group is **independently portable**, gated by the same commands (build, both selftest modes, handshake arms, ctest). Paths are relative to the tree root; where a checkout has the `src/engine/LithoKraft` symlink, port each file once.
- **Port order note (per §10):** (a) before (b) if porting selectively — `generator_loader.cpp` is shared (water hunks in (a), carver hunks in (b)); porting the whole file from the experiment tree carries both at once and is the lower-risk path, since a recursive diff verified the listed files are the **complete** difference between the trees.
- **Cross-reference:** the shipped `oceans` / `lakes` sections in (a) are **inert by construction until the game registers a water block** — that is a game-content dependency, tracked as B13. Porting (a) changes no generated output (all 7 default-config frames byte-identical to the production baseline); it makes the parameters real so B13 can make them live.
- Also designed-but-not-built in the same report, and therefore **not** in Group A: aquifers (§6 — design sketch only, deferred) and the Backrooms dungeon (§4 — design spec only).

---

## (B) DECLARED / STUB / UNWIRED IN PRODUCTION

For each item: where it lives, what "wired" means **concretely** (the missing caller / registration / pass), and a risk/effort note. Ordering follows the UML callouts table, then the fix-program deferred list, then the experiment reports' not-built sections.

### B1. Fragment UI — entirely absent; `FragmentState` has zero production callers

- **Where it lives:** Needs list `game/LithoKraft/docs/FRAGMENT-UI-NEEDS.md` (7 missing pieces + 4 JD decisions — decisions are C3); fix-program deferred #5 (G-M6); UML game diagram, Fragment cluster (dashed).
- **What exists and works (verified, selftest-covered — do not rebuild these):** the Fragment item (registered, granted day one in hotbar slot 0 via `grantStartingItems`, persisted with the inventory in player.dat v4, HUD icon/name); `FragmentState` (`game/fragment_state.h` — tier, markdown-unlock flag, `upgradeFragment()` as the single upgrade entry point, LFS1 serialize/deserialize, fail-closed); the markdown renderer (`fragment_markdown.*`, takes a plain `bool markdownEnabled`); `FragmentGallery` (`game/fragment_gallery.h`, wired exactly once, in Game's screenshot path).
- **What "wired" means — the 7 needed pieces, verbatim in substance from `FRAGMENT-UI-NEEDS.md`:**
  1. **A Fragment book dialog in GameUi.** No dialog/page exists in `game_ui.cpp/.h`. Needs an open/close flow, pages for the journal / unlock log / recipes / chip archive the header lore describes, and the tier display ("displayed on the Fragment" per the design).
  2. **A production owner + caller for `FragmentState`.** Nothing in Game owns an instance (grep-verified: zero production callers). Needs Game (or Player) to own one, `upgradeFragment()` to get a real caller, and the markdown renderer's `markdownEnabled` fed from `markdown_unlocked()`.
  3. **The Spark, or an interim trigger.** The design's upgrade trigger (find the Spark) is "not yet in code" per the header itself. Needs the Spark encounter/event, or the interim trigger JD accepts under C3.
  4. **Per-item use dispatch.** There is no production handler for "use the selected item" (see also B1a below). The dialog's open verb depends on this seam, or on a dedicated key binding that bypasses it — JD's call under C3.
  5. **Persistence for `FragmentState`.** Serialize/deserialize exist but there is no save slot: player.dat carries inventory/stats/vitals, not the Fragment blob. Needs a player.dat block (next version bump) or a field in the stats/vitals-style named blocks, plus load-side wiring. Without it, an unlocked Fragment re-locks on relaunch.
  6. **A gallery viewer (+ a persistence decision).** Screenshots land in an in-memory gallery nothing displays. Needs a gallery page in the book dialog (piece 1) and the C3 decision on whether captures persist.
  7. **Threading discipline (documented, not code):** `FragmentState` is a plain data object with no synchronisation; its future callers must be game-thread only, like the rest of Game-side state. Now stated in `fragment_state.h`.
- **Risk/effort:** Medium effort, low technical risk — every model underneath is finished and tested; the work is UI + one owner + one save block. The risk is sequencing: pieces 1, 4, and 5 cannot be finalised until C3's four decisions land. Until then, every doc/README claim must read: the Fragment is the starting item and its state model + markdown renderer exist and are selftest-covered; **the book cannot yet be opened in game**.

#### B1a. Per-item use handlers — absent as a class (incl. SeedOR till)

- **Where it lives:** `FRAGMENT-UI-NEEDS.md` item 4 (Appendix G H2b); UML game diagram, content cluster (dashed missing seam).
- **What "wired" means:** Items today are registry data + models + icons. Wired = a production dispatch for "use the selected item" plus per-tool world-interaction handlers — the review names SeedOR's Till explicitly as likewise absent. The Fragment dialog (B1 piece 4) is one consumer; tool behaviour is the general case.
- **Risk/effort:** Medium. This is a new seam, not a bug fix — design the dispatch once, for all items, rather than bolting a Fragment-only verb on. B1 can bypass it with a dedicated key only if JD so decides under C3.

### B2. Column eviction / unloading — not built; memory grows without bound

- **Where it lives:** Design + quantified growth + blockers in `~/workspace/fix-program-2026-10-13/p3-2-log.md`, item **B-M1** (deferred-with-blocker); fix-program deferred #1; status report §8 item 6; UML game diagram, world cluster. Go/no-go is C6.
- **Verified state:** `ChunkMap` (`world/chunk_map.h`) has **no erase/remove API at all** (only `insert()` / `columns()` — grep-verified). Streaming is insert-only by construction. **Quantified growth (measured in the p3-2 run):** `sizeof(SubChunk) = 12,448 B` plus palette heap (~4–8 KiB per present sub-chunk) ⇒ ~16–21 KiB per present sub-chunk; a typical surface column carries ~5–7 present sub-chunks ⇒ **~85–145 KiB of voxel data per column**; at the default stream radius (8 ⇒ 289 columns) that is ~25–42 MB per loaded area, **never released**; a session that explores ~10,000 columns carries **~0.85–1.45 GB** of voxel data alone — plus one retained mesh per present sub-chunk, `ChunkStreamer::requested_` growing one entry per column ever requested, and `WorldStore` keeping every touched `RegionFile` (and all its records) in memory for the session. Growth is linear in explored area, unbounded.
- **What "wired" means — all three seams, designed together (a partial eviction leaks sim state or meshes and must not be landed):**
  1. `ChunkMap::erase(cx, cz)` **+** `SignalSim::unscanColumn(cx, cz)` — SignalSim today has no per-column un-index (`scan()`/`scanColumn()` only add cells; the only teardown is a full `scan()` clear). Evicting without unscanning leaves the sim ticking ghost components over air, and a later save would export their SGNL state for a column that no longer exists. SGNL export for the column is taken **before** unscan.
  2. Per-column dirty/generation counter on `ChunkColumn` (set by `ChunkMap::set/setState`) — today there is no cleanliness tracking, so even a clean-column-only eviction cannot distinguish a pristine, regenerable worldgen column from an edited one. The counter enables the clean-only fast path: unedited, component-free columns evict with no save.
  3. Save-on-evict for dirty columns, through the existing `WorldStore` + per-column SGNL path (`loadColumn`'s inverse), in the same sim-stopped window discipline as insertion — and bounded per pump, mirroring the B-M2 insertion budget (4 columns per pump, already landed for insertion).
  4. A renderer retire call keyed `(cx, sy, cz, epoch)` so meshes die with their column — today `Game::pumpStreaming` submits mesh snapshots via `submitMesh_` and there is no retire/un-mesh call in the game/streamer seam.
  5. Eviction radius = stream radius + a hysteresis band (the log proposes e.g. +2 chunks) to avoid thrash at the boundary; `requested_` entries for evicted columns are removed so they can re-stream.
- **Risk/effort:** **Highest-risk item in Group B.** It touches world / signal / render / game and interacts directly with the world-permanence doctrine (a save-on-evict bug is a world-corruption bug). The p3-2 judgment, recorded and endorsed here, was to defer the whole item rather than land a partial eviction. Treat as its own feature workstream, not a tail fix. Gated by C6.

### B3. Dimensions Cosmic / Underwafer / Void — not registered

- **Where it lives:** `game/LithoKraft/src/game/include/lithokraft/game/dimensions.h` header ledger (P2c, G-M5) + needs list in `dimensions.cpp` (`createDimensions`); fix-program deferred #6; status report §8 item 8; UML game diagram, world cluster (dashed).
- **Verified state:** Exactly **two** dimensions are registered — **Wafer and Organic**. Cosmic / Underwafer / Void are not registered, have no worldgen JSON, and `map <id>` answers "unknown dimension" for them. They were deliberately **not** registered in P2c because none can be generated honestly from the existing worldgen stage vocabulary (heightmap / biome_heightmap / density terrain, current carvers and features) without inventing new mechanics or content.
- **What "wired" means, per dimension (from the `dimensions.h` ledger):**
  - **Cosmic:** its own generator content at the full 768 height with **the Empty** as its medium (no atmosphere), space traversal (magnetic boots / free descent into Organic), satellite content (Recons, Lattice Bridge), and progression gating behind the Underwafer — none of which exists as stages, blocks-in-worldgen, or game systems.
  - **Underwafer:** its own generator content (the Sear, hypervisor-ore depth progression, its portal link from the Wafer) — no JSON, no stages for any of it exist.
  - **Void:** infinite extent, infohazard / Hypervisor mechanics, and no terrain content at all — an "empty air" registration would be a falling-forever trap, not the Void.
- **The Void is a design problem, not a wiring task — flag it as such.** Its locked extent is **infinite**, but `packPos` keys pack **21 bits per horizontal axis** (x/z ∈ [−2²⁰, 2²⁰−1], y ∈ [0, 1023]; outside that range keys alias and unrelated cells silently share signal/light/dirty state — B-L2, closed in P3-2 with streamer-side enforcement). The locked finite bounds all fit with an order of magnitude to spare (Underwafer ±131,072; Organic ±196,608; Cosmic ±262,144). The Void cannot be registered until JD gives it a finite in-range bound or a windowed/virtualised coordinate scheme that never feeds unbounded coordinates to `packPos`. **The wiring teams must not invent either.**
- **Adjacent fact (do not "fix"):** MCP pilot numbering in `mcp_player_control.cpp` already addresses dimensions that don't exist (underwafer=2, cosmic=4, void=5, ether=6, undefined=7). That is forward-looking addressing only; those dimensions cannot be entered until they exist in `createDimensions`.
- **Risk/effort:** Large, and mostly **content/design**, not wiring. Cosmic and Underwafer are each a generator-content workstream plus their gating systems; the Void is blocked on the coordinate decision above. No team should register a placeholder dimension to make the count look right.

### B4. Networking — codec + handshake only; no socket transport; MCP live transport is stdio only

- **Where it lives:** UML engine diagram, net cluster (dashed "socket transport — NOT BUILT"); fix-program deferred #8 covers the protocol half (C2 here).
- **Verified state:** `engine/src/net` contains the codec + the v3 ConnectHandshake state machine and **zero socket code** (grep-verified). The 2026-10-11 diagram's "platform services" net box implied a working transport; there is none.
- **What "wired" means:** A socket transport under the existing codec/handshake — listen/connect, byte framing into the codec, connection lifecycle, and the error/close paths — plus whatever session layer JD scopes for it. Until it exists, there is no networked play of any kind, in either direction.
- **MCP, stated precisely:** **MCP stdio is the only wired agent transport** (`--mcp-stdio` in `main.cpp`). The engine MCP library also carries an HTTP transport, but **no live MCP session transport beyond stdio is built/wired into the game**. Do not describe MCP as networked.
- **Risk/effort:** Medium–large, and security-sensitive (first network surface). The protocol question (which versions the handshake offers) is **not** part of this item — it is C2, JD's alone.

### B5. Vulkan — no sprite pass; software renderer is the default backend

- **Where it lives:** `render_vk/vk_renderer.cpp` ("Vulkan has no sprite pass (P1-RENDER H4)" + log-once warning when sprite quads arrive); UML engine diagram, sprite/render clusters (dashed box).
- **Verified state:** The sprite system itself is complete (registry, deterministic `SpriteSystem`, runtime atlas, billboard `emitQuads`) and renders on the software path (verified end-to-end) and GL33 (pass compiles; runtime look unverified — no GL context on the VM). **Vulkan has no sprite path at all** — it warns once and the quads go nowhere.
- **What "wired" means:** A Vulkan sprite pass in `vk_renderer.cpp` consuming the same emitted quads (cutout, then blend/additive sorted far-to-near), so sprite content is visible on the Vulkan backend instead of silently absent.
- **Risk/effort:** Medium, backend-isolated. Not on the critical path while the **software renderer is the decided default backend** (SoftwareRenderer is a full `IRenderer`; `--renderer software` is the default, with loud GL33 fallback) — but any future Vulkan-default decision is blocked on this pass.

### B6. AiWorld — complete library, never wired into the game

- **Where it lives:** UML engine diagram, AI cluster (dashed outline); engine `ai` target (AiWorld / Brain / Nav / Senses).
- **Verified state:** A complete, selftested library with **no live-game integration**. Grep-verified: the only construction of `AiWorld` outside tests is in `app/perf_headless.cpp` — **the perf harness**. Nothing in the live game constructs it, ticks it, or feeds it a world.
- **What "wired" means:** Game-side ownership (a Game/SimThread-adjacent owner), per-tick driving from the sim at the correct phase, world/senses feeds from the live world (not the harness's fixture world), and whatever spawn/lifecycle path JD scopes for agents in the live game.
- **Risk/effort:** Medium. The library risk is already retired (selftests green); the risk is integration semantics — tick phase and world access must respect the deterministic-sim boundary, the same discipline as the control stream (A1).

### B7. `render.fps_cap` — engine pacer exists; game-side wiring pending

- **Where it lives:** Fix-program deferred #3 (D-M3 remainder); status report §8 item 9; UML engine diagram, render cluster. Recipe written in `engine/src/render/include/litholattice/render/frame_pacer.h` (`FramePacer`).
- **Verified state:** The engine frame pacer **landed, default OFF**. The game side does not drive it: there is **no frame-rate limiter active anywhere yet** (pacing today is vsync-only).
- **What "wired" means (the recipe in `frame_pacer.h`):** the `render.fps_cap` cvar + a `Settings` field + main-loop pacer drive (construct/configure the pacer from settings, call it at the frame boundary, apply live on settings change) — the same apply-at-init + on-change pattern the render phases in A3 use for their cvars.
- **Risk/effort:** Small and mechanical — the recipe exists and the engine half is done. The only judgment left (the shipped default value, if any) follows the standing performance doctrine: the targets are stable 30/60 FPS and frame-time variance, not raw FPS.

### B8. Projectile density — plumbing exists; no block assigns `density`

- **Where it lives:** Fix-program deferred #10; status report §8 item 7; UML engine diagram, projectile cluster.
- **Verified state:** The projectile system is a real library (ProjectileManager spawns/ticks, loads chunks ahead of path) and its pierce logic **reads block `density`** — but **no block sets `density`** (all values are 0), so pierce behaviour is uniform regardless of material.
- **What "wired" means:** Content, not code: assign a `density` value to each block definition where pierce resistance should differ, through the block registry's normal property path. No engine change is needed or wanted.
- **Risk/effort:** Small mechanically; the values themselves are a **content/balance call** — the wiring team should assign them only from a JD-approved value table, not invent them per block.

### B9. Certificate signing mechanic — pending

- **Where it lives:** `components/certificate_table.cpp` ("when signing lands, this class grows a SimRole"); `blocks.h` ("signing station (block only; mechanic pending)"); UML game diagram, machines cluster (dashed).
- **Verified state:** The Certificate Table exists as a block/station; the **signing mechanic does not**. The design exists in lore (Certificate of Knowledge / Certificate of Intelligence); no mechanic implements it. Separately, the engine's Systerian codec — the certificate-signing script standard — exists and is verified (see A3c), and Ed25519 mod-signature verification exists and is fail-closed; neither is this mechanic.
- **What "wired" means:** The game mechanic itself: what is presented at the table, what is consumed/earned (cf. Static, the earned/spent stat), what a signed certificate is as an item/state, how it persists, and the `SimRole` growth in `certificate_table.cpp` that the code comment anticipates.
- **Risk/effort:** Medium, and design-led: the station and the script standard are ready, but the mechanic's rules are a game-design build, not a seam-closing. Do not confuse this item with C5 (the release signing **key**) — this is an in-game mechanic; C5 is release infrastructure.

### B10. EtchOR has no block; ChipOR's block is deliberately inert

- **Where it lives:** `blocks.h` ("inert (no sim role)" for ChipOR); UML structural surprises §5.
- **Verified state:** EtchOR — the lithography machine that prints chips — **has no block at all**. ChipOR — the printed chip it produces — **has a block, deliberately inert** (no sim role). The lithography loop currently lives at items/blocks level; there is **no machine behaviour class** for either.
- **What "wired" means:** An EtchOR block + machine behaviour (the wafer-in / ChipOR-out loop, including the EtchOR name field overwriting the chip's name, as designed), and a sim role for ChipOR's block beyond inert placement — scoped exactly as JD directs; the inertness is deliberate, so "fixing" it without a scoped design is out of bounds.
- **Risk/effort:** Medium–large (a machine system, not a registration). The LithographOR registry row is likewise still DISABLED (UML game diagram, blocks registry) — related, and equally a scoped build, not a toggle.

### B11. Sprite system — procgen plant textures not bound into live worldgen

- **Where it lives:** Sprite-system record (landed + verified 2026-10-12); UML engine-procgen diagram (Litho Formula / shrub cluster).
- **Verified state:** The sprite core is landed (registry — PNG bytes/file, SVG text, raw Image, first-wins, fail-closed; runtime atlas; deterministic `SpriteSystem` with splitmix64-seeded emitters, bit-exact per seed; billboard `emitQuads`). The Litho Formula shrubs are landed: procgen sprites whose seed varies **colour only, never the mask**; append-only texture family 3–6 (flower/star/leaf/blob); biome rows bound (DicingPlains→leaf, ResonatorSpires→blob, StaticMarsh→flower). **Open: the procgen plant textures are NOT bound into live worldgen.**
- **What "wired" means:** A bridge from worldgen to the shrub sprite definitions — when live generation places the relevant biome/feature content, it must emit/reference the procgen plant textures instead of (or alongside) whatever it uses today. **Which bridge is JD's call** (recorded open in the sprite record); the wiring team implements the chosen bridge, it does not choose it.
- **Risk/effort:** Small–medium once the bridge is chosen. Determinism is the constraint: any bridge must preserve the seed-varies-colour-only invariant and the bit-exact-per-seed emitter property.

### B12. IDemoRecord §9.3 gap — game-side Locomotion / ControlFrame taps not built

- **Where it lives:** `IDemoRecord-2026-10-13.md` §9 (third bullet), carried over from `Control-Serialization-2026-10-13.md` §9.3. Applies **after** A1+A2 port; listed in Group B because porting alone does not close it.
- **Verified state:** The control stream, the LLCS/LLDM formats, and the record system all **carry** all five event kinds, and the selftest round-trips all five kinds field-for-field. But **no game-side producer taps `Locomotion` or `ControlFrame` where the game consumes them**. MCP pilot input today crosses the sim boundary as Edit/Ray/PlayerChunk via the game's merged input, with source recorded as Local — pilot attribution merges upstream of the sim boundary. A live session record today therefore contains the sim-boundary kinds (Edit / Ray / PlayerChunk) **completely**, and the game-side kinds only when something submits them. **Nothing sim-affecting is missing from a record** — what is missing is attribution/completeness for pilots.
- **What "wired" means:** Two game-side taps: (1) a `Locomotion` producer at the point the game steps the locomotion body (wish direction + pose flags, mirroring `LocomotionInput`); (2) a `ControlFrame` producer in the MCP pilot path (`McpPlayerController::submitFrame` — the kind mirrors `mcp::IPlayerController::ControlFrame` field-for-field; the adapter is game-side because the sim library does not depend on the mcp library), giving per-pilot source attribution in the stream and in demo records.
- **Risk/effort:** Small–medium. Both kinds, formats, and accounting already exist and are tested; the work is two tap points plus attribution-correct source/sourceId stamping, verified by a live-session record containing both kinds.

### B13. Ocean / lake water parameters — inert in shipped generators until a water block + biome eligibility exist

- **Where it lives:** `Worldgen-Completeness-2026-10-13.md` §1 + §7 + §10(a). Cross-reference A4a.
- **Verified state:** Even after A4a ports, the shipped `wafer.json` / `organic.json` `oceans`/`lakes` sections are **inert by construction**, for two engine facts: **the game registers no water block and no sand block**, and the loader is fail-closed on unknown materials — naming a real water material today would break loading outright. The shipped sections therefore use `"water": "air"` (the documented no-op sentinel in `water.h` — the rule constructs and never writes, with an in-file comment explaining why) and **empty `biomes: {}` maps**, so no shipped biome opts in (eligibility 0). Generated output is bit-for-bit unchanged — proven, not assumed.
- **What "wired" means (two game-content steps, in order):** (1) Register a water block (and a sand block, if shore behaviour is wanted) in the game's block registry so the loader can resolve the material name; then point the sections' `"water"` field at it, replacing the `"air"` sentinel. (2) Populate the `biomes` eligibility maps so at least one shipped biome opts in — with eligibility, the parameterized rules (196/98 defaults, `min_depth`, `min_solid_below`) become live generation behaviour.
- **Risk/effort:** Small as code; **medium as worldgen behaviour** — the moment eligibility is non-empty, generated terrain changes. That step needs before/after world comparisons and JD's sign-off on the values, exactly as with B14. Do not bundle the eligibility change silently into the A4a port.

### B14. Long-term world-position probe (chunk-border structure replay)

- **Where it lives:** Fix-program deferred #2 (B-M5 long-term half); status report §8 item 10; design note in `p3-2-log.md` item B-M5 and in the `stages.h` header.
- **Verified state (accurately, per the fix record):** The **short-term half of B-M5 is closed** — the `StructureFeature` / `basesInRect` documentation now states the honest behaviour: neighbours agree **except at simultaneous chunk+biome borders**, where the replaying chunk clamps out-of-chunk bases to its nearest in-chunk column for biome weights/surface height and may resolve a different dominant biome and replay a border structure partially (rare, bounded to `reach` columns, deterministic). `basesInRect` results are documented as **candidate bases** (over-reporting by design; the genmap consumer is annotated "may generate here," not "did generate"). What is deferred is only the **long-term fix**.
- **What "wired" means:** Probe biome/height **by world position** instead of by the clamped in-chunk column, so border replay resolves the same dominant biome and surface height as the originating chunk.
- **Risk/effort:** Medium, and explicitly **not a tail fix**: it changes generated terrain at borders, i.e. it is a worldgen behaviour change needing its own workstream with before/after world comparisons. Existing worlds' already-generated chunks do not change; newly generated border chunks would differ from what the current code would have generated there.

### Also dashed, for completeness (from the UML callouts — no separate work order)

- **LightEngine — STAGED / UNWIRED** (`light_engine.h`, per P3-2 B-L5): no production callers; the live lighting path is the renderer's flood-fill ambient. Its `addSource` clamp to `kMaxLight` (64) is landed and tested; do not treat it as the world's lighting until a wiring workstream connects it.
- **Spatial audio / occlusion — scaffold, not live path** (`audio/spatial_audio.h` + `occlusion_thread.h` exist; not the game's live audio path; the review flagged the occlusion concurrency item).
- **AnvilRegion — EXPERIMENTAL, no production callers** (per P3-2 C-L3); the production region path is `RegionFile` / `WorldStore`. Its malformed-entry quarantine counting/logging is landed; it needs no wiring unless JD adopts it.

---

## (C) DECISION-BLOCKED — NEED JD, never self-decided by the wiring teams

For each: the decision, the options if known, and who/what waits on it. The wiring teams may prepare options, measurements, and recommendations for any item here; they may not decide any of them.

### C1. Timing Handshake — default-ON?

- **Decision:** Flip the Timing Handshake from **default OFF** to default ON in production?
- **Current state:** Wired but opt-in — `settings.h` `timingHandshake = false`; cvar `sim.timing_handshake`; CLI `--timing-handshake`. Lifecycle-safe after fix-program P0a / P1-WAIT / P2a (bounded-join + poisoned-authority stop semantics, WAIT overrun accounting, FREEZE watchdog); determinism hash bit-identical across legacy/handshake arms in every gate since.
- **Options:** (i) Flip the default now; (ii) keep default OFF and enable per-session/per-test; (iii) flip after a further soak criterion JD names.
- **Waits on it:** Any claim that production runs the Handshake by default; the extensive test run's open list §8 item 1. Nothing in Groups A/B is blocked by this — A1/A2 were verified in both modes precisely so this decision stays free.

### C2. H2 networking protocol — {3} only, fail-closed; any change is JD's

- **Decision:** Does the ConnectHandshake offer stay **{3} only**, or are legacy versions installed?
- **Current state:** The handshake offer is **{3}-only, fail-closed**, and (after fix-program F-L4) documented accurately as such. Installing **{1,2,3}** would additionally require **writing the missing v1 codec** — it does not exist.
- **Options:** (i) Stay {3}-only (current, shipped behaviour); (ii) install {1,2,3} + build the v1 codec as a scoped product feature.
- **Waits on it:** B4's transport design surface (which versions a transport must carry) and any interoperability claim. The wiring teams must not widen the offer as a side effect of building transport.

### C3. Fragment UI — the 4 decisions (from `FRAGMENT-UI-NEEDS.md`)

B1 cannot be finished until all four are answered:

1. **Open verb:** dedicated key, "use selected item," or both? (Determines whether B1 depends on the B1a dispatch seam or bypasses it.)
2. **Interim upgrade trigger:** accept an interim trigger until the Spark exists (cheap candidates on record: console command / certificate-table interaction) — or wait for the Spark?
3. **Persistence timing:** persist `FragmentState` in player.dat (format bump) at the same time as the dialog, or dialog first with session-only state?
4. **Gallery:** in-book viewer in v1, or capture-only until later?

- **Waits on it:** B1 pieces 1, 3, 4, 5, and 6 — i.e. effectively the whole Fragment workstream's shape.

### C4. Mod ownership — caller identity

- **Decision/problem:** Per-registrant **ownership enforcement** in the mod loader (fix-program deferred #4, F-M4) awaits a **caller identity** in the mod loader. Format validation has landed; the contract is documented at `registerBlock`; enforcement cannot be written honestly until the loader knows *who* is calling.
- **Options:** Not enumerated in the fix record — the shape of caller identity (what identity, how attested, how it interacts with the mod API) is JD's design call. The wiring teams may prototype nothing here that presumes an answer.
- **Waits on it:** F-M4 enforcement only. Registration, format validation, and the fail-closed mod-hash/signature paths are live today and blocked by nothing.

### C5. Signing-key replacement — JD's offline action

- **Decision/action:** Replace the repo's **compromised/retired test signing key** with the real release keypair — **offline, by JD personally**. No wiring team member can or should do this.
- **Current state (stated precisely, per the fix program + status report):** Release correctly **refuses to boot** on the retired test key until the replacement is installed — that refusal is intact and must stay intact. There is **no private key in the current tree or zips**. **Git history has not been cleared/verified** — that verification is part of the outstanding action, not a completed fact. Consequence on record (fix-program deferred #9): because of the T-H1 mod-hash framing fix, existing mod signatures must be re-issued when signing begins (none are in the wild).
- **Waits on it:** Release boot on a real key; the **MCP live stdio smoke** (fix-program deferred #7 / status §8 item 3 — wiring selftest passes 11/11, but a live-binary smoke needs JD's real keypair installed, or a Debug build). Debug-build work in Groups A/B is not blocked.

### C6. Column eviction — go / no-go (B2)

- **Decision:** Schedule column eviction as its own feature workstream — yes, no, or later?
- **What is and isn't decidable from the p3-2 log — stated precisely:** The log (`p3-2-log.md`, B-M1) **does** settle the engineering facts: the four blockers are verified in current code (no `ChunkMap` erase; no SignalSim per-column un-index; no mesh-retire seam; no per-column dirty tracking); the growth is quantified (~0.85–1.45 GB per 10,000 columns, §B2); a complete **5-step design** exists (erase + unscan with SGNL-export-before-unscan; dirty/generation counter with clean-only fast path; save-on-evict through the existing store path in sim-stopped windows, bounded per pump; renderer retire keyed `(cx, sy, cz, epoch)`; eviction radius = stream radius + hysteresis, with `requested_` cleanup); and the negative is settled — **no partial eviction is safe**, so there is no cheap interim version to approve by accident. What the log does **not** settle, because they are JD's calls and the log presents them as proposals ("e.g."): the **exact hysteresis band** (+2 chunks is an example, not a lock), the **exact per-pump eviction budget** (mirroring B-M2's 4 is the design's suggestion, not a decided number), and above all **scheduling/priority** against every other item in this brief. In short: **no design questions block a go decision; the open questions are parameter values JD locks at scheduling time, plus the go/no-go itself.**
- **Waits on it:** B2 in full. Until JD says go, streaming remains insert-only and session memory growth remains as quantified — that is the accepted cost of the no-go/defer state, not a defect a team may quietly cap (the p3-2 run explicitly landed no caps and no silent drops in its place).

---

## Closing note — authority boundary

**Groups A + B are the wiring teams' scope** once JD green-lights the teams and the specific items: port exactly what Group A lists (no more — the `biome_capture` lab never ports; no less — the shared files in A2 are ported once, with both hunks), and wire exactly what Group B defines as "wired" (no partial column eviction, no placeholder dimensions, no invented density values, no Fragment-only use verb that pre-empts B1a's general seam).

**Group C is explicitly out of the wiring teams' authority.** Every item there — the Timing Handshake default, the H2 protocol offer, the four Fragment decisions, mod caller identity, the signing-key replacement, and the column-eviction go/no-go — is JD's alone. A team that reaches a Group C boundary stops, records the exact question, and continues with unblocked work. Discovering that an A/B item secretly depends on a C item is a reportable finding, not a licence to decide it.

*End of brief. Companion briefs in this set cover the verified state of what **is** wired; this brief covers only what is missing, stubbed, unported, or undecided — the dotted-dash manifest.*

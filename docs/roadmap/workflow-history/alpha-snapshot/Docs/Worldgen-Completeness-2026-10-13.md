# Worldgen Completeness — Lane 3 Build Report — 2026-10-13

**Scope:** clean-room implementation of Items 1–4 of the worldgen completeness
program (Lane 1 audit + Lane 2 sweep, `~/workspace/worldgen-completeness-2026-10-13/`),
plus credits. Item 5 (aquifers) is **designed-but-deferred** — see §6.

**Where the work lives:** the experiment copy only —
`~/workspace/experiments/worldgen-completeness/src` (build tree:
`~/workspace/experiments/worldgen-completeness/build`; logs:
`~/workspace/experiments/worldgen-completeness/logs/`).

**Production was never opened for writing.** All edits, builds and test runs
happened inside the experiment tree. Verified at the end two ways:
(1) `find ~/workspace/litholattice -newermt "2026-10-13 18:30 UTC"` → **0 files**
modified during this session; (2) `md5sum -c` against
`~/workspace/worldgen-completeness-2026-10-13/manifest-pre-work.md5` from the
production root → **all 1615 listed files OK, zero mismatches**.

**Final gate results (experiment tree):**

| Gate | Result |
|---|---|
| Full build (`cmake --build -j2`) | exit 0, no warnings in any changed file |
| `./LithoKraft --selftest` | **1020 PASS / 0 FAIL** (baseline 958 → **+62**) |
| `./LithoKraft --timing-handshake --selftest` | **1020 PASS / 0 FAIL** |
| `./LithoKraft --handshake-test` | HANDSHAKE TESTS: OK |
| Scenario 2048, arms legacy 1 4 / handshake 1 4 / handshake 4 4 / handshake 4 8 | **`faec0f02700d98c6` on all four arms — unchanged** |
| `ctest -j2 --timeout 1200` (48 tests) | 47 passed in the full run; the one failure, `engine_systerian`, is the documented tree-location path assumption (it resolves `../../../../user/files/…` from its source dir); with the location shim in place it was **solo re-run → PASS**, so effectively **48/48**. `engine_audio` passed in the full run (no flake observed). |
| Default-config frames (biome_capture) | **all 7 frames byte-identical** to the production baseline — MD5 table in §7 |

Added selftest checks: **+62 in the game selftest** (Item 2: 25, Item 3: 38 —
the ±1 against the arithmetic total is a log-line interleaving artifact in
name-level diffing, demonstrated on pre-existing blend/snowcap/worley lines in
both logs) **plus +6 in the engine water module** (67 → 73 PASS), which runs
under `litholattice_tests water` rather than the game binary.

---

## 1. Item 1 — Water field rules parameterized (JD ruling: 196/98 are LithoKraft's values, not engine caps)

### What was built

- **The hard caps are gone.** `kMaxOceanDepth` / `kMaxLakeDepth` no longer exist.
  In their place, `water.h` now defines *defaults*: `kDefaultOceanMaxDepth = 196`,
  `kDefaultLakeMaxDepth = 98` (now an independent named default, no longer
  derived as `196 / 2`), `kDefaultMinSolidBelowOcean = 60`. Max depth is purely a
  settings value end to end.
- **`oceans.min_solid_below`** is a real settings field (`OceanSettings.minSolidBelow`,
  default 60) parsed by the loader; the loader's sea-floor check uses the
  parameter, not the literal.
- **`oceans.min_depth`** is a real settings field (`OceanSettings.minDepth`,
  default −1 = unset). When unset, behaviour is preserved exactly: the minimum
  depth derives as `maxDepth / 4` (the old hard-wired rule, now a documented
  derivation). When set, `depthForUnit` maps the unit interval onto
  `[minDepth, maxDepth]`.
- **Constructor clamps are sanity-only** now: `maxDepth` floors at 1; an explicit
  `minDepth` clamps into `[0, maxDepth]`. A settings value of 500 (or 100 000)
  is *kept*, pinned by tests.
- **Loader validation left standing** (all sanity or data, no caps): ocean/lake
  `max_depth` must be positive; `min_depth` must lie in `[0, max_depth]`;
  `min_solid_below` must be ≥ 0; and the one geometric rule —
  `sea_level − max_depth < min_solid_below` is rejected — because both numbers
  are now data, the rule is data-expressed and a mod can relax it by setting
  `min_solid_below` lower (proven by test: sea 200 / depth 196 loads with
  `min_solid_below: 4`).
- **Shipped generators:** `wafer.json` and `organic.json` both gained `oceans`
  and `lakes` sections carrying JD's values (196 / 98, `min_solid_below` 60).
  Two engine facts shaped how: the game registers **no water block and no sand
  block**, and the loader is fail-closed on unknown materials — naming a water
  material would have broken loading outright. So the sections use
  `"water": "air"`, which is `water.h`'s documented no-op sentinel (the rule
  constructs and never writes), with an in-file `"comment"` explaining why,
  and empty `biomes: {}` maps, so no shipped biome opts in. Generated output is
  bit-for-bit unchanged (proofs in §7). The built-in fallback JSON is
  *generated from these same asset files* at build time
  (`embed_text_files.cmake`), so there is no second copy to drift.
- `engine/docs/PROCGEN.md`: the cap-as-law text (including the
  "the cap is what makes mods safe" paragraph) rewritten as settings language.

### Required proofs (all in `water_selftest.cpp`, module total 73 PASS / 0 FAIL)

- **(a) Explicit == default:** a loader-built generator with explicit
  `max_depth` 196/98 produces **byte-identical buffers** to the engine-default
  generator over chunks (0,0), (2,1), (−1,3) on the synthetic field.
- **(b) Above the old cap works:** `max_depth: 300` **loads** (previously
  rejected at the loader) and, on the same field at sea level 400, produces
  strictly deeper water: measured maximum depth **254** at max_depth 300 vs
  **166** at max_depth 196; depth-300 floors are never shallower than
  depth-196 floors. Lake `max_depth: 300` likewise loads (was rejected at 98).
- **(c) Pin updates, line by line** — only pins whose *semantics* moved from
  cap to setting changed:
  1. `caps are 196 and 98 (JD 2026-10-11)` → split into "default ocean
     maxDepth == 196 (a default not a cap)", "default lake maxDepth == 98",
     settings-default checks, and "lake default is an independent field, not
     ocean/2". *Why:* the constants still exist as values; what changed is
     their legal status.
  2. Constructor pin `maxDepth = 10^7` clamps to 196 → replaced by
     "OceanSettings maxDepth 500 is kept (no cap clamp)" and the same for
     lakes. *Why:* the clamp *was* the cap.
  3. Constructor pin `maxDepth = 0` clamps to 196 → "sanity-floors to 1".
     *Why:* the remaining floor is positivity, not the cap value.
  4. Old "min depth exceeds maxDepth" constructor pin → "explicit minDepth
     clamps into [0, maxDepth] (sanity)". *Why:* min depth is now a real
     field with a range rule instead of a derived constant.
  5. Loader pin "ocean max_depth above the cap is rejected (197)" →
     **deleted as a rejection**; replaced by "ocean max_depth 197 loads (was
     cap-rejected)". Note the subtlety the tests now pin explicitly: 197 at
     the *default* sea level 256 is still rejected — but by the
     `min_solid_below` **data** rule (256 − 197 = 59 < 60), with
     `min_solid_below: 0` the identical JSON loads.
  6. Loader pin "sea_level − max_depth < 60 is rejected" → kept, renamed to
     name it a data rule, plus the new companion pin above (same JSON loads
     with `min_solid_below: 4`).
  7. The `depthForUnit` pins (49 at unit 0, 196 at unit 1, monotone, bounded)
     are **unchanged in value** — only re-worded to "(legacy derive)" /
     "(configured max reachable)", because with `minDepth` unset the mapping
     is arithmetically identical. New pins cover the explicit-minimum mapping
     (`depthForUnit(0, 300, 10) == 10`, `(1, 300, 10) == 300`).

### Files (Item 1)

`engine/src/procgen/include/litholattice/procgen/water.h`,
`engine/src/procgen/water.cpp`, `engine/src/procgen/generator_loader.cpp`
(also Item 2), `engine/src/procgen/water_selftest.cpp`,
`game/LithoKraft/assets/worldgen/wafer.json`,
`game/LithoKraft/assets/worldgen/organic.json`, `engine/docs/PROCGEN.md`.

---

## 2. Item 2 — JD's five carvers + the noise-cave family

### JD's-five verdict table

| Name | Verdict |
|---|---|
| Ravine | **PRESENT** before this program (`RavineCarver`, stages.h) — untouched, verified by full-suite green |
| River | **PRESENT** (`RiverCarver`) — untouched |
| Worm | **PRESENT** (`WormCaveCarver`) — untouched |
| Ruins | **PARTIAL → COMPLETED**: inlay mode preserved bit-identically; new config-gated **open mode** built (§2.3) |
| Swiss-cheese | **BUILT as the cheese system.** JD asked for Swiss-cheese by name; it is delivered as `CheeseCarver` and the capture in §8. There is no separate "Swiss-cheese" feature — cheese *is* it. |

### 2.1 CheeseCarver — 3D density-field carver (the Swiss-cheese system)

A world-anchored 3D field (OpenSimplex2S, 3-octave FBm via the engine's
fractal wrapper, frequency 1/24) is sampled per voxel; a voxel is carved where
the field exceeds the threshold (0.30). Pillar retention comes from the field
itself: sub-threshold columns are never touched. Seeding follows the engine's
discipline (dimension seed ⊕ carver-name hash ⊕ index; the field itself is
position-seeded, so neighbouring chunks agree on shared voxels — no seams).
Density gating per dominant biome works exactly as for existing carvers.
Selftest pins (synthetic flat-rock field): carved fraction **0.101** of the
band (band pinned 0.03–0.65), **286 of 1024 columns fully uncarved** (pillars
remain), surface margin 8 respected, deterministic per seed, different seed →
different field.

### 2.2 SpaghettiCarver + NoodleCarver — two-noise boundary carve, one family

Both tiers share `TubeCaveSettings` / `carveNoiseTubes`: two independent 3D
fields A and B; a voxel is carved iff A² + B² < tubeRadius² — the tube is the
intersection of the two fields' zero-crossings, so it wanders smoothly and
branches rarely. Noodle = tighter radius (0.125 vs 0.14) at higher frequency
(1/13 vs 1/20). Both names are real, separately registered, separately
selectable loader features (`"spaghetti"`, `"noodle"`). Selftest pins:
spaghetti carves **3136** cells in the band with max interior depth 1
(Chebyshev blocks-to-solid — a true thickness measure; an axis-run measure was
tried first and discarded because a tube running parallel to an axis has long
runs along its *length*), fullest 3×3×3 core 26/27 (filled, not a shell),
isolated fraction **0.0016**; noodle carves **2308** cells, fullest core 22,
isolated fraction **0.048** — measurably thinner by both ordinals. Both
deterministic per seed.

### 2.3 RuinsCarver open mode

`RuinsSettings.openMode` (default **false** — default shipped behaviour is
bit-identical, proven two ways: a pinned golden hash of default-mode output,
`8d0168dd575353a1`, captured from the pre-open-mode code path, and a
cross-pipeline regeneration equality). Open mode adds, per chunk, N buried
room volumes: template box (position-hashed placement, size in
`[open_min_size, open_max_size]`, height 3) → boundary cells carved only where
a 3D erosion-noise mask passes (ragged, eroded edges, not clean boxes) →
floor cells become the `rubble` material with `rubble_chance` instead of air
(rubble defaults to the trace material when none is named). All parameters
are JSON data: `open_mode`, `rubble`, `open_rooms_per_chunk`, `open_min_size`,
`open_max_size`, `open_height`, `rubble_chance`. The mode default also leaves
the in-chunk RNG draw sequence untouched until the open pass, which is what
keeps default mode bit-identical. Selftest pins (per chunk): open mode
creates **582 buried air cells vs 2** for inlay-only on the same chunk,
**109 rubble cells** on the new floors, no surface breach (surface margin
respected), JSON `open_mode` loads. Design note: open mode is *additive* over
the inlay pass in the same stone (inlay can line an open room's walls) — a
deliberate composition choice, stated here so it isn't mistaken for an
accident.

### 2.4 Registration & defaults

New carvers are registered in the loader's carver dispatch exactly as
existing carvers are (`cheese` / `spaghetti` / `noodle`, full settings in
JSON, fail-closed on bad values). **Neither shipped pipeline gained any new
carver** — wafer.json/organic.json pipelines are unchanged; the new carvers
are available to mods and future dimensions, off everywhere by default.

### Files (Item 2)

`engine/src/procgen/include/litholattice/procgen/stages.h`,
`engine/src/procgen/stages.cpp`,
`engine/src/procgen/generator_loader.cpp` (shared with Item 1),
`engine/src/procgen/procgen_selftest.cpp` (shared with Item 3).

---

## 3. Item 3 — Noise library completion

### Noise additions table

| Addition | Where | Contract |
|---|---|---|
| **Ping-Pong fractal** (`FractalType::PingPong`, names `pingpong` / `ping_pong`) | `modifiers.h/.cpp` | Sibling of FBm/Ridged/Billow/Turbulence. Clean-room, implementation-defined fold: per octave, `tri((n+1)·1.5)` with the period-2 triangle wave — the range folds **twice** across [−1, 1] (peaks at n = −1/3 and n = 1, valley at n = 1/3), so octaves interleave ridges instead of stacking them like Ridged's single fold. Output [0, 1]. |
| **Curl noise 2D + 3D** (`curlNoise2`, `curlNoise3`) | `modifiers.h/.cpp` | Divergence-free vector fields from noise potentials by central finite differences: 2D `(∂ψ/∂y, −∂ψ/∂x)`; 3D `∇×Ψ` over three potentials. Pure functions of (potentials, position, eps). |
| **Cellular cell-ID** (`cellId2/3/4` on `INoise`, `CellularNoise` override) | `noise.h`, `cellular_noise.cpp` | Returns the lattice hash of the F1-winning feature cell under the noise's seed; 0 for all non-cellular noise. Constant inside a cell, changes exactly at borders — per-cell randomisation alongside the existing distance/border returns. |
| **Graph nodes `abs`, `invert`, `power`** | `graph_nodes.cpp` | One-input map nodes. `power` is sign-preserving (`sign(v)·|v|^e`, `exponent` param > 0) so fractional exponents never NaN on negatives. |
| **Graph nodes `fractal`, `warp`** (standalone) | `graph_nodes.cpp`, `graph.h` | Leaf noise nodes sharing the `noise` node's builder; the defining parameter block is **required** — a `fractal` node without a `"fractal"` block (or `warp` without `"warp"`) is a compile error, not silent plain noise. Node count 18 → 23. |

### Selftests (38 new checks in the game selftest)

Range/dimensionality checks for Ping-Pong in 2D/3D/4D via the generic noise
harness; name parse/round-trip; distinct-from-Ridged; golden value pinned
from this implementation's first run (0.58929199 at the pinned sample point,
documented in-test as implementation-defined). Curl: determinism, seed
separation, finiteness/non-degeneracy over grids, and divergence-freeness
measured with the *same* central-difference stencil the construction uses —
the discrete identity where mixed partials cancel term by term; measured max
residual **2.4e-05 (2D)** / **3.6e-05 (3D)**, i.e. float rounding only; golden
vectors pinned. Cellular ID: non-zero, deterministic, seed-separating,
locally constant inside a cell, ≥3 distinct ids along a scan (14 observed),
0 for non-cellular, golden id pinned (`0x57e1faba` at the pinned position).
Graph nodes: `abs`/`invert`/`power` match their definitions **exactly on
every grid point** against the same base noise graph; `fractal`/`warp`
deterministic, finite, differ from the unmodified noise; both rejection
contracts pinned. No golden value anywhere was copied from an external
implementation's tests — every pinned constant is from this tree's first run
of this code, and the tests say so.

### Files (Item 3)

`engine/src/procgen/include/litholattice/procgen/modifiers.h`,
`engine/src/procgen/modifiers.cpp`,
`engine/src/procgen/include/litholattice/procgen/noise.h`,
`engine/src/procgen/cellular_noise.cpp`,
`engine/src/procgen/graph_nodes.cpp`,
`engine/src/procgen/include/litholattice/procgen/graph.h`,
`engine/src/procgen/procgen_selftest.cpp` (shared with Item 2).

---

## 4. Item 4 — Backrooms dungeon: design spec (document only)

Written to
[`backrooms-design.md`](sandbox://workspace/worldgen-completeness-2026-10-13/backrooms-design.md)
(in the program directory alongside the audit and sweep). Summary of what it
specifies:

- **Host:** the existing `DungeonSlot` stage — today a registered no-op that
  already ships in organic.json — named as the natural host to grow into
  `BackroomsStage`.
- **Room model:** chunk-aligned rooms on a global lattice; each room is a
  tile record (solid / wall / door / fake door / sealed door, per-edge). Door
  agreement is structural, not negotiated: a door lives on a room edge and is
  computed from the **edge hash** both neighbours share, so adjacent chunks
  can never disagree.
- **Connectivity:** per-region spanning core (seeded Kruskal MST over the
  room grid) + deliberate edge deletions at `connection_probability` +
  island placement for the `unconnected_fraction` (components of size 1–4)
  and non-Euclidean **remap links**; island fraction is *measured* by
  flood-fill and asserted, not trusted.
- **JD's four parameters as generator-config data:** `room_size_min/max`,
  `connection_probability`, `unconnected_fraction`, `flip_probability` — a
  full JSON example with suggested Undefined values (connection 0.55,
  unconnected 0.25, flip 0.2), matching JD's description: connected *and*
  unconnected, "everything gets flipped around".
- **Transforms:** flips (X / Z / vertical ceiling↔floor), rotations, and
  inversions as pure functions of (seed, chunk, room id); fake doors, sealed
  doors (exist, never traverse), duplication-with-drift (copies re-derived
  from a drifted seed — never stored); a derived door-remap table makes
  non-Euclidean exits explicit data the runtime consumes.
- **Determinism contract + nine selftest invariants** (edge agreement,
  canonical-view regeneration, island-fraction band, flip band, determinism,
  seed separation, sealed-door non-traversability, remap declaration,
  dimension-seed isolation), an implementation sketch (greedy socket
  assembler *before* WFC, with the honest reason), and the safety section:
  off by default three ways (loader fail-closed on unknown sections,
  `enabled: false` inert parse, dimension opt-in), unregistered dimensions
  untouched.

---

## 5. Constants-class outcome table

| Was | Now | Class |
|---|---|---|
| `kMaxOceanDepth = 196` (engine cap, loader-enforced) | `kDefaultOceanMaxDepth = 196`, a settings default; loader accepts any positive value | **moved to data** |
| `kMaxLakeDepth = 196 / 2` (derived cap) | `kDefaultLakeMaxDepth = 98`, independent default | **moved to data** (and de-derived) |
| `kMinSolidBelowOcean = 60` (literal in loader check) | `oceans.min_solid_below` settings field, default 60 | **moved to data** |
| Ocean min depth = `maxDepth / 4` (hard-wired in `floorTarget`) | `oceans.min_depth` field; the /4 rule remains only as the documented default derivation when unset | **moved to data**, legacy derivation preserved |
| Constructor clamps to the caps (`water.cpp:30/90/141`) | sanity-only floors (positivity, range) | **kept as sanity**, justified per-check in §1(c) |
| Cheese threshold 0.30 / frequency 1/24; tube radii 0.14 / 0.125, frequencies 1/20 / 1/13 | settings fields with those values as defaults (`cheese` / `spaghetti` / `noodle` JSON) | **born as data** |
| Ruins open-mode numbers (rooms/chunk, sizes, rubble chance) | settings fields, all JSON-exposed, mode default off | **born as data** |
| Ping-Pong fold constant 1.5; curl stencil eps default 0.01 | algorithm definition / function parameter | **stayed code** — they define the functions, like FBm's lacunarity default; both documented at the definition |

Nothing else in the engine's worldgen surface was found posing as a constant
in this program's scope; caps outside the audit's list were not touched.

---

## 6. Item 5 — Aquifers: designed-but-deferred

**Not built.** Items 1–4 are fully green, but the stretch item was not
started rather than shortchanging the gates, captures and proofs of Items
1–4. The starting point is on record: sweep §1.4's sketch (a local
fluid-level field varying per cave region — per-column aquifer level
sampled from a low-frequency field, lake-style fill inside noise caves only
where the cave floor is below the local level), and Item 1's parameterized
water rules already provide the fluid-level machinery it would sit on. The
credits ledger (§9) records the aquifer's inspiration sources so the future
build starts credited.

---

## 7. Default-behavior proofs

1. **Scenario hash:** `faec0f02700d98c6` on all four arms (legacy 1 4,
   handshake 1 4, handshake 4 4, handshake 4 8) — identical to baseline.
2. **Selftest:** both modes 1020 PASS / 0 FAIL; baseline checks all still
   pass (the only renamed pins are Item 1's, mapped line-by-line in §1(c);
   the ruins default-mode hash is pinned to the pre-change golden
   `8d0168dd575353a1` and passes).
3. **Default-config frames byte-identical** (experiment binary →
   `captures-default/`, baseline = production run in
   `~/workspace/user/files/status-frames-2026-10-13/`):

   | Frame | MD5 (identical in both) |
   |---|---|
   | capacitor_groves.png | `c03f6cd835077ee5b1b636f7a463b597` |
   | dead_zones.png | `a7a86dc8f13a20f10ec58d207b195aa0` |
   | dicing_plains.png | `7b999fea9f8926f152e59bfd83ebf156` |
   | resonator_spires.png | `83f13dd6bbecfca8f2fb3a76d18e199f` |
   | spawn.png | `4e69f7d84592f4ed324ceb615140f263` |
   | trace_forest.png | `6bb626c6b38e0b488d677cfc77ab30ba` |
   | wafer_overview_map.png | `1c52d2fa06ddb45e5d55d409c8fb2ebb` |

   One honest caveat about route: this tree's `biome_capture.cpp` (inherited
   as-is) hardcodes *production* asset paths, so that run rendered production
   wafer.json/assets through the experiment **binary** — it proves the code
   produces identical frames. The experiment **JSONs** are proven separately:
   the game selftest's `createDimensions()` builds dimensions from the
   compiled-in fallback, which is generated from the experiment asset files —
   so 1020/0 includes loading and generating from the edited wafer.json and
   organic.json — and the loader-semantics argument (inert sections: empty
   biome maps ⇒ eligibility 0; `"water": "air"` ⇒ the rule never writes) is
   pinned by the water selftest's explicit-vs-default byte equality.
4. **Water:** explicit-196/98 generator byte-identical to engine-default
   generator (§1a).

---

## 8. Labelled new captures (read and described by the builder)

Produced by a new procgen-level tool, `worldgen_capture`
(`game/LithoKraft/src/app/worldgen_capture.cpp`), which generates through the
real loader on synthetic flat-rock fields and renders a vertical slice
straight from the ColumnBuffers (1 block = 1 pixel, ×4; grey = rock, dark =
carved air, blue = water, purple = trace, teal = via, orange = rubble).
Directory: `~/workspace/experiments/worldgen-completeness/captures-labelled/`.

- **cheese_cavern.png** — large, rounded, interconnected cavern pockets
  hanging in solid grey rock with thick uncarved pillars between them and a
  clean solid ceiling under the surface: the Swiss-cheese structure, visibly
  a *field* carve, not ellipsoid pockets.
- **spaghetti_tubes.png** — thin dark tubes winding through the rock in
  smooth curves, a few blocks wide, clearly distinct from the cheese blobs:
  long, connected, sparse.
- **noodle_tubes.png** — the same family at the tighter tier: many more,
  noticeably thinner and shorter tube segments, denser speckle of
  1–2-block runs.
- **ruins_open.png** — several small rectangular buried rooms with ragged
  edges; orange rubble dots sit on the room floors, and the purple trace
  inlay + a teal via cell from the classic mode are visible in the same
  stone at the right edge: open mode composing with inlay, everything still
  buried.
- **deep_ocean_300_vs_196.png** — one water body (blue) over a grey rock
  floor at sea level 400, split by a white separator: LEFT = `max_depth`
  300, RIGHT = `max_depth` 196, same field. The left floor reaches visibly
  lower, with the deep trench left of the line; measured on the captured
  slice: max water depth **213 (300)** vs **139 (196)**. (Region-wide maxima
  in the selftest: 254 vs 166.)

---

## 9. Credits

- `game/LithoKraft/CREDITS.md` gained an **"Inspiration — worldgen
  completeness (clean-room)"** section: each source → what it inspired → the
  clean-room note (Kniberg's terrain talk; PCGamesN / Windows Central /
  GamesRadar cave coverage; Minecraft Wiki carver pages; Fast Noise Lite
  (MIT) as the noise census; libnoise via its Haxe port, license discrepancy
  flagged; OpenSimplex2; Lagae et al.; Quilez on domain warping; the river
  papers; Blagojevic's Map.h (MIT); Gumin's WFC via InfiniBrains;
  claytondb's backrooms (MIT); the jvanheerikhuize spec and Pabs' vackrooms;
  the Non-Euclidean Framework bibliography).
- The ledger itself is preserved at
  `~/workspace/experiments/worldgen-completeness/credits-ledger.md`.

---

## 10. Production-port instructions

Apply per group; each group is independently portable and gated by the same
commands used here (build, both selftest modes, handshake arms, ctest).
Paths are relative to the tree root (`engine/…`, `game/…`); note the
`src/engine/LithoKraft` symlink in some checkouts points at the same game
files — port each file once.

**(a) Parameterization (Item 1)** — engine:
`engine/src/procgen/include/litholattice/procgen/water.h`,
`engine/src/procgen/water.cpp`,
`engine/src/procgen/water_selftest.cpp`,
the water hunks of `engine/src/procgen/generator_loader.cpp`
(the `oceans`/`lakes` section loaders);
docs: `engine/docs/PROCGEN.md`.
**Game data:** `game/LithoKraft/assets/worldgen/wafer.json`,
`game/LithoKraft/assets/worldgen/organic.json` (inert sections; the built-in
fallback regenerates from them at build time — no second copy exists).

**(b) Carvers (Item 2)** — engine:
`engine/src/procgen/include/litholattice/procgen/stages.h`,
`engine/src/procgen/stages.cpp`,
the carver hunks of `engine/src/procgen/generator_loader.cpp`
(`cheese`/`spaghetti`/`noodle` loaders + ruins `open_mode` keys),
the carver hunks of `engine/src/procgen/procgen_selftest.cpp`.
No game-data changes: shipped pipelines deliberately unchanged.

**(c) Noise (Item 3)** — engine:
`engine/src/procgen/include/litholattice/procgen/modifiers.h`,
`engine/src/procgen/modifiers.cpp`,
`engine/src/procgen/include/litholattice/procgen/noise.h`,
`engine/src/procgen/cellular_noise.cpp`,
`engine/src/procgen/graph_nodes.cpp`,
`engine/src/procgen/include/litholattice/procgen/graph.h`,
the noise/graph hunks of `engine/src/procgen/procgen_selftest.cpp`.

**(d) Credits / docs / tooling** —
`game/LithoKraft/CREDITS.md` (**game data**), and optionally the capture
tool: `game/LithoKraft/src/app/worldgen_capture.cpp` (new file) + its
`game/LithoKraft/src/app/CMakeLists.txt` hunk.
The Backrooms spec is a program document, not a port item:
`~/workspace/worldgen-completeness-2026-10-13/backrooms-design.md`.

**Port order note:** (a) before (b) if porting selectively — the loader file
is shared; porting the whole `generator_loader.cpp` from the experiment tree
carries both at once and is the lower-risk path, since the experiment tree's
file set is exactly production + these diffs (verified by recursive diff:
the files listed above are the *complete* difference between the trees).

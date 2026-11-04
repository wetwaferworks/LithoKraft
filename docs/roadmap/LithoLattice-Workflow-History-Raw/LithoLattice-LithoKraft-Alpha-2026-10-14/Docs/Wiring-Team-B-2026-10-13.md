# Wiring — Team B (Render & World) — 2026-10-13

**Order:** JD's green light (2026-10-13 15:41 EDT) — wire everything the UML drew dashed.
**Team:** coordinator + two workers. Team A (sim/systems) worked the same tree in parallel;
the pre-wiring snapshot at `~/workspace/backups/litholattice-pre-wiring-2026-10-13/` protected
every item. **Nothing was reverted. No item failed its gates twice.**

## Final gates (coordinator's own runs on the settled tree)

| Gate | Result |
|---|---|
| Full build (whole tree, both teams) | **exit 0** |
| Game selftest, default mode | **1179 PASS / 0 FAIL — ALL SELFTESTS: OK** |
| Game selftest, `--timing-handshake` mode | 1178 PASS / 1 FAIL (single FAIL = audio streaming 440 Hz check — the documented load-sensitive flake; the same check passes in the clean default-mode run) |
| Selftest delta accounting | 958 baseline → 1179 = **+221**, reconciling exactly: Team B +122 (B3 +62, B5 +8, wiring suite +52) and Team A +99 (A1 +51, A2 +16, A4 +13, A6 +12, A3 +7) |
| `--handshake-test` | **HANDSHAKE TESTS: OK** |
| Scenario 2048, 4 arms | `d609126cc60d71c7` on **all four arms** (legacy 1/4, handshake 1/4, 4/4, 4/8) — cross-mode determinism intact. The absolute value differs from the pre-wiring `faec0f02700d98c6`: see "Scenario hash" note below |
| ctest (48 tests) | 46/48 in a loaded run (`engine_audio`, `engine_script` failed under load); **both pass solo → effectively 48/48** |
| Biome frames vs `status-frames-2026-10-13` | 2 byte-identical (`dead_zones`, `wafer_overview_map`); 5 differ by 163–877 px (0.018–0.095%) — fully attributed, see B4/Frames |

**Scenario hash note (honest):** the value moved from `faec0f02700d98c6` (still observed by
Worker 2 at ~20:05 UTC on a binary containing B3+B5+B6/B7) to `d609126cc60d71c7`. Of everything
that landed in that window, the only sim-behavior change is Team A's A5 block-density work;
no Team B lane touches the circuit scene (B1/B2 are render-only, B3/B5 worldgen is unused by
the hand-placed scene, B4/B6 registry appends do not renumber, B7 is inventory-side).
Determinism — the property the hash exists to prove — holds on every arm. Flagged here for
the joint review rather than silently re-baselined.

---

## B1 — Render suite ports (Worker 1) — WIRED

Ported from the experiment tree per the three render reports, in phase order, each phase
gated before the next began. Defaults discipline preserved exactly:

- **Phase 1 — model dithering** (`render.model_dither{,_near,_far}`): gate line
  *"Model dither checks: fade/IGN OK, mid-band coverage 2535/7600"* — matches the experiment
  report exactly. The verified split is preserved, not simplified: game **setting defaults
  ON**, renderer **class default OFF**.
- **Phase 2 — AA suite** (`render.aa = off|fxaa|cmaa|ssaa2x`, default OFF): gate line
  *"AA ladder checks: names OK, fxaa/cmaa soften edges + keep flats, ssaa resolve OK"*.
- **Phase 3 — ASCII + Systerian post stage** (`render.ascii`, `render.ascii_cols`,
  `render.ascii_set`): gate lines *"ASCII checks: grid geometry + ramp mapping + image output
  OK"* and *"Systerian checks: value grid 128/255 OK…, glyph strokes drawn (lit 1016)"*.
  The experiment's `software_test.cpp` is now wired into `litholattice_tests` (it was
  compiled nowhere in the experiment tree). The experiment's capture lab was NOT ported.

### JD-ruled amendment (2026-10-13 15:52 EDT) — applied, recorded as an amendment to the experiment build

JD's words: *"I want to have them use the zero to 255 ASCII set, not the zero to 64 one.
We're doing this exception so there can be more variance in the texturing and look of blocks.
And obviously we'll be using the SVG texture generator to get colorization and variance out
of all this stuff."*

- The value→glyph mapping now resolves at **(span+1)-step resolution with span defaulting to
  255 (256 steps)** for BOTH the ASCII ramp resolution and the Systerian glyph value —
  a deliberate, JD-named exception to the game's 64-scale conventions, commented as such at
  the mapping. Parameterized end-to-end as **`render.ascii_value_span`** (clamped 16..255):
  `software_ascii.h/.cpp` (`asciiValueLevel(luma, span)`), `IRenderer::setAsciiValueSpan`,
  renderer state, settings field (default 255), game init + live apply.
- The Systerian value is now the level itself (`composedGlyph(level)`), no longer derived
  from the ramp character; the Systerian text grid is three-digit fixed-width (000..span).
- **Pin recomputed honestly:** the experiment's "mid-gray → 58" pin does NOT survive — it
  encoded the 32..64 code-point subset, not luminance. New pins, all passing:
  `kAsciiDefaultValueSpan == 255`; `asciiValueLevel(128/255, 255) == 128` — **beyond the
  64th step**; `asciiValueLevel(128/255, 64) == 32` (the span is a real parameter);
  value-grid cell 0xFF808080 → **"128"**, white → **"255"**.
- **SVG variance:** the stage's per-cell colour is the average of the finished framebuffer
  over the cell — the framebuffer already carries the engine's SVG-generated texture
  variance, and no path substitutes flat per-block colours. Nothing needed rewiring; the
  after-captures confirm varied per-cell tints inside single block regions.
- **Before/after evidence** (same spires scene, 160 cols; before = experiment build,
  after = production code): Systerian grid went from **7 distinct values** in 7,200 cells
  (77% the single value 42) to **155 distinct values** (range 019–174). Captures and grids
  in `~/workspace/user/files/render-lab-2026-10-13-amended/` (`spires_systerian_160.png`,
  `spires_ascii_160.png` + `.txt` grids), read personally by Worker 1.

## B2 — Vulkan sprite pass (Worker 1) — WIRED (compile + contract proven; on-device look not proven)

Implemented at parity with GL33/software: atlas pages from `FrameParams::spriteAtlasPages`
re-uploaded as one texture array (layer = page) when `spriteAtlasVersion` changes; quads
drawn after world geometry, before lines/HUD, in **input order, never re-sorted**; runs of
equal (shade, page) switch pipeline — cutout (alpha test 0.5 via push constant, depth write),
blend (SRC_ALPHA / ONE_MINUS_SRC_ALPHA), additive (SRC_ALPHA / ONE); per-frame-slot
grow-only vertex buffer; binding 4 = sprite atlas; new shaders `vk/sprite.vert` +
`vk/sprite.frag`; the "sprite pass not implemented" warning is removed. One genuinely
unsupported sub-case is named in a once-only warning instead of being misdrawn: mixed-size
atlas pages (the registry guarantees one size; such an atlas is skipped), as are non-sprite
shades / out-of-range pages (skip-counted, mirroring GL33).

**Proven:** full compile including **glslang SPIR-V compilation of both new shaders**;
render_vk + tests build exit 0; unit contract tests pass — *"VK sprite conversion checks:
expansion/order/runs/shade mapping OK"* (corner order {0,1,2,0,2,3}, order preservation,
run grouping, pipeline/alpha mapping, skip-marking).
**Not proven:** on-device visual output — there is no GPU/Vulkan device on this VM (the
engine probe treats no-device as SKIP), so pipeline creation, descriptor binding 4 in a
live frame, and presented pixels are unverified — the same honesty standard as GL33's
unverified runtime look.

## B3 — Worldgen ports (Worker 2) — WIRED

Groups (a) parameterization, (b) carvers, (c) noise, (d) credits/docs per the Worldgen
Report §10. A pre-port recursive diff returned *exactly* the §10 list — no undocumented
differences — and the files were copied whole per the report's lower-risk path.
The optional `worldgen_capture` tool + its CMake hunk were deliberately **skipped**
(duplicates the production `biome_capture`).

Proof: build exit 0; engine water module **73 PASS / 0 FAIL** (67→73, +6 as specified);
game procgen selftest **PROCGEN SELFTEST OK** (cheese carved fraction 0.1008, 286/1024
pillar columns untouched — the experiment's pinned values; spaghetti/noodle/ruins-open/
ping-pong/curl/cellular-ID/graph-node checks all pass); explicit-196/98 byte-equality is
pinned in the ported water selftest and passes; **no shipped pipeline gained a carver**;
ruins openMode defaults false; LithoKraft's wafer/organic oceans/lakes sections are the
inert `"water": "air"` sentinel form (the game registers no water block — by construction,
not omission). Game selftest delta for B3: **+62** as predicted.

## B4 — Sprite → worldgen binding (coordinator) — WIRED

**True state found:** the `biome_vegetation.h` rows existed and ONE minimal placement path
existed — wafer.json's `shrubs_dry_tuft` GroundCoverFeature placed DryTuft in DicingPlains
(0.012) and ResonatorSpires (0.020) **rendered through the silica_grass block**. The procgen
plant textures themselves were bound NOWHERE. Modeled3D rows (TraceForest Berry,
CapacitorGroves Twisted) remain unplaced — octree→chunk stamping doesn't exist; outside
this item's ask and still true.

**⚑ BRIDGE CHOICE — flagged for JD's later veto:** the **data-asset bridge**, least
invasive of the options on file. The plant textures became real pack assets + real
`Shape::Cross` blocks, and the existing groundcover entries were re-pointed per biome
family. Placement positions and counts are provably unchanged: the feature's
cumulative-density roll sums the two entries' disjoint per-biome maps identically to the
old single entry; the feature **name is unchanged** (it feeds the placement seed); heights
stay 1. Only the placed material changed.

What was built:
- New kept tool `tools/plant_texture_gen.cpp` (build line in its header) generated **all
  35 PNGs, 0 failures**: 7 textures (legacy `procgen_grass_tuft`, `procgen_fern`,
  `procgen_dry_scrub` + formula `procgen_formula_flower/star/leaf/blob`) as block textures
  in pack_16/32/64 (formula textures re-rendered per size from canonical parameters; legacy
  integer-nearest upscale from the 16×16 source) + icons in pack_32/64; registered in every
  pack's `resources.json` (texture + model entries) and `manifest.json`
  (file_count 71 / 322 / 322). Coordinator visually verified the leaf (lens silhouette,
  speckled interior, brown stem) and blob (round canopy on trunk) renders.
- 7 blocks appended, **ids 58–64** in texture-index order, `Shape::Cross`,
  `kTransparent | kAttachable`, role None — matching silica_grass's conventions.
- wafer.json: `shrubs_dry_tuft` split into leaf `{dicing_plains: 0.012}` and blob
  `{resonator_spires: 0.02}` entries, with the comment rewritten to tell the true history.
- `biome_vegetation.h` SCOPE paragraph updated to the new state.

Proof (8 selftest checks, all PASS in both modes): registration (Cross shape, texture
names, ids after EtchOR); the vegetation rows' own formula parameters resolve to leaf (5)
and blob (6); the shipped wafer.json generates **leaf > 0 in DicingPlains columns, blob > 0
in ResonatorSpires columns, silica_grass == 0** across the scanned window; regeneration is
cell-identical. Captures read personally by the coordinator: the leaf tufts are visible on
the DicingPlains dark ground (left edge) and blob tufts on the ResonatorSpires ground
(front edge) in `~/workspace/user/files/wiring-frames-2026-10-13/`.

Notes: StaticMarsh → flower cannot bind yet — StaticMarsh is not one of the five live
wafer.json biomes (the flower block is registered regardless). On JD's SVG-variance point:
plant texture variance here is procedural (hash-varied speckle; the seed varies colour
only, never the mask, per shrubs.h) and the PNG bridge preserves it byte-exact.

## B5 — Dimension registration (Worker 2) — Cosmic + Underwafer WIRED; the Void = analysis (JD decision)

Registered following the wafer/organic pattern (id, name, biomes, `makeGenerator`, setup,
`belowWorldFill = kStone`, chunk registry); the new JSONs are picked up by the existing
`file(GLOB …)` embed — `game/CMakeLists.txt` was **not** edited.

- **Cosmic** — NEW `assets/worldgen/cosmic.json`: single `empty_crust` biome, stone
  heightmap terrain with height field in [128, 224] (inside the design's lower-256 solid
  band of the fixed 768 height; air above = the Empty stand-in). No carvers, no features.
- **Underwafer** — NEW `assets/worldgen/underwafer.json`: single `deep_stone` biome, stone
  heightmap (height ∈ [80, 160]) + the existing `worm_cave` carver at organic's proven
  settings. No invented content.

Proof: an 8-check selftest block, **8/8 PASS in default mode, handshake mode, and ctest** —
registry resolution for both dimensions, `void == nullptr`, overworld alias intact,
generators non-null (fail-closed-clean load), builtin fallback sources exist, and a real
`load(pool, 2)` smoke per dimension (0 load failures, spawn on the surface, cosmic all-air
at y ≥ 256, underwafer spawn standing on stone).

**Honest content gaps** (also written into both JSONs' descriptions and the code comments):
Cosmic — the Empty is **air**, not a named medium: grep-verified that the worldgen
JSON/loader has **no medium/atmosphere field** to name it in. Missing and not faked:
Recons, Lattice Bridge, starship traversal, free descent into Organic, magnetic boots,
Hypervisor/Underwafer gating. Underwafer — missing and not faked: the Sear, hypervisor
ore, any ceiling structure, the Wafer-bottom portal link. What exists is a deep-stone
floor with worm caves.

**Discrepancies found vs the brief/design docs (flagged, not silently resolved):**
1. The brief's Wafer bound **±65,536 is not corroborated anywhere in-tree** — no doc,
   comment, or code states a Wafer bound. `dimensions.h` records bounds only for
   Underwafer (±131,072), Organic (±196,608), Cosmic (±262,144).
2. **No per-dimension bound exists as data:** the `Dimension` struct has no bounds field;
   bounds are convention-only (enforcement is `ChunkStreamer::centreInRange` against the
   global packPos range). There was therefore nothing to "set" at registration — flagged
   for JD if bounds-as-data is wanted.
3. **Height is not per-dimension:** world height is the engine constant `kWorldHeight =
   768` for every dimension; Cosmic's "768 (256 solids + 512 Empty)" is expressed through
   terrain height ≤ 255, not a height setting.

### THE VOID — representability analysis (JD-DECISION item; the Void was NOT registered)

**The two key systems, exactly as they exist in-tree:**

1. **Chunk storage key — `ChunkKey::make(cx, cz)`** (`engine/src/world/include/litholattice/
   world/chunk.h`, used by `ChunkMap`): `key = (uint64(uint32(cx)) << 32) | uint64(uint32(cz))`
   — a full 32 bits per chunk axis. Column *storage* can therefore address cx, cz ∈
   [−2³¹, 2³¹−1] chunks, i.e. ±2³⁵ blocks per axis. Saves address columns by the same int32
   cx/cz. **Storage is not the binding constraint.**
2. **Per-cell key — `packPos(x, y, z)`** (same header; the key for signal-component,
   light-source, and dirty-cell hash sets): `key = (int64(x + 2²⁰) << 31) | (int64(z + 2²⁰)
   << 10) | int64(y)` — a 52-bit packing:
   - **x: 21 bits** (bits 31–51), biased by +2²⁰ → representable **[−1,048,576, +1,048,575]**
   - **z: 21 bits** (bits 10–30), biased by +2²⁰ → representable **[−1,048,576, +1,048,575]**
   - **y: 10 bits** (bits 0–9), unsigned → representable **[0, 1023]** (engine world height
     is 768, so y has headroom)
   Outside the x/z range the key **aliases silently**: x = +2²⁰ and x = −2²⁰ map to the
   same key, merging two unrelated cells' signal/light/dirty state. `packPosInRange()`
   exists for new call sites, and `ChunkStreamer` refuses out-of-range request centres
   loudly at setup — enforcement is at dimension/streamer setup, not in `packPos` itself.

**Comparison against every dimension's bound (blocks, x/z):**

| Dimension | Bound | vs packPos limit (±1,048,575) |
|---|---|---|
| Wafer | ±65,536 per the brief — *not stated in-tree* (see discrepancy 1) | fits, 16× headroom (if the brief's number is the design) |
| Underwafer | ±131,072 (dimensions.h) | fits, 8× headroom |
| Organic | ±196,608 (dimensions.h) | fits, ~5.3× headroom |
| Cosmic | ±262,144 (dimensions.h) — the largest | fits, 4× headroom |
| **Void** | **infinite (locked design)** | **unrepresentable at any width — this is the whole problem** |

**Why there is no sound existing seam:** registering the Void means either (a) letting
unbounded coordinates reach `packPos`, which does not fail — it *aliases*, silently
corrupting signal/light/dirty state for distant cells, the worst failure mode under the
world-permanence doctrine; or (b) having the streamer refuse everything beyond ±2²⁰,
which is just a finite bound wearing the Void's name — a registration that lies about the
design. An "empty air" infinite stub would additionally be a falling-forever trap (fixed
768 height, no content, no infohazard/Hypervisor mechanics). The Void was therefore not
registered, and `map void` continues to answer "unknown dimension" honestly.

**Concrete proposals, with world-permanence / migration implications:**

1. **Give the Void a finite bound inside the packPos range** (e.g. ±262,144 like the
   Cosmic, up to the hard max ±1,048,575). *Permanence impact: zero* — no key, save
   format, or migration changes anywhere; every existing world is untouched, and a Void
   world is an ordinary save from day one. *Cost:* a design change — "infinite" becomes
   "the largest dimension." JD's call alone, but it is the only option that is free forever.
2. **Per-dimension wider key for the Void only** (e.g. a 62-bit packing: 26/26/10).
   *Permanence impact: contained but real* — `packPos` is a single engine-wide inline used
   across signal, light, and dirty tracking, and keyed sets persist in saves (signal state
   export). The Void has no saves yet, so a Void-local key type costs nothing *for the
   Void*; but splitting the key type forks every consumer that touches those sets, and any
   future format unification becomes a migration. Infinity is still not represented —
   26 bits only moves the aliasing wall to ±33 million blocks.
3. **Region/epoch virtual coordinates (floating origin)** — the Void is tiled into windows
   that fit the packPos range; a cell's true coordinate is `epoch × window + local`, and
   only `local` ever reaches `packPos`. *Permanence impact: the scheme must be decided
   before the first Void save exists* — retrofitting means rewriting every persisted
   coordinate in every Void save. Bounded dimensions are unaffected (epoch 0 forever).
   *Cost:* every world-coordinate consumer (rendering, MCP pilots, netcode, world queries)
   must speak virtual coordinates at the Void boundary, and cross-window structures/signal
   nets need an explicit seam story. This is the only option that preserves true unbounded
   extent.
4. **Universal structured key (ChunkKey + local cell index, 128-bit or pair-hash)
   replacing packPos everywhere.** *Permanence impact: worst* — it re-keys persisted state
   in *existing* Wafer/Organic saves, requiring a versioned migration of live worlds;
   directly against the grain of the permanence doctrine for zero benefit to the bounded
   dimensions. Not recommended.

**Recommendation for JD's decision:** Option 1 if a finite Void is design-acceptable
(free, permanent, shippable with registration exactly like the other two). Option 3 if
infinity is load-bearing — in which case the epoch/window format should be locked *before*
any Void content or saves exist, because under world permanence the cheap moment to choose
is now, and it never comes back.

## B6 — EtchOR / ChipOR completion (coordinator) — WIRED

- **EtchOR block** `lithokraft:etchor`, **id 57** (append-only, directly after
  certificate_table 56). `SimRole::EtchOR` appended at the **end** of the SimRole enum
  (after Fusor; never inserted mid-enum). Behavior name `"etchor"` is a describe-only seam
  in new `content/components/etchor.cpp` (`registerEtchOR`, wired in catalog.cpp +
  components.h) — mirroring certificate_table.cpp's rationale: the etch is an inventory
  operation; the SelectOR/ConnectOR/Laysor machine is future work and is NOT faked.
- **Etch flow** (`game/etching.{h,cpp}`): `etchChip(Inventory&, nameField, err)` consumes
  1× blank wafer and produces a ChipOR stack whose name field is the name **verbatim**
  (overwrites the def name; empty name → plain chip). Name cap 64 bytes is **refused,
  never truncated**; control characters refused; the operation is fully atomic (including
  the full-inventory single-wafer edge, handled with restore). Console entry point:
  `etch <name...>` (session-tier, in game_commands + game.cpp bindings).
- **⚑ Blank-wafer stand-in — flagged for JD:** the blank is **Wafer Silicon
  (`kWaferSilicon`)**, the conservative stand-in. The AutoFabOR-stamped blank-wafer item
  of the design does not exist yet; when it lands, the input id is a one-line change.
- **ChipOR "stops being inert" — the interpretation used, stated plainly:** the designed
  extent is **identity** — the name field now flows through creation → inventory →
  identity-aware stacking → persistence (see the v5 note under B7). Placed-block name
  display is impossible without per-cell string storage in the world format; that boundary
  is flagged, and no sim signal behavior was invented — the pre-existing chipor selftest
  pin "sim dispatches no ChipOR case" still passes.
- **Art:** `assets/texture_recipes/etchor.ltxr` (dark slate, violet #A970FF beam grid,
  orange #FF6F00 rivets — read and approved by the coordinator) → block textures 16/32/64 +
  icons pack_32/64, registered in all three packs. This also fixed a real selftest FAIL
  the wiring itself created (every registered block must have a pack icon).

## B7 — Certificate signing mechanic (coordinator) — WIRED

- **Definitions** (`content/certificates.{h,cpp}`): **Clarity** (3 levels, effect
  `reveals_demensors` — DECIDED design; table-crafted per lore, recipe path still open),
  **Knowledge** (5 levels — DECIDED to exist 2026-10-13), **Efficiency** (2) and
  **Mining Haste** (2) — the glossary addendum's own examples. **⚑ All costs are
  conservative placeholders for JD.**
- **Curve:** the **PROPOSED** (not locked) static curve implemented exactly — per-level
  cost a power of two doubling every 4 levels, base 8; pins `staticTotalForLevel(16) ==
  480`, `(32) == 8160`; level derives from total; enchanting-style spend preserving
  in-level progress. The terminology fence is honored throughout (certificate table ≠
  mod signature ≠ Litho asset certificate).
- **Flow** (`game/signing.{h,cpp}`): `signStack(inv, flatSlot, certId, totalStatic&, err)`
  — fail-closed (unknown cert, empty slot, duplicate cert, insufficient level, no room);
  stacks > 1 split so exactly one item is signed. Console: `sign <certificate>` and `etch`
  are session-tier; `certificates` is read-only plain. Signing reads
  `playerAdapter_.staticCharge()` and writes back via
  `enginePlayer().setStat(staticStatId(), …)`; it signs the selected hotbar stack.
- **Deliberately NOT done, with reasoning:** the certificate table's SimRole was left
  `None` and its behavior describe-only this pass. The design prescription offered
  "grows a SimRole + ComponentBehavior::use() OR the game intercepts use" — the game-side
  path was chosen because sim `use()` cannot reach player inventory, and growing the role
  would change component-map admission for zero functional gain. **⚑ Flagged for JD.**
  The table UI is JD's design; the lapis-analogue item is OPEN (no catalyst is consumed).
- **Persistence (serves B6+B7): player.dat bumped v4 → v5** (`PlayerMeta::kVersion = 5`):
  the writer appends a stack-data block (flat slot index, name field, certificate id list)
  after the v4 inventory block; the reader gates `v >= 5` *before* the exact-consumption
  check; **v1–v4 files load with plain stacks** (proven by a hand-built v4 file in the
  selftest). `ItemStack` gained `customName` + `certificates` +
  `hasData()`/`sameIdentity()`; `Inventory::addItem` never merges plain adds into
  data-bearing stacks; new `placeStack`/`canPlaceStack` (carry slots only,
  all-or-nothing); `stackDisplayName()` honors the name field.
- **Selftest:** new `game/wiring_selftest.{h,cpp}` (`runWiringSelfTest()`, hooked into
  `app/selftest.cpp` after `runGameUiSelfTest()`) — **52 checks** for B6/B7 (44) + B4 (8),
  all PASS in both modes, covering registration, the etch flow end-to-end, the curve pins,
  the signing flow, the v5 round-trip, and the v4-compat load. (This suite also caught and
  fixed the second wiring-caused FAIL: a pre-existing check pinning `fileVersion == 4`,
  now pinned to `PlayerMeta::kVersion`.)

---

## Coordination incident — the full-selftest stall (resolved; no Team B file involved)

From the ~20:26 UTC builds onward, every full `--selftest` run stalled permanently at
~40 s CPU. It was first misattributed (by both teams, Team B included) to the
ChunkStreamer integration block; that attribution was wrong — stdout block-buffering hid
true progress, and the block's loops are bounded and pass.

**Root cause** (gdb backtrace of the coordinator's own stalled run; full note in
`~/workspace/wiring-2026-10-13/team-b-coordinator.md`): Team A's A3 file
`game/LithoKraft/src/content/item_use.cpp` — `ensureDefaults()` runs
`std::call_once(once, registerDefaults)`; `registerDefaults()` called the public
`registerItemUseHandler()`, which re-enters `ensureDefaults()` — a recursive `call_once`
on the in-progress flag, deadlocking on first use from any thread. **Team A fixed it**
(registerDefaults now writes the handler map directly; the fix is in the tree) and the
coordinator verified the fix: the item-use checks pass and both selftest modes run to
completion (final gate numbers above). Disclosure: during diagnosis the coordinator
terminated two stalled selftest processes; both were deadlocked in this bug and
unresponsive to their timeout wrappers. Team B applied no fix to Team A's files.

Two FAILs routed to Team B during the program (EtchOR pack icon; the player.dat v4
version pin) were both genuinely Team B's, and both were fixed by Team B — see B6/B7.

## Frames

Final captures: `~/workspace/user/files/wiring-frames-2026-10-13/` (all 7 vistas).
`dead_zones` and `wafer_overview_map` are byte-identical to the pre-wiring baselines.
The five differing frames differ by 163–877 px (0.018–0.095%): `dicing_plains` 448 px and
`resonator_spires` 877 px carry the **B4 tuft-texture swap** (same positions, new leaf/blob
textures — proven by the B4 selftest and visible in the frames); `capacitor_groves` 163 px,
`spawn` 605 px, `trace_forest` 491 px are sparse model-region pixels attributable to the
**B1 port at shipped defaults** (model dithering's game setting defaults ON; AA and ASCII
default OFF and are pixel-identical when off, per the B1 gates). Worker 1's discriminator
rules out asset/worldgen drift: the experiment binary, run against the *current*
production assets, reproduces all 7 baseline MD5s exactly — the deltas are compiled-in
wiring, in the lanes named.

## Changed files — Team B (exact)

**B1 (Worker 1):** NEW `engine/src/render/include/litholattice/render/model_dither.h`,
`software_aa.h`, `software_ascii.h`, `software_test.h`, `engine/src/render/software_aa.cpp`,
`software_ascii.cpp`. Modified: `engine/src/render/include/litholattice/render/renderer.h`,
`software_renderer.h`, `engine/src/render/software_renderer.cpp`, `software_test.cpp`,
`engine/src/render/CMakeLists.txt`, `engine/src/render/gl33/gl33_renderer.cpp`,
`engine/assets/shaders/chunk_shading.glsl`, `chunk.frag`, `chunk_pbr.frag`,
`engine/tests/CMakeLists.txt`, `engine/tests/engine_tests.cpp`,
`game/LithoKraft/src/game/include/lithokraft/game/settings.h`, `settings_cvars.cpp`,
`game.cpp`. Amendment files (separately identifiable): `software_ascii.h`,
`software_ascii.cpp`, `software_test.cpp` (pins), plus span plumbing in `renderer.h`,
`software_renderer.h/.cpp`, `settings.h`, `settings_cvars.cpp`, `game.cpp`.

**B2 (Worker 1):** NEW `engine/assets/shaders/vk/sprite.vert`, `sprite.frag`,
`engine/src/render_vk/include/litholattice/render_vk/sprite_vertices.h`. Modified:
`engine/src/render_vk/vk_pipelines.h`, `vk_pipelines.cpp`, `vk_frame_uniforms.h`,
`vk_renderer.cpp`, `engine/src/render_vk/CMakeLists.txt`, `engine/tests/engine_tests.cpp`,
`engine/tests/CMakeLists.txt`.

**B3 (Worker 2):** `engine/src/procgen/include/litholattice/procgen/water.h`, `stages.h`,
`modifiers.h`, `noise.h`, `graph.h`; `engine/src/procgen/water.cpp`, `water_selftest.cpp`,
`generator_loader.cpp`, `stages.cpp`, `procgen_selftest.cpp`, `modifiers.cpp`,
`cellular_noise.cpp`, `graph_nodes.cpp`; `engine/docs/PROCGEN.md`;
`game/LithoKraft/assets/worldgen/wafer.json`, `organic.json` (shared with B4/B5 edits);
`game/LithoKraft/CREDITS.md`.

**B5 (Worker 2):** NEW `game/LithoKraft/assets/worldgen/cosmic.json`, `underwafer.json`.
Modified: `game/LithoKraft/src/game/dimensions.cpp`,
`game/LithoKraft/src/game/include/lithokraft/game/dimensions.h`,
`game/LithoKraft/src/app/selftest.cpp` (one 8-check block; shared file).

**B4 (coordinator):** NEW `tools/plant_texture_gen.cpp`; 35 generated PNGs under
`game/LithoKraft/assets/packs/pack_16|32|64/textures/` (7 block textures ×3 packs +
7 icons ×2 packs). Modified: `game/LithoKraft/src/content/blocks.cpp`,
`game/LithoKraft/src/content/include/lithokraft/content/blocks.h` (shared with B6),
`game/LithoKraft/assets/worldgen/wafer.json` (groundcover entries),
`game/LithoKraft/src/content/vegetation/biome_vegetation.h` (SCOPE comment),
all three packs' `resources.json` + `manifest.json` (shared with B6),
`game/LithoKraft/src/game/wiring_selftest.cpp` (B4 block; shared with B6/B7).

**B6 (coordinator):** NEW `game/LithoKraft/src/content/components/etchor.cpp`,
`game/LithoKraft/src/game/etching.h`, `etching.cpp`,
`game/LithoKraft/assets/texture_recipes/etchor.ltxr`; generated PNGs (etchor textures
×3 packs + icons ×2 packs). Modified: `blocks.cpp`, `blocks.h`, `components/catalog.cpp`,
`components/components.h`, `content/CMakeLists.txt`, `game/etching.*` wiring in
`game/game_commands.h`, `game_commands.cpp`, `game/game.cpp` (shared files, targeted
hunks), packs' `resources.json`/`manifest.json`.

**B7 (coordinator):** NEW `game/LithoKraft/src/content/certificates.h`, `certificates.cpp`,
`game/LithoKraft/src/game/signing.h`, `signing.cpp`,
`game/LithoKraft/src/game/wiring_selftest.h`, `wiring_selftest.cpp`. Modified:
`content/CMakeLists.txt`, `game/CMakeLists.txt`, `game/inventory.h`, `inventory.cpp`
(stack identity), `game/world_save.h`, `world_save.cpp` (player.dat v5),
`game/game_commands.h/.cpp`, `game/game.cpp` (bindings), `app/selftest.cpp` (hook +
version pin; shared file).

**Union across both teams:** 77 modified files + 76 relevant new files vs the pre-wiring
snapshot (full lists in `~/workspace/wiring-2026-10-13/changed-files-union.txt` and the
coordination logs). Shared hotspot files (selftest.cpp, game.cpp, blocks.cpp, wafer.json,
settings_cvars.cpp, CMakeLists files) carry both teams' targeted hunks; no wholesale
overwrites occurred.

## Open items handed back to JD

1. **The Void** — decision among the 4 proposals above (recommendation: Option 1 if a
   finite Void is acceptable; Option 3 if infinity is load-bearing, locked before the
   first Void save exists).
2. **B4 bridge veto** — the data-asset bridge was chosen as least-invasive; JD may veto
   in favor of a different bridge.
3. **B6 blank wafer** — Wafer Silicon is a stand-in until the AutoFabOR-stamped blank
   exists.
4. **B7 placeholders** — all certificate costs are conservative placeholders; the
   certificate table's SimRole was deliberately left `None` (reasoning in B7); table UI
   and the lapis-analogue item remain JD's design calls.
5. **B5 discrepancies** — the Wafer bound ±65,536 is not corroborated in-tree; bounds are
   not per-dimension data today. JD's call if either should change.
6. **Scenario hash** — re-baseline to `d609126cc60d71c7` or investigate the A5 hunk
   (Team A's lane) at the joint review; determinism itself is intact.

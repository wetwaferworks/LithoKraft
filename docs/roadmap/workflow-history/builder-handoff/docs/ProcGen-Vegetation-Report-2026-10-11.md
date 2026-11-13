# ProcGen Vegetation System — Implementation Report (2026-10-11)

Authorized by JD 2026-10-11 (~21:57): "let's return to the proc gen, start on the trees" — procedural trees plus grass/shrubs with BOTH 3D and 2D output styles ("we like options"). Three workstreams, all landed green.

**Test totals: 28 + 53 + 94 = 175 new checks, all passing.** Full build clean. The 2 grandfathered ctest failures (`engine_sim`, `lithokraft_selftest`) were not touched.

> Build-dir note: several sibling workstreams were editing the shared `build-linux` tree concurrently during this implementation (water, worm-carver, index, mcp). A few ctest entries failed from their in-flight edits (`engine_procgen`, `engine_water`, `engine_index`, `engine_mcp`); none involve vegetation code, and each workstream below re-verified its own suites directly. Details per workstream.

---

## Workstream 1 — Procedural tree generator (engine core)

**Status: complete.** `engine_trees` ctest: **28/28 PASS**.

### Files added
- `engine/src/procgen/include/litholattice/procgen/trees.h` — `litholattice::procgen::TreeParams` (12 fields, exactly per spec), `SparseOctree generateTree(const TreeParams&)`, three family preset functions, `runTreeGenSelfTest()`. Full contract + palette-slot documentation in header comments.
- `engine/src/procgen/trees.cpp` — implementation.
- `engine/src/procgen/trees_selftest.cpp` — 28 headless checks (new ctest entry `engine_trees`).

### Files changed
- `engine/src/procgen/CMakeLists.txt` — added `trees.cpp`/`trees_selftest.cpp`; `litholattice_subdiv` linked PUBLIC (additive; subdiv links nothing, no cycle).
- `engine/tests/CMakeLists.txt`, `engine/tests/engine_tests.cpp` — new `engine_trees` entry.

### Design
- **RNG:** the engine's existing deterministic RNG — `litholattice::procgen::Rng` from `seed.h` (xoshiro256**, seeded through the splitmix64-family `mix64`; same lineage as the `models.cpp` stream). Seeded via `deriveSeed(p.seed, nameHash("tree"))`. No new RNG invented.
- **Organic mode:** tapered trunk column, recursive branching (child dir = parent dir rotated about a random perpendicular axis by `branchAngle + noise`, length ×= falloff, radius ×0.62, upward bias), leaf blobs at branch ends, glowing tip pixels at extremities. Hard segment budget (1500) + hard height ceiling make it fail-closed: max params terminate, tree always ≤ heightCells tall, still deterministic.
- **Crystalline mode:** stacked column alternating rotated boxes and octahedral facets with spiral twist, thin angled side shards (headroom-capped), glowing apex shard. Shard layers alternate trunk/leaf materials for the faceted look.
- **Validation:** `sanitize()` clamps every field with explicit NaN guards. Zero/negative/NaN/extreme inputs → minimal valid tree, never a crash.

### Biome families (presets; seed picks the individual)
1. `clockwoodWirewood(seed)` — 56–64 cells tall, radius 2, 2 sparse branches/node, falloff 0.78, no leaves/tips; expects a copper-brown bark material.
2. `dopantCrystalTree(seed)` — crystalline; expects blue crystal / amber crystal / white-hot core materials (layers alternate, apex shard).
3. `traceForestTree(seed)` — copper trunk, red **and** blue glowing tips; expects copper + red-glow + blue-glow materials (small blobs read as circuit nodes).

### Additive behavior (documented in trees.h, needs JD's lock)
`TreeParams` carries one `tipMaterial` byte, so for the trace-forest's red+blue tips: **when both `tipMaterial` and `leafMaterial` are nonzero, glowing extremities alternate tipMaterial/leafMaterial by extremity index** (two-colour tips). With only `tipMaterial` set, every extremity is `tipMaterial`. Covered by tests.

### Evidence
- `engine_trees` 28/28: determinism (`hashLeaves` equality AND leaf-stream equality), 5-seed pairwise difference, degenerate-param safety, per-family non-emptiness/bounds/palette/level-6/in-volume, preset value checks, crystalline-vs-organic structural difference (993 vs 1423 leaves), SubdivStore round-trip.
- Full build clean, 0 errors, no new warnings. `engine_subdiv` PASS (link change safe).

---

## Workstream 2 — Grass/shrub generator, dual output (engine core)

**Status: complete.** `engine_shrubs` ctest: **53/53 PASS**.

### Files added
- `engine/src/procgen/include/litholattice/procgen/shrubs.h` — the API seam **verbatim** (`PlantStyle`, `ShrubParams`, `CrossQuadPlant`, `generateShrubOctree`, `generateShrubCross`), plus additive helpers: `validShrubParams()`, `kMaxShrubLeaves`, texture-set API (`numPlantTextures()`, `plantTextureName()`, `plantTexturePixels()`, `kPlantTextureSize/Bytes`), 4 preset functions, and a **"2D RENDER BINDING"** doc section.
- `engine/src/procgen/shrubs.cpp` — both generators + 3 procedural 16×16 RGBA textures + presets.
- `engine/src/procgen/shrubs_selftest.cpp` — 53 checks (new ctest entry `engine_shrubs`, Test #9).

### Files changed
- `engine/src/procgen/CMakeLists.txt`, `engine/tests/CMakeLists.txt`, `engine/tests/engine_tests.cpp` — wired in (a briefly-attempted CMake reorder was fully reverted; not needed).

### The dual output (JD's "we like options" call)
- **(a) 3D modeled:** `generateShrubOctree` → small noisy shrub mound octree (leaf-count rail at 1024; max params yield 853 leaves / 73 KB).
- **(b) 2D crossed quads:** `generateShrubCross` → `CrossQuadPlant{widthBlocks, heightBlocks, textureIndex, tintR/G/B}`; width/height/tint derived deterministically from seed+size. The `style` flag is intentionally ignored by the 2D path (it always produces the 2D bundle).
- **Example plants:** 4 presets covering both styles.

### 2D render binding (documented in shrubs.h)
Game registers one `BlockDef` per plant texture with `shape = Shape::Cross`, `tex = plantTextureName(i)` (e.g. `"procgen_grass_tuft"`) — the silica-grass precedent (BlockId 53, `blocks.cpp`). `plantTexturePixels(i)` (16×16 RGBA8, row-major, row 0 = top, alpha 0 = cutout) uploads into the texture pack under that name; the mesher's `emitCross()` draws the two crossed alpha-tested quads (`kShadeCutout`). The 3 procedural textures: grass tuft, fern, dry scrub.

### API deviation (with reason)
Used the existing engine RNG `litholattice::procgen::Rng` (xoshiro256** via SplitMix64) instead of a local splitmix64 copy, per the "check for an existing engine RNG" instruction — it's the module convention, integer-only, cross-platform deterministic.

### Evidence
- `engine_shrubs` 53/53: 3D determinism + 2 golden leaf hashes, 2D determinism, style-flag semantics, seed sensitivity both paths, 10 fail-closed degenerate cases (sizeCells 0/65/-3, NaN/out-of-range density/noise, zero materials), texture layout/names/OOB-null/binary-alpha/determinism + 3 golden texture hashes, all 4 presets valid.
- `make litholattice_procgen`, `make litholattice_tests`: clean, warning-free (`-Wall -Wextra`).
- Full `ctest -R engine_`: 19/21 — the 2 failures are `engine_sim` (grandfathered, untouched) and `engine_procgen` (sibling workstream's in-flight `stages.cpp`/`pipeline.cpp` edits during the session; none of this workstream's files touch that logic).

---

## Workstream 3 — Game-side API, family assets, biome mapping

**Status: complete.** `vegetation_selftest` ctest: **94/94 PASS**.

### Files added (`game/LithoKraft/src/content/vegetation/`, namespace `lithokraft::vegetation`)
- `vegetation.h` / `vegetation.cpp` — the exposed API, exactly per spec: `generateTreeModel(const procgen::TreeParams&, name, author="")` and `generateShrubModel(const procgen::ShrubParams&, name, author="", ShrubCrossInfo* crossOut=nullptr)`. Both copy caller params, bind engine placeholder material slots to game palette ids (`bindTreePalette`/`bindShrubPalette`, public and documented), call the engine core, and return ready-to-save `model::Model`. For `PlantStyle::CrossedQuads`, `crossOut` receives the 2D bundle (width/height/textureIndex/textureName/tint from `generateShrubCross` + `plantTextureName`); the Model geometry holds a thin crossed-plane voxel stand-in (documented as 3D fallback — the bundle is authoritative for the `Shape::Cross` path).
- `vegetation_models.h` / `.cpp` — family author functions + `allVegetationModels()` registry (mirrors the `author_models` pattern); fixed showcase seeds (`0xC10C700D`, `0xD0A67C47`, `0x7ACEF0E57`).
- `author_vegetation.cpp` — authoring tool regenerating `assets/models/*.lthm` (deterministic; byte-identical re-runs verified cross-process with `cmp`).
- `biome_vegetation.h` — the 11-biome mapping table + `fillTreeDensity`/`fillShrubDensity` shaping `procgen::BiomeDensity` for later `blendedDensity` consumption. Marked mapping-only; placement integration explicitly a later step.
- `vegetation_selftest.cpp` — new ctest `vegetation_selftest`.
- `VEGETATION_API.md` — functions, params, style flag, determinism contract, 10-line usage example, seam notes.
- `BIOME_VEGETATION.md` — the mapping table + rationale.
- `CMakeLists.txt` — `lithokraft_vegetation` lib, `author_vegetation` tool, `vegetation_selftest` + `add_test`; wired via `add_subdirectory(vegetation)` in `content/CMakeLists.txt`.

### Checked-in assets (`game/LithoKraft/assets/models/`, lowercase-hyphenated)
- `clockwood-wirewood.lthm` — 363 leaves
- `dopant-crystal-tree.lthm` — 1429 leaves
- `trace-forest-tree.lthm` — 853 leaves
- All L6, all round-trip `modelEquals`, all byte-identical on re-author. `assets/models/README.md` table updated.

### Changed (append-only, no renumbers)
`model/models.h` (+`kCrystalBlue=11`, `kCrystalAmber=12`, `kCrystalCore=13` for the dopant family), `model/models.cpp` (+3 `name()` cases), `content/CMakeLists.txt`, `assets/models/README.md`.

### Evidence
- `vegetation_selftest` 94/94: per-family save→load→`modelEquals` round-trip, re-author determinism, checked-in files match re-author byte-for-byte, palette binding per family (clockwood all-`kCopper`; dopant blue+amber+white-hot apex; trace-forest copper + red AND blue tips), determinism contract (same seed→same `hashLeaves`, different seed→different plant), `bind*Palette` pass-through, shrub 3D/2D paths, texture-set sanity, 11-biome table coverage (DeadZones all-zero, Fab Ruins flagged Organic/not-in-json), `blendedDensity` one-hot smoke test.
- Full `cmake --build`: exit 0, zero warnings on new targets.
- Full ctest at report time: 27 passed / 5 failed / 1 running — failures: `engine_sim` (grandfathered), `lithokraft_selftest` (grandfathered), `engine_procgen` / `engine_water` / `engine_index` / `engine_mcp` (sibling workstreams' in-flight edits, none touching vegetation). `engine_trees`, `engine_shrubs`, `model_selftest`, `authored_models_selftest` all pass.

### Seam notes (absorbed deviations, documented in VEGETATION_API.md)
The engine presets use placeholder material slot ids 1–9 ("Caller remap"); the game binding translates by per-family pattern match — hand-rolled params keep caller ids, and the selftest fails loudly if the engine ever changes a preset's slots. One real catch during the run: a WS1 fix changed the dopant generator (1433→1429 leaves) — the asset was regenerated and the checked-in-file comparison verified it cleanly.

---

## The API (as exposed)

```cpp
// Engine (litholattice::procgen) — deterministic generation core
litholattice::subdiv::SparseOctree generateTree(const TreeParams& p);
litholattice::subdiv::SparseOctree generateShrubOctree(const ShrubParams& p);
CrossQuadPlant generateShrubCross(const ShrubParams& p);
// Game (lithokraft::vegetation) — palette-bound, save-ready wrappers
lithokraft::model::Model generateTreeModel(const procgen::TreeParams&, std::string name, std::string author = "");
lithokraft::model::Model generateShrubModel(const procgen::ShrubParams&, std::string name, std::string author = "", ShrubCrossInfo* crossOut = nullptr);
```

Determinism contract: same (params, seed) → identical leaf stream, always. Verified by `hashLeaves` equality tests in all three suites.

**10-line example** (from `VEGETATION_API.md`):
```cpp
using namespace lithokraft::vegetation;
auto params = litholattice::procgen::clockwoodWirewood(0xC10C700D);
lithokraft::model::Model tree = generateTreeModel(params, "Clockwood Wirewood", "ProcGen");
std::string err;
lithokraft::model::saveModel("assets/models/clockwood-wirewood.lthm", tree, &err);
// 2D grass tuft:
litholattice::procgen::ShrubParams sp;
sp.seed = 1234; sp.style = litholattice::procgen::PlantStyle::CrossedQuads;
ShrubCrossInfo cross;
lithokraft::model::Model tuft = generateShrubModel(sp, "Meadow Tuft", "ProcGen", &cross);
// cross.textureName / cross.widthBlocks / cross.tintR... → Shape::Cross BlockDef
```

---

## Biome → vegetation mapping (11 biomes)

Densities are per-biome hints shaped for later `blendedDensity` consumption. Placement integration into worldgen is a **later step** (explicitly out of scope per JD). `*` = JD-approved, not yet in `wafer.json` (`inWaferJson=false` in the header).

| Biome | Tree (density) | Shrub (density) |
|---|---|---|
| TraceForest | TraceForestTree 0.060 | Berry 3D 0.100 |
| DicingPlains | — 0 | DryTuft 2D 0.012 |
| CapacitorGroves | ClockwoodWirewood 0.045 | Twisted 3D 0.060 |
| ResonatorSpires | — 0 | DryTuft 2D 0.020 |
| DeadZones | — 0 | — 0 |
| EtchedPlains* | — 0 | DryTuft 2D 0.025 |
| ClockwoodForest* | ClockwoodWirewood 0.085 | Berry 3D 0.075 |
| DopantGroves* | DopantCrystalTree 0.075 | Twisted 3D 0.065 |
| OxideDunes* | — 0 | DryTuft 2D 0.030 |
| StaticMarsh* | — 0 | MeadowTuft 2D 0.110 |
| FabRuins* (Organic) | — 0 | DryTuft 2D 0.020 |

---

## Decisions for JD to lock

1. **Two-colour tip alternation** (WS1): when both `tipMaterial` and `leafMaterial` are nonzero, extremities alternate by index. Alternative: tips strictly use `tipMaterial`, leaves only for blobs.
2. **Placeholder slot convention** (WS3): engine presets use material slots 1–9 ("Caller remap"), game binding translates per family. Alternative: engine takes no materials at all and game paints afterwards (less convenient for presets).
3. **CrossedQuads Model fallback** (WS3): the returned Model holds a thin voxel cross stand-in; the `ShrubCrossInfo` bundle is authoritative for the `Shape::Cross` path. Alternative: empty geometry + bundle only.
4. **Single-block ceiling for v1** (all): trees/shrubs generate within one 64-cell block (like the hand-authored `trace_tree`). Multi-block (taller) trees are a follow-up.

## Open items

1. **Worldgen placement integration** — the decoration stage consuming `fillTreeDensity`/`fillShrubDensity` + `blendedDensity`. Explicitly the next step, not this one.
2. **2D texture BlockDef registration** — register the three `plantTextureName` textures as `BlockDef::tex` entries per `shrubs.h` "2D RENDER BINDING" when the block registry work reaches it.
3. **More families** — Oxide Dunes glass shrubs, Static Marsh reeds, Fab Ruins growths; additional presets are cheap now that the core exists.
4. **Sibling test noise** — `engine_procgen`/`engine_water`/`engine_index`/`engine_mcp` failures belong to concurrent workstreams' in-flight edits; worth a clean-room ctest once the tree settles.

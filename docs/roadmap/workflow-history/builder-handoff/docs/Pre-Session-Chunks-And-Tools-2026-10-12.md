# Pre-Session: Chunk Streaming + Lore Tools — 2026-10-12

**End state: build clean · ctest 37/40 · game selftest 851 PASS / 6 FAIL (baseline was 840 / 7 — improved, no regressions). Walking generates the world. Every lore tool is now a real, holdable item with a model and an icon. The Fragment is in your hotbar at spawn.**

Prepared for JD + the builder's hands-on session (~5:07 PM EDT). JD's bar for this pass: working systems with documented caveats beats perfection. Caveats are in §4, plainly.

---

## 1. Chunk streaming — the world now follows the player

### Root cause (JD's catch was correct)
Terrain generation was only ever triggered in **one** place: `Dimension::load()` generates a single fixed square, `[0, sizeChunks)²`, anchored at the world origin (`engine/src/world/dimension.cpp:6-31`). `Game::loadActive()` calls it once at boot/dimension switch with `worldChunks` (default 16 → a 256×256-block square). After that, **nothing ever inserted another column** — the sim only ticks/meshes columns already in the `ChunkMap`, the save loader only overlays saved copies onto columns already in the map, and a missing column reads as void by design. A `BenchStreamer` existed, but it was bench-only and owned a private ChunkMap the sim never saw. (`biome_capture`/selftests pre-generating fixed regions was harness behavior, not the game path.)

### What was built
A new game-layer **`ChunkStreamer`** runs once per frame from `Game::update()`:

- Columns within the load radius around the player's chunk (`GameConfig::tickRadius`, same radius the sim ticks; default 8) that are neither loaded nor in flight are requested **nearest-first**, capped at 32 in flight on the existing `JobPool`.
- Each job is **disk-first**: a new `WorldSave::loadColumn()` (existing `WorldStore`/region format, block-name remap — save format untouched), falling back to the dimension's existing `ColumnGenerator`.
- Finished columns are inserted with the sim thread stopped — the exact stop/insert/start pattern `saveWorld()` already used — and meshed through the existing `submitMesh_` path.

No parallel chunk system, no sim-core changes, no save-format or net-protocol changes.

### Headless walk test (real Game, real player position — not a provider shortcut)
Driven through the headless harness (`lithokraft_headless`), teleport-walking the real player and reading terrain back through the sim's own map. **7 new checks, all PASS:**

- **`testChunkStreamingWalk`**: boot loads exactly the 4×4 pre-bake (16 columns). Player stepped +X in 32-block steps to x=392 — 13 sample points spanning x=8→392, i.e. **328 blocks past the pre-bake edge**. Terrain underfoot (surfaceY ≥ 0) at **every** sample. 288 columns requested by the streamer; map grew 16 → 272 columns.
- **`testStreamingPersistence`** (temp save dir): streamed terrain at x=200 (far outside the pre-bake) → save → fresh Game on the same save dir restores the player at x=200 and the terrain returns via streaming (disk-first per column; streamer column count > 0 on relaunch).
- Honest limit on the persistence proof: unmodified streamed columns are not written to disk — the existing save policy only writes dirty/component columns, because pure terrain is seed-regenerable — and deterministic worldgen makes regenerated terrain bit-identical, so the relaunch check proves the streaming *return path*, not the disk read specifically. Edited streamed columns *are* dirty, do persist, and `loadColumn()` serves them disk-first.

---

## 2. The lore tools — inventory table

Lore source: Glossary v2 "Tool & weapon names" (LOCKED), Lore Bible v0.2 §11, Lore Addendum 2026-10-12. **There are no EtchOR-family hand tools in lore** — EtchOR/SelectOR/ConnectOR/Laysor/PortOR/FabOR are machine blocks/components, deliberately not items. ReadOR is a Fragment-cartridge component (IN DESIGN), also not a hand item.

| Item | Item registered | .lthm model | Icon | In fresh inventory |
|---|---|---|---|---|
| **The Fragment** (starting item) | ✅ new — `kItemFragment` | ✅ new `fragment.lthm` | ✅ new | ✅ **granted on spawn, hotbar slot 0** |
| BashOR (hammer) | ✅ new | ✅ pre-existing | ✅ new | ✅ holdable |
| PickOR (pickaxe) | ✅ new | ✅ pre-existing | ✅ new | ✅ holdable |
| ChopOR (axe) — **tier 1 only** | ✅ new, tier=1, material=`trace_trunk` | ✅ pre-existing | ✅ new | ✅ holdable |
| HeapOR (shovel) | ✅ new | ✅ pre-existing | ✅ new | ✅ holdable |
| SeedOR (hoe) | ✅ new — Till action maps to existing `silicaTill` | ✅ pre-existing | ✅ new | ✅ holdable |
| SlicOR (sword) | ✅ new | ✅ pre-existing | ✅ new | ✅ holdable |
| Firewall (shield) | ✅ new — defense 8 | ✅ new `firewall.lthm` | ✅ new | ✅ holdable |
| Ping (bow — fires packets) | ✅ new — projectile `packet` | ✅ new `ping.lthm` | ✅ new | ✅ holdable |
| Callback (returning thrown weapon) | ✅ new — projectile, returnsToOwner | ✅ new `callback.lthm` | ✅ new | ✅ holdable |
| DebuggOR (debug stick) | ✅ new — Rotate action | ✅ new `debuggor.lthm` | ✅ new | ✅ holdable |
| Sniffor (handheld receiver) | ✅ new — **lore IN DISCUSSION** | ✅ new `sniffor.lthm` | ✅ new | ✅ holdable |
| Atlas (map item) | ✅ new — **name LOCKED, crafting TBD** | ✅ new `atlas.lthm` | ✅ new | ✅ holdable |
| ChipOR (as item) | ✅ new — block-backed item (itemId == `kChipOR`, placesBlock = ChipOR) | ✅ pre-existing (`chipor-demo.lthm`) | ✅ pre-existing | ✅ holdable |
| Silica Seed (existing) | ✅ now in registry (was ad-hoc const) | n/a (material) | ✅ pre-existing | ✅ stacks to 512 |

**Tool/weapon split preserved** per the locked rule: `isTool`/`isWeapon` are independent flags plus `meleeDamage`. BashOR is both (hammer + mace, melee 6); SlicOR is both (sword, melee 7, + Slice action); PickOR/HeapOR/SeedOR/ChopOR are tools (ChopOR also melee 4); Firewall/Ping/Callback are weapons.

**ChopOR tiers:** tier 1 only, exactly one ChopOR def — tier 1 = surface-tree material (`trace_trunk`), as decided today. `tier` + `material` fields are the clean extension point. **No tier 2+ items were invented** (asserted by test).

### Acceptance
New **`lithokraft_items_selftest`: 18/18 PASS** — registry uniqueness/lookups, unified-registry registration (ChipOR `placesBlock` correct), append-only id offsets, tool/weapon split, the ChopOR tier rule, every model stem loads as a non-empty .lthm, and the fresh-inventory suite: fresh `Inventory` starts empty → `grantStartingItems()` puts the Fragment in hotbar slot 0 → grant is idempotent → `addItem` holds every one of the 15 defs → seed still stacks to 512. The game selftest additionally verifies against the real pack stack: every item icon resolves at 16×16, and a fresh inventory holds the Fragment + all tools.

**Bonus:** the selftest's long-standing **hotbar-icons failure now PASSES** — all 15 previously missing block icons were generated from their existing .ltxr recipes (portor, fusor, trace_bark/canopy, trace_tip ×2, capacitor_side, quartz_spire, the five wafer grounds, structure_block).

---

## 3. Test results vs baseline

| | Baseline | Now |
|---|---|---|
| Build | clean | **clean** |
| ctest | 36/39 | **37/40** (+1: new `lithokraft_items_selftest`, passing; headless suite now includes the 7 streaming checks, passing) |
| Game selftest | 840 PASS / 7 FAIL | **851 PASS / 6 FAIL** |

Remaining failures are exactly the pre-existing set: `lithokraft_selftest`'s 4× save/restore + mesher sub-chunk + Vulkan environmental; `lithokraft_scout_selftest`; `lithokraft_chipor_selftest`. (`engine_audio` flaked once under ctest during these runs — its binary predates both crews' edits, neither lane touches audio, and it passed standalone on rerun; it is the known flake.) Concurrency note: the two crews (this pass + the concurrent superformula crew) briefly broke the build once via an ambiguous `ItemDef` in a new file; fixed within minutes, clean since. No baseline pass regressed.

---

## 4. Caveats for the hands-on session — plainly

**Chunks:**
- **No chunk unloading yet (deliberate first pass).** Memory grows with explored area; a long walk keeps every generated column resident and meshed. Unload + save-on-unload is the obvious follow-up.
- **Insertion hitch.** Inserting a finished batch stops/restarts the sim thread (same as a save does). Batches are small and generation is async/nearest-first, but a fast fly/teleport across many chunk borders can micro-stutter; the sim's world-tick counter also resets on each restart (same as today's autosave behavior).
- **Spawn-area surge.** The pre-bake square is origin-anchored and spawn sits near its corner, so at boot roughly a radius-8 ring (~200 columns in the 16-chunk config) streams in over the first seconds. Playable during it (nearest first), but the horizon fills in progressively.
- **Radius = tick radius.** Streaming uses `cfg_.tickRadius` (settings/`--radius`, default 8). Radius 0 = nothing beyond the pre-bake generates.
- **Signal state on streamed-in columns.** Blocks load from disk, but a streamed column's SGNL payload is *not* imported into the live SignalSim (import only happens at dimension load; a rescan isn't safe). Circuits built far from spawn in a *previous* session show their blocks after streaming in, but component timers/memory won't resume until they're inside a future boot's pre-bake. Same-session play is unaffected.
- **Bench note:** `--bench-path stream/soak` now also exercises the real streamer (the bench camera moves the player), so bench numbers from this build onward include real streaming cost — more representative, not comparable to older runs.

**Tools:**
- **Holdable ≠ fully usable, yet.** Defs carry `toolAction`/`meleeDamage`/projectile data, but per-tool world-interaction handlers in the break/place path are not wired, and the damage system is still provisional. Exception: SeedOR's Till maps onto the existing silica till/plant functions.
- **The Fragment can't be opened yet.** Item, spawn grant, and its state/gallery/markdown data models all exist — but there is **no Fragment UI in the game at all** yet (pre-existing gap, documented in `fragment_markdown.h`). Opening the book from inventory needs that UI built.
- **Hotbar vs inventory.** The HUD hotbar is still the BlockId-typed creative hotbar; the tools live in the real 42-slot `Inventory` (spawn/give/pickup). `Player::inventory` is not yet persisted in player.dat (pre-existing — PlayerMeta stores only the block hotbar).
- **Sniffor / Atlas:** registered and holdable for inventory completeness, but Sniffor's lore is IN DISCUSSION and Atlas crafting is TBD — JD's calls, not made here.

---

## 5. File-level changelog

### Chunk streaming (new: 2 files; modified: 7)
- **Created** `game/LithoKraft/src/game/include/lithokraft/game/chunk_streamer.h` — ChunkStreamer class + design/root-cause comment.
- **Created** `game/LithoKraft/src/game/chunk_streamer.cpp` — nearest-first request scheduling (32-in-flight cap), disk-first/generate job bodies, ready queue.
- **Modified** `game/LithoKraft/src/game/CMakeLists.txt` — `chunk_streamer.cpp` added to `lithokraft_game`.
- **Modified** `game/LithoKraft/src/game/include/lithokraft/game/world_save.h` — `loadColumn()` declaration; cached `StableRemap` + mutex members.
- **Modified** `game/LithoKraft/src/game/world_save.cpp` — `loadColumn()` implementation; `store()` mutex-guarded (loadColumn runs on job threads).
- **Modified** `game/LithoKraft/src/game/include/lithokraft/game/game.h` — `streamer_` member, `pumpStreaming()`, small test surface (`playerPosition`, `setPlayerPosition`, `loadedColumnCount`, `streamedColumnCount`, `surfaceYAt`).
- **Modified** `game/LithoKraft/src/game/game.cpp` — streamer built in `loadActive()`, reset in `switchDimension()`/`~Game()`, `pumpStreaming()` called per frame in `update()`.
- **Modified** `game/LithoKraft/src/headless/headless_harness.h` / `.cpp` — `Config::saveDir` plumbed into `GameConfig`.
- **Modified** `game/LithoKraft/src/headless/harness_selftest.cpp` — `testChunkStreamingWalk`, `testStreamingPersistence`, `waitForSurface` helper (7 new checks).

### Items & tools (new: item registry, 7 models, 13 recipes, test binary; modified: 12 files + pack manifests)
- **Created** `game/LithoKraft/src/content/include/lithokraft/content/items.h` — append-only item ids (`kFirstItemId+1..+13`, seed was +0), `ItemDef` (tool/weapon split, tier/material, model/icon), `registerGameItems()`.
- **Created** `game/LithoKraft/src/content/items.cpp` — 15 defs + lookups + registration into the engine's unified `ContentRegistry` item kind (ChipOR `placesBlock`).
- **Created** `game/LithoKraft/src/app/items_selftest_main.cpp` — `lithokraft_items_selftest` binary (18 checks).
- **Created** `game/LithoKraft/assets/models/{firewall,ping,callback,debuggor,fragment,sniffor,atlas}.lthm` — 7 new micro models via author_models.
- **Created** `game/LithoKraft/assets/texture_recipes/items/*.ltxr` — 13 item icon recipes.
- **Created** 81 icon PNGs (27 names × pack_16/32/64 `textures/ui/icons/`): 13 item icons + 14 previously missing block icons, all via `texture_gen`.
- **Modified** `game/LithoKraft/src/content/blocks.cpp` — items registered into the unified registry from the accessors (post-build, pre-seal; ChipOR resolved by string id).
- **Modified** `game/LithoKraft/src/content/CMakeLists.txt` — + `items.cpp`.
- **Modified** `game/LithoKraft/src/content/model/models.h` / `models.cpp` — 7 new author functions + append-only registry rows (8 pre-existing models regenerated byte-identical).
- **Modified** `game/LithoKraft/src/content/model/author_models.cpp` — count-agnostic completion message.
- **Modified** `game/LithoKraft/src/content/model/README.md`, `game/LithoKraft/assets/models/README.md` — new model rows with leaf counts.
- **Modified** `game/LithoKraft/src/game/include/lithokraft/game/inventory.h` — `grantStartingItems()` (idempotent Fragment grant).
- **Modified** `game/LithoKraft/src/game/game.cpp` — fresh-spawn Fragment grant in the Game constructor (after save restore; only when not held).
- **Modified** `game/LithoKraft/src/game/include/lithokraft/game/resource_manager.h` + `resource_manager.cpp` — `itemIcon(itemId)` loading `textures/ui/icons/<iconName>.png` (16×16) for non-block items.
- **Modified** `game/LithoKraft/src/app/texture_gen.cpp` — new `--subdir <rel>` option (default `textures/blocks`); one icon pipeline, reused.
- **Modified** `game/LithoKraft/src/app/CMakeLists.txt` — `lithokraft_items_selftest` ctest target.
- **Modified** `game/LithoKraft/src/app/selftest.cpp` — 2 new checks (item icons; fresh-spawn inventory).
- **Modified** `game/LithoKraft/assets/packs/pack_{16,32,64}/manifest.json` + `resources.json` — 27 new icon entries per pack; refreshed via `engine/tools/convert_pack_manifest.py`.

**Untouched per lanes/guardrails:** vegetation generation, Formula node code, sim core, save format, net protocol, `trace_jit_basic.cpp`, the extern-const block registry (append-only additions only).

---

## 6. For the session — suggested first checks

1. **Walk.** Spawn, pick a direction, walk 300+ blocks. Terrain should keep arriving ahead of you (nearest-first); watch the horizon fill-in during the first seconds after boot (the spawn surge, §4).
2. **Open the inventory.** Fragment in hotbar slot 0; all six -OR tools + Firewall/Ping/Callback/DebuggOR present with models and icons.
3. **Don't judge tool *actions* yet** — holding is wired, per-tool world interaction is the session's fix-it territory (§4).

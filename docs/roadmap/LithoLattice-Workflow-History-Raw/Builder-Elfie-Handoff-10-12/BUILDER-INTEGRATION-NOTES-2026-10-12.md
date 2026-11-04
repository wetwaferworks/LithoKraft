# Builder Integration Notes — 2026-10-12 (17:20 EDT)

**What this package is:** the complete LithoLattice/LithoKraft source tree as it
stands on Elfie's machine after the 2026-10-11/12 work, plus all project
documentation and the visual frames from today's Wafer passes. Drop the
`source/` tree over (or beside) the local tree; nothing in the local tree is
needed from before this weekend except local build config. Build per
`source/BUILD-SETUP-JD.md`.

**Verified state at packaging:** build clean · ctest 37/40 · game selftest
851 PASS / 6 FAIL. Remaining failures are ALL pre-existing: 4× save/restore
round-trip, mesher surface sub-chunk, Vulkan headless (environmental, no GPU),
scout ModelResolver, chipor stale registry guard.

---

## What landed while the builder was offline

### 1. Review-fix program (Elfie, JD directive — all critical/high/medium)
Full top-to-bottom code review (68 findings) fixed and verified. Headlines:
SimThread exception boundary; release signing-key hygiene (release builds
hard-refuse boot on the test key, exit(1)); console permission laundering;
hot-swap def() race; alias cycle guard; net delta saturation; JSON depth cap;
asset symlink canonicalization + PNG bomb cap; script quota UAF.
Report: `docs/Code-Review-Morning-Report-2026-10-12.md`,
`docs/Builder-Handoff-Final-2026-10-12.md` (master packet — read it first).

### 2. Block registry extensibility refactor (2026-10-12)
The 54 hardcoded `constexpr BlockId` constants are now `extern const`,
resolved at startup from the registry by string ID (`resolveId()` in
`game/LithoKraft/src/content/blocks.cpp`). Vanilla registration order is
append-only — save IDs are unchanged. New mod API: `registerBlock(BlockDef)`
(single-threaded mod-init only, before world load). Never hardcode a numeric
BlockId again. `kAir = 0` stays engine-side constexpr.
The BIG version (dynamic mod loading + ID remapping on save-load) is still a
proposal — do not build it without JD's call.

### 3. Tick debugger
`tick_freeze` / `tick_step <n>` / `tick_unfreeze` console commands;
`SimThread::stepTicks(uint64_t)`. KNOWN DEFECTS (open): stepped branch counts
master steps (may advance fewer world ticks under faster domains); stepped
path skips the external random-tick call; pause state can desync between game
and SimThread. Repair is builder work.

### 4. Subdivision / micro-model system (complete)
Sparse octree to 64³ micro-cells, mesher LOD, `.lthm` model format, ChipOR
viewport blocks, 8+ authored models, silica farm loop. Report:
`docs/Subdivision-Report-2026-10-11.md` (in the user-files set).

### 5. Wafer worldgen fixes (2026-10-12, First-Day crew)
- The wafer is now PINNED to seed 0 at the game wiring layer
  (`dimensions.cpp`) — the documented design is seed-independence; the world
  seed re-rolling the wafer was a bug. Existing `--seed` worlds: unsaved
  wafer columns regenerate canonical.
- Crust allocation test restated (resonator spires legitimately allocate a
  4th section on some columns).
- `biome_capture.cpp` no longer injects a duplicate `trace_trees` feature
  (the pack worldgen generator validates now) — and the capture tool now
  loads resources exactly like the game. THE WHITE SCREENSHOTS WERE A
  CAPTURE-TOOL BUG; the game itself was never untextured.
Report: `docs/First-Day-On-The-Wafer-2026-10-12.md`.

### 6. Procedural texture generator (NEW engine module)
`engine/src/texture/` → `litholattice_texture`. Clean-room implementation;
recipes are data files (`.ltxr`, line-based node format) in
`game/LithoKraft/assets/texture_recipes/` (32 recipes). Seeded/deterministic,
seamless tiling by construction, (recipe, seed, size)-keyed LRU cache.
Bake tool: `texture_gen` (game app) regenerates pack PNGs at 64/32/16.
`engine_texture` ctest: 47/47. Measured: all 32 recipes bake in ~44 ms.
Report: `docs/Procedural-Texture-Generator-2026-10-12.md`.

### 7. Superformula vegetation + H3 FIXED
- **H3 is closed**: Formula node polar axis now matches the voxelizer
  (world Y). Regression test uses the non-spherical starfish preset:
  old mapping 0.8756 (fail) → new 0.9673 (pass), in `engine_litho_formula`.
- Trace-tree canopies: 4 superformula archetypes (Round/Spire/Parasol/Lobed)
  with per-instance variance keyed to world position (deterministic across
  chunk borders). Shrubs: superformula cross-sections; dry-tuft shrubs place
  via `GroundCoverFeature`. 3 new `.lthm` gallery models for the ChipOR
  viewport. Canopy stamp 0.079 ms/tree.
- REMAINING: Modeled3D octree shrubs do not stamp into voxel chunks yet.
Report: `docs/Superformula-Vegetation-2026-10-12.md`.

### 8. Chunk streaming (NEW)
Generation is no longer boot-only. Game-layer `ChunkStreamer` runs per frame
from `Game::update()`: columns within the tick radius (default 8) around the
player, ≤32 in flight on the JobPool, disk-first via new
`WorldSave::loadColumn()` (existing region format — save format untouched),
generator fallback, insert with sim stopped. Headless walk test: player
stepped 328 blocks past the pre-bake edge, terrain at all samples, map grew
16 → 272 columns, streamed terrain persists across save/relaunch.
CAVEATS: no chunk unloading yet (memory grows with explored area); insertion
can micro-stutter under fast flight; circuits built far from spawn in a
PREVIOUS session show blocks after streaming in but their SignalSim state is
not imported (same-session play unaffected).

### 9. Lore tools are real items (NEW)
Before today NONE of the lore tools were registered items. Now 15 defs in the
item registry (append-only), all with models and icons (icons via the .ltxr
pipeline): **the Fragment (granted on fresh spawn, hotbar slot 0)**; BashOR,
PickOR, ChopOR, HeapOR, SeedOR, SlicOR; Firewall, Ping, Callback, DebuggOR,
Sniffor, Atlas; ChipOR as a block-backed item; Silica Seed. ChopOR is
**tier 1 only, material `trace_trunk`** — tier/material fields are the
extension point; the tier ladder beyond 1 is UNDECIDED (JD), do not invent
it. `lithokraft_items_selftest`: 18/18. The hotbar-icons selftest failure is
fixed as a side effect (all block icons now generated).
CAVEATS: tools are holdable; per-tool world-interaction handlers are NOT
wired; damage values provisional; **no Fragment UI exists yet** (item, grant,
and data models are ready); `Player::inventory` is not persisted in
player.dat yet (pre-existing); HUD hotbar is still the BlockId creative bar.

## JD design decisions from 2026-10-12 (locked unless marked open)
- **Beta ships CRUST-ONLY.** The Wafer layer stack (crust → passivation →
  dielectric → polysilicon → buried oxide → Substrate) is a POST-HANDOFF
  task. Do not fill the underneath in this session.
- **Phosphoric acid** is the Wafer's hot flowing liquid; acid + water →
  **silicon nitride** blocks (extremely hard; the obsidian slot).
- **Sapphire** is the diamond counterpart: the most valuable item on the
  Wafer, mined deep at diamond depth near the acid (post-stack).
- **Certificate Table** ingredient set: the **Remnant** (a craftable book;
  writing pair is Remnant + **Stencil**, the Book-and-Quill counterpart) +
  silicon nitride (obtained, not crafted) + sapphire. Counts/framing open.
- **Minecraft Equivalence Rule**: early progression mirrors Minecraft's
  shape (punch trees → logs → planks → sticks → ChopOR); expression is ours.
  ChopOR tier 1 = surface-tree material.
- `game/LithoKraft/CREDITS.md` now exists in-tree and is maintained as work
  lands.

## Open queue for this session (JD + builder, Elfie in the loop)
1. Pre-existing selftest failures: 4× save round-trip, mesher sub-chunk.
2. scout ModelResolver one-liner; chipor stale registry guard.
3. MCP stdio server not wired in `main.cpp` (player-path automation).
4. Spawn floats ~11 blocks above the demo platform (reach fix).
5. Fragment UI (first version) — item/data ready.
6. Tool world-interaction handlers + damage tuning pass.
7. Tick-debugger repair (see §3).
8. Modeled3D shrub stamping; silica worldgen feature (system exists, no
   worldgen caller at spawn yet for silica).
9. JD decisions pending: H2 handshake interop ({3} vs {1,2,3}); RELEASE
   SIGNING KEY REPLACEMENT (test key is compromised in git history — never
   trust it; do not claim history was scrubbed); Cosmic worldgen wiring;
   Vulkan selftest SKIP proposal (treat incompatible-driver as [SKIP]).

# Infinite Registry — Phase 2 Report (2026-10-12)

**Status:** JD-LOCKED Option B (unified registry) implemented. Engine builds clean,
all engine tests pass (23/23 ctest), game sources compile.
**Scope:** Fork 4 only. Forks 2 (seal vs hot-swap) and 3 (quarantine vs Air) NOT touched.
Untouched as instructed: sim core, trace_jit, netcode wire format (V3 stays), save format,
procgen, subdiv, assets, storage.

Report: `~/workspace/user/files/Infinite-Registry-Phase2-Report-2026-10-12.md`
Design: `~/workspace/user/files/Infinite-Registry-Design-2026-10-12.md` (§6 fork 4)

---

## 1. What was built

**One kind-tagged `ContentRegistry`** (`engine/src/world/include/litholattice/world/block.h`;
`stableIdFor()` in `world/hash.h`). Four files changed total:
- `world/hash.h` — added `stableIdFor(kindName, id)`: FNV-1a-64 over `kind + ":" + id`,
  fed piecewise (byte-identical to hashing the joined string; verified by test).
- `world/block.h` — `EntryKind`, `ItemDef`/`ItemId`, `ContentRegistry`, `BlockRegistry`
  and `ItemRegistry` typed facades.
- `world/voxel_palette.h` — kind check in `resolveBlockIds` (fail-fast assert).
- `engine/tests/engine_tests.cpp` — 2 phase-1 checks updated for the new derivation,
  new `testInfiniteRegistryPhase2()` suite.

**Identity scheme.** Stable id = `FNV-1a-64(kindName + ":" + namespacedId)`; string key =
`(kind, namespace:path)`. `lithokraft:copper` can be both a block and an item with no
collision. Duplicate rejection on either key, first-wins, never silent overwrite — same
locked WS2 semantics as phase 1, now with supplied stable ids checked **globally** across
kinds (a stable id names exactly one entry, so `kindOf()` is unambiguous).

**Typed views, zero call-site churn.** `BlockRegistry` keeps its exact phase-1 public API
(`add`, `def`, `stableIdOf`, `findById`, `findByStableId`, `findByTexture`, `addAlias`,
`crcOf`, `has`/`hasAll`, `size`, `reserve`, `defByStableId`, `idOf`) and now delegates to
the unified table filtered to kind==Block. It owns a `ContentRegistry` when
default-constructed (tests, game `mutableRegistry()`), or views an external one
(`registry.blocks()` / `registry.items()`). `ChunkMap::setRegistry`, the 109+ game call
sites, `BlockId` currency, and the mesher path are byte-for-byte untouched — verified by
compiling all 15 registry-touching game sources with real build flags (see §3).

**`ItemDef`.** Same treatment as `BlockDef`: namespaced string id, display name, maxStack,
behavior, per-entry CRC32 over canonical content (name, maxStack, placesBlock, behavior —
string id excluded), plus `placesBlock: StableId` (0 = not a block-item). BlockItems are
a field, not a mapping table. `ItemId` is `uint32_t` (matches `inventory.h`); id 0 is the
empty sentinel (no def stored; `itemDef(0)` returns a static empty def — matches the
existing "itemId 0 = empty" convention).

**Kind safety on the voxel path.** `VoxelPalette::resolveBlockIds` now checks
`reg.kindOf(sid)` per palette entry and fail-fast asserts `kind == Block` — once per
palette entry, never per voxel. Debug: aborts loud. Release (asserts compiled out): the
kind-filtered `findByStableId` maps the offender to `kAir` instead — loud in dev, safe
in prod. The W64 degenerate path was already nullopt→kAir safe.

**No new plumbing, ever.** `ContentRegistry::registerKind(name)` is the extension door:
deduplicates on name, mints a session-local kind id. The class docs carry the 3-step
"Adding a content kind" recipe (registerKind → def vector → typed accessors funneling
through the generic `insertEntry()`). No new id scheme, no new remap path, no new wire
codec, no new save table — future kinds reuse all of it. Numeric kind ids are
session-local by design (only the name feeds stable-id derivation); never persist them.

## 2. Migration notes (phase 1 → 2)

The kind prefix changed every stable id: air went `0x82ca3801816336c9` →
`0xf99811fbedcd14d0` (`fnv1a64("block:litholattice:air")`). This was safe by construction:
- **Saves:** LLBN name tables store *string* ids; `readStableRemap` re-derives stable ids
  fresh every load via the registry. BLKS stores uint16 save-time ids. No stable id is
  ever persisted → no save/world migration needed. Old worlds load and re-derive.
- **Network:** `BlockUpdate` stable ids are derived per-session on both peers; V3↔V3 peers
  run the same derivation. Wire format unchanged (u64 varint).
- **Hardcoded constants:** the only one was `kAirStableId` itself (updated in place, plus
  its two test assertions). The `0x82ca3801816336c9` in the BlockUpdate test is arbitrary
  varint-codec test data, not an air reference — left as-is.
- **In-memory:** everything re-registers per session.

## 3. Test evidence

**Updated phase-1 checks** (behavior changes from the locked work, not hacks):
- `kAirStableId == fnv1a64("block:litholattice:air") == stableIdFor("block", "litholattice:air")`
- `rockSid = stableIdFor("block", "test:rock")` (was `fnv1a64("test:rock")`)

**New `testInfiniteRegistryPhase2()`** — 34 checks, all passing:
- Cross-kind determinism: same name → different ids per kind; same (kind,name) → identical
  ids across registry instances; `stableIdFor("block", id) == fnv1a64("block:id")`.
- Dup rejection per (kind,id): `m:thing` registers as both block and item; re-adding
  either kind is first-wins.
- Supplied stable ids rejected globally: block takes `0xABCD1234`, item with the same
  supplied id → nullopt (and vice-versa within kind).
- `kindOf()`: block/item/unknown; cross-kind `findByStableId` returns nullopt both ways;
  `defByStableId` on an item sid → air def (safe).
- `placesBlock` round-trip: item → block and back; plain items have `placesBlock == 0`.
- Item CRC32: cross-session determinism, content-sensitivity, string-id exclusion,
  `placesBlock` feeds the CRC.
- Item id 0 empty sentinel; real items start at 1.
- `registerKind`: mints/dedups kinds, `kindName` round-trips, extension kinds join the
  tagged id space.
- Facade owning mode (`BlockRegistry ro;`) behaves exactly as phase 1.
- LUT kind-mismatch: `kindOf` flags an item sid (the assert predicate); block-view
  `findByStableId` rejects it (release fallback); honest block palettes still resolve.

**Results:**
```
cmake --build . --target litholattice_world      → clean (2 Release-mode
  -Wunused warnings from compiled-out asserts fixed with [[maybe_unused]])
cmake --build . (all targets)                    → clean, no errors
./tests/litholattice_tests world                 → 63 [PASS], ENGINE TESTS OK
./tests/litholattice_tests net / sim / storage / render / core → ENGINE TESTS OK (each)
ctest -R "engine_"                              → 23/23 pass, repeatedly
Game sources (15 files incl. blocks.cpp, world_save.cpp, biome_capture.cpp):
  compiled clean with real flags via compile_commands.json
```
Flakes observed (both unrelated to this work — neither suite touches the registry):
- One ctest run showed a single `engine_procgen` failure; 9+ subsequent runs (full suite
  and standalone, incl. `--repeat until-fail:3`) all pass. Load flake.
- One ctest run hung on `engine_mcp` (a server test; its only `block.h` mentions are doc
  comments). Killed after 8 minutes; all registry-adjacent suites re-verified green after.
`lithokraft_game` link still blocked by the missing X11 dev headers (`X11/Xcursor/Xcursor.h`) —
pre-existing environmental gap from phase 1, unrelated to this work.

## 4. Bugs found and fixed (in code touched)

1. **Forward-declaration ordering** (`block.h`): `ContentRegistry` declares `blocks()`/`items()`
   returning the facade types — compiler error until `BlockRegistry`/`ItemRegistry` were
   forward-declared ahead of it. Fixed, verified by build.
2. **Dangling reference in my own new test** (caught by self-review before compiling):
   `BlockRegistry& bv = cr.content().blocks()...` bound a reference to a temporary view.
   Removed; the test uses named view objects.
3. **Release-mode `-Wunused-but-set-variable`** on the two assert-guarded locals
   (`ContentRegistry` ctor's air id, `resolveBlockIds`' kind): fixed with `[[maybe_unused]]`.

No pre-existing bugs in others' code surfaced this phase (phase 1's `biome_capture` find
remains the standing catch). No test was rewritten to force green: the two updated
phase-1 assertions track the locked derivation change.

## 5. Left for the builder

- **Game link:** install X11 dev headers (`libxcursor-dev` or equivalent) — then
  `lithokraft_selftest` can run the full game suite against the unified registry.
- **Inventory migration to StableId currency** (defined follow-up): `inventory.h` stacks
  still carry `uint32_t itemId`; player-data saves persist whatever the inventory holds.
  The registry side is ready (`itemStableId`/`findItemByStableId`/`kindOf`).
- **WS5 remainder:** "every block auto-gets an item form" — game-side policy decision
  (which blocks get item forms, default maxStack/behavior), then register via `addItem`
  with `placesBlock` set.
- **Forks 2 and 3** (seal vs hot-swap; quarantine vs Air) — untouched per instructions.
  Note: the unified table means each lands once, for all kinds.
- **`engine_audio` under-load flake** — pre-existing, unrelated.

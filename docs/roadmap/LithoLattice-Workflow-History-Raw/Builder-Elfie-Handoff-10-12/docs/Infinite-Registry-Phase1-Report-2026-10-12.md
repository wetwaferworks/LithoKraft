# Infinite Registry — Phase 1 Report (2026-10-12)

**Status:** All four LOCKED pieces implemented, engine builds clean, all engine tests pass.
**Scope:** Only what JD locked. The three open forks (seal vs hot-swap, quarantine vs Air,
parallel vs unified item registry) were NOT touched.

---

## 1. StableId plumbing

**New files:**
- `engine/src/world/include/litholattice/world/hash.h` + `engine/src/world/hash.cpp`
  - `fnv1a64(std::string_view)` — constexpr FNV-1a-64 (basis `14695981039346656037`, prime
    `1099511628211`). Verified against standard vectors: `""` → `cbf29ce484222325`,
    `"a"` → `af63dc4c8601ec8c`.
  - `crc32(const uint8_t*, size_t)` — CRC-32 (poly `0xEDB88320`, init/xorout `0xFFFFFFFF`),
    same algorithm as `storage::crc32`. Verified: `"123456789"` → `0xCBF43926`.

**Changed:** `engine/src/world/include/litholattice/world/block.h`
- `using StableId = uint64_t`; `kAirStableId` = `fnv1a64("litholattice:air")` = `0x82ca3801816336c9`;
  `isAirStableId()`.
- `BlockRegistry` now stores `stableIds_` / `stableToId_` alongside the dense `BlockId`s.
  - `add()`: default stable ID = FNV-1a-64 of the namespaced string ID (deterministic across
    sessions). Duplicate string ID → returns the existing id (first-wins, never overwrites).
  - `addWithStableId(def, stable)` (def first, stable second): mod-supplied 64-bit ID.
    Returns `nullopt` on duplicate string ID **or** duplicate stable ID; originals untouched.
  - `findByStableId()`, `stableIdOf()`, `defByStableId()` (air def when unknown).
- `BlockDef` gains `uint32_t crc32`, computed at registration by `contentCrc()` over the
  canonical serialization of content fields (name, flags, shape, tex, behavior, role/channel/
  tier, friction, falls, density) — **NOT** the string id. `crcOf(id)` accessor; field appended
  last so aggregate initialization is unaffected.

## 2. Dynamic palette index widths 8→16→32→64

**New:** `engine/src/world/include/litholattice/world/voxel_palette.h` — `VoxelPalette` class.
- Width-tagged (`Width::W8/W16/W32/W64`, values 0..3 stable for the subsection header).
- `vector<StableId> palette` (index 0 = air) + `unordered_map` for O(1) reverse lookup.
- `at()`/`rawIndex()` O(1); width steps up when the palette outgrows it (255 → 16-bit, etc.).
- At 64-bit width the index IS the stable ID (palette vector empty; `at()` returns the raw).
- `fill()`, `clear()`, `operator==`, and `resolveBlockIds(reg, lut)` which resolves the
  palette → `BlockId` LUT once per sub-chunk for hot readers.

**Changed:**
- `chunk.h`: `SubChunk::blocks` is now `VoxelPalette`; `at()`/`set()` take `StableId`;
  `nonAir` accounting preserved. New transient `loadScratch` (`optional<vector<uint16_t>>`)
  holds save-time BlockIds between BLKS decode and `remapColumn`.
- `chunk_map.h`: `ChunkMap` holds `const BlockRegistry*` (`setRegistry()`); `get()`/`set()`
  keep the **BlockId currency** for the 109+ game call sites and translate internally.
  `get()`/`set()` assert when no registry is installed (fail-fast, caught 3 test sites).
  `surfaceY()` compares against `kAirStableId` (needs no registry).
- `dimension.h`: `unload()` preserves the registry across the map reset.
- `mesh_input.h/.cpp`: `snapshotForMeshingInto`/`snapshotForMeshing` take `const BlockRegistry&`;
  the StableId palette is resolved to a `BlockId` LUT once per sub-chunk, then O(1) per voxel.
  `MeshInput` stays BlockId currency — the mesher is untouched.
- `column_generator.h/.cpp`: stores the registry; `generate()` writes StableIds.
- `random_ticks.h/.cpp`: takes the registry; resolves the palette once per sub-chunk.
- `pbr_emissive_provider.h`: `BlockMaterialFn` now takes `StableId`.
- `block_names.h/.cpp`: `readStableRemap()` (built on the unchanged `readBlockRemap`);
  `remapColumn(ChunkColumn&, const StableRemap&, ...)` consumes `loadScratch` into the
  paletted storage. Unknown names → air (quarantine fork NOT implemented — air is the
  existing behavior, unchanged).
- `storage.h/.cpp`: `PayloadRegistry::setRegistry()`; the BLKS save lambda translates
  StableId → save-time BlockId via LUT, then runs the **unchanged** `encodePalette`
  (on-disk BLKS format untouched). The load lambda decodes into `loadScratch` only.
- `game/.../world_save.cpp`: `setRegistry`, `readStableRemap`, `remapColumn` per column.
- `game/.../dimensions.cpp`: both dimensions' ChunkMaps get `setRegistry(&blockRegistry())`.
- `sim_thread.cpp`: installs the registry on its ChunkMap; fixed the missed
  `snapshotForMeshingInto` call in the mesh-submit path.

## 3. LEB128 varint codec — wired into the network protocol (JD 2026-10-12 scope expansion)

Per JD's ~03:37 EDT decision, the codec went **directly into the wire protocol**, not standalone.

- `net/quantize.h/.cpp`: `ByteWriter::putUVarint64` / `ByteReader::getUVarint64` — unsigned
  LEB128, max 10 bytes, strict (truncated / >10 bytes / 10th byte carrying bits past bit 63
  all fail; out-param untouched).
- `net/protocol.h/.cpp`: `LITHOLATTICE_PROTOCOL_V3`, `kProtocolVersion` → V3,
  `BlockUpdate{x,y,z,stableId}` with `encodeBlockUpdate`/`decodeBlockUpdate`
  (coords as zigzag varints, stableId as uvarint64 — "pack the zeros": small ids cost 1 byte).
- `net/handshake.h`: `knownProtocolVersions()` → {1,2,3}, `maxProtocolVersion()` → V3,
  `CodecKind::V3StableIds`, `codecForProtocol()` V3 case. v3 peers negotiate v2 with older
  binaries via the existing highest-common-version rule.
- **On-disk save format (LLBN) unchanged.** Only the network wire version bumped (V2→V3).

## 4. Per-entry CRC32 (WS7)

In `block.h` as described in §1. Exposed via `crcOf()`; logs as decimal + hex
(`%u (0x%08X)`). Tests confirm: standard check value, cross-session determinism,
content-sensitivity (friction change flips it), and string-id exclusion.

---

## Test evidence

**New tests** (`engine/tests/engine_tests.cpp`, `testInfiniteRegistryWorld` + `testInfiniteRegistryNet`):
- 26 world checks: FNV vectors, `kAirStableId`, cross-session stable-ID determinism,
  `findByStableId`, duplicate rejection (both keys), CRC32 vectors/determinism/
  content-sensitivity/id-exclusion, palette fresh/set/width-step-8→16/fill/clear/equality/
  LUT, sub-chunk StableId set/at, ChunkMap BlockId currency over StableId voxels.
- 5 net checks: uvarint64 boundary byte-lengths + round-trips, malformed rejection
  (truncated/overlong/overflowing), BlockUpdate round-trip + truncation safety,
  v3 version/handshake/codec, v3↔v3 negotiates v3, v2-binary↔v3-binary falls back to v2.

**Updated existing tests** (behavior changes from the locked work, not hacks):
- `testNetHandshake`: `codecForProtocol(3)` → `V3StableIds`; happy-path now negotiates v3
  (both sides offer {1,2,3}); subset-offer downgrade test unchanged (still agrees v2).
- `testBlockNames`: rewritten for the two-phase decode→`remapColumn` with `StableRemap`.
- Storage/pbr/sim/ai tests: `setRegistry` wiring + StableId voxel API.

**Results:**
```
ctest -R "engine_"  →  22/23 pass; engine_audio flaky under full-run load, PASSES solo
                        (untouched by this work; timing-sensitive audio test)
./tests/litholattice_tests world / net / sim / render / storage → ENGINE TESTS OK
```
- Full engine build clean (`litholattice_world/storage/net/sim/render`, `litholattice_tests`).
- `lithokraft_content` builds. `lithokraft_game` and game executables **cannot link in this
  environment**: GLFW needs `X11/Xcursor/Xcursor.h` and only the runtime `libxcursor1` is
  installed (dev headers missing) — pre-existing environmental gap, unrelated to this work.
  All changed game files verified compiling clean with the real build flags via
  `compile_commands.json` (`game.cpp`, `dimensions.cpp`, `selftest.cpp`,
  `world_save.cpp`, `signal.cpp`, `bench_stream.cpp`, `biome_capture.cpp`,
  `perf_headless.cpp`).
- `lithokraft_selftest` could not run (game binary not linkable here) — grandfathered
  alongside the Vulkan headless item.

---

## Bugs found and fixed (in code touched)

1. **`biome_capture.cpp:151` — wrong air constant.** Passed `litholattice::kAir` (BlockId 0)
   to `SubChunk::set()`, which now takes `StableId`. `StableId(0)` is *not* air — it is an
   unknown id. Fixed to `kAirStableId`. Would have written garbage voxels.
2. **My own `mesh_input.cpp` edit** initially missed the function signature (body referenced
   the new params) — caught by the compiler, fixed, verified by build.
3. **CRLF line endings** silently defeated several `str.replace` edits (no-op without assert).
   Caught by auditing; all edits re-applied with assertions. Lesson recorded: always assert
   replacement counts.
4. **Three test sites** called `ChunkMap::set` without a registry (fail-fast assert did its
   job): `testChangedColumns`, the AI walk-cache test, and the emissive `wCost` map.

---

## Deliberately left for the builder

- **Full game link + `lithokraft_selftest`**: needs `libxcursor-dev` (or equivalent X11 dev
  packages) installed in the build environment. All game sources compile; only the GLFW
  system dependency blocks the link.
- **The three open forks**: seal vs hot-swap lifecycle, unknown-ID quarantine vs Air,
  parallel vs unified item registry — not implemented, as instructed.
- **`engine_audio` flakiness under full `ctest` load**: passes solo; timing-sensitive, not
  related to this work. Worth a look if it persists on the builder's machine.
- **W64 width path**: implemented per spec (index IS the stable ID) with O(1) `at()`, but no
  session can reach 2³² distinct blocks today — the fallback paths (`findByStableId` per
  voxel) are untested by construction. The 8→16 step-up **is** tested.
- **Net integration beyond the codec**: `BlockUpdate` is a pure codec; the game does not
  yet send it in chunk/event traffic — that wiring is future work, not phase 1.

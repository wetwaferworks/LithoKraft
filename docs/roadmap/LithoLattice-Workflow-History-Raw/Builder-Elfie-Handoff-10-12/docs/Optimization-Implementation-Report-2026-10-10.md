# Optimization Implementation Report — 2026-10-10

Six-part engine optimization program on the LithoLattice C++ voxel engine (`~/workspace/litholattice/engine`), all approved by JD. Implemented sequentially by workstream, one source tree, full build green throughout.

**Final verification (coordinator-ran):** full build clean, zero new warnings. `ctest`: **10/13 pass**. The 3 failures are byte-identical to the pristine baseline (each verified by reverting to baseline and rebuilding): `engine_render` (mesher golden hash), `engine_sim` (block-names registry identity), `lithokraft_selftest` (SIGABRT at 0.00s startup, game-side). None are caused by this program.

---

## 1. Per-tick bump allocator — DONE ✅

**Files:** `engine/src/core/include/litholattice/core/tick_arena.h`, `engine/src/core/tick_arena.cpp` (LF); `core/CMakeLists.txt`; `engine_tests.cpp` → `testTickArena()` (6 checks).

**API:** `TickArena(capacityBytes)`; `allocate(size, align)` O(1) bump; `alloc<T>(n)` typed (trivially destructible); `reset()` retains physical blocks (warm ticks never malloc); `mark()`/`rewind()` nested checkpoints; 64-bit `Handle` (blockIdx|offset, floooh-style); telemetry `used()/capacity()/totalReserved()/blockCount()`.

**Decisions locked:** overflow grows chained blocks (≥ previous size, deterministic growth function); single tick-thread owner, not thread-safe v1; determinism = pure function of call sequence (never hash raw pointers — handles only); blocks capped 4 GiB for 32-bit handle offsets.

**Tests:** `litholattice_tests core` **10/10 PASS** — alignment 1–16/max_align_t, reset reproduces identical layout (pointer equality), overflow growth + steady-state malloc-free over 1000 ticks × 200 allocs, checkpoints, handles resolve.

**Open questions:** adopt `Handle` as the standard transient-reference type? Per-system arena sizing budgets unmeasured (`totalReserved()` exists for telemetry). v2 threading model (per-worker arenas) open.

---

## 2. Flood-fill audio occlusion — DONE ✅

**Files:** modified `engine/src/audio/include/litholattice/audio/spatial_audio.h` (LF preserved); `engine/src/audio/audio_selftest.cpp` (CRLF preserved, edited via script). No CMake change needed.

**API:** `OcclusionField::bake(lx,ly,lz, isSolid, radius)` — 6-connected BFS, fixed neighbor order (+X,−X,+Y,−Y,+Z,−Z), integer math only; `occluded(x,y,z)` O(1); `query()` adapter for existing templates; `distance()` BFS depth; `enclosure()`; `checksum()` FNV-1a-64 for lockstep verification. New `perceive`/`audibleSounds` overloads; all existing APIs and behavior unchanged. Radius clamped [1,32]; dense `(2R+1)³×2B` ≤ ~537 KiB (dense chosen over hash: array-order iteration required for determinism).

**Tests:** `litholattice_tests audio` **24/24 PASS** (19 pre-existing + 5 new), stable across 3 runs — baked field matches per-pair DDA on walled 25³ box; determinism bit-identical across shuffled insertion; radius clamps; amortized path bit-exact vs direct path on random 18%-solid layouts (300 events); aging; reverb endpoints.

**Decisions locked:** flood-visibility vs straight-ray DDA documented as different questions; dense storage; co-located source never occluded; `floor()` voxel convention; stale-field semantics via `bakedFor()`.

**Open questions:** wire `bakeOcclusion` into `occlusion_thread.cpp`'s per-tick loop (natural next integration, left untouched per scoping); bake cadence policy (every tick vs on listener-chunk-change).

---

## 3. Binary greedy meshing — DONE ✅

**Files:** `engine/src/render/include/litholattice/render/binary_greedy_mesher.h`, `engine/src/render/binary_greedy_mesher.cpp` (LF); `render/CMakeLists.txt`; `engine_tests.cpp` → `testBinaryMesher()`. `greedy_mesher.cpp`/`mesher.cpp` untouched.

**API:** `BinaryQuad` exactly 8 bytes (`static_assert`): x,y,z min corner, w,h extents, dir 0–5, 8-bit material palette index, 1 reserved `aux` byte (future AO/flags). `binaryMeshChunk16` (raw 16³), `binaryMeshSubChunk` (consumes same `MeshInput`/`MeshContext` as `meshSubChunk`), `expandBinaryQuad`. Three stages per research: 256-bit occupancy planes → word-wide `emit & ~cull` → greedy merge with word-wide row compares, material-aware, fixed iteration order.

**Tests:** `litholattice_tests render` **13/13 PASS** (+1 pre-existing golden FAIL, byte-identical to baseline). Agreement vs `meshSubChunk` by exact unit-face multiset equality: single cube, adjacent cubes (shared face culled), flat plane, checkerboard, 45% random fill (4383 merged vs 6508), staircase+overhang, empty→0 quads, full solid→6 quads, double-run memcmp determinism, material-boundary merge stops, wrapper==raw API.

**Perf (measured):** ~87 µs/chunk on hostile 45% fill, 2 materials (2000 runs, -O2) — inside the research's 50–200 µs band.

**Decisions locked:** `meshSubChunk` is the reference (`greedy_mesher.cpp` is a non-emitting counting stub); opaque cubes only (rest falls back); caller-supplied 8-bit material index, merges require equal material; raw API treats outside as air.

**Open questions:** T-junction crack overdraw not implemented (matters when vertex-pulling renderer lands); whether `BinaryQuad` replaces `PackedVertex` quads engine-wide.

---

## 4. Region layer, Anvil-inverted — DONE ✅

**Files:** `engine/src/storage/REGION_DESIGN.md` (layout, commit protocol, crash recovery, Anvil-vs-ours table); `engine/src/storage/include/litholattice/storage/anvil_region.h`; `engine/src/storage/anvil_region.cpp` (LF); `storage/CMakeLists.txt` (CRLF preserved); `engine_tests.cpp` → `testAnvilRegion()` (19 checks).

**Design:** `.llar` files, 32×32 chunks: 16-byte `LLAR` preamble + Anvil-style 8 KiB header (4 KiB location table with Anvil's exact 3-byte BE offset + 1-byte count encoding, 4 KiB timestamp table), **512-byte sector quantum** (8× less padding waste than Anvil's 4 KiB). Crash safety = the Anvil inversion: per-record CRC32 + **payload-first / header-second** commit — `stage()` writes + fsyncs payload, `commit()` re-verifies staged bytes, then writes timestamp entry and the 4-byte index entry **last** as the single commit point. Free bitmap rebuilt from index on every open; torn trailing appends truncated on open. Opaque payloads + 1-byte codec tag (Anvil numbering 3/4/127); timestamp table = monotonic dirty-generation vector; oversize records spill to `<stem>.c<x>.<z>.llx` sidecars.

**Tests:** **19/19 PASS** — preamble, 200-payload round-trip across 4 codecs (byte-identical after reopen), single-bit-flip → CRC failure with neighbours intact, torn-write simulation (staged-but-uncommitted → old/absent, never garbage; past-EOF index → absent), sub-4KiB packing (100×~110 B records < 1 KiB/chunk vs Anvil's ≥4 KiB), all 1024 slots via direct index, bitmap rebuild reuses freed sectors, sidecar round-trip/erase, generation monotonicity, damaged file fails open without overwriting.

**Decisions locked:** 512 B quantum; 17-sector header; record `[len][codec][flags][payload][crc32]`; commit re-verifies staged payload; sidecar naming.

**Open questions:** multi-record atomicity (tiny journal vs per-record); explicit defrag pass; 512 B vs 1 KiB quantum after measuring real chunk sizes; batched group-commit sync API (current per-`put` fsync correct but chatty).

---

## 5. Networking snapshots — DONE ✅

**Files:** `engine/src/net/include/litholattice/net/{version,scaling,quantize,protocol,snapshot,priority}.h`; `engine/src/net/{quantize,protocol,snapshot,priority}.cpp` (LF); `net/CMakeLists.txt`; `engine_tests.cpp` → `testNet()`. (net/ previously held only CMakeLists + include stubs; built fresh on them. Note: the worker's detailed handoff message was malformed — implementation verified directly from tree + test run.)

**Protocol:** 14-byte header (magic `LL`, version, flags: truncated/needFull, u16 seq with RFC-1982 serial arithmetic, ack + 16-bit selective ackBits, payload type/len). Snapshot payload: serverTick (512 TPS grid), baselineSeq (0 = full), uvarint idGap records sorted by entity id, change mask (pos/vel/orient/flags/spawned/atRest 1-bit flag), Q10 fixed-point positions, smallest-three quaternion packing (29-bit), zigzag varint deltas vs acked baseline. Cadence 32 Hz default (every 16th tick); per-client history ring (depth 64); Quake-III-style no-retransmit (deltas rebase on newest ack); fixed-point Q16 client interpolation with 2-interval render delay, clamp-never-extrapolate; spiral-of-death handled by never emitting catch-up snapshots. `PrioManager`: guaranteed bandwidth floor per stream + priority-ordered leftover.

**Tests:** **16/16 PASS** — varint round-trip incl. INT32_MIN/MAX + malformed rejection, header round-trip + bad-magic rejection, seq arithmetic across u16 wrap, Q10 error bounded by 1/2048 block, quaternion q==−q and error < 0.01, full snapshot bit-exact round-trip, delta (26 B) vs full (1822 B) on 3-of-100 changed, spawn/despawn/move in one packet, 32 Hz cadence + ack never regresses, receiver drops dups/old/malformed + selective acks, lost baseline → NeedBaseline/needFull → full resync, interpolation exact at intermediate ticks, missed slots skipped with delta covering gap, byte-budget truncation with recovery flag, PrioManager floors + determinism.

**Open questions:** reliable-vs-unreliable channel split; wire protocol versioning; encryption — all deferred, not guessed.

---

## 6. Deterministic parallelism scheduler — DONE ✅

**Files:** `engine/src/sim/include/litholattice/sim/det_sched.h`, `engine/src/sim/det_sched.cpp` (LF, ASCII); `sim/CMakeLists.txt`; `engine_tests.cpp` → `testDetSched()`. **`trace_jit_basic.cpp` untouched** (read-only) — verified.

**API:** `SimSystem` subclasses declare `ResourceAccess` (readRes/writeRes, caller-allocated ids); greedy list scheduling in registration order → conflict-DAG stages (conflict = any write involved; read/read shares). `runTick`/`runTicks` — one JobPool job per system per stage, `waitIdle()` per stage. Per-system `CommandBuffer` (Spawn/Despawn/AddComponent/RemoveComponent/Custom) drained single-threaded **after every stage** in (systemId, seq) order. Keyed RNG: `detMix64`/`detKey(entity,tick,salt)`/`detRand32`/`detRand01` — pure functions, thread-count independent. FNV-1a hash helpers for golden-trace verification. Header documents the Trace JIT seam: a compiled `ITraceJit::execute` can wrap as a `SimSystem` writing the signal-network resource.

**Tests:** 5 groups **PASS** — stage layout `{A,C,F,G},{B,D},{E}` from mixed read/write decls; concurrent command pushes drain in exact (system,seq) order; stage-0 spawner visible to stage-1 observer on 1 and 4 threads; keyed RNG stable/distinct/identical across 1–4 threads; 64-tick runs on 1/2/4 workers bit-identical hashes.

**Decisions locked:** drain after every stage (subsumes end-of-tick); `ApplyFn` carries systemId; conservative conflicts; fail-fast throw on system-job exception (never silently continue a corrupted deterministic sim).

**Open questions:** SimThread integration (tick-loop contract — JD's call); Trace JIT composition (builder/JD — the seam is documented, wrapping not done); resource granularity for signal propagation (per-chunk vs per-network vs whole-world); production failure policy (abort vs log-and-halt).

---

## Pre-existing baseline failures (NOT from this program)

All three verified byte-identical on the pristine tree (revert + rebuild by workstreams 1, 3, 4, 6):

1. **`engine_render`** — mesher golden hash mismatch (`scene hash dcb1c88e3af94693, 25 quads`). Deliberate golden; needs updating when the mesh format changes.
2. **`engine_sim`** — `block names: same registry -> identity`. Root-caused by workstream 3: `BlockRegistry()` in `world/block.h:90` passes 10 initializers for the 12-field `BlockDef` — `"litholattice:air"` narrows into `falls` (bool; the `-Wnarrowing` warning) and `id` stays empty, so `idOf(kAir)` returns `"anonymous:Air"`. One-line fix in the world module, out of scope for this program.
3. **`lithokraft_selftest`** — SIGABRT (exit 134) at 0.00s startup, game-side, pre-existing.

## Warnings

Zero new warnings introduced across all six workstreams (`-Wall -Wextra` clean). The single remaining test-compile warning (`world/block.h:90` narrowing) is pre-existing and is the visible symptom of baseline failure #2.

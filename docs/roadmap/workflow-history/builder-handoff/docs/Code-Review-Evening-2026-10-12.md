# Code Review — Evening Pass, 2026-10-12

**Tree:** `~/workspace/litholattice` · **Method:** six parallel read-only reviewers (world/registry, procgen/Formula, assets/SVG, net, game side, build+tests). No code was modified by this review.
**Coverage:** everything that landed tonight — Infinite Registry Phase 1+2, Litho SVG (+ pack wiring + provenance), Litho Formula core + wiring, X11 game-link fix, `setRegistry` fix passes.

---

## CRITICAL

### C1. `LithoKraft --selftest` segfaults — 4 missed/misordered `setRegistry` sites in `signal_selftest.cpp`
- **Evidence:** `build-linux/LithoKraft --selftest` → exit 139 (SIGSEGV). First ~65 checks pass, then dies inside `runSignalSelfTest()`. gdb: `ChunkMap::set` ← `buildFramebufferScene` ← `runSignalSelfTest`. The build is NDEBUG, so Phase 1's fail-fast `assert(registry_ && ...)` compiles out — you get a null-deref segfault with no diagnostics instead of the assert message. Buffered stdout is lost on segfault, so the log tail misleadingly ends mid-line.
- **Root cause:** tonight's fix pass added `setRegistry` to 16 of 20 sites in the file but missed/misordered 4:
  - `game/LithoKraft/src/app/signal_selftest.cpp:623-625` — `run` lambda: `m->set(...)` with no `setRegistry` anywhere in the lambda (first crash site in execution order).
  - `signal_selftest.cpp:651-653` — colourless-Catchor block: same pattern, no `setRegistry`.
  - `signal_selftest.cpp:701-702` — sawtooth `full`: `make(full)` runs `buildSawtoothWall()` → `w.set(...)` (`circuit_scene.cpp:12-13`) *before* `full.setRegistry(...)` on the next line — misordered.
  - `signal_selftest.cpp:709-710` — sawtooth `one`: identical misordering.
- **Blast radius:** `runSignalSelfTest()` runs **first** in the `--selftest` chain (`selftest.cpp:47`), so the entire selftest is dead — every check after it (worldgen, mesher, Vulkan probe, icons) is unreachable. All other 16 sites verified correctly ordered.
- **Fix (builder):** add `m->setRegistry(&blockRegistry());` right after construction at :623 and :651; move the `setRegistry` calls *before* `make(...)` at :701 and :709.

---

## HIGH

### H1. `findBlock` alias recursion has no cycle guard → stack overflow on malicious/cyclic aliases
- `engine/src/world/include/litholattice/world/block.h:442-448`: `findBlock` recurses via `return findBlock(al->second)` with no visited set or depth cap. `addBlockAlias` (`block.h:448`) does no validation — `addAlias("a","b")` + `addAlias("b","a")`, or even self-alias `addAlias("x","x")`, makes any lookup recurse to stack exhaustion. Same path reachable via `stableIdOf(Block, id)`. Today's 5 game aliases are acyclic, but this is a mod-callable API and the Phase-2 design invites mods to register renames. A crash-on-lookup is far from "first-wins, never silent."
- **Fix:** iterative loop with visited set or depth cap; reject self-aliases in `addBlockAlias`.

### H2. Net report's "{1,2,3} offers / v2-fallback" claim does not match the default config
- `engine/src/net/include/litholattice/net/handshake.h:308` (`HandshakeConfig`): empty `supportedProtocols` → ctor (`handshake.cpp:118`) fills `{kProtocolVersion}` = `{3}`. `knownProtocolVersions()` (`handshake.h:198`) = {1,2,3} is the *accepted framing set* pre-handshake (correctly used in `channels.cpp:19`, `snapshot.cpp:206`), **not** the offer list. No game/engine code configures `HandshakeConfig` (zero hits outside net+tests), so effective behavior today is **v3-only offers** → a v2 peer gets `Reject(NoCommonProtocol)`, fail-closed. Secure direction, but anyone deploying per the report expecting interop gets clean rejects. Related staleness: DEPLOYMENT GUIDANCE (`handshake.h:56-67`) still says "games MUST narrow their offer list to {2}" — pre-v3; should say {2,3} (or {3}).

### H3. Litho Formula node vs. stamp polar-axis mismatch — shapes differ by a 90° rotation
- `engine/src/procgen/litho_formula_node.cpp:126-127`: `phiA = asin(qz/qlen)`, `thetaA = atan2(qy, qx)` — formula polar axis (formula z) maps to **world z**. The voxelizer (`litho_formula_voxelize.cpp:78-80`, header: "polar axis becomes grid y (game up)") bakes with polar axis = **world y (up)**, and the stamp path preserves it (`generator_loader.cpp:395`, `stages.cpp:1318`).
- **Empirical proof** (sampler compiled standalone, gallery starfish preset): surface radius along world +Y is 0.743 under the node's convention vs 1.000 under the stamp's; along +Z it's reversed. Blossom's dome points along +Z in the node, +Y in the stamp. The spec's claim (`litho_formula_node_spec.md:75`) that "the analytic field agrees with the voxelizer's baked shape up to voxelization error" is **false**. The node's self-tests cannot catch it — they only test spheres (rotationally symmetric) and replicate the node's own convention (`litho_formula_node_selftest.cpp:91`).
- **Fix:** node should use `phiA = asin(qy/qlen)`, `thetaA = atan2(qz, qx)`; spec §4 needs the same correction (it prescribes the wrong mapping).

---

## MEDIUM

### M1. `registerKind` extension door is documented as runtime-usable but isn't
`block.h:198-206` gives the 3-step "Adding a content kind" recipe; step 2 requires adding a `std::vector<YourDef>` funneling through private `insertEntry()` (`block.h:234-236`). Nobody outside engine source can add a kind *with payload storage* — `registerKind()` alone mints a kind number with nowhere to put defs. Either the door is engine-internal-only (say so) or `insertEntry` needs a public/protected contract. The "mods add kinds at runtime, no new plumbing" claim overstates what's shippable. Needs a docs-or-API decision while the story is fresh.

### M2. `ChunkMap` fail-fast is assert-only → release builds null-deref instead of aborting cleanly
`chunk_map.h:42` (`get`), `:77` (`set`): `assert(registry_ && ...)`. Docs (`chunk_map.h:10-15`) promise fail-fast; in release the assert compiles out and `registry_->findByStableId(sid)` is a null-pointer dereference — UB, not a loud abort. (This is exactly why C1 manifests as a segfault instead of an assert message.) If fail-fast is the doctrine, make it a real check. Note `get()` short-circuits air before the assert, so air reads stay safe either way.

### M3. `defByStableId` silently returns the air def for a valid *item* stable ID
`block.h:555-558`: kind-filtered, so safe — but silent. A caller passing an item sid gets air's def with no signal the kind was wrong. Suggest a debug assert that `kindOf(sid)` is Block or unknown.

### M4. LEB128 "strict" overclaims — non-minimal (overlong) encodings are accepted, untested
`engine/tests/engine_tests.cpp:5499` is labeled "rejects truncated, overlong, and overflowing encodings" but the "overlong" case (lines 5485-5489) is eleven `0x80` bytes (length > 10), not a non-minimal encoding. Hand-walk + ASan/UBSan proof: `getUVarint64` (`quantize.cpp:122-146`) has no minimal-length check — `{0x85,0x00}` → accepted as 5; `{0x80,0x00}` → accepted as 0. The listed properties (truncated / >10 bytes / 10th-byte bits past 63) are true and empirically confirmed; but non-unique encodings break any future byte-equality, hashing, or transcript-signing scheme. Fix: reject non-minimal encodings or correct the claims/test name.

### M5. Handshake downgrade: unauthenticated offer list + nonexistent V1Baseline codec
`negotiate()` (`handshake.cpp:88-99`) is correct (verified matrix). But the offer list is unauthenticated plaintext: MITM strips {1,2,3}→{1} → server agrees v1 → `codecForProtocol(1)` = `V1Baseline`, for which **no codec exists in the tree** (`handshake.h:269-274`). The fail-closed burden lands on game code that doesn't exist yet. Downgrade v3→v2 is feature-loss only (no BlockUpdate traffic exists), not a security regression — but the v3-specific consequence needs acknowledging.

### M6. Litho Formula node spec doc is stale/contradictory
`litho_formula_node_spec.md:9` says "Status: **proposed, not implemented**. Nothing in `graph_nodes.h` / `graph_nodes.cpp` is touched." The node IS wired (`graph_nodes.cpp:413`, `graph_nodes.h:38-39`). Readers will make decisions on false premises.

### M7. `"density": 0.9` footgun in litho worldgen JSON
`loadBiomeDensity` (`generator_loader.cpp:324`) fills *all 256* biome slots with the top-level `density`, so any biome *not* listed in the entry's `biomes` map gets gate 0.9 → ~0.81 stamps/cell. Benign today (wafer's 5 biomes + organic's 2 all enumerated; dead_zones at 0.0), but the file's own convention (`capacitor_clusters`, `resonator_spires_feat` use `density: 0.0`) shows the safe pattern. A 6th wafer biome added later would silently get heavy stamping.

### M8. Provenance verified only for `textures` — markers on animations/palettes silently ignored
`resource_loader.cpp:293-300`: the `verifyTextureProvenance` hook exists only in the textures loop. An author stamping `provenance` on an animation or palette def gets zero verification and zero warning. Extend the hook or document textures-only (the pack-wiring report overclaims "any texture entry").

### M9. `.svg` in `animations` silently becomes a single-frame animation
`resource_loader.cpp:81-93`: `loadImage` rasterizes any `.svg` rel to exactly tile×tile; `TextureRegistry::addAnimation` (`texture_registry.cpp:28-34`) accepts any strip whose height is a multiple of the tile — a tile×tile SVG passes as a 1-frame strip. Multi-frame SVG animation strips are impossible and undocumented.

### M10. Hardcoded numeric BlockIds in `biome_capture.cpp` — correct today, fragile
`biome_capture.cpp:148` (`== 42`), `:158-160` (`38/39/42/43`): counted rows — 38=trace_trunk, 39=trace_branch, 42=capacitor_can, 43=resonator_crystal — all correct today, but they bypass named constants (`kCapacitorCan` etc.). Only append-only discipline keeps them right; one mid-list insertion silently corrupts the capture tool. (The `kAirStableId` fix at :152 verified correct.)

### M11. Per-instance Formula variety is weak by design
No seed (per spec) + scatter gives only position and 4 quarter-turn Y rotations — no scale, mirror, or param jitter. Every starfish is the identical silhouette. At ~1.8% effective cell probability they're rarely side-by-side, so acceptable for alpha — but the spec's open question on seeded jitter is the real answer.

---

## LOW

- **L-world1.** W32/W64 per-section unreachable — a 16³ subchunk holds 4096 voxels < 65536 (W16 capacity), so `stepWidth()` past W16 cannot trigger. The W64 conversion loop was audited correct; the four per-voxel `findByStableId` fallbacks are kind-safe. Dead-but-correct; noting the maintenance surface.
- **L-world2.** `storage.cpp:155` no-registry save path assumes `rawIndex == BlockId`; at W64 `rawIndex` returns the 64-bit StableId truncated to u16. Unreachable in practice (L-world1), tools-only path.
- **L-world3.** Stale phase-1 air constant `0x82ca3801816336c9` lingers as arbitrary varint test data (`engine_tests.cpp:5508`) — footgun for the next person grepping stale constants; one-line comment would fix.
- **L-world4.** Items have no alias mechanism (`addBlockAlias` exists; no `addItemAlias`) — item renames currently have no migration story; one line in builder notes alongside inventory work.
- **L-world5.** `StableId 0` collides with two sentinels (`itemStableId(0)==0`, `placesBlock==0` mean "none") but `addItemWithStableId(d, 0)` would register a real entry with id 0. Trivial guard: reject `stable == 0` in `insertEntry`.
- **L-net1.** MCP `net://version` advertises stale protocol 1 (`engine/src/mcp/server.cpp:1854`: `LITHOLATTICE_PROTOCOL_V1` while `kProtocolVersion` is 3). Should be V3.
- **L-net2.** Doc inaccuracy: report §3 and `protocol.h:61` say coords are "zigzag varints"; `putVarint`/`getVarint` are DDNet-style sign-magnitude, not zigzag. Wire-compatible (both sides same codec), doc wrong.
- **L-net3.** Stale header comment `protocol.h:1` ("snapshot wire protocol (v2)" — it's v3); V1/V2/V3 constants split across `version.h`/`protocol.h`.
- **L-formula1.** Gallery loader accepts any int resolution 8..64 (`litho_formula_gallery.cpp:101`); voxelizer rejects anything but {8,16,32,64} (`litho_formula_voxelize.cpp:33`). Resolution 24 passes gallery validation, fails at bake. Gallery uses 32 today.
- **L-formula2.** Node hard-mask edge: `falloff=0` with `R == rn` exactly → 0/0 = NaN → evaluates 0 (spec says `d>=0 → 1`). Measure-zero.
- **L-formula3.** "Bitwise-identical outputs on every platform" (`litho_formula.h`) overstates — libm transcendentals aren't guaranteed bitwise cross-platform. Deterministic per platform/run; fine in practice.
- **L-formula4.** Voxelizer header one-liner ("parity fill… majority vote over three axes", `litho_formula_voxelize.h:8-9`) contradicts the actual single-axis continuous-span fill; the detailed doc below is accurate.
- **L-formula5.** Column-span fill over-approximates non-axis-star-convex shapes (toroidal/z-pinched); none of the 12 presets trigger it.
- **L-assets1.** Duplicate `"tile_size": 32` key in `pack_32/resources.json:6-7` (hand-edit artifact; `Json::get` returns first match; both are 32 — latent hazard if they diverge).
- **L-assets2.** Cache key doc says "absolute path"; code doesn't canonicalize (`resource_loader.cpp:87`, `resource_pack.h:30`) — duplicate entries possible, never stale.
- **L-assets3.** `SvgRasterCache::KeyHash` omits mtime (`resource_loader.cpp:106-111`) — correctness preserved via `operator==`, minor perf wart.
- **L-assets4.** Unbounded cache growth across hot-reload churn — `store` emplaces per (path,tile,size,mtime), nothing evicts except the test hook; game never clears the shared cache. Small constant per entry, monotonic per session.
- **L-assets5.** Stale-cache edge: same size + same mtime (cp -p, deterministic stampers) serves a stale raster; `makeKey` swallows filesystem errors via `ec`.
- **L-assets6.** Thread safety latent, not live: no mutex on `SvgRasterCache`, `crc32Table()` uses plain `static bool done` — safe today (no threads in assets/game; F9 reload on main thread), but `resource_loader.h` invites UI-held cache instances; future worker-thread loading would race.
- **L-assets7.** Float→int overflow UB on absurd SVG dimensions (`image.cpp:53-54`: `int(std::ceil(...))` on `width="1e30"`) — fails safe in practice, clamp before cast to remove UB.
- **L-assets8.** Same-document `<use>` silently dropped — README only disclaims cross-file use; `nanosvg.h:2836-2898` ignores `text`/`image`/`use`/`filter`/`animate` silently. One doc line.
- **L-assets9.** `engine/docs/RESOURCE_PACKS.md` documents neither `.svg` routing nor `provenance`; the feature's only spec is the report files.
- **L-game1.** Stale numeric ids in `wafer_biomes.h:82-84` comment (claims 49/50/51/46; actual rows 47/48/49/44) — comment-only, code uses named constants, but misleading next to id-sensitive worldgen code.
- **L-game2.** Inventory confirmed still on old ID currency (`game/include/lithokraft/game/inventory.h:31-33`; `kFirstItemId = 1<<16`) — matches the defined builder follow-up; untouched as expected.

---

## Verified test matrix

| Suite | Command | Result |
|---|---|---|
| Full suite | `ctest --output-on-failure` (build-linux) | **34/36 passed** |
| engine_ (all 23) | `ctest -R "engine_"` | **23/23 passed** (re-run) |
| litho_formula | `ctest -R "litho_formula"` | **3/3 passed** |
| procgen | `ctest -R "procgen"` | **1/1 passed** |
| assets | `ctest -R "assets"` | **1/1 passed** |
| net / world / storage / sim | `ctest -R "engine_net\|engine_world\|engine_storage\|engine_sim"` | **4/4 passed** |
| Game link | `cmake --build . --target lithokraft lithokraft_headless` | **clean** — `LithoKraft` executable links; X11 libs resolve |

Full-suite failures: (1) `engine_audio` — FAILED in full run, **passes solo** (confirmed load-flake; the known timing-sensitive streaming check); (2) `lithokraft_selftest` — **genuine SEGFAULT** (C1 above), reproduces in quiet env.

**Morning open items — status:**
- 4 missing HUD icons (ChipOR, ChipOR viewport, silica grass, tilled silicon) → **still missing**, verified by filesystem check across pack_16/32/64 (`chipor.png`, `silica_grass.png`, `silica_tilled.png` absent everywhere).
- 3 worldgen/mesher failures → **cannot re-verify**: selftest segfaults (C1) before reaching them. Last-known from the 02:31 UTC binary: 3 caves checks (3x3 worm cross-chunk replay, no floating debris, branched-worm forks).
- Vulkan headless → **cannot re-verify** via selftest (C1 precedes the probe); environment unchanged (no `/dev/dri`, no `vulkaninfo`) — GPU-less VM, failure expected and environmental.

---

## Crew-report claims corrected

These statements in tonight's phase reports do not match the tree as reviewed — corrected here:
1. **"Full game link blocked by X11 env gap"** (Phase-2 and Formula-wiring reports) — **stale**. X11 dev headers were installed ~05:00 EDT and the game links clean (verified above).
2. **`lithokraft_silica_selftest` abort is "pre-existing / unrelated"** (SVG and Formula-wiring reports) — **wrong**. It is Phase-1 fallout: the test never called `setRegistry()`, tripping the new fail-fast assert. (The `lithokraft_selftest` segfault is the same class of fallout in `signal_selftest.cpp` — C1.)
3. **"Handshake offers {1,2,3}, v2-fallback for older peers"** (Phase-1 report §3) — **overstated**. Default config offers {3} only (H2); `knownProtocolVersions()`={1,2,3} is the accepted-framing set, not the offer list.
4. **"Strict" LEB128** (Phase-1 report §3) — **overclaims** on canonical encodings: non-minimal encodings are accepted and untested (M4). Truncated/>10-byte/bit-63 properties verified true.
5. **"The analytic field agrees with the voxelizer's baked shape up to voxelization error"** (node spec §4) — **false**; they differ by a 90° polar-axis rotation (H3).
6. **"Mods add kinds at runtime, no new plumbing"** (Phase-2 report) — **overstates**: `registerKind()` mints a kind number but `insertEntry` is private, so out-of-engine kinds have no payload storage (M1).

---

## Verified sound (worth stating)

- **Kind-tag identity core is airtight**: `(EntryKind, string)` keying with kind in hash+equality; supplied stable IDs rejected globally across kinds; kind-filtered lookups; `kindOf()` unambiguous. Migration claim holds — LLBN stores string IDs and re-derives per load; no hardcoded phase-2 stable IDs in source; air values verified byte-exact (`0x82ca3801816336c9` → `0xf99811fbedcd14d0`).
- **Duplicate rejection is atomic** (both checks before any mutation); single-threaded-at-startup contract holds.
- **LUT kind asserts are once-per-palette-entry**, never per-voxel; release fallback (→kAir) matches the report.
- **W64 handling consistent** across `at()`/`rawIndex()`/`set()`/`fill()`/`clear()`; W64 conversion loop audited correct.
- **LEB128 memory safety**: 10-byte/bit-63 rejection, truncation-safe, no overread (ASan/UBSan-probed), no shift-overflow UB; BlockUpdate round-trips the full u64 range; codec-only status confirmed (zero call sites outside protocol+tests); save format decoupled from protocol version.
- **Handshake negotiation logic correct** (verified matrix); header version gating rejects loudly; no weakened tests found.
- **NanoSVG integration memory-safe**: all five `decodeSvg` exit paths traced — no leaks, double-frees, or UAF; parser iterative, attr stack capped, bezier recursion capped, fills bounded. CRC stamp independently recomputed (`0x8d48e8dc` over 675 shipped bytes — genuine IEEE CRC32). "Never gates loading" verified in code; malformed markers all → Mismatch + warning, no crash paths.
- **Formula sampler math** matches the standard definition; degenerate rails hold; deterministic (no RNG/unordered_map in the litho path); stamp air-only writes verified (cannot overwrite terrain/player builds); naming compliance verified (Gielis/Genicap/NMS only in header blocks); clean-room — no copied code.
- **Game content registration** uses the kind-tagged API correctly; zero callers of `registerBlock()` rely on old overwrite semantics; `world_save.cpp` remap Phase-2 compatible; `dimensions.cpp` installs registries; SimThread installs its registry.
- **Integration seams found and noted**: the SVG crew left a syntax error in `assets_selftest.cpp` that the Formula crew repaired; crews edited concurrently with test runs (one `-j2` build ran mid-ctest).

---

## Builder punchlist (priority order)

1. **Fix C1** — 4 `setRegistry` sites in `signal_selftest.cpp` (:623, :651 add after construction; :701, :709 move before `make(...)`). Unblocks the entire selftest. Consider making the fail-fast a real check (M2) so the next miss aborts loudly instead of segfaulting.
2. **Fix H3** — lithoform node polar mapping (`phiA = asin(qy/qlen)`, `thetaA = atan2(qz, qx)`) + spec §4 correction. Add a non-spherical node-vs-voxelizer agreement test so it can't regress.
3. **Fix H1** — alias recursion: iterative lookup with visited set; reject self-aliases in `addBlockAlias`.
4. **H2** — correct handshake docs (offer list, deployment guidance → {2,3}); fix MCP `net://version` stale protocol 1 (L-net1).
5. **M1** — decide: `registerKind` engine-internal-only (document) or public `insertEntry` contract.
6. **M7** — litho JSON `"density": 0.9` → `0.0` convention (all three litho entries).
7. **M8/M9** — provenance: extend hook to animations/palettes or document textures-only; document SVG animation-strip limitation.
8. **M4** — decide: reject non-minimal varints or correct claims/test name.
9. **M10** — replace hardcoded BlockIds in `biome_capture.cpp` with named constants.
10. **L-assets1** — remove duplicate `"tile_size"` in pack_32/resources.json.
11. **Converter** — teach `convert_pack_manifest.py` about `.svg` + provenance stamping (else regeneration drops `cert_glyph_1`).
12. **Inventory → StableId migration** (already defined follow-up) + one-line note on item-alias story (L-world4).
13. **Re-run full ctest** once the tree settles; re-verify the 3 morning worldgen/mesher items and the Vulkan probe once C1 is fixed.
14. **HUD icons** — 4 still missing (content gap, not code): ChipOR, ChipOR viewport, silica grass, tilled silicon.

---

## Could NOT verify

- W64 runtime behavior (unreachable by construction — reasoned about, not executed).
- Anything past the segfault point in `lithokraft_selftest` on the current tree (worldgen/mesher checks, Vulkan probe, icon checks).
- Debug-build assert behavior (build-linux is NDEBUG; assert text not confirmed at runtime).
- Full-suite flakes beyond audio (only audio re-run solo; other suites' full-run passes taken at face value).
- On-the-wire interop with a real v2 peer (no v2 binary available).
- Long-session raster-cache growth (reasoned, not measured).
- PNG "byte-identical" by diff (tree is not a git repo — no pre-change baseline; claim rests on inspection).

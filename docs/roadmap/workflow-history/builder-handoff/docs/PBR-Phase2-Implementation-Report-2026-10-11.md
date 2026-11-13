# PBR Phase 2 Implementation Report — 2026-10-11

Phase-2 goal (JD): finish the deferred PBR work to reach pre-alpha. Builds on phase 1 (report: `PBR-Implementation-Report-2026-10-10.md`). Workers run sequentially on the shared tree; each verifies green with self-tests.

## JD decisions (locked during phase 2)
- **BinaryQuad and PackedVertex COEXIST — do not replace one with the other.** They become a user-selectable mesher option (performance/optimization choice). JD's words: "we like options." This supersedes phase 1's "lean Option B (parallel attribute stream)" note: the GL backend must keep BOTH vertex paths alive (BinaryQuad path carries material IDs natively; PackedVertex path stays as-is) with mesher selection exposed via user config. Applies to the phase-2 vertex-format / GL-wiring work.

## Worker sections
(Workers append their sections below in order.)

---

## Worker 1: data foundation — DONE

Scope: phase-1 open questions #1 (startup wiring) and #2 (per-voxel material storage). Items A and B
completed in order, full engine build green, zero new warnings, full ctest run with no new failures.

### Item A — startup wiring — DONE
- `initMaterialRegistry(MaterialRegistry& reg, const std::string& path, std::string* err)`
  (`render/pbr_materials.h` + `pbr_materials.cpp`): the engine init path the game calls. The game
  owns asset-path resolution and passes the resolved path.
- Game wiring (`LithoKraft/src/app/main.cpp`, mirroring the `GameSettings::load(settingsPath)`
  pattern): after `findAssetDirs`, resolves `<engine assets>/materials/lithokraft.materials` and
  calls `initMaterialRegistry`. Missing file = `logError` + continue with the built-in default
  (the engine's degrade-loudly init-error convention, cf. audio's "no output device; continuing
  muted"). The registry outlives the game (chunk saves resolve string IDs through it on the I/O
  thread); plumbed via new `GameConfig::materials` field.
- New `testPbrStartup()` (9 checks, all pass): loads via the real startup path (17 materials =
  16 file + built-in default, default is compact ID 0), missing file and empty path are graceful
  (false + err, registry still usable with the default alone).

### Item B — per-voxel material storage — DONE
- `SubChunk` gains `std::array<uint8_t, kSubChunkVolume> materials` + `materialAt`/`setMaterial`
  accessors (`world/chunk.h`). **Documented choice: a parallel byte array, NOT packed into the
  block word** — `BlockId` is a dense 16-bit palette index with no spare bits, so packing would
  invalidate every existing save and the BLKS palette codec for zero benefit (4 KiB per present
  sub-chunk). `set()` leaves material bytes untouched (documented; the caller assigns them).
- `ChunkColumn` gains `materialPalette[256]` + `materialPaletteSize` (plain data, so `chunk.h`
  stays free of the render module). Byte i → global registry ID; size 0 = every byte maps to
  global ID 0 (the built-in default) — this is what old saves get.
- `MeshInput` gains `uint8_t mat[kMeshPadVolume]`, filled by `snapshotForMeshing` — the bytes now
  ride the existing snapshot into the mesher (`binaryMeshSubChunk` already takes the array).
- Serialization: new engine payload `MATS` v1 (`render/material_payload.h`,
  `makeMaterialPayload(reg)`), registered by the game's `WorldSave` when it has a registry.
  Per-column **string-ID** table (never compact IDs) + per-sub-chunk palette-coded material bytes;
  a separate tag (not a BLKS bump), so old saves without MATS load with the default palette and
  need no migration. Load rules: table > 256 entries = error (phase-1 lock, never silent
  truncation); unknown string IDs → the defined fallback `litholattice:default` (never garbage;
  `initMaterialRegistry` guarantees it is registered); MATS sub-chunk with no BLKS counterpart =
  corrupt. `docs/SAVE_FORMAT.md` documents the layout.
- New `testPbrChunkMaterials()` (22 checks, all pass): set/get per voxel; save→load round-trip;
  **registry remap across sessions** (session B registers the same strings in a different order —
  compact IDs differ, string IDs stable, bytes and resolution correct); unknown-string fallback;
  >256 overflow fails; old save without MATS loads with defaults; `MeshInput` carriage.

### Verification
- Full engine build: exit 0, zero new warnings (remaining warnings are the pre-existing set:
  `block.h:90` narrowing, `greedy_mesher.cpp` unused vars, `testPbrBrdf` unused var).
- `ctest`: **10/13 pass** — the 3 failures are exactly phase 1's documented pre-existing ones,
  unchanged: `engine_render` golden hash (platform/compiler drift; the golden test hashes classic
  `PackedVertex` output, which this work never touches), `engine_sim` block-names
  (`world/block.h:90` narrowing), `lithokraft_selftest` SIGABRT (blocks.cpp 40-vs-36 guard).
  No new failures. Six optimizations, PBR phase 1, audio/threading, menu untouched.

### Decisions locked
- Built-in fallback material `litholattice:default` (matte dielectric, compact ID 0 via
  `initMaterialRegistry` on a fresh registry): the defined answer for unknown strings and empty
  palettes. `defaultMaterialDef()` exposed for tests/games.
- Missing materials file = error logged, game continues with the built-in default (never a
  hard startup failure — tests/headless builds use this path).
- Material storage = parallel per-voxel byte array; never packed into the block word.
- Saves persist string IDs; compact IDs are session-local by design.
- `WorldSave` takes the registry as an optional ctor param (null = no MATS payload); the game
  passes `cfg.materials`.

### Open questions (for JD / later workers)
- Who assigns material bytes at worldgen/edit time (block → material mapping policy)? The
  storage and save path are ready; no producer writes non-default bytes yet.
- The game's mesh jobs don't yet pass `MeshInput::mat` into `binaryMeshSubChunk` — the data
  reaches the snapshot, but the consumer call site is still on the classic path (also relevant
  to the new BinaryQuad/PackedVertex coexistence decision above).
- `setPbrContext` (renderer-side PBR context) is still unwired in the game — the registry now
  exists at the app level for it to consume.

---

## Worker 2: world-side emissive enumeration — DONE

Scope: phase-1 open question #3 (chunk emissive scan vs retained light-engine source list)
and the `IEmissiveProvider` seam in `pbr_software_shaders.h`. Implemented the real
world-side provider against the seam's stable contract (no seam changes needed).

### What was built
- `engine/src/render/include/litholattice/render/pbr_emissive_provider.h` (new, LF,
  header-only in the `pbr_lights.h` pattern): `ChunkEmissiveProvider`, a concrete
  `IEmissiveProvider`. `refresh(map, focusCx, focusCz, radiusChunks)` scans present
  (non-null) sub-chunks in the chunk square around the focus column, resolves each
  non-air voxel through the column's `materialPalette` into the `MaterialRegistry`,
  and caches every voxel whose def has `emissiveIntensity > 0` as
  `(voxel coords, linear rgb, intensity)`. `forEachEmissive` walks the cache in
  deterministic scan order (cx, cz, sub-chunk, voxel index all ascending) and emits
  voxel centers in world space — the exact sink signature `LightCollector::collect`
  expects. Default radius `kEmissiveProviderRadiusChunks = 1` (3x3 columns).
- Two-path material resolution per voxel: (1) the authoritative per-voxel material
  byte (Worker 1's storage); (2) when the byte resolves to the built-in default
  (nothing assigned yet) and a `BlockMaterialFn` (`BlockId -> compact material ID`)
  is installed, the block ID is mapped and *that* def is checked. Explicit bytes
  always win over the fallback, so the fallback silently stops applying once
  producers (worldgen/edit) start writing bytes. This answers Worker 1's open
  question "no producer writes non-default material bytes yet": torches and lava are
  found today via the fallback, and the byte path takes over without code changes.
- Consistency story (no engine mutation hooks): `markDirty()` is called by the
  game's single block-edit funnel; `needsRefresh(cx, cz)` is true when dirty or the
  focus chunk moved; `clear()` drops the cache (dimension change). Worst-case
  staleness is one refresh interval — explicit and game-controlled, never silent
  drift. Not thread-safe (same contract as `MaterialRegistry`): refresh on the
  ChunkMap's owner thread, then hand to the renderer.
- Doc touch-ups (LF files, comments only): the phase-1 seam note in
  `pbr_software_shaders.h` and the OPEN QUESTION in `pbr_lights.h` now point at
  `ChunkEmissiveProvider` as the resolution. No behavior change to phase-1 code.
- `chunk.h` untouched (it is CRLF; nothing needed changing there).

### Verification
- Full engine build: exit 0, zero new warnings (remaining warnings are the
  pre-existing set: `block.h:90` narrowing, `greedy_mesher.cpp` unused vars,
  `testPbrBrdf` unused var).
- New `testPbrEmissiveEnumeration()` (22 checks, all pass): material-byte scan finds
  torch/lava with exact positions/colors/intensities; seam contract (voxel centers,
  scan order) via the sink; all-default + fallback finds torches across a chunk
  boundary; all-default without mapper = no candidates; empty world = no candidates;
  explicit non-emissive byte beats the fallback; K=8 nearest-first through the real
  `LightCollector` (10 torches -> 8 nearest, x=0..7 in order); place/break updates
  the enumeration via `markDirty()`+refresh (and the cache is honestly stale-blind
  until the game marks dirty — the test asserts that); focus-chunk move needs
  refresh; identical order across two refreshes and a second provider instance;
  null registry degrades gracefully.
- Cost (measured in-test): refreshing a 3x3-column region with 200 torch voxels
  took 46 us — present sub-chunks only, ~1 air-check per voxel, registry lookups
  only for non-air voxels. Steady state allocates nothing (member vector
  clear+refill; `forEachEmissive` is allocation-free). TickArena deliberately not
  used: the cache must outlive the tick, so a retained vector is the right tool.
- `ctest`: the 3 grandfathered failures are unchanged (engine_render golden hash,
  engine_sim block-names, lithokraft_selftest SIGABRT). One additional failure,
  `engine_net` ("reliable delivers every byte exactly once, under 25% loss"), is
  NOT mine: it exercises `ReliableChannel` in `src/net/channels.cpp`, landed by the
  NET worker at ~06:03 EDT during my session (JD's 6:00am reliable+unreliable
  decision), uses a fixed RNG seed (deterministic failure in their new code), and
  shares no code or state with the render work. Left untouched for the NET worker.

### Decision locked: cached scan + explicit refresh (scan vs retained)
Picked **cached scan with explicit refresh (time/event-based invalidation)** over a
retained source list with mutation hooks and over per-call scanning:
- Retained list: O(emissives) per query is attractive, but invalidation would need
  bulletproof hooks on every mutation path (`ChunkMap::set`, direct `SubChunk::set`
  writes from worldgen/sim, save decode, column insert/unload). One missed path =
  silently wrong lights until reload. Too many write sites to make bulletproof.
- Per-call scan: O(volume) per `forEachEmissive` call; the software shader calls it
  per fragment — infeasible.
- Cached scan: refresh is O(present sub-chunks in region) at a game-chosen cadence
  (focus change, edit-dirty, or per frame — 46 us makes even per-frame cheap);
  per-query cost is O(cached candidates); hot path allocates nothing; staleness is
  bounded and explicit. Fits the chunk/tick architecture: chunk data is
  single-owner, mutation happens at known points, rendering reads between ticks.
- Investigation note: `LightEngine::addSource` has no production callers and nothing
  yet drives the 1-64 flood fill from block data, so there was no existing
  block->emission table to stay consistent with. The provider's block-ID fallback is
  the forward-compatible stand-in until producers write material bytes.

### Small decisions locked
- Voxel-center positions (x+0.5) in world space; linear-space color; intensity kept
  separate (matches the seam's sink signature exactly).
- Air voxels are never candidates, even if a mod registered an emissive "air".
- Null registry / unresolvable def degrades to the fallback path, never crashes.
- Palette byte out of range (>= materialPaletteSize) resolves to the built-in
  default (same rule as empty palettes), then the fallback may apply.

### Open questions (for JD / later workers)
- Who builds the `BlockMaterialFn` from the game's `BlockRegistry` (e.g. tex key ->
  `"<ns>:" + tex` -> `idFor`), and where it lives long-term once worldgen writes
  real material bytes (fallback then becomes vestigial — keep or remove?).
- The per-frame wiring: who calls `refresh`/`needsRefresh`/`markDirty` in the game
  loop and plumbs the provider into `PbrFragmentContext::emissive` (still on
  Worker 1's `setPbrContext`-unwired list).
- Refresh radius tuning for the GPU path (default 3x3 chunk columns is plenty for
  K=8 with 1/(1+d^2) attenuation, but wide-open scenes may want more).
- Whether the provider should also feed the Vulkan/GL backend's per-frame light
  uniforms, or stay software-renderer-only for now.

---

## Worker 3: GL backend wiring — DONE

Scope: phase-1 open questions #4 (vertex format), #5 (GL activation: replace vs coexist),
#7 (graphics_tiers.h vs PbrTransparencyTier), plus Worker 1's open gap (game mesh jobs
not passing `MeshInput::mat` into `binaryMeshSubChunk`) and Worker 2's open per-frame
plumbing (provider refresh / light uniforms for the GPU path). Full engine build green,
zero new warnings, full ctest with no new failures attributable to this work.

### What was built

**1. Material table upload (SSBO layout, GL33 transport).**
- `render/pbr_gl_upload.h` (new, header-only): `PbrMaterialRow` + `packPbrMaterialRow` /
  `packPbrMaterialTable`. Packs the registry as dense 48-byte rows indexed by compact ID.
- **Layout correction:** the phase-1 comment claimed 64-byte std430 rows. Verified against
  glslang's SPIR-V `Offset` decorations: the true std430 layout is **48 bytes**
  (albedo@0, roughness@12, emissive@16, intensity@28, metalness@32, ior@36, alpha@40,
  stride 48 — vec3 is 12 bytes wide in std430, only its *alignment* is 16). The comment
  in `pbr_shading.glsl` and the CPU packing now match the verified layout.
- **Transport decision (locked):** the GL 3.3 backend has no SSBOs (4.3+; the vendored
  loader is 3.3-only), so the rows travel in a **buffer texture** (`GL_TEXTURE_BUFFER` /
  `GL_R32F`, core since 3.1). The frag declares `uniform samplerBuffer uPbrMaterials`
  and unpacks via `texelFetch(uPbrMaterials, id * 12 + k)` — the same 48-byte rows a
  Vulkan backend would bind as a real std430 SSBO, so the layout contract holds for both.
- `MaterialRegistry::generation()` (new): bumped on every successful new registration
  only (idempotent/conflicting re-registrations don't bump — they change nothing).
  The backend's `setPbrMaterials` re-uploads only when the generation moved: the cheap
  dirty flag, no caller bookkeeping. Sufficient because defs are never mutated in place.

**2. chunk_pbr activation (coexistence, per JD's locked decision).**
- `gl33_renderer.cpp`: `"chunk_pbr"` added to `kProgramNames` (built from pack override
  or built-in, like the rest). **Program selection follows the meshes, not a mode flag:**
  the classic `chunk` program draws `PackedVertex` uploads, `chunk_pbr` draws
  `PbrChunkMesh` uploads; both maps can coexist in one frame.
- `IRenderer` gains four **default no-op** virtuals (`supportsPbrPath`, `setPbrMaterials`,
  `setPbrTier`, `uploadPbrChunkMesh`) — the Vulkan backend, NullRenderer and the
  perf/headless stubs compile unchanged. `FrameParams` gains `pbrLights[8]` +
  `pbrLightCount` (filled by the game, ignored by backends without a PBR path).
- **Activation order (locked):** `chunk_pbr` build failure never fails init — the path
  reports unsupported and the game falls back to PackedVertex with a loud log. Default
  mesher is PackedVertex (zero behavior change for existing users; golden hash untouched).
  A `chunk_pbr` draw is skipped when no material table was ever uploaded (loud-by-design,
  never silent black).
- PBR meshes stay **out of the chunk arena** (separate per-mesh VAO/VBO): the arena's
  stride/attribute assumptions are PackedVertex-specific.

**3. Per-frame light uniforms.**
- The game owns a `ChunkEmissiveProvider` (+ `LightCollector`, K=8) when a material
  registry exists. Per frame: `setPbrMaterials` (generation-gated, nearly free),
  provider `refresh` when `needsRefresh(focusChunk)` (focus move or edit-dirty),
  `collect` K-nearest around the camera into `FrameParams::pbrLights`. The backend
  uploads them to `uPbrLightPos/uPbrLightColor/uPbrLightCount`.
- `noteBlockEdit()` wired at the game's three block-edit sites (Break/Use/Place) —
  Worker 2's `markDirty` funnel, closed. Sim applies edits asynchronously, so worst-case
  staleness is one refresh interval (documented, bounded).

**4. Consumer call site (Worker 1's gap — closed).**
- `MeshInput` now carries the column palette (`matPalette[256]` + size), copied by
  `snapshotForMeshing` — the snapshot stays the async-safe carrier (workers never touch
  the live map).
- New `extractMaterial16()`: de-pads the 18³ `MeshInput::mat` to the dense 16³ layout
  `binaryMeshSubChunk`'s contract expects (found by failing test: the param is 16³).
- The game mesh job, on the BinaryQuad option: `binaryMeshSubChunk(*in, ctx, mat16, quads)`
  → `expandBinaryQuadsToPbr` (resolves byte → compact ID via the snapshot palette,
  stamps coarse light levels) → `uploadPbrChunkMesh`. **The classic mesher still runs
  for every sub-chunk** — the binary mesher's scope is opaque cubes only, so non-cube
  shapes keep rendering through the classic program (coexistence, not cutover; the
  double-mesh cost is documented below).
- New `PbrQuadVertex` (20 bytes) + `PbrChunkMesh`, `render/pbr_quad_mesh.h/.cpp`.
  Corner winding verified per direction (CCW-from-outside, same convention as the
  classic shared index buffer).

**5. Tier relationship (phase-1 open question #7 — resolved).**
- `GraphicsTier` (Low/Medium/High/Ultra → `TransparencyMethod`) is the **classic path's**
  quality preset (`graphics_tiers.h`, untouched). `PbrTransparencyTier` is the **PBR
  path's own user-selectable ladder** — new `GameSettings::pbrTier` (0..3, default
  ScreenDoor), applied live via `setPbrTier`. They are independent by design: JD locked
  all four PBR tiers user-selectable in phase 1, and tying them to GraphicsTier would
  remove that choice. Worker 4's UI may *suggest* pairings; the config keeps them separate.
- The GL path honors the tier: `uniform int uPbrTier` in `chunk_pbr.frag`; blend state
  follows it (Simple/PbrReflections composite by alpha; ScreenDoor discards in-shader,
  no blending/sorting; TrueRefraction is the stub). The ScreenDoor discard now matches
  the CPU `screenDoorTest` **exactly** (`floor(alpha*255) <= bayer*17`, same matrix).
- **Worker 4 seam:** tier value 3 keeps the Snell-ray computation (TIR handling) with
  black output; the contract is the uniform name `uSceneColor` (sampler2D of the
  rendered opaque frame) — the scene-color pass binds it and samples along the ray.
  Tier values are pinned (0..3 = enum order) by test.

**6. Config knob (mesher option).**
- `GameSettings::mesher` (0 = PackedVertex, 1 = BinaryQuad) + `kSettingsMesherNames`,
  JSON round-trip with clamping, settings-UI dropdown "Mesher (restart)" in the Video
  page. **Boot-only (world (re)load)**, like tickRadius/vsync: switching mesh formats
  mid-session would need a full remesh + GPU layout swap. Documented; live-switch is
  future work.
- At construction the game falls back to PackedVertex with a loud log if the backend
  reports no PBR path.

### Real-GL verification (Xvfb + Mesa llvmpipe — done, not just headless)
A scratch harness (`/tmp/pbr_gl_test.c`, not committed) created a real GL 4.5 context and:
- `chunk_pbr.vert` + `chunk_pbr.frag` (include expanded as the backend does) **compile
  and link**; all 8 uniforms resolve (`uVP/uOffset/uCam/uPbrMaterials/uPbrLightPos[0]/
  uPbrLightColor[0]/uPbrLightCount/uPbrTier`).
- **Caught a real phase-1 bug:** `pbr_shading.glsl` used undeclared `skyLevel`
  (param is `skyLight`) — never compiled on real GL before. Fixed.
- End-to-end draw: 1-material TBO (red, roughness 1) + one `PbrQuadVertex` quad +
  VAO layout exactly as the backend binds → center pixel R=255 G=0 B=0 (expected
  full-diffuse red). ScreenDoor tier with alpha=0 → all pixels discarded (matches the
  CPU "0 discards" edge).
- Note: llvmpipe is a software rasterizer — shader *correctness* is verified; real-GPU
  performance characteristics (TBO fetch cost, the 8-light uniform loop) are not.

### Verification
- Full engine build: exit 0. Zero new warnings (only the pre-existing set:
  `block.h:90` narrowing, `greedy_mesher.cpp` unused vars, `testPbrBrdf` unused var).
  (The full `lithokraft` link target needs GLFW/X11 dev headers, absent in this VM —
  pre-existing environment limitation; the game TUs `game.cpp`/`game_ui.cpp`/
  `settings.cpp` were verified with `-fsyntax-only` using the build's exact flags.)
- New `testPbrGlWiring()` (36 checks, all pass): 48-byte packing math + byte-offset
  cross-check vs the glslang layout, generation-counter semantics, expansion geometry /
  winding / material / light for all 6 dirs, out-of-range→0, trim at kMaxQuadsPerMesh,
  determinism, the Worker-1 gap end-to-end (snapshot palette → `binaryMeshSubChunk`
  → compact IDs), headless shader-source contract validation (attributes, uniforms,
  TBO stride, tier values, `#include` presence).
- Standalone `GameSettings` JSON round-trip check (in `/tmp`, game code can't link
  into engine tests): save/load, defaults, out-of-range clamping — all pass.
- `ctest`: 12/16 pass. The 4 failures are NOT this work: `engine_render` golden hash
  (grandfathered platform drift — classic mesher untouched), `engine_sim` block-names
  (`block.h:90`, grandfathered), `lithokraft_selftest` SIGABRT (blocks.cpp guard,
  grandfathered), `lithokraft_headless_harness` (the harness worker's own test,
  segfaulting in *their* boot assertions on the pre-existing blocks desync — their log
  line says "builder owns the fix"; my Game-ctor additions are null-guarded and inert
  in the harness config). `engine_net` passes (the NET worker fixed their runtime
  failure during this session).

### Decisions locked
- BinaryQuad/PackedVertex coexistence as implemented above (phase-1 Option B note in
  `renderer.h` marked superseded; PackedVertex never extended).
- Material-table transport = buffer texture on GL33 (SSBO needs 4.3); the 48-byte row
  layout is the shared contract (Vulkan can bind the same rows as a real SSBO later).
- Registry `generation()` is the dirty flag (defs never mutate in place).
- Mesher option boot-only; PBR tier live; tier defaults: ScreenDoor (GL path).
- Coarse per-quad light levels `(block=0, sky=64)` until the flood fill is wired —
  matches the classic path's unlit brightness so the PBR path doesn't render black
  (same class of gap as the software path's unwired `setPbrContext`).
- BinaryQuad option still runs the classic mesher per sub-chunk for non-cube shapes
  (correctness over the double-mesh cost; the builder can teach the classic mesher to
  skip cubes later without touching the golden hash — not this worker).

### Open questions (for JD / later workers)
- Flood-fill wiring: who populates `LightEngine` from block data, and when the
  per-quad `aLight` stops being the coarse (0, 64) constant (game-side work).
- `BlockMaterialFn` for the provider's block-ID fallback: who builds it from the
  game's `BlockRegistry`, and keep-vs-remove once worldgen writes real material bytes
  (Worker 2's question, still open).
- Live mesher switching (full remesh + GPU layout swap) vs today's restart-only.
- Whether the double-mesh cost on the BinaryQuad option wants the classic mesher to
  skip opaque cubes (needs care: the golden hash pins its output).
- Vulkan: `vk/chunk_pbr.vert/.frag` still to write (SPIR-V compiled in at build time;
  the vk backend shares nothing trivially — checked, not gold-plated).
- The `lithokraft_headless_harness` segfault and the blocks 38-vs-36 desync belong to
  the harness/content workstreams (their logs say so); the tree is being edited
  concurrently — a reconciliation pass is needed before any of these are re-verified.

## Worker 4: true refraction + tier UI — DONE

Scope: phase-1 open question #6 (phase-2 refraction: copy resolution, sampling, pass
structure) and #7's remainder (the deliberately-unwired tier dropdown), plus Worker 3's
seam (tier 3 keeps the Snell ray; the contract is the uniform name `uSceneColor`).
Full engine build green, zero new warnings, full ctest with no new failures
attributable to this work.

### Item 4 — TrueRefraction (Ultra tier): the stub is gone on GL33

**1. Scene-color copy (decisions locked, with reasoning).**
- **What is copied:** the pre-transparency composite — classic chunk meshes, the PBR
  opaque pass, and entities — but NOT overlay lines, the crosshair, or the HUD (they
  draw after the copy). Entities are deliberately inside the copy so refractive
  surfaces show items behind them.
- **Resolution: full-res RGBA8**, via `glCopyTexSubImage2D` from the default
  framebuffer's back buffer. Rationale: the refraction math needs screen-space UV
  accuracy (grazing rays offset by whole texels at half-res, visibly wrong on thin
  geometry); Ultra is the opt-in quality tier, so the fill cost is the accepted price.
  Half-res stays a valid future optimization if Ultra ever needs a perf mode.
- **Sampling: bilinear + mip chain.** One `textureLod(uSceneColor, uv, lod)` with
  `lod = roughness * 4` (1/16-res at roughness 1) — the honest cheap PBR answer for
  rough refraction, far cheaper than a multi-tap gather. The backend calls
  `glGenerateMipmap` once per frame at tier 3 only (the documented Ultra price).
- **The copy is sRGB-encoded** framebuffer output; the shader linearizes the sample
  (`pbrSrgbToLinear`, new in `pbr_shading.glsl`) before the Fresnel split.
- **Pass structure:** two passes over the PBR meshes at tier 3, selected by a new
  `uniform int uRefractionPass`. Pass 0 keeps opaque fragments only (`alpha < 1`
  discards) and builds the copy; pass 1 keeps refractive fragments only
  (`alpha >= 1` discards) and resolves them. No blending in either pass (the resolve
  outputs an opaque composite). Opaque fragments at tier 3 shade exactly like the
  other tiers.

**2. The shader fill.**
- `chunk_pbr.frag`: the tier-3 black stub is replaced by `pbrRefractSample(...)`.
  The phase-1 contract is kept verbatim — uniform name `uSceneColor`, tier value 3.
- `pbr_shading.glsl` gains: `pbrSpecularBrdf` (split out of `pbrShade`; output
  term-for-term identical — the phase-1 energy sweep still passes against the
  refactor), `pbrSrgbToLinear`, `pbrSnellRefract` (explicit TIR handling — GLSL
  `refract()` returns `vec3(0)` on TIR, which would sample a garbage UV; on TIR the
  ray mirror-reflects, same rule as the CPU `snellRefract`), `pbrRefractionUvOffset`
  (screen-space UV offset: project surface point and a 1-world-unit probe along the
  ray, clamp the pixel offset to 96 px, zero offset when either point is
  at/behind the camera), and `pbrRefractSample` (the full resolve).
- **Energy:** `final = specular(F-weighted) + (1 - F) * transmitted + emissive` —
  the reflected/transmitted weights sum to 1 by construction (phase-1 discipline).
  The small diffuse term is folded out for transmissive dielectrics (documented:
  it belongs to in-medium scattering; keeping it would double-count the `(1 - F)`
  weight against the transmitted sample).
- **Documented approximations:** single-interface model (entry face only, air ->
  material — the phase-1 assumption); no thickness / no Beer-Lambert absorption;
  single refractive layer (nested transmissive surfaces resolve to the nearest
  layer via depth testing); probe-distance (not depth-aware) UV offset.

**3. Software renderer: documented gap, not silent divergence.**
The software rasterizer never implemented the `PbrTransparencyTier` ladder at all
(pre-existing: its Pbr tier shades every fragment as opaque PBR), so it cannot run
the Ultra two-pass either. What it DOES get: the full CPU reference math in
`pbr_transparency_tiers.h` (`snellRefractEta`, `refractionScreenOffset`,
`srgbToLinear`, `resolveTrueRefraction`, `refractionPassKeepsFragment`) mirroring
the GLSL term for term, engine-tested with known answers. The gap and the follow-up
(tier selection on `SoftwareRenderer` + a two-pass rasterizer) are documented in
`pbr_software_shaders.h`. `pbrTierIsStubbed` now reports false for every tier (kept
for API stability; the phase-1 stub marker is retired).

**4. Backend (`gl33_renderer.cpp`).**
- `copySceneColor()`: (re)allocates the full-res RGBA8 texture on resize, copies,
  mipmaps, binds it on texture unit 3 (`kSceneColorTexUnit`; 0/1 classic, 2 = PBR
  material TBO), sets `uSceneColor`/`uSceneSize`/`uSceneVP`. New uniform locations
  queried at program build; `-1` (pack override without the uniforms) is a safe
  no-op per GL spec. Destructor deletes the texture.
- `renderFrame`: the PBR draw block is restructured into `drawPbrMeshes` /
  `drawEntities` lambdas; at tier 3 the order is pass 0 -> entities -> copy ->
  pass 1 -> (lines, crosshair, HUD as before). Non-tier-3 paths are byte-identical
  in behavior to Worker 3's.

### Item 5 — tier UI + renderer wiring: DONE

- `settings.h`: `kSettingsPbrTierNames` (enum order, `static_assert` pinned to
  `PbrTransparencyTier` count) + `kSettingsPbrTierCount`.
- `game_ui.cpp` (Video page, after the mesher dropdown): "PBR transparency (live)"
  dropdown (`set-pbrtier`) writing `GameSettings::pbrTier`, plus a hint label
  ("Pairs well with GraphicsTier High/Ultra; independent setting.") — the configs
  stay independent per JD's lock; the hint only suggests a pairing. The tier is
  live (no restart), unlike the mesher.
- No changes needed in `game.cpp` (live `setPbrTier` on change + at construction)
  or `settings.cpp` (JSON round-trip) — Worker 3 landed those; the UI was the only
  missing piece.

### Verification

- Full engine build: exit 0, zero new warnings (only the pre-existing set:
  `block.h:90` narrowing, `testPbrBrdf` unused var in `engine_tests.cpp:947`).
- New `testPbrTrueRefraction()` (19 checks, all pass): Snell known answer
  (air->glass n=1.5 at 45° -> 28.13°, unit length), normal incidence straight
  through, TIR past the critical angle mirror-reflects (eta-explicit exit case),
  air-entry never TIRs, `refractionScreenOffset` known answers (identity-VP
  projection, 96 px clamp, behind-camera zero), `srgbToLinear` known answers +
  round-trip invert, `resolveTrueRefraction` known answer, Fresnel-split energy
  bound sweep (worst case <= 1.0 + 1e-5, no energy creation), two-pass plumbing
  predicate, `IRenderer::setPbrTier` receipt on a recording mock + default no-op
  safety.
- `testPbrTiers` updated: no tier reports stubbed anymore. `testPbrGlWiring`
  headless contract extended: the 4 new uniforms present, the black stub gone,
  `pbrRefractSample` wired, refraction helpers in `pbr_shading.glsl`.
- Standalone `/tmp` GameSettings check (game code can't link into engine tests;
  `lithokraft_selftest` SIGABRTs on the grandfathered blocks guard): pbrTier
  round-trips save/load (3), clamps 99 -> 3 and -5 -> 0, names cover the enum.
- **Real-GL verification (Xvfb + Mesa llvmpipe, scratch harness `/tmp/pbr_w4_gl_test.c`,
  not committed):** `chunk_pbr.vert/.frag` (include-expanded, with Worker 4 edits)
  compile and link; all 12 uniforms resolve (8 old + `uSceneColor`/`uSceneSize`/
  `uSceneVP`/`uRefractionPass`); `glCopyTexSubImage2D` captures the framebuffer
  and `glGenerateMipmap` builds the blur chain; draw-level: tier-3 pass 0 discards
  the refractive fragment, pass 1 resolves the scene sample through the Fresnel
  split — refractive pixel measured **R=0 G=250 B=0** vs the predicted 250 (the old
  stub would be pure black), pass 1 discards opaque fragments. llvmpipe is a
  software rasterizer: shader *correctness* verified; real-GPU fill-rate cost of
  the full-res copy + mip build is not.
- `ctest`: **13/16 pass.** The 3 failures are exactly the grandfathered set, none
  attributable to this work: `engine_render` golden hash (platform/compiler drift;
  the classic mesher is untouched), `engine_sim` block-names (`block.h:90`
  narrowing), `lithokraft_selftest` SIGABRT (blocks.cpp 40-vs-36 guard).
  `lithokraft_headless_harness` passes (its workstream fixed their boot assertions
  during this session). `engine_net` passes.

### Decisions locked (Worker 4's to lock)

- Scene-color copy: full-res RGBA8, pre-transparency composite (classic + PBR
  opaque + entities; no lines/crosshair/HUD), `glCopyTexSubImage2D` + one
  `glGenerateMipmap` per frame at tier 3 only.
- Sampling: single `textureLod` with `lod = roughness * 4`; sRGB sample linearized
  before the Fresnel split.
- Pass structure: `uRefractionPass` 0 = opaque-only scene build, 1 = refractive-only
  resolve; no blending at tier 3; single refractive layer (nearest wins via depth).
- Energy: `spec + (1-F)*transmitted + emissive`; diffuse folded out for
  transmissive dielectrics; TIR -> mirror ray (never a zero vector into the sampler).
- Tier 3 keeps the entry-face (air -> material) assumption from phase 1.
- Software rasterizer: reference math implemented + tested; rasterizer two-pass is
  the documented follow-up (no silent divergence).
- Tier UI: dropdown names == `pbrTransparencyTierName` order, `static_assert`
  pinned; hint label suggests the GraphicsTier pairing; configs independent.

### Real-GPU verification notes (for the builder / JD)

- What needs a real GPU: the per-frame cost of the full-res copy + mip build at
  tier 3 (fill-rate bound; llvmpipe says nothing about this), TBO fetch cost with
  real material counts, and visual inspection of refractive glass in a real scene
  (the binary mesher emits opaque cubes only today, so refractive materials need
  the mesher to emit non-opaque quads first — builder work).
- Verified on llvmpipe: shader correctness, uniform resolution, copy mechanism,
  pass predicates, Fresnel-split numeric output (G=250 measured vs 250 predicted).

### Open questions (for JD / later workers)

- Flood-fill wiring for per-quad `aLight` (Worker 3's question, still open) also
  affects how refractive glass is lit on the diffuse side (folded out at Ultra,
  but the specular path uses the same coarse levels).
- Half-res scene-color copy as a perf mode for Ultra if the full-res copy proves
  fill-bound on real GPUs.
- Beer-Lambert absorption / thickness for the transmitted term (currently none).
- Who emits non-opaque quads from the BinaryQuad mesher (nothing refractive exists
  in the PBR draw today — tier 3 is fully wired but has no refractive geometry to
  act on until the mesher emits it).
- Vulkan `vk/chunk_pbr` still to write (Worker 3's question, still open).
- The `engine_render` golden hash drift is still unaddressed (pre-existing; the
  reconciliation pass should decide re-record vs investigate).

---

## Worker 5: golden hash — RE-RECORDED (verified correct by inspection)

Scope: phase-1 open question #8 / the phase-2 report's closing open question (the
`engine_render` golden-hash drift). JD's instruction: re-record ONLY if the new output is
verified correct by inspection, and document the re-record. Decision: **re-recorded**.

### What the test actually is

`tests/engine_tests.cpp::testRender` — the "golden" is NOT a rendered framebuffer. It is
the classic CPU mesher path (`snapshotForMeshing` + `meshSubChunk`, the PackedVertex
path Worker 3 deliberately kept byte-identical): a fixed 5-block fixture scene
(`(1,1,1)`+`(2,1,1)` rock cubes, `(1,2,1)` half-slab, `(4,1,4)` plant cross-sprite,
`(15,15,15)` lone rock; air fill) is meshed, and the raw `PackedVertex` byte stream is
FNV-1a-64 hashed against the recorded value. No GPU, no software rasterizer involved.

### The drift cause — determined, not guessed

Phase 1 characterized this as "platform/compiler drift". That characterization was
wrong (though its "predates PBR" conclusion was right). The struct is `#pragma pack(1)`
and every field is an integer — there is no float or padding for a compiler to drift
on, and the output is bit-deterministic across runs (same hash twice in a row).

Diffing the tree against JD's 2026-10-10 11:09 source zip
(`~/workspace/litholattice/LithoLattice-engine-src-100ecf3.zip`) shows the real cause:
the **trace Step-1 work (2026-10-10 night session) deliberately changed the mesh
format** — `PackedVertex` gained a `uint32_t slot` field (trace strength-buffer slot)
after `x,y,z`, growing 12 -> 16 bytes (`static_assert` updated in `renderer.h`;
`mesher.cpp`/`mesher.h` emit it). The golden was recorded against the 12-byte layout
and never re-recorded. The test comment itself says "update deliberately if the mesh
format changes" — the format changed deliberately; the re-record just didn't happen.
No PBR phase-1/phase-2 worker touched this path (verified: `block_materials.cpp`
identical to the zip; the only `mesher.cpp` delta is the slot plumbing, which writes
0 when `MeshContext::strengths` is null, as in this test).

### Inspection (before re-recording)

Actual output: hash `dcb1c88e3af94693`, 25 quads. A scratch harness
(`/tmp/golden_inspect.cpp`, not committed; links the engine static libs and rebuilds
the exact fixture scene) verified 14/14 checks, ALL PASS:

- **Byte-identity proof (the decisive check):** stripping the 4-byte `slot` from every
  16-byte vertex recovers the old 12-byte layout; FNV-hashing those 1200 bytes yields
  **exactly the recorded golden `9a6c89e230a77b13`**. The current output differs from
  the golden-era output ONLY by the documented format change — every other vertex
  value (positions, UVs, AO, layer, value, mode) is byte-identical.
- All 100 vertices have `slot == 0` (trace Step-1 writes nothing with null strengths).
- Face set EXACTLY matches hand-derived expectation (21 cube/slab quads + 4 cross
  quads): shared face between the two adjacent rocks culled on both sides; slab is
  half height (y 32..40) with its bottom face culled by the rock below; plant = 4
  cross-sprite quads in `kShadeCutout`; corner rock (15,15,15) emits all 6 faces.
- All 21 cube/slab quads wind CCW seen from outside (signed-area > 0 in face space;
  the AO diagonal flip only rotates the start vertex); no duplicate quads; all
  positions <= 256, UVs <= 16, `layer`/`value` == 0, modes only `kShadeTex`/`kShadeCutout`.
- Corner rock (all-air neighbours): AO == 3 on every corner, as the algorithm dictates.
- Meshing is deterministic across runs (memcmp-identical vertex streams; no
  uninitialized bytes — consistent with the packed all-integer layout).

### The re-record

`tests/engine_tests.cpp` (golden check): `0x9a6c89e230a77b13` ->
**`0xdcb1c88e3af94693`**, with a comment documenting the re-record (date, worker,
cause, the byte-identity proof, and the old value kept for the record). Only this
file was touched.

### Final verification (last worker)

- Engine library targets + `litholattice_tests`: build exit 0, **zero new warnings**
  (remaining warnings are the pre-existing set: `block.h:90` narrowing,
  `audio_selftest` shift-count, `testPbrBrdf`/`engine_tests` unused vars).
- `ctest`: **15/17 pass.** `engine_render` now PASSES with the re-recorded golden.
  The 2 remaining failures are exactly the grandfathered baseline, both unrelated to
  this work and untouched: `engine_sim` block-names (`world/block.h:90` narrowing),
  `lithokraft_selftest` SIGABRT (blocks.cpp 40-vs-36 guard).
- The full `make` (all targets) does NOT currently exit 0, for a reason outside this
  workstream: the **game target fails to compile** — the debug-console workstream's
  mid-flight `src/console/include/litholattice/console/debug_console.h` references
  `litholattice::ui::TextEditor`/`TextBox`, which don't exist yet (a new
  `engine_console` ctest also appeared; it passes). Per constraints this was
  documented and left untouched for the console worker. All engine modules build.
- Tree-collision note: the tree is being edited concurrently despite the "work alone"
  brief — a relink landed mid-`ctest` during this session and produced transient
  `BAD_COMMAND` failures on one run; a clean re-run gives the 15/17 above. The
  reconciliation pass JD/builder have planned should serialize or snapshot the tree.

### Decisions locked (Worker 5's)

- Re-record the golden (yes) — justified by the byte-identity proof + geometric
  inspection above, per JD's "verified correct by inspection" bar.
- Drift attribution corrected for the record: deliberate mesh-format change (trace
  Step-1 `slot` field), NOT platform/compiler drift.

### Open questions (for JD / builder)

- None from the golden itself — it is resolved and green.
- The game-target build breakage (`debug_console.h` vs missing `ui::TextEditor`/
  `TextBox`) belongs to the debug-console workstream; needs their fix before the
  full `make` (all targets) is green again or `lithokraft_selftest` can be rebuilt.

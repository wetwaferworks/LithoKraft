# PBR Phase 1 Implementation Report — 2026-10-10

Phase-1 goal (JD): correctness first — lock in a correct simplified Cook-Torrance PBR path; scale up later. All six pieces implemented sequentially by dedicated workers, each verified building green with self-tests. Tonight's six optimizations (TickArena, OcclusionField, binary mesher, .llar, net snapshots, deterministic scheduler) untouched and still green.

## JD's locked decisions (implemented, not relitigated)
- Simplified Cook-Torrance, energy-conserving. No full GI.
- Many lights: 1–64 light-level flood fill = diffuse for unlimited torches; K nearest emissive blocks (default 8) = analytic lights for specular.
- Data-driven material registry, never hardcoded. String IDs; mods register via data files; voxels store 8-bit palette index → per-chunk palette of 16-bit IDs; saves persist string IDs.
- Transparent tiers, all four user-selectable: simple → screen-door → PBR reflections → true refraction (Ultra).
- Voxel advantage kept: axis-aligned faces, constant normals, no normal mapping, lean BRDF.

## Per-piece status — all DONE

### 1. Material registry — DONE
- `engine/src/render/include/litholattice/render/pbr_materials.h` — `MaterialDef` (albedo rgb + texture ref, roughness, metalness, emissive rgb + intensity, IOR, alpha, reuses `TransparencyMethod`, `opaque()` helper), `MaterialRegistry` (string-ID registration, dense 16-bit compact IDs, `0xFFFF` reserved invalid), `ChunkPalette` (8-bit voxel index → 16-bit global ID). Header documents the `BinaryQuad::material` passthrough contract and the BlockMaterials relationship (PBR = physical params, BlockMaterials = texture bindings).
- `engine/src/render/pbr_materials.cpp` — registry + JSON data-file loader via `core/json.h`.
- `engine/assets/materials/lithokraft.materials` — 16 LithoKraft defaults as data (stone, dirt, grass, wood, log, leaves, sand, glass, ice, water, lava, torch, iron, copper, gold, obsidian); format documented in-file.
- Small decisions locked: conflicting re-registration = error (original kept); identical = idempotent; no `unregister` (saves reference string IDs); defaults loaded via explicit path (game owns asset-path resolution).
- Tests: 22 checks (registration, string+compact lookup, palette round-trip, duplicate handling, data-file load). All pass.

### 2. BRDF core — DONE
- `engine/src/render/include/litholattice/render/pbr_brdf.h` + `engine/src/render/pbr_brdf.cpp` — `CookTorrance`: Fresnel-Schlick (F0=0.04 dielectrics), GGX NDF, Schlick-GGX geometry (Epic k), Lambertian diffuse with kD=(1−F)(1−metalness), metals zero diffuse. Approximations documented with error bounds.
- Tests: 9 checks incl. **energy conservation sweep** (4 angles × 5 roughnesses × metal 0/1; worst observed 0.96 vs 1.05 bound — a real energy-creation bug would read 3–12×), F0=0.04 known answer, near-mirror lobe, monotonicity. All pass.

### 3. Many-lights hybrid — DONE
- `engine/src/render/include/litholattice/render/pbr_lights.h` (header-only) — `AnalyticLight`, `LightCollector(maxLights=8)` (bounded max-heap, nearest-first, ties stable), `irradianceFromLightLevel` (diffuse=max(block,sky)/64, ambient=sky/64), `attenuatedRadiance` (inverse-square +1 softener, specular-only), `filterCandidatesViaLighting` ILighting adapter. Renderer-decoupled by design.
- Tests: 18 checks (K-nearest ordering, K cap, empty/K=0/zero-intensity, tie stability, irradiance sanity, radiance values). All pass.
- Honest gap (documented, not guessed): **no world-side emissive enumeration exists** — `LightEngine` retains no source list. Callers supply candidates for now; a world-side `forEachEmissive` is needed later.

### 4. Transparent tiers framework — DONE
- `engine/src/render/include/litholattice/render/pbr_transparency_tiers.h` (header-only) — `PbrTransparencyTier { Simple, ScreenDoor, PbrReflections, TrueRefraction }`, runtime-settable config, single `shadeTransparent()` entry point. Simple = classic alpha blend; ScreenDoor = full PBR + 4×4 Bayer discard (no sorting); PbrReflections = PBR reflection + simple transmission (energy split exposed); TrueRefraction = **stub only** (sets `refractionRequested`, fills Snell ray + IOR via real Snell math with TIR handling, leaves color black; phase-2 contract documented). Additive is tier-independent (locked small decision); opaque bypasses at every tier.
- Tests: 24 checks (ladder order, determinism, Bayer exactness, F0≈0.04 glass, energy, stub seam). All pass.

### 5. Software-renderer integration — DONE (the chain proof)
- `engine/src/render/include/litholattice/render/pbr_software_shaders.h` — `pbrFragmentShader()`: materialId → registry → CookTorrance::shade() vs K-nearest lights + fill irradiance + emissive → linear→sRGB → RGBA8. `IEmissiveProvider` seam (documented temporary stub; contract stable for the future world-side provider). Missing/null context degrades to `basicFragmentShader` (untouched).
- Wiring: `ShaderVaryings::materialId`, `FragmentInput::pbr` context, `SoftwareRenderer::setShadingTier/setPbrContext`, per-pixel branch honors tier.
- GLSL: `engine/assets/shaders/pbr_shading.glsl` (term-for-term BRDF mirror, no #version so GL330+Vulkan450 can include; documents std430 64-byte `PbrMaterial` layout) + `chunk_pbr.vert/.frag` reference programs — **not wired** into gl33_renderer (no half-wiring); header documents the exact phase-2 seam.
- Tests: 13 checks (CPU↔BRDF parity, material-ID→pixel chain, fallback, tier selection, renderMesh smoke). All pass.
- **Incident fixed:** latent UB — `FragmentInput fin;` left pointer fields indeterminate; harmless while the per-pixel path was dead code, segfaulted once reachable. Fixed with value-init in software_renderer.cpp + test. Verified with repeated clean runs. No behavior change to existing paths.

### 6. Mesher↔registry consistency — DONE
- `testPbrMeshChain()` (12 checks): synthetic multi-material scenes → `binaryMeshChunk16` → every quad's `material` resolves through ChunkPalette → registry to the correct def (albedo/roughness/metalness verified per unit face via `expandBinaryQuad`); null array → material 0; material-aware merging intact (same-material merges, different-material splits); **palette overflow = error** (locked: 256 entries max, never silent truncation).
- Data-file integration: loaded `lithokraft.materials`, meshed stone/iron/torch scene, spot-checked defs (stone 0.50/0.95, iron metal 1.0/0.35, torch emissive 2.0).
- `testPbrVertexSeam()` (5 checks): `PackedVertex` has no material slot — 0xFFFF degrades **bit-identical** to `basicFragmentShader`. Decision note in `renderer.h`: lean **Option B (parallel attribute stream)** for phase 2 — lower blast radius than extending PackedVertex (+12.5% vertex bandwidth, golden-hash churn). Final call JD's/builder's.

## Verification (coordinator-ran, final)
- Full engine build: exit 0, zero new warnings (remaining warnings pre-existing: `greedy_mesher.cpp` unused vars, `block.h` narrowing, `testPbrBrdf` unused var, game_ui initializers).
- `ctest`: **10/13 pass** — the 3 failures are exactly the known pre-existing ones, verified unchanged from the pristine tree (optimization program verified via revert+rebuild on 2026-10-10): `engine_render` golden hash (platform/compiler drift vs recorded hash; mesher untouched by PBR workers), `engine_sim` block-names (`world/block.h:90` narrowing), `lithokraft_selftest` (SIGABRT under ctest; pre-existing).
- No new failures introduced by any PBR worker. Tonight's six optimizations still green.

## Open questions for JD / the builder
1. **Startup wiring**: who calls `MaterialRegistry::loadFile` and with what asset path (game-integration decision).
2. **Voxel material storage**: where the chunk's per-voxel 8-bit material bytes live (world/serialization layer).
3. **Emissive enumeration**: chunk emissive scan vs retained light-engine source list for world-side `forEachEmissive`.
4. **Vertex format**: parallel attribute stream (lean) vs extending PackedVertex — phase-2 call.
5. **GL backend activation**: `chunk_pbr` replaces `chunk.frag` wholesale or coexists per material.
6. **Phase-2 refraction**: scene-color copy resolution, sampling strategy, pass structure.
7. **Tier UI composition**: `graphics_tiers.h` vs `PbrTransparencyTier` relationship in config/UI.
8. **Golden hash**: re-record the drifted mesher golden or investigate (pre-existing; not PBR-caused).

## Phase 2 (not started)
Vulkan/GL backend wiring (`chunk_pbr` activation, SSBO upload, per-frame light uniforms), world-side emissive enumeration, per-voxel material storage in chunks, true refraction scene-color path, tier UI.

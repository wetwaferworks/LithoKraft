# Wafer Biomes — Procedural Generation Report (2026-10-11)

JD authorized full implementation: "we gotta have something to look at."

## Design source
- `~/workspace/goals/lithokraft-game/files/LithoKraft-Lore-Glossary-v2-2026-10-11.md` — "Wafer surface biomes" section (5 biomes, energized ground, Minecraft inversion)
- Concept art: `~/workspace/user/files/concept-art/` (Trace Forest, TurtlOR, Dead Zones, Dicing Plains) — LOCKED look-and-feel

## The five biomes
1. **Trace Forest** — circuit-trees (via trunks, trace branches, red/blue glowing tips), dense
2. **Dicing Plains** — vast flats, die-grid scribe lines, sparse, first-spawn territory
3. **Capacitor Groves** — squat capacitor-can clusters, dense, maze-like
4. **Resonator Spires** — tall quartz crystal spires
5. **Dead Zones** — grey, unpowered, broken circuits poke through

## New blocks (game/LithoKraft/src/content/blocks.{h,cpp}, ids 38-50)
| ID | Block | Notes |
|----|-------|-------|
| 38 | trace_trunk | tree trunk (tex: trace_bark) |
| 39 | trace_branch | tree branch (tex: trace_canopy) |
| 40 | trace_tip_red | glowing red tip (tex: trace_tip) |
| 41 | trace_tip_blue | glowing blue tip (tex: trace_tip) |
| 42 | capacitor_can | grove trunk (tex: capacitor_side) |
| 43 | resonator_crystal | spire crystal (tex: quartz_spire) |
| 44 | dead_silicon | dead-zone surface (tex: wafer_ground_dead) |
| 45 | fossil_trace | underground dead trace (tex: wafer_ground_trace) |
| 46 | broken_via | broken via stub (tex: wafer_ground_dead) |
| 47 | scribe_silicon | Dicing Plains surface (tex: wafer_ground_dicing) |
| 48 | dense_silicon | Capacitor Grove ground (tex: wafer_ground_capacitor) |
| 49 | flecked_silicon | Resonator Spire ground (tex: wafer_ground_resonator) |
| 50 | dead_memory_cell | dead memory bank cell (tex: memor stub) |

**ID fix (2026-10-11):** the disabled kLithographOR/kChipOR (36/37) left a gap — BlockIds 38+ were off by 2 from registration positions (kTraceTrunk=40 actually indexed trace_tip_red). Renumbered dense (36–49), removed the unreferenced disabled constants. Saves are safe (string ids). Children use constants, so no code changes needed on their side.

Texture keys reuse existing art until CloudAtlas does the biome art pass.

## Stub textures (JD, 2026-10-11)
10 biome stub textures generated (pixel-art style) at **64x64 masters** with **32x32 + 16x16 step-downs** (mip chain for the agent renderer pipeline — MCP vision renders tiny frames):
- Grounds: wafer_ground_trace/dicing/capacitor/resonator/dead
- Features: trace_bark, trace_canopy, capacitor_side, quartz_spire, trace_tip
- pack_64/: 64px (162 textures now) · pack_32/: 32px (162 textures) · pack_16/: NEW pack, 16px (10 textures, stub-only)
- Registered in all three manifest.json + resources.json (via convert_pack_manifest.py). `--pack pack_16` works (flag takes any pack dir name).

## Biome blending (JD-locked 2026-10-11)
Minecraft-style: no hard borders — terrain height, surface palette, decoration density, and pulse pattern all blend across biome boundaries. Engine-general system (Wafer configures via JSON; Organic etc. later). Selector outputs weight vectors per column; terrain blends heights, surface dithers palettes, features scale density by weight, pulse interpolates waveform/frequency/tint (fading to zero at Dead Zones).

## Workstream results

### Biome system — DONE (2026-10-11)
**Engine-general blending** (`engine/src/procgen/stages.{h,cpp}`):
- **`BlendedBiomeSelector`**: per-column weights over top-K nearest biomes (softmax over distance gap; max border step 2–3, no hard walls). `blend_radius` tunable (Wafer: 0.25, K=3)
- **`BiomeHeightmapTerrain`**: height = weight-averaged per-biome height functions
- **`LayeredSurface` + `ditherSeed`**: weight-proportioned palette dither (seeded)
- JSON-only opt-in for other dimensions: `{"selector": {"type": "blended", ...}}`
- Sibling contract: `ColumnMaps::biomeWeight`, `IBiomeSelector::blendWeights()`/`biomeCount()`, `blendLerp()` for the pulse
- **wafer.json**: climate noise (T/H/C/E) → 5 biomes; Forest(0.72,0.70) Plains(0.78,0.22) Groves(0.42,0.32) Spires(0.55,0.90) Dead(0.22,0.22)
- **Game** (`wafer_biomes.{h,cpp}`): `WaferPaletteSurface` (per-biome tops by BlockId, weight-dithered), `ScribeLineSurface` (1-wide grid every 32, weight-faded), `CrystalAccentSurface` (~4%, weight-scaled)
- Tests: engine selftest PASS (8 wafer + 14 blend checks); 278k-column verifier ALL 20 PASS (determinism, weights sum to 1, plains flat spread 2, all palettes track weights within 3%)
- Balance note: Capacitor Groves ~50% (nearest climate mean) — reads as the "default" biome; one-line JSON point move if JD wants it rarer

### Energized ground pulse — DONE (2026-10-11)
The wafer's heartbeat, JD-locked. Render/emissive-level only (sim tick untouched):
- **Engine** (`render/ground_pulse.h`): `groundPulseEmissive()` — pure function of (world time, chunk pos, biome weights) → additive emissive RGB. Per-CHUNK phase offsets (integer hash of chunk coords), never per-block timers (~20 flops/fragment)
- **Per-biome waveforms**: TraceForest (blue, slow), DicingPlains (subtle), CapacitorGroves (warm), ResonatorSpires (bright/sharp), DeadZones = zero (fades out smoothly, no cutoff)
- **Blend at mesh time**: biome weights baked into ChunkMesh as two vec4s (freq/sharp/amp + tint); GL33 (uniforms) + Vulkan (push constants); GLSL mirror in chunk_shading.glsl
- **Game** (`wafer_pulse.cpp`): WaferPulseCache — per-chunk weight-blended params from the biome system
- **Tips** wired to `kShadeEmissive` in appearance.cpp (string-id matched, renumber-proof)
- **Cvar** `render.ground_pulse` (live toggle, no remesh); time from sim world-tick (pausing freezes the pulse — never wall-clock)
- Tests: 14/14 engine pulse PASS, headless harness 32/32, content selftest pulse checks PASS

### Silicon trees — DONE (2026-10-11)
Three `procgen::IFeature` placements (`engine/src/procgen/stages.{h,cpp}`), deterministic, chunk-local-safe, gen-time only:
- **Trace tree** (`trace_tree`): via trunk 4–7 tall, 2–4 branches, one tip color per tree (15% red, 85% blue per lore)
- **Capacitor can cluster** (`capacitor_cans`): 3–7 squat cylinders, tight clusters
- **Resonator spire** (`resonator_spire`): 8–16 tall crystal spike
- Cross-chunk determinism via position-keyed seeds + deterministic overlap margin (never overwrites non-air); 0 trees in Dead Zones (strict gate)
- **Blend contract** (defined here, biome sibling adopted): `ColumnMaps::biomeWeight` + `IBiomeSelector::blendWeights()`; effective density = Σ density·weight per column
- Tests: 20/20 tree tests + blend tests PASS (3×3 replica, determinism, density scaling)
- Note: fixed the headless harness's `patch_blocks.cmake` (my guard-text change broke its workaround; now matches the stable abort suffix)

### Underground ruins — DONE (2026-10-11)
**`procgen::RuinsCarver`** (new `ICarver`, `engine/src/procgen/`):
- **Fossil traces**: horizontal dead-trace veins (block 45), 8–24 long, Manhattan routing (right-angle bends only — circuits route orthogonally), 15% missing segments, 1-block PCB spacing
- **Broken via stubs** (block 46): 2–5 tall, hard-capped ≥6 below surface (never breach), 35% grow a trace from the base
- **Delaminated layers**: 35% of chunks get thin buried stone/dirt bands
- Strictly chunk-local (veins turn at borders, zero neighbor reads); seeded via `deriveSeed(chunkSeed, "ruins")`; per-biome density multipliers from JSON; replaces crust 1:1 (no structural voids — largest buried air region 2 blocks)
- **Namespaced-id-first material resolution** in `column_generator.cpp`: `lithokraft:fossil_trace` → 47 (required — tex keys are shared); plain texture names still work
- Tests: 8/8 PASS (determinism, no surface breach, orthogonality, density, solidity)

## How JD finds each biome in-game
- **Dicing Plains** — the spawn area (first impression): vast flats, die-grid scribe lines every 32 blocks, subtle pulse
- **Capacitor Groves** — most common (~50%): dark dense metallic ground, squat capacitor-can clusters, warm pulse
- **Trace Forest** — rolling terrain (~21%): via-trunk trees with red/blue glowing tips, slow blue pulse
- **Resonator Spires** — (~12%): tall quartz crystal spikes, bright sharp pulse
- **Dead Zones** — rarest (~0.9%): grey cracked ground, no pulse (the absence is the tell), surface ruins poke through
- **Screenshots**: once the screenshot workstream lands, `screenshot()` in the console captures what you find; the lowres variant matches agent vision
- **Gap filled (in progress)**: `locatebiome` + structures framework + `genmap("out.png", center, zoom, granularity)` workstream running — 256×256px canvas, zoom 0–6 (256→16K blocks), granularity 8/16/32/64 blocks-per-pixel, coarse granularity enforced at high zooms (perf rule), shared core for future in-game map items.

## Test summary
| Workstream | Tests | Result |
|---|---|---|
| Biome system | engine selftest (8 wafer + 14 blend) + 278k-column verifier (20) | ALL PASS |
| Silicon trees | 20 tree + blend tests | ALL PASS |
| Energized pulse | 14 engine + headless 32/32 + content selftest | ALL PASS |
| Underground ruins | 8 ruins checks | ALL PASS |
| Stub textures | 30 PNGs placed, 3 packs registered | OK |
| Screenshot | new ctest `lithokraft_screenshot_selftest` 27/27 | PASS |
| Scouting (locatebiome/structures/genmap) | new ctest `lithokraft_scout_selftest` 90+ checks | PASS |
| ctest | 20/22 | 2 grandfathered (engine_sim, lithokraft_selftest) |

## Scouting commands (JD, 2026-10-11)
| Call | Effect |
|---|---|
| `locatebiome trace_forest` | Nearest biome coords + distance (spiral search, deterministic, cheat-gated) |
| `genmap "out.png" player 6 64` | 16K-block overview at 256px, ~1s (center = `player` or `x,z`) |
- **Structures framework** (engine-general, blueprint JSON): transformer_ruin (Dead Zones), dead_memory_bank + scribe_monument (Dicing Plains, die-grid aligned)
- **Zoom table**: 0=256 … 6=16384 blocks; perf rule: ≤512px fast, >512px warned, >2048px refused
- Sweet spots (256×256 in ~1s): zoom 4+g16, zoom 5+g32, zoom 6+g64

## Screenshot commands (JD, 2026-10-11)
Lithoscript, registered in game_commands.cpp (console + .lth):
| Call | Effect |
|---|---|
| `screenshot` | Full-res PNG, auto-named `screenshots/lithokraft-YYYYMMDD-HHMMSS.png` |
| `screenshot "path.png"` | Full-res PNG to path |
| `screenshot lowres` | 160×90 (agent vision fidelity) |
| `screenshot "path.png" lowres` | Path + lowres |
| `screenshot fragment [lowres] [caption]` | Into the Fragment gallery |
- GL (glReadPixels) + VK (staging-buffer copy); software renderer not wired (standalone rasterizer)
- Fragment gallery data model built (images section, lore page 5); future UI binds to it
- Note: live GPU readback untested headless — first real capture should be eyeballed on JD's machine

# Procedural Texture Generator — 2026-10-12

**End state: build clean · ctest 36/39 (new `engine_texture` test passes; the 3 failures are the documented pre-existing set) · game selftest 840 PASS / 7 FAIL (exactly baseline) · spawn frame near-white pixels: 0.000% (was ~100%).**

JD's order: the Wafer rendered solid white; build a modern 64×64 procedural texture generator into the engine — clean-room, data-driven recipes, Tron/electron-scale art direction, texture everything flat or placeholder, measure performance. All of it landed.

---

## 1. Why the frames were white — root cause and fix

**Root cause (a capture-tool wiring bug, not missing art):** `game/LithoKraft/src/app/biome_capture.cpp` meshed with a default-constructed `BlockMaterials` — every face resolves to the missing-texture layer 0 — and handed `SoftwareRenderer` a single 16×16 all-white tile "because the shader needs a texture". The pack art was never loaded, never resolved, never bound. The real game was fine; only the screenshot path was blind.

**Fix (in `biome_capture.cpp`):** the tool now loads resources exactly like the game — `lithokraft::ResourceManager` over `{engine/assets, game/assets, game/assets/packs/pack_64}`, kept alive for the run, `MeshContext` built from `rm.materials()`. Since the software shader samples a fixed 16×16 tile per layer (`software_shaders.h:83`), each registry texture (64×64 RGBA) is box-downsampled to 16×16 into the renderer's `layer*256 + ty*16 + tx` buffer, packed `0xAARRGGBB` (verified against the fragment shader's extraction). A dedicated **spawn capture** was added: 3×3 chunks around column (0,0), surface-Y scan, same camera math as the biome shots. Worldgen, mesher, and sky colour unchanged.

---

## 2. Research surveyed (clean-room)

**Clean-room statement:** the survey covered *techniques and math concepts only*, described in the researchers' own words. The implementation is 100% original code written from those concepts — no snippets copied, no implementations ported, no third-party code added.

Sources surveyed:

- Inigo Quilez — Domain Warping: https://iquilezles.org/articles/warp/
- Shaderpath — Domain Warping theory: https://github.com/dris1153/shaderpath/blob/HEAD/content/lessons/07-procedural/domain-warping/theory.en.mdx
- Hesiod — NoiseFbm node reference (periodic/tileable FBM): https://github.com/otto-link/hesiod/blob/HEAD/docs/node_reference/nodes/NoiseFbm.md
- Tileable Noise — 4D Clifford-torus sampling: https://github.com/omgkaustav/tileable-noise/blob/HEAD/README.md
- Tileable-noise discussion (torus route): https://forums.odforce.net/topic/41828-tileable-noise/
- Blender Manual — Noise Texture node: https://docs.blender.org/manual/en/4.0/modeling/geometry_nodes/texture/noise.html
- Blender Manual — Voronoi Texture node: https://docs.blender.org/manual/pt/3.2/render/shader_nodes/textures/voronoi.html
- Substance 3D Designer — 3D Perlin Noise node: https://helpx.adobe.com/substance-3d-designer/substance-compositing-graphs/nodes-reference-for-substance-compositing-graphs/node-library/texture-generators/noises/3d-perlin-noise.html
- Substance 3D Designer — Distance node: https://helpx.adobe.com/substance-3d-designer/substance-compositing-graphs/nodes-reference-for-substance-compositing-graphs/atomic-nodes/distance.html
- Substance 3D Designer — Material Color Blend: https://helpx.adobe.com/substance-3d-designer/substance-compositing-graphs/nodes-reference-for-substance-compositing-graphs/node-library/material-filters/blending-material/material-color-blend.html
- Unity Shader Dev — Voronoi/cellular notes: https://github.com/alex-rachel/unity_shader_dev/blob/HEAD/techniques/voronoi-cellular-noise.md
- 30 Shaders in 30 Days — Cellular Noise (F1/F2/F2−F1): https://github.com/akghxhs55/30-shaders-in-30-days/blob/HEAD/shaders/chapter3_procedural/day18_cellular_noise/README.md
- Soulburn Studios — Procedural Noise lessons: http://www.neilblevins.com/art_lessons/procedural_noise/procedural_noise.html
- Material Maker (node-graph structure/vocabulary): https://github.com/RodZill4/material-maker
- TextureLab: https://github.com/njbrown/texturelab
- Procedural Texture Lab: https://github.com/danielsobrado/procedural-texture-lab
- Ordered Dithering / Bayer matrices: http://en.wikipedia.org/wiki/Ordered_dithering
- Soreine/pixel-art-maker (palette + positional dithering): https://github.com/Soreine/pixel-art-maker
- Xorshift/SplitMix seeding practice: http://en.wikipedia.org/wiki/Xorshift
- FastNoiseLite (surveyed via vendored README — feature vocabulary only): https://github.com/paperprototype/voxels-unity-course/blob/HEAD/FastNoiseLite/README.md

Design conclusions adopted: **periodic lattice noise as the tiling contract** (integer period per octave; mirror/cross-blend rejected as repair tools, not a generation strategy), structure/colour decoupling (scalar field → palette map last), per-node seeds derived by integer hashing (never traversal order/time), Bayer 4×4 dithering (divides 64, never breaks tiling).

---

## 3. The generator — `engine/src/texture/` (new module `litholattice_texture`)

**API (fixed contract):**

```cpp
namespace litholattice {
  Image generateTexture(const TextureRecipe& recipe, uint64_t seed, int size);
  const Image& generateTextureCached(const TextureRecipe&, uint64_t seed, int size); // LRU, 64 entries
  bool parseRecipe(std::string_view text, TextureRecipe* out, std::string* err);
  std::string recipeToText(const TextureRecipe& recipe);
}
```

**Recipe format:** line-based text, one node per line, `key=value`, `#` comments. Nodes evaluate in order into a float **field stack**: generators push a [0,1] field sampled at the current (possibly warped) coordinates; modifiers transform the top field; `blend` pops two fields and pushes the combination; `palette` maps the top field to RGBA; `glow`/`dither` are post steps. Parse errors carry line numbers; unknown nodes/keys/values and out-of-range params are rejected, never silently dropped. Floats serialise with `%.9g` (exact round-trip, test-asserted).

| Node | Parameters | Purpose |
|---|---|---|
| `noise` | `kind=value\|gradient`, `scale` (cells), `octaves`, `gain`, `lacunarity`, `seed` | Tileable value / Perlin-style gradient FBM |
| `voronoi` | `scale`, `mode=f1\|edge`, `seed` | Cellular field; `edge` = F2−F1 crack networks |
| `warp` | `strength`, `scale`, `octaves`, `lacunarity`, `gain`, `seed` | Accumulates a domain offset applied to later generators |
| `ridged` | — | v → 1−\|2v−1\| (crests, veins, strata) |
| `billow` | — | v → (2v−1)² (puffy lumps) |
| `curve` (alias `remap`) | `contrast`, `gamma`, `posterize`, `invert`, `clamp_min/max` | Histogram shaping; posterize = pixel-art bands |
| `blend` | `mode=multiply\|add\|min\|max\|screen`, `mix` | Combine two fields |
| `grid` | `cells`, `width` (cell fraction), `sharpness` | Scribe/dicing lines |
| `constant` | `value` | Base fills, tests |
| `palette` | `stops=pos:#rrggbb[,pos:#rrggbbaa…]` | Field → colour (RGBA stops; alpha enables sprites) |
| `glow` | `threshold`, `intensity`, `color` | Emissive pass over bright field areas |
| `dither` | `amount` | Ordered Bayer 4×4 |

**Tiling:** lattice coordinates are wrapped modulo the integer period *before* hashing; voronoi neighbour cells wrap; non-integer lacunarity is honoured by integer-rounding each octave's period and rescaling sample positions — tiling stays exact for any lacunarity.

**Determinism:** splitmix64-seeded permutation table built per generation; integer hashing only in the random path; no `rand()`, no time. Documented caveat: float libm rounding is identical for a given binary. The cache is keyed by (canonical recipe-text FNV-1a hash, seed, size) — identical key, identical pixels, test-asserted.

---

## 4. Performance (measured, determinism untouched)

Reference vs optimised generator, 64×64, min-of-batches on this VM. All optimisations are **pixel-identical** (asserted in tests against the reference path, which ships as `generateTextureReference`):

| Recipe | Reference | Optimised | Speedup |
|---|---|---|---|
| stone (gradient FBM) | 0.88–0.90 ms | 0.79 ms | ~1.1× |
| circuit (voronoi+grid+glow) | 0.85 ms | 0.70 ms | ~1.2× |
| warped ridges (warp + 5-octave FBM) | 1.93–2.24 ms | 1.39–1.41 ms | ~1.4–1.6× |
| billow clouds | 0.69–0.75 ms | 0.58–0.64 ms | ~1.1–1.3× |

Techniques applied: per-octave lattice-hash memo tables and voronoi feature-point memoisation (pure memoisation of the same hash), scratch-buffer pooling instead of per-node allocation, per-pixel scaled coordinates hoisted out of the octave loop, splitmix-seeded permutation/gradient tables. End-to-end in the game tool: all 32 Wafer recipes generate in **32.69 ms at 64px** (0.74–1.42 ms each), 8.65 ms at 32px, 2.25 ms at 16px — a full three-pack texture refresh is ~44 ms.

---

## 5. Art direction — concept-art match

Concept art lives at **`~/workspace/user/files/concept-art/`** (biome concepts: trace forest, dicing plains, dead zones, turtlor) and **`~/workspace/lithokraft/concepts/`** (wafer biome + item concepts); mob concepts at `~/workspace/user/files/lithokraft-mob-concepts/`. Palette extracted from the trace-forest and dicing-plains pieces and baked into the recipes:

- **Bases:** deep indigo/navy silicon plates (≈ #232B52 → #46538A) — never the pale gray-lavender of the old art
- **Glows:** cyan `#35E0FF` trace lines/channels, violet `#9D7BFF` dicing grids and amethyst, red `#FF3B5C` and electric blue tip orbs, magenta pin accents
- **Metal:** copper/amber `#B5762E → #E8A84C` trace-tree bark and capacitor flecks (JD's purple + orange, both present)
- **Dead Zones** stay deliberately muted — dark cracked gray-violet, the one quiet texture

---

## 6. Textures generated — 32 recipes, 96 PNGs

Recipes are data: **`game/LithoKraft/assets/texture_recipes/<name>.ltxr`** (the future player/mod authoring format). A new tool, **`texture_gen`** (`game/LithoKraft/src/app/texture_gen.cpp`; `--recipes <dir> --pack <dir> --size <n>`, per-texture seed = FNV-1a 64-bit of the texture name, printed per texture), generated every texture natively at 64/32/16 into all three packs. Texture **names are unchanged** — no block, model, or registry edits. Pack manifests refreshed via `engine/tools/convert_pack_manifest.py`: pack_64/32 content-identical (169 textures / 58 models), **pack_16 grew 14 → 32 textures (14 → 28 models)** as the Tier-2 set joined the mip pack. Determinism verified: two independent runs → all 32 SHA-256 hashes identical, at both 64 and 32; pack contents match a fresh generation 32/32 in every pack.

**Tier 1 — spawn-visible:** `silicon` (deep indigo plate, etched circuitry, faint cyan traces) · `wafer_ground_trace` (dark PCB + cyan trace web) · `wafer_ground_dicing` (dark slate + bright violet grid + cyan channel) · `wafer_ground_capacitor` (dense dark metal + copper flecks) · `wafer_ground_resonator` (violet-flecked + amethyst sparkle) · `wafer_ground_dead` (voronoi-cracked dark gray-violet) · `trace_bark` (copper/bronze, etched grooves) · `trace_canopy` (dark copper + cyan pin lights) · `trace_tip` / `trace_tip_red` (radial glow orbs, blue / red) · `capacitor_side` (dark can, copper band, label stripes) · `quartz_spire` (amethyst voronoi facets, bright core) · `silica_grass` (alpha sprite: transparent below field 0.70, glowing cyan-green fronds above, ~40% coverage) · `silica_tilled` (dark rows, violet furrows)

**Tier 2 — measured flat/placeholder** (variance < ~500 or ≤ 11 unique colours before): `stone`, `stone_slab_top`, `stone_slab_side`, `dirt`, `dirt_slab_top`, `dirt_slab_side`, `grass_top`, `grass_side`, `silicon_slab_top`, `silicon_slab_side`, `dish`, `overclockor`, `signal_receivor`, `signal_transmittor`, `chipor`, `portor`, `fusor`, `structure_block` — identities and hues preserved, real procedural depth added (e.g. stone: 210 unique colours, variance 832; chipor gained a pin grid; dish/signal blocks gained their rings).

Left as-is: the painted component/trace/torch/lamp art (measured rich, reads correctly) and icons. Mobs have no textures to replace — there is no Wafer mob content yet (see §9).

---

## 7. Frames — before / after

Before (previous run's spawn frame — the white render):

![spawn before](sandbox://workspace/user/files/wafer-frames/after/spawn.png)

After (same capture viewpoint, real materials + procedural textures):

![spawn textured](sandbox://workspace/user/files/wafer-frames/textured/spawn.png)

![trace forest textured](sandbox://workspace/user/files/wafer-frames/textured/trace_forest.png)

Full set in `~/workspace/user/files/wafer-frames/textured/`: `spawn.png`, `trace_forest.png`, `dicing_plains.png`, `capacitor_groves.png`, `resonator_spires.png`, `dead_zones.png`, `wafer_overview_map.png`. **Frame stats: near-white (>240,>240,>240) pixels = 0.000% in spawn and in all five biome frames** (target was < 5%). Coordinator visually confirmed both frames above: amethyst resonator spires, copper trace trees with cyan/red tip orbs, dark indigo textured ground — the living-circuit read is there.

---

## 8. Tests

- **`engine_texture` (new ctest): 47/47 checks PASS** — byte-identical determinism; exact field periodicity + image seam continuity; non-blank variance > 100 for all bundled recipes; recipe text round-trip (equal recipe + identical pixels); cache ≡ direct generation; parse-error reporting; optimised ≡ reference pixels for every recipe.
- **ctest: 36/39** (suite grew 38 → 39 with `engine_texture`). Failures are only the documented pre-existing set: `lithokraft_selftest`, `lithokraft_scout_selftest`, `lithokraft_chipor_selftest`. (`engine_audio` passed this run; across runs it remains the known flake, as does one audio-streaming check inside the game selftest.)
- **Game selftest: 840 PASS / 7 FAIL** — exactly baseline; the 7 are the pre-existing set (4× save/restore, mesher sub-chunk, Vulkan environmental, hotbar icons — the 15 missing icon files predate this work and are content, not code).
- **Build: clean.**

---

## 9. File-level changelog

**Engine (new module):**
- `engine/src/texture/CMakeLists.txt` — target `litholattice_texture` (links only `litholattice_assets`)
- `engine/src/texture/include/litholattice/texture/texture_recipe.h` — recipe/node data model
- `engine/src/texture/include/litholattice/texture/texture_generator.h` — API contract
- `engine/src/texture/include/litholattice/texture/texture_selftest.h`
- `engine/src/texture/texture_recipe.cpp` — text parser/serialiser
- `engine/src/texture/texture_generator.cpp` — generation, reference path, LRU cache
- `engine/src/texture/texture_selftest.cpp` — 47 checks
- `engine/CMakeLists.txt` — `add_subdirectory(src/texture)`
- `engine/tests/CMakeLists.txt`, `engine/tests/engine_tests.cpp` — `engine_texture` wired into ctest

**Game:**
- `game/LithoKraft/src/app/biome_capture.cpp` — real ResourceManager materials + downsampled texture buffer + spawn capture (Part 1 fix)
- `game/LithoKraft/src/app/texture_gen.cpp` — **new** tool
- `game/LithoKraft/src/app/CMakeLists.txt` — `texture_gen` target
- `game/LithoKraft/assets/texture_recipes/` — **32 new `.ltxr` recipe files**
- `game/LithoKraft/assets/packs/pack_{64,32,16}/textures/blocks/` — 32 PNGs regenerated per pack (pack_16: 14 replaced + 18 new)
- `game/LithoKraft/assets/packs/pack_{64,32,16}/manifest.json` + `resources.json` — refreshed via the pack converter (names unchanged; pack_16 resources grew as noted)

**Untouched per guardrails:** sim core, save format, network protocol, `trace_jit_basic.cpp`, block registry (no blocks added, no texture keys changed).

---

## 10. What the player / mod-tool version needs next

1. **In-game editor surface:** the recipe format is already plain text data — a mod tool needs a node-list UI over `.ltxr` + a live preview calling `generateTexture` (sub-millisecond at 64px, so preview can be per-keystroke). No engine changes required.
2. **Runtime (non-baked) use:** `generateTextureCached` is the hook for mods that ship recipes instead of PNGs — a pack entry type that carries a recipe + seed and resolves through the cache at load. Needs a `resources.json` schema addition in the assets loader.
3. **Golden-image regression:** pin a hash per shipped recipe (the survey's recommendation) so a future generator change can't silently repaint every world — the test harness already asserts optimised ≡ reference; extend it with stored hashes per `.ltxr`.
4. **Mob textures:** when Wafer mob content exists, the same recipes cover skins (voronoi scales, palette-mapped hides); nothing generator-side is missing.
5. **Height/normal companions:** recipes currently bake albedo only; the field stack makes a height output nearly free (the pre-palette field), which the PBR work can consume later.

**Known notes:** the spawn-column surface scan reports Y=255 (the worldgen column at (0,0) genuinely tops there; the frame renders correctly). One judgement call: pack_64/32 manifests were re-run through the converter in place only after a temp-dir run proved the output content-identical, so no existing texture name could be dropped.

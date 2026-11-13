## 0.0.1.49/0.0.1.50 — Vulkan push (2026-10-16, Proposed by JD, Built by Cortana)

Vulkan-only effects, each one a Graphics toggle (Settings > Graphics > "Vulkan effects"; `vkFx` bits in settings.json; `--vkfx N` CLI for benches). GL33 and software keep their current look. Everything is render-only: sim hashes match with effects on and off.

1. **CloudAtlas art (CloudAtlas-2026-10-16).** Trace Bulbs use a new ghost shell (`{red,blue}_trace_bulb_ghost`): the cutout dither at 30% alpha with the lens disc replaced by the glass core. Each shell has `_normal` and `_glow_mask` maps. The glass-core textures, normals and glow masks are registered in pack_32/64. Etched normals (`glass`, `red_trace_cross`, `wafer_ground_trace` `_etched_normal`) are registered in all 3 packs. Bulb faces are `kShadeAlphaBlend` (the Solid shell still turns them emissive). The `.lthm` models are under `assets/models/trace_bulb` and `assets/models/etched`. The new bindings loader (`content/material_bindings.{h,cpp}`) reads `assets/lithokraft/materials/bindings/<model-id>.json`:
   - glass rims 40/42 alpha-blend
   - cores 41/43 glow
   - Trace 5/6 glow
   - etched base 50, line 51

   Selftests cover it.
2. **Per-pixel lighting and normal maps.** Sun + hemisphere + Blinn-Phong, with normal maps on a cotangent frame built from screen-space derivatives (no stored tangents). Per-layer normal/glow maps come from naming conventions (`X_normal`, `X_etched_normal`, `X_glow_mask`).
3. **HDR bloom.** The glow masks feed an RGBA16F glow target. A half-res separable 9-tap Gaussian runs 2 iterations, then the composite pass adds `1-exp(-g)`. Lines and the HUD draw after the bloom, so they are not bloomed. **Pulsing glow** uses mask G as the phase on the sim clock (tick-synced, render-only).
4. **GPU particles.** Procedural Trace sparks (3072) with no vertex buffer, using a hash of `gl_VertexIndex` and a camera-wrapped cube. They draw additively into the frame and the glow target.
5. **Distance fog.** Exp² fog that thins with height, combined via max() with the existing fog.
6. **Blended bulb pass.** Meshes with alpha-blend faces draw again far→near (depth test on, depth write off). The opaque pass drops those faces.

Also: the window title now shows only `LithoKraft - Release 0.0.1.xx` (or Debug). Debug info stays in the in-game overlay. (Proposed by JD, Built by Cortana.)

Bench/screenshot helpers: the still bench logs its pose. `LL_BENCH_SKY="r,g,b"` overrides the sky in still benches (night shots).

### Credits / inspirations (technique only; clean-room, no code copied)
- Normal mapping without precomputed tangents: Christian Schüler, "Followup: Normal Mapping Without Precomputed Tangents" (blog, thetenthplanet.de).
- Integer hash for particles: Chris Wellons, hash-prospector, https://github.com/skeeto/hash-prospector (The Unlicense / public domain).
- Bloom (bright target + separable Gaussian ping-pong): LearnOpenGL "Bloom" by Joey de Vries, https://github.com/JoeyDeVries/LearnOpenGL (code samples CC BY-NC 4.0; only the technique was referenced).
- Multi-pass bloom in Vulkan (render pass / framebuffer structure): Sascha Willems, https://github.com/SaschaWillems/Vulkan (MIT).
- Exponential height fog: Iñigo Quílez, "Better fog" (article, iquilezles.org).

### Files
engine: assets/shaders/vk/{frame.glsl, chunk.vert, chunk.frag, particle.vert, particle.frag, post.vert, post.frag}, assets/shaders/chunk_shading.glsl (all new code under `#ifdef LITHO_VK_FX`), src/render_vk/{vk_frame_uniforms.h, vk_pipelines.h/.cpp, vk_swapchain.h/.cpp, vk_renderer.cpp, CMakeLists.txt}, src/render/{block_materials.cpp, include/.../block_materials.h, include/.../renderer.h, mesher.cpp}.
game: src/content/{appearance.cpp, content_selftest.cpp, CMakeLists.txt, material_bindings.cpp, include/lithokraft/content/material_bindings.h}, src/game/{settings.cpp, include/lithokraft/game/settings.h, game_ui.cpp, game.cpp}, src/app/main.cpp, assets/packs/pack_{16,32,64}/resources.json + textures, assets/models/{trace_bulb,etched}/*.lthm, assets/lithokraft/materials/bindings/*.json.
Backups: `.orig-2026-10-16-2030` (game.cpp also `-2045`).
Note: resources.json was edited directly. Re-running tools/convert_pack_manifest.py from manifest.json would drop these entries until the manifests are updated.

### Known issues (from the 0.0.1.50 Vulkan captures)
- With effects on, Trace Bulbs look like solid, bright gems instead of a ghostly shell. The blend pass alpha plus glow mask plus bloom come out too opaque and too hot. With `--vkfx 0`, they show the intended ghost shell (screen-door). Next step: tune the blend-pass alpha source and the ghost glow-mask gain.
- At a stream radius of 64, the Vulkan vertex arena runs out: "out of vertex memory; sub-chunk not drawn" appears 6.7k–9.9k times per 20 s bench. The 33-64 ring holds about 3.1–3.5 GB of the roughly 4.5 GB of mesh. That ring needs LOD, or the arena needs to be larger.

# Implementation Report, 2026-10-15 18:45 ET (batch 16:25 to 17:50, plus lithography blocks 17:55 to 18:45)

The order JD set was CA-03, then thread-audit F-2/F-4/F-5/F-8, then greedy meshing, then Hi-Z. After that came the self-review and verification. The full rows, numbers and backups are in CHANGES.md (see the section "Batch 16:25 to 17:50 ET").

1. **CA-03.** `PackedVertex{... uint32_t slot:24, layerHi:8}`, still 16 bytes. The layer travels through FaceMaterial, the mesher Look and the software varyings as 16 bits. GL and VK use a new byte-15 attribute. `kMaxLayers` is 2048. Pixels and the golden mesh checksum are unchanged; perf is neutral.
2. **F-2:** a Debug single-reader assert, plus a fix for a real MCP-thread read of `SimThread::stats()`. **F-4:** voice-count accessors. **F-5:** a registry instance id in the StableDefScope key. **F-8:** the seam audit is documented.
3. **Greedy meshing.** Merges are opt-in per `MeshContext::greedy`, which the game sets from `IRenderer::prefersGreedyMeshes()` (software = true). `swTile()` wraps u/v above 1 and is bit-exact in [0,1].
4. **Hi-Z.** Each 8x8 tile keeps a depth upper bound with lazy re-tightening. Rejection is per 8-row band, so drawn rows are bit-identical. Tiles align with the 32-row band strips.
5. **Self-review findings fixed while writing:**
   - Avoided a 196 KB stack temporary when clearing the greedy grid.
   - Kept greedy away from Trace, strip and other per-cell modes, so the Trace-mix pulse counts are unchanged.
   - Disabled Hi-Z after any depth re-fill that raises depth (the SSAA resolve, resize).
   - Killed an orphaned cl.exe left by an interrupted build.

**Verification:**
- Hash `aa82e7afb1eff9d3` in Release and Debug.
- Release and Debug ctest 46/48 (environment-only failures).
- Selftest 1238/0.
- Pixels 7/7 (default).
- Benches are in CHANGES.md.

## Addendum: lithography blocks

The full section from CHANGES.md is copied below.

#### Lithography blocks, 17:55 to 18:45 ET (JD approved 16:31): registration and render only
Backups for every edited source file: `.orig-1755`. The pack `resources.json` files: `.orig-1800`. The PNGs are new files. Source art: `lithography_r2/r3_2026-10-15.rar` (verified byte-identical to the box copy, 123 PNGs). Face map: r3 REGISTRATION-NOTES.md.

| Layer | File | Time(s) | Finding ID | What changed |
|---|---|---|---|---|
| GAME | game/LithoKraft/src/content/blocks.cpp, include/lithokraft/content/blocks.h | 1755 | LITHO-REG | 8 blocks appended, append-only, so no id moves and saves stay compatible: 74 `lithokraft:selector` SelectOR, 75 `connector` ConnectOR, 76 `laysor` Laysor (solid+transparent+directional, not opaque), 77 `emitor` Emitor, 78 `input_buffer` Input Buffer, 79 `fabor` FabOR, 80 `autofabor` AutoFabOR, 81 `vior` Vior (neutral arm run). All have no behavior and SimRole::None; the Etchor logic is the next pass. The EtchOR (57) and the red/blue Vias already existed and are not duplicated. Creative inventory: they appear automatically (the palette lists the whole registry). |
| GAME | game/LithoKraft/assets/packs/pack_16, pack_32, pack_64: resources.json, textures/blocks, textures/items, textures/ui, textures/ui/icons | 1800 | LITHO-ART | 34 block textures (r2: autofabor, buffer, connector, etchor x5, fabor, selector, vior blue/red/run; r3: emitor x4, laysor_crystal x3). The superseded r2 laysor_* art is skipped. Also the 2 r3 item textures, the `etchor_name_field.png` UI texture, 8 inventory icons, and per-face models for the 8 new blocks. The EtchOR model goes from `all: etchor` to per-face (front chip out, back wafer in, sides laser port, top blue in, bottom red in). vior_blue/red_* are registered as textures; the existing Vias keep their art. manifest.json was not regenerated (resources.json was edited directly). |
| ENGINE | engine/src/render/include/litholattice/render/block_appearance.h, engine/src/render/mesher.cpp | 1755 | LITHO-HOOK | New `IBlockAppearance::faceLookCell(cell, slot, active)` (default nullopt): a neighbourhood-aware face look. The mesher tries it before faceLookAt. It is stateless and safe on mesh workers. |
| GAME | game/LithoKraft/src/content/appearance.cpp, include/lithokraft/content/appearance.h | 1755 | LITHO-LOOK | The Laysor renders as kShadeAlphaBlend (blended, not cutout and not screen-door). The Emitor front aperture is kShadeEmissive. Beam as light: a Laysor reached through a run of Laysors by an Emitor front facing it gets value 1, a violet glow. The EtchOR side face the beam enters through turns emissive. No geometry is added. |
| ENGINE | engine/src/render/software_renderer.cpp, include/litholattice/render/software_renderer.h | 1755 | LITHO-BLEND | Blended chunk pass on the software per-pixel path. StoredMesh::hasBlend is set at upload. Inside each 32-row strip, opaque quads draw first (front to back), then the strip's AlphaBlend quads (far to near over meshes that have them). Blending is src*a + dst*(1-a) with the texel alpha: depth-tested, no depth write, no Hi-Z dirty mark. A strip belongs to one thread, so the read-modify-write is race-free. Meshes without blend quads take the unchanged path. |
| ENGINE | engine/assets/shaders/chunk_shading.glsl | 1755 | LITHO-GL | GL chunk mode 12 (AlphaBlend) falls back to the glass screen-door branch. There is no sorted GPU chunk blend pass yet (open item). |
| GAME | game/LithoKraft/src/content/content_selftest.cpp | 1755 | TESTS | The materials-block assert now chains to kSelectOR. New checks: ids 74-81 are dense and namespaced, have no behavior, and the Laysor is see-through. The Laysor face is AlphaBlend and the Emitor front is emissive (back plain). Selftest went from 1238 to 1240 PASS. |
| TOOLS | game/LithoKraft/src/app/biome_capture.cpp, hitch-logs/make-package.ps1 | 1755, 1845 | LITHO-SHOT | `biome_capture <dir> --litho` renders a lithography test scene (litho_blocks.png) and exits; the default run is unchanged (pixels 7/7). The package script also copies the screenshot. |

- **Hash:** `aa82e7afb1eff9d3`, unchanged after registration.
- **Release ctest:** 46/48. **Debug ctest:** 46/48. The only failures are the known environment ones, engine_systerian and engine_storage.
- **Release selftest:** 1240 PASS / 0 FAIL.
- **Pixels:** the 7 default biome_capture PNGs are SHA-256 identical to px-simd-scalar.
- **Short bench** (rq, Release software, 1080p): walk 47.6 FPS (rt8; p99 30.1 ms), orbit 48.7 FPS (rt8; p99 25.6 ms). For rt4: walk 33.5, orbit 33.9.
- **Screenshot:** `hitch-logs/litho_blocks_2026-10-15.png`. It shows the Emitor, then 3 lit blended Laysors, then the EtchOR; one unlit Laysor; and the SelectOR, ConnectOR, Input Buffer, FabOR, AutoFabOR, Vior, blue Via and red Via. The floor shows through the Laysors.
- **Open:**
  - The beam trace reads only the mesh pad (+-1 block past the sub-chunk), so a beam that crosses a 16-block seam stops lighting there. The first test scene showed this; the shot keeps the beam row inside one sub-chunk.
  - Emitor power gating and all Etchor logic and wiring canon come next pass.
  - Laysor quads inside one mesh are not depth-sorted.
  - The GPU uses screen-door, not true blending.
  - The flat (non-per-pixel) software path draws the Laysor opaque.
  - The EtchOR name field is a stub: texture registered, flow still the `etch <name>` console command.
  - The EtchOR lit port faces away from the shot camera, so it is not visually confirmed.
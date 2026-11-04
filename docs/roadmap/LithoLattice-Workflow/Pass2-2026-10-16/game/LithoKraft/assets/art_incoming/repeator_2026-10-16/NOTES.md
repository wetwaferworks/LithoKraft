# LithoKraft Repeator, 2026-10-15 (CloudAtlas)

## States (buttons physically press DOWN and stay down)
| State | Delay | Buttons down (left→right) | Model | Top texture |
|---|---|---|---|---|
| 1 | 2 ticks | 1 (0) | models/repeator_d2.lthm | repeator_top_d2_* |
| 2 | 4 ticks | 2 (0,1) | models/repeator_d4.lthm | repeator_top_d4_* |
| 3 | 8 ticks | 3 (0-2) | models/repeator_d8.lthm | repeator_top_d8_* |
| 4 | 16 ticks | 4 (0-3) | models/repeator_d16.lthm | repeator_top_d16_* |
Next press after state 4 goes back to state 1 (`next_state` in models/repeator_index.json).
Geometry is the main indicator: a raised button stands 3 cells (1.5/16) above the slab, a pressed one 1 cell (0.5/16). Glow is only an accent: pressed buttons glow dim when unpowered and full when powered. Raised buttons stay OFF gray.
You can also assemble it from parts: a slab plus `repeator_button_up.lthm` (4x3x4) or `repeator_button_down.lthm` (4x1x4) at x = 5, 11, 17, 23, z = 14..17, seated at y = 6.

## Model format
- `.lthm` v1 (the same format the octree/Litho Formula bake pipeline writes, per content/model/FORMAT.md). It is written directly by `build_repeator.py`: level 5 (32³ grid), author "CloudAtlas", leaves sorted z,y,x, with a CRC32. Not yet loaded/verified by the engine's `loadModel`, because I didn't build anything there.
- `*.voxels.json` is the same data as `[x,y,z,material]` at 32 resolution. Axes: x = right, y = up, z = front. Input (back) is at z=0, output (front) at z=31.
- The repeator is boxes, not a superformula shape, so the Litho Formula voxelizer (voxelizeLithoShape) doesn't apply. It goes straight to octree cells, which is the same thing ModelBuilder::fillBox would produce.
- The slab is 6/32 = 3/16 tall.

## Materials / tint (color comes from the feeding Trace via the engine tint property)
| id | name | neutral | tinted? |
|---|---|---|---|
| 1 | base | #2B3040 | no |
| 2 | bevel (top rim) | #3C4357 | no |
| 3 | etch (circuit lines) | #454D63 | no |
| 4 | glow_seam: inset seam ring, input/output chevrons, front/back nubs | #FFFFFF | **yes** |
| 5 | button_up | #9A9A9A | **yes** (shown as OFF gray) |
| 6 | button_down | #FFFFFF | **yes** (dim when off, full tint when powered) |
| 7 | button_well | #161922 | no |
Tint colors used in the previews: red off #5A5556, on #FF514B. Blue off #535659, on #2FB4FF (r3 ramp ends).

## Textures (pack_128/64/32/16, NEAREST downsample)
- `textures/blocks/repeator/repeator_<face>_base.png` is the untinted dark base (tint areas painted #161922).
- `..._tintmask.png` is the grayscale tint layer: 255 = full tint, 150 = raised button, 0 = none. The engine multiplies it by the Trace color and lays it over the base.
- Faces: `top_d{2,4,8,16}` (one per state), plus `front` (output), `back` (input) and `side` (left/right). The bottom uses the side base with no tint.
- `preview_tinted/{red,blue}/*_{off,on}.png` are baked composites for review only.

## Contact sheet
`contact_sheet_repeator.png` is an isometric render of the voxel models: 4 states × red OFF/ON, blue OFF/ON, plus a strip of the tinted top textures.

## Open questions
- Which engine tint property should be used (per-material or per-face)? This assumes per-material tint for materials 4/5/6.
- Old batch2 art used a delay of 1-8. This one uses the 4-state 2/4/8/16 cycle.
- Run the .lthm files through `loadModel` on a dev build to confirm they load.

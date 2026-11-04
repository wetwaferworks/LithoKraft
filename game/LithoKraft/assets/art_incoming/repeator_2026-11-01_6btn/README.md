# LithoKraft Repeator: 6-button top (CloudAtlas, 2026-11-01)
The only change is that the top now has 6 buttons instead of 4. The slab (3/16), materials, tint, seam ring, etch, chevrons, front output and back input nubs, sides and the purple seven-segment readout are all unchanged.

## Buttons (on the top face, in a row from left to right, input at z=0 and output at z=31)
| # | x cells (32 grid) | x in /16 | z cells | pressed in states | state -> ticks |
|---|---|---|---|---|---|
| 0 | 4-6   | 2.0-3.5  | 14-17 | 1-6 | state 1 = 2 ticks |
| 1 | 8-10  | 4.0-5.5  | 14-17 | 2-6 | state 2 = 4 |
| 2 | 12-14 | 6.0-7.5  | 14-17 | 3-6 | state 3 = 8 |
| 3 | 16-18 | 8.0-9.5  | 14-17 | 4-6 | state 4 = 16 |
| 4 | 20-22 | 10.0-11.5| 14-17 | 5-6 | state 5 = 32 |
| 5 | 24-26 | 12.0-13.5| 14-17 | 6   | state 6 = 64 |
- As before, the number of buttons pressed equals the state number: 2 ticks presses 1 button, and 64 ticks presses all 6. A press cycles the states 1 through 6, then back to 1 (see `next_state`).
- Each cap is 3 wide and 4 deep. The old caps were 4 wide, so the caps are 1 cell narrower to fit six. The wells share a 1-cell gap and run from x=3 to x=27. Caps stand 3 cells high when raised and 1 cell high when pressed, seated at y=6. That is the same as before.

## Files
- `models/repeator_d{2,4,8,16,32,64}.lthm` and `.voxels.json`, plus `repeator_index.json`, which lists 6 states. `repeator_button_up.lthm` is 3x3x4 and `repeator_button_down.lthm` is 3x1x4.
- `pack_{128,64,32,16}/textures/blocks/repeator/`: the base and tintmask for each face (`top_d*` for all 6 states) and the `preview_tinted` images.
- `textures/top_value/`: the approved readout top (base, mask and digit atlas), unchanged. The readout already has a 2-digit layout, so 32 and 64 display with no art change. This folder also holds the per-delay top tintmasks, rotated 180° to match it.
- `icon/repeator_{256,128,64,32}.png` is the new v4 icon. `repeator_old_*` is the old icon, kept for comparison.
- `preview_old_vs_new.png` is a 4054x1752 comparison. `contact_sheet_repeator.png` shows the 6 states in red and blue, powered and unpowered.
- To rebuild, run `build_repeator_6btn.py`.

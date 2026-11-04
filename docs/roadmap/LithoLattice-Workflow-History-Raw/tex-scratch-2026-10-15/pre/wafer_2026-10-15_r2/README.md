# LithoKraft Wafer refresh r2 (2026-10-15, CloudAtlas)
JD's decisions. Holds only the changed and new files; they sit on top of `../wafer_2026-10-15/`, which is untouched.
Generator: `scripts/refresh128_wafer_r2.py`. Each of pack_128/64/32/16 has 9 PNGs and a manifest.json.

## Changed: copper is off the Wafer surface palette (red and purple replace it)
- entity/coil_sprite and coil_sprite_front: the windings now use red/purple (#7A2C34 #AD4382 #FF514B #FFA29C)
- ui/icons/seedor: the seed head is now purple (it was copper)

## Underground / ore: copper stays in the copper ramp (#6A3A22 #9C5A30 #C47C42 #E8A84C)
- blocks/copper_block and ui/icons/copper: unchanged, still in r1. They're now classed as underground/ore content.
- ui/icons/silicon_copper: unchanged in r1. It's a copper-derived wafer, so it's ore-sourced too.
- NEW blocks/copper_ore and ui/icons/copper_ore: v3b substrate with Minecraft-style copper ore clusters

## NEW: alumitrace (not in canon yet)
- blocks/alumitrace, ui/icons/alumitrace, masks/alumitrace_emissive_mask
- The silicon v3b die grid runs underneath. A 24 px light-silver trace (#9AA3B8 #B4BCCD #CDD3E0 #E6EAF2) runs N-S with two square pads, flanked by a thin red #FF514B rail on the west and a blue #2FB4FF rail on the east.
- The emissive mask is white (opaque) on the silver trace body and pads and transparent everywhere else, for the engine's near-white glow.
- Like red_trace, it only draws the straight N-S piece. If it needs connection pieces (corner, t, cross, end, dot), those still have to be drawn.

`contact_sheet_r2.png` shows the changed files before and after (at 128 and 16), plus copper_ore and alumitrace as isometric cubes with their 128 and 16 faces and the mask.

# Art batch 3 (art v2.1, Oct 10 2026)
Scripts (new, nothing old changed): `scripts/blocks_batch3.py` (textures + masks + baked refs -> `batch3_blocks/`), `scripts/batch3_icons.py` (hotbar icons), `scripts/batch3_previews.py` (previews), `scripts/build_packs_v12.py` (adds batch 3 to `export/pack_64` + `pack_32`; v11 kept; pre-v12 packs backed up in `archive/export_packs_pre_v12_2026-10-10/`).
Style: batch-2 parts reused directly (casing `lamp_frame`, top plate `silicon_base` + `wafer_street` streets, outline #161922 at 2.5-3/128, rx 12, same socket/tab/arrow language). New colours: true red family #FF514B / #A8302C / #FFB3A8 (red icon family) for via cores and red redirector lanes; blue uses palette blue_wire_48 #2FB4FF / _64 #6FDCFF / _16 #1F5FA8. Wire pieces are untouched (neutral grey, shader-tinted).
Previews: `previews/blocks_batch3_sheet.png`, `previews/blocks_batch3_ingame.png`, `previews/blocks_batch3_style_check.png` (side by side with repeater/redirector/thresholder/silicon/wire/lamp, 64 + 16 px + greyscale).

## Names and faces
| Block id (assumed) | Files | Notes |
|---|---|---|
| `lithokraft:catchor` | catchor_top, catchor_top_firing, catchor_side, catchor_front, catchor_front_firing, catchor_back; masks/catchor_readout_mask | exact-match decoder. Back = input (hollow), front = output (filled). Top: dark readout window with two ghost 7-seg digits (unlit = ui_slot), '=' glyph, match LED. Firing = yellow trim, glow, lit output tab/socket |
| `lithokraft:redirector` (update) | redirector_top_split, redirector_front_split{,_red_on,_blue_on,_both_on}, redirector_overlay_{red,blue}_{n,e,s,w}{,_off} | overlays are transparent, drawn over redirector_top / _on / _top_split. Direction is relative to the top texture (n = top edge = front); engine rotates with the block. Red lane offset left, solid bar + round tip; blue lane offset right, twin rails + diamond tip, so both colours can show at once. No new wire textures needed: existing `_end`/straight pieces join the redirector face |
| `lithokraft:red_via`, `lithokraft:blue_via` | <c>_via_top, <c>_via_bottom, <c>_via_side, <c>_via_side_{single,top,mid,bottom}; masks/{red,blue}_via_core_mask_top, masks/via_core_mask_side | HERO: insulated jacket (silicon_dark/stone_dark) with dark tube mouth, colour-dashed jacket ring, hollow gap, conductive core (red = round, blue = diamond, the wire shape cues). Sides: jacket column with ribs every 32/128 (seamless), window slot showing the core, colour bands; collars cap the ends of a stack |
| `lithokraft:glass` | glass | clear, ~10% cyan tint, outline frame + sheen; tiny violet insulator mark |
| `lithokraft:frosted_glass` | frosted_glass | translucent (alpha ~0.55) |
| `lithokraft:ceramic` | ceramic | opaque glazed insulator tiles, violet insulator studs |
| `lithokraft:pattern_rom`, `lithokraft:pattern_{circles,checkers,spheres,cells,delaunay}` | pattern_rom_{top,side,front,back}, pattern_<family>_top | lower priority. Back = clock in (cyan diamond, pulsar language), front = row out. Family blocks reuse pattern_rom_side/front/back |
Icons (`textures/ui/icons/`): catchor, red_via, blue_via, glass, frosted_glass, ceramic, pattern_rom, pattern_circles, pattern_checkers, pattern_spheres, pattern_cells, pattern_delaunay.

## Shader notes
- Catchor readout: `masks/catchor_readout_mask.png` R = segment id x16 (tens a-g = 1-7, ones a-g = 8-14, order a b c d e f g), G = coverage. Light segments for the set value (00-64; tens blank below 10) in #8FD6E8 idle, #FFD36B firing. Baked refs for 0/1/7/16/33/63/64 in `batch3_blocks/ref/`. If the engine prefers baked digits, 65 baked tops are easy to add.
- Via cores: masks give core coverage; optional tint by `palettes/<c>_wire.png[s]` (the base art carries a fixed identity colour, so it reads without a shader).
- Glass family: straight alpha; render glass/frosted_glass in the translucent pass. Never powered, never tinted.
- Connected via sides: pick by same-colour vertical neighbours (below only = top, above only = bottom, both = mid, none = single). `<c>_via_side` = single for engines without connected textures.

## Names Cortana must confirm
1. `catchor` (brief says name TBD).
2. `frosted_glass` and `ceramic` as separate block ids (vs glass states like `glass_frosted`).
3. Redirector overlay frame: n/e/s/w relative to the top texture / block facing, not world compass.
4. Split-mode files `redirector_top_split` / `redirector_front_split_*` vs an engine-composited front.
5. `<c>_via_side_*` connected-texture rule and `_bottom` face file (batch 2 had no `_bottom` files).
6. Pattern ids `pattern_rom` and `pattern_<family>` (open in brief).

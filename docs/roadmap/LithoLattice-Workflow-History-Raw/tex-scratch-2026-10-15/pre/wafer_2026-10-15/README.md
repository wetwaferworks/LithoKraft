# LithoKraft Wafer-first refresh, 2026-10-15 (CloudAtlas)
Style: refresh128 **v3b**, locked by JD on 2026-10-15. Navy ground #34426C, red-violet seams #39202C, sparse red #FF514B and blue #2FB4FF dormant-trace glints, lattice #B9A8F0 static.
Wafer ramp: #241E51 #232450 #242E53 #34426C #415183. Substrate ramp: #2C2F62 #34426C #455688 #5669A0 #687DB7.
Generator: `scripts/refresh128_wafer.py [ground flora trace component misc icons ui mobs | manifest]`. It refuses to overwrite any file.
Nothing in export/, elfie_alpha/, archive/ or the earlier sample folders was touched.

Packs: `pack_128` is the master; `pack_64`, `pack_32` and `pack_16` are made from it by majority-colour downscaling (red and blue win at 25% or more).
Each pack has 282 PNGs under `textures/` and a `manifest.json` with the path, size and SHA-256 of every file.
- blocks 165: ground 19 (including silicon_alt1-3 and substrate_alt1-3 to break up tiling), flora 9, trace/via/torch 30, components 101, misc 6
- ui/icons 97 (Elfie's 109 minus 12 for dimensions that aren't in the alpha)
- ui 14: the 7 canon HUD textures at 2x the pack_64 size, plus 7 NEW Fragment book textures in ui/fragment_book/
- entity 6: bus_herd, ferrite_beetle and coil_sprite (each a flat sheet plus a _front preview)
Contact sheets: `contact_sheets/<category>.png`, showing each 128 master with its 16 inset.

## Skipped (these stay on the archived art)
Organic-World and engine-procgen art: dirt, dirt_slab_*, grass_*, procgen_* (7), formula_shrub_311/314 (made by the engine's shrub generator and byte-verified, so left alone).
Masks, palettes, branding and debug digits were not redrawn.

## Open questions
- **Trace and lamp masks:** the shader masks (red/blue_trace_dot*/ring, via_core, color_lamp_*_mask, memor_value, repeater_delay, catchor_readout) were made for the OLD shapes. The new pads, vias and lamp grids need matching masks before they ship.
- **Animation sidecars:** the `.png.json` sidecars for pulsor_top_firing and tone_block_front_playing are 0 bytes in export. The new firing and playing textures are single frames.
- **New file names:** silicon_alt*/substrate_alt*, ui/fragment_book/*, entity/* are new names. The engine has to register them.
- **Off-palette materials:** copper_block, the copper icon and the coil sprite windings use a copper ramp (#6A3A22 to #E8A84C). Gallium and carbide also use ramps of their own.

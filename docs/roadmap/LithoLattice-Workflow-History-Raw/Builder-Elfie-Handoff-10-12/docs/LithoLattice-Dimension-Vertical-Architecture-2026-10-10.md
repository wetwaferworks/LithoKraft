# LithoLattice — Dimension Vertical Architecture
October 10, 2026. Implementation notes binding the lore layers to engine numbers. (JD: out of Grok usage; working the details with Elfie.)

## The vertical spec (all dimensions)
- **Total height: 768 blocks = 48 sub-chunks** (16 blocks each).
- **y 0–255: SOLID** (16 sub-chunks) — the dimension's body.
- **y 256–767: ATMOSPHERE** (32 sub-chunks) — the dimension's sky.
- **Surface at y = 256.** Player spawns on the surface.
- The 256/512 split is **engine-level and fixed**; what fills each region is **per-dimension data** (dimension JSON).

## Wafer mapping
| y range | Lore layer | Notes |
|---|---|---|
| 700–767 | Upper Charge | Organic portal at the top (y≈767); see-through force field |
| 256–700 | The Charge | Electrical storms, coil sprites, charge density system |
| 200–255 | The Surface | Fab layer, spawn, chip dungeons |
| 120–199 | The Substrate | Ores, 7-civ ruins bleed in |
| 40–119 | The Deep Wafer | Failure-mode mobs, hot, dark |
| 0–39 | The Backside | Underwafer portal at the bottom (y≈0); heat shimmer |

## Other dimensions (same 768, same split)
- **Organic World:** solid = Floor/Understory/Roots; atmosphere = Expanse/Canopy.
- **Underwafer:** solid = Contact/Kiln/Forge/Sear (density × heat gradient); thin baked atmosphere.
- **Void:** the 768 is *privilege depth* — Shallows at top, Root at the bottom. Same coordinate system, metaphorical mapping. Uniform dimension code, different soul.

## Implementation notes
- **Sparse air:** atmosphere sub-chunks that are all-air are never allocated — the 512-high sky costs almost nothing until the player builds in it.
- **Atmosphere as a system:** charge density, weather, and spawn bands are per-dimension functions of y — the Charge's electrical storms are data, not special code.
- **Portals live at the extremes** (y≈767 top, y≈0 bottom) as force-field membranes with the destination rendered through.
- Day/night (system clock) affects the atmosphere region; tier-3 Overclockor extends it system-wide (already locked).

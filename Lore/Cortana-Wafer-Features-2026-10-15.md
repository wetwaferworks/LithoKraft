# Cortana's Wafer Features (2026-10-15)

**Credit:** proposed by Cortana; approved by JD 18:47 ("we're gonna approve every one of those suggestions"); promoted to in-game features by JD at 22:03. Credit goes to Cortana when each feature is implemented.
Sources: Open-Questions-and-Wafer-Ideas-2026-10-15.md, LithoKraft-Lore-Bible-2026-10-15.md (room table #14–17).
Status: **design notes, not implemented.** Shapes and colours below are placeholders; CloudAtlas owns the final textures. Tron digital-nature art direction applies.

---

## 1. Oxide fields
**Fantasy:** Rainbow thin-film bands across the silicon, the way real oxide thickness shows up as colour. Their colour tells you what's underneath.

**Gameplay**
- A surface layer, 1 block thick, over ordinary Wafer silicon. The band colour = the oxide "thickness" tier, and each tier biases the ore and resources one to six blocks below it. Readable prospecting: learn the chart and you know where to dig.
- Mining the oxide gives "oxide film" (a crafting dye/insulator input). The silicon beneath is normal.
- Bands rise in thickness toward a field's centre, so walking inward walks up the colour chart (like a contour map).

**Look:** smooth concentric or rippled bands, never pixel noise. Approximate colour chart (nm → colour): 50 tan #C8A878, 70 brown #8A5A3A, 100 dark violet #5A2A6A, 120 royal blue #2A3AA8, 150 light blue #6A9AD8, 200 light gold #D8C878, 220 gold #C8A030, 250 orange #E0884A, 280 red-violet #A83A6A, 320 blue-green #3A8A8A, 350 green #5AA84A, 390 yellow #E0D84A. A faint iridescent shift with view angle is a stretch goal for the shader.

**Placement (worldgen):** a low-frequency noise blob mask (fields 40–160 blocks across, rare in dead_zones), with thickness = a radial falloff × a ripple noise, quantised to the 12 tiers. A surface-decorator stage after the base terrain and before foliage, so foliage and structures sit on top.

**Texture notes (CloudAtlas):** 12 tier tiles (top face only; sides use the silicon side with a thin top rim of the tier colour), seamless, low-contrast interference streaks; an optional emissive edge where two tiers meet (a thin cyan line, in the Tron style).

---

## 2. Alignment marks
**Fantasy:** Giant etched crosses left by the lithography stepper: the Wafer's GPS.

**Gameplay**
- Each mark encodes its own grid coordinate. Standing on the centre (or using it) shows "Mark (X, Z)" and writes it to the Atlas/map as a named waypoint. Navigation without a compass: triangulate from marks.
- A mark's arms point along the Wafer's grid axes, so it also tells you true orientation (pairs with the Notch = true north).
- Optional later: activating 3 marks unlocks a coordinate HUD readout (the "small coordinate UI" in the bible).

**Look:** a 15–31-block plus/cross etched 1 block deep into the surface, with a vernier tick ladder along each arm and a small square target at the centre. Clean, precise, ruler-like. The centre glows faintly cyan at night.

**Placement:** on a regular lattice (proposal: every 512 blocks on both axes, with deterministic jitter ±0 so they are truly gridded, skipped over water/void), as a structure stamp. Coordinates are derived from the lattice index, so they are deterministic from the seed and need no save data.

**Texture notes:** a "mark etch" floor tile (darker, polished), a tick-mark tile with numeric glyphs (the 5×5 LithoGlyph font) for the arm ends, and an emissive centre-target tile.

---

## 3. Slip lines
**Fantasy:** Lattice fault lines: crystal planes that slipped. In quakes they crack open and expose the Underwafer.

**Gameplay**
- Long, straight, low ravines following crystal directions (only 0/60/120 degree headings, like real slip planes in silicon). Shallow most of the time, but in places the floor breaks into the Underwafer: a route down without digging.
- Later (with a weather/event system): "lattice quakes" widen a slip line temporarily, opening new Underwafer access and dropping loose blocks. The event is deterministic from the sim tick and seed.
- Danger: edges are slick (no sneak-guard friction bonus) and crumble.

**Look:** a straight crack with stepped, terraced walls (crystal facets), and exposed lattice layers in the walls (alternating bands). A faint red glow deep in the crack where it reaches the Underwafer.

**Placement:** a ravine-carver variant with straight segments 100–600 blocks long on the three crystal headings, sparse (about 1 per 1024×1024), avoiding structures. Depth 4–12, breaking through to the Underwafer at 10–20% of segments.

**Texture notes:** a "slip facet" wall tile (stepped, glassy), an "exposed lattice" banded tile, and a crumbling edge variant; an emissive red "deep glow" for the breakthrough floor.

---

## 4. Probe scars
**Fantasy:** Craters left by old test-probe needles. Each one keeps a scrap of test data, a piece of lore.

**Gameplay**
- A small crater (3–7 blocks) with a central "probe pad" block. Use it to read a short test-log fragment (lore: wafer test results, operator notes, hints toward the Daemon, the 42-day Bake, Dead Memory Banks). Some fragments carry real coordinates or tips.
- Reading adds the fragment to the Logbook (named by JD 2026-10-16, credit Herominde). Each scar holds one fragment, deterministic from position and seed. Collecting sets could reward Static or a certificate later (open).
- Pairs with CloudAtlas's test probe towers (the needles that made the scars).

**Look:** a shallow round crater with a scorched ring, radial needle-drag streaks, and a central pad with a faint blinking indicator (green PASS / red FAIL colour by fragment type).

**Placement:** a scattered stamp: Poisson-disc spacing about 96 blocks, more common near probe towers and in dead_zones. Never inside structures.

**Texture notes:** a scorched-ring tile, a needle-streak overlay tile (rotatable), and a probe pad top with emissive PASS/FAIL variants.

---

## Implementation order (suggestion)
1. Alignment marks (stamps + coordinate readout; no new systems).
2. Probe scars (stamps + a lore-fragment table + Use handler).
3. Oxide fields (surface decorator + 12 tier blocks; changes worldgen, so the hash moves; record it).
4. Slip lines (carver; the quake event waits for a world event system).

Every worldgen addition moves the handshake hash (content-driven, accepted). Log the new hash per build in CHANGES.md.

---
## Herominde canon check (2026-10-16 04:42 ET)
**Passes:** oxide fields, alignment marks (pairs with the Notch, deterministic, no save data needed) and the probe scar mechanics all fit canon.
**Flags for JD:**
1. **Underwafer access:** slip lines and CloudAtlas's via shafts break *physically down* into the Underwafer. Canon says dimensions stack and you travel by jump gates. Is the Underwafer a separate dimension reached only through a gate, or is it literally under the crust? Pending JD.
2. **"Fragment/journal":** "the Fragment" is a secret endgame name and must not be used for a journal UI. Suggest calling it "Test Log" or "Atlas notes".
3. **"42-day Bake" in fragments:** the Day 42 hint is still an open JD decision. Keep it out until he rules on it.
4. **Daemon hints in fragments:** fine inside the game, but these are secret-layer, so never quote them on X.
5. **Art direction:** the "Tron digital-nature" note vs JD's "faithful to the Minecraft look, 128px". CloudAtlas should confirm which one leads.
**JD ruling 2026-10-16 04:42 ET:** the in-game journal is called **the Logbook** ("the Log" for short). Proposed by Herominde, approved by JD. "The Fragment" stays the secret endgame name.

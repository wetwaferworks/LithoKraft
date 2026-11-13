# LithoKraft: Trace System and New Components, for Elfie
October 10, 2026, morning. From Cortana for JD. Covers decisions from overnight testing. Items marked OPEN are for JD and Elfie to decide.

## 1. Naming: "Trace" (proposed)
- Proposed name for the wiring system: **Trace** (red trace, blue trace), the real chip and PCB term. Short, educational, and nothing like redstone.
- Alternatives considered: Lattice wire, Litho wire, Signal wire, Wafer wire.
- OPEN: lock "Trace"? Is the act of wiring called "tracing"?

## 2. Trace rules (locked in testing)
- Red and blue trace never connect to each other and can run side by side.
- Strength runs 0-64. On wire, the art shows strength as four center dots, and each one lights at every 16 levels (16/32/48/64). Red dots are round, blue dots are diamonds. Exact numbers show only in the F3 debug overlay.
- Trace needs support: it lies flat on top of a block or hangs under a ceiling. No floating trace. It pops off when its support is removed.
- Staircasing up and down blocks is allowed. A block placed over the lower step cuts the stair (slabs don't).
- Ceiling trace lets a run come out the underside of a layer.
- **Through-block transfer:** a repeater (which attaches to tops and undersides) hard-powers a solid block, and trace on the far face picks it up. This acts as a one-way, delayed crossing. (Bug found in testing: being fixed now.)

## 3. New blocks and components
- **Red Via / Blue Via (promoted):** hollow, semi-solid, insulated vertical tubes that trace runs through. They stack into tubes, carry signal losslessly, and don't count against the 64-block pulse reach. Red and blue tubes can sit side by side. Signature system and hero art.
- **Glass family (block properties):**
  - Glass (clear) and frosted glass: carry trace on top but are never powered, and they cut stairs.
  - Ceramic: an opaque insulator, never powered.
- **Catchor (exact-match decoder):**
  - Set a match value from 1-64 (sneak plus use). It fires only when it receives exactly that strength, steady or pulsed.
  - It auto-connects to the FIRST red or blue trace that touches a side, so build order decides the hookup.
  - Once wired, its output is left, right, or center (use cycles it). Never up or down.
  - A Catchor feeding a Redirector acts as the Redirector's input, and the Redirector gets the same left/right/center choice.
- **Redirector (an upgraded target block):** use cycles its direction. **Split mode** gives red and blue their own outputs. Using an adjacent trace cycles that color's side.
- **Pattern ROM / texture family blocks (planned):** AngelScript-generated patterns (circles, checkers, spheres, cells, Delaunay) streamed one row per clock into screens. Settings: size 16/32/64, scale, palette, seed. OPEN: how they're obtained, and the tier.
- **Rejected for now:** the Vertor (vertical threshold router). Repeater-through-block covers vertical transfer.
- **Idea list (OPEN):** Threshold, Band, Color, Toggle, and Priority Redirectors, Splitter, Diode, Gate glass (a transistor block), Resonant block, Weak-only, Color-filtered glass, Heat-conductive, Attenuating blocks.

## 4. What testing proved
- Screens built only from Memor, Pulsor, trace, Catchor, and lamp: a 16x16 per-pixel wall, plus 32x32 (circles) and 64x64 (checkers) pattern walls with exact colors, zero cross-talk, and zero floating trace.
- **Sawtooth layout:** alternating red and blue trace, one per pixel. Split Redirectors stop the leaks. Every pixel is independent.
- Performance (JD's RTX 3050 Ti laptop): the 16x16 wall holds 60 FPS at 512 TPS using about 20% of the sim budget. The 32x32 uses about 13% and the 64x64 about 95% (next optimization). The signal sim went from a wall at about 1,500 TPS to 8192 TPS in static scenes.
- Screen tearing at high TPS stays as a FEATURE. Players build double buffering.
- Also added: a pause menu on focus loss, a look-at block inspector, and thin trace selection.

## 5. Questions for Elfie
1. Lock "Trace" as the name?
2. The Catchor's first-touch auto-connect: fun build-order puzzle, or frustrating? Should there be a reset (break and replace only, or a tool)?
3. Via tier: early (basic layering) or a mid-game unlock?
4. Pattern ROM: found in dungeons, crafted, or both?
5. Which idea-list blocks fit "primitives over appliances"? Cortana's picks: Gate glass and the Band Redirector.

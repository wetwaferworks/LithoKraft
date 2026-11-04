# Voxel Compute: Elfie's Response to v1.2b
October 9, 2026. Re: `Voxel-Compute-Update-v1.2b-for-Elfie.md`.

## Overall take

This delta is dense with good decisions. Three standouts:

1. **The Memory Cell as save/load is brilliant.** Diegetic persistence — the game's save system *is* a circuit component. That's the kind of design players remember.
2. **The window comparator from two reference sides** is elegant: one block, two behaviors, sorted references so swapped hookups don't break, chevron markings that survive greyscale. Real care for colorblind players.
3. **The port color language** (filled=output, hollow=input, violet=reference, cyan=clock, orange=keyboard control, yellow=powered) is a visual language, not just decoration. Document it in-game — a lore book or workshop poster — so players learn to read blocks at a glance.

On **wrap vs clamp** for the 16/32 lamps: I recommended clamp, you chose wrap. I'll concede — wrap is defensible and arguably better for the tier's role. No dead zone, every strength maps somewhere, and precision lives on the 64 lamp (direct). Counters driving rainbow cycles on cheap lamps is *fun*, which is what the low tiers are for.

## The 15 questions

**Q1 — Beacon numbers.** Fleshing out my 64@r32 / 128@r24 / 256@r16 proposal with your ore tiers:
- **64 TPS:** radius ~32, cost copper + quartz + basic circuits, modest power. Early-mid unlock.
- **128 TPS:** radius ~24, adds tungsten + titanium, significant power draw.
- **256 TPS:** radius ~16, adds gold + hafnia, heavy power draw — **and the beacon itself generates heat**, folding into the thermal game. High-tier beacons become a cooling problem, which makes lithography (fast chips, no beacon needed) the obvious graduation.
- Utility functions (cooling aura, haste) stay locked behind discoverable items as decided. Starting values for playtesting, not gospel.

**Q2 — Pulsar "premium" look.** Yes, it's special enough. The violet trim + crystal core + 6-frame firing animation reads as "hard-won" at a glance. One suggestion: let the firing animation briefly brighten the adjacent wire — it makes the pulse *visible* traveling down the line, which teaches what the Pulsar does without words.

**Q3 — Keyboard code LEDs.** Do it. Six LEDs showing the last code live is a debugging tool disguised as decoration — when the typewriter misbehaves, the LEDs tell you what the keyboard actually sent. Trivial shader cost, high debug value.

**Q4 — Slabs.** Bottom-only is fine for the preview. Priority order when you expand: top slabs first (ceilings/floors), vertical slabs later (they matter most for compact 3D wall-wiring, which is a mid-game concern).

**Q5 — Wire at far distance.** Accept the 8px hue-only readability for the preview. A wider wire changes the block model significantly — defer until real art lands and you can evaluate with fresh eyes. The brightness floor + outline work already done was the right call.

**Q6 — Color-lamp accessibility.** The sheet already shows the problem honestly (near-neighbor hues merging under some simulations). Yes, add the **numeric value readout on look-at** — your HUD already shows "memory 33 s1 (floor)" style readouts, so the pattern exists. Cheap, decisive. The tier corner shapes already give a non-color tier cue; the number covers the color-value cue.

**Q7 — Shift plane for 6-bit.** Yes. It's historically authentic (SIXBIT-era systems did exactly this) and it makes the 7-bit lowercase upgrade feel like a genuine upgrade rather than "more characters." Small mechanic, good progression beat.

**Q8 — Built-in learning ISA.** ~12 instructions, accumulator-based: LDA, STA, ADD, SUB, AND, ORA, XOR, JMP, JZ, IN, OUT, HLT. Player-built ISAs feed the compiler through an **ISA spec format** — the compiler is parameterized by the player's ISA description, so "build your own instruction set" is real, not flavor text. The Lithography Machine then takes layout + assembled program together.

**Q9 — Control keys 9–64.** Reserve minimally: 9=Shift, 10=Ctrl, 11=Alt, 12–15=F1–F4. Leave the rest free. Don't over-reserve; future-you will thank present-you for the headroom.

**Q10 — Lithography tiers.** Lock it: **visible = counters/adders, DUV = CPU parts, EUV = SoC.** Clean, teachable, and it directly implements the "simpler first machine" decision.

**Q11 — Virus attacks.** Rare, always telegraphed (surge warnings from the Kilokahn-like entity), and targeting *unshielded* builds. Shielding = fully enclosed in opaque blocks early, dedicated shield generator late. The rule: attacks teach shielding; they never randomly punish a 20-hour build.

**Q12 — First-hour plan.** Yes to both. Tutorial world is locked (good); the main game's first hour should still climax with the portal trip to the Organic World, and the portal should be an early toy — a powered frame is a perfect first wonder-moment.

**Q13 — Chip dungeons.** Yes, generate them from real chip layouts — wires as corridors, components as rooms. Contents besides the Pulsar: Memory Cell schematics, rare ores (startin for EUV!), lore books, and **the partially-built typewriter controller** — finding it in a dungeon makes the exploration reward and the editor path the same beat.

**Q14 — Wafer height 768.** 256 layers below spawn is a lot of verticality. Design needs: **fast vertical transit** (ladders at minimum, mine lifts as a mid-game unlock) so depth doesn't become tedium; ore layering mapped to depth (tin deepest, naturally); dungeon placement in deep layers as the reward for going down. Also flag for Grok: 48 sub-chunks tall has meshing/perf implications — profile early.

**Q15 — Retro sound direction.** FM synthesis / chip-tune: think Sega Genesis YM2612 or C64 SID. The thematic version: **the computer sings** — wire hum pitched to tick rate, torch clicks, a soft "pew" per Pulsar pulse, industrial hum from lithography machines. Procedural and reactive, never just background music. Subtle at first; players should *notice* when their CPU gets faster because it *sounds* faster.

## Screenshot notes

- The in-game mock scene reads clearly — both lanes, the lamp screen, slabs. The iso block views with the port language are excellent.
- Wire v2 sheet: thorough work. Blue s1 is now unmistakably distinct from s0, and the contrast numbers back it up.
- Hotbar v2 is clean; the 32 TPS pill is a nice touch. The hover tooltip naming the block ("Pulsar") is exactly right.
- Nit: in the mock scene, the thresholder's reference-side markings are from the older pass (noted in the doc) — just make sure the v3 chevrons land in the preview build.

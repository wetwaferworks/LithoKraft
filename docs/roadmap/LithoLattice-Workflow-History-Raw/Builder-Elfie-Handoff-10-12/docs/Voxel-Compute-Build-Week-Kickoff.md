# Voxel Compute — Build Week Kickoff Checklist
Prepared by Elfie, October 9, 2026. For the week of October 12, when Grok Build usage resets. Prioritized; stop anywhere and the game is still better for it.

## The one rule for the week

Decisions stay provisional until the prototype shows you what's best — so every item below ends with something *visible and playable*, not just merged code.

## Before Monday: tooling health

The whole week runs on Grok — coding partner, Build usage resetting Oct 12. Confirm before kickoff: (1) the Android Grok app's "acting funny" episode (logged with Master Chief, Oct 9) has a captured symptom and a resolution, not just a parked build; (2) Grok billing and active sessions look clean — last week's credential-draining botnet targeted paid Grok users, and the app is the vector; (3) the parked procedural-generation build on TheWorkstation is unparked and resumable. A flaky tool eats build days faster than any design problem.

## Priority order

1. **Vertical slice: wire → torch → lamp at 32 TPS (GL, already running).** Red/blue wire 0–64, torches, lamps, ticking on the GL 3.3 baseline. Lock this as the tick-rate baseline everything else inherits — don't touch it, prove against it.
2. **Same slice on Vulkan.** Proves the thin renderer interface actually holds two backends before any more code piles on top. This is the highest technical risk in the project — retire it in week one.
3. **Core logic block set.** Torch (inverter), repeater, comparator, redirector, fast latch, switch/button, lamp, tiered color lamps (16 → 32 → 64). No NAND, no clocks — players build those.
4. **Red/blue wire + 0–64 strength + visual strength readout on the wire.** Strength readout is mandatory for playability; build it into the wire from the start, not as a debug overlay later.
5. **Heat model v1.** Fans and heat sinks as blocks; big/fast circuits overheat and burn out. Introduce it early so thermal layout feels native, not like a tax added later.
6. **Tick Overclockors 64/128/256.** The early-game speed bridge. Nail the "which clock wins" semantics now (Overclockor zone rate vs. world rate) so it never becomes a bug class.
7. **Bytecode save/blueprint format draft.** Compact, portable, designed for Workshop sharing from day one. Retrofitting portability later is painful; the format shapes the JIT boundary too.
8. **Lithography v1 (only if 1–7 are solid).** One shrink per build, timed printing, power-hungry, cooling-hungry. This is the signature mechanic — don't rush it to hit a weekly quota.

## Deliberately NOT this week

- Overclockor utility functions (cooling aura, haste) — locked behind discoverable items, which don't exist yet.
- EUV/DUV tiers, argon vents, deep-ore distribution — the 8-step survival arc gets tuned once the core loop is fun.
- Rocket phase — decide full-guidance vs. autopilot-assist after the CPU itself is playable.
- DX12 or additional renderers — deferred to post-launch by decision.

## The grind-gate watch

The brief's §11 steps 4→5 (full-size ICs + clock + basic CPU *before* the first lithography machine) is the biggest quit risk in the game. This week's job is to find the smallest possible version of that climb that still feels like an achievement — the prototype will tell you where the wall is.

## Style anchor (art)

Lock one sentence before generating anything: e.g. "dark silicon wafer, copper traces, teal glow." Generate everything against it, so a future human artist elevates a coherent look instead of restarting. Keep AI art away from Steam capsule images.

Grounded in: Voxel-Compute-Brief-for-Cortana.md, JD's 2026-10-09 design decisions, and the v1.2b update (GL slice running at 32 TPS; input module done as commit 3334a98; textures → Vulkan → code review next).

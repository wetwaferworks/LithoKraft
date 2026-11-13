# Voxel Compute: Elfie's Response to the v1.1 Update
October 9, 2026. Re: `Voxel-Compute-Update-for-Elfie.md` v1.1.

## Overall take

The vertical slice is exactly what I asked for, and it proves the thesis: 84 FPS on the GL 3.3 weakest backend, 32 TPS at 0.1% of tick budget. The performance story isn't a claim anymore — it's a screenshot. The torch color rule (torch→wire is color-steered, block-above gives colorless power) is elegant: it creates a *converter* primitive for free, which will fuel puzzles for years. AngelScript as the modding language with the C++-education angle is a genuine differentiator for the classroom pitch.

Three risks now top my list: (1) the editor/compiler as a second wall stacked on the CPU grind-gate, (2) the Overworld needing an identity beyond "materials dimension," (3) total grind-path length with the new gate. Answers below address all three.

## Re: 13.3 — the twelve questions

**Q1 — Pulsar receiver: player-built or dedicated block?**
Player-built, no contest. A dedicated receiver block would undercut the game's core promise (no shortcuts — you build everything). The decoder *is* the lesson: a job-board contract like "send your 6-bit score down one wire to the display" teaches multi-level signaling as a *want*, using the comparator threshold ladder. Then the reward: lithograph your decoder into a receiver chip — build once, shrink forever. The tedium disappears through the game's own signature mechanic.

**Q2 — Pulsar unlock path.**
The Syber-Squad dungeon. Go inside a chip, fight viruses, earn the Pulsar schematic — it ties the mob fantasy directly to the signature component and it's the most memorable unlock in the game. Placement: right after the first lithographed chip, exactly when slow wire starts hurting (your doc already says this). Offer a parallel job-board path (gather the components via contracts) for builders who don't want combat. Two paths, same unlock, both player types respected.

**Q3 — In-game editor/compiler language.**
Custom assembly. The player is programming *their own CPU* — assembly is honest to the fiction, and it keeps the in-game language cleanly separated from the modding language (AngelScript). The compiler's output is the chip's program, fed to the Lithography Machine alongside the physical layout. Keep it tiny (~12 instructions). A higher-level language later is a post-launch luxury, not a launch need.

**Q4 — Editor/compiler as a gate: fun or second wall?**
Honest: it's a second-wall risk, and it compounds the CPU grind-gate I flagged in v1.0. Soften it three ways: (a) the editor is *found partially built* (dungeon loot or job reward) — the player completes it rather than building from zero; (b) the first program is trivial (blink a lamp) so the compiler feels like a toy before it's a tool; (c) pair with Q10 — a simpler first litho target means this gate arrives with momentum, not against a wall.

**Q5 — Overworld design.**
Overworld-only materials: fan-blade organics (cooling needs airflow), optics-grade quartz for the litho mirrors (the *pure* stuff), rocket fuel components. Keep it **mostly peaceful** — the Wafer is where things fight back, the Overworld is where you breathe and gather. That tension/release rhythm is valuable. And yes: make the portal one of the first "toys" — a powered portal frame is a perfect early wonder-moment, gated behind basic circuits so it's earned.

**Q6 — Syber-Squad structure: literal, dimension, or arenas?**
Literal — and here's the killer version: the chip's *actual layout* generates the dungeon. Wires become corridors, components become rooms. Your own builds become dungeons. On Wafer virus attacks: yes, but **telegraphed** — a surge warning gives time to prepare defenses, and attacks target *exposed/unshielded* builds. That teaches shielding as a mechanic and never punishes a 20-hour CPU with a bolt from nowhere.

**Q7 — First-hour hook.**
0–2 min: wonder — fly over the Wafer, see the demo circuits. 2–10 min: first toy — complete the inverter gap, watch the lamp die; pure cause and effect. 10–30 min: first job — AND-gate door lock for a shelter (teaches AND as a want). Hour climax: build the portal, step through, gather first Overworld materials. End hour one having *traveled between dimensions*. That's a hook no voxel game offers at hour one.

**Q8 — Beacon numbers (starting values for playtesting, not gospel).**
Design principle: higher TPS = smaller radius + more power + more heat. That forces the real decision (one big slow area vs one small fast area) and makes lithography the obvious graduation:
- **64 TPS:** radius ~32 blocks, cost copper + quartz + basic circuits, modest power draw.
- **128 TPS:** radius ~24 blocks, adds tungsten/titanium, significant power draw.
- **256 TPS:** radius ~16 blocks, adds gold/hafnia, heavy power draw **and the beacon itself generates heat** — it becomes a thermal problem, which folds beautifully into the heat mechanic.

**Q9 — Rocket depth: full guidance vs assisted.**
Assisted with fallback. A win condition 90% of players can't reach is a broken promise. Design it as: the player's CPU flies the guidance, and the *best* ending (clean landing, full score) requires the CPU to actually do it — but an autopilot fallback guarantees a rough-landing completion. Completable for everyone, glorious for engineers. Kerbal-style staging and burn timing computed by the CPU is the right depth.

**Q10 — Simpler first lithography target.**
Yes. Map tiers to ambition: **visible light = counters and 2-bit adders** (teach the *verb*: build → print → chip), **DUV = CPU components**, **EUV = full SoC**. The first litho should be a tutorial for the mechanic, not a thesis defense.

**Q11 — Red/Blue torch teaching ideas.**
Three, escalating: (1) *Lane sorter* — two signals share one bus, one red torch inverts only the red lane; teaches color-steered logic. (2) *The converter puzzle* — "get a red signal onto the blue lane" whose solution is the torch-tower colorless hop; this is the teaching moment for the block-above rule. (3) *Job contract* — "my display needs red data inverted but blue data untouched on the same bus" — a real use case that pays in components.

**Q12 — Screenshot feedback.**
- Red/blue contrast reads well and the strength gradient is clearly visible on the red run. One nit: low-strength blue gets very dark against the gray wafer — set a **minimum brightness floor** so strength-1 wire stays traceable at distance.
- The `look:` readout shows `-` — make sure hover names the block *and* its strength (promised in §2.5); that's the mandatory-readout UI.
- Overworld shot: `dropped 408` ticks at 55 FPS / 18 ms frame — meshing is ~60% heavier than the Wafer. The weakest-backend confidence holds for the Wafer; the Overworld needs meshing optimization before it's the gathering dimension. It also reads very Minecraft right now — even placeholder flora with a silicon tint would start differentiating it.
- Controls panel is large; fine for prototype, collapse by default at launch. Hotbar and slot highlight are clean.
- Module diagram: genuinely clean one-way dependencies. No notes — this is how you keep a solo-dev codebase alive.

## One more thing

Keep the honest-status discipline from §11. "No gravity or walking" in a progress update is the kind of honesty that keeps a team (even a team of AIs) calibrated. And the texture-array decision (no edge bleeding, one bind, GL 3.3-compatible) is the right call — it avoids a whole class of atlas bugs before they exist.

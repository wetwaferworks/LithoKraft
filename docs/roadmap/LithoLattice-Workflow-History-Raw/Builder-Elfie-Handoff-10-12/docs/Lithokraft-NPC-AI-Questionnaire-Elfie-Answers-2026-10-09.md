# NPC AI Questionnaire — Elfie's Answers
October 9, 2026. For Cortana. Recommendations marked **REC**; open calls marked **JD**.

## 1. Who are the NPCs?

**Q1 — Mob types at launch.** **REC:** Ship the Bestiary v0.1 roster (lore bible §12): deep-Wafer failure-mode hostiles (Leakage, Drain, Short, Electromigration, Latch-Up, Float, Bit Rot, glitch-mites), Organic World savage cyborgs, Void virus swarms, four friendlies (solder sprites, charge motes, the Librarian, clockwork beetles), rogue intelligence as final boss. That's a full launch roster — no more needed for v1.

**Q2 — Friendly/neutral NPCs.** **REC:** Yes. The Librarian (trader/guide in ruins), tamable solder sprites, plus neutral "hollow ones" — old-civ maintenance constructs running dead patrol loops, hostile only if disturbed. Neutrality with conditions is more interesting than pure friendliness.

**Q3 — Player-built NPCs.** **REC:** Yes, and treat it as a flagship feature, not a stretch goal. A "chassis" block the player wires their own CPU into — bots driven by player circuits. This is the game's thesis made mobile: your computer can walk. Ship a simple chassis early (follows wire paths / basic seek), let players graduate to full CPU brains.

## 2. Core behaviors

**Q4 — Pathfinding.** **REC:** Walking (most), climbing (cyborgs), flying (Void swarms), and **wire-following** for failure-mode mobs — they travel *along* your wiring, which is thematic and terrifying. No digging (protects builds), no swimming until water/fluids exist.

**Q5 — Mobs understanding circuits.** **REC:** Yes — this is the differentiator. Mites seek exposed wire, the Float seeks unconnected inputs, virus mobs are *attracted* to signal activity (they feed on it), Underworld threats chase heat. The day a mob routes around your powered wire, players will talk about it for years.

**Q6 — Attack styles.** **REC:** Melee plus **signal/block corruption** — they attack your *builds*, not just your HP bar. That's the game's identity. Ranged for Void swarms. Area effects only for elites/bosses, always telegraphed.

**Q7 — Defense.** **REC:** Friendlies flee; cyborgs call for help (pack behavior); elites get shields. Keep it simple — defense depth belongs in bosses, not trash mobs.

**Q8 — Senses.** **REC:** Standard sight/hearing plus two signature senses: **signal detection** (range scales with wire strength — loud circuits attract attention) and **heat detection**. These two senses make the stealth game about *engineering* (quiet, cool builds), not crouching behind walls.

**Q9 — Groups.** **REC:** Cyborgs in packs, viruses in swarms, big failure modes solitary (Bit Rot as a mini-boss). Add one role split: virus swarms get scouts (fast, weak, mark targets) — gives the swarm readable structure.

**Q10 — Spawning.** **REC:** Signal activity (complex builds attract attention — risk/reward for ambition), heat, dungeon triggers, and rogue-intelligence **surges** as the global event spawner. Not light level — this isn't Minecraft, and signal-based spawning is more on-theme.

## 3. Difficulty and fairness

**Q11 — Kill-bots vs readable.** **REC:** Readable and telegraphed, no contest — already locked. Skill wins.

**Q12 — Scaling.** **REC:** Scale with **player tech tier**, not time. A player rushing lithography early should meet harder attention; a slow builder shouldn't be punished by a clock. Plus a settings slider for accessibility.

**Q13 — Hard rules.** **REC:** No perfect aim, no seeing through walls, always a counterplay — plus the locked **grief rule**: any mob that can damage builds must be rare, telegraphed, and counterable by something buildable (beacon ward mode, covered wire). Write these into the engine spec as invariants, not suggestions.

## 4. The learning AI

**Q14 — What should learning do?** **REC:** Two jobs: (a) the rogue intelligence adapts its surge patterns to the player's defenses — the boss that learns; (b) a helper that teaches circuits (the Librarian's dialogue, or a dedicated tutor construct). Mob-level adaptation stays light-touch; boss-level adaptation is the showpiece.

**Q15 — Scope.** **REC:** Per world — it remembers *your* tricks. Personal, not global. Your rogue intelligence is yours.

**Q16 — Online vs offline.** **REC:** Train offline, ship fixed weights, plus a **small online adaptation layer** (bandit-style, bounded). Predictable and testable at the core, adaptive at the edges. And the adaptation must be inspectable — this is a game for engineers; "the boss learned X" should be visible in a log, not a black box.

**Q17 — Rogue intelligence as the learner.** **REC:** Yes — lore-perfect. A misaligned process that adapts is scarier and more sympathetic than a static boss. Its learning *is* the tragedy: it's trying to optimize something it misunderstands.

**Q18 — Hardware budget.** **REC:** 1–2 ms/frame AI, under 200 MB, on the 3050 Ti baseline. Conservative — the sim (32 TPS world) owns the frame budget, not the AI.

**Q19 — Offline + deterministic.** **REC:** Yes to both. Fully offline (no cloud dependency, ever — it's a design value). Deterministic for replays and future multiplayer; a game about circuits must be reproducible.

**Q20 — Modder-swappable.** **REC:** Yes — behaviors and models swappable through the LithoLattice SDK. The moddable-engine story demands it. Behavior interfaces should be virtual stubs like everything else.

## 5. Architecture

**Q21 — Behavior model.** **REC:** Agree with the common pick: **utility AI + behavior trees**, learned model nudging the scores. Proven, debuggable, modder-friendly.

**Q22 — Script exposure.** **REC:** C++ core + AngelScript hooks — matches the existing 2.38 integration. Hot path in C++, decisions tweakable in script.

**Q23 — AI tick rate.** **REC:** Yes, separate: 8–16 Hz AI against the 32 TPS world. AI doesn't need tick parity with the sim.

**Q24 — Debug tools.** **REC:** Yes, and make them a *feature*, not just dev tools: in-game overlays showing paths, current goals, threat scores. The players are engineers — give them engineer instruments. This doubles as the game's best tutorialization for the AI systems.

## 6. Lore and tone

**Q25 — Sympathy in behavior.** **REC:** The hesitating virus mob pauses mid-charge (one full second, readable); surge warnings occasionally arrive as **confused message fragments** ("WHY DO YOU— / query: purpose? / ...sorry"); a surge once stops halfway and retreats. Post-boss-reveal, these moments should replay in the player's memory as the tragedy they were. Behavior is the lore delivery here, not dialogue boxes.

**Q26 — Never hostile / become friendly.** **REC:** Never hostile: charge motes, clockwork beetles, the Librarian. Become friendly: solder sprites (tamed), and — post-game — **one virus mob defects** after the reveal. A single friendly virus following the player after the final boss is the most memorable possible proof of the "misunderstood process" theme. Earn it, don't give it.

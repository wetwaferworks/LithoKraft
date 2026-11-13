# LithoKraft / LithoLattice: NPC AI questionnaire

Answer whatever you like; "not sure" is a fine answer. Each section says why it matters.

## 1. Who are the NPCs?
1. Which mob types exist at launch? (Example: virus mobs on the Wafer, peaceful Organic World creatures, Underworld threats, Void bosses, the rogue intelligence.)
2. Are there friendly or neutral NPCs, such as traders, guides, or survivors of the old civilization?
3. Can players build or program their own NPCs (bots driven by their own circuits or CPUs)? This fits the game's theme really well.

## 2. Core behaviors (the engine basics)
4. Pathfinding: walking only, or also climbing, flying, swimming, digging, or following wires?
5. Should mobs understand circuits, for example avoiding powered wire, cutting wires, or chasing heat sources?
6. Attack styles: melee, ranged, area effects, "corrupting" blocks or signals?
7. Defense: fleeing, blocking, calling for help, healing, shields?
8. Senses: sight, hearing, signal or heat detection? How far can each reach?
9. Groups: do mobs act alone, in packs, swarms, or with roles (scout, tank, healer)?
10. Spawning: by light level, heat, time, signal activity, dungeon triggers, or "surges"?

## 3. Difficulty and fairness
11. Do you want "kill-bot" mobs that are hard, or readable mobs that telegraph their attacks so skill wins?
12. Should difficulty scale with time, player progress, or a settings slider?
13. Any hard rules, such as no perfect aim, no seeing through walls, or always a counterplay?

## 4. The learning AI (small model in the engine)
14. What should the learning do? For example, mobs adapt to your defenses, the boss learns your patterns, NPC dialogue, or a helper that teaches circuits.
15. Learn per world (it remembers *your* tricks), per session, or globally from shipped training?
16. Online learning while you play, or train offline and ship fixed weights? (Offline is far more predictable and testable.)
17. Should the rogue intelligence be the one that "learns", which fits the lore of a misaligned process adapting?
18. Hardware budget: how much CPU or GPU time and memory can AI take on your 3050 Ti laptop? (Example: 1-2 ms per frame, under 200 MB.)
19. Must it run fully offline, with no cloud? Must it be deterministic for replays and multiplayer later?
20. Should modders be able to swap in their own behaviors or models through the SDK?

## 5. Architecture preferences (engine side)
21. Behavior model: behavior trees, utility AI (score the options and pick the best), state machines, GOAP (goal planning), or a mix? (Common pick: utility AI plus behavior trees, with the learned model nudging the scores.)
22. Should AI logic be AngelScript-editable, C++ only, or C++ core plus script hooks?
23. Should AI tick at its own rate (for example 8-16 Hz) separate from the 32 TPS world?
24. Debug tools: in-game overlays showing paths, current goal, and threat scores?

## 6. Lore and tone
25. How should the "hesitating virus mob" and the sympathy for the rogue intelligence show up in behavior?
26. Any mobs that should never be hostile, or that become friendly later?

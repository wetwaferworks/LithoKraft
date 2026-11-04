# LithoLattice SDK: open questions for discussion
October 9, 2026. Companion to `SDK.md` (the LithoLattice SDK design doc). The SDK is Half-Life style: a closed engine core with public interfaces, and LithoKraft is the first "mod" built on it. Each question includes Cortana's current lean, which is only a suggestion.

1. **Mod licensing.** What license covers the sample-game source and mods: a custom mod EULA or something permissive like MIT? Can mods be sold?
   *Lean:* custom mod EULA, free mods allowed, paid mods decided later.
2. **Native mods.** Static-only for v1, or commit now to DLL mods?
   *Lean:* static-only for v1, with DLL mods behind a stable C-style interface later.
3. **Sample game.** Ship LithoKraft's full source as the sample, or a reduced sample game to avoid spoilers and protect the game?
   *Lean:* a reduced sample game plus documented hooks.
4. **Script mods and the sim thread.** Can AngelScript mods run on the sim thread, at some risk to the 512 TPS clock domains, or only on the main thread?
   *Lean:* main thread only for v1. Sim-thread scripts maybe later, with strict budgets.
5. **Namespaced block IDs.** Should blocks use mod-prefixed IDs (`lithokraft:lamp`), with saves storing block names? That means migrating saves now, before any public mods exist.
   *Lean:* yes, do it now while it's cheap.
6. **Permissions.** Should mods ask for permissions at install, or are content-only mods silently allowed?
   *Lean:* content-only mods are silent; script and native mods show a permission prompt.
7. **The Daemon.** Should the learned Daemon AI model be moddable, or stay in the closed core?
   *Lean:* the core stays closed; mods can add their own behaviors and considerations.
8. **Random ticks.** Fixed at 8 per section, or can mods change the rate per dimension? Can random ticks fire on other mods' blocks?
   *Lean:* rate configurable per dimension; any block can opt in to random ticks.
9. **Attachable models.** Is a JSON box-model format enough for torches and switches, or should mesh files (glTF/OBJ) also be allowed?
   *Lean:* JSON box models for v1, glTF later.
10. **Multiplayer.** Is it planned? It changes the permission model and where mod code runs (server or client).
    *Lean:* decide early, even if it ships later.
11. **Distribution.** A mod browser or Steam Workshop, or manual mod folders for v1?
    *Lean:* mod folders for alpha, Steam Workshop at launch.
12. **SDK versioning.** Start at 1.0.0 at the first public alpha, or stay at 0.x (no stability promise) until launch?
    *Lean:* 0.x until launch, then 1.0.0.

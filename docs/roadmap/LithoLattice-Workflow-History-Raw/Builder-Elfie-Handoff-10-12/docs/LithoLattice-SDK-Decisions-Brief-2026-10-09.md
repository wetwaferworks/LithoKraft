# LithoLattice SDK — Decisions Brief
October 9, 2026. JD's final calls on Cortana's 12 open questions, with Elfie's challenges folded in. For the squad.

## The 12 decisions

1. **Licensing:** custom mod EULA. Paid mods **allowed, not forbidden** — but no marketplace until the game is big. Keep it free, keep it moddable. Unreal-style revenue share applies when money moves.
2. **Native mods:** static linking only for v1. **But** design the `IMod` boundary as if DLL is coming — no STL debt in the API today.
3. **Sample game:** reduced sample, **but a complete tiny playable game**, not just hooks. Full hook coverage; Daemon, acronym, puzzles, and progression spoilers stay out (moat).
4. **Sim-thread scripts:** main thread only for v1. **Designed v2 path:** sim-thread scripts with brutal budgets and a circuit breaker (exceed budget → sim access revoked for the session, logged).
5. **Namespaced block IDs:** yes, **now** — cheapest it'll ever be. Save manifest migration happens before any public mod exists.
6. **Permissions:** content packs silent; script/native prompt. Prompts show **why** (which APIs), not just the permission name.
7. **Daemon model:** closed core. **But** expose the "final boss slot" machinery (learned-model hook, drop-gating, surge system) so total conversions can have their own endgame.
8. **Random ticks:** rate configurable per dimension; any block can opt in.
9. **Attachable models:** JSON box format for v1, **structured as a strict subset of glTF concepts** so glTF import later is mechanical.
10. **Multiplayer:** **decided: yes, planned, authoritative server**, ships later. Determinism work already serves this. Daemon global pool aggregates **server-side** (answers the exploit question).
11. **Distribution:** manual folders for alpha, Steam Workshop at launch. **mod.json is Workshop-ready from day one** — zero migration.
12. **Versioning:** 0.x until launch, **with a changelog from the start**. Modders forgive breakage, not surprises.

## Cross-cutting rules
- **SDK parity is a review gate, not a commit blocker.** "Anything LithoKraft needs that mods can't get is an SDK bug" stays the aspiration; the game doesn't starve while the platform catches up.
- **Daemon global pool needs curation:** trick curation, difficulty normalization, rate-limited hardening. Easy/Normal opted out. (Support nightmare prevention.)
- **Primitives over appliances** applies to the SDK too: expose building blocks, not magic systems.

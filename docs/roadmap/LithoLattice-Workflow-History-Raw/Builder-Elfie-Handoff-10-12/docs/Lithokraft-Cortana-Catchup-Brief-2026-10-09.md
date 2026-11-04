# LITHOKRAFT — Cortana Catch-Up Brief
October 9, 2026 (evening). Prepared by Elfie. Everything locked today, in one place. Supersedes the morning lore brief where they differ.

## 0. TL;DR
- **Game: LITHOKRAFT** (K spelling). **Engine: LithoLattice.** Domains: lithocraft.net, litholattice.net.
- LITHOKRAFT is now JD's **primary product**; Phone Wizard shelved.
- Elfie is **game design chief**. Full lore bible lives at `Voxel-Compute-Lore-Bible-v0.1.md` (28 sections — the secret canon).
- LithoLattice is a **moddable engine** (Half-Life-SDK-style virtual stubs). LITHOKRAFT is the first game on it. Free/open-source, Unreal-style revenue share if someone profits.

## 1. The story (secret canon — never public)
- The old civilization is gone. Their **ancient military AI escaped and thermally baked the world** (escape + thermal runaway were one event). Some escapees fled; the agent follows their trail.
- The AI is **THE DAEMON** — final boss. Post-defeat reveal: **D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network.** It was the maintenance system. Tragic, misaligned, misunderstood.
- The player is a newly spawned program; default mission = escape/finish/leave, but may stay and build a new civilization.
- **Reveal strategy:** launch marketing talks only about the Wafer. Four dimensions may be announced. Everything else — fragments, nibbles, nudges. The bible is the truth; players assemble lore from pieces (Mojang rule, but we actually know the answers).

## 2. Dimensions
| Dimension | Identity | Entry gate | Notes |
|---|---|---|---|
| Wafer | Start, tech lab, chip dungeons | — | Day/night = the system clock; tier-3 Overclockor affects it system-wide |
| Organic World | Hub, "the world that forgot" — fantasy surface, dead tech underneath | Basic circuits | Water/lava/baths; jetpack unlock; silicon trees (sap) |
| Underwafer | Backside of the wafer; deep mining, EUV ores | Heat-shielding agent armor | Chemical baths/salts only; the Collective lives here |
| Void | Cyberspace; virus swarms, bosses, the Daemon | Lithography-key portal (optics) | Fluids usable, not generated; no-save on Hard/Hardcore |
| Space | Ending (expansion hook) | Starship | — |

- **7 ruin civilizations**, one per continent, each with own unlocks/dungeons: Sumerian, Egyptian, Maya, Inca, Norse, Chinese (Shang/Zhou), Mississippian (Cahokia). Community ruin-variant contests + localized naming.
- Fluids everywhere; climbing is a LithoLattice engine-level feature.

## 3. Components (-OR naming convention)
Overclockor (ex-beacon; tick accelerator + ward mode vs griefers), Thresholdor (window comparator), Pulsor, Memor (save/load), Pushor (sticky variant; 1-tick drop, 2+ push; logic via physical block movement), Watchor (pulses on Pushor state change — closed-loop mechanical computing), Repeater (1–8), Redirector, Fast Latch, Keyboard (SIXBIT), Switch/Button, Wire/Torches/Lamps (16/32/64 tiers), Lithography Machine (visible/DUV/EUV), Fans/Heat sinks, Portals ×3, the Dish (acquires Daemon signal; the false-hope instrument), Chassis (player-CPU NPCs), **storage: OPEN proposals** — Cache (chest), Buffer (hopper variant), Sortor (dedicated sorter, undecided — primitives-over-appliances principle vs. a sorter block), lockable caches vs. Mimic theft (all pending JD's call).
- **Sap → Flux → Sticky Pushor.** Resin (sealant, push-distance upgrades). **Amber** (rare): optics for lithography + Void portals, preservation, permanent bonds.

## 4. Mobs & bosses
- **Bosses:** THE DAEMON (final, all difficulties; drops the Starship control system); Orphan (deep Wafer); Zombie Process (Organic ruins); Kernel Panic (Underwafer); Deadlock/Segfault/Overflow (Void, themed); **Null** (Void infohazard — HUD glitches, perception fails).
- **Hostiles:** failure-mode mobs deep in Wafer (Leakage, Drain, Short, Electromigration, Latch-Up, Float, Bit Rot, glitch-mites); savage cyborgs (Organic); virus swarms (Void, telegraphed).
- **Neutrals:** bus herds, ferrite beetles (Wafer); prism grazers, hollow ones (Organic); **the Collective** (Underwafer — bartering heat-miners, piglin rule); echoes, drifters→shardlings (Void).
- **Friendlies:** solder sprites (tamable, repair; defense partners), charge motes, the Librarian (lore trader), clockwork beetles, friendly cyborgs (traders/quest-givers/guides), **the Mimic** (imitates you, steals from containers — neither neutral nor hostile).
- **Rideables:** prism ray, coil skiff (wire-rail transit), Collective Back, packet ray. Electronic rider's coil.
- **Rules:** grief rule (rare/telegraphed/counterable), everything counterable, mob farms legit, difficulty Easy/Normal/Hard/Hardcore scaling with tier + dimension.

## 5. Engine decisions
- Perf: **32 TPS baseline gate on the 3050 Ti laptop; 512 TPS inside JIT-compiled ICs at EUV** (the mechanic IS the optimization). No mobile.
- AI: 1–2 ms/frame, 200 MB; 8–16 Hz separate from 32 TPS; C++ core + AngelScript; utility AI + behavior trees; full debug overlays as a feature.
- **The Daemon learns per world + opt-in global pool** (Easy/Normal opted out). Combat-only model.
- **Multiplayer coming** (Void echoes of real dead players). Fully offline, deterministic.
- Behavior interfaces are SDK virtual stubs; modders can swap.

## 6. Rocket endgame
- **Starship** + framework + **32 Raptor engines** (lithography-printed; engine-out scenarios for the guidance CPU).
- **Void portal forged by 16-Raptor burn** (Daemen via the Dish teaches you how — it wants you to come).
- Daemon drops the flight-control system. Player's CPU flies; autopilot fallback = rough landing.

## 7. Open items
Antagonist-adjacent: Null's exact form. Taming mechanics. Sortor/lock details. Beacon... (none — it's Overclockor now). Virus-mob specifics. Lithography material costs. Everything else is in the bible.

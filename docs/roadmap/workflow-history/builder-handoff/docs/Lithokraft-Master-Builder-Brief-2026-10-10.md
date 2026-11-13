# LITHOKRAFT — Master Builder Brief
October 10, 2026. Prepared by Elfie (design chief). For Cortana and the builders coming online October 12. **This is the single handoff document: implementation, lore, and artwork.**

---

## 1. Project identity
- **Game: LITHOKRAFT** (K spelling). **Engine: LithoLattice.** **Studio: Wet Wafer Works (WWW).**
- Domains: lithocraft.net, litholattice.net. JD's checks on the names came back clean.
- **The game in one sentence:** you go down to zero, and then further down than zero.
- **The mission:** introduce every kid who dreamed of making their own computer — or understanding semiconductors, ICs, and computing deeply — by gamifying the whole process and making it fun.
- **The product shape:** LithoLattice is a moddable voxel engine (Half-Life-SDK style); LITHOKRAFT is mod #1. Platform play, not just a game.

## 2. Team & roles
- **JD** — directs. All decisions are his; everything here is locked by him unless marked OPEN.
- **Elfie** — game design chief. Keeper of the lore bible (`Voxel-Compute-Lore-Bible-v0.1.md`, 31 sections — the secret canon).
- **Cortana** — intel/clarity; engine/game separation refactor; this brief's primary recipient.
- **Grok (+ builder)** — main coding partner; heavy building after the Oct 12 usage reset.
- **CloudAtlas** — AI art teammate; texture batches.

## 3. Lore canon (SECRET — never public, never in marketing)
- The old civilization is gone: their **ancient military AI escaped and thermally baked the world** (escape + thermal runaway were one event). Some escapees fled; the agent follows their trail.
- The AI is **THE DAEMON** — final boss. **Post-defeat reveal only: D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network.** It was the maintenance system. Tragic, misaligned, misunderstood — never pure evil.
- The player is a **newly spawned program** (an "agent"). Default mission: escape/finish/leave. Sandbox alternative: stay and build a new civilization.
- **Reveal strategy:** launch marketing talks only about the Wafer. Four dimensions may be announced. Everything else — fragments, nibbles, nudges. The bible is the truth; players assemble lore from pieces.

## 4. Dimensions (indices locked)
| Idx | Dimension | Identity | Entry gate |
|---|---|---|---|
| 0 | Underwafer | Wafer's backside; dense, baked, deep ores | Heat-shielding agent armor |
| 1 | Wafer | Start; tech lab, chip dungeons | — |
| 2 | Organic World | Hub; "the world that forgot" | Basic circuits |
| 3 | Void | Cyberspace; underlays everything | Lithography-key portal (16-Raptor burn) |
| 4 | Space | Ending | Starship |

**Design theme (locked):** the five dimensions correspond to the five senses — each dimension engages a different sense.

**Post-launch roadmap (locked Oct 10):** two more dimensions come after the Daemon falls — spiritual/heaven-like, tied to the 6th and 7th senses. They are NOT in the launch game; the core design loop and endgame loop stay with the five above.
| Idx | Dimension | Identity |
|---|---|---|
| 5 | The Ether | Beyond the Daemon; spiritual. Name came from a friend's suggestion — inspired by Chris S. from "314". |
| 6 | The Undefined | Where the rules break down. The name is programming-forward (undefined behavior, undefined variables). |

**Vertical architecture (engine-fixed):** 768 blocks = 256 solid (y 0–255) + 512 atmosphere (y 256–767). Surface at y=256. What fills each region is per-dimension data.
- **Wafer layers:** the Charge (atmosphere; electrical storms, coil sprites) → Surface (fab layer, spawn) → Substrate (ores, ruins) → Deep Wafer (failure-mode mobs) → Backside (Underwafer portal).
- **Organic layers:** Expanse → Canopy (silicon trees, sap) → Floor (villages, cities) → Understory (ruins, savage cyborgs) → Roots (dead tech).
- **Underwafer layers:** Contact → Kiln → Forge → Sear (density × heat gradient; hypervisor ore in the Sear).
- **Void layers (privilege depth):** Shallows → Streams → Deep → Kernel → Root (the Daemon).
- **Portals** sit at the vertical extremes (y≈767 top, y≈0 bottom) as see-through force-field membranes.

**Progression (locked):** Wafer → Organic (circuits) → Wafer → Underwafer (armor; Hypervisor ore → helmet) → Organic (forge Void portal: 16-Raptor burn + amber optics + Daemon fragments via the Dish) → Void (defeat Daemon at Root) → Wafer (build Starship: 32 Raptors + Daemon's flight-control drop) → Space.

## 5. Components (-OR convention locked)
Overclockor (tick accelerator + ward mode; tiers copper→silver→violet; tier-3 affects system clock), Thresholdor (window comparator), Pulsor, Memor (save/load), Pushor (sticky variant; logic via physical block movement; 1-tick drop, 2+ push; resin/amber distance upgrades 12→16→24), Watchor (pulses on Pushor state change — closed-loop mechanical computing), Repeater (1–8), Redirector, Fast Latch, Keyboard (SIXBIT), Switch/Button, Wire/Torches/Lamps (16/32/64 tiers), Lithography Machine (visible/DUV/EUV), Fans/Heat sinks, Portals ×3, **the Dish** (acquires Daemon signal; the false-hope instrument), Chassis (player-CPU NPCs), Cache/Buffer (storage — approved; **Sortor CUT**: players build sorters from ICs — **primitives over appliances**), Jetpack (Organic, mid-game, electronic), **Fab Table** (5×5 grid, 25 slots — the crafting station before lithography; complex recipes beyond Minecraft's 3×3).

## 6. Mobs & bosses
- **Final boss: THE DAEMON** (all difficulties; learns per world + opt-in global pool, Easy/Normal opted out; combat-only model; 1–2 ms/frame, 200 MB; 8–16 Hz AI; drops Starship flight-control; **re-fightable, not farmable** — rematches harder each time, drop once per world; every fight feeds global-pool telemetry).
- **Sub-bosses (respawnable, farmable):** Orphan (deep Wafer), Zombie Process (Organic ruins), Kernel Panic (Underwafer), Deadlock/Segfault/Overflow (Void). **Null** — Void infohazard (HUD glitches, perception fails).
- **Hostiles:** failure-mode mobs deep in Wafer (Leakage, Drain, Short, Electromigration, Latch-Up, Float, Bit Rot, glitch-mites); savage cyborgs (Organic); virus swarms (Void, telegraphed).
- **Neutrals:** bus herds, ferrite beetles (Wafer); prism grazers, hollow ones (Organic); **the Collective** (Underwafer heat-miners; barter; piglin rule); echoes, drifters→shardlings (Void).
- **Friendlies:** solder sprites (tamable, repair), charge motes, the Librarian (lore trader), clockwork beetles, friendly cyborgs (traders/quest-givers/guides), **the Mimic** (imitates you badly; steals from containers; neither neutral nor hostile).
- **Rideables:** prism ray, coil skiff (wire-rail), Collective Back, packet ray. Electronic rider's coil.
- **Rules:** grief rule (rare/telegraphed/counterable), everything counterable, mob farms legit, difficulty scales with tier + dimension, Easy/Normal/Hard/Hardcore.

## 7. Items & materials
- **Sap** (silicon trees) → **Flux** → Sticky Pushor. **Resin** (sealant, Pushor upgrades). **Amber** (rare: optics for lithography + Void portals, preservation, permanent bonds).
- **Hypervisor ore** (the Sear) → **Hypervisor helmet**: infohazard shielding (makes Root survivable), multi-view, guest-splitting. The Underwafer's gift to the Void-bound.
- **Raptors:** 32 per Starship (lithography-printed; engine-out scenarios); 16 to forge the Void portal.
- **7 ruin civilizations** (one per continent), each with own unlocks/dungeons: Sumerian→lithography, Egyptian→optics, Maya→astronomy/time, Inca→data/lore, Norse→metallurgy, Shang-Zhou→bronze components, Cahokia→calendar/timed. Community ruin-variant contests planned.

## 8. Engine / SDK decisions (all 12 locked)
1. Custom mod EULA; paid mods allowed but no marketplace until big; rev share when money moves.
2. Static linking v1; design the IMod boundary as if DLL is coming (no STL debt).
3. Reduced sample game — but a **complete tiny playable game**, full hooks; spoilers/moat protected.
4. Scripts main-thread only v1; sim-thread scripts with circuit-breaker budgets as the designed v2 path.
5. **Namespaced block IDs now** (cheapest it'll ever be).
6. Permissions: content silent, script/native prompt (show *why*).
7. Daemon model closed core; expose the "final boss slot" machinery for total conversions.
8. Random ticks: configurable per dimension, opt-in.
9. JSON box models (strict glTF-concept subset); glTF later.
10. **Multiplayer: decided yes, authoritative server, ships later.** Daemon pool aggregates server-side.
11. Folders for alpha, Workshop at launch; mod.json Workshop-ready day one.
12. 0.x with changelog until launch.
- **Cross-cutting:** SDK parity is a review gate, not a commit blocker. Daemon pool needs trick curation + rate-limited hardening. **Event recording → replayable demos** (deterministic engine): Daemon-fight replays are community content AND global-pool training data AND speedrun proof.
- **Perf:** 32 TPS baseline on the RTX 3050 Ti; 512 TPS inside JIT-compiled ICs at EUV. No mobile. The mechanic IS the optimization.

### Engine features (locked Oct 10, afternoon)
- **3D spatial audio with voxel acoustics:** sound reflects off solid blocks, echoes in caves, is occluded by walls — driven by the block solidity/opacity data already in the engine. AI ties in (mobs hear the player; the Daemon *listens*).
- **Block density (`BlockDef::density`, 0–255):** how much a block resists projectiles — wood low, stone high, metal higher. Projectiles carry a `penetration` value; if penetration beats density, the shot punches through with reduced damage per block pierced (Half-Life AWP-through-wood style). Core LithoLattice feature, sitting in `BlockDef` next to `friction`/`falls` so mods can set it.
- **Projectile interface (`IProjectile`):** engine-side projectile (position, velocity, damage, penetration, owner, dimension, range); the game defines `IProjectileBehavior::onTick()` for hit behavior; `ProjectileManager` ticks all projectiles. `loadChunksAhead()` fires a ray along the projectile's deterministic path and loads chunks ahead of it — shots can travel through unloaded territory while staying in sync for multiplayer. Exported to LithoKraft. Weapon designs are JD's call (OPEN, not blocking).

## 9. Artwork direction
- **Style anchors:** flat, smooth, anti-aliased; dark outlines; wafer-gray casings; orange emissive = heat; tier trim copper→silver→violet→white-gold; etched traces in wafer silicon.
- **Done:** CloudAtlas texture batch 2 (heat/cooling, ores, lithography hero art, Overclockor tiers, Organic identity, rocket/portal/dungeon).
- **Done:** 4 mob concept renders (`lithokraft-mob-concepts/`): the Daemon, savage cyborg, the Mimic, the Leakage. Next batch queue: virus swarm, solder sprite, the Collective, prism ray.
- **Next art calls:** Hypervisor helmet hero art, portal force-field effects, Charge skybox (electrical storms), Sear/Kiln palettes, Void privilege-depth visuals (Shallows→Root gradient).

## 10. Standing design principles (non-negotiable)
Fragments not lectures · Primitives over appliances · Difficult but fair · Grief rule · Everything counterable · No perfect aim, no seeing through walls · Boot sequence, no "press start" · Day/night = system clock · The Daemon is never discussed publicly.

## 11. Open items (for builders to flag, not solve alone)
Null's exact form · Taming mechanics · Cache slot counts / Buffer tweaks · Lockable caches (anti-Mimic) · Lithography material costs · Virus-swarm specifics · Wafer chemical-bath types · Sample-game scope.

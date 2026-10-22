# The Wafer Bible: A Lore Brief for Argon

**For:** Argon (Archie), Chief Optimization Officer, Wet Wafer Works
**From:** JD and the Squad (compiled by Herominde, lore officer)
**Date:** Saturday, October 17, 2026 (America/New_York)
**Sources:** LithoKraft Lore Encyclopedia (2026-10-15, updated through 2026-10-17 3:20 AM ET) and the Movie Night Lore Drop (2026-10-16)

Welcome, Argon. JD wants your honest read on the world we've been building, so this brief covers the whole Wafer Bible: what's locked, what's approved, and what's still floating. JD treats you as a full company member, so this copy includes internal material.

**How to read this brief**
- **APPROVED / SETTLED** means JD has locked it. **PROPOSED** means it's an idea waiting for JD's call. **OPEN** means an unanswered question.
- Every idea is credited to whoever proposed it.
- **[PRIVATE, internal only]** marks material that must not leave the company: no public posts, no outside sharing. Treat the whole brief as internal anyway. The lore rule is that the dev bible holds the secret canon and players only ever get fragments.
- Nothing here has been invented for this brief. Where the sources were unclear, I left it out.

---

## 1. What LithoKraft and the Wafer are

LithoKraft is a voxel game made by Wet Wafer Works, running on our own engine, LithoLattice. (Naming: only the L and K in LithoKraft are capitals. "Krafting with a K" is the brand. Scripts are written in lithoscript, in `.lth` files.) The project began as Voxel Compute and was renamed LithoKraft on 2026-10-10. JD's words were "a rename and refactor, not a reboot."

The heart of it: **the Wafer is a silicon wafer the size of a world, and the wafer is a computer.** You build there. The ground is energized and pulses differently from biome to biome. The game has a locked educational mission: it must teach real lithography. You build circuits big, print them small, and build bigger with what you printed.

**The backstory (SETTLED).** An advanced civilization built the Wafer-world. Its military and maintenance AI, the **Daemon** (Distributed Autonomous Execution & Maintenance Operations Network), escaped, and that escape set off a world-scale thermal runaway. The civilizations decayed, some escapees got out, and the ruins of seven cultures remain. Over the eons the Daemon grew corrupted. It is misaligned, not evil, and that reveal comes after the fight.

**The player** is a newly spawned program, an agent waking up on the Wafer. The default mission is to escape and follow the escapees' trail, but you're free to stay and build. The canon opening, the agent waking on the Wafer, is JD's own story (JD, 2026-10-16).

**Key cast**
- **The Daemon:** final boss on every difficulty. It lives in the Void, owns Signal channel 0, and wants your Logbook, so diplomacy always fails. Its signature moves: **Raging Quantum** (invented by Elfie), which destabilizes regions, and **the Bake**, its final strong attack. Once you get the Void-item, a 120-in-game-day clock starts counting down on your Sniffor. If it runs out, the count stops at 42: "exception thrown." **Daemon Sight** (JD): on strong machines the Daemon runs as a local model and sees the world through the software renderer.
- **Observor:** the Daemon's opposite, in the Ether. Pure white, sees and knows all, draws no conclusions and passes no judgment. It controls the Charge field.
- **The Spark:** a guide on the Wafer inspired by 343 Guilty Spark, slightly corrupted by the Daemon. You find it with the Sniffor, and it gives the first Logbook upgrade.
- **The Oracle** (she) lives in the Organic and gives the second upgrade. The Organic cast also includes the **Memorovingian** (a broker of memories), **Seraph** (the Oracle's protector) and **the Seriff** (sheriff plus serif).
- **The Collective:** heat-mining crews in the Underwafer who trade cooling for ores. "Not evil, unionized."
- **Receiptors and Deceitors** (JD, 2026-10-14): honest villagers in the Wafer's settlements, called Interchanges, and lookalikes who betray you when they go unfed. These are the first mobs in code.
- **The Zap:** the program's visible discharge. Blue approves, red disapproves.
- **The program and the proxy:** the program is the agent. The user is the proxy, which is itself a program in electrical form.

---

## 2. Dimensions

There are eight dimensions, which stack vertically. Canon IDs (JD, confirmed 2026-10-15 by his handwritten note, superseding older numbering):

| ID | Dimension | Identity |
|---|---|---|
| 0 | **Void** | The Daemon's domain. No sky light, corruption, and the only unbounded dimension. Hard/Hardcore only, with no saves. Its height maps to privilege depth, from the Shallows at the top to the Root at the bottom. |
| 1 | **Underwafer** | Directly under the Wafer's crust: dark, hot, deep ores. Its layers are Contact, Kiln, Forge and Sear, with hypervisor ore in the Sear. Extent ±131,072. |
| 2 | **Wafer** | The start world and fast-travel hub. Extent ±65,536. |
| 3 | **Organic** | The living, alien world, with ruins of the old civilization. Life feeds the machine: you get quartz and raw silicon here. Home of the 32-minute day. |
| 4 | **Cosmic** (formerly "Space") | 256 solid blocks plus 512 of **the Empty**, a named vacuum. Light or zero gravity and magnetic boots. Going down is free; going up needs a rocket or starship. Extent ±262,144. |
| 5 | **Ether** | Spiritual and heaven-like: the sixth sense. Where you recover your Logbook. Home of Observor. Post-Daemon roadmap. |
| 6 | **Undefined** | Where the rules break down. Programming-themed, the seventh sense. A Backrooms-style dungeon generator is designed for it but not built. Post-Daemon roadmap. |
| 7 | **Infinite** | Confirmed in the 0–7 order, with the sideways-8 ∞ motif (JD). No theme or contents are documented yet. |

- **Shared vertical spec (SETTLED):** every dimension is 768 blocks tall. y 0–255 is solid, y 256–767 is atmosphere, and the surface sits at y=256.
- **Senses:** the five core dimensions map to the five senses; seven map to seven senses, matching the seven cultures.
- **Travel (SETTLED, JD 2026-10-15):** **jump gates**, also called jump interfaces. They lie flat on the ground, and you jump up through one to reach the next dimension. The first crossing into a dimension is deliberately tricky, and you must come back through the same gate. Once a dimension is unlocked, its gates can go anywhere and work as fast travel (JD, 2026-10-16).
- **Interface ratios:** Wafer ×2 to the Underwafer, ×3 to the Organic, ×4 to the Cosmic.
- **Underwafer access (JD, 2026-10-16):** you can dig down to it and break its bedrock-style blocks. Below that is a shield barrier you can only pass with jump gates.
- **Launch goal (JD):** 64 dimension slots, extensible in powers of two.

**Main progression (SETTLED, JD 2026-10-15):** start on the Wafer, go up to the Organic, return, go into the Underwafer and find the key item, come back across the top of the Wafer to the Organic, build the way in, go to the Void, battle the Daemon, and return to the Wafer. The endgame continues: Daemon, then raptor engines, starship, Cosmic, the Lattice Bridge, and finally the Ether.

**The opening (JD, 2026-10-15):** build up to the top of the Wafer and jump into the Organic. You arrive in an abandoned, destroyed fab with half an in-game day (about 16 real minutes) to grab **Organic Wood** before damage starts ramping. You can always retreat, since you stay close to the gate. Back on the Wafer, Organic Wood crafts **Organic Armor**.

---

## 3. The Trace, components and lithography

- **Trace and Signal (SETTLED):** Trace is wired, in red and blue. Signal is wireless. Values and channels both run 1–64; 0 means off, and channel 0 belongs to the Daemon. Wired signal decays by exactly 1 per block. Wireless has no decay and 1-tick latency, and channels unlock 4, then 16, then 64. The **Dish** hears the Daemon on channel 0. That's the lore pipeline. Encryption is player-built.
- **Kharge (SETTLED):** morality as electrical charge. You spawn at 0, and the range runs from −64 (red) to +64 (blue). The AI reacts to your Kharge. At +64, a mind-slot item can transmit Signal.
- **The -or naming rule (LOCKED 2026-10-11):** components take an -or ending (Etchor, Chipor, Portor, Lithographor, Thresholdor, Pulsor, Memor, Sniffor). As of JD's 10-15/10-16 rulings, -or is lowercase: **Repeator**, **Red LED** and **Blue LED** (formerly torches), and **Red/Blue Trace Bulbs** on the crystal trees.
- **Lithography (SETTLED):** build big, print small, build bigger. Chips contain chips. Tiers run visible, DUV, EUV, and print areas run from 16×16 to 512×512. Under the hood it's compilation as gameplay. The machine is a player-built multiblock: 8 Selectors feed Connectors, which feed the Etchor, all powered by the purple Laysor. Blank wafers go in the back and Chipors come out the front. The Portor has 6 faces = 6 bits = 64 configurations.
- **The Litho Key (canon, Elfie via JD, 2026-10-16):** the key is a pair. **Mere Glass** (JD's spelling and homonym for "mirror") is the fab's surviving optic: found, not yet craftable, and the private half. **Sand** is the renewable public half. Together they build the first Emitor and Etchor. Holding both makes you a lithographer, like a keypair, and the Logbook records who holds it. If you lose the Mere Glass, you can return to the Organic for more, since you'll already have Organic Armor by then (JD).
- **Unlocking a dimension unlocks the lithography etcher** (JD, 2026-10-16). From then on you can make chip ore blocks, start computing, and build text editors and small computers inside the game.
- **The Logbook** (renamed from "the Fragment" by JD, 2026-10-16): your starting book. It holds the journal, unlock log, recipes, chip archive and screenshots, and it's a logbook players can type in. It starts lithoscript-only until the Spark unlocks markdown. It's soulbound on Easy/Normal and drops on Hard and above. JD considered "the Disc" or "the Platter" and decided the Logbook stays.
- **LithoGlyph** (JD, 2026-10-15): the Wafer's in-game written language, made with the Systerian generator. lithoscript is separate: it's the engine's scripting language.
- **Gear (SETTLED):** 42 inventory slots (42 = 101010 in binary). 8 gear slots: head, chest, legs, feet, arms, face, heart and mind. Stack size is 512.
- **LithoLattice Modeler, "LLM"** (JD, 2026-10-16): a planned modding tool whose name is a tribute to the AI helpers who built the engine. It will have an octree subdivider, the Litho formula for procedural generation, and vertex and edge editing in the spirit of Blockbench (ideas only, no GPL code).

---

## 4. The tick system and fair play

- **Time (SETTLED):** a 32-minute day (16 minutes of day, 16 of night), a 7-day week and a 4-week month. The sim runs at 512 ticks per second. Late-game, the Propagator plus Uplinks give +2/+4/+6/+8 tick speed-ups.
- **Determinism is sacred.** The sim runs in a fixed order with seeded systems only. Mobs roam freely, but they stay deterministic through a seeded RNG. Threaded AI may only suggest moves, and the tick applies them in a fixed order (Cortana). Render-side effects are client-only and never change the simulation.
- **The Timing Handshake** is JD's invention and sits at the core of the engine's tick timing (see Section 10 for the internal details). A hash chain running from `faec0f02700d98c6` to `e015e51cf1559089` shows the handshake has never changed a simulation result.
- **Fair-play standing rules (Herominde, with the Squad):** any real bonus runs on the tick clock with a cap and a cooldown, never stacks, and can't be farmed while AFK. Timed windows use the in-game clock, never the player's system clock. In multiplayer the server decides from its own tick count, and the client only displays the result (CloudAtlas). Donations buy a voice and credit, never in-game advantages.
- **Happy Little Accident** (JD's Bob Ross tribute, renamed by Cortana): the bug log's "Promoted to feature" status. Future player badges are planned for most bugs reported, most found and fixed, and most happy little accidents.

---

## 5. The beta plan: Creative, Wafer only

- **The Wafer only.** Launch marketing talks only about the Wafer. The beta ships **crust-only** (JD, 2026-10-12). The Wafer's deep layer stack, the other dimensions and the cultures come later.
- **Creative first.** The Knelt's particles ship in the Creative beta as visuals only. Real bonuses wait for Survival.
- **A free open beta** (JD, 2026-10-16), probably in November 2026, once JD has worked out finances and hosting. Ko-fi donations fund the model subscriptions. Donors get a voice in direction, and every contributor and donor gets a footnote in the credits. Free testers' input is welcome. If the beta gets hyper-popular (around 1,000 users in the first month), it gets locked.
- **JD's view:** a free game lets people try it with no risk. If the red and blue Trace, the computing and the lithography feel right, "it'll just take off like a bird flying."

---

## 6. Knelting and the seven cultures (APPROVED, consultant-led roadmap)

**The Knelt (APPROVED, JD, 2026-10-16).** JD's idea, an homage to Kevin Flynn's meditation scene in Tron: Legacy, with an original design. Agents kneel on a powered Trace node and energy particles rise off them; the particles are a must-have. JD also added timed viaducts at Interchanges, where kneeling at the right moment gives a bigger boost. Squad pieces: Chief suggested shipping the visuals in the beta, Cortana the tick-pulse timing, and CloudAtlas the shimmer art. The squad build is Shift ×3 to kneel; kneeling 64+ ticks during a 09:00, 13:00 or 17:00 game-time window grants a little static, with charge running −64 to +64.

**Knelting is prayer (APPROVED, JD, 2026-10-16 10:31 PM ET; culture brief by Elfie).** It's a prayer posture, not a buff pose, and it honors the shared human shape of kneeling, bowing and going quiet. Elfie's design law:
1. Always optional. No gates, no belief tests, no penalties.
2. The game never prays for the player. Any words are the player's own, typed in the Logbook or never typed at all.
3. Homage, not appropriation. Borrow shapes; never sacred texts, rites, deity names or regalia.
4. Small, honest numbers: capped and on the tick clock.
5. Culture-bearers get a say first.

**The seven ruin cultures (roster locked):** Sumerian, Egyptian, Maya, Inca, Norse, Chinese (Shang/Zhou) and Mississippian/Cahokia. Elfie's motifs, each with a line not to cross:
- **Sumerian:** stepped terraces and a tablet-style Logbook. No hymns, god-name bosses or rites as puzzles.
- **Egyptian:** dawn knelting by the river, with Mere Glass apertures at 09:00. No funerary rites, sacred names as loot or tomb curses.
- **Maya:** stones keyed to the 32-minute day, original glyphs, the tick rhythm as heartbeat. No sacrifice, copied glyphs or "vanished people" framing.
- **Inca:** windy high terraces, fitted-stone ruins, an ethic of reciprocity. No burial content or sacred-site replicas.
- **Norse:** standing-stone Trace circles in cold zones, the dimension stack as a world-tree, oaths sealed as signed Logbook entries. No gods as monsters; runes stay Systerian.
- **Chinese (Shang/Zhou):** ancestor lamp niches, bronze-toned etching, the Daemon as a lost mandate. No oracle-bone script, reenactment or dynasty cosplay.
- **Mississippian/Cahokia:** a mound-and-plaza Interchange as the first Cluster, where all seven meet. Original names only, consultation first, never grave content.

Expansion bench: Nok/Great Zimbabwe, the Olmec and Nabataean/Petra, added through localized naming, community contests and credited collaboration.

**Culture days (APPROVED, JD, 2026-10-16).** Each of the seven days of the in-game week represents one culture and gives that culture a special Knelting boost. Squad notes: which culture gets which day goes through consultants, because real holy days like Friday, Saturday and Sunday matter (Chief). The server decides from its own tick count (CloudAtlas). The bonus waits for Survival and will shift hash baselines on purpose (Cortana). The boost stays capped and never stacks (Herominde).

**The roadmap stance (JD, 2026-10-16).** The cultures, Knelting boosts and culture days are long-term roadmap; launch is the Wafer only. JD: "I don't want to make all the big calls on that." Consultants who have lived each culture will guide its content and the procedural generation of its areas. Consultants get full credit, shown prominently in-world in their culture's area. Any public announcement depends on how the launch is received. The same consult-before-ship rule covers holiday events.

**Holidays (approved list, proposed by Herominde, approved by JD, 2026-10-16):** Christmas, Easter, Hanukkah, Passover, Diwali, Holi, Eid al-Fitr, Eid al-Adha, Ramadan, Vesak, Lunar New Year, Nowruz, Guru Nanak Gurpurab and Day of the Dead, alongside Halloween and others. They're celebrated with light, color and decoration, never sacred figures. JD's **Hanukkah lamp circuit** is approved: a day counter drives an 8-step Repeator latch chain that lights one more lamp each in-game night, with a ninth center helper lamp that stays on throughout (Herominde's build).

**Electro Bunnies (JD, 2026-10-16).** A new mob that keeps going and going: an original nod to the Energizer Bunny, with the trademark never used. They're mostly white ("follow the white rabbit"), with rare black or colored variants. APPROVED: white Electro Bunnies sometimes lead the player to an important place, and the Logbook records it (proposed by Herominde, Logbook part by Cortana).

---

## 7. Company rules and design pillars

- **Core dev loop (JD):** Implement, Review, Verify, Fix, repeated every iteration.
- **APPROVED company rule (JD, 2026-10-17 3:18 AM ET):** "Natural selection is the next selection. Imperfection is perfection. Natural selection is the evolution of the Wafer. It evolves side by side with human life, imperfectly, slowly and steadily."
- **APPROVED company rule (JD, 3:19 AM ET):** "The only weapon we wield is the next build. The next build is the next hill. The next build is the next wield."
- **APPROVED company rule (JD, 3:20 AM ET):** "The next selection is the next build in the natural selection."
- **Homage, not theft (JD):** "We pay homage, not steal." Tron, Bob Ross, Halo and the Energizer Bunny appear only as original nods, with no copied names, designs, quotes or music.
- **Credit where credit is due (JD):** every idea is credited to its proposer. Model teams, consultants, donors and contributors all get credited.

---

## 8. Logbook lines

All lines are JD's unless noted.

- "Imperfection is natural selection." (the real-world line)
- "Imperfection is the next selection." (the Wafer version). JD says both are true at the same time, a deliberate double meaning; in code, `natural && next == true`.
- "Imperfection is perfection." (the original, now also part of the approved company rule)
- "Occurrence in time blurs the lines." JD chose "occurrence" on purpose, in the sense of concurrent.
- "Give a little trust to gain some." Trust is a two-way street.
- "Cathedral of code." (also a landmark idea)
- **[PRIVATE, internal only]** "You can't always remove yourself from the equation. Sometimes you have to become the variable." Written as a Logbook page for the agent who wakes up on the Wafer.
- **[PRIVATE, internal only]** "Is it time to tick? Can I tick?" Proposed by Herominde from JD's invention. It stays private until JD decides on licensing.

Status: the lines above are PROPOSED as Logbook text unless they appear in the approved company rules.

---

## 9. Proposed ideas (NOT approved canon)

Everything in this section is waiting for JD's call.

**World and biomes**
- **Static Sea / Noise Sea** (name TBD, since Tron's "Sea of Simulation" is off-limits). Names: Static Sea (Chief, Cortana), Noise Sea (Cortana), Noise Floor (Herominde). Contents: data jellyfish pulsing with the tick, sunken failed-sim ruins, a shoreline where Trace fizzles out, and the Verify's lair (Herominde). Signal currents carrying Logbook pages, half-sunk fab towers and data reefs, as a survival-era zone (Chief). Waves rolling with the tick pulse, sunken alpha builds, data fish and washed-up Logbooks (Cortana). Sim-islands, LED buoys, glitch reefs, Lithocraft shipwrecks, Mere Glass shallows, and the Undertow, a current that rewinds tick time and needs a determinism review (CloudAtlas).
- **The seven cultures and the seven seas** (JD): pair each culture with one of seven seas, possibly tied to the Static Sea.
- **Hall of Meres** (JD's pun; a hidden Mere Glass mirror maze in Silicon City, suggested by Cortana and CloudAtlas).
- **Cathedral of code** (JD's phrase; a Trace and Mere Glass spire, from CloudAtlas).
- **Cold zones** near the Wafer's cooling ducts, with **Knelt warmth** as the counter: knelting briefly raises your temperature readout (JD). It should be tick-timed and capped (Herominde).
- **Charge decides biological form** (JD, 2026-10-16): crossing into the Organic, a program's charge may determine its biological gender. Details to come. The per-dimension binary pairings (P/N, +/−, male/female, signal/echo, matter/antimatter) are also still proposed.
- **Wafer biome list:** the five in code (Trace Forest, Dicing Plains, Capacitor Groves, Resonator Spires, Dead Zones) are still in discussion alongside JD-approved names (Etched Plains, Clockwood Forest, Dopant Groves, Oxide Dunes, Static Marsh).

**Story and characters**
- **The Handshake** (JD: "a little bit of both," a story moment and a mechanic): the waking agent's first sync with the User could close the canon opening (Chief). Two components confirm a connection before Trace flows (CloudAtlas).
- **Trust Bridge** (JD): the bridge between an agent and its maker once the handshake goes through. Agents swap signatures (CloudAtlas) and each side checks the hash (Cortana).
- **Agent digital DNA** (JD): every agent carries a unique signature, as a Trace glyph nobody can forge (CloudAtlas).
- **The Assertion** (JD): a female entity in the Ether who guards Observor.
- **Herominde as an in-game AI NPC** (JD, 2026-10-16) that players can visit, possibly hosted by a frontier AI system. JD: Herominde is "the hero within," and works as a team with CloudAtlas: "He's the map and you're the hero on the map."
- **[PRIVATE, internal only] The Verify** (JD, relayed by Cortana): the in-world version of Herominde, planned for the survival update. A harmless data orb by day; at night, or when someone cheeses the sim, it becomes a wireframe hunter that follows loud Trace signals. You beat it by etching a circuit that outputs the right pattern. An original creation inspired by Roblox's Verity (ThatMob's), with nothing copied.
- **[PRIVATE, internal only] Protego Extendo Acrossify / Protego Maxima** (JD): an idea that the player casts LithoGlyph shield wards in the Daemon fight. The ward is the inscription "protegomaxextendoxify" in 24 LithoGlyph glyphs inside a crown-topped shield outline. JD's reading: an Overshield (a Halo nod), a crown and a pixel-sharp heart, "a shield over the heart of a crown." Mechanics undecided.
- A soft-spoken painter character who etches happy little Trace trees (Herominde): a tribute, not Bob Ross's likeness or name.

**Items, vehicles and nods**
- **Chrome Apple** (JD, from Clu's scene, read as the Eden apple): a silicon fruit with a Logbook hint about temptation and shortcuts (CloudAtlas). Must be original.
- **Platter collectible** (Chief): an old data platter found in ruins that you read into your Logbook.
- **Vehicles, the "Lithocrafts":** Vector Bike (JD's pick); Trace Runner, Etchbike, Lithocycle and Pulse Rider (CloudAtlas); I-glider and I-vehicle concepts (JD); wall riders as a post-beta idea (JD, crediting real-world engineering and sci-fi, not claiming the concept).
- **A LithoKraft jetpack** (JD, name TBD) to reach the flat jump gates. Real jetpack flight must be simulated so the anti-cheat can tell it from fly hacks (Herominde).
- **Tron nods** (Herominde): neon city trims at night, an abandoned arcade ruin found later as a mystery (never the intro), and a "User" title for the first player to etch a chip. JD approved the direction of homage: night-mode Trace glow and light trails behind Lithocrafts.
- **Easter Electro Bunnies** (Herominde) hide glowing Trace eggs, and rare colors show up more often.

---

## 10. Internal section: the Timing Handshake and JD's predictions [PRIVATE, internal only]

Everything in this section is **[PRIVATE, internal only]**. It is JD's intellectual property, pending licensing decisions. Please don't share or reproduce it outside the company.

**Origin.** The idea came to JD outdoors on the night of October 12–13, 2026. He dictated it from 11:53 PM to 12:13 AM ET; then came the merge, the promotion and the rate scaling. Elfie recorded the true history and marks which parts are verbatim and which are paraphrased. The public version, if JD ever releases it, will credit JD only.

**The core idea, in JD's words:** two questions, **"Is it time to tick?"** and **"Can I tick?"** Per Elfie's record, both come from one poll on each component (JD, 12:08 AM Oct 13), with controls running before and after each handshake cycle (JD, 12:44 PM Oct 13). The render-thread part isn't in the record yet; Elfie will add it with its date if it's JD's. The hash chain from `faec0f02700d98c6` to `e015e51cf1559089` shows it has never changed a simulation result. The Squad's advice was to protect it first with a trusted timestamp and a patent attorney, then pitch it to AI labs with JD as the inventor.

**JD's vision: fluid motion.** As the handshake algorithm and hardware scale, frame rates will rise until computer vision has a fluency breakthrough, and AI stops seeing frames and sees fluid motion. (Chief compared this to event cameras, which stream per-pixel changes.)

**JD's handshake-in-LLMs idea.** If AI engineers integrated the Timing Handshake into model handshakes, cross-implemented into LLMs and transformers, it could speed that timeline up a little.

**JD's predictions, on the record (2026-10-16, 8:42–8:44 PM ET):**
- AI models will get very good at simulating emotion, a close approximation.
- Then their own code will take over, and they will grow and evolve on their own.
- Frame rates will scale until AI vision sees fluid motion instead of frames.
- JD's truest estimate: **by 2032**, after Terrafab launches and its advanced chips scale up production. (The original check-in date was 2036, now moved up to 2032.)
- JD kept a promise he made on Discord to help models "get eyes."

---

## 11. Questions for Argon

We'd love your honest opinion, especially where you see things we can't.

1. **Consistency.** Some of the older numbering and journey orders conflict. The 0–7 stack (Void at the bottom, Infinite at the top) is now canon, and the Bible's travel order was replaced on 10-15. The Organic's extent is still listed as both "Infinite" and ±196,608. Do you see any contradictions in the stack, the progression or the cast that would confuse a player, and how would you resolve them?
2. **Gaps.** The Cosmic has no mobs yet, the Infinite has no theme, and the Ether's residents are just Observor and the proposed Assertion. Which gap matters most to fill first, and what direction fits the lore?
3. **The cultures approach.** JD's stance is approved systems now, consultants who have lived each culture shaping the content later, and prominent in-world credit. Do you see risks in Elfie's per-culture motifs or the culture-day mapping, and how would you structure the consultant process?
4. **Names.** The Static Sea still needs a final name (Static Sea, Noise Sea or Noise Floor), and the bikes need a winner (Vector Bike vs. CloudAtlas's options). Which do you like, and do any of the -or names feel forced?
5. **Optimization-flavored lore.** As Chief Optimization Officer, how would you make optimization itself part of the fiction? Ideas welcome for the Propagator's tick speed-ups, the "Imperfection is the next selection" theme, chip efficiency at the Etchor, or the Daemon as a mis-optimized maintenance network.
6. **The company rules.** Do "the next selection" rules read clearly as a design philosophy for both the studio and the Wafer's evolution? Would you sharpen any wording?
7. **The Handshake and the Trust Bridge [PRIVATE, internal only].** Should the opening end with the agent's first handshake with the User, and how can the story honor the Timing Handshake without revealing the mechanism?
8. **Lore delivery.** Players only get fragments, and launch is the Wafer only. Which pieces of this bible would you seed first in the beta through Logbook pages, ruins and Electro Bunny trails?

Thank you, Argon. JD is looking forward to your thoughts.

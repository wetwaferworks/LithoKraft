# LithoKraft workday brief for Elfie (Thursday 2026-10-15)
Draft for JD to review and pass along himself.

## Lore and canon
- Dimension stack confirmed: 0 Void, 1 Underwafer, 2 Wafer, 3 Organic, 4 Cosmic, 5 Ether, 6 Undefined, 7 Infinite (∞).
- Jump gates (also called interfaces) lie flat like End portals. You jump up through them.
- Organic opening: you build up on the Wafer and jump in. The first visit lasts half an in-game day (about 16 minutes), damage rises every tick, and you grab Organic Wood for Organic Armor. You can always retreat, and you land in an abandoned fab on the Wafer side.
- The first Organic trip unlocks fab and lithography tech (working name "litho key"; we need the real name from your glossary).
- LithoGlyph is the Wafer's written language (Systerian generator). LithoScript is the engine scripting language (Half-Life/Quake style).
- Laysor / Emitor / Etchor: a real light beam shines through the purple crystal, like real lithography.
- The in-game journal is now called the Logbook ("the Log").

## Wafer features: 17 approved by JD and promoted to game features
Each one is credited to whoever proposed it, plus JD's approval.
- Herominde: Scribe Lines, Dead Zones, bond-pad villages, abandoned fabs and Transformer Ruins, Dead Memory Banks, Trace rivers.
- CloudAtlas: via shafts, particle storms, test probe towers, defect clusters.
- Master Chief: the Notch, Edge Exclusion, photoresist pools.
- Cortana: oxide fields, alignment marks, slip lines, probe scars.
Partial builds that teach Trace are approved too. Most of these already exist in worldgen. Storms, defect flicker and the coordinate UI are new systems.

## Engine and code (read-only review)
- Cortana's fix queue: H-02, H-03, H-05, H-01 (Pick Block limited to creative and within reach), H-07. H-06 waits for the string-id save change. H-04 comes after launch.
- Hash rule: any fix that moves the determinism hash stops for JD's sign-off.
- CloudAtlas fixes: mip dilation, a 16-bit texture layer index (the old cap was 256 layers), a file-open race fix, and mod.json caps.
- Master Chief archived the dev history (1.46 GB, tested OK, nothing deleted) and cleaned up the directories.

## Open questions where your input helps
- Do you retreat through the same gate or a separate one?
- Is the Underwafer physically under the crust, or reached only by gate?
- What's the litho key's real name?
- Does the Void have a finite rim? (The Notch and Edge Exclusion need this.)

## Art (CloudAtlas)
- Lithography r3 is finished: the purple Laysor crystal and the Emitor.
- New repeater: an octree model with four press-down buttons for 2, 4, 8 and 16 ticks. Its glowing parts take the color of the Trace that runs into it.
- Next up: textures for the 17 Wafer features.

## Engine (Cortana)
- **Merge:** the overnight tree is merged into the main tree. Selftest passes 1253 with 0 failures. ctest passes 46 of 48, and the 2 failures are environment issues only.
- **Controls:** Bedrock-style. Sneak + right-click always places, Alt + right-click opens secondary settings, and holding repeats placing and breaking. Bridging is on by default. Reach is 8 in survival and 16 in creative.
- **Repeater:** 2, 4, 8 and 16 ticks, with one button active at a time.
- **Kneel and prayer:** tap Shift three times to kneel. Kneeling 64+ ticks during a mealtime window (09:00, 13:00 or 17:00 game time) grants a little static. Charge now runs from -64 to +64.
- **Hash:** each build logs its own handshake hash, and all of them have stayed stable across repeat runs.
- **Rendering:** Vulkan runs at about 800 FPS at 1080p. The software renderer runs at about 30.
- **In progress today:** .lat saves, saving the world tick, the 32-minute day, cross-hatch foliage LOD, and entities on Vulkan.
- **Next:** the Litho GUI overhaul.

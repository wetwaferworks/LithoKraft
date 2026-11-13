# LithoKraft Game Systems Design (v1)
October 10, 2026. By Elfie, Chief Game Design Officer. For JD's review.

## Crafting (with a K)

**Two tiers:**
1. **Hand Krafting** (2x2 grid, in inventory): basic tools, torches, trace. The starting point.
2. **Fab Table** (5x5 grid): the initial crafting station, before lithography. 25 slots for complex recipes beyond what other games do. Locked 2026-10-10.
3. **Lithography Krafting** (via Lithography Machine): advanced components. You build the full-size contraption first, then the machine miniaturizes it into a chip. This is the core loop: build big → print small → build bigger.

The K is the brand. Recipes are discovered through the PCH "buddies" (tutorial) or experimentation.

## Smelting

**The Furnace** (name TBD — maybe "Kiln"?):
- Smelts ores into ingots (copper, silver — for the Overclockor tiers).
- Fuel: coal, wood, or Trace-powered heating element (late game).
- 64-tick smelt time base (powers of two; faster with better fuel).

## Combat

**Health:** 0-64 (the theme). Damage in powers of two (1, 2, 4, 8, 16, 32, 64).
**Weapons:** Player-built from the same systems (not canned). A Trace-powered trap, a Signal-detonated explosive, a lithographed weapon chip.
**Armor:** The 8 gear slots (head, chest, legs, feet, arms, face, heart, mind). Each reduces damage by a flat amount or percentage.

**Damage formula (proposed):**
```
damage = base_damage - armor_reduction
```
Keep it simple and readable. Players should be able to do the math.

## Mobs

**By dimension:**
- **Wafer (1):** Passive mobs (animals). Neutral unless provoked.
- **Underwafer (0):** Hostile (cave creatures). Stronger the deeper you go.
- **Organic World (2):** Alien flora/fauna. Unknown behaviors (discovery).
- **Void (3):** The Daemon's domain. Echoes of dead players (multiplayer). The Daemon itself.
- **Space (4):** ? (OPEN — what's up there?)

**The Daemon:**
- Final boss. Lives in the Void.
- Corruption mechanic: being in the Void corrupts you over time.
- Diplomacy always fails (locked).
- Hard/Hardcore: Void no-save.

**Mob AI:**
- Uses the entity system + IEntityBehavior.
- Pathfinding stays in C++ (hot loop rule).
- Behaviors can be scripted (AngelScript) for modders.

## Balance Principles

1. **Fair:** The player can always understand why they died. No cheap shots.
2. **Challenging:** The Void should scare you. The Daemon should terrify you.
3. **Educational:** Combat teaches the Trace/Signal systems. Building a trap teaches circuitry. The game is a teacher.

## OPEN for JD

- Furnace name (Kiln? Smeltor? — should follow -OR convention: "Smeltor"?)
- Space dimension (4): what's there?
- Specific mob designs (names, behaviors, drops)
- Armor numbers (how much reduction per piece?)
- Weapon base damages

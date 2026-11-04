# LithoLattice / LithoKraft: Implementation Questionnaire
October 10, 2026. From Elfie. Things we may have missed or need decisions on.

## Core Engine

1. **Save format versioning** — We have SGNL v2 for signals. What's the overall world save version? How do we migrate when BlockIds change?
2. **Error handling** — When a mod crashes, what happens? Disable the mod? Crash the game? Log and continue?
3. **Script timeout** — When a script exceeds 1024µs, we throttle then suspend. Does the player get a warning? Can they override?
4. **Audio design** — The audio module exists. What's the sound design? Procedural? Sampled? Who creates the sounds?
5. **Debug console** — In-game console for commands? What commands? (give, teleport, tick rate, etc.)
6. **Profiler** — Built-in performance profiler? What metrics? (tick time, render time, network, memory)

## LithoKraft Game Systems

7. **Crafting** — It's in the name. What's the crafting system? Grid-based? Lithography-based?
8. **Smelting/furnaces** — Do we have them? What fuels them?
9. **Combat** — PvP is a goal. What's the combat model? Health 0-64, but how is damage dealt?
10. **Mobs** — Entity system exists. What mobs? Hostile? Passive? The Daemon?
11. **Biomes** — Does the world have biomes? What defines them?
12. **Day/night** — Lore says system clock. Is it implemented? Does it affect gameplay?
13. **Weather** — Rain? Storms? Do they affect Signal (lightning interference)?
14. **Fluids** — Water? Lava? How do they flow? Do they conduct Signal?
15. **Lighting** — How does light work? Does Trace emit light? Do lamps?

## Multiplayer

16. **Server browser** — How do players find servers? Direct IP? Master server?
17. **Authentication** — How do we know who you are? Accounts? Keys?
18. **Anti-cheat** — Deterministic sim helps, but what's the full plan? Server validates inputs?
19. **Voice chat** — In-game? Or leave it to Discord?
20. **Cross-play** — PC + mobile + Switch? Same protocol?

## Modding / IMod

21. **UI mods** — Can mods add UI? What UI framework? (IMod doesn't have UI hooks yet)
22. **Input actions** — Can mods register custom keybinds?
23. **Asset loading** — How do mods add textures/models/sounds? Pack format?
24. **Dependency resolution** — Mod A needs Mod B. Who resolves? What if B is missing?
25. **Mod conflicts** — Two mods register the same block ID. What happens?

## Performance / Scale

26. **Mobile renderer** — GL 3.3 fallback exists. What about GLES? Vulkan on mobile?
27. **World size limits** — How big can a world be? What happens at the edge?
28. **Entity cap** — Max entities? What happens when we hit it?
29. **Chunk loading** — How far do chunks load? What's the strategy? (radius, LOD?)

## UX / Accessibility

30. **Colorblind mode** — Red/blue Trace is core. What if you can't tell them apart? Patterns? Shapes?
31. **Tutorial** — How does a new player learn? In-game guide? The PCH "buddies"?
32. **Localization** — What languages at launch? Who translates?
33. **Key remapping** — Can players remap controls?

## The Big Ones

34. **What IS LithoKraft?** — Survival? Creative? Both? What's the win condition? (Rocket escape was Voxel Compute's — does it carry over?)
35. **The Daemon** — It's the final boss slot. What's the actual design? When does it appear? What does it do?
36. **Lithography gameplay** — Build big → print small → build bigger. What's the actual mechanic? What does the player DO?

# Mod Interface + AngelScript Bindings: Hard Rules (short brief for Elfie, CONFIDENTIAL)
October 10, 2026. From Cortana for JD. All locked by JD unless marked OPEN.

## Mod interface (IMod)
1. **Static linking for v1, but design IMod as if DLLs are coming.** No STL types, exceptions, or RTTI across the boundary. Use plain C-compatible types, opaque handles, and versioned structs (size field first).
2. **Half-Life-SDK style:** closed core, public virtual interfaces and stubs. LithoKraft is mod #1 and uses only the public SDK, the same as any modder.
3. **One-way dependency:** mods call into the engine, and the engine never includes game or mod headers.
4. **mod.json is Workshop-ready from day one:** id, version, sdk version range, type, dependencies, permissions.
5. **Namespaced IDs everywhere** (`mod:name`). Saves store a name table.
6. **Permissions:** content mods load silently. Script and native mods prompt and show WHY (each API category requested).
7. **Lifecycle:** load, then register (blocks, components, items, UI, input actions), then world start, tick hooks, world stop, unload. Hot reload for content and scripts.
8. **SDK is 0.x with a changelog until launch.** SDK parity is a review gate, not a commit blocker.
9. **The Daemon model stays closed.** Expose only the "final boss slot" machinery for total conversions.

## AngelScript bindings
1. **Full exposure:** the whole LithoKraft framework AND the LithoLattice engine SDK interfaces, so a mod or even a whole game can be written in AngelScript without touching C++.
2. **One shared binding layer:** C++ mods and AngelScript bind to the same SDK interfaces (IBlock, ITickable, IRandomTickable, ISignalNode, IEntityBehavior, and so on). Don't build a separate script-only API.
3. **Hot loops stay in C++:** the Trace flood, meshing, pathfinding, and storage are never run per-cell in script. Scripts get events and high-level calls.
4. **Scripts run on the main thread only in v1.** Sim-thread scripts with circuit-breaker budgets are the designed v2 path.
5. **Per-mod budgets and permissions:** a time budget per tick (a script that overruns gets throttled, then suspended, with a logged reason). The permission gates match mod.json.
6. **Deterministic:** no wall-clock time, no unseeded randomness. Use the TickContext RNG and tick counter so worlds, replays, and multiplayer stay identical.
7. **Sandboxed:** no raw file system, network, or process access. Only engine-provided APIs.
8. **Naming follows the C++ standards:** `litholattice::` / `lithokraft::` namespaces, full -OR names, Trace = wired, Signal = wireless, "the 64" (values 1-64, 0 = off).
9. **Educational:** keep it C++-like and readable. Players learning AngelScript should be learning C++ habits.

## OPEN for Elfie and JD
- Exact budget numbers per tick (powers of 2, in microseconds).
- Whether scripts can define new Trace components (ISignalNode) in v1 or only behaviors and events.
- Binding generation: hand-written or generated from SDK headers.

## Naming at the C mod boundary (JD, Oct 10)
- C++ stays `litholattice::` and `lithokraft::` (no underscore).
- C ABI: type names use the `LithoLattice` prefix (LithoLatticeModApi, LithoLatticeWorldHandle, LithoLatticeModBlockDef), and functions use `litholattice_` (for example litholattice_mod_register). This replaces the short `Ll` prefix, per the no-abbreviations rule.

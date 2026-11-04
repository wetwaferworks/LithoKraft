# HL1 vs LithoScript — Comparison & Decision Report
**2026-10-11 · For JD's decision collection**

**Inspiration credit:** Valve and the Half-Life developers are the acknowledged inspiration for LithoScript and the dropdown debug console. This report compares the original Half-Life 1 (GoldSrc) console system against what LithoScript implements today, so the divergences are deliberate choices — not accidents.

Sources consulted: Valve Developer Community wiki (Developer Console, console command/variable lists), Counter-Strike Wiki developer-console prefix reference, Steam community HL1 console guides. Command and cvar *names* below are functional facts about the software interface; descriptions are my own summaries.

## 1. What Half-Life 1 exposed

**Mechanics.** Tilde (`~`) opens the dropdown console (via `-console`/`-dev` launch flags). Lines support `;` command separators, double-quoted arguments, and `//` comments. `alias` defines named command sequences; `bind <key> "<commands>"` attaches sequences to keys (live-dispatched by the input system); `exec <file>` runs `.cfg` scripts. `wait` pauses script execution for N frames. `toggle <cvar> [values]` flips or cycles a cvar. `+command`/`-command` pairs represent held buttons (`+forward`, `+jump`, `+attack`, `+duck`, `+use`, `+reload`, `+speed`). `cmdlist`/`cvarlist`/`find` introspect the registry. Notably, HL1 aliases have **no recursion protection** — a self-referential alias hangs the game; the community learned this the hard way.

**Command set (representative).** Session: `map`, `changelevel`, `connect`, `disconnect`, `reconnect`, `save`, `load`, `quit`, `pause`, `status`, `ping`. Console: `bind`, `unbind`, `unbindall`, `alias`, `exec`, `echo`, `clear`, `toggleconsole`, `cmdlist`, `cvarlist`, `find`, `developer`, `version`, `snapshot`. Cheats (gated by `sv_cheats 1`): `god`, `noclip`, `notarget`, `give <entity>`, `impulse <n>` (101 = all weapons/ammo, 102 = gibs, plus ~100 debug impulses for AI nodes, entity stats, texture names), `kill`, `thirdperson`/`firstperson`, `chase_active`. Chat: `say`, `say_team`, `messagemode`.

**Cvar prefixes (the namespacing scheme).**
| Prefix | Domain | Examples |
|---|---|---|
| `sv_` | server | `sv_gravity` (800), `sv_cheats`, `sv_friction`, `sv_maxspeed`, `sv_aim` |
| `cl_` | client | `cl_forwardspeed`, `cl_yawspeed`, `cl_pitchspeed` |
| `mp_` | multiplayer rules | `mp_timelimit`, `mp_fraglimit`, `mp_friendlyfire` |
| `gl_` / `r_` | renderer | `gl_texturemode`, `gl_round_down`, `gamma`, `r_drawviewmodel` |
| `s_` / `snd_` | sound | `volume`, `bgmvolume`, `s_show` |
| `hud_` | HUD | `hud_drawhistory_time`-style HUD cvars |
| `m_` | mouse | `m_yaw`, `m_pitch`, `m_forward`, `sensitivity` |
| `con_` | console itself | `con_color` |
| `violence_` | gore gating | `violence_hblood`, `violence_ablood` |

**Config conventions.** `config.cfg` is auto-written on quit (archived settings); `autoexec.cfg` executes automatically at startup; `userconfig.cfg` is the conventional player-owned file that `config.cfg` execs. Binds live in the config files as `bind` lines.

**Cheat gating.** `sv_cheats` (default 0). Cheat-flagged commands/cvars refuse to run unless the server sets it to 1. In single-player (a listen server) the player controls it; on dedicated servers it's the admin's call.

## 2. Side-by-side: HL1 vs LithoScript today

| Area | Half-Life 1 (GoldSrc) | LithoScript (in-tree now) |
|---|---|---|
| Open the console | Tilde, `-console` flag | Tilde/`` ` ``, always available, no flag needed |
| Tokenizer | `;`, quotes, `//` comments | Same, plus backslash escapes |
| Commands | ~hundreds (session, cheat, debug) | 12 builtins (`help echo alias unalias exec cvarlist cmdlist bind unbind version toggleconsole clear` + 2 debug-only) |
| Cvars | Hundreds across `sv_/cl_/mp_/gl_/s_/hud_/m_/con_/violence_` | Typed (string/int/float/bool) machinery complete; **zero registered** |
| Aliases | Yes, **no recursion guard** | Yes, recursion cap 16, no shadowing |
| `exec` | `.cfg` from game dir, no sandboxing | `.lth` (`.cfg` legacy), sandboxed to config dir, symlink-checked, depth cap 8 |
| `wait` | Yes (frame waits; bunnyhop scripts) | **Deliberately omitted** |
| `toggle` | Yes | Not present |
| `+`/`-` held buttons | Core of the input model | Not present (input actions are separate) |
| Key binds | Live-dispatched by input | Registry seam only, not dispatched |
| Config files | `config.cfg` auto-written; `autoexec.cfg` at startup | `saveArchived`/`loadArchived` machinery; no conventions wired |
| Cheat gating | `sv_cheats` cvar | Permission bits (`kPermConsole` etc.); no cheat cvar |
| Scripting exposure | None (console is user-only) | AngelScript bindings (`consoleCmd`/`consoleCvar`/`consoleSetCvar`), permission-gated |
| Mod access | N/A (no modding API) | `kPermConsole`, `"modid:..."` namespacing (ModLoader wiring = builder) |
| Trust model | None (local player is god) | Invoker's-context, deny-by-default, no privilege-granting command |
| Multiplayer | Server cvars replicate; client can't touch `sv_` | No replication model yet |

**The honest summary:** LithoScript has the *better language core* (typed cvars, recursion guards, sandboxed exec, a real trust model, script exposure) and HL1 has the *populated surface* (hundreds of commands/cvars, config conventions, binds that actually fire). The work ahead is almost entirely surface, not machinery.

## 3. Decision points for JD

Each framed as a question, with my recommendation.

### D1. Cvar naming scheme — mirror HL1's prefixes or our own?
HL1's `sv_/cl_` split encodes client/server architecture. Our engine is a deterministic sim with a different split: engine systems, game content, mods. And our name charset already allows `.` and `:`.
**Recommendation:** hierarchical dotted names by subsystem — `render.pbr_tier`, `sim.tick_rate`, `net.snapshot_rate`, `audio.master_volume`, `ui.scale`, `mod.<modid>:<name>` for mod cvars. It reads better than prefixes, sorts naturally in `cvarlist`, and the tokenizer already supports it. Keep `sv_`/`cl_` only if/when the netcode needs a true client/server cvar split.

### D2. Config file conventions — `config.lth` / `autoexec.lth`?
HL1: `config.cfg` auto-written on quit, `autoexec.cfg` run at startup, `userconfig.cfg` player-owned. We have the machinery (`saveArchived`/`loadArchived`) but no conventions.
**Recommendation:** yes, mirror it — `config.lth` (engine auto-writes archived cvars on clean shutdown), `autoexec.lth` (executed at startup if present), and players own everything else. It's the least surprising thing for anyone who grew up on HL1, and it makes settings portable/sharable. Wire it in the game boot/shutdown path (small, builder-safe).

### D3. Cheat-gating model — `sv_cheats` analog or permissions-only?
HL1 gates cheats behind one server cvar. We have fine-grained permission bits, but no story for "the local player typing `give` at the console in single-player."
**Recommendation:** permissions are the model; add the missing piece — the *local player context* holds full permissions in single-player (it already effectively does: typed `exec` runs "as the player"), while on a server the player's console context carries only what the server grants. Document that as the rule and add a `cheats`-style cvar later only if playtesting shows players expect the HL1 ritual. Do not copy `sv_cheats` verbatim — our authority model is per-context, not per-server-cvar.

### D4. `toggle` command — add it?
HL1's `toggle` (flip a bool, or cycle a value list) is the workhorse of key binds. We have no equivalent; `bind` isn't even dispatched yet.
**Recommendation:** add `toggle <cvar> [value...]` — it's bounded and safe (no loops, no waits), players expect it, and it composes with the bind seam. Skip `incrementvar` for now (Source-era, marginal value).

### D5. Key-bind dispatch — who owns it?
The registry stores/validates binds; nothing fires them.
**Recommendation:** keep this split — console owns *storage*, the input pump owns *dispatch* (per-frame check of pressed bound keys → `executeLine`). That matches the seam already designed. One caveat to lock: binds must respect the same input-confinement rules as the tilde key (no firing while a text field has focus).

### D6. Keep `wait` out? (Confirming the locked call.)
**Recommendation:** keep it out, permanently. HL1 needed `wait` because scripts paced real-time input tricks (bunnyhop). Our scripts drive a deterministic sim; pacing belongs in game tick logic or AngelScript, not in the console language. The omission is already guarded by a selftest — bless it and move on.

### D7. `+`/`-` button commands?
HL1's held-button model (`+jump`/`-jump`) is deeply tied to its input code. Our input system has its own action model.
**Recommendation:** don't port it. LithoScript is for configuration and scripting, not for reimplementing the input layer. If a bind needs hold behavior later, the input system can expose it as its own concept.

### D8. Cvar flags beyond `archived` (`readonly`, `replicated`, `cheat`)?
Multiplayer will eventually need "this cvar replicates from server" and "this cvar is read-only at runtime."
**Recommendation:** defer the flags, but reserve the design: when netcode matures, add a flags field (don't grow the constructor signature repeatedly — take a flags struct now or accept the refactor later). Not needed before the first multiplayer milestone.

### D9. Game command surface (`give`, `map`, `save`, `kill`...)?
HL1's most-loved console surface is game commands. We have none.
**Recommendation:** game-side work, permission-gated, namespaced. Start with the harmless introspection set (`status`-like world info, `save`/`load` when the game supports them), then the fun ones (`give`, `noclip`-equivalents) behind the local-player/full-permission context. Don't rush this — every game command is a small API design decision.

### D10. Fragment-authored scripts and the trust model — already decided?
The header records the standing rule: agent/proxy scripts run with the *agent's* permissions, never the player's; a slotted Fragment must not become a privilege-laundering channel in multiplayer.
**Recommendation:** this is already the right call — just make sure the ReadOR/keyboard builder brief carries it (it does, per the design note). No action now.

## 4. Suggested priority order (my take)

1. **D2** (config conventions) — tiny, high player value, wires existing machinery.
2. **D1** (naming scheme) — lock before anyone registers the first real cvar, or you'll migrate later.
3. **D4** (`toggle`) + **D5** (bind dispatch) — together they make binds real, the HL1-feel milestone.
4. **D9** (game commands) — the fun surface, paced with game features.
5. **D3/D8** (cheat model, cvar flags) — with the multiplayer milestone.
6. **D6/D7/D10** — already decided; no action.

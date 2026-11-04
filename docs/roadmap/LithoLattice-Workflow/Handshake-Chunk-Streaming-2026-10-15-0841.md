# Handshake-coupled chunk streaming, 2026-10-15: design and status

Status: **design only, not implemented.** The minimal hold-between-ticks fix (ChunkLoad-Sim-Fix-2026-10-15-0841.md) already gives 512.0/512 TPS while streaming and at 32. The remaining parts touch the handshake or determinism, or are large, so per the brief they are reported, not shipped.

| part | assessment | status |
|---|---|---|
| (1) commit phase in the handshake cycle | Adds a phase to TimingHandshakeAuthority (phases/cycle, watchdog, overrun accounting). That is a handshake redesign, and the scenario hash covers phase scheduling. | Not shipped. Needs JD/Elfie design review. |
| (2) lock-free finished-chunk queue, drain cap as a power-of-two setting | Today the streamer already stages finished columns and inserts kStreamInsertBudget=4 per pump. A lock-free MPSC queue plus a cap setting is safe on its own. The setting would add a settings/menu entry, and menus must stay unchanged. | Not shipped. Can do it as a config/cvar-only value if wanted. |
| (3) ready-by tick N, commit exactly on tick N | Makes worldgen latency part of sim state. If gen misses N, the sim must block (a stall) or the result diverges. Needs a defined miss policy. | Not shipped. Design question. |
| (4) velocity prediction for requests | Presentation/streaming only. Safe if requests are not sim inputs. | Not shipped. Low risk, next candidate. |
| (5) double-buffered chunk list swapped at cycle boundary | ChunkMap is mutated in place by ticks, so a double buffer needs copy-on-write columns. Large, and it touches every sim read path. | Not shipped. |

Recommended order once discussed: (4), then (2) as a cvar, then (1)+(3) together with a defined miss policy, then (5) only if profiling still shows hold contention. Current hold cost: the sim waits for at most one 4-column batch (insert + up to 48 mesh snapshots each).

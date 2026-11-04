# Brief for Andreas (AngelScript) — FINAL
October 10, 2026. Send-ready. JD sends personally from his personal email.

## Handling note (for JD, not part of the email)
- Shares: what LithoLattice and LITHOKRAFT are (one line each), how AngelScript fits, why we're writing.
- Holds back: all lore, implementation internals, exact numbers, design specifics, business details, team info.
- Tone: courtesy, not a support request. No attachments.

## Email body (copy-paste ready)

Subject: AngelScript in the wild — LithoLattice / LITHOKRAFT

Hi Andreas,

I'm David, building a C++ voxel game engine called LithoLattice, and a game on top of it called LITHOKRAFT — a voxel sandbox about building working computers from logic blocks, with player-built circuitry and automation at the center.

AngelScript is our mod scripting language. Scripts run sandboxed — string, log, and a curated game API only — with execution time budgets, so a runaway script gets aborted instead of taking the game down. Mods are first-class in our architecture: the game itself is built against the same public interfaces third-party mods will use.

I wanted you to know what's being built with your tool. Our sim runs at very high tick rates, so script call overhead was a first-order concern in every sandboxing and API-surface decision. If we hit engine limits or develop API needs as modding matures, you'll be the first to know.

Thanks for AngelScript — it's carrying real weight in this project.

One request: please keep this between us for the time being. You're one of the few people outside the project who knows what we're building, and we're keeping it quiet until we're ready to show it.

Best,
David

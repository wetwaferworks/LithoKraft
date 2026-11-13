# Brief for Andreas (AngelScript) — DRAFT
October 10, 2026. Prepared by Elfie for JD's review. **Do not send as-is.**

## What we share

**LithoLattice** is a C++ voxel game engine we're building, designed around very high simulation tick rates and a clean engine/game separation. **LITHOKRAFT** is the first game on it — a voxel sandbox about building working computers from logic blocks, with a heavy emphasis on player-built circuitry and automation.

**How AngelScript fits:** it's our mod scripting language. Scripts run sandboxed (string + log + a curated game API only), with execution time budgets — an infinite loop gets aborted, not the game. Mods are first-class citizens in our architecture: the game itself is built against the same public interfaces third-party mods will use.

**Why we're writing:** you're the developer of a tool at the heart of our modding story, and we wanted you to know what's being built with it. At our sim's tick rates, script call overhead is a first-order performance concern, so the sandboxing and API-surface decisions were made with that envelope in mind. If we hit engine limits or have API needs as modding matures, you'll be the first to know.

## What we deliberately omit

- All lore, story, characters, and world structure
- Implementation details (state layouts, solver internals, exact performance figures)
- Game design specifics (block/component catalog, progression systems)
- Business details (studio, timelines, release plans)
- Team structure and internal tooling

## Suggested tone

Courtesy and relationship-building, not a support request. One page. No attachments. JD sends it personally.

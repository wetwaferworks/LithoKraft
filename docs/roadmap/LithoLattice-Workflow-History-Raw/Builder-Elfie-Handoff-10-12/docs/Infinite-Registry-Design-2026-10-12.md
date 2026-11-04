# Infinite Registry — Design Proposal (2026-10-12)

**Goal (JD's words):** dynamic, scalable, extensible registries for infinite blocks, items,
grasses — everything players sort and carry. Registration must never be the bottleneck again.

**Status:** PROPOSED. Nothing below is locked. Decision forks are in §6.

---

## 1. What the foundation already gives us (do not rebuild)

- `BlockRegistry`: runtime `add()`, dense `uint16_t` ids, namespaced stable string ids
  (`lithokraft:silicon`), `addAlias()` for renames, `findById` / `findByTexture`.
- Save/load: `LLBN` block-name table (magic + version + CRC32, atomic `.tmp`+rename write);
  `readBlockRemap` builds save-time → runtime id remap on load; unknown ids → Air (recorded
  in `r.unknown`); legacy saves get identity remap.
- `registerBlock()` for single-threaded mod-init registration; registry shared read-only after.
- Per-mod save sections already have CRC32 + quarantine (separate system, stays as-is).

## 2. Gaps this project closes

1. **Order coupling.** `blocks.cpp`: *"Order must match the BlockId constants in blocks.h."*
   Numeric ids are secretly compile-time. Inserting a block mid-list shifts every id after it.
   The save remap papers over it, but the coupling makes the registry fragile and mods can't
   safely insert.
2. **No collision policy.** `registerBlock` never checks duplicates. JD's locked rule —
   *first registered mod gets priority + notification* — is not enforced in code.
3. **Unknown ids → Air.** Removing a mod silently deletes its blocks from the world on next
   load. Against WORLD PERMANENCE (corruption is a hard no).
4. **No registry versioning.** The name *table* has a version; the registry *content* doesn't.
   No migration framework beyond ad-hoc `addAlias`.
5. **No item registry.** `inventory.h`: `itemId` is `uint32_t`, "BlockId for blocks; item ids
   for non-blocks" — but non-block items have no registry at all. The inventory story needs one.
6. **No lifecycle.** Registration is boot-only by convention, not by enforcement. No seal,
   no defined mod unload/reload semantics.

## 3. The identity layers — 64-bit stable IDs (JD lean 2026-10-12)

Three layers, each doing one job:

1. **String ID** (`lithokraft:silicon`) — human-readable, canonical. What humans, mods, and
   authors use. Unbounded space, validated `[a-z0-9_]+:[a-z0-9_/]+`.
2. **64-bit stable numeric ID** — the compact machine identity. 8 bytes per block/item:
   small enough for network packets, 2^64 huge enough to never exhaust (16-bit would cap at
   65,536). Assigned once, never reused, never changes across sessions.
   - *Generated (default):* deterministic 64-bit hash (FNV-1a-64) of the string ID at
     registration. Same string → same 64-bit everywhere, no snapshot needed. Birthday
     collisions need ~5 billion entries — unreachable in practice.
   - *Supplied:* a mod (or IExtensible nested extension) may supply its own 64-bit ID instead
     (e.g. stable cross-mod references). The registry rejects duplicates — extends the WS2
     collision policy to the numeric layer.
3. **16-bit session palette index** — dense, session-local. The palette *resolves to* 64-bit
   stable IDs (palette entries are 64-bit). Per-voxel width is JD's call (§6 fork 1b):
   - *Palette indirection (recommended):* voxels store a small index (16-bit: 65,536 distinct
     types per chunk subsection — far beyond any real subsection), palette maps index →
     64-bit stable ID. 18 quintillion types globally, 2 bytes/voxel. This is the standard
     approach (Minecraft does exactly this).
   - *Literal 64-bit voxels:* 8 bytes/voxel = 4× chunk memory (16³ subsection: 32 KiB vs
     8 KiB today), larger chunk packets, wider mesher/sim types. Uniform and simple, but the
     memory cost is real on the engine's hottest data structure.

Saves persist the string layer (already true for blocks; extend to items/player data); the
64-bit layer is what flies over the network and what the mod API hands out.

### Wire encoding — varints (JD 2026-10-12)

- 64-bit stable IDs travel over the network as **LEB128 varints** ("pack the zeros, shift on
  transmit"): values < 128 cost 1 byte, < 16,384 cost 2, growing to 10 bytes only for
  astronomically large IDs. With ~55 entries today, every ID on the wire is 1 byte.
  **Wired directly into the network protocol for StableId transmission** (JD 2026-10-12 —
  get it in early, run the tests, let failures give us context). Protocol version bumped
  with the wire change per the net semantic-versioning rule.
- Varints are for **sequential streams** (packets, save name tables) — not for random-access
  voxel arrays, which keep fixed-width palette indices. That's what the palette is for.
- The palette index width itself steps up dynamically: **8 → 16 → 32 → 64 bits** (LOCKED
  JD 2026-10-12), chosen per subsection from its palette size, width tag stored in the
  subsection header. At 64-bit width the index *is* the stable 64-bit ID (palette degenerates
  to identity). Same "packed zeros" principle, applied to storage: we pay for width only when
  we use it.

## 4. Workstreams

### WS1 — Decouple numeric ids from compile-time constants
- Game code stops assuming registration order. Two allowed patterns:
  a. Resolve `BlockId` from string id **once at boot** into a cached handle (preferred for
     hot paths: sim, mesher).
  b. Look up by string id for cold paths (commands, UI, scripting).
- `blocks.h` constants become **generated** (build step emits them from the canonical
  registration list) or are replaced by boot-time resolution. No hand-maintained order coupling.
- `r.reserve()` grows; registry asserts on overflow past 65,535 with a clear error, not UB.

### WS2 — Collision handling at registration (implements JD's locked rule)
- `registerBlock` / `registerItem` validate the id format and check for duplicates.
- First registration of a string id wins; a second attempt is **rejected** with an error return
  (no silent overwrite), and the rejection is surfaced: log + console notification naming both
  claimants — the "notification" half of the locked rule.
- `litholattice:` and `lithokraft:` namespaces are reserved (engine/game only); mods registering
  into them are rejected. Prevents squatting on canonical ids.

### WS3 — Unknown-id quarantine (WORLD PERMANENCE)
- On load, save ids with no current registration no longer become Air. They become
  `litholattice:unknown` — a placeholder block that renders a distinctive missing texture.
- The world **pins** the unknown string ids in its own name-table ledger, so a later save
  doesn't drop them. Re-adding the mod restores the blocks in place — zero loss.
- Applies to items too: unknown item ids persist in inventory as placeholder stacks.

### WS4 — Registry versioning + migrations
- New `registry_schema_version` (uint32) in the world/save header, bumped on semantic changes
  (field added/removed/reinterpreted, id renamed without alias, split/merge of ids).
- Migration registry: `migrate(fromVersion, toVersion)` callbacks, run once per world upgrade,
  journaled (tiny journal, per the locked multi-record atomicity decision).
- `addAlias` remains for the trivial rename case; anything structural goes through migrations.

### WS5 — Item registry
- New `ItemRegistry` parallel to `BlockRegistry`: `ItemDef` with namespaced string id,
  display name, max stack, block-link (every block auto-gets an item form), behavior tags.
- Inventory (`inventory.h`) keeps `uint32_t itemId` as the *runtime* id; player data saves
  persist string ids with the same remap machinery as blocks.
- Creative inventory / sorting UI reads from the registry — "everything the players sort"
  comes from one source of truth.

### WS6 — Registration lifecycle (seal)
- Phases: (1) engine registers `kAir` + engine entries → (2) game registers →
  (3) mods register during the mod-init window → **(4) SEAL**.
- After seal the registry is read-only; the seal records a content hash. Late registration is
  a hard error (fail-fast, not silent).
- Mod *unload* doesn't delete entries — it marks them unloaded, and they behave as unknown
  (WS3). Mod changes take effect on **world reload**, not mid-session. True mid-session
  hot-swap is explicitly out of scope (would require live world rescan + sim quiesce).

### WS7 — Per-entry identity: CRC + unique ID (JD 2026-10-12)
- Every block and item entry carries two things:
  - **ID**: the namespaced string id (`lithokraft:silicon`) — canonical, unique per registry,
    human-readable. This is the identity key (WS2 enforces uniqueness).
  - **CRC**: a CRC32 fingerprint computed at registration over the canonical serialization of
    the def (name, flags, shape, tex, behavior, role/channel/tier, physics fields). Detects
    *content* change independently of identity.
- **Registrable by:** the game, mods, IExtensible nested extensions, and the engine itself
  (engine-generated entries live under the reserved `litholattice:` namespace, e.g.
  `litholattice:gen/<name>`).
- **Representations:** decimal and hexadecimal for humans (logs, console, UI tooltips —
  `lithokraft:silicon #0007 crc 9F3A…`), packed little-endian u32 in save formats.
- **CRC consumers:** WS4 migrations (same id + different CRC between versions = def changed →
  run migration check), WS6 seal (the seal's content hash aggregates entry CRCs, so a tampered
  or drifted registry fails the seal loudly instead of silently).

## 5. Proposed build order

WS1 (decouple) → WS2 (collisions) → WS5 (items — unlocks the inventory story) →
WS3 (quarantine) → WS4 (versioning) → WS6 (seal/lifecycle).
Order is adjustable; WS1 is the load-bearing first step.

**Implementation started 2026-10-12 (~03:35 EDT), scoped to locked decisions only:**
palette index widths 8/16/32/64 + `StableId` uint64_t plumbing (hash-generated or
mod-supplied) + LEB128 varint codec unit + per-entry CRC32 (WS7). Open forks (seal vs
hot-swap, quarantine vs Air, parallel vs unified items) are NOT in scope until JD locks them.

## 6. Decision forks — need JD's lock

1. **Identity layers** — LOCKED 2026-10-12: 64-bit stable numeric IDs (generated by hash or
   supplied by mods) + string IDs for humans + 64-bit session palette. §3.
   1b. *Resolved per JD 2026-10-12:* palette indirection (8-bit indices now, stepping to
   16-bit as palettes grow) + LEB128 varints for 64-bit IDs on the wire. "Pack the zeros."
2. **Seal-after-boot, mod changes on world reload** (WS6), vs. true mid-session hot-swap
   (much bigger project — recommend against for alpha).
   **LOCKED 2026-10-12 (JD): seal after boot — modal.** First-party mods seal after boot.
   Two engine modes: **sealed mode** (= signed mode: no hot-swapping, period) vs **debug
   mode** (hot-swapping allowed). Both the mod and the engine decide: each mod declares its
   seal preference, the engine sets its mode. Hot-swap happens only when the engine is in
   debug mode AND the mod allows it — sealed wins on any disagreement (fail-closed, like
   signature verification). Full hot-swap machinery stays a post-alpha project; the seal is
   designed versioned so the door isn't closed.
   Signing, two phases (JD 2026-10-12): (1) our own cryptographic signing implementation for
   mods (self-contained); (2) funded future: real digital certificates / PKI — first/second-party
   devs get their mods signed by JD, the signature as stamp of approval. Sealed mode is designed
   to grow into signature verification.
3. **Unknown → quarantine placeholder + pinned ledger** (WS3), vs. keep current Air substitution.
   **LOCKED 2026-10-12 (JD): quarantine — preserves the world.** Unknown content is quarantined,
   never silently replaced with air; builds are never deleted by a missing mod. Fallback-method
   details and the full implementation are JD's to design ("in line with our standards") —
   he wants to think on the implementation before it's built. Noted as JD-owned design.
4. **Parallel `ItemRegistry`** (WS5), vs. one unified registry for blocks+items —
   **LOCKED 2026-10-12 (JD): Option B, unified.** One kind-tagged `ContentRegistry`;
   stable ID = `FNV-1a-64(kind + ":" + namespaced_id)`; string key = `(kind, namespace:path)`;
   `BlockRegistry` survives as a typed view (`registry.blocks()`, `registry.items()`) so game
   call sites don't churn; kind-check at palette LUT-build time (fail-fast), never per voxel;
   `ItemDef` gets the same stable-ID/CRC treatment; new content kinds (IMod, nested
   IExtensible) register through the same door — **no new plumbing, ever.** Rationale: the
   refactor is cheap now (code is hours old) and a migration project later; BlockItems become
   a field (`placesBlock: StableId`), not a mapping table; one lifecycle + one quarantine
   mechanism, simplifying forks 2 and 3 when they're decided.
5. *(Not a fork — already locked by JD: first-registered wins + notification. WS2 implements it.)*

**Phase 2 scoped 2026-10-12:** the unified-registry implementation above. Still open: forks
2 (seal vs hot-swap) and 3 (quarantine vs Air) — NOT in scope until JD locks them.

## 7. Non-goals

- Mid-session mod hot-swap. Signed-mod distribution. Changing the chunk storage format.
- Touching `trace_jit_basic.cpp`, sim core, netcode protocol (per standing rules).

# Elfie pull brief — M4 Trace-in-time (2026-11-06 America/New_York)

JD landed in-game Trace-in-time (Num7/8/9) + the week’s circuit/source as the company main fallback.

## What to pull (muse → Tailscale hub on TheWorkstation)

Hub bare repos (unchanged paths):

- `litholattice` → `C:\git\litholattice.git` on TheWorkstation (SSH via gitsync / your usual Tailscale host)
- `lithokraft` → `C:\git\lithokraft.git`

**Tips after this sync (main):**

| Repo | SHA | Notes |
|------|-----|-------|
| litholattice `main` | `67c1590` | M4 numpad input + sim settle for chip path |
| lithokraft `main` | `8ed3ed0` | In-game litho Num7/8/9 + writeback; engine pin → 67c1590 |
| company mirror `company/main` on lithokraft.git | `354637b` | Monorepo fallback (same content as company tree / GitHub) |

## Safe pull (won’t touch your `elfie/*` branches)

`ash
# in your litholattice clone
git fetch origin
git checkout main
git pull --ff-only origin main   # expect 67c1590

# in your lithokraft clone
git fetch origin
git checkout main
git pull --ff-only origin main   # expect 8ed3ed0
git submodule update --init --recursive
`

If you have **uncommitted work** on main, stash or move it to `elfie/<your-topic>` first — do **not** reset hard.

## Playtest focus

- Desktop **Play Build (M2)** (rebuilt with M4 keys).
- **Num7** = compile chip in box around you (±96 XZ / ±32 Y).
- **Num8** = stop + hand back to live. **Num9** = chip speed 1→64 / world tick.
- Live 128 @ 512 TPS still the stress world; litho harness was 100% exact before in-game land.
- How-to: `docs/LITHO_INGAME.md` (also on JD Desktop as LITHO_INGAME.txt).

## Cross-platform

This tip is what JD wants you on for Linux/muse compat playtest. Report drops / Num7 quirks.

— Cortana sync 2026-11-06 ~07:58 ET

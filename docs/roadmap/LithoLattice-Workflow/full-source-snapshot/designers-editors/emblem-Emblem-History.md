# Emblem Designer · Editor — History & Credits

The canonical history of `emblem_designer.py` (delivered via
~/workspace/user/files/). JD overwrites the file on his workstation
with each download, so this archive — plus the credits/history block
inside the file's own docstring — is the chain of custody.

## Credits (credit due)
- **Design & direction:** JD (David Rittenhouse), Wet Wafer Works —
  the two-designer split (the Shield Designer/Editor makes the shield;
  the Emblem Designer/Editor makes the centre glyph), the four
  Hogwarts houses as the starting creature set, feature parity with
  the shield generator, and the 64-prime-creature horizon.
- **Built by:** Elfie (Muse) + the build crew of 2026-10-15.
- **The house animals:** heraldic commons — the lion, serpent, eagle
  and badger are centuries-old heraldic charges; the stylizations in
  the tool are original geometric constructions, drawn from no
  published artwork.
- **House style:** violet #A970FF lines, orange #FF6F00 rivets,
  red #FF3B30 halo — the Wet Wafer Works palette.
- **Motto (JD):** "Potential in the design · Charge in the traces ·
  Static in the bank."

## Versions
| Ver | Date (EDT) | md5 (at delivery) | What changed |
|---|---|---|---|
| v1 | 2026-10-15 03:39–04:05 | 07d7391f… | First pass: registry-driven designer with the four house animals (Lion/Gryffindor, Serpent/Slytherin, Eagle/Ravenclaw, Badger/Hufflepuff) as original geometric stylizations; grouped scrollable registry picker; data-driven dials with per-creature memory + shared placement dials (scale/spin/line weight) and a global halo dial; one mutation funnel; feature-point click → governing dials + scroll wheel; trace mode with point-to-point connection, de-echo window, consumed feature clicks; auto-fit view; docked layout (canvas left/top, panel right), debounced resize; live specs + Copy specs; dark/checker preview; SVG + transparent 1200×1200 PNG exports; CLI `--selftest` / `--render` / `--sheet` / `--composite`; `root.editor_api` automation handle. 1,798 lines, single file, pure stdlib. |

## Archives
- Build report: Emblem-Designer-v1-Report-2026-10-15.md (this dir).
- Renders: renders/ — lion/serpent/eagle/badger PNGs, lion.svg,
  contact-sheet.png (2×2), shield-composite-lion.png (the lion
  seated in the canonical Protego Maxima shield).
- GUI smoke: hidden_files/smoke_emblem.py (Xvfb, drives editor_api).

## Roadmap (JD, 2026-10-15)
Support for **64 prime creatures** eventually. The EMBLEMS registry
in the file is the single extension point: one entry (builder + dial
table + defaults + vertex table) per creature; the picker, dial
panel, specs and exports are generated from the registry, and
per-creature state is a dict keyed by creature id, unbounded. No UI
surgery per creature.

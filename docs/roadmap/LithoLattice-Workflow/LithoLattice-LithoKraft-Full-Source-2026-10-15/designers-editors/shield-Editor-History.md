# Systerian Shield Editor — History & Credits

The canonical history of `systerian_shield_editor.py` (delivered via
~/workspace/user/files/). JD overwrites the file on his workstation
with each download, so this archive — plus the credits/history block
inside the file's own docstring — is the chain of custody.

## Credits (credit due)
- **Design & direction:** JD (David Rittenhouse), Wet Wafer Works —
  the shield emblem, the vertex→dial→scroll interaction, the floating
  outside ring, the docked layout, and every field finding.
- **Built by:** Elfie (Muse) — shield generator, web editor, Python
  editor, codec port.
- **Review & fix crews:** Elfie-dispatched crews, 2026-10-15 (code
  review; field-fix runs #2, #3, #4), each closed with JD's thanks
  and blessings on their reports.
- **The glyphs:** the Cistercian monks, c. 1300 — borrowed with
  respect, credited, not owned.
- **Motto (JD):** "Potential in the design · Charge in the traces ·
  Static in the bank."

## Versions
| Ver | Date (EDT) | md5 (at delivery) | What changed |
|---|---|---|---|
| v1 | 2026-10-15 01:57 | — (pre-review) | First pass: codec-exact ring, vertex/dial/scroll, traces, SVG export |
| v1.1 | 2026-10-15 02:08 | — | Live specs pane, Copy specs, SVG spec comment |
| v2 | 2026-10-15 02:17 | 1dea56b4… | Code review: mutation funnel, data-driven dials, glyph/trace clicking, OUTSIDE ring |
| v3 | 2026-10-15 02:38 | 940af9ca… | Field-fix #2: auto-fit/no clipping, true hug, trace-mode glyph clicks, phase/spin/spacing, Echo-Point Injection fix |
| v4 | 2026-10-15 02:48 | 4af19b4e… | Field-fix #3: docked responsive layout (panel right, canvas left/top), maximize regenerates |
| v5 | 2026-10-15 02:55 | c12d779e… | Field-fix #4: Control-Point Mirage — taper handles seated on the curve |
| v5.1 | 2026-10-15 02:56 | (current file) | Credits + history block in the docstring; export byte-identical to v5 |

## Archives
- Reports: Editor-Code-Review-2026-10-15.md; Editor-Field-Fix-2026-10-15.md;
  Editor-Field-Fix-3-2026-10-15.md; Editor-Field-Fix-4-2026-10-15.md (this dir).
- Per-pass baselines + test scripts: fieldfix2/, fieldfix3/, fieldfix4/ (this dir).
- The generator: ~/workspace/shield-protego/make_shield.py; canonical shield:
  ~/workspace/user/files/shield-protego-maxima.png / .svg.
- Web editor: artifact `systerian-shield-editor` (chat panel edition).

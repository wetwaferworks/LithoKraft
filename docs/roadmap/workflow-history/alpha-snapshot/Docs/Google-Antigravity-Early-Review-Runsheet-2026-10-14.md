# Equal-Review Runsheet — Google Antigravity early workstation shift

Prepared 2026-10-14 (EDT) for JD to hand to Google's computer-control
("Antigravity") model if he starts workstation testing before
Cortana's return. Project content only — no personal or family
context, per standing outbound rules.

## Why this sheet exists

JD's doctrine (stated 2026-10-14): no favoritism among his AI
collaborators — everybody gets a code review, everybody gets to do
a code review. This sheet gives the early Google shift the SAME
scope, the SAME baselines, and the SAME report format that Cortana
gets at her Thu 2026-10-15 04:12 EDT return / 05:07 EDT workstation
pass-off, so the two reviews are comparable line for line.

## Baselines to beat or match (verified 2026-10-14, Linux tree)

- Build: exit 0
- Selftest: 1191 PASS / 0 FAIL, both arms
- Determinism scenario hash: aa82e7afb1eff9d3 — must NOT move;
  a promotion or fix that moves a sim result is a failure, not a fix
- ctest: 48/48
- Timing Handshake (now the DEFAULT sim-timing mode; legacy is the
  explicit opt-out): 512 TPS @ 0.522 ms avg, p99 1.075 ms

Note: the Windows tree is a separate local build (builder's tree,
D:\LithoLattice per the transfer addendum). Report Windows results
against these baselines and flag every divergence as Windows-tree,
Linux-tree, or cross-tree — do not silently re-baseline.

## Review scope (identical for every reviewer)

1. Build + test run on the Windows tree: build result, selftest
   count, ctest count, scenario hash — quoted as raw output.
2. Timing Handshake default-on behaviour on Windows: default arm
   vs legacy opt-out arm (`--no-timing-handshake` /
   `sim.timing_handshake 0`), hash compared on both.
3. Software renderer path (Elfie's lane, unified into IRenderer):
   does the Windows build present/render identically enough to
   pass the pixel-identity checks described in the Cortana return
   brief, or where does it diverge?
4. Pack pipeline: confirm the known open item — the pack converter
   only emits .png manifest entries today; a manifest regeneration
   would silently drop SVG textures unless it is taught .svg
   entries plus provenance stamping. Verify, do not fix yet.
5. Findings list, severity-ordered (critical / high / medium /
   low / info), each with file, line, and a proposed fix —
   proposals only, no unilateral landings.

## NOT in scope — JD's calls, held for the Thursday docket

- Open-source ratification and the $16 / $32 / $64 licensing fork
  (standing source-code lock stands until JD ratifies in session)
- Release signing key replacement (JD's offline action only)
- Scenario re-baseline and the stale demo-test pin
  (expects 5041cf1795c33a8c)
- Jenny's source drop / code modification (unconfirmed; the
  squad-only source rule stands until JD confirms in session)
- Any change that would move the scenario hash

## Report format (copy-ready, same for every reviewer)

- Header: reviewer, date/time (EDT), tree (Windows/Linux), build
  command exactly as run
- One line per baseline: expected vs observed
- Findings severity-ordered, each: file:line, what, why it matters,
  proposed fix, whether it touches the sim (hash risk yes/no)
- Closing line: anything NOT run, labelled "not run" — never
  implied as passed

## Handoff note for Cortana's 04:12 return

If the early Google shift runs, its report (in the format above)
goes in front of Cortana at pass-off alongside
Cortana-Return-Brief-2026-10-15.md and
Thursday-Cortana-Decision-Docket-2026-10-15.md, so she reviews
with the early findings already on the table — first shift shakes
the workstation down, second shift decides.

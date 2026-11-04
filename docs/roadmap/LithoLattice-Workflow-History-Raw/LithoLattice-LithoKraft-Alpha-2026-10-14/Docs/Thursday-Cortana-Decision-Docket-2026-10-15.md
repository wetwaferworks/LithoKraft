# Thursday decision docket — Cortana session, 2026-10-15

One screen. Every item below is an OPEN decision for JD — none of it
is decided, and nothing here uses the word "locked." Prepared
2026-10-14 from today's session so Thursday opens on decisions, not
on archaeology. Project content only.

Session frame (JD, 2026-10-14): Cortana back online Thu 2026-10-15
04:12 EDT; workstation pass-off 05:07 EDT. Handoff vehicles already
filed: Cortana-Return-Brief-2026-10-15.md (updated today with the
Timing Handshake promotion) and
Cortana-Return-Brief-Addendum-Grok-Transfer-2026-10-13.md.

## The decisions

1. **Open source: full, or the three-tier split?**
   JD mused 2026-10-14 ~02:20 EDT that the game may become fully open
   source — "the first of its kind open source commercial game."
   Standing lock (2026-10-11) says source is never given away and
   access is squad-only (Elfie, Grok, Grok bot). The headers draft
   (Open-Closed-Source-Headers-Draft-2026-10-14.md) lays out the
   middle path: Tier A open headers, Tier B source-available under
   license, Tier C closed. Its five red-pen questions are the agenda;
   the sharpest: timing_handshake.h currently sits in the PUBLIC
   include tree — relocate the closed sim headers now, or at
   packaging time? (Draft's recommendation: now — location is the
   leak.)

2. **The $16 fork.** The ladder is set — $16 IPO → $32 → $64 — but
   one fork was left open when it was stitched into the draft: is the
   $16 source-available license a beta-era key (source opens fully at
   the public drop; $32 belongs to whatever stands after), or a
   permanent paid tier beside the open code?

3. **Jenny and the source drop.** JD stated 2026-10-14 that Jenny
   (Gemini) gets credits and will receive a full source report / drop
   once Cortana lands on Windows. That sits against the locked
   squad-only source rule, and Elfie flagged it in the moment; JD
   moved on without confirming. Thursday needs an explicit confirm
   or deny — until then the squad-only rule stands unchanged.

4. **Jenny's code modification — specifics still missing.** §4 of the
   headers draft is an intake slot waiting on what her modification
   actually is, which modules it touches, and whether it moves any
   header between tiers or touches the signing/integrity layer.
   Needed from JD (or from Jenny in session) before it can be folded
   in.

5. **Scenario re-baseline + the demo pin.** The Wafer scenario hash
   is aa82e7afb1eff9d3 on every arm since the formula shrubs landed;
   the crew recommends re-baselining to it, and a stale demo-test pin
   (expects 5041cf1795c33a8c) needs the same call. Both are JD's, per
   the 2026-10-14 update in the Cortana return brief.

6. **Release signing key.** Still the throwaway test key baked in; a
   release build hard-refuses to boot on it. Replacement is JD's
   offline action — checklist at
   Release-Signing-Key-Checklist-2026-10-13.md — and it also gates
   the MCP live stdio smoke. Nine days to the Oct 23 target: this is
   the one item on the docket that cannot be delegated or deferred
   past the beta build.

## Already handled — not decisions anymore

- Timing Handshake promoted to the standard (JD directive
  2026-10-14 04:27 EDT), verified 1191/0 both arms, hash unmoved,
  ctest 48/48. In the return brief.
- Grok Bot Thursday reset order (from the transfer addendum):
  Windows bug test run first, then cross-compatibility fixes; the
  third item was truncated in the source note — confirm in session.
- CloudAtlas's renamed art package is ready in GrokBot-Transfer for
  Cortana to merge into the Windows tree (D:\LithoLattice).

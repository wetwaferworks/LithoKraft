# Bolt Cortana V1 — Roadmap

_Decisions by JD, 2026-11-01. Research: RESEARCH_Bolt_Cortana_V1.md (Elfie's report, regenerated 2026-11-01 evening)._

## Changelog
- **2026-11-01 (evening):** Research replaced with Elfie's regenerated report (old kept as RESEARCH_Bolt_Cortana_V1.orig.md). Name's final word changed Anchored → **Analyzer** (JD). Added purpose (in-game LithoKraft companion), co-play learning, consent spine, expanded calibration workstream, and release order (Bolt PCHIE ships first). Previous roadmap kept as ROADMAP_Bolt_Cortana_V1.orig.md.
- **2026-11-01:** Initial roadmap and locked decisions.

## Purpose
Cortana is the in-game companion of LithoKraft. She plays alongside the player and helps them learn the Trace (how signal moves, how components combine, why a circuit behaves as it does). She **never builds for the player**, and she can always be **muted or dismissed**. She speaks through three primitives: **Choice**, **Score**, and **Noul**.

## Name
**C.O.R.T.A.N.A** = Calibrated, Options, Replayable, Tokenized, Auditable, Neural, **Analyzer** (Analyzer replaces Anchored, JD 2026-11-01 evening).

## Locked decisions
- **Scaffold:** built on Bolt PCHIE; keeps every Bolt PCHIE feature (hash-chained tokenizer, sealed reasoning steps, replay).
- **Thinking levels:** Cortana V1 (System One decision head) is the top thinking level; Bolt PCHIE's existing levels are the fallbacks when confidence is below policy threshold.
- **Weights:** Nemotron weights plug directly into the Cortana V1 level. License cleared by JD for private use.
- **Release order:** Bolt PCHIE (Peachy) ships first; Cortana is then trained on it, on better hardware.
- **Recipes:** prototype with Recipe 1; ship with the Recipe 2 head.
- **Numbers stay in deterministic code:** numbers, dates and arithmetic are never produced by the model.
- **Claim discipline:** marketing line stays "the harness hash-chains every step so a hallucination cannot pass as fact."

## Learning from co-play
- Each co-play decision is recorded as a graded tuple: **state hash, question, options, decision, confidence, outcome**, and chained.
- Training happens **only between versions**. Weights are pinned and hashed per version; a live run is **never mutated**.

## Consent spine
- Co-play data is collected from **adults only**, with consent; COPPA-compliant.
- Players who decline still get the **full companion**.

## Phases
1. **Prototype (Recipe 1):** read option logits off Nemotron; Choice / Score / Noul primitives; no generation.
2. **Chain the decision tuple:** state hash, question, option set, decision, distribution, confidence, outcome, weights hash.
3. **Threshold policy in code:** above line acts; below line escalates to Bolt PCHIE fallback levels / human; escalation chained.
4. **Calibration workstream (from day one):** soft labels, temperature scaling, conformal prediction sets, a co-play outcome ledger, and a **gold decision set before ship**.
5. **Shipping head (Recipe 2):** NLI-style decision head inside the micro-model curriculum.
6. **Long term (Recipe 3):** from-scratch scorer for full ownership.

## Open
- Which Nemotron size for the top level (fits 4 GB ceiling, or top level allowed larger).
- Measure accuracy/speed on our own gold data before any claims.

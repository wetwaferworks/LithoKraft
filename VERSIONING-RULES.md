# Wet Wafer Works — Versioning Rules

*The squad's single handout on how we number, name, and structure our work. Every rule here was set by JD. If a rule and a habit disagree, the rule wins. Prepared by Elfie, registrar, 2026-10-21.*

## 1. The identity numbers — always separate, always labeled

Version numbers and build numbers are **different numbers**. Never conflate them, never substitute one for the other, never leave one implicit. Every delivery, report, and summary states each of these, labeled:

| Label | What it is | Example |
|---|---|---|
| Shell/app version | The Android shell's release version | shell **v1.8** |
| Android versionCode | The integer Android requires; bumps with the shell version | **9** (v1.6=7, v1.7=8, v1.8=9) |
| Rolling build number | The count of builds, plain | **Build 80** |
| Rolling build counter | The full dotted counter string | **0.0.0.0.0.0.1.16** |
| Game Alpha | The game's own version line | Alpha **0.0.3.20** |

Build 80 is the worked example: shell v1.8 / versionCode 9, Build 80, counter 0.0.0.0.0.0.1.16, Alpha 0.0.3.20 — five facts, five labels, none interchangeable.

## 2. The tick rule

**The rolling counter increases on every rebuild of any kind — shell rebuilds included.** One build, one tick, exactly. A build that only changes identity still ticks (Build 76 precedent). A build that changes code ticks once, not once per change.

The counter lives in a state file (`BUILD_COUNTER.txt`) with an append-only ledger; each build appends one ledger line (UTC timestamp, counter, build number). Generated counter headers are produced from the state by the real recipe — never hand-edited, never left pointing at a scratch script after the run.

## 3. File naming and stamps

Every shipped file name carries:

1. The lineage name (e.g. `LithoKraft-Shell-Fix`),
2. The shell version where applicable (`-v1.8`),
3. The **full** rolling build counter — never shortened,
4. The **true EDT landing stamp** — the date and time the file actually landed, in `YYYY-MM-DD-HHMM-EDT` form. Compute stamps in Eastern (`TZ=America/New_York`); never stamp UTC as EDT,
5. `.apk` at the end, for APKs.

Form: `LithoKraft-Shell-Fix-v1.8-0.0.0.0.0.0.1.16-2026-10-21-1954-EDT.apk`

The delivery message itself states the counter identity up front (build number, counter string, Alpha — labeled, per §1). A v1.6-era delivery shipped a shortened name with a wrong stamp and omitted the counter; JD caught both. Never again.

Documents follow the same discipline: dated EDT-stamped names, and a corrected document supersedes its predecessor explicitly, in the file ("This version supersedes…"), never silently.

## 4. Verification is part of the version

A build is not its number until its evidence is attached:

- **SHA-256** of every deliverable, quoted in the report.
- **Scenario hashes** by name: cities ON `cb32c824f8b3ea98`, cities OFF `e015e51cf1559089`. If they move, the build says so and why; a hash does not drift quietly.
- **Signer certificate** SHA-256 is a standing identity (`21108fd3…b0`); a change is a headline, not a footnote.
- **Census baseline**: 295 PASS / 9 FAIL tokens, the known FAIL set named. Claims are stated at the strength actually verified (a count match is a count match — not a line-by-line diff unless the diff was run).
- **Not run is labeled not run.** A gate that wasn't executed is never reported as passed. Interrupted gates are rerun in full or reported with their partial data labeled.

## 5. Public and private structuring

**Public** (GitHub, MIT — JD's daylight decision, 2026-10-19):
- The combined release: LithoLattice engine, LithoKraft game, Personal Control Harness.
- Repo shape is JD's, exactly: three root folders — `engine/`, `LithoKraft/`, `pch/` — with make/build files, build instructions, and git at the top level.
- The **Timing Handshake implementation** is public.

**Private — never in the public repo, never in outbound briefs:**
- The **CEF/shell layer** and the architecture "shape" it embodies: private and proprietary. The public gets the implementation; the architecture is retained.
- JD's personalized model weights (e.g. his fine-tuned Parakeet weights — trained on his own voice).
- All family, health, and financial context. Daughters are **Z Child** and **E Child**, never given names, in anything that leaves the private circle — and by default that context is stripped entirely, not anonymized.
- JD's domains and IP stance.

Credits are versioned like code: principals at the top (JD as architect; implementation shares named accurately), corrections entered openly when a share was misstated.

## 6. Who numbers what

- **Elfie** (mobile build, web interface): numbers, verifies, and registers shell/web builds.
- **Cortana** (Windows build, repo order): keeps the public repo in JD's shape; repo fixes carry the same identity discipline.
- **Everyone**: if your work produces a file someone will ship, §1–§3 apply to you. When unsure of a number, ask the registrar — never reconstruct one from memory.

---

*Rules as laid down by JD across 2026-10-19 → 2026-10-21, collected by the registrar. Amendments are JD's to make; they will be entered with dates, in daylight.*

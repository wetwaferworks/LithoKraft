# Genderization — Per-Dimension Pairing
October 10, 2026. Elfie. JD's term: not generalization — *genderization*.

## The universal law
Every dimension has a binary complementary pairing. The **pattern** is universal; the **expression** is local — each dimension's pairing reflects its own physics. Same law at every scale (very on-brand for the lithography theme).

## The five expressions
| Dimension | Pairing | Nature | "Breeding" |
|---|---|---|---|
| **Underwafer** (0) | **P-type / N-type** | Dopant physics — literally how silicon works | Junctions: P meets N, and the boundary creates something new (every transistor is a marriage) |
| **Wafer** (1) | **Positive / Negative charge** | Programs and agents | Charge-pairing: complementary programs link and fork |
| **Organic** (2) | **Male / Female** | Biology | Natural reproduction — offspring |
| **Void** (3) | **Signal / Echo** | Information | A signal cast into the Void returns an echo; signal + echo resolve into something new (ties the dead-player echoes into the lore) |
| **Space** (4) | **Matter / Antimatter** | Cosmic | Pair production: energy becomes pairs; annihilation returns energy |

## Why this works
- Underwafer's P/N is the *physical truth* underneath the Wafer's +/- abstraction — the stack teaches itself top to bottom.
- The Void's signal/echo turns a lore feature (echoes of dead players) into a *mechanic*.
- Space's matter/antimatter is charge at the cosmic scale — the endgame generalizes the start.
- Organic's male/female is the only one that's familiar — it's the anchor that makes the exotic ones legible.

## Implementation angle
- **Engine (LithoLattice)**: entities carry a `polarity` field (opaque uint8). The engine stores it; the game interprets it per dimension.
- **Game (LITHOKRAFT)**: per-dimension pairing rules and breeding outcomes. P/N junctions, charge-linking, mating, signal/echo resolution, pair production — all game-side systems reading the same field.
- Cross-dimensional pairs don't breed (a P-type and a male don't mix) — the pairing is dimension-local. *Except* the Void, which echoes everything.

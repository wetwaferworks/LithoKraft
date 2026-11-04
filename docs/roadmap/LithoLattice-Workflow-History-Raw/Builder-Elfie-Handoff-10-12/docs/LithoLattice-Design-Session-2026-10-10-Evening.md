# LithoLattice / LithoKraft Design Session — Evening 2026-10-10

Design session between JD and Elfie. What got landed:

## Locks
- **"Organic World" renamed to "Organic"** (one-name convention; it was the only dimension with "World" in its name).
- **Organic bound: ±196,608** (393,216 wide, area 154,618,822,656 = 9×2³⁴) — the sweet spot between Underwafer (±131,072) and Space (±262,144): both the arithmetic midpoint AND Underwafer + Wafer (131,072 + 65,536).
- **World sizes (LithoKraft):** Wafer ±65,536 · Underwafer ±131,072 · Organic ±196,608 · Space ±262,144 · Void infinite. Every dimension bounded except the Void.
- **Engine principle:** bounds are game-level (LithoKraft) constraints, not engine limits. LithoLattice supports infinite/unbounded generation as far as mathematically possible; games apply their own borders on top.
- **Space verticality:** full 768 total height (256 solids + 512 of **the Empty**, the named vacuum medium, instead of the Charge atmosphere/air).
- **Hypervisor required in Space** — "isolation" is the helmet's theme (Void mind-attacks, the Empty alike); gates Space behind the Underwafer's Sear.
- **Space physics/traversal:** lightened or zero gravity; **magnetic boots** (feet gear) for sticking to surfaces while building; the Organic visible below as an overview; **falling/jumping from Space drops directly into the Organic** — descent free, ascent needs rocket/starship (unique to Space; other dimensions use interfaces).
- **Interface fast travel (Nether-style):** interfaces translate X/Z by exact bound ratios — from Wafer ×2→Underwafer, ×3→Organic, ×4→Space; from Underwafer ×1.5→Organic, ×2→Space; from Organic ×4/3→Space (reverse divides). Edges map to edges, integer math, deterministic-sim clean. The Wafer (smallest) is the fast-travel hub.
- **Interface mechanics:** paired by proximity like Nether portals; pads lie **flat on the XY (ground) plane** — jump inside/down to enter, jump up to exit. Vertical (standing) orientation parked for later. In-world slang: **"interface jumps"**.

## Proposals (not yet locked)
- **Space = "an expansion of the Signal."** Two tiers: (1) **Recons** — cheap 1-block + 1-raptor-engine satellites, unmanned launch, each giving partial signal boost (staged); (2) **Lattice Bridge** — 6 lattice-point satellites (corrected from 8), deployed in person via starship at specific coordinates; all 6 placed → full boost → Signal transmits across all dimensions → Ether unlocks. Elfie suggested an octahedral layout (±X, ±Y, ±Z).
- **Raptor engine as a single lithographized block** (build big → print small → fly) — the namesake mechanic powering the endgame.
- **Ether access** via an Ether item/interface in the Organic (interfaces, not portals) — ties into Fragment restoration via the Ether.
- **Dimension stack arrangement:** single tower Space → Organic → Wafer → Underwafer → Void. Known links: Space→Organic by falling, Wafer→Underwafer via bottom portal, Void below Underwafer. Organic↔Wafer adjacency assumed.
- Recon-as-survey-camera idea (recons feed the orbital overview of the Organic) — offered, not decided.

## Open questions
- Landing mechanics of the Space→Organic drop (damage? controlled?); does x/z carry over?
- Y-coordinate handling on interface translation.
- Recon caps / diminishing returns; what the Hypervisor does about breathing (recycle air vs no need).
- Coordinate discovery method for the 6 lattice points.
- Ether / Undefined placement relative to the tower.

## Graphics (saved alongside)
- `LithoLattice-Dimension-Grid-Wafer-Underwafer-2026-10-10.html` — to-scale 2D XY grid, Wafer nested in Underwafer (drawn before Organic/Space bounds locked).
- `LithoLattice-Dimension-Stack-2026-10-10.html` — full cosmology tower, widths to scale, final bounds.

JD: "You're going to get a very good credit line in the history and documentation of the creation of all this stuff."

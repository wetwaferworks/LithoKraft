# Joint Review — Decision Sheet (Wiring Teams A + B, 2026-10-13)

_One screen. Everything the settled tree left for JD, distilled from the two team reports. Nothing here is decided — each item is JD's call, with the evidence and the cost of each option._

**Settled-tree state (both teams' final batteries agree):** build exit 0 · selftest **1184/0** (Team A battery; Team B's coordinator run read 1179/0 default + the known audio flake in handshake mode) · ctest **48/48** serial · **512 TPS** both modes · scenario hash `d609126cc60d71c7` identical across legacy / 2-worker / 4-worker arms.

Sources: `Wiring-Team-A-2026-10-13.md`, `Wiring-Team-B-2026-10-13.md` (both in `~/workspace/user/files/`).

---

## Decisions, in the order they block things

### 1. The Void — lock before any Void save exists ⛔ time-sensitive by design
The Void was **not** registered: `packPos` packs x/z into 21 bits (±1,048,575); beyond that, coordinates **alias silently**, merging unrelated cells' signal/light/dirty state — the worst failure mode under world permanence. Four options (Team B report §B5):

| # | Option | Permanence cost | Design cost |
|---|--------|---|---|
| 1 | **Finite bound inside packPos range** (e.g. ±262,144 like the Cosmic, up to ±1,048,575) | **Zero** — no key/save/migration changes; a Void world is an ordinary save from day one | "Infinite" becomes "the largest dimension" — a design change, JD's call alone |
| 2 | Per-dimension wider key for the Void only (62-bit: 26/26/10) | Contained but real — forks every consumer of the keyed sets; future format unification becomes a migration | Infinity still not represented — aliasing wall just moves to ±33M blocks |
| 3 | Region/epoch virtual coordinates (floating origin) | **Must be locked before the first Void save exists** — retrofitting rewrites every persisted coordinate in every Void save | Only option preserving true unbounded extent; every coordinate consumer (render, MCP pilots, netcode, queries) must speak virtual coordinates at the Void boundary |
| 4 | Universal structured key replacing packPos everywhere | **Worst** — re-keys persisted state in existing Wafer/Organic saves; versioned migration of live worlds | Not recommended by Team B; against the grain of the permanence doctrine |

**Team B recommendation:** Option 1 if a finite Void is design-acceptable (free forever). Option 3 if infinity is load-bearing — and then the epoch/window format gets locked *now*, while no Void content or saves exist, because that is the cheap moment.
**Why it can't drift:** options 3 (and 2, partially) get more expensive the moment any Void save exists. The other decisions below can wait; this one's price only goes up.

### 2. Scenario hash — re-baseline, but bisect first (procedure below, no JD decision needed to run it)
Hash moved `faec0f02700d98c6` → `d609126cc60d71c7`. **Determinism never blinked** — identical on all arms at the new value. The teams dispute attribution: B credits A's A5 density table; A credits B's content wiring. **The timeline both reports agree on:**
- ~20:00Z — A's port-time gates read `faec…` in both modes.
- ~20:05Z — B's Worker 2 read `faec…` on a binary already containing B3+B5+B6/B7 (B's content wiring present, hash unmoved).
- ~20:45Z — B's run first measured `d609…` on the settled tree.

That timeline already weakens the "B content" theory (B's content was in at 20:05 with the old hash) — but timeline is not proof. **Bisection, two runs, settles it:**
1. On the settled tree, revert only the A5 density hunk in `blocks.cpp` (pre-wiring snapshot: `~/workspace/backups/litholattice-pre-wiring-2026-10-13/`) → run `--handshake-scenario 2048`. If the hash returns to `faec…`, A5 is causal. If it stays `d609…`, A5 is exonerated.
2. Whichever way run 1 lands, restore and (only if run 1 exonerates A5) bisect B's lanes the same way.
**Then JD's call:** re-baseline to `d609…` once causal attribution is known and content is declared final — and in the same step, repin the one stale constant in `demo_record_test.cpp` (`5041cf1795c33a8c` → `d609126cc60d71c7`; the `--demo-test` 11/12 sole FAIL). Repinning before the bisection would paper over the one open question.

### 3. Shrubs bridge — JD veto window (Team B §B4)
Shrubs were bound into worldgen via the **data-asset bridge**, chosen as least-invasive and explicitly flagged for JD's veto. Frames were read and confirmed — tufts generate in terrain. **Call:** keep the bridge, or name a different one. No cost to deciding later except that worldgen work builds on it meanwhile.

### 4. Certificate signing — costs are placeholders (Team B §B7)
Clarity / Knowledge / Efficiency / Mining Haste are defined; the proposed static curve was implemented exactly; signing is fail-closed. **All costs are conservative placeholders, not final.** Also open from the same item: the table's SimRole was deliberately left `None` (reasoning in B7), the **table UI is JD's design**, and the **lapis-analogue item is still OPEN** (no catalyst is consumed today). Blank wafer: Wafer Silicon is a stand-in until the AutoFabOR-stamped blank exists (B6).

### 5. Projectile densities — balance values, not approved (Team A §A5)
All 65 blocks carry densities from a material-class table built because no JD-approved table existed: non-solids 0, glass 10, earth 20, trace 20/25/30, silicon 35/45/60, stone 40/50, machines 45, ChipOR + viewport 70, ceramic 80 (full table in Team A report). Marked in code as **JD-DECISION (balance review)**. **Call:** approve, adjust, or replace — it's a tuning pass, not a blocker. Note item 2: this table is also the prime suspect in the hash move, so reviewing it and bisecting it are the same work session.

### 6. Two small factual flags (Team B §B5 discrepancies)
- The Wafer bound **±65,536 is not corroborated in-tree** (it comes from the brief/design docs). Bounds are **not per-dimension data today** — they're global. **Call:** confirm ±65,536 as the design number, and decide whether bounds should become per-dimension data.
- World height is the engine constant `kWorldHeight = 768` for every dimension; the Cosmic's "256 solids + 512 Empty" is expressed through content, not a per-dimension height.

### 7. AiWorld — empty by design (Team A §A6)
Production host exists, gated by `ai.enabled`, **default OFF**, starts empty. **No mob content was invented** — what spawns is JD's scoping call, whenever he's ready. No deadline.

---

## Explicitly NOT on this sheet (reserved and untouched, per both reports)
Timing Handshake default stays **OFF** · H2/net protocol stays **{3}-only, fail-closed** · signing keys untouched (release key replacement remains JD's offline call from the earlier checklist) · Fragment UI's 4 decisions untouched, quoted verbatim in Team A's §A3.

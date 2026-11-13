# The Lithography Machine (mask-and-expose), 2026-10-15

Cortana 2026-10-15 1900, following JD's wiring canon. The code is in `game/LithoKraft/src/game/lithography.{h,cpp}` (the scan), `game.cpp` (the `etch`, `etchscan` and `etchload` commands) and `content/litho_scene.cpp` (the test machine).

## Canon
- **EtchOR in the centre.** It faces its FRONT, where the named chip exits.
- **Top half (EXPOSURE):** 3 ConnectORs in a row, 2 blocks above the EtchOR. A **blue Vior** (the existing blue Via) sits between them and the EtchOR.
- **Bottom half (MASK):** 5 ConnectORs in a row, 2 blocks below the EtchOR. A **red Vior** (the existing red Via) sits between them and the EtchOR.
- **Lasers enter from the sides.** On the machine's left and/or right, a powered **Emitor** faces inward through one or more **Laysors** and ends at an outer top ConnectOR. The Emitor is a Lamp: it lights on adjacent power (a torch-powered block, Trace, a switch or a Repeater).
- **Feeding:** every ConnectOR is fed by a **strongly powered** neighbour: a block with a lit torch under it, a block with an on switch attached, or the block a lit Repeater faces. Structure blocks never count as feeders. The exception is the **middle top ConnectOR, which takes Trace dust**: lit Trace laid on top of it.
- **Wafer:** an **Input Buffer** directly BEHIND the EtchOR, its front facing the EtchOR, holds the wafers (up to 64; `etchload` loads them from your inventory).
- **Expose:** a laser reaches the top row AND at least one top ConnectOR is fed.
- **Mask:** the fed pattern of the 5 bottom ConnectORs.
- **Chip code:** the 3 top bits plus the 5 bottom bits shifted left by 3 (0 to 255).
- **Etch** (`etch <name>` within 6 blocks of the EtchOR): the machine must be formed and exposed, have a wafer in the back buffer, and have air in front. It consumes one wafer and the named ChipOR exits the front. This pass the chip goes into your inventory, because the game has no dropped-item entities yet.

## Front view (looking at the chip-out face)
Left and right are the MACHINE's left and right (mirrored to you). F = feeder block, which sits one block in front of its ConnectOR (toward you).

```
          col:  -3      -2      -1       0       1       2       3
 y+3                                   [Trace]                          <- lit Trace dust on the middle top ConnectOR
 y+2         [Emitor>][Laysor][ConnOR][ConnOR*][ConnOR][Laysor][<Emitor]   EXPOSURE row (lasers enter from the sides)
 y+1         [ pwr  ]                 [blueVia]                 [ pwr  ]   (block under each Emitor = its power block)
 y           [ pdst ]                 [ETCHOR ]                 [ pdst ]   front face = chip out
 y-1                                  [redVia ]
 y-2                 [ConnOR][ConnOR][ConnOR][ConnOR][ConnOR]              MASK row (bits from left: -2 .. +2)
```

## Side view (machine's right side; front to the right)
```
             back                 front
 y+3                     [Trace]  [ F ]     F = torch-powered block feeding the Trace
 y+2                     [ConnOR] [ F ]     F = feeders of the outer top ConnectORs (torch below each)
 y+1                     [blueVia]
 y     [InputBuffer>]    [ETCHOR] [air]  -> named chip exits here
 y-1                     [redVia]
 y-2                     [ConnOR] [ F ]     F = mask feeders (torch below = bit on)
```

## Back view (looking at the wafer-in face)
```
 y+2         [Emitor>][Laysor][ConnOR][ConnOR][ConnOR][Laysor][<Emitor]
 y+1                                  [blueVia]
 y                                    [InBuffer]   <- wafers in (front faces the EtchOR)
 y-1                                  [redVia ]
 y-2                 [ConnOR][ConnOR][ConnOR][ConnOR][ConnOR]
```

## Test scenes (hands-on)
- `--bench-scene litho` builds one UNWIRED machine 10 blocks south of the spawn, with the EtchOR facing you.
  - All canon blocks and feeder blocks are in place, and the back buffer holds 8 wafers.
  - There are no torches under the feeders or Emitors and no Trace.
  - A row of 6 red torches on the floor at the front edge is your power source to wire from.
- `--bench-scene litho-wired` builds the same, plus a WIRED reference copy 16 blocks east.
  - Mask 10101, both lasers lit, middle top fed by Trace.
  - `etchscan` there reads `formed; laser LR, expose(top)=111, mask(bottom)=10101, code=175, wafers=8`.
- Trace (red/blue), torches, Repeaters, switches and all lithography blocks are in the creative inventory (the palette lists the whole registry).

## Upgrade tier: wax and coolant (APPROVED, NOT IMPLEMENTED)
Status: **approved as proposed by JD, not implemented, waiting until the rest of the mechanics are worked out.** There is no code for it.
- The Trace wired into the SelectORs and ConnectORs must be **waxed** (insulated).
- The whole machine is **submerged in coolant liquid**.
- Being filled with liquid is the reason the lithography machine **cannot be shrunk or miniaturized**.
- **Open question for JD:** what happens without wax or coolant (the failure behaviour) is not decided.

## Open items
- **Where the chip lands:** it goes into the player inventory, not as a dropped item out of the front (no item entities yet).
- **The chip's code is not stored on the item:** ItemStack has no data field for it, and adding one changes the save format, so that is JD's call. The code is shown in the etch message.
- **SelectORs are not in the machine yet:** they are placed but not read. The canon names ConnectORs for the bits; JD to say how SelectORs join (recipe choice).
- **Wafer loading:** wafers enter the back buffer through `etchload`; automatic hopper-style feeding does not exist yet.
- **Beam glow at chunk seams:** the glow still stops at a 16-block sub-chunk seam. The mesher only sees one block past its sub-chunk, so carrying it across needs beam data in the mesh input: not cheap, so not done. The etch LOGIC is not affected (the scan reads the world directly, up to 16 blocks).
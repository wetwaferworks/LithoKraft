# LithoLattice world save format (v1)

Our own format (no LevelDB). The engine module `litholattice::storage` owns the container and the
block codec. The game registers its own payloads (SDK style). Status of each item is marked
**[v1]** (implemented and tested in `tests/` `storage`) or **[planned]**.

## World directory
`<world>/r.<rx>.<rz>.llr`, one region file per 32 x 32 chunk columns **[v1]**. Region coordinates are
int32 and file names are created only for regions that hold data, so addressing is sparse and
effectively unlimited (about 2^36 blocks a side). The test saves columns 2 billion blocks out.
`<world>/level.json` (seed, dimension list, game metadata) **[planned]**.

## Region file (little-endian)
| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `LLGR` (0x52474C4C) |
| 4 | 2 | region format version (1) |
| 6 | 2 | reserved |
| 8 | 1024 x 16 | index: per column slot (`lz * 32 + lx`) `u64 offset, u32 size, u32 crc32`; size 0 = absent |
| 16392 | ... | chunk records |

Random access: read the 16 KB index, then seek straight to one record. **[v1]** (v1 reads the whole region on first touch and keeps it cached.)

**Crash safety [v1]:** a save writes the whole region to `r.x.z.llr.tmp`, flushes it, then renames it
over the old file. The rename is atomic on one volume, so a crash leaves either the old region or the
new one, never a torn one. A region whose header or index is unreadable is never overwritten: it is
kept for recovery and its saves are refused with an error. **[planned]:** fsync of the file and the
directory, and append-in-place with a small write-ahead journal so one column does not rewrite 1 MB.

## Chunk record
`u16 recordVersion (1), i32 cx, i32 cz, u16 payloadCount`, then per payload
`u32 tag (fourcc), u16 payloadVersion, u32 length, bytes`. CRC-32 (IEEE) of the whole record sits in
the region index; a mismatch reports "CRC mismatch" and that column is skipped. **[v1]**

## Payloads and versioning
- `BLKS` v1 (engine): `u64 presentMask` (bit per non-empty sub-chunk of 48), then per present
  sub-chunk the block ids and the cell states, each **palette + bit-packed** (u16 palette size,
  palette entries, then `ceil(log2 n)`-bit indices in u64 words; a uniform sub-chunk takes 4 bytes).
  The cell states carry LithoKraft's per-cell signal state (value + facing). **[v1]**
- Game payloads: `PayloadRegistry::add(PayloadType{tag, version, save, load, migrate})`. On load,
  an older payload goes through `migrate[v] : v -> v+1` until it reaches the current version. A
  payload newer than the build is refused, never misread. Unknown tags are skipped. **[v1]**
- `SGNL` v1 (LithoKraft, `SignalSim::kStateVersion`): per component in the column, sorted by
  position: `u64 packPos, u8 out, out2, pulse, prevIn, prevSide, prevClk, history, timer, u16
  nKeys, (u8 front, u8 control) x nKeys`. On load, `scan()` re-indexes components from the blocks and
  `importState()` restores their memory. Wire strengths and torch / lamp states are cell states in
  `BLKS`. **[v1]**
- `<root>/player.dat` (LithoKraft): `"LLPM", u32 crc32, u32 len, body{u16 version 1, f32 x y z yaw
  pitch, string dimension, u8 hot slot, u8 n, u16 hotbar x n}`, written with tmp + rename. **[v1]**
- Entities: none exist in the game yet; `ENTS` payload **[planned]** when they do.

### LithoKraft wiring [v1]
Saves live in `<data root>/saves/world/<dimension>/` (`--save-dir`, `--no-save`; `--bench` never
saves). Startup loads the player and its dimension. Each generated column with a saved copy is
replaced by it (loads stream on the I/O thread), then the signal memory is imported. Saves happen
on F5 (Debug context, was free), every 5 minutes (autosave), on dimension switch and on exit
(exit waits for the disk). A save stops the sim thread for the snapshot: the columns and signal
state are encoded on the main thread, and the files are written on the I/O thread.

## Compression
v1 relies on the palette + bit-packing (2 typical sub-chunks: under 6 KB instead of 32 KB raw).
**[planned]:** per-record LZ4 (BSD-2-Clause, fast load) or zstd (BSD-3-Clause / GPL-2.0 dual,
used under BSD) behind a codec byte in the record header. Both licenses are commercial-friendly.
No compression library is linked yet.

## Threading
`WorldStore` runs all file I/O on its own thread. `saveAsync(column)` encodes the column on the
caller's thread (a snapshot, so the caller may keep editing), and the I/O thread writes it.
`loadAsync(cx, cz, callback)` decodes on the I/O thread. `flush()` blocks until every region is
swapped in. **[v1]**

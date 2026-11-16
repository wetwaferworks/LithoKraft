# Nemo View scratch notes, 15 November 2026

Provisional hashes only. Nothing here is locked, promoted, or a golden. Official engine changelog is left alone.

These notes cover three local commits on `nemo-view-2026-11-15`, plus the display-brightness follow-up. Work stayed in the LithoKraft scratch tree. Nothing was pushed or merged.

## How to open the pop-up

From the scratch build folder, not a Play package:

    set LL_NEMO_VIEW=2
    set LL_MCP_NEMO_VIEW=1
    LithoKraft.exe

Optional: `LL_ASCII_GRID=80x45` (or 96x54, 160x90, 180x80, 240x135) picks the preview grid at launch. The window keys 1-5 and [ ] also cycle those sizes. The console settings are `render.ascii_grid` and its alias `render.nemo_view_cols`.

The display default is 160x90. The hashing / record default stays 80x45, so existing 80-column hashes remain valid. MCP `vision.capture_ascii` still defaults to 80, and the fovea is still the centre 128x48 of that 320x180 capture (32x12). The Nemo prompt token budget is unchanged.

## Commit 1 — packed cell (renderer)

Pack each cell into a 64-bit word (`nemo_cell.v1`). Nemo's default prompt is still the glyph grid; the other fields are opt-in.

## Commit 2 — maze key and exit

The PCHIE maze used the Lamp block for both the key and the exit, so the object table labelled the key as a light (`*`). They still look like lamps, but they now have their own ids (`lithokraft:maze_key`, `lithokraft:maze_exit`) and Nemo can show `k` and `E`. Palette hidden. The maze legend log now reports those ids, so PCHIE can keep reading the legend at runtime. No change was needed in the PCHIE scratch tree: its voxel map already follows the legend, it does not hard-code Lamp.

JD accepted that this moves maze and P7 hashes. This LithoKraft tree does not itself pin a P7 golden (the in-engine maze is explicitly unhashed). Official `lithokraft_frame_hash_selftest` still passed after the split, so the regular frame-hash record did not move. Any later PCHIE run that includes those two cells will move; the cause is the key/exit block split, not a lock.

## Commit 3 — selectable grids

Selectable grids, each its own hash format beside 80x45:

| Grid    | Capture (default Nemo / MCP) | Cell size | Role                          |
|---------|------------------------------|-----------|-------------------------------|
| 80x45   | 320x180                      | 4x4       | hashing / record default      |
| 96x54   | 384x216                      | 4x4       | selectable                    |
| 160x90  | 320x180                      | 2x2       | display default               |
| 180x80  | 360x160                      | 2x2       | selectable                    |
| 240x135 | 480x270                      | 2x2       | selectable                    |

The size is a renderer / config option (`nemoview::Grid`, `render.ascii_grid`), not something the Nemo View window invents. A later human-playable ASCII mode can point the same builder at the main LithoKraft window (for example 1920x1080 at 8x8 for 240x135) without changing the pop-up. That path is not wired yet; this only keeps the hook open.

### Fixture hashes (objects mode, provisional)

80x45 is unchanged from the packed-cell renderer. The other four are new formats, not replacements.

| Grid    | sha256 (fixture, objects)                                          | Notes                    |
|---------|--------------------------------------------------------------------|--------------------------|
| 80x45   | 95c90f393878009ae385adf585f891b8b317857f2ccff79f7be0c677ef0989bc | unchanged from renderer  |
| 96x54   | 1e5023fd71dcd0295cba017141da5c446b7937d0aa97bf217540bc1ab9f7b102 | new format               |
| 160x90  | 27d5dfb36aed0b61c0e0d685fffa547950bb26dc35cc211a079d031967c8edab | new format               |
| 180x80  | ee0ea6bff2cb194e2b1ab631e71f278e94a84ef40e403fe608b3898e0fe5becf | new format               |
| 240x135 | 6f2ff0aee98db91852e2f2209b0cdcb5ec8795296864b2e29c167dfb0a4eec2a | new format               |

Other 80-column records, also unchanged from the packed-cell renderer:

- 80x45 pixels+depth prefix: d953eea23039744b
- 40x45 objects prefix: 71513388aedc51e5
- fovea 32x12 prefix: a47b19cc57ab30fc
- rendered scene sha256: 2e050fbcd1b8b1b6649e2245c235496c3ddeab76552ec0565a70ad81e7412034
- depth-band LUT: c4b3f1700c0d9c94 (unchanged)

Build-time speed, single-threaded, same machine as the earlier 0.498 ms 80x45 run:

| Grid    | ms/frame | fail-above |
|---------|----------|------------|
| 80x45   | 0.331    | 5.0 ms     |
| 96x54   | 0.491    | 7.2 ms     |
| 160x90  | 1.146    | 20.0 ms    |
| 180x80  | 1.044    | 20.0 ms    |
| 240x135 | 2.373    | 45.0 ms    |

## Official frame hash

`lithokraft_frame_hash_selftest` passed after the renderer, after the key/exit split, and after the selectable grids. No official frame-hash move was observed in this tree.

# Tick x Streaming Baseline (2026-10-25)

Reference rig: RTX 3050 Ti Laptop 4 GB (minimum spec, JD 7:05 AM ET). Build 861b7ac+ working tree, mode 3 with 4.2a (`LL_VK_MEGA=3 LL_VK_FASTCULL=1`), `--display borderless`, `--bench-scene circuits` (size 120, 19,401 blocks, ticking at spawn the whole time), `--tps N`. Script: `rename-run\mega\tick.ps1`; raw results in `tick_main.txt`. Each 128/256 cell is one 30 s run; 512 is two 60 s runs. Not a video run.

Columns: achieved TPS avg/min (sag = 1 - min/target, per-second samples); tick avg ms and worst per-second tick max; sim budget %; dropped ticks; FPS avg, frame p99, 1%/0.1% lows; hitch count (>2x avg); GPU ms; uploads/frame.

| TPS | Path | TPS avg/min | Sag | Tick avg / worst max | Budget | Dropped | FPS | p99 ms | 1% / 0.1% | Hitches | GPU ms | Up/f |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 128 | flyover r32 | 128.0 / 127.7 | 0.2% | 0.67 / 14.2 ms | 9% | 0 | 154.5 | 8.9 | 102 / 68 | 8 | 6.2 | 1.5 |
| 128 | sprint r32 | 128.0 / 127.7 | 0.2% | 0.87 / 16.3 ms | 12% | 0 | 109.0 | 15.6 | 52 / 37 | 25 | 8.1 | 1.9 |
| 128 | sprint r48 | 128.0 / 125.6 | 1.9% | 2.24 / 34.4 ms | 30% | 0 | 80.0 | 38.3 | 21 / 14 | 74 | 7.2 | 5.7 |
| 256 | flyover r32 | 256.0 / 254.2 | 0.7% | 0.61 / 19.5 ms | 16% | 0 | 153.4 | 10.2 | 66 / 47 | 15 | 6.3 | 1.5 |
| 256 | sprint r32 | 256.2 / 253.1 | 1.1% | 0.91 / 32.5 ms | 24% | 0 | 106.3 | 34.2 | 23 / 17 | 34 | 8.4 | 2.3 |
| 256 | sprint r48 | 255.7 / 232.7 | **9.1%** | 3.14 / 114 ms | **81%** | **29** | **24.1** | 137 | 6.7 / 6.2 | 88 | 10.3 | 14.7 |
| 512 #1 | flyover r32 | 512.0 / 507.2 | 0.9% | 0.38 / 18.6 ms | 20% | 0 | 153.2 | 8.4 | 69 / 42 | 22 | 6.4 | 0.8 |
| 512 #2 | flyover r32 | 512.0 / 507.6 | 0.9% | 0.37 / 16.1 ms | 19% | 0 | 152.1 | 8.5 | 78 / 45 | 23 | 6.4 | 0.8 |
| 512 #1 | sprint r32 | 512.0 / 499.1 | 2.5% | 0.52 / 31.6 ms | 27% | 0 | 106.6 | 24.6 | 26 / 17 | 65 | 8.8 | 1.2 |
| 512 #2 | sprint r32 | 512.2 / 508.5 | 0.7% | 0.49 / 32.2 ms | 26% | 0 | 107.5 | 21.4 | 30 / 16 | 63 | 8.9 | 1.2 |
| 512 #1 | sprint r48 | 511.3 / 473.2 | **7.6%** | 1.45 / 109 ms | **75%** | **49** | **31.6** | 161 | 5.9 / 5.1 | 167 | 11.3 | 8.5 |
| 512 #2 | sprint r48 | 512.1 / 480.7 | **6.1%** | 1.14 / 86 ms | **59%** | 0 | **46.2** | 113 | 7.8 / 6.4 | 164 | 11.3 | 5.8 |
| 1024 | all | not runnable: the sim thread rejects it ("must be one of the supported tick modes (32/64/128/256/512); got 1024") even with `--tps-unlock` | | | | | | | | | | |

Worst frames: flyover about 16-28 ms; sprint r32 about 52-68 ms (mostly in the first 3 s); sprint r48 140-200 ms spread across the run.

## Where it sags
- **Flyover r32 (no streaming):** steady at every tick rate. TPS sag is 1% or less with no dropped ticks, and FPS stays about 152-155 from 128 to 512 TPS. The circuit sim alone is not the limit, even at 512 (19-20% budget).
- **Sprint r32:** ticks hold at every rate (sag 0.7-2.5%, no drops), and average FPS holds at about 106-109. The 1% lows are the first thing to go: 52 at 128 TPS falls to 23-30 at 256 and 512, and the worst per-second tick max doubles (16 to 32 ms). This sag starts at 256.
- **Sprint r48:** this is the break point. Already at 128 TPS, FPS is 80 and the 1% low is 21. At 256 and 512 it collapses: FPS 24-46, 1% lows 6-8, sim budget 59-81%, TPS sag 6-9%, and up to 49 dropped ticks. Per-second tick max reaches 86-114 ms, so streaming work (gen, mesh, upload and eviction) and the sim are starving each other on CPU: tick avg rises from 0.5 to 1.1-3.1 ms with the same circuit load. The two 512 r48 runs differ a lot (31.6 vs 46.2 FPS), so this regime is noisy.
- **Conclusion:** at r32, the reference rig runs 512 TPS circuits plus sprint streaming with steady ticks but a weak 1% low. At r48, any rate of 256 or more breaks both frames and ticks. The next lever is the 4.1 governor (Archie Q5: staging byte cap plus a CPU deadline of about 1.5 ms), together with keeping the sim thread's core free from stream workers.

Direction and final verify JD. LithoPipeline designed by Archie, implemented by Cortana; Phase 4 research compiled by Elfie.

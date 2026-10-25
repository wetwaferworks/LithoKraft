# Display-mode baseline, 2026-10-25

Direction and final verify JD. Design Archie (mega-draw and Mega-Pipeline briefs). Implementation Cortana.

This benchmark compares the three display modes (exclusive fullscreen, borderless, and bordered windowed maximized) on the current build, before any Phase 2 work.

## How it was run

- **Build.** build-gate Release at commit 5c94b0b. The code is the same as b4597aa (5c94b0b only changed docs), with no Phase 2 edits. The test machine is an RTX 3050 Ti Laptop GPU on Vulkan 1.4 with the packed12 vertex format.
- **Display mode.** Each mode was selected with the existing command-line switch, `--display fullscreen|borderless|maximized`. No new flag was needed, and JD's config files were not touched.
- **Runs.** Each run was 30 seconds of benchmark camera path. There were two runs for each combination of:
  - display mode;
  - scene (the plain flyover, and the heavy test world via `--bench-scene testworld`, which also uses the flyover path);
  - stream radius (8, 32, 48);
  - draw mode (0 = the old path, 3 = the new default).
- **Bad run, redone.** The first exclusive-fullscreen run (radius 8, mode 0, run 1) recorded no real frames. It showed about 22,000 FPS with render time 0, because the window had no focus right after the mode switch. I discarded it and ran it again.
- **Present mode.** Every run used immediate present with vsync off, so no frame rate was capped by vsync.
- **Refresh rate.** Exclusive fullscreen switched the monitor to 1920x1080 at 144 Hz. Borderless and maximized ran on the desktop's 60 Hz mode.
- **Resolution.** Fullscreen and borderless render at 1920x1080; maximized renders at 1920x998 because the title bar and taskbar take the rest.

## Results

Each row is the average of the two runs. **Max** is the worst single frame and **Hitches** is the total across both runs. A hitch is a frame over 10 ms and more than twice the running average. **CPU ms** is the time spent recording the chunk draw commands on the CPU, per frame. Frame times are in milliseconds.

| Scene | Radius | Mode | Display | Resolution, refresh, present / vsync | FPS | p50 | p99 | p99.9 | Max | Hitches | Std dev | GPU ms | CPU ms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| flyover | 8 | 0 | borderless | 1920x1080 60Hz immediate/vsync off | 506.1 | 1.88 | 3.67 | 4.22 | 6.58 | 0 | 0.39 | 1.95 | 0.158 |
| flyover | 8 | 0 | fullscreen | 1920x1080 144Hz immediate/vsync off | 526.3 | 1.81 | 3.78 | 4.28 | 6.02 | 0 | 0.41 | 1.76 | 0.174 |
| flyover | 8 | 0 | maximized | 1920x998 60Hz immediate/vsync off | 514.9 | 1.85 | 3.67 | 4.20 | 6.05 | 0 | 0.39 | 1.88 | 0.167 |
| flyover | 8 | 3 | borderless | 1920x1080 60Hz immediate/vsync off | 491.2 | 1.94 | 3.75 | 4.27 | 5.36 | 0 | 0.40 | 1.98 | 0.125 |
| flyover | 8 | 3 | fullscreen | 1920x1080 144Hz immediate/vsync off | 498.2 | 1.91 | 3.70 | 4.18 | 4.65 | 0 | 0.39 | 1.98 | 0.116 |
| flyover | 8 | 3 | maximized | 1920x998 60Hz immediate/vsync off | 505.4 | 1.88 | 3.65 | 4.21 | 8.87 | 0 | 0.39 | 1.95 | 0.115 |
| flyover | 32 | 0 | borderless | 1920x1080 60Hz immediate/vsync off | 151.4 | 6.62 | 9.12 | 12.82 | 63.04 | 6 | 1.26 | 6.19 | 1.186 |
| flyover | 32 | 0 | fullscreen | 1920x1080 144Hz immediate/vsync off | 152.8 | 6.58 | 9.25 | 13.47 | 16.60 | 8 | 1.08 | 6.14 | 1.231 |
| flyover | 32 | 0 | maximized | 1920x998 60Hz immediate/vsync off | 148.4 | 6.77 | 9.24 | 12.65 | 18.36 | 1 | 1.05 | 6.33 | 1.293 |
| flyover | 32 | 3 | borderless | 1920x1080 60Hz immediate/vsync off | 149.4 | 6.74 | 9.31 | 10.13 | 21.72 | 1 | 1.06 | 6.29 | 0.951 |
| flyover | 32 | 3 | fullscreen | 1920x1080 144Hz immediate/vsync off | 150.4 | 6.70 | 9.39 | 10.75 | 19.95 | 4 | 1.10 | 6.24 | 0.962 |
| flyover | 32 | 3 | maximized | 1920x998 60Hz immediate/vsync off | 146.3 | 6.87 | 9.47 | 10.23 | 12.94 | 2 | 1.06 | 6.41 | 0.960 |
| flyover | 48 | 0 | borderless | 1920x1080 60Hz immediate/vsync off | 87.4 | 11.46 | 15.18 | 16.06 | 17.40 | 4 | 1.79 | 9.91 | 2.386 |
| flyover | 48 | 0 | fullscreen | 1920x1080 144Hz immediate/vsync off | 87.9 | 11.37 | 15.42 | 19.17 | 24.10 | 5 | 1.84 | 9.88 | 2.396 |
| flyover | 48 | 0 | maximized | 1920x998 60Hz immediate/vsync off | 85.2 | 11.78 | 15.57 | 17.38 | 18.85 | 4 | 1.85 | 10.11 | 2.458 |
| flyover | 48 | 3 | borderless | 1920x1080 60Hz immediate/vsync off | 86.3 | 11.61 | 15.39 | 16.10 | 19.13 | 4 | 1.78 | 9.95 | 1.909 |
| flyover | 48 | 3 | fullscreen | 1920x1080 144Hz immediate/vsync off | 87.0 | 11.55 | 15.41 | 19.33 | 25.50 | 7 | 1.89 | 9.94 | 1.962 |
| flyover | 48 | 3 | maximized | 1920x998 60Hz immediate/vsync off | 84.5 | 11.88 | 15.59 | 17.12 | 20.00 | 7 | 1.85 | 10.24 | 2.092 |
| testworld | 8 | 0 | borderless | 1920x1080 60Hz immediate/vsync off | 511.3 | 1.86 | 3.66 | 4.22 | 5.48 | 0 | 0.39 | 1.93 | 0.156 |
| testworld | 8 | 0 | fullscreen | 1920x1080 144Hz immediate/vsync off | 508.8 | 1.87 | 3.70 | 4.18 | 6.19 | 0 | 0.40 | 1.94 | 0.161 |
| testworld | 8 | 0 | maximized | 1920x998 60Hz immediate/vsync off | 512.9 | 1.85 | 3.65 | 4.17 | 7.35 | 0 | 0.39 | 1.92 | 0.160 |
| testworld | 8 | 3 | borderless | 1920x1080 60Hz immediate/vsync off | 496.2 | 1.92 | 3.73 | 4.22 | 4.77 | 0 | 0.40 | 1.99 | 0.117 |
| testworld | 8 | 3 | fullscreen | 1920x1080 144Hz immediate/vsync off | 500.2 | 1.91 | 3.75 | 4.25 | 7.20 | 0 | 0.40 | 1.96 | 0.118 |
| testworld | 8 | 3 | maximized | 1920x998 60Hz immediate/vsync off | 507.5 | 1.87 | 3.68 | 4.22 | 7.36 | 0 | 0.40 | 1.94 | 0.122 |
| testworld | 32 | 0 | borderless | 1920x1080 60Hz immediate/vsync off | 152.1 | 6.61 | 9.09 | 9.81 | 16.15 | 2 | 1.02 | 6.18 | 1.144 |
| testworld | 32 | 0 | fullscreen | 1920x1080 144Hz immediate/vsync off | 152.0 | 6.63 | 9.17 | 9.84 | 10.54 | 0 | 1.01 | 6.18 | 1.138 |
| testworld | 32 | 0 | maximized | 1920x998 60Hz immediate/vsync off | 148.8 | 6.73 | 9.36 | 12.25 | 22.76 | 4 | 1.09 | 6.31 | 1.231 |
| testworld | 32 | 3 | borderless | 1920x1080 60Hz immediate/vsync off | 149.2 | 6.74 | 9.39 | 10.26 | 18.43 | 2 | 1.08 | 6.30 | 0.974 |
| testworld | 32 | 3 | fullscreen | 1920x1080 144Hz immediate/vsync off | 149.9 | 6.74 | 9.38 | 10.22 | 13.42 | 0 | 1.05 | 6.27 | 0.927 |
| testworld | 32 | 3 | maximized | 1920x998 60Hz immediate/vsync off | 146.2 | 6.88 | 9.52 | 12.17 | 29.87 | 4 | 1.14 | 6.41 | 1.066 |
| testworld | 48 | 0 | borderless | 1920x1080 60Hz immediate/vsync off | 87.6 | 11.44 | 15.11 | 16.99 | 18.39 | 3 | 1.79 | 9.83 | 2.334 |
| testworld | 48 | 0 | fullscreen | 1920x1080 144Hz immediate/vsync off | 87.4 | 11.44 | 15.46 | 17.30 | 20.62 | 7 | 1.86 | 9.90 | 2.350 |
| testworld | 48 | 0 | maximized | 1920x998 60Hz immediate/vsync off | 85.2 | 11.79 | 15.58 | 17.33 | 29.31 | 6 | 1.87 | 10.16 | 2.572 |
| testworld | 48 | 3 | borderless | 1920x1080 60Hz immediate/vsync off | 86.7 | 11.54 | 15.22 | 16.11 | 22.36 | 5 | 1.79 | 9.94 | 1.930 |
| testworld | 48 | 3 | fullscreen | 1920x1080 144Hz immediate/vsync off | 86.7 | 11.57 | 15.55 | 17.93 | 24.39 | 4 | 1.91 | 9.91 | 1.885 |
| testworld | 48 | 3 | maximized | 1920x998 60Hz immediate/vsync off | 84.5 | 11.88 | 15.60 | 16.83 | 18.77 | 2 | 1.87 | 10.22 | 2.027 |

## What this shows

- All three display modes land within about 3% of each other on average FPS, p50 and p99. The GPU is the limit, mostly on shading, so how the window is presented barely matters while vsync is off.
- **Borderless** has the steadiest worst-case frames:
  - At radius 48 it has the lowest p99.9 in both scenes: about 16.1 ms, against 17.1-17.4 ms for maximized and 17.3-19.3 ms for fullscreen.
  - Exclusive fullscreen had the largest single spikes at radius 48 (24-25.5 ms).
  - The one outlier is a 63 ms spike in one borderless flyover run at radius 32 with mode 0.
- **Maximized** is about 2-3% slower than borderless at radius 32-48, even though it renders about 8% fewer pixels. That is consistent with the desktop compositor copying the windowed image. It also has more test-world spikes (up to 29.9 ms).
- **Mode 3 against mode 0:** within 1-2% on FPS in every display mode, with 15-25% less CPU recording time at radius 32-48.

## Recommendation

**Use borderless for play and benchmarks.**
- It is as fast as exclusive fullscreen, has the steadiest worst-case frames, and switches windows without a mode change.
- JD's saved settings already pick borderless (displayMode 2).
- One caveat: the Windows desktop on this laptop is set to 60 Hz while the panel supports 144 Hz. Only exclusive fullscreen gets 144 Hz today. With vsync or a frame cap on, borderless and maximized would be held to 60 FPS. Setting Windows to 144 Hz (Settings > Display > Advanced display) gives borderless the same 144 Hz.

**The code default.** JD asked for maximized windowed as the code default, and that stays as it is (commit b4597aa). It only applies when no saved setting exists, and the gap is small. If JD prefers, the default could become borderless again; that would be a one-line change.
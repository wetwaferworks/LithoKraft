# Voxel Compute — engine prototype

Minimal C++20 + OpenGL 3.3 core prototype: a flat-ish **Wafer Silicon** world (16×16 chunks, sub-chunk
storage from `../docs/ARCHITECTURE.md`), face-culled + ambient-occlusion meshing, free-fly camera, and real
threading: render thread (GL owner), a sim thread (32/64/128/256 TPS + stub 512 Hz chip clock domain), and a worker pool for worldgen + meshing.
No components or wiring yet.

Dependencies: CMake ≥ 3.16, a C++20 compiler, Git (CMake downloads **GLFW 3.4** from the official
github.com/glfw/glfw repo). The OpenGL loader is a tiny vendored file (`src/gl_loader.*`), no glad/GLEW needed.

## Controls
| Key | Action |
|---|---|
| Mouse | look |
| W A S D | move |
| Space / Left Shift | up / down |
| Left Ctrl (hold) | fast |
| F1 / F2 / F3 / F4 | world tick rate 32 / 64 / 128 / 256 TPS (default 64) |
| F5 | toggle the stub EUV chip clock domain (512 Hz) |
| Esc | release mouse (click in the window to grab it again) |

The window title shows FPS (and worst frame time), achieved/target TPS, average tick ms, sim budget % (sim CPU time
per wall second), EUV chip-domain Hz, dropped ticks (catch-up cap), active sub-chunks, and mesh counts.

Command line: `voxel_compute [--size chunks] [--tps 32|64|128|256] [--no-euv] [--radius chunks] [--seconds s] [--vsync]`
(defaults: 16 chunks, 64 TPS, EUV domain on, radius 8, run until closed, vsync off so FPS is uncapped).

## Windows (laptop) build

1. Install **Visual Studio 2022 Build Tools** with the "Desktop development with C++" workload (includes MSVC
   and CMake), plus Git:
   ```powershell
   winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
   winget install --id Git.Git -e
   ```
2. Open **"x64 Native Tools Command Prompt for VS 2022"** (Start menu), then:
   ```bat
   cd C:\path\to\voxel-compute\proto
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   build\Release\voxel_compute.exe
   ```
   (Alternative with Ninja: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` then
   `cmake --build build` and run `build\voxel_compute.exe`.)
3. On a laptop with an RTX GPU, make sure it runs on the NVIDIA GPU: Windows Settings → System → Display →
   Graphics → add `voxel_compute.exe` → High performance.

## Linux build
```bash
sudo apt install build-essential cmake git libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/voxel_compute
```

## Source map
- `src/world.*` — block registry + flags, sub-chunk / column storage, chunk hash map, worldgen
- `src/mesher.*` — face-culled mesher with AO (18³ padded opacity cube, 4-byte vertices)
- `src/sim.*` — fixed-rate sim thread, ticking radius, scheduled-tick queue (stub work for now)
- `src/threading.h` — lock-free triple buffer, job pool, O(1) hand-off queue
- `src/gl_loader.*` — minimal GL 3.3 function loader via `glfwGetProcAddress`
- `src/main.cpp` — window, camera, GPU upload budget, frustum culling, render loop

## Box test (Oct 9 2026)
Linux box, **software rendering** (Mesa llvmpipe on the CPU, no GPU), 1280×720: ~30–36 FPS (GPU-less; expect
hundreds of FPS on a real GPU). Worldgen 256 columns in ~5 ms; all 1200 sub-chunk meshes uploaded in the first second.

| Mode | Achieved TPS | Avg world tick | Sim budget | EUV domain |
|---|---|---|---|---|
| 32 | 32.9 (31.9 steady) | 0.029 ms | 0.11% | 512 Hz |
| 64 (default) | 64.0 | 0.021 ms | 0.15% | 512 Hz |
| 128 | 127.8 | 0.011 ms | 0.15% | 512 Hz |
| 256 | 256.9 | 0.007 ms | 0.20% | 513 Hz |
| 256, EUV off | 256.0 | 0.007 ms | 0.19% | off |

Runtime switching (F4 → F1 → F2) took effect within one tick. Catch-up cap test: freezing the process for 1 s
dropped 462 master steps, and the next second ran at target with no runaway. Screenshot: `screenshot.png`.

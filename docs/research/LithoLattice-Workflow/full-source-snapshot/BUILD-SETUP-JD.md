# LithoLattice + LithoKraft — Local Build Setup (JD)

## What you need
**Windows (your laptop):**
1. Visual Studio 2022 Build Tools — "Desktop development with C++" workload:
   `winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"`
2. Git: `winget install --id Git.Git -e`
3. Python 3 (for the build driver — you have this)

**Linux:** `sudo apt install build-essential cmake git python3`

That's it — CMake fetches everything else itself (GLFW, AngelScript, miniaudio, stb, Vulkan headers). No Vulkan SDK needed.

## Build & test
1. Unzip `litholattice-full-2026-10-11.zip` anywhere (e.g. `D:\LithoLattice`)
2. Run: `python tools\build.py` (Windows) or `python3 tools/build.py` (Linux)

One command: checks prerequisites → configures → builds Release → runs the full ctest suite.

Other useful forms:
- `python tools\build.py --build` — build only, skip tests
- `python tools\build.py --test` — run tests on an existing build
- `python tools\build.py --clean` — wipe the build dir, start fresh

## What to expect
- First build takes a while (it's fetching deps + compiling ~47k lines). Go make coffee.
- `ctest` should show the same results as the crew's runs. Two tests are
  grandfathered builder failures (`engine_sim`, `lithokraft_selftest`) —
  they fail here too, that's expected, don't chase them.
- The game exe also runs `--selftest` for a quick smoke check.

## If something breaks
Send Elfie the error output — the exact command and the last 30 lines.

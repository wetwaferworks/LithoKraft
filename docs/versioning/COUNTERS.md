# Build counters and public versions (JD, 2026-10-23, fix run 2)

| Module | Counter file (rolling) | Public version file | Who writes it |
|---|---|---|---|
| Litho Engine | `BUILD_COUNTER.txt` = `0.0.0.0.0.0.1.19` (8-wheel, label "Litho Engine", so it reads "Litho Engine 0.0.0.0.0.0.1.19") | (the counter is the version) | **A person only.** Builds run `tools/build_counter.py --no-bump`, which writes the header and nothing else. `-DLL_ENGINE_COUNTER_BUMP=ON` opts back in. |
| LithoKraft | `LITHOKRAFT_BUILD_COUNTER.txt` (a plain decimal that goes up by 1 on each game build) | `LITHOKRAFT_VERSION.txt` (for example `Alpha 0.0.1`) | `game/LithoKraft/cmake/bump_build_counter.cmake` |

The generated header defines `LITHOKRAFT_BUILD_NUMBER`, `LITHOKRAFT_PUBLIC_VERSION` and `LITHOKRAFT_ALPHA_VERSION` ("<public> build N"). You can change the public label without touching the counter.

## Rules for new modules
- Name the counter `<MODULE>_BUILD_COUNTER.txt` and the label file `<MODULE>_VERSION.txt`. The counter is a plain rolling decimal.
- A name must never match another counter if you ignore case (Windows file names are case-insensitive). The game's bump script stops with FATAL_ERROR on a name that equals `build_counter.txt`.
- No build may write `BUILD_COUNTER.txt`. Verified 2026-10-23 with a Windows Release build of the scratch tree: the file and its log were unchanged.
- ILithoGui has no counter yet. Add one by following these rules.
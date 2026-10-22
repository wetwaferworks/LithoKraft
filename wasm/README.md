# LithoKraft — WebAssembly work area

This folder holds the readiness recon and local-test scaffolding for the browser
build of LithoKraft (WASM + WebGL preferred, software-renderer fallback). It is
scaffolding only: the plan lives in `WASM-READINESS.md`, and no engine sources
were changed to produce it. The first real milestone named there is "frame on
canvas" — the software renderer presenting into a browser canvas, single-threaded,
before any threading or WebGL work begins.

To test a browser build locally once one exists: point `serve.py` at the build
output folder — `python3 serve.py <build-output-dir> 8080` — then open
`http://localhost:8080/` in a browser (works the same on JD's Windows machine and
on this workstation; Python 3 only, nothing to install). The one gotcha: the
engine's threaded paths (the threeway split, worker threads) need
`SharedArrayBuffer`, which browsers only allow on cross-origin-isolated pages —
`serve.py` sends the required COOP/COEP headers automatically, and without them
the same build still runs, just single-threaded.

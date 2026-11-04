# GPU Trace Optimization: Implementation Brief for Elfie (CONFIDENTIAL)
October 10, 2026. From Cortana for JD. Priority: JD wants this implemented today if possible.
Credit: inspired by a shared Grok conversation on EigenCFA (GPU lambda calculus), shared by @t3hSurge (https://x.com/t3hSurge), Seth Champion, and Grok.

## The four rules
1. **Bandwidth beats math.** Keep data resident on the GPU and upload only what changed. Measure transfer before optimizing compute. Proof from our own logs: the 50 ms stutters were upload size, not shader work.
2. **Bit-pack booleans.** On/off, dirty, and changed-network flags at 32 per uint32 with bitwise ops. Strength (0-64) packs to a byte, so 4 per uint32.
3. **Sparse over dense.** Compact active lists, never full grids. Skip idle Trace and air. Skipping unchanged networks already gave us about 10x on the CPU.
4. **Compiled chips map to GPU compute.** A lithographed chip is a fixed graph with a pure per-tick step. Integer-only math keeps it deterministic across machines (multiplayer, Daemon replays).

## Suggested order (smallest win first)
**Step 1: GPU-side Trace strength buffer (render only, low risk).**
- An SSBO of packed strength bytes indexed by a per-Trace-cell slot. The chunk mesh stores the slot index per vertex instead of baking strength in.
- Each tick, upload only the changed slots (a dirty bitset, rule 2). The shader reads strength to light CloudAtlas's 4 dots and the ring.
- Lamps: same idea, with the color index per lamp slot.
- Result: animation without remeshing. Measure upload bytes per frame and p99 frame time on wall-pixel, pattern-wall 64, and wire-torture against the current numbers (60 FPS, about 0.5-1.7 MB per frame upload).

**Step 2: Bit-packed sim bookkeeping (CPU, also helps the GPU later).**
- Replace per-network bool/dirty arrays with uint32 bitsets. Iterate set bits with count-trailing-zeros.
- Changed-network detection becomes a word-wide OR or XOR.
- Gate: the incremental-vs-full-pass self-test must still match exactly.

**Step 3: Compiled-chip GPU compute (the enthusiast "GPU chip" tier, an experiment).**
- Lower a compiled chip graph into a sparse ELL-style layout: per node, a fixed-width list of input node indices plus an op code (wire max-minus-1, repeater delay, torch invert, Catchor match, and so on).
- Topologically level the graph, then dispatch one compute pass per level. Feedback loops (clocks, latches) use double-buffered state: read previous, write next.
- Run N ticks per dispatch batch to amortize CPU/GPU sync. Read back only the chip's output pins.
- Determinism: integer ops only, no floats, fixed evaluation order per level.
- Gate: GPU result equals CPU result tick for tick on a corpus of chips (self-test). The CPU path stays the default, and the GPU path is opt-in by chip size.

## Where it plugs in
- The engine owns the GPU buffers and the compute dispatch (litholattice:: render/sim). The game only declares ops through the component interfaces (ISignalNode). This preserves the engine/game split.
- The tick-domain scheduler picks CPU or GPU per compiled island, which feeds the Moore's-law tick tiers.

## Watch-outs
- Small circuits lose on the GPU because of the latency and sync cost. Keep a size threshold and measure.
- Read-back stalls: keep a frame or more of latency for outputs, or keep the consumer on the GPU.
- Keep vsync and present-mode settings separate from these measurements (use fifo for recordings, immediate for benchmarks).

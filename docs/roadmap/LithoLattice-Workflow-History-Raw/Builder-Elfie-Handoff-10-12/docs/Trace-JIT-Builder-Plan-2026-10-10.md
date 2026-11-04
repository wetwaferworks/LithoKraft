# Trace JIT: Builder Implementation Plan
October 10, 2026. What the JIT needs to actually work.

## Current state

- `ITraceJit` interface defined (`trace_jit.h`)
- `BasicTraceJit` is a stub (copies pointers, doesn't compile)
- `CompiledGraph` struct defined (id, nodeCount, tickCost, bytecode)

## What the builder needs to implement

### 1. Graph extraction
Walk the ISignalNode graph from the 16x16 area:
- Nodes = Trace components (repeater, torch, wire, etc.)
- Edges = wire connections
- Inputs = PortOR blocks (input mode)
- Outputs = PortOR blocks (output mode)

### 2. Topological sort
Order nodes so inputs come before outputs:
- Standard Kahn's algorithm or DFS
- Detect cycles (feedback loops) — these need special handling
- Cycles become stateful (like a latch)

### 3. Bytecode generation
For each node in topological order, emit opcodes:
- `LOAD_INPUT n` — read PortOR n
- `AND`, `OR`, `NOT`, `XOR` — logic ops
- `DELAY n` — repeater delay
- `STORE_OUTPUT n` — write PortOR n
- `LATCH` — for cycles (memor behavior)

### 4. Optimizations
- **Constant folding**: if inputs are constant, precompute
- **Dead code elimination**: remove nodes that don't affect outputs
- **Common subexpression**: reuse computed values

### 5. Execution
- `execute()` runs the bytecode (interpreter loop)
- Should be 10-100x faster than ticking nodes individually
- Deterministic: same inputs → same outputs

### 6. Validation
- Run original nodes and compiled graph side-by-side
- Compare outputs for 1000+ ticks with random inputs
- Must match exactly or compilation failed

## Interface additions needed

```cpp
// In trace_jit.h, add:
struct JitStats {
  uint32_t originalNodes;
  uint32_t optimizedNodes;
  uint32_t bytecodeSize;
  double speedup;  // vs interpreted
};

class ITraceJit {
  // ... existing ...
  virtual JitStats getStats(const CompiledGraph& graph) = 0;
  virtual bool validate(const CompiledGraph& graph,
                        ISignalNode** nodes, uint32_t count) = 0;
};
```

## Testing

1. Compile a simple AND gate → verify output matches
2. Compile a 4-bit adder → verify all 256 input combos
3. Compile with feedback loop → verify latch behavior
4. Benchmark: compiled vs interpreted (should be 10x+ faster)

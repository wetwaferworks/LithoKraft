# PCH — UML and Engine/Game Hooks

*Read-only analysis, 2026-10-23. Sources: `R:\repos\PersonalControlHarness` (public PCH, `pch/` folder) and `C:\Users\jdwetwork\.aaa.www.inc\engine` / `game\LithoKraft`. No CEF/shell-layer code, key files or private PCH contents were opened. Every claim cites a file; where something does not exist, it says so.*

Path shorthand: `PCH/` = `R:\repos\PersonalControlHarness\pch\`, `ENG/` = `C:\Users\jdwetwork\.aaa.www.inc\engine\`, `GAME/` = `C:\Users\jdwetwork\.aaa.www.inc\game\LithoKraft\`.

## 1. PCH modules and types

The PCH is a small Python prototype, not a class hierarchy. It has no classes, just module-level functions over plain dict records.

- `PCH/harness/stepstream.py`: the record format (kinds `header`, `prefill`, `control`, `step`, `event`, `trailer`; docstring lines 1–15) plus `canonical`, `chain_next`, `seal`, `check_chain`, `save_stream`, `load_stream`. The chain is `sha256(prev_chain || canonical_json(record))` from `GENESIS = "0"*64`.
- `PCH/harness/harness.py`: `toy_args`, `build_model`, `weights_digest`, `cache_digest`, `derive_step_seed`, `sample_seeded`, `chosen_prob`, `run_live`, `verify_replay`. It imports the DeepSeek-V3 `inference/model.py` "specimen" from `PCH_SPECIMEN_DIR` (or `../DeepSeek-V3/inference`), and that code is never modified or shipped (lines 16–33).
- `PCH/harness/kernel.py`: a CPU shim that stands in for the specimen's Triton `kernel` module. `act_quant`, `weight_dequant` and `fp8_gemm` all raise if they're reached, so it runs on bf16 only.
- `PCH/harness/pch_more_tests.py`: `witnessed_verify(model, records, eps, drift)`, the cross-hardware "Witnessed Replay" tier Argon proposed (`PCH/docs/Pillar-PlugIn-Map.md`, Addendum 1).
- `PCH/harness/verify_streams.py` and `disc_test_llm.py`: verification and test scripts over the shipped `stream_greedy.jsonl` and `stream_sampled.jsonl`.
- Test beds: `R:\repos\PersonalControlHarness\testbeds\` (`receipt_check.py`, `tb_*.py`, `pch_testlib.py`).

```mermaid
classDiagram
    direction LR
    class stepstream_py {
      <<module>>
      GENESIS = "0"*64
      canonical(rec) bytes
      chain_next(prev, rec) str
      seal(records) list
      check_chain(records) bool
      save_stream(records, path)
      load_stream(path) list
    }
    class harness_py {
      <<module>>
      toy_args() ModelArgs
      build_model(init_seed) Transformer
      weights_digest(model) str
      cache_digest(model, upto) str
      derive_step_seed(run_seed, step) int
      sample_seeded(logits, T, run_seed, step) int
      chosen_prob(logits, T, token) float
      run_live(model, prompt, max_new, T, run_seed, control_at)
      verify_replay(model, records) (ok, reason)
    }
    class kernel_py {
      <<CPU shim>>
      act_quant() raises
      weight_dequant() raises
      fp8_gemm() raises
    }
    class pch_more_tests_py {
      <<module>>
      witnessed_verify(model, records, eps, drift)
    }
    class DeepSeekV3_inference {
      <<external specimen, not redistributed>>
      model.Transformer
      model.ModelArgs
    }
    class Record {
      <<dict, JSONL line>>
      kind: header|prefill|control|step|event|trailer
      chain: sha256 hex
    }
    harness_py --> stepstream_py : seal / check_chain
    harness_py --> DeepSeekV3_inference : import model as dsm
    DeepSeekV3_inference ..> kernel_py : "from kernel import" (shadowed)
    pch_more_tests_py --> harness_py
    stepstream_py --> Record : produces / hashes
```

## 2. Sequence: one sealed run, then replay/verify

Source: `PCH/harness/harness.py` `run_live` and `verify_replay`, plus `PCH/harness/stepstream.py`.

```mermaid
sequenceDiagram
    autonumber
    participant Caller
    participant H as harness.run_live
    participant M as DeepSeek-V3 Transformer
    participant S as stepstream
    participant V as harness.verify_replay
    Caller->>H: run_live(model, prompt, max_new, T, run_seed, control_at)
    H->>H: header{run_seed, T, weights=weights_digest(model), prompt}
    H->>M: forward(prompt, 0)  (prefill = one macro-step)
    H->>H: prefill{chunk, cache=cache_digest}
    loop t = 0..max_new-1
        opt t == control_at
            H->>H: control{step t, op} (admitted only at a boundary)
        end
        H->>M: forward(prev_token, cur_pos)
        H->>H: token = sample_seeded(logits, T, run_seed, t)<br/>seed = sha256("ll-harness:run_seed:t")
        H->>H: step{t, token, p=chosen_prob}
    end
    H->>S: seal(records): chain_i = sha256(chain_{i-1} || canonical(rec_i))
    H->>H: append trailer{steps, final_chain} (trailer itself is NOT chained)
    H-->>Caller: records (receipt: hash-only, unsigned)
    Caller->>V: verify_replay(fresh model, records)
    V->>S: check_chain(records minus trailer)
    V->>V: header.weights == weights_digest(model)?
    V->>M: re-run prefill, compare cache digest
    loop each step record
        V->>M: teacher-forced forward(recorded prev token)
        V->>V: sample_seeded(...) == recorded token?
    end
    V->>V: trailer.final_chain == last sealed chain?
    V-->>Caller: (ok, reason)
```

Notes:
- The trailer is appended after `seal()` and so isn't hash-covered. `verify_replay` only compares `final_chain` with the last chain value and never checks `steps` (see `harness.py` `run_live`/`verify_replay`). This matches the fuzz finding on the `testbeds-and-buildenv` branch, which is reported in `testbeds/SUMMARY.md`.
- The `event` record kind is defined in `stepstream.py` but never emitted by `run_live`.
- No signing exists. Receipts are hash chains only (`stepstream.py`).

## 3. Where the PCH meets the engine and game today

**Plainly: there is no real hook today.** Nothing under `ENG/src`, `ENG/docs` or `GAME/src` refers to PCH, ControlHarness, DeepSeek or the step-stream (I grepped for `PCH|ControlHarness|Personal Control|receipt|DeepSeek|stepstream`). The PCH runs as a standalone Python process. A copy of it also lives in `R:\repos\PersonalControlHarness\harness\` next to `pch/`.

What the engine does have are three seams designed in the same "serialized stream" style. A PCH would plug in through them:

- **ControlStream** (`ENG/src/sim/include/litholattice/sim/control_stream.h`): every sim-affecting input crosses one stream stamped `(seq, tick)` and is applied at the POST phase. The world is "a pure function of (initial state, stamped stream)" (lines 1–24). `ControlSource` already has `McpPilot`, `Script`, `Replay` and `Test` (lines 37–43). It offers `submit()` and `submitStamped()` plus a recording tap.
- **IDemoRecord** (`ENG/src/sim/include/litholattice/sim/idemo_record.h`): the `LLDM` file of LLCS segments, with a trailer carrying `finalWorldHash` and an FNV-1a checksum. That's the game-side analogue of a PCH receipt (FNV-1a, not SHA-256, and also unsigned).
- **MCP server** (`ENG/src/mcp/`, `server.h` `McpBackends` and the `I*` seams such as `IPlayerController`, `IWorldReader`, `IWorldWriter`). `ENG/docs/MCP-GAME-SEAM.md` states that the game-side `--mcp-stdio` wiring is "Nothing here is implemented yet" and that Phase 4 writes go through `IWorldWriter` onto the sim thread.
- **Engine AI** (`ENG/src/ai/include/litholattice/ai/brain.h`): deterministic utility AI and behaviour trees "with an optional learned scorer hook" (lines 1–2). It's a possible in-game seat for model decisions, but it isn't connected to the PCH.

```mermaid
flowchart LR
    subgraph PCH["Public PCH (Python, R:\\repos\\PersonalControlHarness\\pch)"]
      RL[run_live] --> SS[stepstream seal / JSONL receipt]
      VR[verify_replay / witnessed_verify]
      DS[(DeepSeek-V3 inference specimen<br/>PCH_SPECIMEN_DIR)]
      RL --> DS
    end
    subgraph ENG["LithoLattice engine (.aaa.www.inc\\engine\\src)"]
      MCP[mcp: McpServer + McpBackends<br/>IPlayerController / IWorldReader / IWorldWriter]
      CS[sim: ControlStream<br/>ControlSource::McpPilot / Script / Replay]
      DR[sim: IDemoRecord LLDM<br/>finalWorldHash + FNV-1a]
      AI[ai: brain utility AI<br/>optional learned scorer hook]
      MCP -->|pilot control frames| CS
      CS -->|recording tap| DR
    end
    subgraph GAME["LithoKraft (.aaa.www.inc\\game\\LithoKraft)"]
      MAIN[src/app/main.cpp<br/>--mcp-stdio NOT implemented]
      TESTS[control_stream_test / demo_record_test]
    end
    MAIN -.planned.-> MCP
    TESTS --> CS
    TESTS --> DR
    PCH -.->|"NO HOOK TODAY (proposed): PCH control/step events -> MCP pilot or ControlSource"| MCP
    SS -.->|"proposed: receipt alongside / inside demo record"| DR
    PCH -.->|"proposed: model as learned scorer"| AI
```

Dashed edges are proposals. The design intent behind them is written up in `PCH/docs/Pillar-PlugIn-Map.md`: Pillars 1–3, the verification request/verdict carried as events and control in one stream, and "the same rule the game sim already lives by".

## 4. Option: PCH loaded via hardware CEF layer (Proposed by JD)

*This is a design sketch from JD's description only ("if you hook the PCH to the hardware CEF layer, it's a different kind of thing and can always be loaded with the engine and local model weights"). No CEF or shell-layer code was read, so nothing here describes how that layer actually works.*

**Idea:** the PCH stops being an optional side process and becomes a resident service in the CEF layer. It would load whenever the engine loads, with local model weights held alongside it.

**Pros**
- It's always there. Every model-driven act in the game could go through the step-boundary gate, rather than only when someone runs the Python harness.
- One place to load and attest the weights. `weights_digest` (`harness.py`) could be computed once at load and stamped into every receipt.
- It fits the existing design. ControlStream is already "one stream, one sequencer" (`control_stream.h`), and a resident PCH could feed it as a dedicated `ControlSource`.

**Cons / risks**
- The public/private split gets harder. The CEF/shell layer is private, but the PCH is public MIT, so a CEF-resident PCH means either a public interface with a private host or a fork. The private PCH folder under `.aaa.www.inc` already exists, though I only listed it.
- Runtime: today's PCH is Python plus torch, bf16 on CPU only (`kernel.py`). The engine is C++. Running it resident means either embedding Python or porting it to C++ or ONNX-style inference.
- Memory and startup cost of keeping the weights resident with the engine.
- Its failure modes are now the engine's. A harness fault could block engine startup unless it's isolated.

**Open questions**
1. **Load order:** does the PCH load before the engine, so it can gate the first tick, or after it? What happens to the engine if the weights are missing?
2. **Weights residency:** shared memory, a separate process, or in-process? Who owns the GPU or CPU budget versus the renderer?
3. **Determinism:** the engine sim is tick-quantized (`control_stream.h`), while the PCH is token-step-quantized (`harness.py`). How do PCH steps map to world ticks? The Map's honest blockers also apply: same-hardware replay vs Witnessed Replay across hardware (`Pillar-PlugIn-Map.md`, Argon Addendum 1).
4. **Receipts:** should there be one combined record (PCH SHA-256 chain plus LLDM FNV-1a, `idemo_record.h`) or two linked ones? The trailer-coverage fix and signing (S1–S5, PCH core) would be needed first, and both need JD's go.
5. **Public vs private:** which part is the public "PCH API", and which stays in the private CEF host?
6. **Threading:** MCP and other producers never mutate the world directly (`MCP-GAME-SEAM.md`). Does a resident PCH follow the same rule and submit only through ControlStream?

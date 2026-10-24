# LithoKraft limits sheet + stress-test plan (2026-10-24, playtest)

JD: "I wanna know its limits... they're going to try and make it drop and make it fail, so we need to make it fail before they do."

Every figure is tagged: **[code]** = constant/clamp in source (file named), **[measured]** = an existing benchmark/selftest log (source named), **[estimate]** = arithmetic, not yet run. Nothing in this sheet was changed in the engine.

## A. Limits sheet

### Textures (per renderer)
| Item | Value | Tag / source |
|---|---|---|
| Storage model | Texture **array** (one layer per texture, all layers the same tile size), not a 2D atlas. The tile size is therefore NOT limited by an atlas dimension. | [code] gl_texture_array.cpp, vk_texture_array.cpp |
| Max layers | 2048 (16-bit layer in chunk vertices; 2048 = common GL_MAX_ARRAY_TEXTURE_LAYERS). Pack today = 658. | [code] texture_registry.h kMaxLayers |
| Pack manifest tile size | accepted 1..1024 | [code] resource_loader.cpp:48 |
| Menu / cvar choices | 0 auto, 16, 32, 64, 128 (no 256/512 option yet) | [code] settings_cvars.cpp:226, game_ui.cpp:393 |
| Software | capped at 64 (min(texRes,64)); stored 16/32/64, 5 mips max; LL_SW_TEXRES env overrides the auto pick | [code] game.cpp:678, cpu_texture_array.h |
| GL33 | GL_TEXTURE_2D_ARRAY RGBA8, glGenerateMipmap, no driver-limit check before glTexImage3D | [code] gl_texture_array.cpp |
| Vulkan | 2D-array RGBA8, full mip chain by blits; ONE host staging buffer of tile*tile*4*layers bytes; no check vs maxImageDimension2D / maxImageArrayLayers before create | [code] vk_texture_array.cpp:114-120 |
| GPU spec minimums | GL 3.3: max array layers >= 256, 2D size >= 1024; Vulkan: maxImageArrayLayers >= 256, maxImageDimension2D >= 4096. Real desktop GPUs (incl. RTX 3050 Ti): 2048 layers, 16384/32768 size. 658 layers is above the spec minimum, fine on real GPUs. | [estimate] spec values |
| VRAM 128 (today) | 658 x 64 KiB = 41 MiB, +1/3 mips = 55 MiB | [estimate] |
| VRAM 256 | 658 x 256 KiB = 165 MiB, with mips **~219 MiB**; Vulkan staging +165 MiB host-visible during load | [estimate] |
| VRAM 512 | 658 x 1 MiB = 658 MiB, with mips **~877 MiB**; staging +658 MiB at load; CPU copy of the images in the registry another ~658 MiB of RAM | [estimate] |
| Verdict | 256: should work on GL33 and Vulkan (needs a menu/cvar value and pack_256; main.cpp only auto-picks pack_128). 512: fits a 4 GB GPU but is the first real risk: ~0.9 GB VRAM + ~1.3 GB transient RAM at load, and Vulkan/GL have no graceful fallback if allocation fails (issue to report: add a max-size/VRAM check and fall back to 128). Software stays 64. | [estimate] |

### Tick rate
| Item | Value | Tag / source |
|---|---|---|
| Normal TPS modes | 32/64/128/256 world modes; 512 via clock domains (kMaxDomainHz 512); --tps 32..512 | [code] sim_thread.h:40, clock_domain.h:10, main.cpp:1049 |
| Debug ceiling | --tps-unlock: any power of two up to 8192 | [code] main.cpp:1169 |
| Static scene | holds 8192 TPS at 0.07 ms/tick | [measured] docs/LithoKraft-Night-Update-for-Elfie.md:13 |
| Framebuffer screen | size 64 holds 512 TPS; 128 holds 256; 256 about 100 TPS | [measured] same doc :15 |
| Wire torture | 1176 stair wires toggled every tick: 1.78 ms/tick, 561 TPS max | [measured] selftest log 2026-10-24 (rename-run\fu_notes3) |
| All test screens | 60 FPS at 512 TPS on RTX 3050 Ti laptop | [measured] docs/00-README-HANDOFF.md:13 |
| 64x64 wall tick | 0.63 ms (was 2.2) | [measured] docs/ROADMAP.md:6 |
| Render vs sim | ~1500 FPS at every tick rate (render not the bottleneck; possibly an FPS cap) | [measured] Elfie doc :14 |

### Distances / world
| Item | Value | Tag / source |
|---|---|---|
| Render (stream) radius | 0 auto (Vulkan 64), clamp 1..64 chunks; new default 16 | [code] main.cpp:1100, settings.cpp:50 |
| Sim (tick) radius | clamp 0..512, default 8, diamond area | [code] settings.cpp:41, main.cpp:1095 |
| Chunk | 16 x 16, world height 768 | [code] chunk.h:18-19 |
| Horizontal world size | no hard cap constant found (streamed columns) | [code] search found none |
| Streamer | 32 chunk jobs in flight | [code] chunk_streamer.cpp:113 |

### Signal / circuits
| Item | Value | Tag / source |
|---|---|---|
| Signal strength | 0..64 (kMaxSignal) | [code] blocks.h:160 |
| Trace range | decays 1 per block: 64 at source -> 0 after 64 blocks; Repeator re-boosts to 64 | [code]+[measured] selftest "decays 1 per block (55 after 10)" |
| Stack size | 512 | [code] inventory.h:18 |
| Clock domains | 4 | [code] sim_thread.h:72 |
| Trace nodes / Memor / Pulsor / lamps / lights / entities | **no hard cap constant found**: limited only by memory and tick time (see wire torture and framebuffer figures). Unbounded counts are exactly what testers will push. | [code] search found none |
| Console scripts | 16384 steps, 64 suspended, wait 512 / 4096 ticks | [code] concmd.h |
| Trees / shrubs | 1500 segments, 1024 leaves (fail-closed) | [code] trees.cpp, shrubs.h |

## B. Stress-test plan (overnight, headless/minimized, only while LithoKraft is closed)

Rules: no engine changes; issues found are reported, not fixed; -j6 / modest CPU; never kill JD's game (each run checks `Get-Process LithoKraft` first and aborts if JD's exe is running); every run uses `--no-save` and its own `--log`.

**Pass/fail (any = FAIL):** crash or non-zero exit; hang (no log line for 60 s, or run > 2x planned length); FPS < 30 sustained 5 s (windowed runs); achieved TPS < requested for > 5 s; hash mismatch vs cb32c824f8b3ea98 on the 3 scenarios after each block; RSS growth > 10% over the last half of a soak; any ERROR line not already known (dungeon R-PG-2 known).

**Metrics:** exit code, wall time, achieved TPS, ms/tick p50/p99, FPS p1/p50, peak RSS + VRAM (nvidia-smi sampling), log ERROR count.

| # | Scenario | How (existing flags) | Target / ceiling probed |
|---|---|---|---|
| S1 | TPS sweep, static | `--perf-headless --bench-scene display --tps-unlock --tps 512..8192` | find first rate that misses |
| S2 | Screen size x TPS | `--bench-scene framebuffer --scene-size 64/128/256` x tps 128..1024, `pattern-wall`, `wall-pixel`, `sawtooth` | the 100-TPS @256 knee; does 512 hold at 64 |
| S3 | Wire torture | `--bench-scene wire-torture` x tps 256/512/1024 | 561 TPS ceiling |
| S4 | Render distance sweep | `--bench 60 --bench-path flyover --radius 8/16/32/48/64`, vulkan + gl33, minimized | FPS floor 30; VRAM at 64 |
| S5 | Streaming abuse | `--bench-path stream --radius 64` 10 min | memory growth, streamer stalls |
| S6 | Soak | `--soak 120 --test-world` (playtest exe) | RSS growth, hash after |
| S7 | Texture size | pack_256 / pack_512 generated into a scratch dir (not committed), `--pack`, vulkan + gl33 | allocation failure behaviour (expect issue at 512) |
| S8 | Counts with no cap | generated worlds: 100k Trace cells, 10k Memor+Pulsor, 10k lamps (scratch script via --bench-scene builders at larger --scene-size) | ms/tick and RAM per 10k |
| S9 | Save/load torture | existing save selftests + a 1M-block world save/reload in a temp `--save-dir` | data loss, fail-closed paths |
| S10 | Determinism under load | the 3 hash scenarios at 2048 + `--tps-unlock` runs back to back | hash stays cb32c824f8b3ea98 |
| S11 | Software renderer | S4 with `--renderer software` at radius 8/16 | FPS floor, AVX2 path |

Order for one night (~6 h): S10 (baseline) -> S1 -> S3 -> S2 -> S4 -> S7 -> S8 -> S5 -> S6 -> S11 -> S10 again. Results go to `rename-run\stress-<date>\` as one summary table plus raw logs.

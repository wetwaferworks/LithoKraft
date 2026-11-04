| run | present | display | frames | avg ms | p99 ms | max ms | upload ms | render ms | tick ms | cols/s | RAM peak MB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| vulkan-orbit-mailbox | mailbox | windowed | 3601 | 8.3355 | 15.8842 | 16.6464 | 0.0003 | 8.2713 | 0.0488 | 0.00 | 234.7 |
| vulkan-orbit-immediate | immediate | windowed | 68903 | 0.4354 | 4.0210 | 4.8391 | 0.0002 | 0.3943 | 0.0548 | 0.00 | 240.6 |
| vulkan-orbit-fifo | fifo | windowed | 1801 | 16.6671 | 18.1332 | 18.8570 | 0.0004 | 16.5846 | 0.0539 | 0.00 | 228.3 |
| gl33-orbit | swap-interval-0 | windowed | 39636 | 0.7569 | 1.4172 | 2.6610 | 0.0002 | 0.7198 | 0.0555 | 0.00 | 157.3 |
| vulkan-stream | immediate | windowed | 57993 | 0.5174 | 5.3526 | 6.3026 | 0.0082 | 0.4691 | 0.0096 | 41.13 | 253.3 |
| gl33-stream | swap-interval-0 | windowed | 26038 | 1.1522 | 1.7643 | 2.8096 | 0.0139 | 1.0969 | 0.0134 | 41.13 | 174.2 |
| vulkan-circuits-32 | immediate | windowed | 73817 | 0.4064 | 3.0352 | 3.7119 | 0.0002 | 0.3649 | 0.6397 | 0.00 | 244.8 |
| vulkan-circuits-64 | immediate | windowed | 70840 | 0.4235 | 3.5485 | 4.2468 | 0.0002 | 0.3822 | 0.6283 | 0.00 | 243.6 |
| vulkan-circuits-128 | immediate | windowed | 68144 | 0.4403 | 4.0270 | 4.7387 | 0.0002 | 0.3991 | 0.6018 | 0.00 | 243.1 |
| vulkan-circuits-256 | immediate | windowed | 65394 | 0.4588 | 4.5010 | 5.4118 | 0.0002 | 0.4183 | 0.5697 | 0.00 | 243.2 |
| vulkan-circuits-512 | immediate | windowed | 62228 | 0.4821 | 5.1084 | 5.9842 | 0.0002 | 0.4410 | 0.5271 | 0.00 | 240.3 |
| vulkan-orbit-fullscreen-arena-host | immediate | fullscreen | 36023 | 0.8328 | 1.6310 | 2.5412 | 0.0003 | 0.7853 | 0.0540 | 0.00 | 267.1 |
| vulkan-orbit-fullscreen-arena-device | immediate | fullscreen | 46417 | 0.6463 | 1.5438 | 2.3416 | 0.0002 | 0.6035 | 0.0574 | 0.00 | 251.8 |
| vulkan-orbit-fullscreen-arena-rebar | immediate | fullscreen | 46419 | 0.6463 | 1.6523 | 2.3326 | 0.0003 | 0.5997 | 0.0593 | 0.00 | 251.5 |
| gl33-orbit-fullscreen-arena-off | swap-interval-0 | fullscreen | 38252 | 0.7842 | 1.3772 | 2.0475 | 0.0002 | 0.7520 | 0.0573 | 0.00 | 144.6 |
| gl33-orbit-fullscreen-arena-on | swap-interval-0 | fullscreen | 38166 | 0.7861 | 1.3444 | 1.7789 | 0.0002 | 0.7554 | 0.0552 | 0.00 | 152.8 |
| vulkan-stream-fullscreen-arena-host | immediate | fullscreen | 21302 | 1.4084 | 1.8482 | 2.6588 | 0.0133 | 1.3479 | 0.0112 | 41.13 | 310.0 |
| vulkan-stream-fullscreen-arena-device | immediate | fullscreen | 46309 | 0.6478 | 1.7178 | 2.6258 | 0.0100 | 0.5931 | 0.0105 | 41.13 | 266.9 |
| vulkan-stream-fullscreen-arena-rebar | immediate | fullscreen | 46351 | 0.6473 | 1.6337 | 2.4168 | 0.0088 | 0.5966 | 0.0108 | 41.13 | 265.0 |
| gl33-stream-fullscreen-arena-off | swap-interval-0 | fullscreen | 24603 | 1.2194 | 1.6175 | 4.5133 | 0.0191 | 1.1552 | 0.0136 | 41.13 | 199.9 |
| gl33-stream-fullscreen-arena-on | swap-interval-0 | fullscreen | 24542 | 1.2224 | 1.5531 | 3.1618 | 0.0128 | 1.1722 | 0.0138 | 41.13 | 175.1 |

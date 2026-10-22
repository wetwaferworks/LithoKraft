#!/bin/bash
cd "$(dirname "$0")"
G=http://127.0.0.1:8902/benchgl.html
E=http://127.0.0.1:8901/embed.html
node bench_driver.js top    "$G?bench=1&bsecs=120&bpath=still&radius=20" ship-top-gl-still-r20-b 420
node bench_driver.js top    "$G?bench=1&bsecs=120&bpath=still&radius=20" ship-top-gl-still-r20-c 420
node bench_driver.js iframe "$E?bsecs=120&bpath=still&radius=20"          ship-frame-gl-still-r20-a 420
node bench_driver.js iframe "$E?bsecs=120&bpath=still&radius=20"          ship-frame-gl-still-r20-b 420
node bench_driver.js top    "$G?bench=1&bsecs=90&bpath=still&radius=16"   ship-top-gl-still-r16-a 360
node bench_driver.js top    "$G?bench=1&bsecs=90&bpath=still&radius=12"   ship-top-gl-still-r12-a 360
node bench_driver.js top    "$G?bench=1&bsecs=60&bpath=orbit&radius=20"   ship-top-gl-orbit-r20-a 300
echo MATRIX1_DONE

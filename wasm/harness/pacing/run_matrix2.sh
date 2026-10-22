#!/bin/bash
cd "$(dirname "$0")"
G=http://127.0.0.1:8902
E=http://127.0.0.1:8901/embed.html
node cast_driver.js top    "$G/gl.html"            cbase-top-gl-r20-a 660 25
node cast_driver.js top    "$G/gl.html"            cbase-top-gl-r20-b 660 25
node cast_driver.js iframe "$E"                    cbase-frame-gl-r20-a 660 25
node cast_driver.js iframe "$E"                    cbase-frame-gl-r20-b 660 25
node cast_driver.js top    "$G/benchgl.html?radius=16" cbase-top-gl-r16-a 660 25
node cast_driver.js top    "$G/benchgl.html?radius=12" cbase-top-gl-r12-a 660 25
echo MATRIX2_DONE

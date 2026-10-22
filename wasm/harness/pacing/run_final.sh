#!/bin/bash
# Final config matrix against the NEW build staged in newstage/ (port 8903).
cd "$(dirname "$0")"
G=http://127.0.0.1:8903
node cast_driver.js top "$G/gl.html"            new-top-gl-r20-a 660 20
node cast_driver.js top "$G/gl.html"            new-top-gl-r20-b 660 20
node cast_driver.js top "$G/gl.html"            new-top-gl-r20-c 660 20
node cast_driver.js top "$G/gl.html?aa=off"     new-top-gl-aaoff-a 660 20
node cast_driver.js top "$G/gl.html?aa=on"      new-top-gl-aaon-a 660 20
node cast_driver.js top "$G/gl.html?aa=on"      new-top-gl-aaon-b 660 20
node cast_driver.js top "$G/gl.html?aa=on"      new-top-gl-aaon-c 660 20
node cast_driver.js top "$G/gl.html?radius=16"  new-top-gl-r16-a 660 20
node cast_driver.js top "$G/gl.html?radius=16"  new-top-gl-r16-b 660 20
node cast_driver.js top "$G/gl.html?radius=16"  new-top-gl-r16-c 660 20
node cast_driver.js top "$G/gl.html?radius=12"  new-top-gl-r12-a 660 20
node cast_driver.js top "$G/gl.html?radius=12"  new-top-gl-r12-b 660 20
node cast_driver.js top "$G/gl.html?radius=12"  new-top-gl-r12-c 660 20
echo FINAL_MATRIX_DONE

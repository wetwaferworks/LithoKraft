#!/bin/bash
# Node smoke runner for the browser-targeted LithoKraft.js build.
#
# Why this exists: the web build sets -sOFFSCREENCANVAS_SUPPORT=1, so at
# startup the Emscripten runtime transfers Module.canvas to the proxied
# main pthread. Under Node there is no DOM canvas: with no Module at all
# the transfer aborts boot ("could not find canvas with ID #canvas").
# This runner concatenates a one-line prelude + the unmodified
# LithoKraft.js into node-combined.js (same directory, so the .wasm/.data
# resolve) where the prelude's Module.canvas is a minimal stand-in whose
# transferControlToOffscreen returns a MessageChannel port — a real
# transferable, which is all the runtime needs. The smoke modes
# (--selftest / --demo-test) never create a GL context, so the stand-in
# is never drawn to.
#
# Usage: run-node-smoke.sh [timeout-seconds] -- <args...>
#   e.g. run-node-smoke.sh 240 -- --demo-test --cities on
set -e
BUILD="$(cd "$(dirname "$0")/../../build-wasm2" && pwd)"
TIMEOUT="${1:-240}"; shift; [ "$1" = "--" ] && shift
printf '%s\n' 'const _mc = new MessageChannel(); Module = { canvas: { id: "canvas", width: 1280, height: 720, transferControlToOffscreen() { return _mc.port1; } } };' > "$BUILD/node-combined.js"
cat "$BUILD/LithoKraft.js" >> "$BUILD/node-combined.js"
cd "$BUILD"
timeout "$TIMEOUT" node node-combined.js "$@" || true

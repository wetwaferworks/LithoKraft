# ReadOR — design note (2026-10-11, JD)

## What it is
A new -OR component: the **ReadOR**. You place your Fragment inside it, and it sends lithoscript scripts directly to a **keyboard** component. The keyboard feeds those scripts into bigger contraptions constructed by the user, the proxy, or the agent.

## The pipeline
Fragment (player-written lithoscripts) → ReadOR (reads them out) → keyboard (script input surface) → player/agent/proxy-built machines.

## Serialization line (JD 2026-10-11)
- Players will build big memory chips in-world: DRAM, SDRAM, hard drives.
- The ReadOR opens a **serialization line**: the Fragment's scripts and data export directly into the world, written into player-built memory chips and drives.
- So the Fragment is both script cartridge AND data exporter: write in the book, slot it into the ReadOR, serialize out into your hardware.

## Design implications
- This is the **physical mechanism** for the Fragment → lithoscript → world pipeline (the seam already noted in the lithoscript work).
- The Fragment becomes a script *cartridge*: write scripts in your book, slot it into a ReadOR, your machines run them.
- The keyboard is the script *input* side for contraptions — a component that accepts lithoscript and routes it into machine logic.
- Three authors feed the pipeline: the user (hand-written), the proxy, and the agent. The permission model for agent/proxy-authored scripts needs definition (who can write what, where scripts live, whose authority they run under).
- Trust model: a script read from a Fragment runs with the Fragment owner's context — define what that means for multiplayer (whose permissions? owner's? placer's?).

## Status
Design noted 2026-10-11. NOT built — builder work. The lithoscript engine-side seams (command registry, permission gating, Fragment/typewriter note) should be shaped so the ReadOR can drive them later.

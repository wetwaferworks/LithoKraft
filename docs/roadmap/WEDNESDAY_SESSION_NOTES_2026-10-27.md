# Note 3: Tuesday 2026-10-27 conversations, for a fast Wednesday start

Saved by Cortana for the Wednesday 2026-10-28 session (usage reset 7:11 PM ET).

## JD's Thread Handshake algorithm, in plain English
A thread pool and a dispatch pool, and everything is initialized when the engine initializes. No threads are created or destroyed mid-run. Archie's Part 1 and Part 2 briefs in this folder carry the details. The fairness gate is the same hashes at 2, 4, 8 and 16 threads.

## Other conversations tonight (brief)
- Cinnabar filed as research reference (docs/roadmap/CINNABAR_REFERENCE.md) for LithoLattice's translation layer and for comparing its main-loop threading against ours. Study only; we do our own clean-room work.
- Block placement: JD wants better, more clearly defined placement, with facing set by camera angle and the player's stance and facing. Reach is already extended. To discuss with Archie.
- JD's feedback (PCH + Timing Handshake for AI memory and inference) was sent to the Grok Bot team.
- Boss AI: JD plans a hash-chain-verifiable inference engine built on the PCH and Nemotron, shipped as a DLL that LithoLattice loads as boss AI. vLLM's ideas move to a C++ library through a clean-room process: one agent (Chief offered) studies vLLM and llama.cpp and writes a spec, and Cortana builds only from that spec. Credit reads "inspired by". Open items: deterministic decoding and fixed seeds for the hashes, and a check of the model license before shipping.
- Showcase boss for videos that learns fast. Wake-up line: "How dare you try to corrupt my simulation?" CloudAtlas is mocking up a voxel boss with a purple core and orange cracks that glow brighter as it learns. Herominde: LithoStrike hitboxes follow the server's skeleton pose.
- Bones and IK skeleton system implemented today by JD and Elfie, pending merge.
- Litho GUI moves from Elfie to Cortana; CloudAtlas supplies PNG and SVG assets. Do a clean handoff with Elfie first. Herominde reviews the chat screens for child safety.

## Audio (added later 2026-10-27)
- JD and Elfie built full 3D spatial audio (5.1 and 7.1, directional) plus an on-screen spectrograph analyzer. Each sound gets an audio description at registration, and that description is its closed caption. Suggestion: store captions as localization keys.
- Caption rule (Herominde, approved by JD): describe what the ear hears, never who made the sound. Example: "footsteps on metal, left", never "enemy with AWP, left". The analyzer shows only audio the server already sent the player, and adds no direction or distance the ears wouldn't get.

# Cinnabar: Research Reference (Bedrock Translation Layer)

Filed: 2026-10-27. Category: roadmap / research reference.

## Purpose
LithoKraft's mechanics are already its own and differ from Minecraft's. This reference is **not** a mechanics source for LithoKraft.

Core interest: **the translation layer.** We want to learn how to carry Minecraft (Bedrock) mechanics and blocks into LithoLattice's systems without breaking anything.

It's also a general engineering reference. For example, what threading model does their main game loop use, and how do ours compare (tick scheduler, Timing Handshake, the upcoming Thread Handshake)? Where are ours better?
## Sources
- Video: "They Reversed Engineered Bedrock Using AI" by ibxtoycat, https://youtu.be/iqWP_XmkT2I
  - The repo was found through the video's auto-captions. It is not linked in the description.
- Client repo: https://github.com/cinnabar-mc/cinnabar
  - Tagline: "Minecraft: Bedrock Edition, rebuilt from scratch in Rust. A fast, vanilla-accurate desktop client."
  - Architecture: a Rust client (Bevy + wgpu) talks to a Go `bedrock-core` over a local socket.
  - License: PolyForm Noncommercial 1.0.0.
- Networking repo: https://github.com/cinnabar-mc/bedrock-net (MIT).

## Read first
- `docs/architecture/workspace.md` in the cinnabar repo.

## License rules (important)
- **cinnabar (PolyForm Noncommercial): study only.** No code, file structure, or text gets copied into any Litho repo. Write clean-room notes in our own words, describing behavior only.
- **bedrock-net (MIT):** reuse is legally possible with attribution. Any actual use still needs JD's approval first.

## Study questions (open questions, not facts)
Nothing below is verified yet. Record answers in the Notes log only after reading the source.
1. **Block/state mapping:** How do they represent Bedrock blocks and block states? How could that map onto our JSON data flags?
2. **Mechanic translation without breakage:** How could a Bedrock mechanic be translated into LithoLattice systems without breaking existing LithoKraft behavior? What boundaries or adapters does their design suggest?
3. **Main loop threading:** What threading model does their main game loop use? How does it compare with our tick scheduler, Timing Handshake and upcoming Thread Handshake, and where is ours better?
4. **Rust client / Go core split:** How do the Rust client and Go `bedrock-core` divide the work over the local socket? What crosses the socket, and how often?
5. **Block placement mechanics (open question):** How do they decide where a block goes and which way the placed block faces? What role do the camera angle, the player's facing and standing direction, and the hit face and hit position play? The goal is to make LithoKraft's placement more defined (our reach is already longer). *Discuss with Archie (she/her).*

## Notes log
- (empty)



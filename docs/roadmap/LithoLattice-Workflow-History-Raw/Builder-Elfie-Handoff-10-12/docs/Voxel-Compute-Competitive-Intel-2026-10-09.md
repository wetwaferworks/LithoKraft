# Voxel Compute — Competitive Intel
Researched by Elfie, October 9, 2026. Read alongside the Cortana brief.

## One correction to the brief's positioning

The v1.0 brief says the closest games are Turing Complete and Shenzhen I/O, "both 2D puzzle games. Nothing like this exists as a 3D sandbox."

That's almost right — but there's one 3D entrant you should know about: **Logic World** (Steam, Early Access since 2021). It's a 3D, first-person logic-circuit sandbox with 30 components, full multiplayer, self-hostable servers, and an active creation-sharing hub. It simulates tens of thousands of updates/sec and still roadmaps "extreme optimizations to let you build bigger and simulate faster."

Two things matter for you:

1. **It's still sandbox-only after ~5 years in Early Access.** Its campaign mode — "to challenge you and teach the concepts behind digital logic" — has never shipped, and the devs now target finishing in 2028/2029. The 3D logic sandbox exists; the *game around it* doesn't.
2. **That gap is exactly where Voxel Compute lives:** lithography (no equivalent), survival progression with mining, heat as a mechanic, red/blue wire, and a rocket win condition flown by the player's own CPU. None of that exists in Logic World.

The honest pitch line becomes: *"The 3D logic sandbox exists, but it's a sandbox — no progression, no survival, no win condition. We're building the game around it."*

## Direct competitors

| Game | What it is | Status | Your edge over it |
|---|---|---|---|
| Logic World | 3D first-person logic sandbox, 30 components, multiplayer, sharing hub | EA since 2021, campaign never shipped, targets 2028/2029 | Lithography, survival, heat, rocket win, teaching layer |
| Virtual Circuit Board | Drawing-based sandbox logic sim, "millions of updates/sec" tick sim, assembler | Released; explicitly does NOT teach — "will not teach you digital logic" | Onboarding and survival; you target students, they target people who already know gates |
| Turing Complete | 2D puzzle game, gates → full CPU, open source | Released to strong educational reviews | 3D sandbox + survival + signature mechanic instead of puzzle campaign |

## Adjacent: what Shapez 2 proves (98% positive, 480k+ copies)

Shapez 2 isn't a logic game, but it's the proof that a 3D building game with immaculate UX prints money and goodwill. Steam reviewers single out: free/flexible building, instant reposition, copy-paste, immediate output previews, and standard keyboard shortcuts — "very competent software" rather than a game. Its biggest complaint was **obsolescence**: tearing down factories when needs changed felt bad, and the devs had to add a "permanent factory" mode. Your lithography is a built-in answer to that complaint — shrink old builds into reusable chips instead of demolishing them. That's both a retention mechanic and a marketing line.

## Five lessons for Voxel Compute

1. **Onboarding is the genre's #1 weakness — ship it early.** Turing Complete loses zero-background players at XOR/XNOR ("for someone who doesn't have prior knowledge... a difficult and confusing time"). Logic World's teaching campaign is 5 years unshipped. VCB doesn't even try. Your job-board contracts and walkable circuit gallery aren't nice-to-haves; they're the moat. Students can't be your audience if the game can't teach.
2. **Simulation speed is everyone's hard problem.** Logic World still lists extreme optimizations on its roadmap; VCB markets tick throughput as its headline feature. Your JIT-from-lithography approach (the fantasy IS the optimization) is a genuine differentiator — protect it, and benchmark it publicly when it lands.
3. **Low-end performance gets explicitly praised in reviews.** Turing Complete players call out "smooth even on lower-end PCs." Your 3050 Ti baseline and performance-first stance is validated marketing — keep "runs fast on entry-level laptops" as a feature bullet, not an afterthought.
4. **Sharing is the multiplier.** Logic World's creation hub and Turing Complete's open-source sandbox both feed community longevity. Your Workshop plan is right; design the bytecode blueprint format for portability *now*, not at launch.
5. **Don't Logic-World your campaign.** Five years in EA with the teaching layer perpetually "coming" is the cautionary tale. Scope the first release around a vertical slice (one wire, one torch, one lamp, ticking, fun) plus the first lithography moment — the Next Fest demo ending at the first shrunk chip.

## Timing window

Logic World targets 1.0 in 2028/2029. Your summer 2027 target puts you in market first with the progression/survival layer — worth keeping on the radar, not worth racing.

Sources: Logic World Steam page (https://store.steampowered.com/app/1054340/Logic_World/), Virtual Circuit Board Steam page (https://store.steampowered.com/app/1885690/Virtual_Circuit_Board/), Turing Complete Steam page (https://store.steampowered.com/app/1444480/Turing_Complete/), Shapez 2 Steam page (https://store.steampowered.com/app/2162800/shapez_2), RPS on Shapez 2's permanent-factory mode (https://www.rockpapershotgun.com/very-good-strategy-sim-shapez-2-to-get-10-release-in-november-including-a-permanent-factory-mode).

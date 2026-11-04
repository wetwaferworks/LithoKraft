# LITHOKRAFT — AngelScript outreach draft
Drafted by Elfie, 2026-10-09. NOT SENT — for JD's approval/editing. Recipient: Andreas Jönsson (AngelCode), via the contact channel at angelcode.com.

---

Subject: AngelScript 2.38 is going into a real game — thought you'd want to know

Hi Andreas,

I'm David Rittenhouse, a solo dev in Ohio building **LITHOKRAFT** — a 3D voxel sandbox where players build real working logic circuits, from single gates up to full CPUs, then shrink them into JIT-compiled chips and escape on a rocket flown by their own CPU. It's built on my own C++20 engine, LithoLattice (free/open source, Unreal-style revenue share), targeting Steam.

I chose **AngelScript 2.38** deliberately for the AI and mob-behavior scripting layer: C++ core for the hot paths, AngelScript hooks for the tweakable decisions — utility AI + behavior trees with the learned model nudging the scores, ticking at 8–16 Hz separate from the 32 TPS world sim, with full in-game AI debug overlays (paths, goals, threat scores) as a *player-facing feature*, not a dev tool. I didn't want to write my own scripting language when a stable, battle-tested one already exists — the syntax mapping to C++ classes made it the obvious call.

The game is early (design locked, engine prototyping now), but I thought you'd want to hear your library is going into a real game. Two questions if you have a minute: anything I should know about best practices for high-frequency per-frame script contexts at this tick rate, and is there anything on your roadmap that would change how I should structure the integration?

Happy to credit AngelScript in the docs and share any feedback or edge cases I hit along the way.

Thanks for two decades of keeping the library alive.

— David Rittenhouse
lithocraft.net

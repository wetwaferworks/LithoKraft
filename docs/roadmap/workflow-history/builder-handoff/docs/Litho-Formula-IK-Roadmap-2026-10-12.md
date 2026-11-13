# IK & Boning for Litho Formula Entities — Proposed Roadmap (2026-10-12)

JD's directive: with Litho Formula wired up, the Daemon can create models and mobs on demand
to fire at the player — but generated mobs need generated rigs. A procedural model without
bones is a statue. This is the proposed roadmap for the IK/boning system. Roadmap only —
not scheduled, not scoped for alpha unless JD says so.

## The problem

Litho Formula emits voxel models (`.lthm`). For the Daemon's on-demand mobs (and any
procedural entity) to *move*, each generated model needs:
1. A skeleton (bones placed sensibly for the shape),
2. Skinning (voxels bound to bones with weights),
3. An IK solver (feet on ground, limbs reaching, procedural attack poses).

## Phases

1. **Skeleton extraction.** Heuristics over the voxel model: medial-axis / limb detection to
   place bones (spine, limbs, head). Deterministic per model — same model, same rig.
2. **Skinning.** Voxel-to-bone weight assignment (distance + flood-fill through the volume
   so a limb's voxels follow its bone, not the nearest one through empty space).
3. **IK solvers.** Two-bone analytic for limbs (cheap, exact); CCD/FABRIK for longer chains
   (spine, tails, tentacles — the Daemon will want tentacles).
4. **Entity integration.** Rigs plug into the mob/entity animation system; generated mobs get
   locomotion (walk cycles from IK leg placement) and attack poses.
5. **The Daemon pipeline.** Formula params → voxel model → bones → IK → behavior: the Daemon
   forges a mob mid-fight and it walks out swinging. On demand, per fight, never the same
   twice.

## Locked (JD 2026-10-12)

- Procedural rigs AND authored rig templates both supported (not either/or).
- Modders get the boning tools eventually (not Daemon-only); the Daemon has access obviously.
- IK runs at render rate, NEVER at the 512 TPS sim rate — IK poses derive from sim state but never feed back into it (determinism + avoids 512x-per-second animation error compounding).
- Starting budgets: 32 bones/mob cap, 4 influences/vertex, GPU-first skinning (software renderer gets LOD), analytic two-bone IK for limbs + CCD/FABRIK (max 4 joints, 10 iterations) for spines/tails/tentacles, max 4 active IK chains per mob, ~16 concurrent full-IK mobs, skeleton extraction under ~200ms cached per model.

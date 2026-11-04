# Roadmap: Thresholdor subtract mode

Requested by JD, Sunday 2026-10-25, 2:09 AM ET. Status: planned, not started.

## Why
JD's "comparator clock" from Minecraft counts down from the signal fed in. It needs a comparator that subtracts. Today the Thresholdor only compares (low/high refs) and passes the input strength through; there is no subtract mode, so the clock can't be built as designed.

## What
- Add a subtract mode to the Thresholdor (LithoKraft's analog comparator), toggled in-game like Minecraft's comparator mode switch.
- Subtract: output = max(0, back input - strongest side input).
- Existing compare/window behaviour stays the default, so old saves and builds don't change.
- The current mode must be readable at a glance (model or top marker) and in the look-at inspector.

## Notes
- Sim change: needs selftests, a save-format check for the stored mode, and new scenario baselines if the hashes move.
- Acceptance test: JD's comparator clock (a feedback loop that counts down from the input strength) runs in the test world.

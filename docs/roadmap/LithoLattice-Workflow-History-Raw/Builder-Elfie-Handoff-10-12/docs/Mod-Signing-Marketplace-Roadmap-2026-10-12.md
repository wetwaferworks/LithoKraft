# Mod Signing & Marketplace — Future Roadmap (2026-10-12)

JD's directives, captured verbatim in spirit. This is roadmap, not spec — the builder
should treat it as the direction of travel, not a work order.

## Trust tiers

- **First-party:** JD / LithoKraft itself. Signed, sealed, canonical.
- **Second-party:** approved partners. Their mods get signed by JD — the signature is the
  stamp of approval.
- **Third-party / community:** unsigned (or self-signed). They run — the safety nets below
  make that safe — but the certificate question ("signed or unsigned?") is always answerable,
  and players always know which tier they're holding.

## Signing, two phases

**Phase 1 (crude in-house) LANDED 2026-10-12:** clean-room SHA-256 + Ed25519 (RFC 8032, NIST/RFC test vectors), `litho_sign` CLI (keygen/sign/verify), canonical mod-hash spec in `engine/src/crypto/signing.h`, tamper detection verified end-to-end. Engine verification crew still in progress (public-key embedding, trust tiers, seal rework).

1. **Our own cryptographic signing** (self-contained, no CA, no funds needed). Mods carry
   a LithoLattice signature; the engine verifies it. Sealed mode is designed from the start
   to grow into this verification.
2. **Funded future: real digital certificates / PKI.** First/second-party developers contact
   JD to get their mods digitally signed. The certificate becomes the stamp of approval.

## Safety nets for the community (unsigned mods)

Certificates answer *who vouches for this mod* — they do not replace containment. Unsigned
community mods run under the full sandbox the engine already provides:

- Mod file sandboxing (O_NOFOLLOW + fd re-validation)
- Permission manifest, deny-by-default
- Per-mod CPU/memory quotas; script binding allowlist (AngelScript)
- Crash isolation per extension level; net/UI/input confinement
- Per-mod save sections (CRC32 + quarantine)

An unsigned mod is a guest in a locked room: it can play, it can't break the house.

## The marketplace (future)

If a marketplace is ever built into LithoKraft and the LithoLattice framework/ecosystem:

- **Every marketplace mod is signed and approved** — paid or free, no exceptions.
- The marketplace is the trust anchor for players: browse, install, and know it's vetted.
- Certificates make the tiers visible at a glance: signed = approved, unsigned = community.

## How it composes

| | Sealed mode | Debug mode |
|---|---|---|
| Signed 1st/2nd-party mod | loads, verified | loads, verified |
| Unsigned community mod | loads, sandboxed, clearly labeled | loads, sandboxed, hot-swappable only if mod opts in |
| Signature invalid | refused, logged | refused, logged |

Sealed wins on any disagreement (fail-closed) — the lock from 2026-10-12 stands through
both signing phases.

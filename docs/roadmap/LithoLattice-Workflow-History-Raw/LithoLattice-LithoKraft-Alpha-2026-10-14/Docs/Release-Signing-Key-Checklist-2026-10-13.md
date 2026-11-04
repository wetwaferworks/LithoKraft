# Release Signing Key — JD Checklist (prepared 2026-10-13)

Why this gates the beta: the Release binary currently refuses to boot
because the baked-in mod-signing public key is still the retired
throwaway test key. The boot guard is working as designed. Only JD can
fix this — the private key must be generated and held offline by him.
Until it lands: no Release boot, and no live MCP stdio smoke test
(the substitute wiring selftest passes 11/11 in its place).

Key facts (from engine/src/crypto/include/litholattice/crypto/signing.h
and engine/src/assets/include/litholattice/assets/mod_signature.h):
- Scheme: Ed25519, algorithm string "ed25519-sha256-v1"
- Keys are raw 32-byte files: <prefix>.priv (seed) and <prefix>.pub
- The public key bakes into the engine as 64 hex chars in
  kBakedPublicKeyHex in mod_signature.h

## The steps (the header calls it a 2-minute operation)

1. On JD's OFFLINE machine, in a build tree:
   ./litho_sign keygen --out jd_master
   -> creates jd_master.priv and jd_master.pub
2. Guard jd_master.priv: it is the crown jewels. Never ships, never
   commits to any repo, never goes in a chat or cloud folder. Back it
   up offline (e.g. encrypted USB kept separate from the machine).
3. Copy ONLY jd_master.pub to the build machine. Get its hex:
   od -A n -t x1 jd_master.pub | tr -d ' \n'
4. Paste the 64 hex chars into kBakedPublicKeyHex in
   engine/src/assets/include/litholattice/assets/mod_signature.h,
   replacing the test key (currently 3bb44b55...9bb787).
5. Rebuild Release. Boot it — the test-key refusal should be gone.
6. Verify end to end:
   ./litho_sign verify --key jd_master.pub --mod <a signed mod>
   and confirm the engine reports "Verified" at mod load.
7. Then close the last deferred inch from the fix program: pipe one
   MCP `initialize` line through the now-bootable Release build for
   the live stdio smoke test.

## Notes
- Re-issue: the P1b hash-framing change (v2) changed every canonical
  mod hash, so any existing signatures must be re-issued. There are
  none in the wild — no signed mod has shipped — so this is the cheap
  moment.
- Guard scope warning (P3-3, E-L3): the test-key check lives ONLY in
  the game's main() boot sequence. Any future executable linking the
  assets library must call bakedKeyIsTestKey() itself at startup.
- Elfie cannot do any of this for JD: generating a private key on
  Elfie's VM would defeat the entire point of JD holding it offline.

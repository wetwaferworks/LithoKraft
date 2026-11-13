# Pass 2 scripts

PowerShell helpers from the Pass 2 work package (2026-10-16). Their source tree is now `docs/roadmap/LithoLattice-Workflow/pass2/`.

- `p2apply.ps1`: unpack a patch tarball over the Pass 2 render sources, keeping `.orig-<stamp>` backups
- `p2build.ps1`, `p2build2.ps1`: configure and build the Pass 2 tree
- `p2test.ps1`: run the selftest against a built `LithoKraft.exe` and write logs
- `p2cmp.ps1`: compare two test logs
- `configure-error-output.txt`: a captured configure failure (`cmake` not on PATH), kept for the record

The scripts hard-code the original checkout path (`R:\repos\LithoLattice-Workflow\Pass2-2026-10-16`). Edit `$d` / `$r` before reusing them. They are not part of the main build.
# Software-renderer rework scripts

PowerShell helpers from the software-renderer rework in October 2026. The tree they worked on is in `docs/roadmap/LithoLattice-Workflow/software-renderer-rework/`, and the change notes are in `CHANGES-PASS2.md`.

- `p2apply.ps1`: unpack a patch tarball over the render sources, keeping `.orig` backups
- `p2build.ps1`, `p2build2.ps1`: configure and build that tree
- `p2test.ps1`: run the selftest against a built `LithoKraft.exe` and save the logs
- `p2cmp.ps1`: compare two test logs
- `configure-error-output.txt`: a configure failure captured when `cmake` was not on PATH

The scripts hard-code the original checkout path (`R:\repos\...`), so edit it before reusing them. They are not part of the main build.
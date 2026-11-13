# Workstation Update Checklist — Patch Tuesday, 2026-10-13

Prepared for JD's stated plan today (18:11 UTC): "It's time to update my
machines… I need those Secure Boot certificates."

## The one check that comes first (new fact, not in today's chat)

Today's chat covered the cumulative update KB5127648. What it did not
cover: **Windows 11 version 24H2 (Home/Pro) reaches end of support today,
Oct 13, 2026**, per reporting on the October 2026 Patch Tuesday release.
A cumulative update on an end-of-support feature version does not restore
support — the machine needs the feature update (25H2) as well.

1. On each machine: run `winver`.
2. If it shows **24H2 Home/Pro** → install the 25H2 feature update Windows
   Update offers, not just KB5127648. Back up first; note the BitLocker
   recovery key before a feature update.
3. If it shows **25H2 / 26H2** → KB5127648 (builds 26200.9711 / 26300.9711)
   is the right cumulative and moves the machine forward.
4. If it shows **26H1** → different line: KB5127646 (build 28000.3231,
   Arm64); the 24H2 deadline does not apply.

JD's actual version(s) are unknown — that is why step 1 is a check, not an
assumption.

## Secure Boot certificates (JD's stated priority)

- KB5127648 "continues the deployment of updated Secure Boot certificates"
  to eligible devices (Neowin / ElevenForum summaries of the release notes,
  Oct 13, 2026).
- The **Microsoft Windows Production PCA 2011** certificate, which signs
  the Windows boot loader, expires **October 19, 2026** (WindowsLatest,
  Oct 13, 2026). Machines without the replacement certs still boot and
  update normally, but the rollover is the root-of-trust item JD cares
  about.
- After updating: verify at **Windows Security → Device security → Secure
  Boot** — if it does not confirm the certificate updates are applied, keep
  Windows Update on and check for a firmware update from the PC maker,
  which some machines need first.

## After the reboot, on the laptop specifically

- Re-test the walk-away lock: JD and Cortana fixed the automatic-lock bug
  themselves weeks ago and reported the fix to Microsoft end-to-end; today's
  published notes list no official auto-lock fix (checked in chat, 18:10
  UTC), so confirm their own fix survived the update before the next
  library trip with the game in the bag.
- Confirm BitLocker still reports on (the snatch-and-run threat model from
  today's chat).

## Sources

- https://www.neowin.net/news/windows-11-kb5127648-and-kb5127646-patch-tuesday-updates-are-now-available/
- https://www.elevenforum.com/t/kb5127648-windows-11-cumulative-update-build-26100-9711-24h2-26200-9711-25h2-26300-9711-26h2-oct-13.50115/
- https://www.windowslatest.com/2026/10/13/i-tested-windows-11-october-2026-update-heres-everything-new-improved-and-fixed/
- https://windows.gadgethacks.com/news/windows-11-24h2-end-of-support-hits-oct-13-steps-to-take/

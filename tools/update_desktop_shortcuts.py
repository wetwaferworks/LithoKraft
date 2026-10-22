#!/usr/bin/env python3
"""Keep JD's three LithoKraft desktop shortcuts pointed at the recommended, gated build.

Proposed by JD, Built by Cortana (2026-10-16).

What it does
  Creates or overwrites exactly these three files in C:\\Users\\jdwetwork\\OneDrive\\Desktop
  (nothing else on the desktop is read or written):
    LithoKraft Alpha - Vulkan (Test World).lnk      --test-world --renderer vulkan
    LithoKraft Alpha - OpenGL 3.3 (Test World).lnk  --test-world --renderer gl33
    LithoKraft Alpha - Software (Test World).lnk    --test-world --renderer software
  Target = the chosen exe, working dir = the Pass2 root, icon = the exe, description =
  "LithoKraft <version> test world (<renderer>)". Each .lnk is read back after writing; any
  mismatch exits non-zero. Shortcuts are written through PowerShell's WScript.Shell COM object
  (stdlib subprocess only; no pywin32 needed).

Which exe
  --exe <path>  explicit exe (absolute, or a file name inside build\\Release).
  otherwise     build\\Release\\RECOMMENDED.txt (one line: the exe file name).
  The exe must be listed in build\\Release\\GATED.txt (one file name per line; gate.ps1 appends
  a build there only when its gate fully passes), so an ungated build never reaches the
  desktop. LithoKraft.exe itself is always refused (it is restored to 0.0.1.44 after every
  build, so it is a moving target). --allow-ungated overrides the GATED.txt check for an
  explicit --exe only (manual use, JD's call).

Hook (p2logs\\gate.ps1)
  At the end of gate.ps1, only when the gate fully passes (selftests ALL OK, the four sim
  hashes match, all seven shots written with 0 log errors):
    - the gated exe name is appended to GATED.txt;
    - with -Promote, the gated exe name is written to RECOMMENDED.txt and this script runs;
    - without -Promote, this script runs with --if-changed, which does nothing unless
      RECOMMENDED.txt now names a different exe than the shortcuts point at.
  It is deliberately NOT a CMake post-build step: builds produce an ungated LithoKraft.exe
  (later restored to 0.0.1.44), and ungated builds must never reach the desktop.

Usage
  python tools\\update_desktop_shortcuts.py --dry-run
  python tools\\update_desktop_shortcuts.py              (RECOMMENDED.txt)
  python tools\\update_desktop_shortcuts.py --exe LithoKraft-0.0.1.53-a-direct.exe
  python tools\\update_desktop_shortcuts.py --if-changed (used by gate.ps1)
Exit codes: 0 ok / nothing to do, 1 verification mismatch, 2 bad target or input.
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RELEASE = os.path.join(ROOT, "build", "Release")
DESKTOP = r"C:\Users\jdwetwork\OneDrive\Desktop"
SHORTCUTS = [  # (file name, --renderer value, label)
    ("LithoKraft Alpha - Vulkan (Test World).lnk", "vulkan", "Vulkan"),
    ("LithoKraft Alpha - OpenGL 3.3 (Test World).lnk", "gl33", "OpenGL 3.3"),
    ("LithoKraft Alpha - Software (Test World).lnk", "software", "Software"),
]


def fail(code, msg):
    print("error: " + msg, file=sys.stderr)
    sys.exit(code)


def read_lines(path):
    if not os.path.isfile(path):
        return []
    with open(path, encoding="utf-8-sig") as f:
        return [ln.strip() for ln in f if ln.strip() and not ln.strip().startswith("#")]


def version_of(exe_name):
    m = re.match(r"LithoKraft-(\d+\.\d+\.\d+\.\d+)", exe_name)
    return m.group(1) if m else os.path.splitext(exe_name)[0]


def ps(script):
    r = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                       capture_output=True, text=True)
    if r.returncode != 0:
        fail(1, "powershell failed: " + (r.stderr.strip() or r.stdout.strip()))
    return r.stdout


def q(s):  # single-quoted PowerShell literal
    return "'" + s.replace("'", "''") + "'"


def wanted(exe):
    ver = version_of(os.path.basename(exe))
    out = []
    for name, renderer, label in SHORTCUTS:
        out.append({"path": os.path.join(DESKTOP, name), "target": exe,
                    "args": "--test-world --renderer " + renderer, "workdir": ROOT,
                    "icon": exe + ",0", "desc": "LithoKraft %s test world (%s)" % (ver, label)})
    return out


def read_back(paths):
    lines = ["$sh=New-Object -ComObject WScript.Shell; $r=@()"]
    for p in paths:
        lines.append("if(Test-Path -LiteralPath %s){$l=$sh.CreateShortcut(%s); $r+=[pscustomobject]@{path=%s;target=$l.TargetPath;"
                     "args=$l.Arguments;workdir=$l.WorkingDirectory;icon=$l.IconLocation;desc=$l.Description}}"
                     " else {$r+=[pscustomobject]@{path=%s;missing=$true}}" % (q(p), q(p), q(p), q(p)))
    lines.append("ConvertTo-Json -InputObject @($r) -Compress")
    data = json.loads(ps("; ".join(lines)) or "[]")
    return {d["path"]: d for d in (data if isinstance(data, list) else [data])}


def main():
    ap = argparse.ArgumentParser(description="Point the three LithoKraft desktop shortcuts at a gated build.")
    ap.add_argument("--exe", help="exe path or file name in build\\Release (default: RECOMMENDED.txt)")
    ap.add_argument("--dry-run", action="store_true", help="print what would be written; change nothing")
    ap.add_argument("--if-changed", action="store_true", help="do nothing if the shortcuts already match")
    ap.add_argument("--allow-ungated", action="store_true", help="with --exe: skip the GATED.txt check")
    a = ap.parse_args()

    if a.exe:
        exe = a.exe if os.path.isabs(a.exe) else os.path.join(RELEASE, a.exe)
    else:
        rec = read_lines(os.path.join(RELEASE, "RECOMMENDED.txt"))
        if len(rec) != 1:
            fail(2, "build\\Release\\RECOMMENDED.txt must hold exactly one exe file name")
        exe = os.path.join(RELEASE, rec[0])
    exe = os.path.normpath(exe)
    name = os.path.basename(exe)
    if name.lower() == "lithokraft.exe":
        fail(2, "refusing LithoKraft.exe (restored to 0.0.1.44 after builds; use a parked LithoKraft-0.0.1.x-*.exe)")
    if not name.lower().endswith(".exe") or not os.path.isfile(exe):
        fail(2, "exe not found: " + exe)
    gated = [g.lower() for g in read_lines(os.path.join(RELEASE, "GATED.txt"))]
    if name.lower() not in gated and not (a.exe and a.allow_ungated):
        fail(2, "%s is not in build\\Release\\GATED.txt (ungated builds never go to the desktop)" % name)
    if not os.path.isdir(DESKTOP):
        fail(2, "desktop folder not found: " + DESKTOP)

    want = wanted(exe)
    keys = ("target", "args", "workdir", "icon", "desc")
    if a.if_changed:
        have = read_back([w["path"] for w in want])
        if all(not have[w["path"]].get("missing") and
               all(str(have[w["path"]].get(k, "")).lower() == w[k].lower() for k in keys) for w in want):
            print("shortcuts already point at %s; nothing to do" % name)
            return 0

    for w in want:
        print("%s %s\n    target  %s\n    args    %s\n    workdir %s\n    icon    %s\n    desc    %s"
              % ("would write" if a.dry_run else "writing", w["path"], w["target"], w["args"], w["workdir"], w["icon"], w["desc"]))
    if a.dry_run:
        return 0

    cmds = ["$sh=New-Object -ComObject WScript.Shell"]
    for w in want:
        cmds.append("$l=$sh.CreateShortcut(%s); $l.TargetPath=%s; $l.Arguments=%s; $l.WorkingDirectory=%s; "
                    "$l.IconLocation=%s; $l.Description=%s; $l.Save()"
                    % (q(w["path"]), q(w["target"]), q(w["args"]), q(w["workdir"]), q(w["icon"]), q(w["desc"])))
    ps("; ".join(cmds))

    have = read_back([w["path"] for w in want])
    bad = 0
    for w in want:
        h = have[w["path"]]
        for k in keys:
            if h.get("missing") or str(h.get(k, "")).lower() != w[k].lower():
                print("MISMATCH %s %s: wanted %r, got %r" % (os.path.basename(w["path"]), k, w[k], h.get(k)), file=sys.stderr)
                bad += 1
    if bad:
        return 1
    print("verified %d shortcuts -> %s" % (len(want), name))
    return 0


if __name__ == "__main__":
    sys.exit(main())

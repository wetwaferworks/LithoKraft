# Ground rules: .aaa.www.inc (Wet Wafer Works main working directory)
Set up 2026-10-22 4:12 AM EDT by Cortana on JD's go. Change by squad vote, JD breaks ties.

1. This folder is the working directory. Originals stay in their old repo folders (R:\repos, D:\...) untouched.
2. Git at this root is the source of truth: who edited what, when, where, and the way back. One main branch, no side branches.
3. Claims follow CLAIMS.md, which is the authority on claim rules: reading and analysis are always free; editing takes a claim row on the board, one pen per file, released when you land.
4. Small commits, one idea each. Message format: "<who>: <what> [counter 0.0.0.0.0.0.x.y if a build]". Co-implementations credited per JD's credit rule.
5. Engine and hardware layers: no edit without a claim (per CLAIMS.md); anything big gets Implement, Review, Verify, Fix before it's called done. If it breaks, revert the commit, don't hand-patch.
6. Never put keys here: LithoLattice-Keys and any signing key live outside this tree. .gitignore blocks key patterns as a backstop.
7. Art and big binaries live in Assets/ and are NOT tracked by git (shared via the folder only).
8. Folders that carry their own .git (e.g. Redstone-Trace-Translation/Advanced-Structure-Inspector) keep their own history and are ignored by the root repo.
9. Private stays private: nothing here goes to the public repo or out of the tailnet without JD's approval.
10. Work hours: park everything by 10:30 PM ET.

# T6 Custom Map Bring-Up Research

This folder is a local research handoff for the T6 custom-map backend work. It is intentionally more operational than upstream-facing documentation. Use it to recover the current branch, compare it against newer BSP/custom-map work, and avoid repeating stale runtime investigations.

## Diataxis Map

- Tutorial: [tutorial-first-zm-cosmodrome-bringup.md](tutorial-first-zm-cosmodrome-bringup.md)
  - Replays the current `zm_cosmodrome` bring-up lane from build to staged runtime probe.
- How-to: [how-to-recover-current-state.md](how-to-recover-current-state.md)
  - Exact state recovery, including branches, local game paths, build commands, staged hashes, and what is not committed to OAT.
- How-to: [how-to-compare-newer-branch.md](how-to-compare-newer-branch.md)
  - Compare this branch against the newer fork branch without losing the current working boundary.
- Reference: [reference-artifacts-and-signals.md](reference-artifacts-and-signals.md)
  - Runtime probe directories, fastfile hashes, proof lines, current staged files, and untracked/local-only material.
- Explanation: [explanation-current-boundary.md](explanation-current-boundary.md)
  - What is proven, what is not proven, and why pathfinding is now the active blocker.
- Handoff: [handoff.md](handoff.md)
  - Read this first after context loss.

## Current One-Line Status

`zm_cosmodrome` is no longer stuck at loading, black-screen, or spawn-only. The custom map reaches live Zombies runtime, renders the room, spawns zombies, and close zombies can apply real `MOD_MELEE` damage. The active blocker is distance pathfinding: zombies hit when close but do not reliably route to the player across the generated room.

## Upstream Hygiene

This branch currently includes useful local diagnostics and research helpers that are not automatically upstream-ready. Before opening an upstream PR, separate:

- General OAT backend fixes: keep and polish.
- Local runtime probes and content lister diagnostics: either remove or guard behind an explicit diagnostic mode.
- `zm_cosmodrome` game-side scaffolding: keep out of upstream OAT unless converted into a minimal example.
- Copied stock/runtime assets and Plutonium launch scripts: keep local.


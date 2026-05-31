# How To Compare Against The Newer Fork Branch

The likely newer lane is `myfork/bsp-compilation-2`. It is substantially different from this branch and should be compared deliberately, not merged blind.

## Known Branch Facts At Pause Time

```text
current branch: feature/t6-custom-map-backend
current pre-save HEAD: d7cf4ef1 feat(t6): add custom map backend
newer candidate: myfork/bsp-compilation-2
newer candidate head: 26ce6495 chore: formatting
merge base: 69143d80fb42d77c6ec80eda41463f79e80b1a10
divergence: current has 592 unique commits, bsp-compilation-2 has 89 unique commits
```

## Safe Compare Workflow

```powershell
cd "Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\OpenAssetTools"
git fetch --all --prune
git status --short --branch
git merge-base HEAD myfork/bsp-compilation-2
git rev-list --left-right --count HEAD...myfork/bsp-compilation-2
git log --oneline --left-right --cherry-pick HEAD...myfork/bsp-compilation-2
```

Compare by subsystem:

```powershell
git diff --stat HEAD..myfork/bsp-compilation-2
git diff HEAD..myfork/bsp-compilation-2 -- src/ObjLoading/Game/T6/Map
git diff HEAD..myfork/bsp-compilation-2 -- src/Common/Game/T6
git diff HEAD..myfork/bsp-compilation-2 -- src/ZoneCode/Game/T6
git diff HEAD..myfork/bsp-compilation-2 -- test/ObjLoadingTests/Game/T6/Map
```

For a no-risk local integration preview:

```powershell
git worktree add "..\OpenAssetTools-bsp-compilation-2" myfork/bsp-compilation-2
```

Then inspect the newer branch in the separate worktree. Do not merge it into `feature/t6-custom-map-backend` until the custom-map runtime boundary has been preserved in a pushed commit.

## Questions To Answer During Compare

- Does the newer branch already solve BSP/world generation in a more general way?
- Does it add pathnode/path graph generation that supersedes the current `MapWorldT6.cpp` changes?
- Does it change T6 asset structs, zonecode, or writer behavior that affects `GameWorldMp`, `clipMap`, or `GfxWorld`?
- Can our runtime-proven deltas be reduced to small patches on top of the newer branch?
- Which current files are pure diagnostics and should not be carried forward?

## Current Runtime-Proven Deltas To Preserve

- ZM emits `GameWorldMp`, matching stock `zm_nuked`.
- Generated floor collision is traceable and supports live player/zombie movement.
- Backface/culling handling matters for collision generated from custom FBX.
- ZM requires the basic zombie attack `xanim` payload for stock melee notetrack/damage behavior.
- Pathfinding beyond close range remains the unsolved blocker.


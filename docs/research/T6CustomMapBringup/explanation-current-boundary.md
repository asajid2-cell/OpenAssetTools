# Explanation: Current Boundary

## Why This Is Real Progress

Earlier states were ambiguous: the map could be stuck at a loading screen, render black, fail before `InitGame`, or show a room without proof of stock Zombies gameplay. That is no longer the boundary.

The current runtime lane proves a real gameplay slice:

- the custom map fastfile is discovered and loaded
- map scripts run
- the generated room renders
- the player enters the map
- zombies spawn
- zombies acquire the player
- stock combat/melee animation code runs
- `melee_anim` notetracks fire
- `MOD_MELEE` damage is applied

This means the backend is past basic FF loading and basic render bring-up.

## Why It Is Not Done

The map is still not a complete custom Zombies gameplay experience. Zombies hit when close, but they do not reliably pathfind to the player at distance. The logs show target acquisition and sometimes clear traces, but pathing still emits `bad_path` and `maymove* = 0`.

That means the current blocker is navigation fidelity, not launch, scripts, xanim, or melee damage.

## What The XAnim Experiment Proved

Before the focused melee xanim assetlist, the zombie could enter a melee-like state but only emitted `note=end`. There was no `note=fire` and no player damage.

After adding the stock basic zombie attack xanim names plus `zm_nuked_basic.asd/.atr`, the runtime emitted `note=fire` and applied real melee damage. This strongly indicates that authored custom maps need the zombie attack xanim payload available in the map or support zones.

This should become a general requirement or example rule, not a hidden local accident.

## What The Path-Grid Experiment Is For

The stock `zm_nuked` path data and generated `zm_cosmodrome` path data look similar in the visible fields: `pathVis=true`, `smoothBytes=0`, `dynLinks=0`, and path link flags like `40`.

The previous `zm_cosmodrome` node layout was sparse and biased toward one side of the room. The current path-grid experiment adds 49 authored `node_pathnode` entries covering the room. If this improves runtime pathing, the backend may be basically consuming pathnodes correctly and the example was under-authored. If it does not, the next suspect is deeper generated world/path metadata.

The path-grid artifact is staged but not runtime-proven because the capture was interrupted.

## What To Compare Next

The fork branch `myfork/bsp-compilation-2` appears to be the newer work lane. It should be compared before more implementation. The likely outcomes are:

- port the runtime-proven custom-map deltas onto the newer branch
- replace current map generation code with better newer BSP generation
- keep only the minimal xanim/path requirements and tests from this branch
- discard local diagnostics before upstream PR cleanup


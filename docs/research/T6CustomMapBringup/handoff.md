# Handoff: T6 Custom Map Bring-Up

Read this file first after compaction or when resuming the work.

## Active Goal

Pause runtime work, preserve the exact current state, and compare this branch against the newer fork work before implementing more. The underlying product goal remains a general T6 custom-map backend suitable for an eventual OpenAssetTools PR, using `zm_cosmodrome` only as a local runtime proving ground.

## Current Branch State

- Repo: `Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\OpenAssetTools`
- Branch: `feature/t6-custom-map-backend`
- Remote tracking branch: `myfork/feature/t6-custom-map-backend`
- Pre-save HEAD before these docs: `d7cf4ef1 feat(t6): add custom map backend`
- Snapshot branch still relevant for historical comparison: `snapshot/t6-custom-map-loading-20260403`
- Historical snapshot commit: `c4829ab67bf2741e67af91b5e2f37288f3e4a0e8`
- Newer fork comparison candidate found by fetch: `myfork/bsp-compilation-2`
  - Head at pause time: `26ce6495 chore: formatting`
  - Merge base with this branch: `69143d80fb42d77c6ec80eda41463f79e80b1a10`
  - Divergence at pause time: this branch has `592` commits not in `myfork/bsp-compilation-2`; `myfork/bsp-compilation-2` has `89` commits not in this branch.
- Upstream `origin/main` after fetch: `f7be1ac9`

## What Is Proven

- `zm_cosmodrome` custom FF and localized FF load.
- Runtime reaches `InitGame`.
- Client connects.
- Custom room renders with HUD.
- Player can move in the custom room.
- Zombies spawn and acquire the player.
- Close zombie combat works without forced melee/combat scaffolding.
- Stock melee notetracks fire.
- Real player damage is applied as `MOD_MELEE`.

Best proof probe:

- `runtime\probes\spawn_top_capture_20260531_123513_zm_cosmodrome`
- Fastfile SHA256: `0505FBD85B6E7A50291A66D2EC2AC7126C92FEA807BD1FE481E2B3BFB69959C9`
- This was the clean natural probe with no player pin, no forced `meleecombat()`, no forced `zm_combat::main()`, and no synthetic `dodamage()` from the local miss hook.
- Key proof lines:
  - `t6_custom_map_player_damage: count=1 amount=60 type=MOD_MELEE health=40 attacker_origin=(-109.694,-91.2185,0.125)`
  - `t6_custom_map_player_damage: count=2 amount=60 type=MOD_MELEE health=40 attacker_origin=(-109.829,-91.4017,0.125)`
  - `t6_custom_map_melee_note: note=fire ...`

## Current Blocker

Distance pathfinding is still weak. The user's observed behavior is accurate: zombies hit when close, but they do not reliably pathfind across the room.

Clean natural probe symptoms:

- repeated `event=bad_path`
- repeated `maymove_player=0` and `maymove_meleepoint=0`
- line/floor traces can be clear while navigation still refuses
- `enemy_is_player=1` and `favoriteenemy=1` are present, so this is not target acquisition
- melee notetracks and damage work at close range, so this is not the old xanim/notetrack blocker

## Current Staged Runtime State

The latest staged artifact is a path-grid experiment, not yet runtime-proven:

- Build output: `Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\out_zm_cosmodrome_pathgrid_20260531_current\zm_cosmodrome.ff`
- SHA256: `66B065EBCC1C6A313A7D819CDDA7ACB384D52B45877F476F43CD6E5C92335E3E`
- Length: `55205376`
- Staged in:
  - `Z:\Games\pluto_t6_full_game\mods\zm_cosmodrome\zone\all\zm_cosmodrome.ff`
  - `C:\Users\Ahmed\AppData\Local\Plutonium\storage\t6\mods\zm_cosmodrome\zone\all\zm_cosmodrome.ff`
- Base/global map and survival copies were verified absent during staging:
  - `Z:\Games\pluto_t6_full_game\zone\all\zm_cosmodrome.ff`
  - `Z:\Games\pluto_t6_full_game\zone\all\so_zsurvival_zm_cosmodrome.ff`
  - `Z:\Games\pluto_t6_full_game\zone\all\so_zsurvival_zm_cosmodrome.ipak`
- Runtime capture was started and interrupted by the user:
  - partial directory: `runtime\probes\spawn_top_capture_20260531_124052_zm_cosmodrome`
  - only `launch_stdout.log` exists
  - no `summary.json`
  - do not treat this path-grid artifact as runtime-proven

## Local Game-Side State

These files are not in the OAT git repo, but they are required to reproduce the exact current runtime lane:

- `Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\zone_raw\zm_cosmodrome\maps\mp\zm_cosmodrome.gsc`
  - clean natural probe state
  - zombie limits raised to 4
  - forced melee/combat threads no longer started
  - player position pin no longer started
  - miss hook no longer synthesizes damage
- `Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\zone_raw\zm_cosmodrome\bsp\entities.json`
  - path-grid experiment currently present
  - 49 authored `node_pathnode` entries covering `x,y = -384,-256,-128,0,128,256,384`
  - 4 close `riser_location` entries are enabled
- `Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\zone_raw\zm_cosmodrome\assetlist\zm_nuked_basic_melee_xanims.csv`
  - local runtime assetlist that brought in the stock basic zombie attack xanim payload
- `Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\zone_source\zm_cosmodrome.zone`
  - includes `assetlist,zm_nuked_basic_melee_xanims`

## Do Not Regress

Do not spend time re-debugging these unless a fresh `zm_cosmodrome` probe shows that exact failure:

- launch command mechanics
- `party_maxplayers`
- Plutonium client executable mismatch
- IPAK slot pressure
- stale BO3/Servant work
- black empty room
- missing custom FF/localized FF
- melee xanim/notetrack failure

The current blocker is generated custom-map navigation/pathfinding.


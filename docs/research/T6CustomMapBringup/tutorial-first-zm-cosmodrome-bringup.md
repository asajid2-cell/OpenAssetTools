# Tutorial: First `zm_cosmodrome` Bring-Up

This is the practical path that got the example from "loads/renders" to "close zombies can damage the player." It is not an upstream tutorial yet.

## 1. Build With Zombies Donor Zones

Use `build_zm_cosmodrome.ps1` with the local OAT `Linker.exe` and the stock ZM donor zones. The important donor zones are `so_zsurvival_zm_transit.ff`, `zm_transit.ff`, and `zm_nuked.ff`, plus common/patch zones.

The map zone includes:

```text
>game,T6
>name,zm_cosmodrome
>map,zm
assetlist,zm_nuked_basic_melee_xanims
```

The xanim assetlist is required for the current melee proof.

## 2. Stage Only The Mod Lane

Use `stage_t6_custom_map_runtime_support.ps1` with:

```text
-StageSurvivalSupport
-UseCustomSurvivalFastfile
-SkipModIwd
```

Avoid base/global staging flags unless specifically testing them. The current stable lane keeps these absent:

```text
Z:\Games\pluto_t6_full_game\zone\all\zm_cosmodrome.ff
Z:\Games\pluto_t6_full_game\zone\all\so_zsurvival_zm_cosmodrome.ff
Z:\Games\pluto_t6_full_game\zone\all\so_zsurvival_zm_cosmodrome.ipak
```

## 3. Capture Without Clipboard Paste

Use:

```powershell
& 'Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\runtime\tools\capture_t6_zm_after_connect.ps1' -TimeoutSec 300 -PostConnectWaitSec 100
```

Do not raw-paste console command strings into the game. That caused focus leakage into other windows and corrupted unrelated workflows.

## 4. Validate These Signals

Required load/session signals:

```text
loaded_map_fastfile=true
loaded_localized_map_fastfile=true
init_game=true
sv_running=true
client_connected=true
com_error=false
out_of_memory=false
clientfield_mismatch=false
```

Required gameplay signals:

```text
t6_custom_map_melee_note: note=fire
t6_custom_map_player_damage: ... type=MOD_MELEE ...
```

Expected current pathfinding problem:

```text
t6_custom_map_ai_event: event=bad_path
maymove_player=0
maymove_meleepoint=0
```

## 5. Interpret Correctly

If close zombies damage the player, do not regress to loading or melee xanim debugging. The next problem is pathfinding.

If the path-grid artifact is tested next, the question is narrow:

Does denser authored pathnode coverage reduce `bad_path` and allow zombies to close distance from far parts of the room?


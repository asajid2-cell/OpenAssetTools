# How To Recover The Current State

This recovers the OAT branch plus the local `zm_cosmodrome` runtime lane as it existed at the pause point.

## 1. Checkout The OAT Fork Branch

```powershell
cd "Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\OpenAssetTools"
git fetch --all --prune
git checkout feature/t6-custom-map-backend
git status --short --branch
```

Expected branch:

```text
## feature/t6-custom-map-backend...myfork/feature/t6-custom-map-backend
```

At the pause point, the branch contains the custom-map backend work plus research diagnostics. It is not yet an upstream-clean PR branch.

## 2. Know What Is Not In Git

The wrapper folder `Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend` is not itself a git repo. Runtime probes, the top-level `handoff.md`, Plutonium launch scripts, staged fastfiles, and `zm_cosmodrome_project` content live outside the OAT git repo.

Important external state:

```text
Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\handoff.md
Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\runtime\probes\
Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\
Z:\Games\pluto_t6_full_game\tools\build_zm_cosmodrome.ps1
Z:\Games\pluto_t6_full_game\tools\stage_t6_custom_map_runtime_support.ps1
```

Current staged path-grid artifact:

```text
SHA256 66B065EBCC1C6A313A7D819CDDA7ACB384D52B45877F476F43CD6E5C92335E3E
Length 55205376
```

Locations:

```text
Z:\Games\pluto_t6_full_game\mods\zm_cosmodrome\zone\all\zm_cosmodrome.ff
C:\Users\Ahmed\AppData\Local\Plutonium\storage\t6\mods\zm_cosmodrome\zone\all\zm_cosmodrome.ff
```

This artifact was staged after a 49-node path-grid edit, but the capture was interrupted and has no runtime proof.

## 3. Rebuild The Current Path-Grid Artifact

```powershell
$out='Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\out_zm_cosmodrome_pathgrid_20260531_current'
$linker='Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\OpenAssetTools\build\bin\Release_x64\Linker.exe'
$zones=@(
  'Z:\Games\pluto_t6_full_game\zone\all\zm_nuked_patch.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\zm_transit_patch.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\patch_mp.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\common_mp.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\patch_zm.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\common_zm.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\so_zsurvival_zm_transit.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\zm_transit.ff',
  'Z:\Games\pluto_t6_full_game\zone\all\zm_nuked.ff'
)
& 'Z:\Games\pluto_t6_full_game\tools\build_zm_cosmodrome.ps1' -OutputFolder $out -LinkerPath $linker -LoadZone $zones -IncludeWeaponDonors
Get-FileHash -Algorithm SHA256 (Join-Path $out 'zm_cosmodrome.ff')
```

Expected hash if the local game-side files are unchanged:

```text
66B065EBCC1C6A313A7D819CDDA7ACB384D52B45877F476F43CD6E5C92335E3E
```

## 4. Stage The Artifact

```powershell
$out='Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\out_zm_cosmodrome_pathgrid_20260531_current'
$support='Z:\Games\pluto_t6_full_game\_build\custom_maps\zm_cosmodrome_project\out_zm_cosmodrome_autoxmodels_20260530_current'
& 'Z:\Games\pluto_t6_full_game\tools\stage_t6_custom_map_runtime_support.ps1' `
  -BuiltFastfile (Join-Path $out 'zm_cosmodrome.ff') `
  -BuiltLocalizedFastfile (Join-Path $support 'en_zm_cosmodrome.ff') `
  -BuiltModFastfile (Join-Path $support 'mod.ff') `
  -BuiltPatchFastfile (Join-Path $support 'zm_cosmodrome_patch.ff') `
  -BuiltLocalizedPatchFastfile (Join-Path $support 'en_zm_cosmodrome_patch.ff') `
  -BuiltSurvivalFastfile (Join-Path $support 'so_zsurvival_zm_cosmodrome.ff') `
  -BuiltLocalizedSurvivalFastfile (Join-Path $support 'en_so_zsurvival_zm_cosmodrome.ff') `
  -StageSurvivalSupport -UseCustomSurvivalFastfile -SkipModIwd
```

Do not pass `-StageBaseSurvivalIpak` or base/global staging flags unless specifically testing that global lane.

## 5. Capture A Probe Without Clipboard Pasting

```powershell
& 'Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\runtime\tools\capture_t6_zm_after_connect.ps1' -TimeoutSec 300 -PostConnectWaitSec 100
```

The interrupted pause-point capture was:

```text
runtime\probes\spawn_top_capture_20260531_124052_zm_cosmodrome
```

It has no `summary.json`, so rerun before making any claim about the path-grid experiment.

## 6. Stop Runtime Processes Before Pausing

```powershell
Get-Process | Where-Object { $_.ProcessName -match 'plutonium|t6|bootstrapper' }
Stop-Process -Name plutonium-bootstrapper-win32 -Force
```


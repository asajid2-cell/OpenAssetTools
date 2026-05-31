# Reference: Artifacts And Runtime Signals

## OAT Working Diff At Pause Time

Modified OAT files before saving:

```text
src/ObjLoading/Game/T6/Map/MapClipMapT6.cpp
src/ObjLoading/Game/T6/Map/MapGeometryT6.cpp
src/ObjLoading/Game/T6/Map/MapGfxWorldT6.cpp
src/ObjLoading/Game/T6/Map/MapWorldT6.cpp
src/Unlinking/ContentLister/ContentPrinter.cpp
test/ObjLoadingTests/Game/T6/Map/MapClipMapT6Test.cpp
test/ObjLoadingTests/Game/T6/Map/MapGeometryT6Test.cpp
test/ObjLoadingTests/Game/T6/Map/MapGfxWorldT6Test.cpp
test/ObjLoadingTests/Game/T6/Map/MapWorldT6Test.cpp
```

Untracked and not intended for commit:

```text
Material/JsonMaterial.h.manual.log
```

## Key Runtime Artifacts

### Close Combat And Damage Proof

```text
probe: runtime\probes\spawn_top_capture_20260531_123513_zm_cosmodrome
artifact: out_zm_cosmodrome_natural_20260531_current\zm_cosmodrome.ff
sha256: 0505FBD85B6E7A50291A66D2EC2AC7126C92FEA807BD1FE481E2B3BFB69959C9
length: 55202496
```

Important signals:

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

Important proof lines:

```text
t6_custom_map_player_damage: count=1 amount=60 type=MOD_MELEE health=40 attacker_origin=(-109.694,-91.2185,0.125)
t6_custom_map_player_damage: count=2 amount=60 type=MOD_MELEE health=40 attacker_origin=(-109.829,-91.4017,0.125)
t6_custom_map_melee_note: note=fire
```

Pathfinding blocker lines:

```text
t6_custom_map_ai_event: event=bad_path
maymove_player=0
maymove_meleepoint=0
```

### Focused XAnim Fix Proof

```text
probe: runtime\probes\spawn_top_capture_20260531_122604_zm_cosmodrome
artifact: out_zm_cosmodrome_meleexanim_20260531_current\zm_cosmodrome.ff
sha256: 92A563BCCA67AF471F82D897E992751B7168187DD5AE83F0A35A36834F3A408A
length: 55202496
```

This proved that adding stock basic zombie melee xanim payloads converted the previous `note=end` only symptom into `note=fire` plus real `MOD_MELEE` damage.

### Path-Grid Experiment

```text
artifact: out_zm_cosmodrome_pathgrid_20260531_current\zm_cosmodrome.ff
sha256: 66B065EBCC1C6A313A7D819CDDA7ACB384D52B45877F476F43CD6E5C92335E3E
length: 55205376
```

Generated `GameWorldMp` path details from content lister:

```text
nodes=49
originalNodes=49
visBytes=294
smoothBytes=0
nodeTrees=63
pathVis=true
firstNode origin=(-384, -384, 0)
firstNode links=16
```

Runtime proof status:

```text
partial probe: runtime\probes\spawn_top_capture_20260531_124052_zm_cosmodrome
summary.json: absent
status: interrupted, not runtime-proven
```

## Stock Comparison

Stock `zm_nuked.ff` content-lister sample:

```text
gameworldmp, maps/mp/zm_nuked.d3dbsp
nodes=889
originalNodes=889
visBytes=98679
smoothBytes=0
nodeTrees=451
pathVis=true
firstNode links=8
sample link flags=40
dynLinks=0
```

Current generated `zm_cosmodrome` before path grid:

```text
gameworldmp, maps/mp/zm_cosmodrome.d3dbsp
nodes=15
originalNodes=15
visBytes=27
smoothBytes=0
nodeTrees=15
pathVis=true
firstNode links=14
sample link flags=40
dynLinks=0
```

Interpretation: inspected fields look structurally close to stock for simple pathlinks. The current failure may be authored-node coverage, incomplete world/path metadata, or a subtler path flag/tree/volume expectation.

## Local Build/Test Commands

OAT unit test used during backend validation:

```powershell
& 'Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\OpenAssetTools\build\bin\Release_x64\ObjLoadingTests.exe' "[t6][map]"
```

Runtime capture:

```powershell
& 'Z:\328\CMPUT328-A2\codexworks\301\WorkRepo\oat-custom-map-backend\runtime\tools\capture_t6_zm_after_connect.ps1' -TimeoutSec 300 -PostConnectWaitSec 100
```

Use this instead of clipboard/raw paste.


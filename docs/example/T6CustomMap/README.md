# T6 Zombies Custom Map Example

This example documents the zone metadata and source-file layout for T6 zombies custom-map fastfiles.
It is intentionally minimal and is meant as a reference for the `>map,zm` flow that is implemented in OAT.

Example zone metadata:

```text
>game,T6
>name,zm_example
>map,zm
```

Supported map modes:

- `>map,mp` builds MP-style map assets and synthesizes MP spawn entities from `spawns.json`.
- `>map,sp` builds the same imported BSP/GFX/clipmap path, but emits `GameWorldSp` and does not synthesize MP spawn classes.
- `>map,zm` emits `GameWorldSp`, requires authored `bsp/entities.json`, and treats entity keys containing `zbarrier` as `zbarrier` asset dependencies.

Expected source files under the target directory:

- `bsp/map_gfx.fbx`
- `bsp/map_col.fbx`
- `bsp/entities.json`
- `bsp/spawns.json` for `>map,mp`

Script handling:

- `>map,mp` requires the conventional T6 script set under `maps/mp` and `clientscripts/mp`.
- `>map,zm` also requires that conventional script set under the same `maps/mp` and `clientscripts/mp` paths.
- `>map,sp` probes those same conventional paths if present, but does not fail if they are absent.
- If a map needs extra scripts that are not covered by the conventional lookup, add them explicitly in the zone definition.

Notes:

- T6 still uses `maps/mp/...` and `clientscripts/mp/...` naming for the conventional map scripts, including non-MP map names.
- For zombies maps, entity data must be authored explicitly. The file must be a JSON object with an `entities` array, and the first entity must be `worldspawn`.
- For zombies maps, entity keys containing `zbarrier` are treated as real `zbarrier` asset references and must resolve during linking.
- OAT does not try to synthesize zombie gameplay entities from a placeholder spawn list.
- Zombie maps should still use conventional `zm_*` naming. Tool-side linking no longer depends on the prefix alone, but T6 zombie naming conventions are still used elsewhere.
- This example only documents tool-side behavior. It does not imply runtime validation in the game.

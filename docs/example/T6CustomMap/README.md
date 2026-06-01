# T6 Custom Map Backend

This example documents the initial zone-definition contract for T6 custom map fastfiles.

Custom map zones use the `>map` metadata key:

```text
>game,T6
>name,zm_example
>map,zm
```

Supported map modes are:

- `>map,mp`
- `>map,sp`
- `>map,zm`

The current backend recognizes map targets, validates the source layout, parses
zombies entities, reads FBX geometry, and generates the required core world
assets for the map target:

- `MapEnts`
- `ComWorld`
- `GameWorldSp` for `>map,sp`
- `GameWorldMp` for `>map,mp` and `>map,zm`
- `GfxWorld`
- `ClipMap`
- `ClipMapPvs`
- `SkinnedVertsDef`

The generated world assets still depend on normal T6 material/image assets
being available through the usual asset search paths. FBX surface material names
must name renderable T6 world materials, for example `wpc/...` materials loaded
from an existing zone or authored in the custom map source. Placeholder DCC
materials such as `lambert1`, `white`, or `,white` are replaced by the default
world-material fallback when that asset is available. Known model-material
families such as `mc/...` and `mlv/...` are rejected for generated `GfxWorld`
surfaces because they can link successfully but render black in-game. A
source-only test fixture without those dependencies is expected to fail during
linking or warn during world surface generation.

Expected source files:

- `bsp/map_gfx.fbx` for all T6 custom map modes. This must be a valid FBX with renderable mesh geometry.
- `bsp/map_col.fbx` optionally provides separate collision geometry. If omitted, `map_gfx.fbx` is reused for collision.
- `bsp/entities.json` for `>map,zm`

The zombies entity file must contain a JSON object with an `entities` array.
Every entity must be an object with string values, and the first entity must be
`worldspawn`. For `>map,zm`, every entity must define a non-empty `guid`, and
`worldspawn` must also define non-empty `lightgridoffset`, `lutmaterial`,
`fogtime`, `fsi`, `wsi`, `skyboxmodel`, `newsun`, `lightingquality`, and
`removeredundantlinks` keys. The backend treats `lutmaterial` as a material
dependency, `fsi` and `wsi` as `vision/*.vision` rawfile dependencies,
`skyboxmodel` as an xmodel dependency, and entity keys containing `zbarrier` as
`zbarrier` asset references.

The backend is tool-side support only. Runtime loading still requires building
and staging a complete map package and validating it in game.

For Zombies maps, the generated world assets are only one part of the runtime
package. The map must also provide the script, table, actor, animation, and
support assets expected by the selected Zombies setup. In particular, if a map
uses a stock zombie `animtree` and `animstatedef`, its asset lists or loaded
donor zones must include the referenced body movement and combat `xanim`
assets. Supplying only attack animations is not enough: stock scripts can enter
`walk` / `move` states while the actor remains immobile if the locomotion xanim
payload is absent.

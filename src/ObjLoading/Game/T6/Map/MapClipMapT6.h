#pragma once

#include "Asset/AssetCreationContext.h"
#include "Game/T6/Map/MapGeometryT6.h"
#include "Utils/MemoryManager.h"

namespace map
{
    [[nodiscard]] T6::clipMap_t* CreateClipMapT6(MemoryManager& memory,
                                                 AssetCreationContext& context,
                                                 const T6MapGeometry& geometry,
                                                 const T6::GfxWorld& gfxWorld);
}

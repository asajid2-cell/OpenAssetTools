#pragma once

#include "Asset/AssetCreationContext.h"
#include "Game/T6/Map/MapGeometryT6.h"
#include "SearchPath/ISearchPath.h"
#include "Utils/MemoryManager.h"

namespace map
{
    [[nodiscard]] T6::GfxWorld* CreateGfxWorldT6(MemoryManager& memory,
                                                 ISearchPath& searchPath,
                                                 AssetCreationContext& context,
                                                 const T6MapGeometry& geometry);
}

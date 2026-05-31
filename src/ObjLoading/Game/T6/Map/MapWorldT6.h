#pragma once

#include "Asset/AssetCreationContext.h"
#include "MapSourceT6.h"
#include "Zone/Definition/ZoneDefinition.h"
#include "Zone/Zone.h"

#include <string>

namespace map
{
    [[nodiscard]] std::string GetT6MapWorldAssetName(const std::string& mapName);

    [[nodiscard]] bool EmitPreGfxWorldAssetsT6(AssetCreationContext& context, Zone& zone, const std::string& mapAssetName);

    [[nodiscard]] bool EmitRuntimeWorldAssetsT6(AssetCreationContext& context,
                                                Zone& zone,
                                                const std::string& mapAssetName,
                                                const T6MapEntitySource& entitySource,
                                                ZoneDefinitionMapType mapType);

    [[nodiscard]] bool EmitBaseWorldAssetsT6(AssetCreationContext& context,
                                             Zone& zone,
                                             const std::string& mapAssetName,
                                             const T6MapEntitySource& entitySource,
                                             ZoneDefinitionMapType mapType);
}

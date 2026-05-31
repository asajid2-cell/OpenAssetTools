#pragma once

#include "Asset/IAssetCreator.h"
#include "SearchPath/ISearchPath.h"
#include "Zone/Definition/ZoneDefinition.h"
#include "Zone/Zone.h"

#include <memory>
#include <string>

namespace map
{
    std::unique_ptr<IAssetCreator> CreateLoaderT6(ISearchPath& searchPath, Zone& zone, ZoneDefinitionMapType mapType, std::string mapAssetName);
}

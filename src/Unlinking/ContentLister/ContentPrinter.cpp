#include "ContentPrinter.h"

#include "Game/T6/T6.h"
#include "Utils/Logging/Log.h"

#include <algorithm>
#include <format>
#include <iostream>
#include <string>

namespace
{
    template<typename Vec3> std::string Vec3ToString(const Vec3& value)
    {
        return std::format("({}, {}, {})", value.x, value.y, value.z);
    }

    std::string DpvsNodesToString(const T6::GfxWorld& gfxWorld)
    {
        if (!gfxWorld.dpvsPlanes.nodes || gfxWorld.nodeCount <= 0)
            return "(none)";

        std::string result;
        const auto count = std::min(gfxWorld.nodeCount, 16);
        for (auto nodeIndex = 0; nodeIndex < count; nodeIndex++)
        {
            if (!result.empty())
                result += ", ";
            result += std::format("{}", gfxWorld.dpvsPlanes.nodes[nodeIndex]);
        }

        return result;
    }

    std::string DpvsPlanesToString(const T6::GfxWorld& gfxWorld)
    {
        if (!gfxWorld.dpvsPlanes.planes || gfxWorld.planeCount <= 0)
            return "(none)";

        std::string result;
        const auto count = std::min(gfxWorld.planeCount, 4);
        for (auto planeIndex = 0; planeIndex < count; planeIndex++)
        {
            const auto& plane = gfxWorld.dpvsPlanes.planes[planeIndex];
            if (!result.empty())
                result += "; ";
            result += std::format("{}:{}@{}", planeIndex, static_cast<int>(plane.type), plane.dist);
        }

        return result;
    }

    std::string ClipLeavesToString(const T6::clipMap_t& clipMap)
    {
        if (!clipMap.leafs || clipMap.numLeafs <= 0)
            return "(none)";

        std::string result;
        const auto count = std::min(clipMap.numLeafs, 8u);
        for (auto leafIndex = 0u; leafIndex < count; leafIndex++)
        {
            const auto& leaf = clipMap.leafs[leafIndex];
            if (!result.empty())
                result += "; ";
            result += std::format(
                "{}:cluster={} leafBrushNode={} aabbs={}/{} contents=({}, {}) bounds={} -> {}",
                leafIndex,
                leaf.cluster,
                leaf.leafBrushNode,
                leaf.firstCollAabbIndex,
                leaf.collAabbCount,
                leaf.brushContents,
                leaf.terrainContents,
                Vec3ToString(leaf.mins),
                Vec3ToString(leaf.maxs));
        }

        return result;
    }

    std::string ClipAabbsToString(const T6::clipMap_t& clipMap)
    {
        if (!clipMap.aabbTrees || clipMap.aabbTreeCount <= 0)
            return "(none)";

        std::string result;
        const auto count = std::min(clipMap.aabbTreeCount, 8);
        for (auto aabbIndex = 0; aabbIndex < count; aabbIndex++)
        {
            const auto& aabb = clipMap.aabbTrees[aabbIndex];
            if (!result.empty())
                result += "; ";
            result += std::format(
                "{}:children={} material={} index={} origin={} half={}",
                aabbIndex,
                aabb.childCount,
                aabb.materialIndex,
                aabb.childCount > 0 ? aabb.u.firstChildIndex : aabb.u.partitionIndex,
                Vec3ToString(aabb.origin),
                Vec3ToString(aabb.halfSize));
        }

        return result;
    }

    std::string ClipLeafBrushNodesToString(const T6::clipMap_t& clipMap)
    {
        if (!clipMap.info.leafbrushNodes || clipMap.info.leafbrushNodesCount <= 0)
            return "(none)";

        std::string result;
        const auto count = std::min(clipMap.info.leafbrushNodesCount, 16u);
        for (auto nodeIndex = 0u; nodeIndex < count; nodeIndex++)
        {
            const auto& node = clipMap.info.leafbrushNodes[nodeIndex];
            if (!result.empty())
                result += "; ";

            if (node.leafBrushCount > 0)
            {
                result += std::format("{}:axis={} count={} contents={} firstBrush={}",
                                      nodeIndex,
                                      static_cast<int>(node.axis),
                                      node.leafBrushCount,
                                      node.contents,
                                      node.data.leaf.brushes ? node.data.leaf.brushes[0] : 0);
            }
            else
            {
                result += std::format("{}:axis={} count={} contents={} children=(dist={} range={} offsets={}/{})",
                                      nodeIndex,
                                      static_cast<int>(node.axis),
                                      node.leafBrushCount,
                                      node.contents,
                                      node.data.children.dist,
                                      node.data.children.range,
                                      node.data.children.childOffset[0],
                                      node.data.children.childOffset[1]);
            }
        }

        return result;
    }

    void PrintT6MapAssetDetails(const XAssetInfoGeneric& asset)
    {
        using namespace T6;

        if (!asset.m_ptr)
            return;

        if (asset.m_type == ASSET_TYPE_GFXWORLD)
        {
            const auto* gfxWorld = static_cast<const GfxWorld*>(asset.m_ptr);
            const auto& lightGrid = gfxWorld->lightGrid;
            const auto* firstSurface = gfxWorld->surfaceCount > 0 && gfxWorld->dpvs.surfaces ? &gfxWorld->dpvs.surfaces[0] : nullptr;
            const auto* firstCell = gfxWorld->dpvsPlanes.cellCount > 0 && gfxWorld->cells ? &gfxWorld->cells[0] : nullptr;
            const auto* firstAabb = firstCell && firstCell->aabbTreeCount > 0 && firstCell->aabbTree ? &firstCell->aabbTree[0] : nullptr;
            const auto* firstDpvsPlane = gfxWorld->planeCount > 0 && gfxWorld->dpvsPlanes.planes ? &gfxWorld->dpvsPlanes.planes[0] : nullptr;
            const auto* firstLightGridRow = lightGrid.rawRowData && lightGrid.rawRowDataSize >= sizeof(GfxLightGridRow)
                                                ? reinterpret_cast<const GfxLightGridRow*>(lightGrid.rawRowData)
                                                : nullptr;
            const auto* firstSurfaceMaterial = firstSurface && firstSurface->material ? firstSurface->material : nullptr;
            const auto* firstVertex = gfxWorld->draw.vd0.data && gfxWorld->draw.vertexCount > 0
                                          ? reinterpret_cast<const GfxPackedWorldVertex*>(gfxWorld->draw.vd0.data)
                                          : nullptr;

            con::debug(
                "  T6 gfxworld details: bounds={} -> {} planes={} nodes={} surfaces={} vertices={} indices={} vertexData=({}, {}) lightmaps={} firstLightmap=({}, {}) "
                "reflectionProbes={} cells={} cellBits={} models={} primaryLights={} sunPrimaryLightIndex={} materialMemory={} dpvsStaticSurfs={} "
                "streamInfo=({}, {}) dpvsVisData=({}, {}) lightGridMins=({}, {}, {}) lightGridMaxs=({}, {}, {}) lightGridRaw={} lightGridRows=({}, {}, {}, {}, {}) "
                "lightGridEntries={} lightGridColors={} "
                "lightGridCoeffs={} lightGridSkyVolumes={} dynEntClientCount=({}, {}) dynEntClientWordCount=({}, {}) "
                "firstSurface=(firstVertex={} vertexCount={} triCount={} baseIndex={} offsets=({}, {}) triBounds={} -> {} surfaceBounds={} -> {} material={} "
                "drawSurf=0x{:016X}/obj={} custom={} refl={} matSort={} primaryLight={} surfType={} prepass={} sortKey={}) "
                "firstDrawVertex=(xyz={} color=0x{:08X} tex=0x{:08X} normal=0x{:08X} tangent=0x{:08X} lmap=0x{:08X}) "
                "firstCellAabb=(children={} surfaces={} startSurf={} smodels={} bounds={} -> {}) firstDpvsPlane=(type={} dist={} normal={}) "
                "dpvsNodes=({}, {}, {}, {}) dpvsNodeWords=[{}] dpvsPlanes=[{}] sunLightType={} runtimePtrs=({}, {}, {}, {})",
                Vec3ToString(gfxWorld->mins),
                Vec3ToString(gfxWorld->maxs),
                gfxWorld->planeCount,
                gfxWorld->nodeCount,
                gfxWorld->surfaceCount,
                gfxWorld->draw.vertexCount,
                gfxWorld->draw.indexCount,
                gfxWorld->draw.vertexDataSize0,
                gfxWorld->draw.vertexDataSize1,
                gfxWorld->draw.lightmapCount,
                gfxWorld->draw.lightmaps && gfxWorld->draw.lightmapCount > 0 ? gfxWorld->draw.lightmaps[0].primary != nullptr : false,
                gfxWorld->draw.lightmaps && gfxWorld->draw.lightmapCount > 0 ? gfxWorld->draw.lightmaps[0].secondary != nullptr : false,
                gfxWorld->draw.reflectionProbeCount,
                gfxWorld->dpvsPlanes.cellCount,
                gfxWorld->cellBitsCount,
                gfxWorld->modelCount,
                gfxWorld->primaryLightCount,
                gfxWorld->sunPrimaryLightIndex,
                gfxWorld->materialMemoryCount,
                gfxWorld->dpvs.staticSurfaceCount,
                gfxWorld->streamInfo.aabbTreeCount,
                gfxWorld->streamInfo.leafRefCount,
                gfxWorld->dpvs.surfaceVisData[0] != nullptr,
                gfxWorld->dpvs.surfaceVisData[1] != nullptr,
                lightGrid.mins[0],
                lightGrid.mins[1],
                lightGrid.mins[2],
                lightGrid.maxs[0],
                lightGrid.maxs[1],
                lightGrid.maxs[2],
                lightGrid.rawRowDataSize,
                firstLightGridRow ? firstLightGridRow->colStart : 0,
                firstLightGridRow ? firstLightGridRow->colCount : 0,
                firstLightGridRow ? firstLightGridRow->zStart : 0,
                firstLightGridRow ? firstLightGridRow->zCount : 0,
                firstLightGridRow ? firstLightGridRow->firstEntry : 0,
                lightGrid.entryCount,
                lightGrid.colorCount,
                lightGrid.coeffCount,
                lightGrid.skyGridVolumeCount,
                gfxWorld->dpvsDyn.dynEntClientCount[0],
                gfxWorld->dpvsDyn.dynEntClientCount[1],
                gfxWorld->dpvsDyn.dynEntClientWordCount[0],
                gfxWorld->dpvsDyn.dynEntClientWordCount[1],
                firstSurface ? firstSurface->tris.firstVertex : 0,
                firstSurface ? firstSurface->tris.vertexCount : 0,
                firstSurface ? firstSurface->tris.triCount : 0,
                firstSurface ? firstSurface->tris.baseIndex : 0,
                firstSurface ? firstSurface->tris.vertexDataOffset0 : 0,
                firstSurface ? firstSurface->tris.vertexDataOffset1 : 0,
                firstSurface ? Vec3ToString(firstSurface->tris.mins) : "(none)",
                firstSurface ? Vec3ToString(firstSurface->tris.maxs) : "(none)",
                firstSurface ? Vec3ToString(firstSurface->bounds[0]) : "(none)",
                firstSurface ? Vec3ToString(firstSurface->bounds[1]) : "(none)",
                firstSurfaceMaterial && firstSurfaceMaterial->info.name ? firstSurfaceMaterial->info.name : "(none)",
                firstSurfaceMaterial ? static_cast<unsigned long long>(firstSurfaceMaterial->info.drawSurf.packed) : 0ull,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.objectId : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.customIndex : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.reflectionProbeIndex : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.materialSortedIndex : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.primaryLightIndex : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.surfType : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.prepass : 0,
                firstSurfaceMaterial ? firstSurfaceMaterial->info.drawSurf.fields.primarySortKey : 0,
                firstVertex ? Vec3ToString(firstVertex->xyz) : "(none)",
                firstVertex ? firstVertex->color.packed : 0u,
                firstVertex ? firstVertex->texCoord.packed : 0u,
                firstVertex ? firstVertex->normal.packed : 0u,
                firstVertex ? firstVertex->tangent.packed : 0u,
                firstVertex ? firstVertex->lmapCoord.packed : 0u,
                firstAabb ? firstAabb->childCount : 0,
                firstAabb ? firstAabb->surfaceCount : 0,
                firstAabb ? firstAabb->startSurfIndex : 0,
                firstAabb ? firstAabb->smodelIndexCount : 0,
                firstAabb ? Vec3ToString(firstAabb->mins) : "(none)",
                firstAabb ? Vec3ToString(firstAabb->maxs) : "(none)",
                firstDpvsPlane ? static_cast<int>(firstDpvsPlane->type) : -1,
                firstDpvsPlane ? firstDpvsPlane->dist : 0.0f,
                firstDpvsPlane ? Vec3ToString(firstDpvsPlane->normal) : "(none)",
                gfxWorld->nodeCount > 0 && gfxWorld->dpvsPlanes.nodes ? gfxWorld->dpvsPlanes.nodes[0] : 0,
                gfxWorld->nodeCount > 1 && gfxWorld->dpvsPlanes.nodes ? gfxWorld->dpvsPlanes.nodes[1] : 0,
                gfxWorld->nodeCount > 2 && gfxWorld->dpvsPlanes.nodes ? gfxWorld->dpvsPlanes.nodes[2] : 0,
                gfxWorld->nodeCount > 3 && gfxWorld->dpvsPlanes.nodes ? gfxWorld->dpvsPlanes.nodes[3] : 0,
                DpvsNodesToString(*gfxWorld),
                DpvsPlanesToString(*gfxWorld),
                gfxWorld->sunLight ? static_cast<int>(gfxWorld->sunLight->type) : -1,
                gfxWorld->cellCasterBits != nullptr,
                gfxWorld->sceneDynModel != nullptr,
                gfxWorld->shadowGeom != nullptr,
                gfxWorld->lightRegion != nullptr);
        }
        else if (asset.m_type == ASSET_TYPE_MATERIAL)
        {
            const auto* material = static_cast<const Material*>(asset.m_ptr);
            con::debug(
                "  T6 material details: name={} gameFlags={} sortKey={} surfaceTypeBits={} surfaceFlags={} contents={} hashIndex={} drawSurf=0x{:016X}/obj={} custom={} refl={} matSort={} primaryLight={} surfType={} prepass={} sortKey={}",
                material->info.name ? material->info.name : "(none)",
                material->info.gameFlags,
                static_cast<int>(material->info.sortKey),
                material->info.surfaceTypeBits,
                material->info.surfaceFlags,
                material->info.contents,
                material->info.hashIndex,
                static_cast<unsigned long long>(material->info.drawSurf.packed),
                material->info.drawSurf.fields.objectId,
                material->info.drawSurf.fields.customIndex,
                material->info.drawSurf.fields.reflectionProbeIndex,
                material->info.drawSurf.fields.materialSortedIndex,
                material->info.drawSurf.fields.primaryLightIndex,
                material->info.drawSurf.fields.surfType,
                material->info.drawSurf.fields.prepass,
                material->info.drawSurf.fields.primarySortKey);
        }
        else if (asset.m_type == ASSET_TYPE_COMWORLD)
        {
            const auto* comWorld = static_cast<const ComWorld*>(asset.m_ptr);
            const auto* firstLight = comWorld->primaryLightCount > 0 && comWorld->primaryLights ? &comWorld->primaryLights[0] : nullptr;
            const auto* secondLight = comWorld->primaryLightCount > 1 && comWorld->primaryLights ? &comWorld->primaryLights[1] : nullptr;
            con::debug("  T6 comworld details: inUse={} primaryLights={} firstType={} secondType={}",
                       comWorld->isInUse,
                       comWorld->primaryLightCount,
                       firstLight ? static_cast<int>(firstLight->type) : -1,
                       secondLight ? static_cast<int>(secondLight->type) : -1);
        }
        else if (asset.m_type == ASSET_TYPE_CLIPMAP || asset.m_type == ASSET_TYPE_CLIPMAP_PVS)
        {
            const auto* clipMap = static_cast<const clipMap_t*>(asset.m_ptr);
            const auto* firstLeaf = clipMap->numLeafs > 0 && clipMap->leafs ? &clipMap->leafs[0] : nullptr;
            const auto* firstModel = clipMap->numSubModels > 0 && clipMap->cmodels ? &clipMap->cmodels[0] : nullptr;
            const auto* firstNode = clipMap->numNodes > 0 && clipMap->nodes ? &clipMap->nodes[0] : nullptr;
            const auto* firstPlane = clipMap->info.planeCount > 0 && clipMap->info.planes ? &clipMap->info.planes[0] : nullptr;
            const auto* firstPartition = clipMap->partitionCount > 0 && clipMap->partitions ? &clipMap->partitions[0] : nullptr;
            const auto* firstAabb = clipMap->aabbTreeCount > 0 && clipMap->aabbTrees ? &clipMap->aabbTrees[0] : nullptr;
            const auto* firstLeafBrushNode =
                clipMap->info.leafbrushNodesCount > 0 && clipMap->info.leafbrushNodes ? &clipMap->info.leafbrushNodes[0] : nullptr;
            const auto* firstClipMaterial =
                clipMap->info.numMaterials > 0 && clipMap->info.materials ? &clipMap->info.materials[0] : nullptr;

            con::debug(
                "  T6 clipmap details: inUse={} staticModels={} verts={} tris={} partitions={} aabbTrees={} nodes={} leafs={} planes={} firstNode=({}, {}) firstPlane=(type={} dist={} normal={}) "
                "brushInfo=(materials={} brushSides={} leafBrushNodes={} leafBrushes={} brushVerts={} brushes={}) firstClipMaterial=(name={} surface={} contents={}) "
                "firstLeafBrushNode=(axis={} leafBrushCount={} contents={} firstBrush={}) "
                "firstLeafBounds={} -> {} firstLeaf=(cluster={} leafBrushNode={} contents=({}, {}) aabbs={} firstAabb={}) firstPartition=(tris={} firstTri={} uinds={} fuind={}) "
                "firstAabb=(origin={} half={} children={} material={} index={}) "
                "subModels={} cmodel0Bounds={} -> {} cmodel0LeafBounds={} -> {} cmodel0Info={} clusters={} clusterBytes={} visibility={} "
                "boxModelBounds={} -> {} boxModelLeafBounds={} -> {} boxModelLeaf=(cluster={} leafBrushNode={} contents=({}, {}) aabbs={} firstAabb={}) "
                "boxBrush=(contents={} sides={} verts={} mins={} maxs={} axial0=({}, {}) axial1=({}, {})) "
                "pInfo={} mapEnts={} dynEntCount=({}, {}, {}, {}) originalDynEntCount={} constraints={} maxRopes={} checksum={} "
                "leafSamples=[{}] aabbSamples=[{}] leafBrushNodeSamples=[{}]",
                clipMap->isInUse,
                clipMap->numStaticModels,
                clipMap->vertCount,
                clipMap->triCount,
                clipMap->partitionCount,
                clipMap->aabbTreeCount,
                clipMap->numNodes,
                clipMap->numLeafs,
                clipMap->info.planeCount,
                firstNode ? firstNode->children[0] : 0,
                firstNode ? firstNode->children[1] : 0,
                firstPlane ? static_cast<int>(firstPlane->type) : -1,
                firstPlane ? firstPlane->dist : 0.0f,
                firstPlane ? Vec3ToString(firstPlane->normal) : "(none)",
                clipMap->info.numMaterials,
                clipMap->info.numBrushSides,
                clipMap->info.leafbrushNodesCount,
                clipMap->info.numLeafBrushes,
                clipMap->info.numBrushVerts,
                clipMap->info.numBrushes,
                firstClipMaterial && firstClipMaterial->name ? firstClipMaterial->name : "(none)",
                firstClipMaterial ? firstClipMaterial->surfaceFlags : 0,
                firstClipMaterial ? firstClipMaterial->contentFlags : 0,
                firstLeafBrushNode ? static_cast<int>(firstLeafBrushNode->axis) : 0,
                firstLeafBrushNode ? firstLeafBrushNode->leafBrushCount : 0,
                firstLeafBrushNode ? firstLeafBrushNode->contents : 0,
                firstLeafBrushNode && firstLeafBrushNode->leafBrushCount > 0 && firstLeafBrushNode->data.leaf.brushes
                    ? firstLeafBrushNode->data.leaf.brushes[0]
                    : 0,
                firstLeaf ? Vec3ToString(firstLeaf->mins) : "(none)",
                firstLeaf ? Vec3ToString(firstLeaf->maxs) : "(none)",
                firstLeaf ? firstLeaf->cluster : 0,
                firstLeaf ? firstLeaf->leafBrushNode : 0,
                firstLeaf ? firstLeaf->brushContents : 0,
                firstLeaf ? firstLeaf->terrainContents : 0,
                firstLeaf ? firstLeaf->collAabbCount : 0u,
                firstLeaf ? firstLeaf->firstCollAabbIndex : 0u,
                firstPartition ? static_cast<int>(firstPartition->triCount) : 0,
                firstPartition ? firstPartition->firstTri : 0,
                firstPartition ? firstPartition->nuinds : 0,
                firstPartition ? firstPartition->fuind : 0,
                firstAabb ? Vec3ToString(firstAabb->origin) : "(none)",
                firstAabb ? Vec3ToString(firstAabb->halfSize) : "(none)",
                firstAabb ? firstAabb->childCount : 0,
                firstAabb ? firstAabb->materialIndex : 0,
                firstAabb ? (firstAabb->childCount > 0 ? firstAabb->u.firstChildIndex : firstAabb->u.partitionIndex) : 0,
                clipMap->numSubModels,
                firstModel ? Vec3ToString(firstModel->mins) : "(none)",
                firstModel ? Vec3ToString(firstModel->maxs) : "(none)",
                firstModel ? Vec3ToString(firstModel->leaf.mins) : "(none)",
                firstModel ? Vec3ToString(firstModel->leaf.maxs) : "(none)",
                firstModel && firstModel->info != nullptr,
                clipMap->numClusters,
                clipMap->clusterBytes,
                clipMap->visibility != nullptr,
                Vec3ToString(clipMap->box_model.mins),
                Vec3ToString(clipMap->box_model.maxs),
                Vec3ToString(clipMap->box_model.leaf.mins),
                Vec3ToString(clipMap->box_model.leaf.maxs),
                clipMap->box_model.leaf.cluster,
                clipMap->box_model.leaf.leafBrushNode,
                clipMap->box_model.leaf.brushContents,
                clipMap->box_model.leaf.terrainContents,
                clipMap->box_model.leaf.collAabbCount,
                clipMap->box_model.leaf.firstCollAabbIndex,
                clipMap->box_brush ? clipMap->box_brush->contents : 0,
                clipMap->box_brush ? clipMap->box_brush->numsides : 0,
                clipMap->box_brush ? clipMap->box_brush->numverts : 0,
                clipMap->box_brush ? Vec3ToString(clipMap->box_brush->mins) : "(none)",
                clipMap->box_brush ? Vec3ToString(clipMap->box_brush->maxs) : "(none)",
                clipMap->box_brush ? clipMap->box_brush->axial_cflags[0][0] : 0,
                clipMap->box_brush ? clipMap->box_brush->axial_sflags[0][0] : 0,
                clipMap->box_brush ? clipMap->box_brush->axial_cflags[1][0] : 0,
                clipMap->box_brush ? clipMap->box_brush->axial_sflags[1][0] : 0,
                clipMap->pInfo != nullptr,
                clipMap->mapEnts != nullptr,
                clipMap->dynEntCount[0],
                clipMap->dynEntCount[1],
                clipMap->dynEntCount[2],
                clipMap->dynEntCount[3],
                clipMap->originalDynEntCount,
                clipMap->num_constraints,
                clipMap->max_ropes,
                clipMap->checksum,
                ClipLeavesToString(*clipMap),
                ClipAabbsToString(*clipMap),
                ClipLeafBrushNodesToString(*clipMap));
        }
        else if (asset.m_type == ASSET_TYPE_GAMEWORLD_SP)
        {
            const auto* gameWorld = static_cast<const GameWorldSp*>(asset.m_ptr);
            const auto& path = gameWorld->path;
            const auto* firstNode = path.nodeCount > 0 && path.nodes ? &path.nodes[0] : nullptr;
            const auto* firstTree = path.nodeTreeCount > 0 && path.nodeTree ? &path.nodeTree[0] : nullptr;
            con::debug(
                "  T6 gameworldsp details: nodes={} originalNodes={} visBytes={} smoothBytes={} nodeTrees={} pathVis={} smoothCache={} "
                "firstNode=(type={} origin={} radius={} links={}) firstTree=(axis={} dist={} leafCount={})",
                path.nodeCount,
                path.originalNodeCount,
                path.visBytes,
                path.smoothBytes,
                path.nodeTreeCount,
                path.pathVis != nullptr,
                path.smoothCache != nullptr,
                firstNode ? static_cast<int>(firstNode->constant.type) : -1,
                firstNode ? Vec3ToString(firstNode->constant.vOrigin) : "(none)",
                firstNode ? firstNode->constant.fRadius : 0.0f,
                firstNode ? firstNode->constant.totalLinkCount : 0,
                firstTree ? firstTree->axis : 0,
                firstTree ? firstTree->dist : 0.0f,
                firstTree && firstTree->axis < 0 ? firstTree->u.s.nodeCount : 0);
        }
        else if (asset.m_type == ASSET_TYPE_GAMEWORLD_MP)
        {
            const auto* gameWorld = static_cast<const GameWorldMp*>(asset.m_ptr);
            const auto& path = gameWorld->path;
            const auto* firstNode = path.nodeCount > 0 && path.nodes ? &path.nodes[0] : nullptr;
            const auto* firstTree = path.nodeTreeCount > 0 && path.nodeTree ? &path.nodeTree[0] : nullptr;
            con::debug(
                "  T6 gameworldmp details: nodes={} originalNodes={} visBytes={} smoothBytes={} nodeTrees={} pathVis={} smoothCache={} "
                "firstNode=(type={} origin={} radius={} links={}) firstTree=(axis={} dist={} leafCount={})",
                path.nodeCount,
                path.originalNodeCount,
                path.visBytes,
                path.smoothBytes,
                path.nodeTreeCount,
                path.pathVis != nullptr,
                path.smoothCache != nullptr,
                firstNode ? static_cast<int>(firstNode->constant.type) : -1,
                firstNode ? Vec3ToString(firstNode->constant.vOrigin) : "(none)",
                firstNode ? firstNode->constant.fRadius : 0.0f,
                firstNode ? firstNode->constant.totalLinkCount : 0,
                firstTree ? firstTree->axis : 0,
                firstTree ? firstTree->dist : 0.0f,
                firstTree && firstTree->axis < 0 ? firstTree->u.s.nodeCount : 0);
        }
        else if (asset.m_type == ASSET_TYPE_MAP_ENTS)
        {
            const auto* mapEnts = static_cast<const MapEnts*>(asset.m_ptr);
            con::debug(
                "  T6 mapents details: chars={} triggerModels={} triggerHulls={} triggerSlabs={}",
                mapEnts->numEntityChars,
                mapEnts->trigger.count,
                mapEnts->trigger.hullCount,
                mapEnts->trigger.slabCount);
        }
        else if (asset.m_type == ASSET_TYPE_ADDON_MAP_ENTS)
        {
            const auto* addonMapEnts = static_cast<const AddonMapEnts*>(asset.m_ptr);
            con::debug(
                "  T6 addonmapents details: chars={} triggerModels={} triggerHulls={} triggerSlabs={} subModels={} info={} cmodels={} models={}",
                addonMapEnts->numEntityChars,
                addonMapEnts->trigger.count,
                addonMapEnts->trigger.hullCount,
                addonMapEnts->trigger.slabCount,
                addonMapEnts->numSubModels,
                addonMapEnts->info != nullptr,
                addonMapEnts->cmodels != nullptr,
                addonMapEnts->models != nullptr);
        }
    }
} // namespace

ContentPrinter::ContentPrinter(const Zone& zone)
    : m_zone(zone)
{
}

void ContentPrinter::PrintContent() const
{
    const auto& pools = m_zone.m_pools;
    const auto* game = IGame::GetGameById(m_zone.m_game_id);
    con::info("Zone '{}' ({})", m_zone.m_name, game->GetShortName());
    con::info("Content:");

    for (const auto& asset : pools)
    {
        con::info("{}, {}", *game->GetAssetTypeName(asset->m_type), asset->m_name);
        if (m_zone.m_game_id == GameId::T6)
            PrintT6MapAssetDetails(*asset);
    }

    con::info("");
}

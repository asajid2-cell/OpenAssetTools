#include "Game/T6/CommonT6.h"
#include "MapGfxWorldT6.h"

#include "Utils/Alignment.h"
#include "Utils/Logging/Log.h"
#include "Utils/Pack.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

using namespace T6;

namespace
{
    constexpr auto DEFAULT_STATIC_LIGHT_INDEX = 0u;
    constexpr auto DEFAULT_SUN_LIGHT_INDEX = 1u;
    constexpr auto DEFAULT_LIGHT_COUNT = 2u;
    constexpr auto DEFAULT_SURFACE_LIGHTMAP = 0;
    constexpr auto DEFAULT_SURFACE_REFLECTION_PROBE = 0;
    constexpr auto DEFAULT_SURFACE_FLAGS = GFX_SURFACE_CASTS_SUN_SHADOW | GFX_SURFACE_CASTS_SHADOW;
    constexpr auto DEFAULT_LIGHTGRID_MAX_X = 200u;
    constexpr auto DEFAULT_LIGHTGRID_MAX_Y = 200u;
    constexpr auto DEFAULT_LIGHTGRID_MAX_Z = 50u;
    constexpr auto LEGACY_LIGHTGRID_COL_COUNT = 0x1000u;
    constexpr auto LEGACY_LIGHTGRID_Z_COUNT = 0xFFu;
    constexpr auto LEGACY_LIGHTGRID_ENTRY_COUNT = 60000u;
    constexpr auto LEGACY_LIGHTGRID_COLOR_COUNT = 0x1000u;
    constexpr auto GENERATED_REFLECTION_COLOR = 128u;
    constexpr auto GENERATED_LIGHTING_COLOR = 255u;
    constexpr auto LEGACY_LIGHTGRID_COLOR = GENERATED_LIGHTING_COLOR;
    constexpr auto DEFAULT_DYN_ENTITY_CLIENT_RESERVE_COUNT = 256u;
    constexpr auto MIN_VERTEX_DATA1_BYTES = 0x20u;
    constexpr auto DEFAULT_SUN_DIRECTION_X = -0.242f;
    constexpr auto DEFAULT_SUN_DIRECTION_Y = -0.841f;
    constexpr auto DEFAULT_SUN_DIRECTION_Z = -0.485f;

    constexpr const char* FALLBACK_SURFACE_MATERIAL = "wpc/concrete_sidewalk_dirty";
    constexpr const char* REFLECTION_PROBE_IMAGE_SUFFIX = "reflection_probe0";
    constexpr const char* LIGHTMAP_PRIMARY_IMAGE_SUFFIX = "lightmap0";
    constexpr const char* LIGHTMAP_SECONDARY_IMAGE_SUFFIX = "lightmap0_secondary";
    constexpr const char* OUTDOOR_IMAGE = "$outdoor";
    constexpr auto T6_DXGI_FORMAT_R8G8B8A8_UNORM = 0x1c;
    constexpr auto T6_IWI_FLAG_NOMIPMAPS = 1 << 1;
    constexpr auto T6_IWI_FLAG_CUBEMAP = 1 << 2;

    enum class GeneratedMapImageKind
    {
        Lightmap,
        ReflectionProbe,
        Outdoor
    };

    [[nodiscard]] const char* GetGeneratedMapImageKindName(const GeneratedMapImageKind kind)
    {
        switch (kind)
        {
        case GeneratedMapImageKind::Lightmap:
            return "lightmap";

        case GeneratedMapImageKind::ReflectionProbe:
            return "reflection probe";

        case GeneratedMapImageKind::Outdoor:
            return "outdoor lookup";
        }

        return "map image";
    }

    [[nodiscard]] MapType GetExpectedMapImageType(const GeneratedMapImageKind kind)
    {
        return kind == GeneratedMapImageKind::ReflectionProbe ? MAPTYPE_CUBE : MAPTYPE_2D;
    }

    [[nodiscard]] ImageCategory GetExpectedMapImageCategory(const GeneratedMapImageKind kind)
    {
        return kind == GeneratedMapImageKind::Lightmap ? IMG_CATEGORY_LIGHTMAP : IMG_CATEGORY_AUTO_GENERATED;
    }

    void UpdateAabbWithPoint(const vec3_t& point, vec3_t& mins, vec3_t& maxs)
    {
        mins.x = std::min(mins.x, point.x);
        mins.y = std::min(mins.y, point.y);
        mins.z = std::min(mins.z, point.z);
        maxs.x = std::max(maxs.x, point.x);
        maxs.y = std::max(maxs.y, point.y);
        maxs.z = std::max(maxs.z, point.z);
    }

    void UpdateAabb(const vec3_t& newMins, const vec3_t& newMaxs, vec3_t& mins, vec3_t& maxs)
    {
        UpdateAabbWithPoint(newMins, mins, maxs);
        UpdateAabbWithPoint(newMaxs, mins, maxs);
    }

    [[nodiscard]] float SafeReciprocal(const float value)
    {
        if (value == 0.0f)
            return 1.0f;

        return 1.0f / value;
    }

    template<typename T> T* AllocZeroed(MemoryManager& memory, const std::size_t count = 1u)
    {
        auto* result = memory.Alloc<T>(count);
        std::memset(result, 0, sizeof(T) * count);
        return result;
    }

    template<typename T> T* AllocRuntimeArrayPointer(MemoryManager& memory, const std::size_t count)
    {
        // T6 runtime code sometimes distinguishes a null pointer from an empty asset-owned buffer.
        return AllocZeroed<T>(memory, std::max<std::size_t>(count, 1u));
    }

    [[nodiscard]] XAssetInfo<GfxImage>* TryLoadAuthoredMapImage(AssetCreationContext& context, const std::string& assetName)
    {
        if (assetName.empty())
            return nullptr;

        const auto embeddedAssetName = std::format("*{}", assetName);
        if (context.HasAsset<AssetImage>(embeddedAssetName))
        {
            auto* dependency = context.LoadDependencyGeneric(AssetImage::EnumEntry, embeddedAssetName, false);
            if (dependency && !dependency->IsReference())
                return static_cast<XAssetInfo<GfxImage>*>(dependency);
        }

        if (context.HasAsset<AssetImage>(assetName))
        {
            auto* dependency = context.LoadDependencyGeneric(AssetImage::EnumEntry, assetName, false);
            if (dependency && !dependency->IsReference())
                return static_cast<XAssetInfo<GfxImage>*>(dependency);
        }

        return nullptr;
    }

    void SetMapImageRuntimeMetadata(GfxImage& image,
                                    const GeneratedMapImageKind kind,
                                    const unsigned int dataSize,
                                    const char levelCount)
    {
        image.hash = Common::R_HashString(image.name, 0u);
        image.delayLoadPixels = false;
        image.category = GetExpectedMapImageCategory(kind);
        image.streaming = 0;
        image.streamedPartCount = 0;
        image.pixels = nullptr;
        image.skippedMipLevels = 0;
        image.baseSize = dataSize;
        image.loadedSize = dataSize;
        image.cardMemory.platform[0] = static_cast<int>(dataSize);
        image.cardMemory.platform[1] = static_cast<int>(dataSize);
        image.levelCount = levelCount > 0 ? levelCount : 1;
    }

    void PopulateGeneratedMapImage(MemoryManager& memory, GfxImage& image, const char* assetName, const GeneratedMapImageKind kind)
    {
        image.name = memory.Dup(assetName);
        image.picmip.platform[0] = 0;
        image.picmip.platform[1] = 0;
        image.noPicmip = true;
        image.semantic = TS_FUNCTION;
        image.track = 0;
        image.width = 1u;
        image.height = 1u;
        image.depth = 1u;

        const auto isCube = kind == GeneratedMapImageKind::ReflectionProbe;
        image.mapType = static_cast<char>(GetExpectedMapImageType(kind));

        const auto faceCount = isCube ? 6u : 1u;
        const auto dataSize = faceCount * 4u;
        auto* loadDef = static_cast<GfxImageLoadDef*>(memory.AllocRaw(offsetof(GfxImageLoadDef, data) + dataSize));
        image.texture.loadDef = loadDef;
        loadDef->levelCount = 1;
        loadDef->flags = T6_IWI_FLAG_NOMIPMAPS | (isCube ? T6_IWI_FLAG_CUBEMAP : 0);
        loadDef->format = T6_DXGI_FORMAT_R8G8B8A8_UNORM;
        loadDef->resourceSize = static_cast<int>(dataSize);

        const auto color = kind == GeneratedMapImageKind::Lightmap ? GENERATED_LIGHTING_COLOR : GENERATED_REFLECTION_COLOR;
        const unsigned char pixel[4] = {
            static_cast<unsigned char>(color),
            static_cast<unsigned char>(color),
            static_cast<unsigned char>(color),
            255u};
        for (auto face = 0u; face < faceCount; face++)
            std::memcpy(&loadDef->data[face * 4u], pixel, sizeof(pixel));

        SetMapImageRuntimeMetadata(image, kind, dataSize, loadDef->levelCount);
    }

    void NormalizeMapImageForRuntime(MemoryManager& memory, GfxImage& image, const char* assetName, const GeneratedMapImageKind kind)
    {
        const auto expectedMapType = GetExpectedMapImageType(kind);
        const auto* loadDef = image.texture.loadDef;

        if (!loadDef || loadDef->resourceSize <= 0 || image.mapType != expectedMapType)
        {
            con::warn(
                "T6 custom map {} image \"{}\" is missing runtime-ready image data; replacing it with a generated placeholder.",
                GetGeneratedMapImageKindName(kind),
                assetName);
            PopulateGeneratedMapImage(memory, image, assetName, kind);
            return;
        }

        if (!image.name || !image.name[0])
            image.name = memory.Dup(assetName);

        SetMapImageRuntimeMetadata(image, kind, static_cast<unsigned int>(loadDef->resourceSize), loadDef->levelCount);
    }

    [[nodiscard]] XAssetInfo<GfxImage>* CreateGeneratedMapImage(MemoryManager& memory,
                                                                AssetCreationContext& context,
                                                                const char* assetName,
                                                                const GeneratedMapImageKind kind,
                                                                const bool tryAuthoredImage = true)
    {
        if (tryAuthoredImage)
        {
            const auto existing = TryLoadAuthoredMapImage(context, assetName);
            if (existing)
            {
                NormalizeMapImageForRuntime(memory, *existing->Asset(), assetName, kind);
                return existing;
            }
        }

        auto* image = AllocZeroed<GfxImage>(memory);
        PopulateGeneratedMapImage(memory, *image, assetName, kind);
        con::debug("Generated T6 custom-map world image \"{}\".", assetName);
        return context.AddAsset<AssetImage>(assetName, image);
    }

    void LoadDrawData(MemoryManager& memory, const map::T6MapWorldGeometry& geometry, GfxWorld& gfxWorld)
    {
        const auto vertexCount = geometry.m_vertices.size();
        gfxWorld.draw.vertexCount = static_cast<unsigned int>(vertexCount);
        const auto vertexDataBytes = vertexCount * sizeof(GfxPackedWorldVertex);
        gfxWorld.draw.vertexDataSize0 = static_cast<unsigned int>(vertexDataBytes);

        auto* vertexData = memory.Alloc<byte128>(gfxWorld.draw.vertexDataSize0);
        std::memset(vertexData, 0, gfxWorld.draw.vertexDataSize0 * sizeof(byte128));
        auto* vertexBuffer = reinterpret_cast<GfxPackedWorldVertex*>(vertexData);

        for (auto vertexIndex = 0u; vertexIndex < vertexCount; vertexIndex++)
        {
            const auto& sourceVertex = geometry.m_vertices[vertexIndex];
            auto& targetVertex = vertexBuffer[vertexIndex];

            targetVertex.xyz = sourceVertex.m_position;
            targetVertex.color.packed = pack32::Vec4PackGfxColor(sourceVertex.m_color.v);
            targetVertex.texCoord.packed = pack32::Vec2PackTexCoordsUV(sourceVertex.m_tex_coord.v);
            targetVertex.normal.packed = pack32::Vec3PackUnitVecThirdBased(sourceVertex.m_normal.v);
            targetVertex.tangent.packed = pack32::Vec3PackUnitVecThirdBased(sourceVertex.m_tangent.v);
            targetVertex.binormalSign = 0.0f;
            targetVertex.lmapCoord.packed = 0u;
        }

        gfxWorld.draw.vd0.data = vertexData;

        gfxWorld.draw.vertexDataSize1 = MIN_VERTEX_DATA1_BYTES;
        gfxWorld.draw.vd1.data = memory.Alloc<byte128>(gfxWorld.draw.vertexDataSize1);
        std::memset(gfxWorld.draw.vd1.data, 0, gfxWorld.draw.vertexDataSize1 * sizeof(byte128));

        const auto indexCount = geometry.m_indices.size();
        gfxWorld.draw.indexCount = static_cast<int>(indexCount);
        gfxWorld.draw.indices = memory.Alloc<std::uint16_t>(indexCount);
        std::memcpy(gfxWorld.draw.indices, geometry.m_indices.data(), sizeof(std::uint16_t) * indexCount);
    }

    [[nodiscard]] std::string_view SafeStringView(const char* value)
    {
        return value ? std::string_view(value) : std::string_view();
    }

    [[nodiscard]] bool IsPlaceholderSurfaceMaterial(const std::string_view materialName)
    {
        return materialName == "lambert1" || materialName == "white" || materialName == ",white" || materialName == "mc/lambert1"
               || materialName == ",mc/lambert1";
    }

    [[nodiscard]] bool HasKnownModelMaterialPrefix(const std::string_view materialName)
    {
        return materialName.starts_with("mc/") || materialName.starts_with(",mc/") || materialName.starts_with("mlv/")
               || materialName.starts_with(",mlv/");
    }

    [[nodiscard]] bool HasKnownModelTechniqueSetPrefix(const std::string_view techniqueSetName)
    {
        return techniqueSetName.starts_with("mc_") || techniqueSetName.starts_with("mlv_");
    }

    [[nodiscard]] bool IsKnownIncompatibleWorldSurfaceMaterial(const Material& material)
    {
        if (material.techniqueSet && material.techniqueSet->name && HasKnownModelTechniqueSetPrefix(material.techniqueSet->name))
            return true;

        return HasKnownModelMaterialPrefix(SafeStringView(material.info.name));
    }

    [[nodiscard]] XAssetInfo<Material>* LoadFallbackSurfaceMaterial(AssetCreationContext& context)
    {
        auto* fallbackMaterial = context.LoadDependency<AssetMaterial>(FALLBACK_SURFACE_MATERIAL);
        if (!fallbackMaterial)
            return nullptr;

        if (IsKnownIncompatibleWorldSurfaceMaterial(*fallbackMaterial->Asset()))
        {
            con::error(
                "T6 custom map fallback surface material \"{}\" is not world-compatible. Use a renderable T6 world material such as wpc/...",
                FALLBACK_SURFACE_MATERIAL);
            return nullptr;
        }

        return fallbackMaterial;
    }

    [[nodiscard]] XAssetInfo<Material>* LoadSurfaceMaterial(AssetCreationContext& context, const map::T6MapSurface& surface)
    {
        std::string materialName = FALLBACK_SURFACE_MATERIAL;
        if (surface.m_material.m_type == map::T6MapMaterialType::Texture && !surface.m_material.m_name.empty())
            materialName = surface.m_material.m_name;

        if (IsPlaceholderSurfaceMaterial(materialName))
        {
            con::warn(
                "T6 custom map surface material \"{}\" is a DCC placeholder; using world-material fallback \"{}\".",
                materialName,
                FALLBACK_SURFACE_MATERIAL);
            return LoadFallbackSurfaceMaterial(context);
        }

        auto* material = context.LoadDependency<AssetMaterial>(materialName);
        if (material)
        {
            if (material->Asset()->info.drawSurf.packed == 0u)
            {
                con::warn(
                    "T6 custom map surface material \"{}\" has a zero drawSurf; using world-material fallback \"{}\".",
                    materialName,
                    FALLBACK_SURFACE_MATERIAL);

                if (materialName != FALLBACK_SURFACE_MATERIAL)
                    return LoadFallbackSurfaceMaterial(context);

                con::error("T6 custom map fallback surface material \"{}\" has a zero drawSurf.", FALLBACK_SURFACE_MATERIAL);
                return nullptr;
            }

            if (IsKnownIncompatibleWorldSurfaceMaterial(*material->Asset()))
            {
                const auto techniqueSetName =
                    material->Asset()->techniqueSet ? SafeStringView(material->Asset()->techniqueSet->name) : std::string_view();
                const auto techniqueSetSuffix =
                    techniqueSetName.empty() ? std::string() : std::format(" with technique set \"{}\"", techniqueSetName);
                con::error(
                    "T6 custom map surface material \"{}\"{} is a model-material family. Generated GfxWorld surfaces require renderable "
                    "T6 world materials such as wpc/...; model materials can link but render black in-game.",
                    materialName,
                    techniqueSetSuffix);
                return nullptr;
            }

            return material;
        }

        if (materialName != FALLBACK_SURFACE_MATERIAL)
        {
            con::warn(
                "Unable to load T6 custom map surface material \"{}\"; using world-material fallback \"{}\".",
                materialName,
                FALLBACK_SURFACE_MATERIAL);
            return LoadFallbackSurfaceMaterial(context);
        }

        return nullptr;
    }

    [[nodiscard]] GfxDrawSurf BuildSurfaceDrawSurf(const GfxSurface& surface, const unsigned int surfaceIndex)
    {
        auto drawSurf = surface.material->info.drawSurf;
        drawSurf.fields.objectId = surfaceIndex;
        drawSurf.fields.customIndex = static_cast<unsigned>(surface.lightmapIndex);
        drawSurf.fields.reflectionProbeIndex = static_cast<unsigned>(surface.reflectionProbeIndex);
        drawSurf.fields.primaryLightIndex = static_cast<unsigned>(surface.primaryLightIndex);
        return drawSurf;
    }

    void LoadMaterialMemory(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        if (gfxWorld.surfaceCount <= 0)
        {
            gfxWorld.materialMemoryCount = 0;
            gfxWorld.materialMemory = nullptr;
            return;
        }

        auto* materialMemory = memory.Alloc<MaterialMemory>(gfxWorld.surfaceCount);
        auto materialMemoryCount = 0;

        for (auto surfaceIndex = 0; surfaceIndex < gfxWorld.surfaceCount; surfaceIndex++)
        {
            auto* material = gfxWorld.dpvs.surfaces[surfaceIndex].material;
            if (!material)
                continue;

            auto alreadyAdded = false;
            for (auto materialIndex = 0; materialIndex < materialMemoryCount; materialIndex++)
            {
                if (materialMemory[materialIndex].material == material)
                {
                    alreadyAdded = true;
                    break;
                }
            }

            if (alreadyAdded)
                continue;

            materialMemory[materialMemoryCount].material = material;
            materialMemory[materialMemoryCount].memory = 0;
            materialMemoryCount++;
        }

        gfxWorld.materialMemoryCount = materialMemoryCount;
        gfxWorld.materialMemory = materialMemoryCount > 0 ? materialMemory : nullptr;
    }

    bool LoadMapSurfaces(MemoryManager& memory, AssetCreationContext& context, const map::T6MapWorldGeometry& geometry, GfxWorld& gfxWorld)
    {
        LoadDrawData(memory, geometry, gfxWorld);

        const auto surfaceCount = geometry.m_surfaces.size();
        gfxWorld.surfaceCount = static_cast<int>(surfaceCount);
        gfxWorld.dpvs.staticSurfaceCount = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.surfaces = AllocZeroed<GfxSurface>(memory, surfaceCount);

        for (auto surfaceIndex = 0u; surfaceIndex < surfaceCount; surfaceIndex++)
        {
            const auto& sourceSurface = geometry.m_surfaces[surfaceIndex];
            auto& targetSurface = gfxWorld.dpvs.surfaces[surfaceIndex];

            targetSurface.primaryLightIndex = DEFAULT_SUN_LIGHT_INDEX;
            targetSurface.lightmapIndex = DEFAULT_SURFACE_LIGHTMAP;
            targetSurface.reflectionProbeIndex = DEFAULT_SURFACE_REFLECTION_PROBE;
            targetSurface.flags = DEFAULT_SURFACE_FLAGS;

            targetSurface.tris.triCount = static_cast<std::uint16_t>(sourceSurface.m_tri_count);
            targetSurface.tris.baseIndex = static_cast<int>(sourceSurface.m_first_index);
            targetSurface.tris.vertexDataOffset0 = static_cast<int>(sourceSurface.m_first_vertex * sizeof(GfxPackedWorldVertex));
            targetSurface.tris.vertexDataOffset1 = 0;
            targetSurface.tris.firstVertex = static_cast<int>(sourceSurface.m_first_vertex);

            auto surfaceVertexCount = 0u;
            for (auto indexPosition = 0u; indexPosition < sourceSurface.m_tri_count * 3u; indexPosition++)
                surfaceVertexCount = std::max(surfaceVertexCount, static_cast<unsigned>(gfxWorld.draw.indices[sourceSurface.m_first_index + indexPosition]) + 1u);
            targetSurface.tris.vertexCount = static_cast<std::uint16_t>(surfaceVertexCount);

            auto* material = LoadSurfaceMaterial(context, sourceSurface);
            if (!material)
            {
                con::error("Unable to load a fallback surface material for T6 custom map geometry.");
                return false;
            }

            targetSurface.material = material->Asset();

            auto* firstVertex = reinterpret_cast<GfxPackedWorldVertex*>(reinterpret_cast<unsigned char*>(gfxWorld.draw.vd0.data)
                                                                          + targetSurface.tris.vertexDataOffset0);
            targetSurface.bounds[0] = firstVertex[0].xyz;
            targetSurface.bounds[1] = firstVertex[0].xyz;
            targetSurface.tris.mins = firstVertex[0].xyz;
            targetSurface.tris.maxs = firstVertex[0].xyz;

            for (auto indexPosition = 0u; indexPosition < targetSurface.tris.triCount * 3u; indexPosition++)
            {
                const auto vertexIndex = gfxWorld.draw.indices[targetSurface.tris.baseIndex + indexPosition];
                const auto& vertexPosition = firstVertex[vertexIndex].xyz;
                UpdateAabbWithPoint(vertexPosition, targetSurface.bounds[0], targetSurface.bounds[1]);
                UpdateAabbWithPoint(vertexPosition, targetSurface.tris.mins, targetSurface.tris.maxs);
            }
        }

        gfxWorld.dpvs.sortedSurfIndex = memory.Alloc<std::uint16_t>(surfaceCount);
        for (auto surfaceIndex = 0u; surfaceIndex < surfaceCount; surfaceIndex++)
            gfxWorld.dpvs.sortedSurfIndex[surfaceIndex] = static_cast<std::uint16_t>(surfaceIndex);

        gfxWorld.dpvs.surfaceMaterials = AllocZeroed<GfxDrawSurf_align4>(memory, surfaceCount);
        for (auto surfaceIndex = 0u; surfaceIndex < surfaceCount; surfaceIndex++)
        {
            if (gfxWorld.dpvs.surfaces[surfaceIndex].material)
                gfxWorld.dpvs.surfaceMaterials[surfaceIndex] = BuildSurfaceDrawSurf(gfxWorld.dpvs.surfaces[surfaceIndex], surfaceIndex);
        }
        gfxWorld.dpvs.litSurfsBegin = 0u;
        gfxWorld.dpvs.litSurfsEnd = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.emissiveOpaqueSurfsBegin = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.emissiveOpaqueSurfsEnd = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.emissiveTransSurfsBegin = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.emissiveTransSurfsEnd = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.litTransSurfsBegin = static_cast<unsigned int>(surfaceCount);
        gfxWorld.dpvs.litTransSurfsEnd = static_cast<unsigned int>(surfaceCount);

        const auto alignedSurfaceCount = utils::Align(surfaceCount, 128uz);
        gfxWorld.dpvs.surfaceVisDataCount = static_cast<unsigned int>(alignedSurfaceCount);
        gfxWorld.dpvs.surfaceVisData[0] = memory.Alloc<raw_byte128>(alignedSurfaceCount);
        gfxWorld.dpvs.surfaceVisData[1] = memory.Alloc<raw_byte128>(alignedSurfaceCount);
        gfxWorld.dpvs.surfaceVisData[2] = memory.Alloc<raw_byte128>(alignedSurfaceCount);
        gfxWorld.dpvs.surfaceVisDataCameraSaved = memory.Alloc<raw_byte128>(alignedSurfaceCount);
        gfxWorld.dpvs.surfaceCastsShadow = memory.Alloc<raw_byte128>(alignedSurfaceCount);
        gfxWorld.dpvs.surfaceCastsSunShadow = memory.Alloc<raw_byte128>(alignedSurfaceCount);
        std::memset(gfxWorld.dpvs.surfaceVisData[0], 0xFF, alignedSurfaceCount * sizeof(raw_byte128));
        std::memset(gfxWorld.dpvs.surfaceVisData[1], 0xFF, alignedSurfaceCount * sizeof(raw_byte128));
        std::memset(gfxWorld.dpvs.surfaceVisData[2], 0xFF, alignedSurfaceCount * sizeof(raw_byte128));
        std::memset(gfxWorld.dpvs.surfaceVisDataCameraSaved, 0xFF, alignedSurfaceCount * sizeof(raw_byte128));
        std::memset(gfxWorld.dpvs.surfaceCastsShadow, 0xFF, alignedSurfaceCount * sizeof(raw_byte128));
        std::memset(gfxWorld.dpvs.surfaceCastsSunShadow, 0xFF, alignedSurfaceCount * sizeof(raw_byte128));

        return true;
    }

    void LoadXModels(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        const auto modelCount = 0u;
        gfxWorld.dpvs.smodelCount = modelCount;
        gfxWorld.dpvs.smodelInsts = AllocRuntimeArrayPointer<GfxStaticModelInst>(memory, modelCount);
        gfxWorld.dpvs.smodelDrawInsts = AllocRuntimeArrayPointer<GfxStaticModelDrawInst>(memory, modelCount);

        const auto alignedModelCount = utils::Align(modelCount, 128u);
        gfxWorld.dpvs.smodelVisDataCount = static_cast<unsigned int>(alignedModelCount);
        gfxWorld.dpvs.smodelVisData[0] = AllocRuntimeArrayPointer<raw_byte128>(memory, alignedModelCount);
        gfxWorld.dpvs.smodelVisData[1] = AllocRuntimeArrayPointer<raw_byte128>(memory, alignedModelCount);
        gfxWorld.dpvs.smodelVisData[2] = AllocRuntimeArrayPointer<raw_byte128>(memory, alignedModelCount);
        gfxWorld.dpvs.smodelVisDataCameraSaved = AllocRuntimeArrayPointer<raw_byte128>(memory, alignedModelCount);
        gfxWorld.dpvs.smodelCastsShadow = AllocRuntimeArrayPointer<raw_byte128>(memory, alignedModelCount);
        gfxWorld.dpvs.usageCount = 0;
    }

    void CleanGfxWorld(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        gfxWorld.checksum = 0u;
        gfxWorld.coronaCount = 0u;
        gfxWorld.coronas = nullptr;
        gfxWorld.exposureVolumeCount = 0u;
        gfxWorld.exposureVolumes = nullptr;
        gfxWorld.exposureVolumePlaneCount = 0u;
        gfxWorld.exposureVolumePlanes = nullptr;
        gfxWorld.heroLightCount = 0u;
        gfxWorld.heroLightTreeCount = 0u;
        gfxWorld.heroLights = nullptr;
        gfxWorld.heroLightTree = nullptr;
        gfxWorld.lutVolumeCount = 0u;
        gfxWorld.lutVolumes = nullptr;
        gfxWorld.lutVolumePlaneCount = 0u;
        gfxWorld.lutVolumePlanes = nullptr;
        gfxWorld.numOccluders = 0u;
        gfxWorld.occluders = nullptr;
        gfxWorld.numSiegeSkinInsts = 0u;
        gfxWorld.siegeSkinInsts = nullptr;
        gfxWorld.numOutdoorBounds = 0u;
        gfxWorld.outdoorBounds = nullptr;
        gfxWorld.ropeMaterial = nullptr;
        gfxWorld.lutMaterial = nullptr;
        gfxWorld.waterMaterial = nullptr;
        gfxWorld.coronaMaterial = nullptr;
        gfxWorld.shadowMapVolumeCount = 0u;
        gfxWorld.shadowMapVolumes = nullptr;
        gfxWorld.shadowMapVolumePlaneCount = 0u;
        gfxWorld.shadowMapVolumePlanes = nullptr;
        gfxWorld.streamInfo.aabbTreeCount = 0;
        gfxWorld.streamInfo.aabbTrees = nullptr;
        gfxWorld.streamInfo.leafRefCount = 0;
        gfxWorld.streamInfo.leafRefs = nullptr;
        std::memset(&gfxWorld.sun, 0, sizeof(sunflare_t));
        gfxWorld.sun.hasValidData = false;
        gfxWorld.waterDirection = 0.0f;
        gfxWorld.waterBuffers[0].bufferSize = 0u;
        gfxWorld.waterBuffers[0].buffer = nullptr;
        gfxWorld.waterBuffers[1].bufferSize = 0u;
        gfxWorld.waterBuffers[1].buffer = nullptr;
        gfxWorld.worldFogModifierVolumeCount = 0u;
        gfxWorld.worldFogModifierVolumes = nullptr;
        gfxWorld.worldFogModifierVolumePlaneCount = 0u;
        gfxWorld.worldFogModifierVolumePlanes = nullptr;
        gfxWorld.worldFogVolumeCount = 0u;
        gfxWorld.worldFogVolumes = nullptr;
        gfxWorld.worldFogVolumePlaneCount = 0u;
        gfxWorld.worldFogVolumePlanes = nullptr;
        gfxWorld.materialMemoryCount = 0;
        gfxWorld.materialMemory = nullptr;
        gfxWorld.sunLight = AllocZeroed<GfxLight>(memory);
    }

    void LoadGfxLights(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        gfxWorld.primaryLightCount = DEFAULT_LIGHT_COUNT;
        gfxWorld.sunPrimaryLightIndex = DEFAULT_SUN_LIGHT_INDEX;

        gfxWorld.sunLight->type = GFX_LIGHT_TYPE_DIR;
        gfxWorld.sunLight->canUseShadowMap = 0;
        gfxWorld.sunLight->color.x = 1.0f;
        gfxWorld.sunLight->color.y = 0.89f;
        gfxWorld.sunLight->color.z = 0.69f;
        gfxWorld.sunLight->dir.x = DEFAULT_SUN_DIRECTION_X;
        gfxWorld.sunLight->dir.y = DEFAULT_SUN_DIRECTION_Y;
        gfxWorld.sunLight->dir.z = DEFAULT_SUN_DIRECTION_Z;
        gfxWorld.sunLight->diffuseColor.x = 1.0f;
        gfxWorld.sunLight->diffuseColor.y = 0.89f;
        gfxWorld.sunLight->diffuseColor.z = 0.69f;
        gfxWorld.sunLight->diffuseColor.w = 13.5f;

        gfxWorld.shadowGeom = AllocZeroed<GfxShadowGeometry>(memory, gfxWorld.primaryLightCount);
        gfxWorld.lightRegion = AllocZeroed<GfxLightRegion>(memory, gfxWorld.primaryLightCount);

        for (auto lightIndex = 0u; lightIndex < gfxWorld.primaryLightCount; lightIndex++)
        {
            gfxWorld.shadowGeom[lightIndex].smodelCount = 0u;
            gfxWorld.shadowGeom[lightIndex].surfaceCount = 0u;
            gfxWorld.shadowGeom[lightIndex].smodelIndex = nullptr;
            gfxWorld.shadowGeom[lightIndex].sortedSurfIndex = nullptr;
            gfxWorld.lightRegion[lightIndex].hullCount = 0u;
            gfxWorld.lightRegion[lightIndex].hulls = nullptr;
        }

        const auto lightEntShadowVisSize = (gfxWorld.primaryLightCount - gfxWorld.sunPrimaryLightIndex - 1u) * 8192u;
        gfxWorld.primaryLightEntityShadowVis = lightEntShadowVisSize > 0u ? memory.Alloc<unsigned int>(lightEntShadowVisSize) : nullptr;
    }

    void LoadLightGrid(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        gfxWorld.lightGrid.mins[0] = 0u;
        gfxWorld.lightGrid.mins[1] = 0u;
        gfxWorld.lightGrid.mins[2] = 0u;
        gfxWorld.lightGrid.maxs[0] = DEFAULT_LIGHTGRID_MAX_X;
        gfxWorld.lightGrid.maxs[1] = DEFAULT_LIGHTGRID_MAX_Y;
        gfxWorld.lightGrid.maxs[2] = DEFAULT_LIGHTGRID_MAX_Z;
        gfxWorld.lightGrid.rowAxis = 0u;
        gfxWorld.lightGrid.colAxis = 1u;
        gfxWorld.lightGrid.sunPrimaryLightIndex = DEFAULT_SUN_LIGHT_INDEX;
        gfxWorld.lightGrid.offset = 0.0f;

        const auto rowDataStartSize = gfxWorld.lightGrid.maxs[gfxWorld.lightGrid.rowAxis] - gfxWorld.lightGrid.mins[gfxWorld.lightGrid.rowAxis] + 1u;
        gfxWorld.lightGrid.rowDataStart = memory.Alloc<std::uint16_t>(rowDataStartSize);
        for (auto rowIndex = 0u; rowIndex < rowDataStartSize; rowIndex++)
            gfxWorld.lightGrid.rowDataStart[rowIndex] = 0u;

        gfxWorld.lightGrid.rawRowDataSize = sizeof(GfxLightGridRow) + 0x0Fu;
        auto* row = static_cast<GfxLightGridRow*>(memory.AllocRaw(gfxWorld.lightGrid.rawRowDataSize));
        std::memset(row, 0, gfxWorld.lightGrid.rawRowDataSize);
        row->colStart = 0u;
        row->colCount = LEGACY_LIGHTGRID_COL_COUNT;
        row->zStart = 0u;
        row->zCount = LEGACY_LIGHTGRID_Z_COUNT;
        row->firstEntry = 0u;
        std::memset(row->lookupTable, 0, 0x10u);
        gfxWorld.lightGrid.rawRowData = reinterpret_cast<aligned_byte_pointer*>(row);

        gfxWorld.lightGrid.entryCount = LEGACY_LIGHTGRID_ENTRY_COUNT;
        gfxWorld.lightGrid.entries = memory.Alloc<GfxLightGridEntry>(gfxWorld.lightGrid.entryCount);
        for (auto entryIndex = 0u; entryIndex < gfxWorld.lightGrid.entryCount; entryIndex++)
        {
            gfxWorld.lightGrid.entries[entryIndex].colorsIndex = 0u;
            gfxWorld.lightGrid.entries[entryIndex].primaryLightIndex = DEFAULT_SUN_LIGHT_INDEX;
            gfxWorld.lightGrid.entries[entryIndex].visibility = 0u;
        }

        gfxWorld.lightGrid.colorCount = LEGACY_LIGHTGRID_COLOR_COUNT;
        gfxWorld.lightGrid.colors = memory.Alloc<GfxCompressedLightGridColors>(gfxWorld.lightGrid.colorCount);
        std::memset(gfxWorld.lightGrid.colors, LEGACY_LIGHTGRID_COLOR, gfxWorld.lightGrid.colorCount * sizeof(GfxCompressedLightGridColors));

        gfxWorld.lightGrid.coeffCount = 0u;
        gfxWorld.lightGrid.coeffs = nullptr;
        gfxWorld.lightGrid.skyGridVolumeCount = 0u;
        gfxWorld.lightGrid.skyGridVolumes = nullptr;
    }

    void LoadWorldBounds(GfxWorld& gfxWorld)
    {
        gfxWorld.mins = gfxWorld.dpvs.surfaces[0].bounds[0];
        gfxWorld.maxs = gfxWorld.dpvs.surfaces[0].bounds[1];

        for (auto surfaceIndex = 1; surfaceIndex < gfxWorld.surfaceCount; surfaceIndex++)
            UpdateAabb(gfxWorld.dpvs.surfaces[surfaceIndex].bounds[0], gfxWorld.dpvs.surfaces[surfaceIndex].bounds[1], gfxWorld.mins, gfxWorld.maxs);
    }

    void LoadGfxCells(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        const auto cellCount = 1;
        gfxWorld.dpvsPlanes.cellCount = cellCount;
        gfxWorld.cellBitsCount = ((cellCount + 127) >> 3) & 0x1FFFFFF0;
        gfxWorld.cellCasterBits = memory.Alloc<unsigned int>(cellCount * ((cellCount + 31) / 32));
        gfxWorld.dpvsPlanes.sceneEntCellBits = memory.Alloc<unsigned int>(cellCount * 512);
        std::memset(gfxWorld.cellCasterBits, 0xFF, cellCount * ((cellCount + 31) / 32) * sizeof(unsigned int));
        std::memset(gfxWorld.dpvsPlanes.sceneEntCellBits, 0xFF, cellCount * 512 * sizeof(unsigned int));

        gfxWorld.cells = AllocZeroed<GfxCell>(memory, cellCount);

        auto& cell = gfxWorld.cells[0];
        cell.portalCount = 0;
        cell.portals = nullptr;
        cell.mins = gfxWorld.mins;
        cell.maxs = gfxWorld.maxs;
        cell.reflectionProbeCount = 1;
        cell.reflectionProbes = memory.Alloc<char>(1u);
        cell.reflectionProbes[0] = DEFAULT_SURFACE_REFLECTION_PROBE;
        cell.aabbTreeCount = 1;
        cell.aabbTree = AllocZeroed<GfxAabbTree>(memory, 1u);
        cell.aabbTree[0].childCount = 0;
        cell.aabbTree[0].childrenOffset = 0;
        cell.aabbTree[0].startSurfIndex = 0u;
        cell.aabbTree[0].surfaceCount = static_cast<std::uint16_t>(gfxWorld.surfaceCount);
        cell.aabbTree[0].smodelIndexCount = static_cast<std::uint16_t>(gfxWorld.dpvs.smodelCount);
        cell.aabbTree[0].smodelIndexes = AllocRuntimeArrayPointer<unsigned short>(memory, gfxWorld.dpvs.smodelCount);
        for (auto smodelIndex = 0u; smodelIndex < gfxWorld.dpvs.smodelCount; smodelIndex++)
            cell.aabbTree[0].smodelIndexes[smodelIndex] = static_cast<unsigned short>(smodelIndex);
        cell.aabbTree[0].mins = gfxWorld.mins;
        cell.aabbTree[0].maxs = gfxWorld.maxs;

        gfxWorld.planeCount = 0;
        gfxWorld.dpvsPlanes.planes = nullptr;

        gfxWorld.nodeCount = 1;
        gfxWorld.dpvsPlanes.nodes = AllocZeroed<std::uint16_t>(memory, gfxWorld.nodeCount);
        gfxWorld.dpvsPlanes.nodes[0] = 1u;
    }

    void LoadModels(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        gfxWorld.modelCount = 1;
        gfxWorld.models = AllocZeroed<GfxBrushModel>(memory, 1u);
        gfxWorld.models[0].startSurfIndex = 0u;
        gfxWorld.models[0].surfaceCount = static_cast<unsigned int>(gfxWorld.surfaceCount);
        gfxWorld.models[0].bounds[0] = gfxWorld.mins;
        gfxWorld.models[0].bounds[1] = gfxWorld.maxs;
        std::memset(&gfxWorld.models[0].writable, 0, sizeof(GfxBrushModelWritable));
    }

    void LoadSunData(GfxWorld& gfxWorld)
    {
        gfxWorld.sunParse.fogTransitionTime = 0.001f;
        gfxWorld.sunParse.name[0] = '\0';
        gfxWorld.sunParse.initWorldSun->control = 0u;
        gfxWorld.sunParse.initWorldSun->exposure = 2.5f;
        gfxWorld.sunParse.initWorldSun->angles.x = -29.0f;
        gfxWorld.sunParse.initWorldSun->angles.y = 254.0f;
        gfxWorld.sunParse.initWorldSun->angles.z = 0.0f;
        gfxWorld.sunParse.initWorldSun->sunCd.x = 1.0f;
        gfxWorld.sunParse.initWorldSun->sunCd.y = 0.89f;
        gfxWorld.sunParse.initWorldSun->sunCd.z = 0.69f;
        gfxWorld.sunParse.initWorldSun->sunCd.w = 13.5f;
        gfxWorld.sunParse.initWorldFog->baseDist = 150.0f;
        gfxWorld.sunParse.initWorldFog->baseHeight = -100.0f;
        gfxWorld.sunParse.initWorldFog->halfDist = 4450.0f;
        gfxWorld.sunParse.initWorldFog->halfHeight = 2000.0f;
    }

    [[nodiscard]] std::string GetMapScopedImageName(const GfxWorld& gfxWorld, const char* suffix)
    {
        const auto* baseName = gfxWorld.baseName && gfxWorld.baseName[0] ? gfxWorld.baseName : "custom_map";
        return std::format("{}_{}", baseName, suffix);
    }

    bool LoadReflectionProbeData(MemoryManager& memory, AssetCreationContext& context, GfxWorld& gfxWorld)
    {
        gfxWorld.draw.reflectionProbeCount = 1u;
        gfxWorld.draw.reflectionProbeTextures = AllocZeroed<GfxTexture>(memory, 1u);
        gfxWorld.draw.reflectionProbes = AllocZeroed<GfxReflectionProbe>(memory, 1u);
        gfxWorld.draw.reflectionProbes[0].mipLodBias = -8.0f;

        const auto imageName = GetMapScopedImageName(gfxWorld, REFLECTION_PROBE_IMAGE_SUFFIX);
        auto* image = CreateGeneratedMapImage(memory, context, imageName.c_str(), GeneratedMapImageKind::ReflectionProbe);
        if (!image)
            return false;

        gfxWorld.draw.reflectionProbes[0].reflectionImage = image->Asset();
        return true;
    }

    bool LoadLightmapData(MemoryManager& memory, AssetCreationContext& context, GfxWorld& gfxWorld)
    {
        gfxWorld.draw.lightmapCount = 1;
        gfxWorld.draw.lightmapPrimaryTextures = AllocZeroed<GfxTexture>(memory, 1u);
        gfxWorld.draw.lightmapSecondaryTextures = AllocZeroed<GfxTexture>(memory, 1u);

        const auto primaryImageName = GetMapScopedImageName(gfxWorld, LIGHTMAP_PRIMARY_IMAGE_SUFFIX);
        auto* primaryImage = CreateGeneratedMapImage(memory, context, primaryImageName.c_str(), GeneratedMapImageKind::Lightmap);
        if (!primaryImage)
            return false;

        const auto secondaryImageName = GetMapScopedImageName(gfxWorld, LIGHTMAP_SECONDARY_IMAGE_SUFFIX);
        auto* secondaryImage = CreateGeneratedMapImage(memory, context, secondaryImageName.c_str(), GeneratedMapImageKind::Lightmap);
        if (!secondaryImage)
            return false;

        gfxWorld.draw.lightmaps = AllocZeroed<GfxLightmapArray>(memory, 1u);
        gfxWorld.draw.lightmaps[0].primary = primaryImage->Asset();
        gfxWorld.draw.lightmaps[0].secondary = secondaryImage->Asset();
        return true;
    }

    void LoadSkyBox(MemoryManager& memory, ISearchPath& searchPath, AssetCreationContext& context, const map::T6MapGeometry& geometry, GfxWorld& gfxWorld)
    {
        gfxWorld.skyBoxModel = memory.Dup("");
        gfxWorld.skyDynIntensity.angle0 = 0.0f;
        gfxWorld.skyDynIntensity.angle1 = 0.0f;
        gfxWorld.skyDynIntensity.factor0 = 1.0f;
        gfxWorld.skyDynIntensity.factor1 = 1.0f;

        const auto skyBoxName = std::format("skybox_{}", geometry.m_map_name);
        const auto skyBoxFile = std::format("xmodel/{}.json", skyBoxName);
        if (!searchPath.Open(skyBoxFile).IsOpen())
            return;

        const auto* skyBox = context.LoadDependency<AssetXModel>(skyBoxName);
        if (skyBox)
            gfxWorld.skyBoxModel = memory.Dup(skyBoxName.c_str());
        else
            con::warn("Unable to load custom-map skybox xmodel {}, continuing without skybox model.", skyBoxName);
    }

    void LoadDynEntData(MemoryManager& memory, GfxWorld& gfxWorld)
    {
        gfxWorld.dpvsDyn.dynEntClientCount[0] = DEFAULT_DYN_ENTITY_CLIENT_RESERVE_COUNT;
        gfxWorld.dpvsDyn.dynEntClientCount[1] = 0u;
        gfxWorld.dpvsDyn.dynEntClientWordCount[0] = (gfxWorld.dpvsDyn.dynEntClientCount[0] + 31u) >> 5;
        gfxWorld.dpvsDyn.dynEntClientWordCount[1] = 0u;
        gfxWorld.dpvsDyn.usageCount = 0;

        const auto dynEntCellBitsSize = gfxWorld.dpvsDyn.dynEntClientWordCount[0] * static_cast<unsigned>(gfxWorld.dpvsPlanes.cellCount);
        gfxWorld.dpvsDyn.dynEntCellBits[0] = dynEntCellBitsSize ? AllocZeroed<unsigned int>(memory, dynEntCellBitsSize) : nullptr;
        gfxWorld.dpvsDyn.dynEntCellBits[1] = nullptr;

        const auto dynEntVisDataSize = gfxWorld.dpvsDyn.dynEntClientWordCount[0] * 32u;
        gfxWorld.dpvsDyn.dynEntVisData[0][0] = dynEntVisDataSize ? AllocZeroed<raw_byte16>(memory, dynEntVisDataSize) : nullptr;
        gfxWorld.dpvsDyn.dynEntVisData[0][1] = dynEntVisDataSize ? AllocZeroed<raw_byte16>(memory, dynEntVisDataSize) : nullptr;
        gfxWorld.dpvsDyn.dynEntVisData[0][2] = dynEntVisDataSize ? AllocZeroed<raw_byte16>(memory, dynEntVisDataSize) : nullptr;
        gfxWorld.dpvsDyn.dynEntVisData[1][0] = nullptr;
        gfxWorld.dpvsDyn.dynEntVisData[1][1] = nullptr;
        gfxWorld.dpvsDyn.dynEntVisData[1][2] = nullptr;

        const auto dynEntShadowVisCount = gfxWorld.dpvsDyn.dynEntClientCount[0] * (gfxWorld.primaryLightCount - gfxWorld.sunPrimaryLightIndex - 1u);
        gfxWorld.primaryLightDynEntShadowVis[0] = dynEntShadowVisCount ? AllocZeroed<unsigned int>(memory, dynEntShadowVisCount) : nullptr;
        gfxWorld.primaryLightDynEntShadowVis[1] = nullptr;
        gfxWorld.sceneDynModel =
            gfxWorld.dpvsDyn.dynEntClientCount[0] ? AllocZeroed<GfxSceneDynModel>(memory, gfxWorld.dpvsDyn.dynEntClientCount[0]) : nullptr;
        gfxWorld.sceneDynBrush = nullptr;
    }

    bool LoadOutdoors(MemoryManager& memory, AssetCreationContext& context, GfxWorld& gfxWorld)
    {
        const auto xRecip = SafeReciprocal(gfxWorld.maxs.x - gfxWorld.mins.x);
        const auto yRecip = SafeReciprocal(gfxWorld.maxs.y - gfxWorld.mins.y);
        const auto zRecip = SafeReciprocal(gfxWorld.maxs.z - gfxWorld.mins.z);

        std::memset(gfxWorld.outdoorLookupMatrix, 0, sizeof(gfxWorld.outdoorLookupMatrix));
        gfxWorld.outdoorLookupMatrix[0].x = xRecip;
        gfxWorld.outdoorLookupMatrix[1].y = yRecip;
        gfxWorld.outdoorLookupMatrix[2].z = zRecip;
        gfxWorld.outdoorLookupMatrix[3].x = -(xRecip * gfxWorld.mins.x);
        gfxWorld.outdoorLookupMatrix[3].y = -(yRecip * gfxWorld.mins.y);
        gfxWorld.outdoorLookupMatrix[3].z = -(zRecip * gfxWorld.mins.z);
        gfxWorld.outdoorLookupMatrix[3].w = 1.0f;

        if (context.HasAsset<AssetImage>(OUTDOOR_IMAGE))
        {
            auto* outdoorRef = static_cast<XAssetInfo<GfxImage>*>(context.LoadDependencyGeneric(AssetImage::EnumEntry, OUTDOOR_IMAGE, false));
            if (outdoorRef && !outdoorRef->IsReference())
            {
                NormalizeMapImageForRuntime(memory, *outdoorRef->Asset(), OUTDOOR_IMAGE, GeneratedMapImageKind::Outdoor);
                gfxWorld.outdoorImage = outdoorRef->Asset();
                return true;
            }
        }

        auto* image = CreateGeneratedMapImage(memory, context, OUTDOOR_IMAGE, GeneratedMapImageKind::Outdoor);
        if (!image)
            return false;

        gfxWorld.outdoorImage = image->Asset();
        return true;
    }

} // namespace

namespace map
{
    GfxWorld* CreateGfxWorldT6(MemoryManager& memory, ISearchPath& searchPath, AssetCreationContext& context, const T6MapGeometry& geometry)
    {
        auto* gfxWorld = AllocZeroed<GfxWorld>(memory);
        gfxWorld->baseName = memory.Dup(geometry.m_map_name.c_str());
        gfxWorld->name = memory.Dup(geometry.m_world_asset_name.c_str());
        gfxWorld->lightingFlags = 0;
        gfxWorld->lightingQuality = 4096;

        CleanGfxWorld(memory, *gfxWorld);

        if (!LoadMapSurfaces(memory, context, geometry.m_gfx_world, *gfxWorld))
            return nullptr;
        LoadMaterialMemory(memory, *gfxWorld);

        LoadXModels(memory, *gfxWorld);

        if (!LoadLightmapData(memory, context, *gfxWorld))
            return nullptr;

        LoadSkyBox(memory, searchPath, context, geometry, *gfxWorld);

        if (!LoadReflectionProbeData(memory, context, *gfxWorld))
            return nullptr;

        LoadWorldBounds(*gfxWorld);

        if (!LoadOutdoors(memory, context, *gfxWorld))
            return nullptr;

        LoadGfxCells(memory, *gfxWorld);
        LoadLightGrid(memory, *gfxWorld);
        LoadGfxLights(memory, *gfxWorld);
        LoadModels(memory, *gfxWorld);
        LoadSunData(*gfxWorld);
        LoadDynEntData(memory, *gfxWorld);

        con::debug(
            "T6 custom map gfxworld \"{}\": surfaces={} vertices={} indices={} vertexData0={} vertexData1={} bounds=({}, {}, {}) -> ({}, {}, {}) cells={} "
            "models={} lightGridRows={} lightGridRaw={} lightGridEntries={} lightGridColors={} dynEntClientCount=({}, {}) dynEntClientWordCount=({}, {})",
            geometry.m_world_asset_name,
            gfxWorld->surfaceCount,
            gfxWorld->draw.vertexCount,
            gfxWorld->draw.indexCount,
            gfxWorld->draw.vertexDataSize0,
            gfxWorld->draw.vertexDataSize1,
            gfxWorld->mins.x,
            gfxWorld->mins.y,
            gfxWorld->mins.z,
            gfxWorld->maxs.x,
            gfxWorld->maxs.y,
            gfxWorld->maxs.z,
            gfxWorld->dpvsPlanes.cellCount,
            gfxWorld->modelCount,
            gfxWorld->lightGrid.maxs[gfxWorld->lightGrid.rowAxis] - gfxWorld->lightGrid.mins[gfxWorld->lightGrid.rowAxis] + 1u,
            gfxWorld->lightGrid.rawRowDataSize,
            gfxWorld->lightGrid.entryCount,
            gfxWorld->lightGrid.colorCount,
            gfxWorld->dpvsDyn.dynEntClientCount[0],
            gfxWorld->dpvsDyn.dynEntClientCount[1],
            gfxWorld->dpvsDyn.dynEntClientWordCount[0],
            gfxWorld->dpvsDyn.dynEntClientWordCount[1]);

        return gfxWorld;
    }
} // namespace map

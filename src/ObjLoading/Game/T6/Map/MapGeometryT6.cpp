#include "MapGeometryT6.h"

#include "MapWorldT6.h"
#include "Utils/Logging/Log.h"

#include <cassert>
#include <format>
#include <limits>
#include <memory>
#include <string_view>
#include <ufbx.h>

namespace
{
    using UfbxScenePtr = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;

    [[nodiscard]] T6::vec3_t ConvertToT6Coords(const T6::vec3_t& coordinate)
    {
        T6::vec3_t result;
        result.x = coordinate.x;
        result.y = -coordinate.z;
        result.z = coordinate.y;
        return result;
    }

    [[nodiscard]] UfbxScenePtr LoadFbxScene(ISearchPath& searchPath, const std::string& fileName, const bool required)
    {
        const auto file = searchPath.Open(fileName);
        if (!file.IsOpen())
        {
            if (required)
                con::error("Failed to open T6 custom map FBX source file: {}", fileName);

            return {nullptr, &ufbx_free_scene};
        }

        if (file.m_length <= 0)
        {
            con::error("T6 custom map FBX source file is empty: {}", fileName);
            return {nullptr, &ufbx_free_scene};
        }

        std::vector<char> fileData(static_cast<std::size_t>(file.m_length));
        file.m_stream->read(fileData.data(), file.m_length);
        if (file.m_stream->gcount() != file.m_length)
        {
            con::error("Read error for T6 custom map FBX source file: {}", fileName);
            return {nullptr, &ufbx_free_scene};
        }

        ufbx_load_opts opts{};
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.generate_missing_normals = true;
        opts.allow_missing_vertex_position = false;
        opts.retain_dom = true;

        ufbx_error error{};
        auto* scene = ufbx_load_memory(fileData.data(), static_cast<std::size_t>(file.m_length), &opts, &error);
        if (!scene)
        {
            con::error("Failed to load T6 custom map FBX source file \"{}\": {}", fileName, error.description.data);
            return {nullptr, &ufbx_free_scene};
        }

        return {scene, &ufbx_free_scene};
    }

    [[nodiscard]] bool UfbxStringEquals(const ufbx_string& value, const std::string_view expected)
    {
        if (!value.data)
            return expected.empty();

        return std::string_view(value.data, value.length) == expected;
    }

    [[nodiscard]] bool NodeHasCullingOff(const ufbx_node* node)
    {
        if (!node)
            return false;

        const auto cullingProperty = ufbx_find_string(&node->props, "Culling", ufbx_empty_string);
        if (UfbxStringEquals(cullingProperty, "CullingOff"))
            return true;

        const auto* domNode = node->element.dom_node;
        if (!domNode)
            return false;

        const auto* cullingNode = ufbx_dom_find(domNode, "Culling");
        if (!cullingNode || cullingNode->values.count == 0u)
            return false;

        const auto& value = cullingNode->values.data[0];
        return value.type == UFBX_DOM_VALUE_STRING && UfbxStringEquals(value.value_str, "CullingOff");
    }

    [[nodiscard]] T6::vec3_t Negate(const T6::vec3_t& value)
    {
        T6::vec3_t result;
        result.x = -value.x;
        result.y = -value.y;
        result.z = -value.z;
        return result;
    }

    bool AddMeshToWorld(ufbx_node* node, map::T6MapWorldGeometry& world, bool& hasTangentSpace, const bool includeCullingOffBackfaces)
    {
        auto* mesh = node->mesh;

        assert(node->attrib_type == UFBX_ELEMENT_MESH);

        if (mesh->instances.count != 1)
            con::warn("Mesh {} has {} instances; only the first instance will be used.", node->name.data, mesh->instances.count);

        if (mesh->num_triangles == 0)
        {
            con::warn("Ignoring mesh {} because its triangle count is 0.", node->name.data);
            return true;
        }

        if (mesh->num_indices % 3 != 0)
        {
            con::warn("Ignoring mesh {} because it is not triangulated.", node->name.data);
            return true;
        }

        for (std::size_t index = 0; index < mesh->num_indices; index++)
        {
            if (mesh->vertex_indices[index] > std::numeric_limits<std::uint16_t>::max())
            {
                con::error("Ignoring mesh {} because it exceeds the T6 uint16 index limit.", node->name.data);
                return false;
            }
        }

        if (!mesh->vertex_tangent.exists)
            hasTangentSpace = false;

        auto transform = node->local_transform;
        transform.translation.x /= 100.0f;
        transform.translation.y /= 100.0f;
        transform.translation.z /= 100.0f;
        transform.scale.x /= 100.0f;
        transform.scale.y /= 100.0f;
        transform.scale.z /= 100.0f;
        const auto meshMatrix = ufbx_transform_to_matrix(&transform);
        const auto isDoubleSided = includeCullingOffBackfaces && NodeHasCullingOff(node);

        for (const auto& meshPart : mesh->material_parts)
        {
            if (meshPart.num_faces == 0)
                continue;

            map::T6MapSurface surface;
            surface.m_first_vertex = static_cast<unsigned>(world.m_vertices.size());
            surface.m_first_index = static_cast<unsigned>(world.m_indices.size());

            if (mesh->materials.count == 0)
            {
                surface.m_material.m_type = map::T6MapMaterialType::Empty;
                surface.m_material.m_name.clear();
            }
            else
            {
                surface.m_material.m_type = map::T6MapMaterialType::Texture;
                surface.m_material.m_name = mesh->materials.data[meshPart.index]->name.data;
            }

            std::vector<map::T6MapVertex> tempVertices;
            std::vector<std::uint32_t> tempIndices(mesh->max_face_triangles * 3u);
            auto emittedTriCount = 0u;

            for (const auto faceIndex : meshPart.face_indices)
            {
                const auto* face = &mesh->faces.data[faceIndex];
                const auto triangulatedTriCount = ufbx_triangulate_face(tempIndices.data(), tempIndices.size(), mesh, *face);

                for (std::uint32_t triangleIndex = 0; triangleIndex < triangulatedTriCount; triangleIndex++)
                {
                    map::T6MapVertex triangleVertices[3];

                    for (std::uint32_t vertexOffset = 0; vertexOffset < 3u; vertexOffset++)
                    {
                        const auto index = tempIndices[(triangleIndex * 3u) + vertexOffset];
                        auto& vertex = triangleVertices[vertexOffset];

                        const auto transformedPosition = ufbx_transform_position(&meshMatrix, ufbx_get_vertex_vec3(&mesh->vertex_position, index));
                        T6::vec3_t sourcePosition;
                        sourcePosition.x = static_cast<float>(transformedPosition.x);
                        sourcePosition.y = static_cast<float>(transformedPosition.y);
                        sourcePosition.z = static_cast<float>(transformedPosition.z);
                        vertex.m_position = ConvertToT6Coords(sourcePosition);

                        vertex.m_color.x = 1.0f;
                        vertex.m_color.y = 1.0f;
                        vertex.m_color.z = 1.0f;
                        vertex.m_color.w = 1.0f;

                        const auto uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                        vertex.m_tex_coord.x = static_cast<float>(uv.x);
                        vertex.m_tex_coord.y = static_cast<float>(1.0f - uv.y);

                        const auto normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
                        T6::vec3_t sourceNormal;
                        sourceNormal.x = static_cast<float>(normal.x);
                        sourceNormal.y = static_cast<float>(normal.y);
                        sourceNormal.z = static_cast<float>(normal.z);
                        vertex.m_normal = ConvertToT6Coords(sourceNormal);

                        if (mesh->vertex_tangent.exists)
                        {
                            const auto tangent = ufbx_get_vertex_vec3(&mesh->vertex_tangent, index);
                            T6::vec3_t sourceTangent;
                            sourceTangent.x = static_cast<float>(tangent.x);
                            sourceTangent.y = static_cast<float>(tangent.y);
                            sourceTangent.z = static_cast<float>(tangent.z);
                            vertex.m_tangent = ConvertToT6Coords(sourceTangent);
                        }
                        else
                        {
                            vertex.m_tangent.x = 0.0f;
                            vertex.m_tangent.y = 0.0f;
                            vertex.m_tangent.z = 0.0f;
                        }
                    }

                    tempVertices.emplace_back(triangleVertices[0]);
                    tempVertices.emplace_back(triangleVertices[1]);
                    tempVertices.emplace_back(triangleVertices[2]);
                    emittedTriCount++;

                    if (isDoubleSided)
                    {
                        triangleVertices[0].m_normal = Negate(triangleVertices[0].m_normal);
                        triangleVertices[1].m_normal = Negate(triangleVertices[1].m_normal);
                        triangleVertices[2].m_normal = Negate(triangleVertices[2].m_normal);
                        triangleVertices[0].m_tangent = Negate(triangleVertices[0].m_tangent);
                        triangleVertices[1].m_tangent = Negate(triangleVertices[1].m_tangent);
                        triangleVertices[2].m_tangent = Negate(triangleVertices[2].m_tangent);

                        tempVertices.emplace_back(triangleVertices[2]);
                        tempVertices.emplace_back(triangleVertices[1]);
                        tempVertices.emplace_back(triangleVertices[0]);
                        emittedTriCount++;
                    }
                }
            }

            ufbx_vertex_stream streams[1] = {
                {tempVertices.data(), tempVertices.size(), sizeof(map::T6MapVertex)},
            };
            surface.m_tri_count = emittedTriCount;
            std::vector<std::uint32_t> generatedIndices(static_cast<std::size_t>(surface.m_tri_count) * 3u);
            const auto generatedVertexCount =
                ufbx_generate_indices(streams, 1, generatedIndices.data(), generatedIndices.size(), nullptr, nullptr);

            if (generatedVertexCount == 0 || generatedVertexCount > std::numeric_limits<std::uint16_t>::max())
            {
                con::error("Mesh {} could not be converted into a T6-compatible uint16 vertex/index stream.", node->name.data);
                return false;
            }

            tempVertices.resize(generatedVertexCount);
            world.m_vertices.insert(world.m_vertices.end(), tempVertices.begin(), tempVertices.end());

            surface.m_tri_count = static_cast<unsigned>(generatedIndices.size() / 3u);

            for (std::size_t indexPosition = 0; indexPosition < generatedIndices.size(); indexPosition += 3)
            {
                world.m_indices.emplace_back(static_cast<std::uint16_t>(generatedIndices[indexPosition]));
                world.m_indices.emplace_back(static_cast<std::uint16_t>(generatedIndices[indexPosition + 1]));
                world.m_indices.emplace_back(static_cast<std::uint16_t>(generatedIndices[indexPosition + 2]));
            }

            world.m_surfaces.emplace_back(std::move(surface));
        }

        return true;
    }

    bool LoadWorldGeometry(const ufbx_scene& scene, map::T6MapWorldGeometry& world, const bool includeCullingOffBackfaces)
    {
        bool hasTangentSpace = true;

        for (auto* node : scene.nodes)
        {
            if (node->attrib_type == UFBX_ELEMENT_MESH)
            {
                if (!AddMeshToWorld(node, world, hasTangentSpace, includeCullingOffBackfaces))
                    return false;
            }
            else
            {
                con::debug("Ignoring FBX node type {}: {}", static_cast<int>(node->attrib_type), node->name.data);
            }
        }

        if (!hasTangentSpace)
            con::warn("One or more T6 custom map meshes have no tangent space. Export FBX files with tangent space enabled.");

        return true;
    }

    [[nodiscard]] bool HasRenderableGeometry(const map::T6MapWorldGeometry& world)
    {
        return !world.m_surfaces.empty() && !world.m_vertices.empty() && !world.m_indices.empty();
    }
} // namespace

namespace map
{
    std::string GetT6MapSourceFileName(const std::string& assetName)
    {
        return std::format("bsp/{}", assetName);
    }

    std::unique_ptr<T6MapGeometry> LoadMapGeometryT6(ISearchPath& searchPath, const std::string& mapName, const ZoneDefinitionMapType mapType)
    {
        const auto gfxScene = LoadFbxScene(searchPath, GetT6MapSourceFileName("map_gfx.fbx"), true);
        if (!gfxScene)
            return nullptr;

        auto geometry = std::make_unique<T6MapGeometry>();
        geometry->m_map_name = mapName;
        geometry->m_world_asset_name = GetT6MapWorldAssetName(mapName);
        geometry->m_map_type = mapType;

        if (!LoadWorldGeometry(*gfxScene, geometry->m_gfx_world, true))
            return nullptr;

        const auto colScene = LoadFbxScene(searchPath, GetT6MapSourceFileName("map_col.fbx"), false);
        if (colScene)
        {
            if (!LoadWorldGeometry(*colScene, geometry->m_collision_world, true))
                return nullptr;
        }
        else
        {
            con::warn("No T6 custom map collision FBX found; using map_gfx.fbx for collision geometry.");
            if (!LoadWorldGeometry(*gfxScene, geometry->m_collision_world, true))
                return nullptr;
        }

        if (!HasRenderableGeometry(geometry->m_gfx_world))
        {
            con::error("T6 custom map graphics FBX did not produce any renderable geometry.");
            return nullptr;
        }

        if (!HasRenderableGeometry(geometry->m_collision_world))
        {
            con::error("T6 custom map collision FBX did not produce any collision geometry.");
            return nullptr;
        }

        return geometry;
    }
} // namespace map

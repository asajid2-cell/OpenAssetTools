#include "GameWorldMpLinker.h"

using namespace T6;

namespace
{
    template <typename TGameWorld>
    TGameWorld* LinkGameWorld(MemoryManager& memory, const BSP::BSPData& bsp)
    {
        auto* gameWorld = memory.Alloc<TGameWorld>();

        gameWorld->name = memory.Dup(bsp.bspName.c_str());

        gameWorld->path.nodeCount = 0;
        gameWorld->path.originalNodeCount = 0;
        gameWorld->path.visBytes = 0;
        gameWorld->path.smoothBytes = 0;
        gameWorld->path.nodeTreeCount = 0;

        // The game has 128 empty nodes allocated
        const auto extraNodeCount = gameWorld->path.nodeCount + 128u;
        gameWorld->path.nodes = memory.Alloc<pathnode_t>(extraNodeCount);
        gameWorld->path.basenodes = memory.Alloc<pathbasenode_t>(extraNodeCount);
        gameWorld->path.pathVis = nullptr;
        gameWorld->path.smoothCache = nullptr;
        gameWorld->path.nodeTree = nullptr;

        return gameWorld;
    }
} // namespace

namespace BSP
{
    GameWorldMpLinker::GameWorldMpLinker(MemoryManager& memory, ISearchPath& searchPath, AssetCreationContext& context)
        : m_memory(memory),
          m_search_path(searchPath),
          m_context(context)
    {
    }

    GameWorldMp* GameWorldMpLinker::LinkGameWorldMp(const BSPData& bsp) const
    {
        return LinkGameWorld<GameWorldMp>(m_memory, bsp);
    }
} // namespace BSP

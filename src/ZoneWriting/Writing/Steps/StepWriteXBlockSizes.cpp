#include "StepWriteXBlockSizes.h"

#include "Utils/Logging/Log.h"

StepWriteXBlockSizes::StepWriteXBlockSizes(const Zone& zone)
    : m_zone(zone)
{
}

void StepWriteXBlockSizes::PerformStep(ZoneWriter* zoneWriter, IWritingStream* stream)
{
    for (const auto& block : zoneWriter->m_blocks)
    {
        auto blockSize = static_cast<xblock_size_t>(block->m_buffer_size);
        con::debug("Zone \"{}\" XBlock {} size {}", m_zone.m_name, block->m_name, blockSize);
        stream->Write(&blockSize, sizeof(blockSize));
    }
}

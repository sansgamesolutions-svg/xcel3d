#pragma once
#include <cstdint>
#include <string>

namespace xcel::io {

enum class ChunkType : uint8_t
{
    Mesh             = 0x01,
    Skeleton         = 0x02,
    SceneGraph       = 0x03,
    AnimCatalogue    = 0x04,
    AnimFrame        = 0x05,
};

struct ChunkDescriptor
{
    ChunkType type   = ChunkType::Mesh;
    uint32_t  id     = 0;
    uint32_t  flags  = 0;
    uint64_t  offset = 0;
    uint64_t  size   = 0;
    std::string name;
};

} // namespace xcel::io

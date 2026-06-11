#pragma once
#include "IO/Animation/AnimationTrackInfo.h"
#include "Kernel/PrimitiveSet.h"
#include <span>
#include <string_view>

namespace xcel::io {

// Receiver interface passed to IFormatReader::Read.
// The reader calls ReceiveMesh (and optional hooks) to deliver loaded data.
// All spans are transient — valid only for the duration of each call.
//
// The application implements this interface; the plugin DLL only calls it.
// This ensures all persistent polymorphic Kernel objects (CoordTable,
// ScalarTable, PrimitiveSet, ColorTable) are allocated by the exe, so their
// vtables are never inside a plugin DLL that could be unloaded.
class ISceneReceiver
{
public:
    virtual ~ISceneReceiver() = default;

    // Deliver one mesh.
    //   positions          : xyz interleaved, size = 3 * vertexCount
    //   primType           : element topology
    //   indices            : flat connectivity, size = elemCount * indicesPerElement
    //   indicesPerElement  : e.g. 3 for triangles, 8 for hexahedra
    //   scalars            : per-element values; empty => treat as constant 0
    virtual void ReceiveMesh(
        std::string_view           name,
        std::span<const float>     positions,
        xcel::PrimitiveType        primType,
        std::span<const uint32_t>  indices,
        uint32_t                   indicesPerElement,
        std::span<const float>     scalars
    ) = 0;

    // Optional — no-op default so mesh-only receivers need not override.
    virtual void ReceiveAnimationTrack(const AnimationTrackInfo&) {}
};

} // namespace xcel::io

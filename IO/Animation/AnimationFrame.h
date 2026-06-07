#pragma once
#include "Graphics/ScalarTable.h"
#include "Graphics/CoordTable.h"
#include <cstdint>
#include <memory>

namespace xcel::io {

struct AnimationFrame
{
    uint32_t                           frameIndex   = 0;
    float                              timeSeconds  = 0.f;
    std::shared_ptr<xcel::ScalarTable> scalars;       // per-element field (stress, temp, …)
    std::shared_ptr<xcel::CoordTable>  displacements; // nodal displacements (modal/transient)
};

} // namespace xcel::io

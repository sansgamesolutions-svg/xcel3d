#pragma once
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include "Graphics/PrimitiveSet.h"
#include <memory>
#include <string>
#include <vector>

namespace xcel::io {

struct MeshData
{
    std::string                                name;
    std::shared_ptr<xcel::CoordTable>          coords;
    std::shared_ptr<xcel::ScalarTable>         scalars;
    std::shared_ptr<xcel::ColorTable>          colorTable;
    std::vector<std::shared_ptr<xcel::PrimitiveSet>> primSets;
};

} // namespace xcel::io

#pragma once
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/PrimitiveSet.h"
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

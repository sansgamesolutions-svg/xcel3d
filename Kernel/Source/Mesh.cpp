#include "Kernel/Mesh.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/PrimitiveSet.h"

namespace xcel {

struct Mesh::Impl {
    std::shared_ptr<CoordTable>  coords;
    std::shared_ptr<ScalarTable> scalars;
    std::vector<std::shared_ptr<PrimitiveSet>> primitives;
};

Mesh::Mesh()
    : m_impl(std::make_unique<Impl>()) {}

Mesh::~Mesh() = default;

void Mesh::SetCoords(std::shared_ptr<CoordTable> c)    { m_impl->coords  = std::move(c); }
void Mesh::SetScalars(std::shared_ptr<ScalarTable> s)  { m_impl->scalars = std::move(s); }
void Mesh::AddPrimitiveSet(std::shared_ptr<PrimitiveSet> ps)
{
    m_impl->primitives.push_back(std::move(ps));
}

const CoordTable*  Mesh::Coords()  const { return m_impl->coords.get(); }
const ScalarTable* Mesh::Scalars() const { return m_impl->scalars.get(); }

const std::vector<std::shared_ptr<PrimitiveSet>>& Mesh::PrimitiveSets() const
{
    return m_impl->primitives;
}

} // namespace xcel

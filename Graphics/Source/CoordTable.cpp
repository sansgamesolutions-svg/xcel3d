#include "Graphics/CoordTable.h"

namespace xcel {

struct CoordTable::Impl {
    std::vector<glm::vec3> coords;
};

CoordTable::CoordTable()
    : m_impl(std::make_unique<Impl>()) {}

CoordTable::CoordTable(std::vector<glm::vec3> positions)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->coords = std::move(positions);
}

CoordTable::~CoordTable() = default;

void CoordTable::Reserve(size_t n)          { m_impl->coords.reserve(n); }
void CoordTable::AddCoord(const glm::vec3& p) { m_impl->coords.push_back(p); }
size_t CoordTable::Size() const             { return m_impl->coords.size(); }

const glm::vec3& CoordTable::operator[](size_t i) const { return m_impl->coords[i]; }
const std::vector<glm::vec3>& CoordTable::Data() const  { return m_impl->coords; }

} // namespace xcel

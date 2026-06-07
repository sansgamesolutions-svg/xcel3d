#include "Graphics/CoordTable.h"

namespace xcel {

CoordTable::CoordTable(std::vector<glm::vec3> positions)
    {
    m_coords = std::move(positions);
}

void CoordTable::Reserve(size_t n)          { m_coords.reserve(n); }
void CoordTable::AddCoord(const glm::vec3& p) { m_coords.push_back(p); }
size_t CoordTable::Size() const             { return m_coords.size(); }

const glm::vec3& CoordTable::operator[](size_t i) const { return m_coords[i]; }
const std::vector<glm::vec3>& CoordTable::Data() const  { return m_coords; }

} // namespace xcel

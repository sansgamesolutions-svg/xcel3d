#pragma once
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>

namespace xcel {

class CoordTable
{
public:
    CoordTable() = default;
    explicit CoordTable(std::vector<glm::vec3> positions);

    void             Reserve(size_t n);
    void             AddCoord(const glm::vec3& p);
    size_t           Size() const;
    const glm::vec3& operator[](size_t i) const;
    const std::vector<glm::vec3>& Data() const;

private:
    std::vector<glm::vec3> m_coords;
};

} // namespace xcel

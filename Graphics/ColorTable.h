#pragma once
#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

namespace xcel {

class ColorTable
{
public:
    virtual ~ColorTable() = default;

    virtual glm::vec3 ColorForElement(size_t elem,
                                      float  value,
                                      float  minVal,
                                      float  maxVal) const = 0;

protected:
    ColorTable() = default;
    ColorTable(const ColorTable&)            = delete;
    ColorTable& operator=(const ColorTable&) = delete;
};

class SingleColor : public ColorTable
{
public:
    explicit SingleColor(glm::vec3 color);

    glm::vec3 ColorForElement(size_t elem,
                              float  value,
                              float  minVal,
                              float  maxVal) const override;

private:
    glm::vec3 m_color{0.f, 0.f, 0.f};
};

class PaletteColor : public ColorTable
{
public:
    PaletteColor();

    glm::vec3 ColorForElement(size_t elem,
                              float  value,
                              float  minVal,
                              float  maxVal) const override;

private:
    std::vector<glm::vec3> m_table;
};

class MeshColor : public ColorTable
{
public:
    MeshColor() = default;

    void AddColor(glm::vec3 color);
    void SetColor(size_t elem, glm::vec3 color);

    glm::vec3 ColorForElement(size_t elem,
                              float  value,
                              float  minVal,
                              float  maxVal) const override;

private:
    std::vector<glm::vec3> m_colors;
};

} // namespace xcel

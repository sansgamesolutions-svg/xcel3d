#include "Graphics/ColorTable.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>

namespace xcel {

// ============================================================================
// SingleColor
// ============================================================================

SingleColor::SingleColor(glm::vec3 color)
    {
    m_color = color;
}

glm::vec3 SingleColor::ColorForElement(size_t /*elem*/,
                                        float  /*value*/,
                                        float  /*minVal*/,
                                        float  /*maxVal*/) const
{
    return m_color;
}

// ============================================================================
// PaletteColor
// ============================================================================

PaletteColor::PaletteColor()
    {
    m_table.resize(256);
    // Cool-to-warm: blue(0,0,1) -> cyan(0,1,1) -> green(0,1,0) -> yellow(1,1,0) -> red(1,0,0)
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        glm::vec3 c;
        if (t < 0.25f) {
            float s = t / 0.25f;
            c = glm::mix(glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 1.f, 1.f), s);
        } else if (t < 0.5f) {
            float s = (t - 0.25f) / 0.25f;
            c = glm::mix(glm::vec3(0.f, 1.f, 1.f), glm::vec3(0.f, 1.f, 0.f), s);
        } else if (t < 0.75f) {
            float s = (t - 0.5f) / 0.25f;
            c = glm::mix(glm::vec3(0.f, 1.f, 0.f), glm::vec3(1.f, 1.f, 0.f), s);
        } else {
            float s = (t - 0.75f) / 0.25f;
            c = glm::mix(glm::vec3(1.f, 1.f, 0.f), glm::vec3(1.f, 0.f, 0.f), s);
        }
        m_table[i] = c;
    }
}

glm::vec3 PaletteColor::ColorForElement(size_t /*elem*/,
                                         float  value,
                                         float  minVal,
                                         float  maxVal) const
{
    float range = maxVal - minVal;
    if (range <= 0.f) return m_table[128];
    float t = (value - minVal) / range;
    int idx = static_cast<int>(t * 255.f);
    idx = std::clamp(idx, 0, 255);
    return m_table[idx];
}

// ============================================================================
// MeshColor
// ============================================================================

void MeshColor::AddColor(glm::vec3 color)
{
    m_colors.push_back(color);
}

void MeshColor::SetColor(size_t elem, glm::vec3 color)
{
    if (elem >= m_colors.size())
        throw std::out_of_range("MeshColor::SetColor: element index out of range");
    m_colors[elem] = color;
}

glm::vec3 MeshColor::ColorForElement(size_t elem,
                                      float  /*value*/,
                                      float  /*minVal*/,
                                      float  /*maxVal*/) const
{
    return m_colors[elem];
}

} // namespace xcel

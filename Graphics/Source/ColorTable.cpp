#include "Graphics/ColorTable.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>

namespace xcel {

// ============================================================================
// SingleColor
// ============================================================================

struct SingleColor::Impl {
    glm::vec3 color;
};

SingleColor::SingleColor(glm::vec3 color)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->color = color;
}

SingleColor::~SingleColor() = default;

glm::vec3 SingleColor::ColorForElement(size_t /*elem*/,
                                        float  /*value*/,
                                        float  /*minVal*/,
                                        float  /*maxVal*/) const
{
    return m_impl->color;
}

// ============================================================================
// PaletteColor
// ============================================================================

struct PaletteColor::Impl {
    std::vector<glm::vec3> table;  // 256 entries, cool-to-warm
};

PaletteColor::PaletteColor()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->table.resize(256);
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
        m_impl->table[i] = c;
    }
}

PaletteColor::~PaletteColor() = default;

glm::vec3 PaletteColor::ColorForElement(size_t /*elem*/,
                                         float  value,
                                         float  minVal,
                                         float  maxVal) const
{
    float range = maxVal - minVal;
    if (range <= 0.f) return m_impl->table[128];
    float t = (value - minVal) / range;
    int idx = static_cast<int>(t * 255.f);
    idx = std::clamp(idx, 0, 255);
    return m_impl->table[idx];
}

// ============================================================================
// MeshColor
// ============================================================================

struct MeshColor::Impl {
    std::vector<glm::vec3> colors;
};

MeshColor::MeshColor()
    : m_impl(std::make_unique<Impl>()) {}

MeshColor::~MeshColor() = default;

void MeshColor::AddColor(glm::vec3 color)
{
    m_impl->colors.push_back(color);
}

void MeshColor::SetColor(size_t elem, glm::vec3 color)
{
    if (elem >= m_impl->colors.size())
        throw std::out_of_range("MeshColor::SetColor: element index out of range");
    m_impl->colors[elem] = color;
}

glm::vec3 MeshColor::ColorForElement(size_t elem,
                                      float  /*value*/,
                                      float  /*minVal*/,
                                      float  /*maxVal*/) const
{
    return m_impl->colors[elem];
}

} // namespace xcel

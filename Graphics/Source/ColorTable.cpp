#include "Graphics/ColorTable.h"
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>

namespace xcel {

struct ColorTable::Impl {
    std::vector<glm::vec3> table;  // 256 entries
};

ColorTable::ColorTable()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->table.resize(256);
    // Cool-to-warm: blue (0,0,1) → cyan (0,1,1) → green (0,1,0) → yellow (1,1,0) → red (1,0,0)
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

ColorTable::~ColorTable() = default;

glm::vec3 ColorTable::Map(float t) const {
    int idx = static_cast<int>(t * 255.f);
    idx = std::clamp(idx, 0, 255);
    return m_impl->table[idx];
}

glm::vec3 ColorTable::MapScalar(float value, float minVal, float maxVal) const {
    float range = maxVal - minVal;
    if (range <= 0.f) return m_impl->table[128];
    float t = (value - minVal) / range;
    return Map(t);
}

} // namespace xcel

#pragma once
#include <memory>
#include <glm/glm.hpp>

namespace xcel {

// 256-entry cool-to-warm colormap (blue → cyan → green → yellow → red).
class ColorTable {
public:
    ColorTable();
    ~ColorTable();

    // Map normalized t in [0,1] to RGB.
    glm::vec3 Map(float t) const;

    // Map a raw scalar value given the global min/max to RGB.
    glm::vec3 MapScalar(float value, float minVal, float maxVal) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

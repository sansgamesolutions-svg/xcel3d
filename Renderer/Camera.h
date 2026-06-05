#pragma once
#include <memory>
#include <glm/glm.hpp>

namespace xcel {

// Orbit camera defined by a target point and spherical coordinates.
class Camera {
public:
    Camera();
    ~Camera();

    void Orbit(float dAzimuth, float dElevation);
    void Zoom(float dRadius);
    void Pan(float dx, float dy);

    // Fit camera so the given bounding sphere fills roughly 70% of the view.
    void FitToSphere(const glm::vec3& center, float radius);

    glm::mat4 ViewMatrix() const;
    glm::mat4 ProjMatrix(float aspectRatio) const;
    glm::vec3 Position() const;

    float fovY      = glm::radians(45.0f);
    float nearPlane = 0.01f;
    float farPlane  = 1000.0f;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

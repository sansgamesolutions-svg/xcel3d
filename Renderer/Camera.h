#pragma once
#include <glm/glm.hpp>

namespace xcel {

class Camera
{
public:
    Camera()  = default;
    ~Camera() = default;

    void Orbit(float dAzimuth, float dElevation);
    void Zoom(float dRadius);
    void Pan(float dx, float dy);
    void FitToSphere(const glm::vec3& center, float radius);

    // Snap to an exact orientation (radians).
    void SetAzimuth(float a)   { m_azimuth   = a; }
    void SetElevation(float e) { m_elevation  = e; }
    float Azimuth()   const { return m_azimuth;   }
    float Elevation() const { return m_elevation; }
    float Radius()    const { return m_radius;    }

    glm::mat4 ViewMatrix() const;
    glm::mat4 ProjMatrix(float aspectRatio) const;
    glm::vec3 Position() const;

    float fovY      = glm::radians(45.0f);
    float nearPlane = 0.01f;
    float farPlane  = 1000.0f;

private:
    glm::vec3 m_target    = {0.f, 0.f, 0.f};
    float     m_azimuth   = 0.f;
    float     m_elevation = 0.3f;
    float     m_radius    = 5.f;
};

} // namespace xcel

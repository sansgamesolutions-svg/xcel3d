#include "Graphics/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace xcel {

glm::vec3 Camera::Position() const
{
    float cosEl = std::cos(m_elevation);
    float sinEl = std::sin(m_elevation);
    float cosAz = std::cos(m_azimuth);
    float sinAz = std::sin(m_azimuth);
    return m_target + m_radius * glm::vec3(cosEl * sinAz, sinEl, cosEl * cosAz);
}

glm::mat4 Camera::ViewMatrix() const
{
    glm::vec3 pos = Position();
    glm::vec3 up  = glm::vec3(0.f, 1.f, 0.f);
    // Avoid gimbal lock near poles.
    if (std::abs(m_elevation) > glm::radians(89.f))
        up = glm::vec3(std::cos(m_azimuth), 0.f, -std::sin(m_azimuth));
    return glm::lookAt(pos, m_target, up);
}

glm::mat4 Camera::ProjMatrix(float aspectRatio) const
{
    glm::mat4 proj = glm::perspective(fovY, aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1; // flip Y for Vulkan NDC
    return proj;
}

void Camera::Orbit(float dAzimuth, float dElevation)
{
    m_azimuth   += dAzimuth;
    m_elevation  = std::clamp(m_elevation + dElevation,
                                    glm::radians(-89.f), glm::radians(89.f));
}

void Camera::Zoom(float dRadius)
{
    m_radius = std::max(m_radius + dRadius, 0.01f);
}

void Camera::Pan(float dx, float dy)
{
    glm::vec3 fwd   = glm::normalize(m_target - Position());
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.f, 1.f, 0.f)));
    glm::vec3 up    = glm::cross(right, fwd);
    m_target += right * dx + up * dy;
}

void Camera::FitToSphere(const glm::vec3& center, float radius)
{
    m_target = center;
    m_radius = radius * 2.5f;
    nearPlane = radius * 0.01f;
    farPlane  = radius * 100.f;
}

} // namespace xcel

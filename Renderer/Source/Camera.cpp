#include "Renderer/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace xcel {

struct Camera::Impl {
    glm::vec3 target    = {0.f, 0.f, 0.f};
    float     azimuth   = 0.f;
    float     elevation = 0.3f;
    float     radius    = 5.f;
};

Camera::Camera()
    : m_impl(std::make_unique<Impl>()) {}

Camera::~Camera() = default;

glm::vec3 Camera::Position() const
{
    float cosEl = std::cos(m_impl->elevation);
    float sinEl = std::sin(m_impl->elevation);
    float cosAz = std::cos(m_impl->azimuth);
    float sinAz = std::sin(m_impl->azimuth);
    return m_impl->target + m_impl->radius * glm::vec3(cosEl * sinAz, sinEl, cosEl * cosAz);
}

glm::mat4 Camera::ViewMatrix() const
{
    glm::vec3 pos = Position();
    glm::vec3 up  = glm::vec3(0.f, 1.f, 0.f);
    // Avoid gimbal lock near poles.
    if (std::abs(m_impl->elevation) > glm::radians(89.f))
        up = glm::vec3(std::cos(m_impl->azimuth), 0.f, -std::sin(m_impl->azimuth));
    return glm::lookAt(pos, m_impl->target, up);
}

glm::mat4 Camera::ProjMatrix(float aspectRatio) const
{
    glm::mat4 proj = glm::perspective(fovY, aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1; // flip Y for Vulkan NDC
    return proj;
}

void Camera::Orbit(float dAzimuth, float dElevation)
{
    m_impl->azimuth   += dAzimuth;
    m_impl->elevation  = std::clamp(m_impl->elevation + dElevation,
                                    glm::radians(-89.f), glm::radians(89.f));
}

void Camera::Zoom(float dRadius)
{
    m_impl->radius = std::max(m_impl->radius + dRadius, 0.01f);
}

void Camera::Pan(float dx, float dy)
{
    glm::vec3 fwd   = glm::normalize(m_impl->target - Position());
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.f, 1.f, 0.f)));
    glm::vec3 up    = glm::cross(right, fwd);
    m_impl->target += right * dx + up * dy;
}

void Camera::FitToSphere(const glm::vec3& center, float radius)
{
    m_impl->target = center;
    m_impl->radius = radius * 2.5f;
    nearPlane = radius * 0.01f;
    farPlane  = radius * 100.f;
}

} // namespace xcel

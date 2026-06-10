#pragma once
#include <glm/glm.hpp>
#include <optional>

namespace xcel {

struct Ray
{
    glm::vec3 origin;
    glm::vec3 dir; // unit length
};

// Unprojects a screen-space mouse position into a world-space ray.
// mouseX/Y are in pixels (top-left origin, matching GLFW cursor pos).
inline Ray RayFromScreen(double   mouseX,
                          double   mouseY,
                          int      fbWidth,
                          int      fbHeight,
                          const glm::mat4& invViewProj,
                          glm::vec3        camPos)
{
    // Map to NDC [-1,1] x [-1,1]; Y is flipped (GLFW top=0, NDC top=+1).
    float ndcX = static_cast<float>(2.0 * mouseX / fbWidth  - 1.0);
    float ndcY = static_cast<float>(1.0 - 2.0 * mouseY / fbHeight);

    glm::vec4 nearH = invViewProj * glm::vec4(ndcX, ndcY, 0.f, 1.f);
    glm::vec4 farH  = invViewProj * glm::vec4(ndcX, ndcY, 1.f, 1.f);
    glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
    glm::vec3 farW  = glm::vec3(farH)  / farH.w;

    Ray r;
    r.origin = camPos;
    r.dir    = glm::normalize(farW - nearW);
    return r;
}

// Slab method. Returns t of entry point or nullopt if miss.
inline std::optional<float> RayVsAABB(const Ray&    r,
                                       glm::vec3     mn,
                                       glm::vec3     mx)
{
    glm::vec3 invD = 1.f / r.dir;
    glm::vec3 t0   = (mn - r.origin) * invD;
    glm::vec3 t1   = (mx - r.origin) * invD;
    glm::vec3 tMin = glm::min(t0, t1);
    glm::vec3 tMax = glm::max(t0, t1);
    float tNear = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
    float tFar  = glm::min(glm::min(tMax.x, tMax.y), tMax.z);
    if (tNear > tFar || tFar < 0.f) return std::nullopt;
    return tNear >= 0.f ? tNear : tFar;
}

// Infinite capsule (cylinder + hemispherical caps) test.
// Returns t of closest hit along ray or nullopt.
inline std::optional<float> RayVsCapsule(const Ray&    r,
                                          glm::vec3     base,
                                          glm::vec3     tip,
                                          float         radius)
{
    glm::vec3 axis  = tip - base;
    float     axLen = glm::length(axis);
    if (axLen < 1e-6f) return std::nullopt;
    glm::vec3 axDir = axis / axLen;

    glm::vec3 oc = r.origin - base;
    float dDotA  = glm::dot(r.dir,  axDir);
    float ocDotA = glm::dot(oc,     axDir);

    float a = 1.f - dDotA * dDotA;
    float b = glm::dot(r.dir, oc) - dDotA * ocDotA;
    float c = glm::dot(oc, oc)    - ocDotA * ocDotA - radius * radius;

    std::optional<float> best;

    // Infinite cylinder intersection.
    if (a > 1e-9f) {
        float disc = b * b - a * c;
        if (disc >= 0.f) {
            float sqrtD = std::sqrt(disc);
            for (float sign : {-1.f, 1.f}) {
                float t = (-b + sign * sqrtD) / a;
                if (t < 0.f) continue;
                float proj = ocDotA + t * dDotA;
                if (proj >= 0.f && proj <= axLen) {
                    if (!best || t < *best) best = t;
                }
            }
        }
    }

    // Hemispherical caps.
    for (int cap = 0; cap < 2; ++cap) {
        glm::vec3 centre = (cap == 0) ? base : tip;
        glm::vec3 co = r.origin - centre;
        float bS = glm::dot(r.dir, co);
        float cS = glm::dot(co, co) - radius * radius;
        float disc = bS * bS - cS;
        if (disc < 0.f) continue;
        float t = -bS - std::sqrt(disc);
        if (t < 0.f) t = -bS + std::sqrt(disc);
        if (t >= 0.f && (!best || t < *best)) best = t;
    }

    return best;
}

} // namespace xcel

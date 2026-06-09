#include "Graphics/Manipulator/ManipulatorGeometry.h"
#include "Graphics/DeviceContext.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <numbers>
#include <cmath>

namespace xcel {

namespace {

constexpr int kArrowSides   = 8;
constexpr float kShaftLen   = 0.75f;
constexpr float kShaftR     = 0.04f;
constexpr float kConeR      = 0.10f;
constexpr float kArrowLen   = 1.0f;

void PushCylinder(std::vector<MeshVertex>& verts,
                  std::vector<uint32_t>&   idx,
                  glm::vec3 base, glm::vec3 tip,
                  float radius, int sides,
                  glm::vec3 color)
{
    glm::vec3 axis  = glm::normalize(tip - base);
    // Build two orthogonal vectors to axis.
    glm::vec3 ref   = (std::abs(axis.y) < 0.9f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 u     = glm::normalize(glm::cross(axis, ref));
    glm::vec3 v     = glm::cross(axis, u);

    uint32_t start = static_cast<uint32_t>(verts.size());
    for (int i = 0; i < sides; ++i) {
        float a0 = 2.f * glm::pi<float>() * i       / sides;
        float a1 = 2.f * glm::pi<float>() * (i + 1) / sides;
        glm::vec3 n0 = std::cos(a0) * u + std::sin(a0) * v;
        glm::vec3 n1 = std::cos(a1) * u + std::sin(a1) * v;
        glm::vec3 b0 = base + n0 * radius;
        glm::vec3 b1 = base + n1 * radius;
        glm::vec3 t0 = tip  + n0 * radius;
        glm::vec3 t1 = tip  + n1 * radius;
        glm::vec3 norm = glm::normalize(n0 + n1); // face normal approximation
        uint32_t base_i = static_cast<uint32_t>(verts.size());
        verts.push_back({b0, norm, color});
        verts.push_back({b1, norm, color});
        verts.push_back({t1, norm, color});
        verts.push_back({t0, norm, color});
        idx.push_back(base_i+0); idx.push_back(base_i+1); idx.push_back(base_i+2);
        idx.push_back(base_i+0); idx.push_back(base_i+2); idx.push_back(base_i+3);
    }
    (void)start;
}

void PushCone(std::vector<MeshVertex>& verts,
              std::vector<uint32_t>&   idx,
              glm::vec3 base, glm::vec3 apex,
              float baseRadius, int sides,
              glm::vec3 color)
{
    glm::vec3 axis = glm::normalize(apex - base);
    glm::vec3 ref  = (std::abs(axis.y) < 0.9f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 u    = glm::normalize(glm::cross(axis, ref));
    glm::vec3 v    = glm::cross(axis, u);

    for (int i = 0; i < sides; ++i) {
        float a0 = 2.f * glm::pi<float>() * i       / sides;
        float a1 = 2.f * glm::pi<float>() * (i + 1) / sides;
        glm::vec3 n0   = std::cos(a0) * u + std::sin(a0) * v;
        glm::vec3 n1   = std::cos(a1) * u + std::sin(a1) * v;
        glm::vec3 b0   = base + n0 * baseRadius;
        glm::vec3 b1   = base + n1 * baseRadius;
        glm::vec3 norm = glm::normalize(n0 + n1);
        uint32_t bi    = static_cast<uint32_t>(verts.size());
        verts.push_back({b0,   norm, color});
        verts.push_back({b1,   norm, color});
        verts.push_back({apex, norm, color});
        idx.push_back(bi+0); idx.push_back(bi+1); idx.push_back(bi+2);
    }
}

} // anonymous namespace

void ManipulatorGeometry::BuildArrow(std::vector<MeshVertex>& verts,
                                      std::vector<uint32_t>&   idx)
{
    // Neutral arrow along +Y, colored white. Caller tints via instModel transform + color.
    glm::vec3 col  = {1.f, 1.f, 1.f};
    glm::vec3 base = {0.f, 0.f, 0.f};
    glm::vec3 mid  = {0.f, kShaftLen, 0.f};
    glm::vec3 tip  = {0.f, kArrowLen, 0.f};
    PushCylinder(verts, idx, base, mid,  kShaftR, kArrowSides, col);
    PushCone    (verts, idx, mid,  tip,  kConeR,  kArrowSides, col);
}

void ManipulatorGeometry::BuildCube(std::vector<MeshVertex>& verts,
                                     std::vector<uint32_t>&   idx)
{
    // Six faces of a unit cube [-0.5..0.5]; white vertices, face normals.
    static const glm::vec3 corners[8] = {
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},
        {0.5f, 0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},
    };
    // Face definitions: 4 indices (CCW from outside), outward normal.
    static const int faces[6][4] = {
        {0,1,2,3}, {5,4,7,6}, {0,4,5,1}, {2,6,7,3}, {0,3,7,4}, {1,5,6,2},
    };
    static const glm::vec3 normals[6] = {
        {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},
    };
    // Face colors matching standard orientation widget conventions.
    static const glm::vec3 colors[6] = {
        {0.6f,0.6f,0.6f},{0.6f,0.6f,0.6f},
        {0.3f,0.8f,0.3f},{0.3f,0.8f,0.3f},
        {0.8f,0.3f,0.3f},{0.8f,0.3f,0.3f},
    };
    for (int f = 0; f < 6; ++f) {
        uint32_t bi = static_cast<uint32_t>(verts.size());
        for (int k = 0; k < 4; ++k)
            verts.push_back({corners[faces[f][k]], normals[f], colors[f]});
        idx.push_back(bi+0); idx.push_back(bi+1); idx.push_back(bi+2);
        idx.push_back(bi+0); idx.push_back(bi+2); idx.push_back(bi+3);
    }
}

void ManipulatorGeometry::BuildPlane(std::vector<MeshVertex>& verts,
                                      std::vector<uint32_t>&   idx)
{
    // Unit quad in XZ plane at Y=0 ([-0.5..0.5] x Z[-0.5..0.5]), normal +Y, white.
    glm::vec3 n = {0.f, 1.f, 0.f};
    glm::vec3 c = {1.f, 1.f, 1.f};
    uint32_t bi = static_cast<uint32_t>(verts.size());
    verts.push_back({{-0.5f, 0.f, -0.5f}, n, c});
    verts.push_back({{ 0.5f, 0.f, -0.5f}, n, c});
    verts.push_back({{ 0.5f, 0.f,  0.5f}, n, c});
    verts.push_back({{-0.5f, 0.f,  0.5f}, n, c});
    idx.push_back(bi+0); idx.push_back(bi+1); idx.push_back(bi+2);
    idx.push_back(bi+0); idx.push_back(bi+2); idx.push_back(bi+3);
    // Back face (same quad reversed so section plane is visible from both sides).
    idx.push_back(bi+0); idx.push_back(bi+2); idx.push_back(bi+1);
    idx.push_back(bi+0); idx.push_back(bi+3); idx.push_back(bi+2);
}

void ManipulatorGeometry::Upload(DeviceContext&                dev,
                                  GizmoMesh&                   mesh,
                                  const std::vector<MeshVertex>& verts,
                                  const std::vector<uint32_t>&   idxData)
{
    VkDeviceSize vBytes = verts.size()   * sizeof(MeshVertex);
    VkDeviceSize iBytes = idxData.size() * sizeof(uint32_t);

    mesh.verts.Create(dev.Device(), dev.PhysicalDevice(), vBytes,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mesh.verts.UploadViaStaging(dev, verts.data(), vBytes);

    mesh.indices.Create(dev.Device(), dev.PhysicalDevice(), iBytes,
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mesh.indices.UploadViaStaging(dev, idxData.data(), iBytes);

    mesh.indexCount = static_cast<uint32_t>(idxData.size());
}

void ManipulatorGeometry::Build(DeviceContext& dev)
{
    {
        std::vector<MeshVertex> v; std::vector<uint32_t> i;
        BuildArrow(v, i); Upload(dev, m_arrow, v, i);
    }
    {
        std::vector<MeshVertex> v; std::vector<uint32_t> i;
        BuildCube(v, i); Upload(dev, m_cube, v, i);
    }
    {
        std::vector<MeshVertex> v; std::vector<uint32_t> i;
        BuildPlane(v, i); Upload(dev, m_plane, v, i);
    }
}

void ManipulatorGeometry::Destroy(VkDevice device)
{
    m_arrow.verts.Destroy(device);   m_arrow.indices.Destroy(device);
    m_cube.verts.Destroy(device);    m_cube.indices.Destroy(device);
    m_plane.verts.Destroy(device);   m_plane.indices.Destroy(device);
}

} // namespace xcel

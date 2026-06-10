#include "Renderer/Manipulator/SectionCutManipulator.h"
#include "Renderer/Camera.h"
#include "Renderer/DeviceContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace xcel {

void SectionCutManipulator::Build(DeviceContext&   dev,
                                   const GizmoMesh& planeMesh,
                                   const GizmoMesh& arrowMesh)
{
    m_planeMesh = &planeMesh;
    m_arrowMesh = &arrowMesh;
    m_dev       = &dev;

    glm::mat4 identity{1.f};
    m_planeInstanceBuf.Create(
        dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_planeInstanceBuf.WriteHostVisible(&identity, sizeof(glm::mat4));

    m_arrowInstanceBuf.Create(
        dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_arrowInstanceBuf.WriteHostVisible(&identity, sizeof(glm::mat4));
}

void SectionCutManipulator::Destroy(VkDevice device)
{
    m_planeInstanceBuf.Destroy(device);
    m_arrowInstanceBuf.Destroy(device);
}

glm::vec4 SectionCutManipulator::PlaneEquation() const
{
    if (!m_enabled) return {0.f, 0.f, 0.f, 0.f};
    return {m_normal.x, m_normal.y, m_normal.z, m_d};
}

void SectionCutManipulator::SetSceneAABB(glm::vec3 mn, glm::vec3 mx)
{
    m_planeSize = glm::length(mx - mn) * 0.6f;
    UpdateBuffers();
}

void SectionCutManipulator::UpdateBuffers()
{
    // Plane world position: point on plane is normal * (-d).
    glm::vec3 planeCenter = m_normal * (-m_d);

    // Build rotation: plane mesh is in XZ plane (normal = +Y); rotate to m_normal.
    glm::vec3 up    = {0.f, 1.f, 0.f};
    glm::quat rot   = glm::rotation(up, m_normal);
    float     sz    = m_planeSize;

    glm::mat4 planeMat = glm::translate(glm::mat4{1.f}, planeCenter)
                       * glm::mat4_cast(rot)
                       * glm::scale(glm::mat4{1.f}, glm::vec3(sz, 1.f, sz));
    m_planeInstanceBuf.WriteHostVisible(glm::value_ptr(planeMat), sizeof(glm::mat4));

    // Arrow: at planeCenter, pointing along m_normal, scaled to planeSize * 0.2.
    glm::mat4 arrowMat = glm::translate(glm::mat4{1.f}, planeCenter)
                       * glm::mat4_cast(rot)
                       * glm::scale(glm::mat4{1.f}, glm::vec3(m_planeSize * 0.2f));
    m_arrowInstanceBuf.WriteHostVisible(glm::value_ptr(arrowMat), sizeof(glm::mat4));
}

bool SectionCutManipulator::OnMouseButton(MouseButton btn,
                                            InputAction action,
                                            const Ray&  ray,
                                            Camera&     /*camera*/,
                                            flecs::world& /*ecs*/)
{
    if (!m_enabled || btn != MouseButton::Left) return false;

    if (action == InputAction::Release) {
        m_dragging = false;
        return false;
    }

    // Test ray vs arrow handle capsule.
    glm::vec3 planeCenter = m_normal * (-m_d);
    glm::vec3 arrowTip    = planeCenter + m_normal * (m_planeSize * 0.2f);
    float     capsuleR    = m_planeSize * 0.04f;

    auto hit = RayVsCapsule(ray, planeCenter, arrowTip, capsuleR);
    if (!hit) return false;

    m_dragging = true;
    m_dragT0   = *hit;
    return true;
}

bool SectionCutManipulator::OnCursorMove(double /*x*/, double /*y*/,
                                          int    /*fbWidth*/,
                                          int    /*fbHeight*/,
                                          const Ray&   ray,
                                          Camera&      /*camera*/,
                                          flecs::world& /*ecs*/)
{
    if (!m_dragging) return false;

    // Project camera ray onto the plane normal axis to get new position.
    glm::vec3 planeCenter = m_normal * (-m_d);
    float cosAngle = glm::dot(ray.dir, m_normal);
    if (std::abs(cosAngle) < 1e-4f) return true;

    // Parametric distance: t where ray.origin + t*ray.dir is closest to normal line.
    float t = glm::dot(planeCenter - ray.origin, m_normal) / cosAngle;
    glm::vec3 hitPoint = ray.origin + ray.dir * t;

    // The new plane passes through hitPoint with the same normal.
    m_d = -glm::dot(m_normal, hitPoint);
    if (m_dev) UpdateBuffers();
    return true;
}

void SectionCutManipulator::GatherSolidDrawCalls(std::vector<DrawCall>& out) const
{
    if (!m_enabled || !m_arrowMesh || m_arrowMesh->indexCount == 0) return;
    DrawCall dc{};
    dc.vertexBuffer   = &m_arrowMesh->verts;
    dc.indexBuffer    = &m_arrowMesh->indices;
    dc.indexCount     = m_arrowMesh->indexCount;
    dc.instanceBuffer = &m_arrowInstanceBuf;
    dc.instanceCount  = 1;
    dc.material       = {1.f, 0.8f, 0.f, 1.f}; // yellow arrow, shininess≠0 = axis override
    out.push_back(dc);
}

void SectionCutManipulator::GatherAlphaDrawCalls(std::vector<DrawCall>& out) const
{
    if (!m_enabled || !m_planeMesh || m_planeMesh->indexCount == 0) return;
    DrawCall dc{};
    dc.vertexBuffer   = &m_planeMesh->verts;
    dc.indexBuffer    = &m_planeMesh->indices;
    dc.indexCount     = m_planeMesh->indexCount;
    dc.instanceBuffer = &m_planeInstanceBuf;
    dc.instanceCount  = 1;
    // Alpha plane: encode RGBA(0.2, 0.7, 1.0, 0.3) via material fields.
    // ManipulatorPass reads shininess==0 as "use vertex color at full alpha"
    // and shininess>0 as color override.  We encode RGBA in the 4 floats.
    dc.material = {0.2f, 0.7f, 1.0f, 0.3f};
    out.push_back(dc);
}

} // namespace xcel

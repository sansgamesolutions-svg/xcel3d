#include "Graphics/Manipulator/TranslateManipulator.h"
#include "Graphics/Camera.h"
#include "Graphics/DeviceContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

namespace xcel {

namespace {

// Axis colors: X=red, Y=green, Z=blue.
static const glm::vec3 kAxisColors[3] = {
    {1.f, 0.2f, 0.2f},
    {0.2f, 1.f, 0.2f},
    {0.2f, 0.2f, 1.f},
};

// World-space axis directions.
static const glm::vec3 kAxisDirs[3] = {
    {1.f, 0.f, 0.f},
    {0.f, 1.f, 0.f},
    {0.f, 0.f, 1.f},
};

// Project a point onto a ray and return parameter t.
float ProjectOntoRay(glm::vec3 origin, glm::vec3 dir, glm::vec3 point)
{
    return glm::dot(point - origin, dir);
}

// Closest point between two rays (r1=origin1+t*dir1, r2=origin2+s*dir2).
// Returns t on r1.
float ClosestParamOnRay(glm::vec3 o1, glm::vec3 d1,
                         glm::vec3 o2, glm::vec3 d2)
{
    glm::vec3 w  = o1 - o2;
    float     a  = glm::dot(d1, d1);
    float     b  = glm::dot(d1, d2);
    float     c  = glm::dot(d2, d2);
    float     d  = glm::dot(d1, w);
    float     e  = glm::dot(d2, w);
    float     det = a * c - b * b;
    if (std::abs(det) < 1e-9f) return 0.f;
    return (b * e - c * d) / det;
}

} // anonymous namespace

void TranslateManipulator::Build(DeviceContext& dev, const GizmoMesh& arrowMesh)
{
    m_arrowMesh = &arrowMesh;
    m_dev       = &dev;

    glm::mat4 identity{1.f};
    for (int i = 0; i < 3; ++i) {
        m_instanceBufs[i].Create(
            dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_instanceMats[i] = identity;
        m_instanceBufs[i].WriteHostVisible(&identity, sizeof(glm::mat4));
    }
}

void TranslateManipulator::Destroy(VkDevice device)
{
    for (auto& buf : m_instanceBufs) buf.Destroy(device);
}

void TranslateManipulator::ArrowCapsule(int axis, glm::vec3 center, float scale,
                                         glm::vec3& base, glm::vec3& tip) const
{
    base = center;
    tip  = center + kAxisDirs[axis] * scale;
}

void TranslateManipulator::UpdateInstanceBuffers(DeviceContext& /*dev*/,
                                                  glm::vec3 center,
                                                  float scale)
{
    // Arrow geometry is along +Y; rotate each arrow to its world axis direction.
    static const glm::quat kAxisRots[3] = {
        glm::angleAxis(glm::radians(-90.f), glm::vec3{0,0,1}), // +Y → +X
        glm::quat{1,0,0,0},                                      // +Y stays +Y
        glm::angleAxis(glm::radians( 90.f), glm::vec3{1,0,0}), // +Y → +Z
    };

    for (int i = 0; i < 3; ++i) {
        glm::mat4 m = glm::translate(glm::mat4{1.f}, center)
                    * glm::mat4_cast(kAxisRots[i])
                    * glm::scale(glm::mat4{1.f}, glm::vec3(scale));
        // Tint color into the vertices at draw time by baking it into a uniform-colored
        // instance buffer; instead we rely on vertex color baked in BuildArrow (white)
        // and use a push constant color override set per draw call.
        m_instanceMats[i] = m;
        m_instanceBufs[i].WriteHostVisible(glm::value_ptr(m), sizeof(glm::mat4));
    }
}

void TranslateManipulator::Update(const Camera& camera, flecs::world& ecs)
{
    // Find selected entity.
    m_visible = false;
    flecs::entity selected;
    ecs.each([&](flecs::entity e, const SelectedComponent&) {
        selected = e;
    });
    if (!selected.is_alive()) return;

    // Compute center from AABB or transform.
    glm::vec3 center{0.f};
    if (const auto* bb = selected.get<BoundingBoxComponent>())
        center = (bb->min + bb->max) * 0.5f;
    else if (const auto* tc = selected.get<TransformComponent>())
        center = glm::vec3(tc->matrix[3]);

    // Scale so gizmo appears constant screen size.
    float dist  = glm::length(camera.Position() - center);
    float scale = dist * std::tan(camera.fovY * 0.5f) * 0.3f;

    m_center  = center;
    m_scale   = scale;
    m_visible = true;
    m_selectedEntity = selected;

    if (m_dev) UpdateInstanceBuffers(*m_dev, center, scale);
}

bool TranslateManipulator::OnMouseButton(MouseButton btn,
                                          InputAction action,
                                          const Ray&  ray,
                                          Camera&     /*camera*/,
                                          flecs::world& /*ecs*/)
{
    if (!m_visible) return false;
    if (btn != MouseButton::Left) return false;

    if (action == InputAction::Release) {
        m_dragging   = false;
        m_activeAxis = -1;
        return false;
    }

    // Test each axis capsule.
    float capsuleR = m_scale * 0.12f;
    float best     = std::numeric_limits<float>::max();
    int   picked   = -1;

    for (int i = 0; i < 3; ++i) {
        glm::vec3 base, tip;
        ArrowCapsule(i, m_center, m_scale, base, tip);
        auto hit = RayVsCapsule(ray, base, tip, capsuleR);
        if (hit && *hit < best) { best = *hit; picked = i; }
    }

    if (picked < 0) return false;

    m_dragging   = true;
    m_activeAxis = picked;
    // Record initial axis-ray parameter for drag delta.
    m_dragOrigin = ray.origin + ray.dir * best;
    return true;
}

bool TranslateManipulator::OnCursorMove(double /*x*/, double /*y*/,
                                         int    /*fbWidth*/,
                                         int    /*fbHeight*/,
                                         const Ray&   ray,
                                         Camera&      /*camera*/,
                                         flecs::world& ecs)
{
    if (!m_dragging || m_activeAxis < 0) return false;

    // Find closest point on axis ray to the camera ray.
    glm::vec3 axisDir = kAxisDirs[m_activeAxis];
    float t = ClosestParamOnRay(m_center, axisDir, ray.origin, ray.dir);
    glm::vec3 newCenter = m_center + axisDir * t;
    glm::vec3 delta     = newCenter - m_center;

    // Apply to selected entity transform.
    if (m_selectedEntity.is_alive()) {
        if (auto* tc = m_selectedEntity.get_mut<TransformComponent>()) {
            tc->matrix[3] += glm::vec4(delta, 0.f);
        }
    }
    // Move the bounding box too.
    if (m_selectedEntity.is_alive()) {
        if (auto* bb = m_selectedEntity.get_mut<BoundingBoxComponent>()) {
            bb->min += delta;
            bb->max += delta;
        }
    }

    m_center = newCenter;
    if (m_dev) UpdateInstanceBuffers(*m_dev, m_center, m_scale);
    return true;
}

void TranslateManipulator::GatherSolidDrawCalls(std::vector<DrawCall>& out) const
{
    if (!m_visible || !m_arrowMesh || m_arrowMesh->indexCount == 0) return;

    for (int i = 0; i < 3; ++i) {
        DrawCall dc{};
        dc.vertexBuffer   = &m_arrowMesh->verts;
        dc.indexBuffer    = &m_arrowMesh->indices;
        dc.indexCount     = m_arrowMesh->indexCount;
        dc.instanceBuffer = &m_instanceBufs[i];
        dc.instanceCount  = 1;
        // Encode axis color in the material ambient slot (r,g,b,0) — ManipulatorPass
        // reads this as the color override push constant.
        dc.material.ambientFactor  = kAxisColors[i].r;
        dc.material.diffuseFactor  = kAxisColors[i].g;
        dc.material.specularFactor = kAxisColors[i].b;
        dc.material.shininess      = 1.f; // non-zero = "is axis arrow"
        out.push_back(dc);
    }
}

} // namespace xcel

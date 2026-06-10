#include "Renderer/Manipulator/ViewManipulator.h"
#include "Renderer/Camera.h"
#include "Renderer/DeviceContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace xcel {

namespace {

// Standard snap views: (azimuth, elevation) in radians.
struct SnapView { float az; float el; };
struct FaceSnap { glm::vec3 faceNormal; SnapView snap; };

static const FaceSnap kFaceSnaps[6] = {
    {{ 1, 0, 0}, {0.f,                    0.f}},                         // +X right
    {{-1, 0, 0}, {glm::radians(180.f),    0.f}},                         // -X left
    {{ 0, 1, 0}, {0.f,                    glm::radians( 89.f)}},         // +Y top
    {{ 0,-1, 0}, {0.f,                    glm::radians(-89.f)}},         // -Y bottom
    {{ 0, 0, 1}, {glm::radians( 90.f),    0.f}},                         // +Z front
    {{ 0, 0,-1}, {glm::radians(270.f),    0.f}},                         // -Z back
};

} // anonymous namespace

void ViewManipulator::Build(DeviceContext& dev, const GizmoMesh& cubeMesh)
{
    m_cubeMesh = &cubeMesh;
    glm::mat4 identity{1.f};
    m_instanceBuf.Create(
        dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_instanceBuf.WriteHostVisible(&identity, sizeof(glm::mat4));
}

void ViewManipulator::Destroy(VkDevice device)
{
    m_instanceBuf.Destroy(device);
}

void ViewManipulator::SetViewInfo(const glm::mat4& view, VkExtent2D extent)
{
    m_extent = extent;
    // Extract rotation-only from view matrix (invert the rotation part).
    glm::mat3 rot = glm::mat3(view);
    // The cube should rotate opposite to the camera, at a fixed scale.
    glm::mat4 world = glm::mat4(glm::transpose(rot)) * glm::scale(glm::mat4{1.f}, glm::vec3(0.4f));
    // Position the cube inside the corner region center (in world space is irrelevant;
    // the vertex shader applies proj*view*instModel, so we place the cube at a position
    // that projects to the top-right corner).  We use an NDC-space trick: the cube is
    // placed at a fixed depth and NDC offset will be applied via a custom projection.
    // For simplicity we store the combined world transform and ManipulatorPass applies a
    // special projection for the corner viewport.
    m_cubeWorld = world;
    m_instanceBuf.WriteHostVisible(glm::value_ptr(world), sizeof(glm::mat4));
}

bool ViewManipulator::InCornerRegion(double x, double y, int fbWidth, int fbHeight) const
{
    return x >= fbWidth  - kSize && x < fbWidth
        && y >= 0        && y < kSize;
}

bool ViewManipulator::OnMouseButton(MouseButton btn,
                                     InputAction action,
                                     const Ray&  /*ray*/,
                                     Camera&     camera,
                                     flecs::world& /*ecs*/)
{
    if (btn != MouseButton::Left || action != InputAction::Press) return false;
    // The ray is in world space but we need the corner-click face normal.
    // We reconstruct from the camera view: which cube face is closest to the ray dir.
    // Since the cube is centered at origin (in view space), find the dominant axis.
    glm::vec3 dir = camera.Position() - glm::vec3{0.f};
    dir = glm::normalize(dir);

    float best = -1.f;
    const FaceSnap* snap = nullptr;
    for (const auto& fs : kFaceSnaps) {
        float d = glm::dot(fs.faceNormal, dir);
        if (d > best) { best = d; snap = &fs; }
    }
    if (snap) {
        camera.SetAzimuth(snap->snap.az);
        camera.SetElevation(snap->snap.el);
    }
    return true;
}

void ViewManipulator::GatherSolidDrawCalls(std::vector<DrawCall>& out) const
{
    if (!m_cubeMesh || m_cubeMesh->indexCount == 0) return;
    DrawCall dc{};
    dc.vertexBuffer   = &m_cubeMesh->verts;
    dc.indexBuffer    = &m_cubeMesh->indices;
    dc.indexCount     = m_cubeMesh->indexCount;
    dc.instanceBuffer = &m_instanceBuf;
    dc.instanceCount  = 1;
    dc.material       = {1.f, 0.f, 0.f, 0.f}; // ambient=1 signals "view cube" to pass
    out.push_back(dc);
}

} // namespace xcel

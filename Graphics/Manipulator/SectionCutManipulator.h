#pragma once
#include "Graphics/Manipulator/IManipulator.h"
#include "Graphics/Manipulator/ManipulatorGeometry.h"
#include "Graphics/GpuBuffer.h"
#include <glm/glm.hpp>
#include <optional>

namespace xcel {

class DeviceContext;

// Interactive section-cut plane.
// Renders a semi-transparent quad for the cut plane and a solid normal arrow handle.
// Dragging the arrow translates the plane along its normal.
// The plane equation (xyz=world-normal, w=d) is written into FrameUBO each frame
// so mesh.frag can discard geometry on the negative side.
class SectionCutManipulator final : public IManipulator
{
public:
    SectionCutManipulator()  = default;
    ~SectionCutManipulator() = default;

    SectionCutManipulator(const SectionCutManipulator&)            = delete;
    SectionCutManipulator& operator=(const SectionCutManipulator&) = delete;

    void Build(DeviceContext& dev, const GizmoMesh& planeMesh, const GizmoMesh& arrowMesh);
    void Destroy(VkDevice device);

    void SetEnabled(bool e) { m_enabled = e; }
    bool IsEnabled()  const { return m_enabled; }

    // Returns current plane equation.  (0,0,0,0) when disabled.
    glm::vec4 PlaneEquation() const;

    // Scene AABB hint to size the plane quad.
    void SetSceneAABB(glm::vec3 mn, glm::vec3 mx);

    bool OnMouseButton(MouseButton btn,
                       InputAction action,
                       const Ray&  ray,
                       Camera&     camera,
                       flecs::world& ecs) override;

    bool OnCursorMove(double x, double y,
                      int    fbWidth,
                      int    fbHeight,
                      const Ray&   ray,
                      Camera&      camera,
                      flecs::world& ecs) override;

    void GatherSolidDrawCalls(std::vector<DrawCall>& out) const override;
    void GatherAlphaDrawCalls(std::vector<DrawCall>& out) const override;

    bool IsActive() const override { return m_dragging; }

private:
    void UpdateBuffers();

    const GizmoMesh* m_planeMesh = nullptr;
    const GizmoMesh* m_arrowMesh = nullptr;

    // Host-visible instance buffers: one for the plane quad, one for the normal arrow.
    GpuBuffer m_planeInstanceBuf;
    GpuBuffer m_arrowInstanceBuf;
    DeviceContext* m_dev = nullptr;

    // Plane state.
    glm::vec3 m_normal{0.f, 1.f, 0.f};
    float     m_d      = 0.f;          // plane equation: dot(p,normal)+d=0
    float     m_planeSize = 10.f;      // half-extent of the visible quad
    bool      m_enabled   = false;

    // Drag state.
    bool      m_dragging  = false;
    float     m_dragT0    = 0.f;       // ray t at drag start
};

} // namespace xcel

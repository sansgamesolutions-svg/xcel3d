#pragma once
#include "Graphics/Manipulator/IManipulator.h"
#include "Graphics/Manipulator/ManipulatorGeometry.h"
#include "Graphics/GpuBuffer.h"
#include "Graphics/Component.h"
#include <glm/glm.hpp>
#include <array>
#include <optional>

namespace xcel {

class DeviceContext;

// Three-axis translate gizmo shown when an entity has SelectedComponent.
// Arrows: X=red, Y=green, Z=blue.  Drag an arrow to move the entity along that axis.
class TranslateManipulator final : public IManipulator
{
public:
    TranslateManipulator()  = default;
    ~TranslateManipulator() = default;

    TranslateManipulator(const TranslateManipulator&)            = delete;
    TranslateManipulator& operator=(const TranslateManipulator&) = delete;

    void Build(DeviceContext& dev, const GizmoMesh& arrowMesh);
    void Destroy(VkDevice device);

    // Must be called once per frame before GatherSolidDrawCalls.
    void Update(const Camera& camera, flecs::world& ecs);

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

    bool IsActive() const override { return m_dragging; }

private:
    // Returns the arrow axis capsule endpoints for picking (in world space).
    void ArrowCapsule(int axis, glm::vec3 center, float scale,
                      glm::vec3& base, glm::vec3& tip) const;

    // Recompute instance buffers based on selected entity position and camera.
    void UpdateInstanceBuffers(DeviceContext& dev, glm::vec3 center, float scale);

    const GizmoMesh* m_arrowMesh = nullptr;

    // 3 instance buffers, one per axis. Each holds a single mat4.
    std::array<GpuBuffer, 3> m_instanceBufs;
    std::array<glm::mat4, 3> m_instanceMats{};

    glm::vec3 m_center{0.f};
    float     m_scale  = 1.f;
    bool      m_visible = false;

    // Drag state.
    bool             m_dragging    = false;
    int              m_activeAxis  = -1; // 0=X,1=Y,2=Z
    glm::vec3        m_dragOrigin{0.f};
    flecs::entity    m_selectedEntity{};
    DeviceContext*   m_dev = nullptr;
};

} // namespace xcel

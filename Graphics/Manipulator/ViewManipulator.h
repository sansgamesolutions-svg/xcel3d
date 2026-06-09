#pragma once
#include "Graphics/Manipulator/IManipulator.h"
#include "Graphics/Manipulator/ManipulatorGeometry.h"
#include "Graphics/GpuBuffer.h"
#include <glm/glm.hpp>

namespace xcel {

class DeviceContext;

// Orientation cube rendered in the top-right corner of the viewport.
// Clicking a face snaps the camera to the corresponding standard view.
class ViewManipulator final : public IManipulator
{
public:
    ViewManipulator()  = default;
    ~ViewManipulator() = default;

    ViewManipulator(const ViewManipulator&)            = delete;
    ViewManipulator& operator=(const ViewManipulator&) = delete;

    void Build(DeviceContext& dev, const GizmoMesh& cubeMesh);
    void Destroy(VkDevice device);

    // Corner size in pixels.
    static constexpr int kSize = 80;

    bool OnMouseButton(MouseButton btn,
                       InputAction action,
                       const Ray&  ray,
                       Camera&     camera,
                       flecs::world& ecs) override;

    bool OnCursorMove(double /*x*/, double /*y*/,
                      int    /*fbWidth*/,
                      int    /*fbHeight*/,
                      const Ray&   /*ray*/,
                      Camera&      /*camera*/,
                      flecs::world& /*ecs*/) override { return false; }

    void GatherSolidDrawCalls(std::vector<DrawCall>& out) const override;

    // Returns true if (x,y) in pixels is inside the corner region.
    bool InCornerRegion(double x, double y, int fbWidth, int fbHeight) const;

    // Call each frame with current viewport extent so the cube tracks camera.
    void SetViewInfo(const glm::mat4& view, VkExtent2D extent);

private:
    // Instance buffer holding the cube's world transform (rotation only, in corner).
    GpuBuffer m_instanceBuf;

    const GizmoMesh* m_cubeMesh = nullptr;
    VkExtent2D       m_extent   = {};
    glm::mat4        m_cubeWorld{1.f};
};

} // namespace xcel

#include "Renderer/Manipulator/ManipulatorController.h"
#include "Renderer/Manipulator/PickingRay.h"
#include "Renderer/Camera.h"
#include "Renderer/DeviceContext.h"

namespace xcel {

void ManipulatorController::Build(DeviceContext& dev)
{
    m_geo.Build(dev);
    m_view.Build(dev, m_geo.Cube());
    m_translate.Build(dev, m_geo.Arrow());
    m_sectionCut.Build(dev, m_geo.Plane(), m_geo.Arrow());
}

void ManipulatorController::Destroy(VkDevice device)
{
    m_sectionCut.Destroy(device);
    m_translate.Destroy(device);
    m_view.Destroy(device);
    m_geo.Destroy(device);
}

bool ManipulatorController::OnMouseButton(MouseButton btn, InputAction action,
                                            double mouseX, double mouseY,
                                            int    fbWidth, int fbHeight,
                                            const glm::mat4& invViewProj,
                                            Camera& camera, flecs::world& ecs)
{
    // View cube: checked by screen-space region first (no ray needed).
    if (btn == MouseButton::Left && action == InputAction::Press) {
        if (m_view.InCornerRegion(mouseX, mouseY, fbWidth, fbHeight)) {
            Ray dummyRay{camera.Position(), glm::normalize(-camera.Position())};
            return m_view.OnMouseButton(btn, action, dummyRay, camera, ecs);
        }
    }

    Ray ray = RayFromScreen(mouseX, mouseY, fbWidth, fbHeight, invViewProj, camera.Position());

    if (m_translate.OnMouseButton(btn, action, ray, camera, ecs)) return true;
    if (m_sectionCut.OnMouseButton(btn, action, ray, camera, ecs)) return true;

    return false;
}

bool ManipulatorController::OnCursorMove(double x, double y,
                                          int    fbWidth, int fbHeight,
                                          const glm::mat4& invViewProj,
                                          Camera& camera, flecs::world& ecs)
{
    Ray ray = RayFromScreen(x, y, fbWidth, fbHeight, invViewProj, camera.Position());

    if (m_translate.OnCursorMove(x, y, fbWidth, fbHeight, ray, camera, ecs)) return true;
    if (m_sectionCut.OnCursorMove(x, y, fbWidth, fbHeight, ray, camera, ecs)) return true;

    return false;
}

void ManipulatorController::Update(const Camera& camera, flecs::world& ecs,
                                    const glm::mat4& view, VkExtent2D extent)
{
    m_view.SetViewInfo(view, extent);
    m_translate.Update(camera, ecs);
}

void ManipulatorController::GatherDrawCalls(std::vector<DrawCall>& solid,
                                             std::vector<DrawCall>& alpha) const
{
    m_view.GatherSolidDrawCalls(solid);
    m_translate.GatherSolidDrawCalls(solid);
    m_sectionCut.GatherSolidDrawCalls(solid);
    m_sectionCut.GatherAlphaDrawCalls(alpha);
}

glm::vec4 ManipulatorController::SectionPlane() const
{
    return m_sectionCut.PlaneEquation();
}

void ManipulatorController::EnableSectionCut(bool enable)
{
    m_sectionCut.SetEnabled(enable);
}

} // namespace xcel

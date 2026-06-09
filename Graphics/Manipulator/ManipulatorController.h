#pragma once
#include "Graphics/Manipulator/ManipulatorGeometry.h"
#include "Graphics/Manipulator/ViewManipulator.h"
#include "Graphics/Manipulator/TranslateManipulator.h"
#include "Graphics/Manipulator/SectionCutManipulator.h"
#include "Graphics/DrawCall.h"
#include "Platforms/IWindowWidget.h"
#include <glm/glm.hpp>
#include <flecs.h>
#include <vector>

namespace xcel {

class Camera;
class DeviceContext;

// Owns all manipulators and routes input events.
// WindowContext calls the On* methods first; if they return true the event is consumed
// and camera orbit/pan handling is skipped.
class ManipulatorController
{
public:
    ManipulatorController()  = default;
    ~ManipulatorController() = default;

    ManipulatorController(const ManipulatorController&)            = delete;
    ManipulatorController& operator=(const ManipulatorController&) = delete;

    void Build(DeviceContext& dev);
    void Destroy(VkDevice device);

    // Input routing — called from WindowContext callback lambdas.
    // Returns true if the event should not reach the camera.
    bool OnMouseButton(MouseButton btn, InputAction action,
                       double mouseX, double mouseY,
                       int    fbWidth, int fbHeight,
                       const glm::mat4& invViewProj,
                       Camera& camera, flecs::world& ecs);

    bool OnCursorMove(double x, double y,
                      int    fbWidth, int fbHeight,
                      const glm::mat4& invViewProj,
                      Camera& camera, flecs::world& ecs);

    // Called once per frame before GatherDrawCalls; updates gizmo positions.
    void Update(const Camera& camera, flecs::world& ecs,
                const glm::mat4& view, VkExtent2D extent);

    // Populates draw call lists for ManipulatorPass.
    void GatherDrawCalls(std::vector<DrawCall>& solid,
                          std::vector<DrawCall>& alpha) const;

    // Returns the active section plane for FrameUBO upload.
    glm::vec4 SectionPlane() const;

    void EnableSectionCut(bool enable);

    // Expose manipulators for fine-grained control if needed.
    SectionCutManipulator& GetSectionCutManipulator() { return m_sectionCut; }

private:
    ManipulatorGeometry   m_geo;
    ViewManipulator       m_view;
    TranslateManipulator  m_translate;
    SectionCutManipulator m_sectionCut;
};

} // namespace xcel

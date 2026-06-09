#pragma once
#include "Graphics/DrawCall.h"
#include "Graphics/Manipulator/PickingRay.h"
#include "Platforms/IWindowWidget.h"
#include <flecs.h>
#include <vector>

namespace xcel {

class Camera;

class IManipulator
{
public:
    virtual ~IManipulator() = default;

    // Returns true if the event was consumed (caller should not pass to camera).
    virtual bool OnMouseButton(MouseButton btn,
                               InputAction action,
                               const Ray&  ray,
                               Camera&     camera,
                               flecs::world& ecs) = 0;

    virtual bool OnCursorMove(double x, double y,
                              int    fbWidth,
                              int    fbHeight,
                              const Ray&   ray,
                              Camera&      camera,
                              flecs::world& ecs) = 0;

    virtual void GatherSolidDrawCalls(std::vector<DrawCall>& out) const = 0;
    virtual void GatherAlphaDrawCalls(std::vector<DrawCall>& /*out*/) const {}

    virtual bool IsActive() const { return false; }
};

} // namespace xcel

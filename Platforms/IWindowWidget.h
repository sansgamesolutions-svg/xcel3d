#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <string>
#include <vector>

namespace xcel {

// Platform-neutral mouse button identifiers.
// Values intentionally match GLFW constants so GlfwWindowWidget can static_cast.
enum class MouseButton { Left = 0, Right = 1, Middle = 2 };

// Platform-neutral input action.
// Values intentionally match GLFW constants (GLFW_RELEASE = 0, GLFW_PRESS = 1).
enum class InputAction  { Release = 0, Press = 1 };

// Pure-virtual interface for a platform window that supports Vulkan rendering.
// No GLFW types appear here — every concrete implementation hides them in its
// own PIMPL .cpp.
//
// Responsibilities:
//   - Window lifecycle (creation handled by the concrete constructor)
//   - Event polling / waiting
//   - Framebuffer size query (needed by Swapchain extent selection)
//   - Vulkan surface creation and required instance extension list
//   - Input event delivery via std::function callbacks
//
// Concrete implementations: GlfwWindowWidget (Platforms/GlfwWindowWidget.h)
class IWindowWidget {
public:
    virtual ~IWindowWidget() = default;

    // -- Lifecycle -------------------------------------------------------------
    [[nodiscard]] virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;
    virtual void WaitEvents() = 0;

    // -- Size ------------------------------------------------------------------
    virtual void GetFramebufferSize(int& width, int& height) const = 0;

    // -- Vulkan integration ----------------------------------------------------
    [[nodiscard]] virtual VkSurfaceKHR             CreateVulkanSurface(VkInstance instance) const = 0;
    [[nodiscard]] virtual std::vector<const char*> RequiredVulkanExtensions()               const = 0;

    // -- Input callbacks (no platform types in signatures) ---------------------
    virtual void SetFramebufferResizeCallback(std::function<void(int, int)>                       cb) = 0;
    virtual void SetScrollCallback           (std::function<void(double, double)>                 cb) = 0;
    virtual void SetCursorPosCallback        (std::function<void(double, double)>                 cb) = 0;
    virtual void SetMouseButtonCallback      (std::function<void(MouseButton, InputAction, int)>  cb) = 0;
};

} // namespace xcel

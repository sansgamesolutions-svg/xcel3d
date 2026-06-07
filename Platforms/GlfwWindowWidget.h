#pragma once
#include "Platforms/IWindowWidget.h"
#include <functional>
#include <string>

struct GLFWwindow;  // forward declaration — keeps <GLFW/glfw3.h> out of this header

namespace xcel {

class GlfwWindowWidget : public IWindowWidget
{
public:
    GlfwWindowWidget(int width, int height, const std::string& title);
    ~GlfwWindowWidget() override;

    GlfwWindowWidget(const GlfwWindowWidget&)            = delete;
    GlfwWindowWidget& operator=(const GlfwWindowWidget&) = delete;

    [[nodiscard]] bool ShouldClose() const override;
    void PollEvents() override;
    void WaitEvents() override;

    void GetFramebufferSize(int& width, int& height) const override;

    [[nodiscard]] VkSurfaceKHR             CreateVulkanSurface(VkInstance instance) const override;
    [[nodiscard]] std::vector<const char*> RequiredVulkanExtensions()               const override;

    void SetFramebufferResizeCallback(std::function<void(int, int)>                      cb) override;
    void SetScrollCallback           (std::function<void(double, double)>                cb) override;
    void SetCursorPosCallback        (std::function<void(double, double)>                cb) override;
    void SetMouseButtonCallback      (std::function<void(MouseButton, InputAction, int)> cb) override;

private:
    static void OnFramebufferResize(GLFWwindow* w, int width, int height);
    static void OnScroll(GLFWwindow* w, double xOffset, double yOffset);
    static void OnCursorPos(GLFWwindow* w, double x, double y);
    static void OnMouseButton(GLFWwindow* w, int button, int action, int mods);

    GLFWwindow* m_window = nullptr;

    std::function<void(int, int)>                      m_resizeCb;
    std::function<void(double, double)>                m_scrollCb;
    std::function<void(double, double)>                m_cursorCb;
    std::function<void(MouseButton, InputAction, int)> m_mouseBtnCb;
};

} // namespace xcel

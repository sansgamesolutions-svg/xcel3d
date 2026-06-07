#include "Platforms/GlfwWindowWidget.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace xcel {

void GlfwWindowWidget::OnFramebufferResize(GLFWwindow* w, int width, int height)
{
    auto& self = *static_cast<GlfwWindowWidget*>(glfwGetWindowUserPointer(w));
    if (self.m_resizeCb) self.m_resizeCb(width, height);
}

void GlfwWindowWidget::OnScroll(GLFWwindow* w, double xOffset, double yOffset)
{
    auto& self = *static_cast<GlfwWindowWidget*>(glfwGetWindowUserPointer(w));
    if (self.m_scrollCb) self.m_scrollCb(xOffset, yOffset);
}

void GlfwWindowWidget::OnCursorPos(GLFWwindow* w, double x, double y)
{
    auto& self = *static_cast<GlfwWindowWidget*>(glfwGetWindowUserPointer(w));
    if (self.m_cursorCb) self.m_cursorCb(x, y);
}

void GlfwWindowWidget::OnMouseButton(GLFWwindow* w, int button, int action, int mods)
{
    auto& self = *static_cast<GlfwWindowWidget*>(glfwGetWindowUserPointer(w));
    if (self.m_mouseBtnCb)
        self.m_mouseBtnCb(static_cast<MouseButton>(button),
                          static_cast<InputAction>(action),
                          mods);
}

GlfwWindowWidget::GlfwWindowWidget(int width, int height, const std::string& title)
{
    if (!glfwInit())
        throw std::runtime_error("GlfwWindowWidget: glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window)
        throw std::runtime_error("GlfwWindowWidget: glfwCreateWindow failed");

    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, OnFramebufferResize);
    glfwSetScrollCallback         (m_window, OnScroll);
    glfwSetCursorPosCallback      (m_window, OnCursorPos);
    glfwSetMouseButtonCallback    (m_window, OnMouseButton);
}

GlfwWindowWidget::~GlfwWindowWidget()
{
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

bool GlfwWindowWidget::ShouldClose() const
{
    return glfwWindowShouldClose(m_window) != 0;
}

void GlfwWindowWidget::PollEvents() { glfwPollEvents(); }
void GlfwWindowWidget::WaitEvents() { glfwWaitEvents(); }

void GlfwWindowWidget::GetFramebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(m_window, &width, &height);
}

VkSurfaceKHR GlfwWindowWidget::CreateVulkanSurface(VkInstance instance) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("GlfwWindowWidget: glfwCreateWindowSurface failed");
    return surface;
}

std::vector<const char*> GlfwWindowWidget::RequiredVulkanExtensions() const
{
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(exts, exts + count);
}

void GlfwWindowWidget::SetFramebufferResizeCallback(std::function<void(int, int)> cb)
{
    m_resizeCb = std::move(cb);
}

void GlfwWindowWidget::SetScrollCallback(std::function<void(double, double)> cb)
{
    m_scrollCb = std::move(cb);
}

void GlfwWindowWidget::SetCursorPosCallback(std::function<void(double, double)> cb)
{
    m_cursorCb = std::move(cb);
}

void GlfwWindowWidget::SetMouseButtonCallback(std::function<void(MouseButton, InputAction, int)> cb)
{
    m_mouseBtnCb = std::move(cb);
}

} // namespace xcel

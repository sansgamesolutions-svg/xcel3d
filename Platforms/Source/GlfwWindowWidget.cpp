#include "Platforms/GlfwWindowWidget.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace xcel {

// â”€â”€ Impl â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct GlfwWindowWidget::Impl {
    GLFWwindow* window = nullptr;

    std::function<void(int, int)>                      resizeCb;
    std::function<void(double, double)>                scrollCb;
    std::function<void(double, double)>                cursorCb;
    std::function<void(MouseButton, InputAction, int)> mouseBtnCb;

    // â”€â”€ GLFW static trampolines â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Each trampoline retrieves the Impl pointer stored via glfwSetWindowUserPointer
    // and forwards the event to the registered std::function callback.

    static void OnFramebufferResize(GLFWwindow* w, int width, int height)
    {
        auto& impl = *static_cast<Impl*>(glfwGetWindowUserPointer(w));
        if (impl.resizeCb) impl.resizeCb(width, height);
    }

    static void OnScroll(GLFWwindow* w, double xOffset, double yOffset)
    {
        auto& impl = *static_cast<Impl*>(glfwGetWindowUserPointer(w));
        if (impl.scrollCb) impl.scrollCb(xOffset, yOffset);
    }

    static void OnCursorPos(GLFWwindow* w, double x, double y)
    {
        auto& impl = *static_cast<Impl*>(glfwGetWindowUserPointer(w));
        if (impl.cursorCb) impl.cursorCb(x, y);
    }

    static void OnMouseButton(GLFWwindow* w, int button, int action, int mods)
    {
        auto& impl = *static_cast<Impl*>(glfwGetWindowUserPointer(w));
        if (impl.mouseBtnCb)
            impl.mouseBtnCb(static_cast<MouseButton>(button),
                            static_cast<InputAction>(action),
                            mods);
    }
};

// â”€â”€ Construction / destruction â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

GlfwWindowWidget::GlfwWindowWidget(int width, int height, const std::string& title)
    : m_impl(std::make_unique<Impl>())
{
    if (!glfwInit())
        throw std::runtime_error("GlfwWindowWidget: glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

    m_impl->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_impl->window)
        throw std::runtime_error("GlfwWindowWidget: glfwCreateWindow failed");

    // Store the Impl pointer so trampolines can reach the callbacks
    glfwSetWindowUserPointer(m_impl->window, m_impl.get());

    glfwSetFramebufferSizeCallback(m_impl->window, Impl::OnFramebufferResize);
    glfwSetScrollCallback         (m_impl->window, Impl::OnScroll);
    glfwSetCursorPosCallback      (m_impl->window, Impl::OnCursorPos);
    glfwSetMouseButtonCallback    (m_impl->window, Impl::OnMouseButton);
}

GlfwWindowWidget::~GlfwWindowWidget()
{
    if (m_impl->window) {
        glfwDestroyWindow(m_impl->window);
        m_impl->window = nullptr;
    }
    glfwTerminate();
}

// â”€â”€ IWindowWidget â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool GlfwWindowWidget::ShouldClose() const
{
    return glfwWindowShouldClose(m_impl->window) != 0;
}

void GlfwWindowWidget::PollEvents() { glfwPollEvents(); }
void GlfwWindowWidget::WaitEvents() { glfwWaitEvents(); }

void GlfwWindowWidget::GetFramebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(m_impl->window, &width, &height);
}

VkSurfaceKHR GlfwWindowWidget::CreateVulkanSurface(VkInstance instance) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_impl->window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("GlfwWindowWidget: glfwCreateWindowSurface failed");
    return surface;
}

std::vector<const char*> GlfwWindowWidget::RequiredVulkanExtensions() const
{
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(exts, exts + count);
}

// â”€â”€ Callback registration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void GlfwWindowWidget::SetFramebufferResizeCallback(std::function<void(int, int)> cb)
{
    m_impl->resizeCb = std::move(cb);
}

void GlfwWindowWidget::SetScrollCallback(std::function<void(double, double)> cb)
{
    m_impl->scrollCb = std::move(cb);
}

void GlfwWindowWidget::SetCursorPosCallback(std::function<void(double, double)> cb)
{
    m_impl->cursorCb = std::move(cb);
}

void GlfwWindowWidget::SetMouseButtonCallback(std::function<void(MouseButton, InputAction, int)> cb)
{
    m_impl->mouseBtnCb = std::move(cb);
}

} // namespace xcel

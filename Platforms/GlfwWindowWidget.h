#pragma once
#include "Platforms/IWindowWidget.h"
#include <memory>
#include <string>

namespace xcel {

// GLFW-backed implementation of IWindowWidget.
// <GLFW/glfw3.h> is included only in GlfwWindowWidget.cpp — this header is clean.
class GlfwWindowWidget : public IWindowWidget {
public:
    GlfwWindowWidget(int width, int height, const std::string& title);
    ~GlfwWindowWidget() override;

    GlfwWindowWidget(const GlfwWindowWidget&)            = delete;
    GlfwWindowWidget& operator=(const GlfwWindowWidget&) = delete;

    // -- IWindowWidget ---------------------------------------------------------
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
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

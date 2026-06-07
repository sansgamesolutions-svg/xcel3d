#pragma once
#include "Common/ISystem.h"
#include "Platforms/IWindowWidget.h"
#include "Graphics/VulkanContext.h"
#include "Graphics/Swapchain.h"
#include "Graphics/DescriptorManager.h"
#include "Graphics/Camera.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/GpuBuffer.h"
#include "Graphics/PassOptions.h"
#include "Graphics/World.h"
#include "Common/ThreadPool.h"
#include <concepts>
#include <functional>
#include <memory>
#include <vector>

namespace xcel {

class WindowContext
{
public:
    explicit WindowContext(std::unique_ptr<IWindowWidget> widget);
    ~WindowContext();

    WindowContext(const WindowContext&)            = delete;
    WindowContext& operator=(const WindowContext&) = delete;

    World&  GetWorld();
    Camera& GetCamera();

    void SetPassOptions(const PassOptions& opts);
    void Run();

    template<typename T, typename... Args>
        requires std::derived_from<T, ISystem>
    T& AddSystem(Args&&... args)
    {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref   = *sys;
        AddSystemImpl(std::move(sys));
        return ref;
    }

    size_t         DeviceCount() const;
    DeviceContext& GetDevice(size_t index) const;
    DeviceContext* FindDevice(std::function<bool(const DeviceContext&)> pred) const;

    VkInstance   Instance() const;
    VkSurfaceKHR Surface()  const;

private:
    void InitVulkan();
    void BuildMeshes();
    void MainLoop();
    void DrawFrame();
    void UpdateUBO(uint32_t frameIndex);
    void Cleanup();
    void AddSystemImpl(std::unique_ptr<ISystem> system);

    // Windowing + input
    std::unique_ptr<IWindowWidget> m_widget;
    double m_lastMouseX = 0.0, m_lastMouseY = 0.0;
    bool   m_mousePressed = false;

    // Vulkan bootstrap
    VulkanContext m_vulkan;

    // Rendering
    Swapchain         m_swapchain;
    DescriptorManager m_descriptors;
    Camera            m_camera;
    RenderGraph       m_renderGraph;
    GpuBuffer         m_defaultInstanceBuffer;
    PassOptions       m_passOptions;
    bool              m_graphDirty         = false;
    bool              m_framebufferResized = false;

    // Scene + systems
    World                                 m_world;
    std::vector<std::unique_ptr<ISystem>> m_systems;
    ThreadPool                            m_pool;
};

} // namespace xcel

#pragma once
#include "Common/ISystem.h"
#include "Platforms/IWindowWidget.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/Swapchain.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/TextureManager.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/PassOptions.h"
#include "Renderer/World.h"
#include "Renderer/Manipulator/ManipulatorController.h"
#include "Common/ThreadPool.h"
#include <concepts>
#include <filesystem>
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

    World&                 GetWorld();
    Camera&                GetCamera();
    ManipulatorController& GetManipulators();

    void SetPassOptions(const PassOptions& opts);
    void SetShaderDir(std::filesystem::path dir);

    // Upload RGBA8 pixels from CPU; returns opaque texture index for use in MaterialComponent.
    uint32_t UploadTexture(uint32_t width, uint32_t height, const void* pixels);
    void     FreeTexture(uint32_t index);

    // Two-phase API for embedding in a foreign event loop.
    // Call Init() once, then Tick() each frame (returns false when the window
    // requests close). Run() is the blocking convenience wrapper.
    void Init();
    bool Tick();
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
    void DrawFrame();
    void UpdateUBO(uint32_t frameIndex);
    void Cleanup();
    void AddSystemImpl(std::unique_ptr<ISystem> system);

    // Windowing + input
    std::unique_ptr<IWindowWidget> m_widget;
    double m_lastMouseX   = 0.0, m_lastMouseY   = 0.0;
    double m_mouseDownX   = 0.0, m_mouseDownY   = 0.0;
    bool   m_leftPressed  = false;
    bool   m_rightPressed = false;
    bool   m_isDragging   = false;
    bool   m_initialized  = false;

    std::filesystem::path m_shaderDir = "shaders/";

    // Vulkan bootstrap
    VulkanContext m_vulkan;

    // Rendering
    Swapchain         m_swapchain;
    DescriptorManager m_descriptors;
    TextureManager    m_textures;
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

    // Per-frame draw call lists (reused across frames to avoid heap alloc)
    std::vector<DrawCall> m_drawCalls;

    // Manipulators and picking
    ManipulatorController m_manipulators;
    std::vector<DrawCall> m_manipulatorSolidDraws;
    std::vector<DrawCall> m_manipulatorAlphaDraws;
};

} // namespace xcel

#pragma once
#include "Common/ISystem.h"
#include "Platforms/IWindowWidget.h"
#include "Graphics/VulkanContext.h"
#include "Graphics/PassOptions.h"
#include "Graphics/World.h"
#include <concepts>
#include <functional>
#include <memory>

namespace xcel {

class Camera;

// Owns the Vulkan rendering stack for one application window.
// The windowing backend is injected as an IWindowWidget.
//
// Scene objects (meshes, instances) are added through the World returned by
// GetWorld(). Input drives the Camera returned by GetCamera().
class WindowContext {
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

    // ── Multi-device access ───────────────────────────────────────────────────
    size_t         DeviceCount() const;
    DeviceContext& GetDevice(size_t index) const;
    DeviceContext* FindDevice(std::function<bool(const DeviceContext&)> pred) const;

    // ── Vulkan instance / surface ─────────────────────────────────────────────
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

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

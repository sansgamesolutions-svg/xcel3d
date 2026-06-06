#pragma once
#include "Common/ISystem.h"
#include "Platforms/IWindowWidget.h"
#include "Graphics/DeviceContext.h"
#include "Graphics/PassOptions.h"
#include "Graphics/World.h"
#include <concepts>
#include <functional>
#include <memory>

namespace xcel {

class Camera;

// Owns the complete Vulkan rendering stack for one application window.
// The windowing backend is injected as an IWindowWidget — callers create a
// GlfwWindowWidget (or any other backend) and move it in:
//
//   auto w = std::make_unique<GlfwWindowWidget>(1280, 720, "Title");
//   WindowContext ctx(std::move(w));
//
// Scene objects (meshes, instances) are added through the World returned by
// GetWorld(), not through WindowContext directly.
class WindowContext {
public:
    explicit WindowContext(std::unique_ptr<IWindowWidget> widget);
    ~WindowContext();

    WindowContext(const WindowContext&)            = delete;
    WindowContext& operator=(const WindowContext&) = delete;

    // Returns the scene graph; use it to add meshes and instances before Run().
    World& GetWorld();

    // Returns the orbit camera driven by input events.
    Camera& GetCamera();

    // Configure which GPU culling passes are active.
    // Takes effect on the next DrawFrame() call (triggers a RenderGraph rebuild).
    void SetPassOptions(const PassOptions& opts);

    // Enter the blocking render loop. Returns when the window is closed.
    void Run();

    // Register a system to run once per frame before the draw call.
    template<typename T, typename... Args>
        requires std::derived_from<T, ISystem>
    T& AddSystem(Args&&... args)
    {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref   = *sys;
        AddSystemImpl(std::move(sys));
        return ref;
    }

    // ── Multi-device access ────────────────────────────────────────────────────
    size_t         DeviceCount() const;
    DeviceContext& GetDevice(size_t index) const;
    DeviceContext* FindDevice(std::function<bool(const DeviceContext&)> pred) const;

    // ── Vulkan instance / surface ──────────────────────────────────────────────
    VkInstance   Instance() const;
    VkSurfaceKHR Surface()  const;

private:
    void InitVulkan();
    void CreateInstance(bool enableValidation);
    void SetupDebugMessenger();
    void CreateSurface();
    void EnumerateDevices(bool enableValidation);
    void BuildMeshes();
    void MainLoop();
    void DrawFrame();
    void UpdateUBO(uint32_t frameIndex);
    void HandleResize();
    void Cleanup();

    bool IsDeviceSuitable(VkPhysicalDevice dev) const;
    void AddSystemImpl(std::unique_ptr<ISystem> system);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT    severity,
        VkDebugUtilsMessageTypeFlagsEXT           type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                     pUserData);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

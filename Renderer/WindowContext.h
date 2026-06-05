#pragma once
#include "Common/Entity.h"
#include "Common/ISystem.h"
#include "Platforms/IWindowWidget.h"
#include "Renderer/DeviceContext.h"
#include <concepts>
#include <functional>
#include <memory>
#include <string>

namespace xcel {

class Camera;
class Drawable;

// Owns the complete Vulkan rendering stack for one application window.
// The windowing backend is injected as an IWindowWidget â€” callers create a
// GlfwWindowWidget (or any other backend) and move it in:
//
//   auto w = std::make_unique<GlfwWindowWidget>(1280, 720, "Title");
//   WindowContext ctx(std::move(w));
//
// No GLFW or platform types appear in this header.
//
// Object management uses the ECS Registry internally. AddMesh() returns an
// Entity handle for per-object operations (visibility, transform, etc.).
class WindowContext {
public:
    explicit WindowContext(std::unique_ptr<IWindowWidget> widget);
    ~WindowContext();

    WindowContext(const WindowContext&)            = delete;
    WindowContext& operator=(const WindowContext&) = delete;

    // Tessellate and upload a mesh to GPU-resident buffers. Must be called
    // before Run(). Returns the Entity handle for the newly created object.
    Entity AddMesh(const std::string& name, std::shared_ptr<Drawable> mesh);

    // Returns the orbit camera driven by input events.
    Camera& GetCamera();

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

    // â”€â”€ Multi-device access â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    size_t         DeviceCount() const;
    DeviceContext& GetDevice(size_t index) const;
    DeviceContext* FindDevice(std::function<bool(const DeviceContext&)> pred) const;

    // â”€â”€ Vulkan instance / surface â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    VkInstance   Instance() const;
    VkSurfaceKHR Surface()  const;

private:
    void InitVulkan();
    void CreateInstance(bool enableValidation);
    void SetupDebugMessenger();
    void CreateSurface();
    void EnumerateDevices(bool enableValidation);
    void BuildMeshes();
    void CreateSyncObjects();
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

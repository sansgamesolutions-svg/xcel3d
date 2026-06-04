#pragma once
#include "Common/Entity.h"
#include "Common/ISystem.h"
#include "Graphics/DeviceContext.h"
#include <concepts>
#include <functional>
#include <memory>
#include <string>

struct GLFWwindow;

namespace xcel {

class Camera;
class StaticMesh;

// Owns the GLFW window and the complete Vulkan rendering stack for one
// application window.  This class is the hard boundary between the Viewer
// layer and the Graphics/Vulkan layer: its public API exposes no raw Vulkan
// or GLFW types — those are fully hidden behind PIMPL.
//
// Vulkan instance, surface, debug messenger, and device management are
// owned directly by this class.  All suitable physical devices are enumerated
// at Init time, ranked by score, and exposed as DeviceContext objects.
//
// Object management uses the ECS Registry internally. AddMesh() returns an
// Entity handle that callers can use to query or modify the object later via
// the registry (VisibilityComponent, TransformComponent, etc.).
class WindowContext {
public:
    WindowContext(int width, int height, const std::string& title);
    ~WindowContext();

    WindowContext(const WindowContext&)            = delete;
    WindowContext& operator=(const WindowContext&) = delete;

    // Tessellate and upload a mesh to GPU-resident buffers. Must be called
    // before Run(). Returns the Entity handle for the newly created object.
    Entity AddMesh(const std::string& name, std::shared_ptr<StaticMesh> mesh);

    // Returns the orbit camera driven by mouse input.
    Camera& GetCamera();

    // Enter the blocking render loop. Returns when the window is closed.
    void Run();

    // Register a system to run once per frame (in registration order) before
    // the draw call. Systems are constructed with the provided arguments and
    // receive the ECS Registry on every Update() call.
    //
    // Example:
    //   ctx.AddSystem<MyTransformSystem>(someRef);
    template<typename T, typename... Args>
        requires std::derived_from<T, ISystem>
    T& AddSystem(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref   = *sys;
        AddSystemImpl(std::move(sys));
        return ref;
    }

    // ── Multi-device access ───────────────────────────────────────────────────
    size_t         DeviceCount() const;
    DeviceContext& GetDevice(size_t index) const;
    DeviceContext* FindDevice(std::function<bool(const DeviceContext&)> pred) const;

    // ── Vulkan instance / surface (for advanced callers) ─────────────────────
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

    static void FramebufferResizeCallback(GLFWwindow* w, int width, int height);
    static void ScrollCallback(GLFWwindow* w, double xOffset, double yOffset);
    static void CursorPosCallback(GLFWwindow* w, double x, double y);
    static void MouseButtonCallback(GLFWwindow* w, int button, int action, int mods);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT    severity,
        VkDebugUtilsMessageTypeFlagsEXT           type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                     pUserData);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

#pragma once
#include "Common/Entity.h"
#include "Common/ISystem.h"
#include "Platforms/IWindowWidget.h"
#include "Renderer/DeviceContext.h"
#include <glm/glm.hpp>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xcel {

class Camera;
class CoordTable;
class ColorTable;
class PrimitiveSet;
class ScalarTable;

// Owns the complete Vulkan rendering stack for one application window.
// The windowing backend is injected as an IWindowWidget — callers create a
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

    // ── Batched mesh ───────────────────────────────────────────────────────────
    // Create a mesh entity. All data goes as ECS components directly on the entity.
    // BatchingSystem packs it into a shared GPU page (by PrimitiveType + byte budget).
    // Must be called before Run().
    Entity AddMesh(const std::string&                         name,
                   std::shared_ptr<CoordTable>                coords,
                   std::shared_ptr<ScalarTable>               scalars,
                   std::shared_ptr<ColorTable>                colorTable,
                   std::vector<std::shared_ptr<PrimitiveSet>> primSets);

    // ── Instanced mesh ─────────────────────────────────────────────────────────
    // Create a mesh entity for GPU instancing. An InstanceDrawable is created
    // internally and wired to the entity's ECS components.
    // Call AddInstance(entity, transform) for each copy. Must be called before Run().
    Entity AddInstanceMesh(const std::string&                         name,
                            std::shared_ptr<CoordTable>                coords,
                            std::shared_ptr<ScalarTable>               scalars,
                            std::shared_ptr<ColorTable>                colorTable,
                            std::vector<std::shared_ptr<PrimitiveSet>> primSets);

    // Create an instance entity that renders templateEntity's mesh at transform.
    // templateEntity must be created via AddInstanceMesh(). Must be called before Run().
    Entity AddInstance(Entity templateEntity,
                       const glm::mat4& transform = glm::mat4{1.f});

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

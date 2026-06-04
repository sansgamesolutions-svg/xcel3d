#pragma once
#include <vulkan/vulkan.h>
#include <memory>

namespace xcel {

// Owns one logical Vulkan device and its per-queue-type resources.
// Created and ranked by VulkanContext; callers access via VulkanContext::GetDevice().
//
// Queue slots are sparse: only queue types the physical device actually supports
// are created. Check SupportsXxx() before calling the corresponding accessors.
class DeviceContext {
public:
    enum class QueueType { Graphics, Compute, Transfer };

    DeviceContext();
    ~DeviceContext();

    DeviceContext(const DeviceContext&)            = delete;
    DeviceContext& operator=(const DeviceContext&) = delete;

    void Create(VkPhysicalDevice physDev, VkSurfaceKHR surface, bool enableValidation);
    void Destroy();

    // ── Capability queries ────────────────────────────────────────────────────
    bool SupportsGraphics()  const;
    bool SupportsCompute()   const;
    bool SupportsPresent()   const;
    bool SupportsTransfer()  const;

    // ── Core handles ──────────────────────────────────────────────────────────
    VkPhysicalDevice PhysicalDevice() const;
    VkDevice         Device()         const;

    // ── Queues (throws std::bad_optional_access if queue type not supported) ─
    VkQueue GraphicsQueue()  const;
    VkQueue ComputeQueue()   const;
    VkQueue PresentQueue()   const;
    VkQueue TransferQueue()  const;

    // ── Command pools (graphics / compute / transfer only; present has none) ─
    VkCommandPool GraphicsCommandPool()  const;
    VkCommandPool ComputeCommandPool()   const;
    VkCommandPool TransferCommandPool()  const;

    // ── Queue family indices ──────────────────────────────────────────────────
    uint32_t GraphicsFamily() const;
    uint32_t ComputeFamily()  const;
    uint32_t PresentFamily()  const;
    uint32_t TransferFamily() const;

    // ── Device metadata ───────────────────────────────────────────────────────
    const VkPhysicalDeviceProperties& Properties() const;
    // Ranking score: discrete=1000, integrated=100, other=1
    int Score() const;

    // ── One-shot command helpers ──────────────────────────────────────────────
    VkCommandBuffer BeginSingleTimeCommands(QueueType type = QueueType::Graphics);
    void            EndSingleTimeCommands(VkCommandBuffer cmd,
                                          QueueType type = QueueType::Graphics);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

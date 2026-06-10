#pragma once
#include <vulkan/vulkan.h>
#include <optional>

namespace xcel {

class DeviceContext
{
public:
    enum class QueueType { Graphics, Compute, Transfer };

    DeviceContext()  = default;
    ~DeviceContext() = default;

    DeviceContext(const DeviceContext&)            = delete;
    DeviceContext& operator=(const DeviceContext&) = delete;

    void Create(VkPhysicalDevice physDev, VkSurfaceKHR surface, bool enableValidation);
    void Destroy();

    bool SupportsGraphics()  const;
    bool SupportsCompute()   const;
    bool SupportsPresent()   const;
    bool SupportsTransfer()  const;

    VkPhysicalDevice PhysicalDevice() const;
    VkDevice         Device()         const;

    VkQueue GraphicsQueue()  const;
    VkQueue ComputeQueue()   const;
    VkQueue PresentQueue()   const;
    VkQueue TransferQueue()  const;

    VkCommandPool GraphicsCommandPool()  const;
    VkCommandPool ComputeCommandPool()   const;
    VkCommandPool TransferCommandPool()  const;

    uint32_t GraphicsFamily() const;
    uint32_t ComputeFamily()  const;
    uint32_t PresentFamily()  const;
    uint32_t TransferFamily() const;

    const VkPhysicalDeviceProperties& Properties() const;
    int Score() const;

    VkCommandBuffer BeginSingleTimeCommands(QueueType type = QueueType::Graphics);
    void            EndSingleTimeCommands(VkCommandBuffer cmd, QueueType type = QueueType::Graphics);

private:
    struct QueueSlot {
        uint32_t      familyIndex = UINT32_MAX;
        VkQueue       queue       = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
    };

    VkPhysicalDevice           m_physicalDevice    = VK_NULL_HANDLE;
    VkDevice                   m_device            = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_properties        = {};
    bool                       m_validationEnabled = false;

    std::optional<QueueSlot> m_graphics;
    std::optional<QueueSlot> m_compute;
    std::optional<QueueSlot> m_present;
    std::optional<QueueSlot> m_transfer;
};

} // namespace xcel

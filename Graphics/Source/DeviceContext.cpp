#include "Graphics/DeviceContext.h"
#include <stdexcept>
#include <optional>
#include <vector>
#include <set>

namespace xcel {

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// â”€â”€ Impl â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct DeviceContext::Impl {
    VkPhysicalDevice           physicalDevice    = VK_NULL_HANDLE;
    VkDevice                   device            = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties        = {};
    bool                       validationEnabled = false;

    struct QueueSlot {
        uint32_t      familyIndex = UINT32_MAX;
        VkQueue       queue       = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE; // VK_NULL_HANDLE for present slot
    };

    std::optional<QueueSlot> graphics;
    std::optional<QueueSlot> compute;
    std::optional<QueueSlot> present;  // commandPool is always VK_NULL_HANDLE
    std::optional<QueueSlot> transfer;
};

// â”€â”€ Internal queue-family search â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct QueueSearch {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> present;
    std::optional<uint32_t> transfer; // dedicated (no graphics/compute bits)
};

static QueueSearch FindQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    QueueSearch result;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const auto& f = families[i];

        if (!result.graphics && (f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            result.graphics = i;

        if (!result.compute && (f.queueFlags & VK_QUEUE_COMPUTE_BIT))
            result.compute = i;

        // Prefer a family that only has transfer (no graphics/compute contention)
        if (!result.transfer
                && (f.queueFlags & VK_QUEUE_TRANSFER_BIT)
                && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                && !(f.queueFlags & VK_QUEUE_COMPUTE_BIT))
            result.transfer = i;

        if (surface != VK_NULL_HANDLE && !result.present) {
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &supported);
            if (supported) result.present = i;
        }
    }

    // No dedicated transfer family â†’ fall back to graphics
    if (!result.transfer && result.graphics)
        result.transfer = result.graphics;

    return result;
}

// â”€â”€ Construction / destruction â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

DeviceContext::DeviceContext()
    : m_impl(std::make_unique<Impl>()) {}

DeviceContext::~DeviceContext() { Destroy(); }

// â”€â”€ Create / Destroy â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void DeviceContext::Create(VkPhysicalDevice physDev, VkSurfaceKHR surface, bool enableValidation)
{
    m_impl->physicalDevice    = physDev;
    m_impl->validationEnabled = enableValidation;
    vkGetPhysicalDeviceProperties(physDev, &m_impl->properties);

    auto families = FindQueueFamilies(physDev, surface);

    // Build unique set of family indices to satisfy Vulkan's "one create-info per family" rule
    std::set<uint32_t> uniqueFamilies;
    if (families.graphics) uniqueFamilies.insert(*families.graphics);
    if (families.compute)  uniqueFamilies.insert(*families.compute);
    if (families.present)  uniqueFamilies.insert(*families.present);
    if (families.transfer) uniqueFamilies.insert(*families.transfer);

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueFamilies.size());
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = (uint32_t)queueInfos.size();
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.enabledExtensionCount   = (uint32_t)kDeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
    createInfo.pEnabledFeatures        = &features;
    if (enableValidation) {
        createInfo.enabledLayerCount   = (uint32_t)kValidationLayers.size();
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    if (vkCreateDevice(physDev, &createInfo, nullptr, &m_impl->device) != VK_SUCCESS)
        throw std::runtime_error("DeviceContext: vkCreateDevice failed");

    // Each supported queue type gets its own slot (and its own command pool)
    auto makeSlot = [this](uint32_t family) -> Impl::QueueSlot {
        Impl::QueueSlot slot;
        slot.familyIndex = family;
        vkGetDeviceQueue(m_impl->device, family, 0, &slot.queue);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = family;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(m_impl->device, &poolInfo, nullptr, &slot.commandPool) != VK_SUCCESS)
            throw std::runtime_error("DeviceContext: vkCreateCommandPool failed");
        return slot;
    };

    if (families.graphics) m_impl->graphics = makeSlot(*families.graphics);
    if (families.compute)  m_impl->compute  = makeSlot(*families.compute);
    if (families.transfer) m_impl->transfer = makeSlot(*families.transfer);

    // Present slot: queue handle only â€” present operations need no command pool
    if (families.present) {
        Impl::QueueSlot slot;
        slot.familyIndex = *families.present;
        vkGetDeviceQueue(m_impl->device, *families.present, 0, &slot.queue);
        m_impl->present = slot;
    }
}

void DeviceContext::Destroy()
{
    if (m_impl->device == VK_NULL_HANDLE) return;

    auto destroyPool = [this](std::optional<Impl::QueueSlot>& slot) {
        if (slot && slot->commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_impl->device, slot->commandPool, nullptr);
            slot->commandPool = VK_NULL_HANDLE;
        }
    };
    destroyPool(m_impl->graphics);
    destroyPool(m_impl->compute);
    destroyPool(m_impl->transfer);
    // present slot has no pool

    vkDestroyDevice(m_impl->device, nullptr);
    m_impl->device = VK_NULL_HANDLE;
}

// â”€â”€ Capability queries â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool DeviceContext::SupportsGraphics() const { return m_impl->graphics.has_value(); }
bool DeviceContext::SupportsCompute()  const { return m_impl->compute.has_value();  }
bool DeviceContext::SupportsPresent()  const { return m_impl->present.has_value();  }
bool DeviceContext::SupportsTransfer() const { return m_impl->transfer.has_value(); }

// â”€â”€ Core handles â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

VkPhysicalDevice DeviceContext::PhysicalDevice() const { return m_impl->physicalDevice; }
VkDevice         DeviceContext::Device()         const { return m_impl->device; }

// â”€â”€ Queue accessors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

VkQueue DeviceContext::GraphicsQueue()  const { return m_impl->graphics.value().queue; }
VkQueue DeviceContext::ComputeQueue()   const { return m_impl->compute.value().queue;  }
VkQueue DeviceContext::PresentQueue()   const { return m_impl->present.value().queue;  }
VkQueue DeviceContext::TransferQueue()  const { return m_impl->transfer.value().queue; }

// â”€â”€ Command pool accessors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

VkCommandPool DeviceContext::GraphicsCommandPool() const { return m_impl->graphics.value().commandPool; }
VkCommandPool DeviceContext::ComputeCommandPool()  const { return m_impl->compute.value().commandPool;  }
VkCommandPool DeviceContext::TransferCommandPool() const { return m_impl->transfer.value().commandPool; }

// â”€â”€ Family index accessors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

uint32_t DeviceContext::GraphicsFamily() const { return m_impl->graphics.value().familyIndex; }
uint32_t DeviceContext::ComputeFamily()  const { return m_impl->compute.value().familyIndex;  }
uint32_t DeviceContext::PresentFamily()  const { return m_impl->present.value().familyIndex;  }
uint32_t DeviceContext::TransferFamily() const { return m_impl->transfer.value().familyIndex; }

// â”€â”€ Device metadata â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

const VkPhysicalDeviceProperties& DeviceContext::Properties() const { return m_impl->properties; }

int DeviceContext::Score() const
{
    switch (m_impl->properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 1000;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return  100;
    default:                                     return    1;
    }
}

// â”€â”€ One-shot command helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

VkCommandBuffer DeviceContext::BeginSingleTimeCommands(QueueType type)
{
    VkCommandPool pool = [&] {
        switch (type) {
        case QueueType::Graphics: return GraphicsCommandPool();
        case QueueType::Compute:  return ComputeCommandPool();
        case QueueType::Transfer: return TransferCommandPool();
        }
        return GraphicsCommandPool();
    }();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_impl->device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void DeviceContext::EndSingleTimeCommands(VkCommandBuffer cmd, QueueType type)
{
    VkCommandPool pool  = VK_NULL_HANDLE;
    VkQueue       queue = VK_NULL_HANDLE;
    switch (type) {
    case QueueType::Graphics: pool = GraphicsCommandPool(); queue = GraphicsQueue(); break;
    case QueueType::Compute:  pool = ComputeCommandPool();  queue = ComputeQueue();  break;
    case QueueType::Transfer: pool = TransferCommandPool(); queue = TransferQueue(); break;
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(m_impl->device, pool, 1, &cmd);
}

} // namespace xcel

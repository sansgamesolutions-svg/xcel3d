#pragma once
#include <vulkan/vulkan.h>

namespace xcel {

// Physical-device capabilities relevant to rendering quality choices.
// Populated once by QueryHardwareCaps() after device selection; never modified.
struct HardwareCaps
{
    VkSampleCountFlagBits maxSamples        = VK_SAMPLE_COUNT_1_BIT;
    bool                  samplerAnisotropy = false;
    bool                  fillModeNonSolid  = false;
    bool                  multiDrawIndirect = false;
};

[[nodiscard]] HardwareCaps QueryHardwareCaps(VkPhysicalDevice physDev) noexcept;

} // namespace xcel

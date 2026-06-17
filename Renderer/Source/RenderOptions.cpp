#include "Renderer/RenderOptions.h"
#include "Renderer/HardwareCaps.h"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace xcel {

HardwareCaps QueryHardwareCaps(VkPhysicalDevice physDev) noexcept
{
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceFeatures   feats{};
    vkGetPhysicalDeviceProperties(physDev, &props);
    vkGetPhysicalDeviceFeatures(physDev, &feats);

    HardwareCaps caps;

    // The effective MSAA limit is the intersection of what the GPU can do for
    // both color and depth attachments simultaneously.
    const VkSampleCountFlags counts =
        props.limits.framebufferColorSampleCounts &
        props.limits.framebufferDepthSampleCounts;

    for (VkSampleCountFlagBits s : {
            VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_2_BIT})
    {
        if (counts & s)
        {
            caps.maxSamples = s;
            break;
        }
    }

    caps.samplerAnisotropy = feats.samplerAnisotropy == VK_TRUE;
    caps.fillModeNonSolid  = feats.fillModeNonSolid  == VK_TRUE;
    caps.multiDrawIndirect = feats.multiDrawIndirect  == VK_TRUE;

    return caps;
}

EffectiveCaps ComputeEffectiveCaps(
    const HardwareCaps&        hw,
    const GlobalRenderOptions& opts) noexcept
{
    EffectiveCaps ec;

    // Clamp the requested sample count to what the GPU actually supports.
    ec.msaaSamples = static_cast<VkSampleCountFlagBits>(
        std::min(static_cast<uint32_t>(hw.maxSamples),
                 static_cast<uint32_t>(opts.maxMsaaSamples)));

    ec.anisotropy = hw.samplerAnisotropy && opts.allowAnisotropy;

    return ec;
}

} // namespace xcel

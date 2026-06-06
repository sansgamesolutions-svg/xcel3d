#include "Graphics/BatchDrawable.h"
#include "Graphics/GpuBuffer.h"
#include "Graphics/MeshTessellator.h"

namespace xcel {

struct BatchDrawable::Impl {
    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;
    uint32_t  indexCount = 0;
};

BatchDrawable::BatchDrawable()
    : m_impl(std::make_unique<Impl>()) {}

BatchDrawable::~BatchDrawable() = default;

void BatchDrawable::Rebuild(
    DeviceContext&                         dev,
    std::span<const MeshTessellationInput> inputs,
    ThreadPool*                            pool)
{
    m_impl->vertexBuffer.Destroy(dev.Device());
    m_impl->indexBuffer.Destroy(dev.Device());
    m_impl->indexCount = 0;

    TessellatedMesh combined = TessellateAndMerge(inputs, pool);
    if (combined.vertices.empty()) return;

    m_impl->indexCount = static_cast<uint32_t>(combined.indices.size());

    VkDeviceSize vbSize = combined.vertices.size() * sizeof(MeshVertex);
    VkDeviceSize ibSize = combined.indices.size()  * sizeof(uint32_t);

    m_impl->vertexBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_impl->vertexBuffer.UploadViaStaging(dev, combined.vertices.data(), vbSize);

    m_impl->indexBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_impl->indexBuffer.UploadViaStaging(dev, combined.indices.data(), ibSize);
}

void BatchDrawable::Destroy(VkDevice device)
{
    m_impl->vertexBuffer.Destroy(device);
    m_impl->indexBuffer.Destroy(device);
    m_impl->indexCount = 0;
}

const GpuBuffer& BatchDrawable::VertexBuffer() const { return m_impl->vertexBuffer; }
const GpuBuffer& BatchDrawable::IndexBuffer()  const { return m_impl->indexBuffer; }
uint32_t         BatchDrawable::IndexCount()   const { return m_impl->indexCount; }

} // namespace xcel

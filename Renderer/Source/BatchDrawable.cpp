#include "Renderer/BatchDrawable.h"
#include "Renderer/GpuBuffer.h"
#include "Kernel/MeshTessellator.h"

namespace xcel {

void BatchDrawable::Rebuild(
    DeviceContext&                         dev,
    std::span<const MeshTessellationInput> inputs,
    ThreadPool*                            pool)
{
    m_vertexBuffer.Destroy(dev.Device());
    m_indexBuffer.Destroy(dev.Device());
    m_indexCount = 0;

    TessellatedMesh combined = TessellateAndMerge(inputs, pool);
    if (combined.vertices.empty()) return;

    m_indexCount = static_cast<uint32_t>(combined.indices.size());

    VkDeviceSize vbSize = combined.vertices.size() * sizeof(MeshVertex);
    VkDeviceSize ibSize = combined.indices.size()  * sizeof(uint32_t);

    m_vertexBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_vertexBuffer.UploadViaStaging(dev, combined.vertices.data(), vbSize);

    m_indexBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_indexBuffer.UploadViaStaging(dev, combined.indices.data(), ibSize);
}

void BatchDrawable::Destroy(VkDevice device)
{
    m_vertexBuffer.Destroy(device);
    m_indexBuffer.Destroy(device);
    m_indexCount = 0;
}

const GpuBuffer& BatchDrawable::VertexBuffer() const { return m_vertexBuffer; }
const GpuBuffer& BatchDrawable::IndexBuffer()  const { return m_indexBuffer; }
uint32_t         BatchDrawable::IndexCount()   const { return m_indexCount; }

} // namespace xcel

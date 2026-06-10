#include "Renderer/BatchDrawable.h"
#include "Renderer/GpuBuffer.h"
#include "Kernel/MeshTessellator.h"

namespace xcel {

void BatchDrawable::Rebuild(
    DeviceContext&                         dev,
    std::span<const MeshTessellationInput> inputs,
    ThreadPool*                            pool)
{
    TessellatedMesh combined = TessellateAndMerge(inputs, pool);
    if (combined.vertices.empty()) {
        m_vertexBuffer.Destroy(dev.Device());
        m_indexBuffer.Destroy(dev.Device());
        m_indexCount = 0;
        return;
    }

    VkDeviceSize vbSize = combined.vertices.size() * sizeof(MeshVertex);
    VkDeviceSize ibSize = combined.indices.size()  * sizeof(uint32_t);

    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;
    try {
        vertexBuffer.Create(
            dev.Device(), dev.PhysicalDevice(), vbSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        indexBuffer.Create(
            dev.Device(), dev.PhysicalDevice(), ibSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        const GpuBufferUpload uploads[] = {
            {&vertexBuffer, combined.vertices.data(), vbSize},
            {&indexBuffer, combined.indices.data(), ibSize},
        };
        UploadGpuBuffersViaStaging(dev, uploads);
    } catch (...) {
        vertexBuffer.Destroy(dev.Device());
        indexBuffer.Destroy(dev.Device());
        throw;
    }

    m_vertexBuffer.Destroy(dev.Device());
    m_indexBuffer.Destroy(dev.Device());
    m_vertexBuffer = std::move(vertexBuffer);
    m_indexBuffer  = std::move(indexBuffer);
    m_indexCount   = static_cast<uint32_t>(combined.indices.size());
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

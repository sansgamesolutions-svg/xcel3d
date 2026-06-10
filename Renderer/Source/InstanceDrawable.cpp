#include "Renderer/InstanceDrawable.h"
#include "Renderer/Component.h"
#include "Kernel/MeshTessellator.h"
#include "Kernel/TessellationStrategy.h"
#include "Renderer/GpuBuffer.h"
#include "Common/ThreadPool.h"
#include <bit>
#include <limits>
#include <stdexcept>
#include <vector>

namespace xcel {

InstanceDrawable::InstanceDrawable(flecs::entity meshEntity)
    {
    m_entity = meshEntity;
}

void InstanceDrawable::Build(DeviceContext& dev, const ColorTable&, ThreadPool* pool)
{
    const auto* cc  = m_entity.get<CoordTableComponent>();
    const auto* sc  = m_entity.get<ScalarTableComponent>();
    const auto* co  = m_entity.get<ColorTableComponent>();
    const auto* psc = m_entity.get<PrimitiveSetsComponent>();
    const auto* stc = m_entity.get<TessellationStrategyComponent>();

    if (!cc || !sc || !co || !psc)
        throw std::runtime_error("InstanceDrawable::Build: missing ECS components");

    std::vector<MeshTessellationInput> inputs;
    inputs.reserve(psc->sets.size());
    size_t scalarOffset = 0;
    for (const auto& ps : psc->sets) {
        inputs.push_back({
            ps.get(),
            cc->coords.get(),
            sc->scalars.get(),
            co->colorTable.get(),
            stc ? stc->strategy.get() : nullptr,
            scalarOffset});
        scalarOffset += ps->ElementCount();
    }
    TessellatedMesh combined = TessellateAndMerge(inputs, pool);

    if (combined.vertices.empty())
        throw std::runtime_error("InstanceDrawable::Build: no geometry produced");

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

void InstanceDrawable::UpdateInstances(DeviceContext& dev,
                                        std::span<const glm::mat4> transforms)
{
    m_instanceCount = 0;
    if (transforms.empty()) return;
    if (transforms.size() > std::numeric_limits<uint32_t>::max())
        throw std::overflow_error("InstanceDrawable: too many instances");

    if (transforms.size() > m_instanceCapacity) {
        const size_t newCapacity = std::bit_ceil(transforms.size());
        m_instanceBuffer.Destroy(dev.Device());
        m_instanceCapacity = 0;
        const VkDeviceSize capacityBytes =
            newCapacity * sizeof(glm::mat4);
        m_instanceBuffer.Create(
            dev.Device(), dev.PhysicalDevice(), capacityBytes,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_instanceCapacity = newCapacity;
    }

    const VkDeviceSize size = transforms.size() * sizeof(glm::mat4);
    m_instanceBuffer.WriteHostVisible(transforms.data(), size);
    m_instanceCount = static_cast<uint32_t>(transforms.size());
}

void InstanceDrawable::Destroy(VkDevice device)
{
    m_vertexBuffer.Destroy(device);
    m_indexBuffer.Destroy(device);
    m_instanceBuffer.Destroy(device);
    m_indexCount    = 0;
    m_instanceCount = 0;
    m_instanceCapacity = 0;
}

const GpuBuffer& InstanceDrawable::VertexBuffer()   const { return m_vertexBuffer; }
const GpuBuffer& InstanceDrawable::IndexBuffer()    const { return m_indexBuffer; }
uint32_t         InstanceDrawable::IndexCount()     const { return m_indexCount; }
const GpuBuffer* InstanceDrawable::InstanceBuffer() const { return &m_instanceBuffer; }
uint32_t         InstanceDrawable::InstanceCount()  const { return m_instanceCount; }

} // namespace xcel

#include "Renderer/InstanceDrawable.h"
#include "Renderer/Component.h"
#include "Kernel/MeshTessellator.h"
#include "Kernel/TessellationStrategy.h"
#include "Renderer/GpuBuffer.h"
#include "Common/ThreadPool.h"
#include <stdexcept>

namespace xcel {

struct InstanceDrawable::Impl {
    flecs::entity entity;
    GpuBuffer     vertexBuffer;
    GpuBuffer     indexBuffer;
    GpuBuffer     instanceBuffer;
    uint32_t      indexCount    = 0;
    uint32_t      instanceCount = 0;
};

InstanceDrawable::InstanceDrawable(flecs::entity meshEntity)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->entity = meshEntity;
}

InstanceDrawable::~InstanceDrawable() = default;

void InstanceDrawable::Build(DeviceContext& dev, const ColorTable&, ThreadPool* pool)
{
    const auto* cc  = m_impl->entity.get<CoordTableComponent>();
    const auto* sc  = m_impl->entity.get<ScalarTableComponent>();
    const auto* co  = m_impl->entity.get<ColorTableComponent>();
    const auto* psc = m_impl->entity.get<PrimitiveSetsComponent>();
    const auto* stc = m_impl->entity.get<TessellationStrategyComponent>();

    if (!cc || !sc || !co || !psc)
        throw std::runtime_error("InstanceDrawable::Build: missing ECS components");

    TessellatedMesh combined;
    for (const auto& ps : psc->sets) {
        MeshTessellationInput inp{ps.get(),
                                   cc->coords.get(),
                                   sc->scalars.get(),
                                   co->colorTable.get(),
                                   stc ? stc->strategy.get() : nullptr};
        auto     part = TessellateInput(inp, pool);
        uint32_t base = static_cast<uint32_t>(combined.vertices.size());
        for (auto idx : part.indices)
            combined.indices.push_back(base + idx);
        combined.vertices.insert(combined.vertices.end(),
                                  part.vertices.begin(), part.vertices.end());
    }

    if (combined.vertices.empty())
        throw std::runtime_error("InstanceDrawable::Build: no geometry produced");

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

void InstanceDrawable::UpdateInstances(DeviceContext& dev,
                                        std::span<const glm::mat4> transforms)
{
    m_impl->instanceBuffer.Destroy(dev.Device());
    m_impl->instanceCount = 0;
    if (transforms.empty()) return;

    VkDeviceSize size = transforms.size() * sizeof(glm::mat4);
    m_impl->instanceBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_impl->instanceBuffer.UploadViaStaging(dev, transforms.data(), size);
    m_impl->instanceCount = static_cast<uint32_t>(transforms.size());
}

void InstanceDrawable::Destroy(VkDevice device)
{
    m_impl->vertexBuffer.Destroy(device);
    m_impl->indexBuffer.Destroy(device);
    m_impl->instanceBuffer.Destroy(device);
    m_impl->indexCount    = 0;
    m_impl->instanceCount = 0;
}

const GpuBuffer& InstanceDrawable::VertexBuffer()   const { return m_impl->vertexBuffer; }
const GpuBuffer& InstanceDrawable::IndexBuffer()    const { return m_impl->indexBuffer; }
uint32_t         InstanceDrawable::IndexCount()     const { return m_impl->indexCount; }
const GpuBuffer* InstanceDrawable::InstanceBuffer() const { return &m_impl->instanceBuffer; }
uint32_t         InstanceDrawable::InstanceCount()  const { return m_impl->instanceCount; }

} // namespace xcel

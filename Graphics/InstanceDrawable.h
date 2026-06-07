#pragma once
#include "Graphics/Drawable.h"
#include "Graphics/GpuBuffer.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <span>

namespace xcel {

class InstanceDrawable : public Drawable
{
public:
    explicit InstanceDrawable(flecs::entity meshEntity);
    ~InstanceDrawable() = default;

    InstanceDrawable(const InstanceDrawable&)            = delete;
    InstanceDrawable& operator=(const InstanceDrawable&) = delete;

    void Build(DeviceContext& dev, const ColorTable& /*colormap*/,
               ThreadPool* pool = nullptr) override;

    void UpdateInstances(DeviceContext& dev, std::span<const glm::mat4> transforms);

    void             Destroy(VkDevice device) override;
    const GpuBuffer& VertexBuffer()   const override;
    const GpuBuffer& IndexBuffer()    const override;
    uint32_t         IndexCount()     const override;
    const GpuBuffer* InstanceBuffer() const override;
    uint32_t         InstanceCount()  const override;

private:
    flecs::entity m_entity;
    GpuBuffer     m_vertexBuffer;
    GpuBuffer     m_indexBuffer;
    GpuBuffer     m_instanceBuffer;
    uint32_t      m_indexCount    = 0;
    uint32_t      m_instanceCount = 0;
};

} // namespace xcel

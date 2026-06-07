#pragma once
#include "Graphics/Drawable.h"
#include "Graphics/GpuBuffer.h"
#include "Graphics/TessellationStrategy.h"
#include <span>

namespace xcel {

class ThreadPool;

class BatchDrawable : public Drawable
{
public:
    BatchDrawable()  = default;
    ~BatchDrawable() = default;

    BatchDrawable(const BatchDrawable&)            = delete;
    BatchDrawable& operator=(const BatchDrawable&) = delete;

    void Rebuild(DeviceContext&                         dev,
                 std::span<const MeshTessellationInput> inputs,
                 ThreadPool*                            pool = nullptr);

    void             Build(DeviceContext&, const ColorTable&, ThreadPool*) override {}
    void             Destroy(VkDevice device) override;
    const GpuBuffer& VertexBuffer() const override;
    const GpuBuffer& IndexBuffer()  const override;
    uint32_t         IndexCount()   const override;

private:
    GpuBuffer m_vertexBuffer;
    GpuBuffer m_indexBuffer;
    uint32_t  m_indexCount = 0;
};

} // namespace xcel

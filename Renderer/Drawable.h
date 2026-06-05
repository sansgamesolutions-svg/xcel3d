#pragma once
#include "Renderer/DeviceContext.h"
#include "Kernel/ColorTable.h"
#include <cstdint>
#include <memory>

namespace xcel {

class GpuBuffer;
class ThreadPool;

// Abstract interface for any GPU-resident object the renderer can draw.
//
// Build()   — tessellate / upload data to device-local GPU buffers.
//             Must be called once before Run(), after all CPU data is set.
// Destroy() — release GPU buffers; safe to call even if Build() was skipped.
//
// CommandRecorder reads VertexBuffer/IndexBuffer/IndexCount each frame to
// emit vkCmdDrawIndexed without knowing the concrete drawable type.
class Drawable {
public:
    virtual ~Drawable() = default;

    virtual void Build(DeviceContext& dev,
                       const ColorTable& colormap,
                       ThreadPool* pool = nullptr) = 0;

    virtual void Destroy(VkDevice device) = 0;

    virtual const GpuBuffer& VertexBuffer() const = 0;
    virtual const GpuBuffer& IndexBuffer()  const = 0;
    virtual uint32_t         IndexCount()   const = 0;

protected:
    Drawable() = default;
    Drawable(const Drawable&)            = delete;
    Drawable& operator=(const Drawable&) = delete;
};

} // namespace xcel

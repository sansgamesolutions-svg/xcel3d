#pragma once
#include "Graphics/Drawable.h"
#include "Graphics/TessellationStrategy.h"
#include <memory>
#include <span>

namespace xcel {

class ThreadPool;

// Drawable that owns merged GPU vertex/index buffers for one page.
// A page holds the tessellated geometry of all visible mesh entities
// that share the same PrimitiveType and fit within the page's byte budget.
//
// Rebuild() re-tessellates and re-uploads whenever the visible mesh set changes.
// Build() is a no-op; GPU data is managed entirely via Rebuild().
class BatchDrawable : public Drawable {
public:
    BatchDrawable();
    ~BatchDrawable() override;

    // Re-tessellate all inputs and upload to device-local GPU buffers.
    // Safe to call multiple times (destroys old buffers before re-allocating).
    // Inputs should already be filtered to the correct PrimitiveType by the caller.
    void Rebuild(DeviceContext&                         dev,
                 std::span<const MeshTessellationInput> inputs,
                 ThreadPool*                            pool = nullptr);

    // Drawable interface
    void             Build(DeviceContext&, const ColorTable&, ThreadPool*) override {}
    void             Destroy(VkDevice device) override;
    const GpuBuffer& VertexBuffer() const override;
    const GpuBuffer& IndexBuffer()  const override;
    uint32_t         IndexCount()   const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

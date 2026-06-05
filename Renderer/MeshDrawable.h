#pragma once
#include "Renderer/Drawable.h"
#include "Kernel/Mesh.h"
#include <memory>

namespace xcel {

// A Mesh tessellated and uploaded to GPU-resident vertex/index buffers.
//
// Lifecycle:
//   1. Populate the Mesh base (SetCoords, SetScalars, AddPrimitiveSet).
//   2. Call Build() once — tessellates all PrimitiveSets in parallel (if pool
//      is provided and element count > 1024) and uploads to device-local VRAM.
//   3. Pass to WindowContext::AddMesh(); the render loop calls VertexBuffer()
//      / IndexBuffer() / IndexCount() every frame via the Drawable interface.
//   4. Destroy() is called automatically by WindowContext on shutdown.
//
// The CPU-side Mesh data (CoordTable, ScalarTable, PrimitiveSets) can be
// released after Build() completes — it is no longer referenced.
class MeshDrawable : public Drawable, public Mesh {
public:
    MeshDrawable();
    ~MeshDrawable() override;

    void Build(DeviceContext& dev,
               const ColorTable& colormap,
               ThreadPool* pool = nullptr) override;

    void Destroy(VkDevice device) override;

    const GpuBuffer& VertexBuffer() const override;
    const GpuBuffer& IndexBuffer()  const override;
    uint32_t         IndexCount()   const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

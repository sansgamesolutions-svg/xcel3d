#pragma once
#include "Graphics/DeviceContext.h"
#include "Graphics/Mesh.h"
#include <memory>

namespace xcel {

class ColorTable;
class GpuBuffer;
class ThreadPool;

// A Mesh that has been tessellated and uploaded to GPU-resident vertex/index
// buffers.  This is the concrete rendering unit: once Build() returns, the mesh
// is ready to be drawn by CommandRecorder every frame.
//
// Build() is the central step of the VTK → GPU pipeline:
//
//   1. CPU data model (CoordTable + ScalarTable + HexPrimitiveSet)
//        — populated by the caller before Build() is invoked.
//
//   2. Tessellation (TessellateHexMesh / TessellateHexMeshRange)
//        — converts each VTK hex element to 6 quad faces (12 triangles, 24 verts).
//        — bakes per-element color into vertex attributes.
//        — can run in parallel across worker threads via ThreadPool.
//
//   3. GPU upload (GpuBuffer::UploadViaStaging)
//        — creates device-local VkBuffer objects for vertices and indices.
//        — copies data through a transient host-visible staging buffer.
//        — after this step the CPU-side TessellatedMesh is discarded.
//
//   4. Render-loop use
//        — DrawFrame collects (VertexBuffer, IndexBuffer, IndexCount) from every
//          registered StaticMesh and emits one vkCmdDrawIndexed per mesh.
//
// Lifetime rules:
//   • Call Build() exactly once, *after* SetCoords/SetScalars/AddPrimitiveSet
//     and *before* Application::Run().
//   • Call Destroy() before the VkDevice is destroyed (Application::Cleanup
//     handles this via MeshManager::Clear).
//   • The Mesh base class (CoordTable, ScalarTable, PrimitiveSets) is no longer
//     needed after Build() completes; callers may release those shared_ptrs.
class StaticMesh : public Mesh {
public:
    StaticMesh();
    ~StaticMesh() override;

    // Tessellate all HexPrimitiveSets and upload the result to GPU.
    // Passing a non-null pool parallelises tessellation across worker threads
    // for element counts above the internal kParallelThreshold (1 024 elements).
    // The GPU upload step is always sequential (Vulkan queue submission).
    void Build(DeviceContext& dev, const ColorTable& colormap, ThreadPool* pool = nullptr);

    // Release GPU buffers.  Safe to call even if Build() was never called.
    void Destroy(VkDevice device);

    const GpuBuffer& VertexBuffer() const;
    const GpuBuffer& IndexBuffer()  const;
    uint32_t         IndexCount()   const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

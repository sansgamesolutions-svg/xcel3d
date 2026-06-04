#include "Graphics/StaticMesh.h"
#include "Graphics/MeshTessellator.h"
#include "Graphics/ColorTable.h"
#include "Graphics/GpuBuffer.h"
#include "Common/ThreadPool.h"
#include <stdexcept>
#include <algorithm>
#include <future>
#include <vector>

namespace xcel {

// Below this element count tessellation runs serially on the calling thread.
// Above it, elements are split across pool->ThreadCount() parallel tasks.
// The threshold avoids the thread-dispatch overhead for small demo meshes.
static constexpr size_t kParallelThreshold = 1'024;

struct StaticMesh::Impl {
    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;
    uint32_t  indexCount = 0;
};

StaticMesh::StaticMesh()
    : m_impl(std::make_unique<Impl>()) {}

StaticMesh::~StaticMesh() = default;

// Build() is the complete VTK → GPU pipeline for this mesh.
//
// STEP 1 — Validate prerequisites.
//   CoordTable and ScalarTable must have been set by the caller (importer /
//   demo builder).  The HexPrimitiveSet elements index into CoordTable, and
//   ScalarTable values drive per-element colormap lookup.
//
// STEP 2 — Tessellate.
//   For each HexPrimitiveSet, every VTK hex element expands to 24 vertices and
//   12 triangles.  If a ThreadPool is provided and the element count exceeds
//   kParallelThreshold, the element range is split into T equal chunks (one
//   per hardware thread) and dispatched to the pool as independent futures.
//   Each chunk calls TessellateHexMeshRange with the *global* scalar min/max
//   (pre-computed once) so the color scale stays consistent across all chunks.
//   After all futures resolve, partial results are concatenated in order —
//   index values in later chunks are offset by the vertex count of all
//   earlier chunks so the combined index buffer is contiguous.
//
// STEP 3 — Merge across multiple PrimitiveSets.
//   A Mesh may contain more than one HexPrimitiveSet (e.g., separate sets for
//   different materials or parts).  After tessellating each set individually,
//   the results are merged into a single TessellatedMesh with a global index
//   offset applied so all draw calls can share one vertex buffer.
//
// STEP 4 — GPU upload via staging.
//   Device-local (GPU-only) VkBuffers are created for vertices and indices.
//   Data is copied through a transient host-visible staging buffer:
//     CPU array → (memcpy) → host-visible staging VkBuffer
//                         → (vkCmdCopyBuffer, one-shot command) → device-local VkBuffer
//   The staging buffer is destroyed immediately after the copy completes.
//   After this step the CPU-side TessellatedMesh goes out of scope and is freed.
void StaticMesh::Build(DeviceContext& dev, const ColorTable& colormap, ThreadPool* pool) {
    if (!Coords() || !Scalars())
        throw std::runtime_error("StaticMesh::Build: coords and scalars must be set");

    // STEP 2 & 3: tessellate and merge all HexPrimitiveSets.
    TessellatedMesh combined;

    for (const auto& ps : PrimitiveSets()) {
        size_t N = ps->ElementCount();

        TessellatedMesh part;

        if (pool && N > kParallelThreshold) {
            // Parallel path: split [0, N) into T chunks and tessellate concurrently.
            size_t T         = pool->ThreadCount();
            size_t chunkSize = (N + T - 1) / T;

            // Pre-compute global scalar range so every chunk uses the same color scale.
            float minS = Scalars()->MinValue();
            float maxS = Scalars()->MaxValue();

            std::vector<std::future<TessellatedMesh>> futures;
            futures.reserve(T);

            for (size_t t = 0; t < T; ++t) {
                size_t begin = t * chunkSize;
                size_t end   = std::min(begin + chunkSize, N);
                if (begin >= N) break;
                // Each lambda captures begin/end by value; all other inputs are
                // read-only and safe to capture by reference.
                futures.push_back(pool->Submit([&, begin, end] {
                    return TessellateRange(
                        *ps,
                        *Coords(), *Scalars(), colormap,
                        begin, end,
                        minS, maxS);
                }));
            }

            // Collect results in submission order to preserve element ordering in
            // the final buffer (important for deterministic visual output).
            for (auto& f : futures) {
                auto partial = f.get();
                // Offset partial indices by the current vertex count so they remain
                // valid after appending to the growing 'part' vertex array.
                auto base = static_cast<uint32_t>(part.vertices.size());
                for (auto idx : partial.indices)
                    part.indices.push_back(base + idx);
                part.vertices.insert(part.vertices.end(),
                                     partial.vertices.begin(), partial.vertices.end());
            }
        } else {
            // Serial path for small element counts or when no pool is provided.
            part = Tessellate(*ps, *Coords(), *Scalars(), colormap);
        }

        // Merge this PrimitiveSet's result into the combined buffer, offsetting
        // indices by the vertex count already accumulated from earlier sets.
        uint32_t baseVertex = static_cast<uint32_t>(combined.vertices.size());
        for (auto idx : part.indices)
            combined.indices.push_back(baseVertex + idx);
        combined.vertices.insert(combined.vertices.end(),
                                 part.vertices.begin(), part.vertices.end());
    }

    if (combined.vertices.empty())
        throw std::runtime_error("StaticMesh::Build: no geometry produced");

    m_impl->indexCount = static_cast<uint32_t>(combined.indices.size());

    // STEP 4: upload to GPU via staging buffers.
    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT → VRAM; fastest for repeated rendering.
    // TRANSFER_DST_BIT is required because the device-local buffer is the copy
    // destination in vkCmdCopyBuffer.
    VkDeviceSize vbSize = combined.vertices.size() * sizeof(MeshVertex);
    VkDeviceSize ibSize = combined.indices.size()  * sizeof(uint32_t);

    m_impl->vertexBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    m_impl->vertexBuffer.UploadViaStaging(dev, combined.vertices.data(), vbSize);

    m_impl->indexBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    m_impl->indexBuffer.UploadViaStaging(dev, combined.indices.data(), ibSize);

    // combined goes out of scope here; CPU memory is freed.
}

void StaticMesh::Destroy(VkDevice device) {
    m_impl->vertexBuffer.Destroy(device);
    m_impl->indexBuffer.Destroy(device);
    m_impl->indexCount = 0;
}

const GpuBuffer& StaticMesh::VertexBuffer() const { return m_impl->vertexBuffer; }
const GpuBuffer& StaticMesh::IndexBuffer()  const { return m_impl->indexBuffer; }
uint32_t         StaticMesh::IndexCount()   const { return m_impl->indexCount; }

} // namespace xcel

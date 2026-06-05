#include "Renderer/MeshDrawable.h"
#include "Kernel/MeshTessellator.h"
#include "Kernel/ColorTable.h"
#include "Renderer/GpuBuffer.h"
#include "Common/ThreadPool.h"
#include <stdexcept>
#include <algorithm>
#include <future>
#include <vector>

namespace xcel {

// Below this element count tessellation runs serially on the calling thread.
// Above it, elements are split across pool->ThreadCount() parallel tasks.
static constexpr size_t kParallelThreshold = 1'024;

struct MeshDrawable::Impl {
    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;
    uint32_t  indexCount = 0;
};

MeshDrawable::MeshDrawable()
    : m_impl(std::make_unique<Impl>()) {}

MeshDrawable::~MeshDrawable() = default;

void MeshDrawable::Build(DeviceContext& dev, const ColorTable& colormap, ThreadPool* pool)
{
    if (!Coords() || !Scalars())
        throw std::runtime_error("MeshDrawable::Build: coords and scalars must be set");

    TessellatedMesh combined;

    for (const auto& ps : PrimitiveSets()) {
        size_t N = ps->ElementCount();

        TessellatedMesh part;

        if (pool && N > kParallelThreshold) {
            size_t T         = pool->ThreadCount();
            size_t chunkSize = (N + T - 1) / T;

            float minS = Scalars()->MinValue();
            float maxS = Scalars()->MaxValue();

            std::vector<std::future<TessellatedMesh>> futures;
            futures.reserve(T);

            for (size_t t = 0; t < T; ++t) {
                size_t begin = t * chunkSize;
                size_t end   = std::min(begin + chunkSize, N);
                if (begin >= N) break;
                futures.push_back(pool->Submit([&, begin, end] {
                    return TessellateRange(
                        *ps,
                        *Coords(), *Scalars(), colormap,
                        begin, end,
                        minS, maxS);
                }));
            }

            for (auto& f : futures) {
                auto partial = f.get();
                auto base = static_cast<uint32_t>(part.vertices.size());
                for (auto idx : partial.indices)
                    part.indices.push_back(base + idx);
                part.vertices.insert(part.vertices.end(),
                                     partial.vertices.begin(), partial.vertices.end());
            }
        } else {
            part = Tessellate(*ps, *Coords(), *Scalars(), colormap);
        }

        uint32_t baseVertex = static_cast<uint32_t>(combined.vertices.size());
        for (auto idx : part.indices)
            combined.indices.push_back(baseVertex + idx);
        combined.vertices.insert(combined.vertices.end(),
                                 part.vertices.begin(), part.vertices.end());
    }

    if (combined.vertices.empty())
        throw std::runtime_error("MeshDrawable::Build: no geometry produced");

    m_impl->indexCount = static_cast<uint32_t>(combined.indices.size());

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
}

void MeshDrawable::Destroy(VkDevice device)
{
    m_impl->vertexBuffer.Destroy(device);
    m_impl->indexBuffer.Destroy(device);
    m_impl->indexCount = 0;
}

const GpuBuffer& MeshDrawable::VertexBuffer() const { return m_impl->vertexBuffer; }
const GpuBuffer& MeshDrawable::IndexBuffer()  const { return m_impl->indexBuffer; }
uint32_t         MeshDrawable::IndexCount()   const { return m_impl->indexCount; }

} // namespace xcel

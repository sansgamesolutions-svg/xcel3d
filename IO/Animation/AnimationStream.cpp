#include "IO/Animation/AnimationStream.h"
#include "IO/Core/FileStreamSource.h"
#include "Common/ThreadPool.h"
#include "Kernel/ScalarTable.h"
#include <stdexcept>

namespace xcel::io {

AnimationStream::AnimationStream(AnimationTrackInfo           info,
                                 std::vector<ChunkDescriptor> frameChunks,
                                 std::filesystem::path        sourcePath,
                                 size_t                       cacheCapacity)
    : m_info(std::move(info))
    , m_frameChunks(std::move(frameChunks))
    , m_sourcePath(std::move(sourcePath))
    , m_cache(cacheCapacity)
{}

std::shared_future<AnimationFrame>
AnimationStream::GetFrame(uint32_t frameIndex, xcel::ThreadPool& pool)
{
    if (frameIndex >= m_info.frameCount)
        throw std::out_of_range("AnimationStream::GetFrame: frameIndex out of range");

    // Cache hit — wrap in a ready future.
    if (const AnimationFrame* hit = m_cache.Get(m_info.id, frameIndex))
    {
        std::promise<AnimationFrame> p;
        p.set_value(*hit);
        return p.get_future().share();
    }

    return pool.Submit([this, frameIndex]() -> AnimationFrame {
        AnimationFrame frame = DecodeFrame(frameIndex);
        m_cache.Put(m_info.id, frameIndex, frame);
        return frame;
    }).share();
}

void AnimationStream::Prefetch(uint32_t startFrame, uint32_t count,
                               xcel::ThreadPool& pool)
{
    uint32_t end = std::min(startFrame + count, m_info.frameCount);
    for (uint32_t i = startFrame; i < end; ++i)
    {
        if (!m_cache.Get(m_info.id, i))
            pool.Submit([this, i] {
                AnimationFrame frame = DecodeFrame(i);
                m_cache.Put(m_info.id, i, std::move(frame));
            });
    }
}

AnimationFrame AnimationStream::DecodeFrame(uint32_t frameIndex) const
{
    if (frameIndex >= m_frameChunks.size())
        throw std::out_of_range("AnimationStream::DecodeFrame: no chunk for frame");

    const ChunkDescriptor& chunk = m_frameChunks[frameIndex];

    // Each decode opens its own file handle so parallel calls don't collide.
    FileStreamSource source(m_sourcePath);
    source.Seek(chunk.offset);

    // Decode a flat array of float scalars (one per element).
    uint32_t elementCount = 0;
    source.Read(reinterpret_cast<std::byte*>(&elementCount), sizeof(elementCount));

    auto scalars = std::make_shared<xcel::ElementScalarTable>();
    for (uint32_t i = 0; i < elementCount; ++i)
    {
        float v = 0.f;
        source.Read(reinterpret_cast<std::byte*>(&v), sizeof(v));
        scalars->AddScalar(v);
    }

    return AnimationFrame{
        frameIndex,
        static_cast<float>(frameIndex) * m_info.timeStepSeconds,
        std::move(scalars),
        nullptr
    };
}

} // namespace xcel::io

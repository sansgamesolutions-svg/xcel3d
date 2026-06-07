#pragma once
#include "IO/Animation/AnimationFrame.h"
#include "IO/Animation/AnimationTrackInfo.h"
#include "IO/Animation/FrameCache.h"
#include "IO/Scene/ChunkDescriptor.h"
#include <filesystem>
#include <future>
#include <memory>
#include <vector>

namespace xcel {
class ThreadPool;
}

namespace xcel::io {

class IStreamSource;

class AnimationStream
{
public:
    AnimationStream(AnimationTrackInfo              info,
                    std::vector<ChunkDescriptor>    frameChunks,
                    std::filesystem::path           sourcePath,
                    size_t                          cacheCapacity = 32);

    const AnimationTrackInfo& TrackInfo()  const { return m_info; }
    uint32_t                  FrameCount() const { return m_info.frameCount; }

    // Returns immediately; decoding happens on a pool thread.
    std::shared_future<AnimationFrame> GetFrame(uint32_t frameIndex,
                                                xcel::ThreadPool& pool);

    // Warms the cache for [startFrame, startFrame+count).
    void Prefetch(uint32_t startFrame, uint32_t count, xcel::ThreadPool& pool);

private:
    AnimationFrame DecodeFrame(uint32_t frameIndex) const;

    AnimationTrackInfo           m_info;
    std::vector<ChunkDescriptor> m_frameChunks; // one per frame, offset+size into file
    std::filesystem::path        m_sourcePath;
    FrameCache                   m_cache;
};

} // namespace xcel::io

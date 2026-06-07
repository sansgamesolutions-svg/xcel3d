#include "IO/Scene/SceneDocument.h"
#include <stdexcept>

namespace xcel::io {

void SceneDocument::AddMesh(MeshData mesh)
{
    ChunkDescriptor desc;
    desc.type = ChunkType::Mesh;
    desc.id   = static_cast<uint32_t>(m_meshes.size());
    desc.name = mesh.name;
    m_toc.push_back(desc);
    m_meshes.push_back(std::move(mesh));
}

void SceneDocument::AddSkeleton(SkeletonData skel)
{
    ChunkDescriptor desc;
    desc.type = ChunkType::Skeleton;
    desc.id   = static_cast<uint32_t>(m_skeletons.size());
    desc.name = skel.name;
    m_toc.push_back(desc);
    m_skeletons.push_back(std::move(skel));
}

void SceneDocument::AddAnimationTrack(AnimationTrackInfo info)
{
    ChunkDescriptor desc;
    desc.type = ChunkType::AnimCatalogue;
    desc.id   = info.id;
    desc.name = info.name;
    m_toc.push_back(desc);
    m_catalogue.AddTrack(std::move(info));

    // Ensure slot exists in frame chunk table.
    if (m_frameChunks.size() <= info.id)
        m_frameChunks.resize(info.id + 1);
}

void SceneDocument::AddFrameChunk(uint32_t trackId, uint32_t frameIndex,
                                   ChunkDescriptor desc)
{
    if (m_frameChunks.size() <= trackId)
        m_frameChunks.resize(trackId + 1);

    auto& track = m_frameChunks[trackId];
    if (track.size() <= frameIndex)
        track.resize(frameIndex + 1);

    track[frameIndex] = std::move(desc);
}

void SceneDocument::SetSceneRoot(SceneNode root)
{
    m_sceneRoot = std::move(root);
}

void SceneDocument::SetSourcePath(std::filesystem::path path)
{
    m_sourcePath = std::move(path);
}

AnimationStream SceneDocument::OpenAnimationStream(uint32_t trackId,
                                                    size_t   cacheCapacity) const
{
    const AnimationTrackInfo* info = nullptr;
    for (size_t i = 0; i < m_catalogue.TrackCount(); ++i)
    {
        if (m_catalogue.Track(i).id == trackId)
        {
            info = &m_catalogue.Track(i);
            break;
        }
    }
    if (!info)
        throw std::runtime_error("SceneDocument::OpenAnimationStream: unknown trackId");

    std::vector<ChunkDescriptor> chunks;
    if (trackId < m_frameChunks.size())
        chunks = m_frameChunks[trackId];

    return AnimationStream(*info, std::move(chunks), m_sourcePath, cacheCapacity);
}

} // namespace xcel::io

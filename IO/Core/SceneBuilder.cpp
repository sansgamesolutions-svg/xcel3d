#include "IO/Core/SceneBuilder.h"
#include "IO/Scene/SceneDocument.h"

namespace xcel::io {

uint32_t SceneBuilder::AddMesh(MeshData mesh)
{
    std::scoped_lock lock(m_mutex);
    uint32_t id = m_nextMeshId++;
    m_meshes.push_back(std::move(mesh));
    return id;
}

uint32_t SceneBuilder::AddSkeleton(SkeletonData skel)
{
    std::scoped_lock lock(m_mutex);
    uint32_t id = m_nextSkelId++;
    m_skeletons.push_back(std::move(skel));
    return id;
}

uint32_t SceneBuilder::AddAnimationTrack(AnimationTrackInfo info)
{
    std::scoped_lock lock(m_mutex);
    info.id = m_nextTrackId++;
    m_animTracks.push_back(std::move(info));
    return m_animTracks.back().id;
}

void SceneBuilder::AddAnimationFrameAtOffset(uint32_t trackId, uint32_t frameIndex,
                                              uint64_t offset,  uint64_t size)
{
    std::scoped_lock lock(m_mutex);
    ChunkDescriptor desc;
    desc.type   = ChunkType::AnimFrame;
    desc.id     = frameIndex;
    desc.offset = offset;
    desc.size   = size;
    m_frameChunks.emplace_back(trackId, std::move(desc));
}

void SceneBuilder::SetSceneRoot(SceneNode root)
{
    std::scoped_lock lock(m_mutex);
    m_sceneRoot = std::move(root);
    m_hasRoot   = true;
}

std::shared_ptr<SceneDocument> SceneBuilder::Build()
{
    auto doc = std::make_shared<SceneDocument>();

    for (auto& m : m_meshes)
        doc->AddMesh(std::move(m));

    for (auto& s : m_skeletons)
        doc->AddSkeleton(std::move(s));

    for (auto& t : m_animTracks)
        doc->AddAnimationTrack(std::move(t));

    for (auto& [trackId, chunk] : m_frameChunks)
        doc->AddFrameChunk(trackId, chunk.id, chunk);

    if (m_hasRoot)
        doc->SetSceneRoot(std::move(m_sceneRoot));

    return doc;
}

} // namespace xcel::io

#pragma once
#include "IO/Animation/AnimationTrackInfo.h"
#include "IO/Scene/ChunkDescriptor.h"
#include "IO/Scene/MeshData.h"
#include "IO/Scene/SceneNode.h"
#include "IO/Scene/SkeletonData.h"
#include <memory>
#include <mutex>
#include <vector>

namespace xcel::io {

class SceneDocument;

// Mutable staging area populated by IFormatReader::Read().
// Thread-safe: readers may call AddMesh / AddSkeleton from parallel tasks.
class SceneBuilder
{
public:
    // Returns the assigned id for the mesh (used to link animations).
    uint32_t AddMesh(MeshData mesh);

    uint32_t AddSkeleton(SkeletonData skel);

    // Register an animation track (metadata only).
    uint32_t AddAnimationTrack(AnimationTrackInfo info);

    // For native .xcel streaming: record where a frame lives on disk rather
    // than decoding it immediately.
    void AddAnimationFrameAtOffset(uint32_t trackId, uint32_t frameIndex,
                                   uint64_t offset,  uint64_t size);

    // Replace the root of the scene graph (call once).
    void SetSceneRoot(SceneNode root);

    // Finalises the document; called by IOManager after Read() returns.
    std::shared_ptr<SceneDocument> Build();

private:
    std::mutex m_mutex;

    std::vector<MeshData>          m_meshes;
    std::vector<SkeletonData>      m_skeletons;
    std::vector<AnimationTrackInfo> m_animTracks;

    // (trackId, frameIndex) → ChunkDescriptor for lazy frame loading
    std::vector<std::pair<uint32_t, ChunkDescriptor>> m_frameChunks;

    SceneNode  m_sceneRoot;
    bool       m_hasRoot = false;

    uint32_t   m_nextMeshId   = 0;
    uint32_t   m_nextSkelId   = 0;
    uint32_t   m_nextTrackId  = 0;
};

} // namespace xcel::io

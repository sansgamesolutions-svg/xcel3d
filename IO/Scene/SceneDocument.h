#pragma once
#include "IO/Animation/AnimationCatalogue.h"
#include "IO/Animation/AnimationStream.h"
#include "IO/Scene/ChunkDescriptor.h"
#include "IO/Scene/MeshData.h"
#include "IO/Scene/SceneNode.h"
#include "IO/Scene/SkeletonData.h"
#include <filesystem>
#include <future>
#include <memory>
#include <vector>

namespace xcel {
class ThreadPool;
}

namespace xcel::io {

// Represents a loaded (or in-memory) scene.
// Header + TOC are always in RAM; mesh/skeleton/frame data is demand-loaded.
class SceneDocument
{
public:
    // ── Construction (called by SceneBuilder::Build) ────────────────────────

    SceneDocument() = default;

    void AddMesh(MeshData mesh);
    void AddSkeleton(SkeletonData skel);
    void AddAnimationTrack(AnimationTrackInfo info);
    void AddFrameChunk(uint32_t trackId, uint32_t frameIndex, ChunkDescriptor desc);
    void SetSceneRoot(SceneNode root);
    void SetSourcePath(std::filesystem::path path);

    // ── TOC / metadata (always available) ───────────────────────────────────

    const std::vector<ChunkDescriptor>& Toc()       const { return m_toc; }
    const AnimationCatalogue&           Catalogue() const { return m_catalogue; }
    const SceneNode&                    SceneRoot() const { return m_sceneRoot; }

    // ── Synchronous accessors (data already in memory) ───────────────────────

    size_t                MeshCount()          const { return m_meshes.size(); }
    const MeshData&       Mesh(size_t i)       const { return m_meshes[i]; }

    size_t                SkeletonCount()      const { return m_skeletons.size(); }
    const SkeletonData&   Skeleton(size_t i)   const { return m_skeletons[i]; }

    // ── Demand-loaded (open an animation stream for a track) ─────────────────

    // Returns a stream for seeking individual frames.
    AnimationStream OpenAnimationStream(uint32_t trackId,
                                        size_t   cacheCapacity = 32) const;

private:
    std::filesystem::path           m_sourcePath;
    std::vector<ChunkDescriptor>    m_toc;
    AnimationCatalogue              m_catalogue;
    SceneNode                       m_sceneRoot;

    std::vector<MeshData>           m_meshes;
    std::vector<SkeletonData>       m_skeletons;

    // Per-track frame chunks: m_frameChunks[trackId][frameIndex]
    std::vector<std::vector<ChunkDescriptor>> m_frameChunks;
};

} // namespace xcel::io

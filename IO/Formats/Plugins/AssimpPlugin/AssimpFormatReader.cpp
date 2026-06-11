#include "AssimpFormatReader.h"
#include "IO/Core/IStreamSource.h"
#include "IO/Core/ISceneReceiver.h"
#include "IO/Animation/AnimationTrackInfo.h"
#include "Common/ThreadPool.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace xcel::io {

// ── CanRead ──────────────────────────────────────────────────────────────────

bool AssimpFormatReader::CanRead(std::string_view extension) const
{
    std::string ext = ".";
    ext += extension;
    Assimp::Importer importer;
    return importer.IsExtensionSupported(ext);
}

// ── Read ─────────────────────────────────────────────────────────────────────

void AssimpFormatReader::Read(IStreamSource& source, ISceneReceiver& receiver,
                               xcel::ThreadPool* pool)
{
    // Drain the entire source into a buffer so Assimp can parse from memory.
    std::vector<std::byte> buf(static_cast<size_t>(source.Size()));
    source.Seek(0);
    source.Read(buf.data(), buf.size());

    Assimp::Importer importer;
    constexpr unsigned int kFlags =
        aiProcess_Triangulate          |
        aiProcess_JoinIdenticalVertices|
        aiProcess_GenNormals           |
        aiProcess_FlipUVs;

    const aiScene* scene = importer.ReadFileFromMemory(
        buf.data(), buf.size(), kFlags, nullptr);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        throw std::runtime_error(
            std::string("AssimpFormatReader: ") + importer.GetErrorString());

    // ── Meshes ───────────────────────────────────────────────────────────────
    // Build transient flat arrays per mesh and deliver via receiver.
    // ISceneReceiver::ReceiveMesh is called under receiverMutex — the receiver
    // (WorldSceneReceiver) modifies ECS state and is not thread-safe.

    std::vector<std::future<void>> meshFutures;
    std::mutex                     receiverMutex;

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        auto convert = [&, i]()
        {
            const aiMesh* mesh = scene->mMeshes[i];

            // Flat position array: xyz interleaved.
            std::vector<float> positions;
            positions.reserve(mesh->mNumVertices * 3);
            for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
            {
                positions.push_back(mesh->mVertices[v].x);
                positions.push_back(mesh->mVertices[v].y);
                positions.push_back(mesh->mVertices[v].z);
            }

            // Flat index array: triangle triples (aiProcess_Triangulate guarantees 3).
            std::vector<uint32_t> indices;
            indices.reserve(mesh->mNumFaces * 3);
            for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }

            // Deliver — empty scalars span means receiver uses constant 0.
            std::scoped_lock lock(receiverMutex);
            receiver.ReceiveMesh(
                mesh->mName.C_Str(),
                positions,
                xcel::PrimitiveType::PT_TRIANGLE,
                indices,
                3,
                {} // no per-element scalars
            );
        };

        if (pool)
            meshFutures.push_back(pool->Submit(convert));
        else
            convert();
    }
    for (auto& f : meshFutures) f.get();

    // ── Animations ───────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
        ConvertAnimation(scene->mAnimations[i], i, /*meshId=*/0, receiver);
}

// ── ConvertAnimation ─────────────────────────────────────────────────────────

void AssimpFormatReader::ConvertAnimation(const aiAnimation* anim, uint32_t animIndex,
                                          uint32_t meshId, ISceneReceiver& receiver)
{
    AnimationTrackInfo info;
    info.name            = anim->mName.C_Str();
    info.meshId          = meshId;
    info.timeStepSeconds = (anim->mTicksPerSecond > 0.0)
                           ? static_cast<float>(1.0 / anim->mTicksPerSecond)
                           : (1.f / 24.f);
    info.frameCount      = static_cast<uint32_t>(anim->mDuration) + 1;
    (void)animIndex;
    receiver.ReceiveAnimationTrack(info);
}

} // namespace xcel::io

// ── Plugin ABI ───────────────────────────────────────────────────────────────

static const XcelPluginInfo kPluginInfo = {
    XCEL_IO_API_VERSION,
    "Assimp",
    "fbx;gltf;glb;obj;dae;3ds;ply;stl;blend;abc",
    false
};

extern "C" {

XCEL_IO_EXPORT const XcelPluginInfo* xcel_plugin_info()
{
    return &kPluginInfo;
}

XCEL_IO_EXPORT xcel::io::IFormatReader* xcel_create_reader()
{
    return new xcel::io::AssimpFormatReader();
}

XCEL_IO_EXPORT void xcel_destroy_reader(xcel::io::IFormatReader* p)
{
    delete p;
}

} // extern "C"

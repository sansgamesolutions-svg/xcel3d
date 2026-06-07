#include "AssimpFormatReader.h"
#include "IO/Core/IStreamSource.h"
#include "IO/Core/SceneBuilder.h"
#include "IO/Scene/MeshData.h"
#include "IO/Scene/SceneNode.h"
#include "IO/Scene/SkeletonData.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include "Graphics/PrimitiveSet.h"
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
    // Assimp expects a dot-prefixed extension.
    std::string ext = ".";
    ext += extension;
    Assimp::Importer importer;
    return importer.IsExtensionSupported(ext);
}

// ── Read ─────────────────────────────────────────────────────────────────────

void AssimpFormatReader::Read(IStreamSource& source, SceneBuilder& out,
                               xcel::ThreadPool* pool)
{
    // Drain the entire source into a buffer so Assimp can parse from memory.
    std::vector<std::byte> buf(static_cast<size_t>(source.Size()));
    source.Seek(0);
    source.Read(buf.data(), buf.size());

    Assimp::Importer importer;
    constexpr unsigned int kFlags =
        aiProcess_Triangulate          |  // all faces → triangles
        aiProcess_JoinIdenticalVertices|
        aiProcess_GenNormals           |
        aiProcess_FlipUVs;

    const aiScene* scene = importer.ReadFileFromMemory(
        buf.data(), buf.size(), kFlags,
        nullptr   // hint: nullptr lets Assimp try all registered formats
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        throw std::runtime_error(
            std::string("AssimpFormatReader: ") + importer.GetErrorString());

    // ── Meshes ───────────────────────────────────────────────────────────────

    std::vector<std::future<void>> meshFutures;
    std::mutex                     outMutex;

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        auto convert = [&, i]() {
            MeshData data = ConvertMeshToData(scene->mMeshes[i], i);
            std::scoped_lock lock(outMutex);
            out.AddMesh(std::move(data));
        };

        if (pool)
            meshFutures.push_back(pool->Submit(convert));
        else
            convert();
    }
    for (auto& f : meshFutures) f.get();

    // ── Animations ───────────────────────────────────────────────────────────

    for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
        ConvertAnimation(scene->mAnimations[i], i, /*meshId=*/0, out);

    // ── Scene graph ──────────────────────────────────────────────────────────

    out.SetSceneRoot(ConvertNode(scene->mRootNode));
}

// ── ConvertMeshToData ────────────────────────────────────────────────────────

MeshData AssimpFormatReader::ConvertMeshToData(const aiMesh* mesh, uint32_t /*idx*/)
{
    MeshData data;
    data.name       = mesh->mName.C_Str();
    data.coords     = std::make_shared<xcel::CoordTable>();
    data.colorTable = std::make_shared<xcel::PaletteColor>();

    data.coords->Reserve(mesh->mNumVertices);
    for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
    {
        data.coords->AddCoord({
            mesh->mVertices[v].x,
            mesh->mVertices[v].y,
            mesh->mVertices[v].z
        });
    }

    auto triSet = std::make_shared<xcel::TrianglePrimitiveSet>();
    for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue; // aiProcess_Triangulate guarantees 3
        triSet->AddElement({face.mIndices[0], face.mIndices[1], face.mIndices[2]});
    }
    data.primSets.push_back(std::move(triSet));

    // Scalar field: constant zero; updated per-frame during animation playback.
    data.scalars = std::make_shared<xcel::ConstantScalarTable>(0.f, mesh->mNumFaces);

    return data;
}

// ── ConvertAnimation ─────────────────────────────────────────────────────────

void AssimpFormatReader::ConvertAnimation(const aiAnimation* anim, uint32_t animIndex,
                                          uint32_t meshId, SceneBuilder& out)
{
    AnimationTrackInfo info;
    info.name          = anim->mName.C_Str();
    info.meshId        = meshId;
    info.timeStepSeconds = (anim->mTicksPerSecond > 0.0)
                           ? static_cast<float>(1.0 / anim->mTicksPerSecond)
                           : (1.f / 24.f);
    info.frameCount    = static_cast<uint32_t>(anim->mDuration) + 1;

    out.AddAnimationTrack(std::move(info));
    (void)animIndex;
    // Note: Assimp animations are keyframe-based (per bone/channel), not per-frame
    // scalar fields. Frame-by-frame scalar decoding is N/A here; the track metadata
    // is still registered so the AnimationCatalogue knows the animation exists.
    // Full skeletal animation playback would be added via a separate bone-transform
    // AnimationFrame variant when the skeleton system is extended.
}

// ── ConvertNode ──────────────────────────────────────────────────────────────

xcel::io::SceneNode AssimpFormatReader::ConvertNode(const aiNode* node)
{
    xcel::io::SceneNode sn;
    sn.name = node->mName.C_Str();

    const aiMatrix4x4& m = node->mTransformation;
    // Assimp is row-major; GLM is column-major — transpose on construction.
    sn.localTransform = glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );

    // Link first referenced mesh; UINT32_MAX = no mesh on this node.
    sn.meshId = (node->mNumMeshes > 0) ? node->mMeshes[0] : UINT32_MAX;

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
        sn.children.push_back(ConvertNode(node->mChildren[i]));

    return sn;
}

} // namespace xcel::io

// ── Plugin ABI ───────────────────────────────────────────────────────────────

static const XcelPluginInfo kPluginInfo = {
    XCEL_IO_API_VERSION,
    "Assimp",
    "fbx;gltf;glb;obj;dae;3ds;ply;stl;blend;abc",
    false   // write not supported via Assimp plugin
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

#pragma once
#include "IO/Core/IFormatReader.h"
#include "IO/Core/IFormatPlugin.h"
#include "IO/Scene/MeshData.h"
#include "IO/Scene/SceneNode.h"

struct aiMesh;
struct aiAnimation;
struct aiNode;
struct aiScene;

namespace xcel::io {

class AssimpFormatReader final : public IFormatReader
{
public:
    // Delegates to Assimp::Importer::IsExtensionSupported — covers ~50 formats.
    bool CanRead(std::string_view extension) const override;

    // Reads the entire source into memory, then hands it to Assimp.
    void Read(IStreamSource& source, SceneBuilder& out,
              xcel::ThreadPool* pool) override;

private:
    static MeshData ConvertMeshToData(const aiMesh* mesh, uint32_t meshIndex);

    static void ConvertAnimation(const aiAnimation* anim, uint32_t animIndex,
                                 uint32_t meshId, SceneBuilder& out);

    static xcel::io::SceneNode ConvertNode(const aiNode* node);
};

} // namespace xcel::io

// ── Plugin ABI ───────────────────────────────────────────────────────────────
extern "C" {
    XCEL_IO_EXPORT const XcelPluginInfo*      xcel_plugin_info();
    XCEL_IO_EXPORT xcel::io::IFormatReader*   xcel_create_reader();
    XCEL_IO_EXPORT void                       xcel_destroy_reader(xcel::io::IFormatReader* p);
}

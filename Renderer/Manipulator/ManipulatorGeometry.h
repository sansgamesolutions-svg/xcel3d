#pragma once
#include "Renderer/GpuBuffer.h"
#include "Kernel/MeshTessellator.h"
#include <cstdint>

namespace xcel {

class DeviceContext;

struct GizmoMesh
{
    GpuBuffer verts;
    GpuBuffer indices;
    uint32_t  indexCount = 0;
};

// Builds procedural GPU meshes for gizmo rendering.
// All geometry uses MeshVertex (position, normal, color) so the manipulator
// pipeline can reuse the same vertex-input description as the scene pipeline.
class ManipulatorGeometry
{
public:
    ManipulatorGeometry()  = default;
    ~ManipulatorGeometry() = default;

    ManipulatorGeometry(const ManipulatorGeometry&)            = delete;
    ManipulatorGeometry& operator=(const ManipulatorGeometry&) = delete;

    void Build(DeviceContext& dev);
    void Destroy(VkDevice device);

    // Unit arrow along +Y: shaft from (0,0,0) to (0,0.8,0), cone head to (0,1,0).
    const GizmoMesh& Arrow() const { return m_arrow; }

    // Unit cube [-0.5..0.5]^3, face colors white.
    const GizmoMesh& Cube()  const { return m_cube; }

    // Unit quad in XZ plane [-0.5..0.5] at Y=0, normal +Y, color white.
    const GizmoMesh& Plane() const { return m_plane; }

private:
    static void BuildArrow(std::vector<MeshVertex>& verts, std::vector<uint32_t>& idx);
    static void BuildCube (std::vector<MeshVertex>& verts, std::vector<uint32_t>& idx);
    static void BuildPlane(std::vector<MeshVertex>& verts, std::vector<uint32_t>& idx);

    static void Upload(DeviceContext& dev, GizmoMesh& mesh,
                       const std::vector<MeshVertex>& verts,
                       const std::vector<uint32_t>&   idx);

    GizmoMesh m_arrow;
    GizmoMesh m_cube;
    GizmoMesh m_plane;
};

} // namespace xcel

#pragma once
#include "Graphics/Drawable.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <memory>
#include <span>

namespace xcel {

// Drawable for GPU instancing: one geometry rendered N times per draw call.
// All mesh data (coords, scalars, colorTable, primitiveSets, tessellation strategy)
// is read from ECS components on the supplied mesh entity during Build().
//
// Lifecycle:
//   1. Create via WindowContext::AddInstanceMesh() — entity + drawable are wired together.
//   2. Call WindowContext::AddInstance(entity, transform) for each copy.
//   3. WindowContext::Run() calls Build() then UpdateInstances() automatically.
class InstanceDrawable : public Drawable {
public:
    // meshEntity must have CoordTableComponent, ScalarTableComponent,
    // ColorTableComponent, and PrimitiveSetsComponent before Build() is called.
    explicit InstanceDrawable(flecs::entity meshEntity);
    ~InstanceDrawable() override;

    // Tessellates geometry from the entity's ECS components and uploads to VRAM.
    // The colormap parameter is unused — ColorTableComponent on the entity is used.
    void Build(DeviceContext& dev, const ColorTable& /*colormap*/,
               ThreadPool* pool = nullptr) override;

    // Upload per-instance model matrices. Call after Build().
    void UpdateInstances(DeviceContext& dev, std::span<const glm::mat4> transforms);

    void             Destroy(VkDevice device) override;
    const GpuBuffer& VertexBuffer()   const override;
    const GpuBuffer& IndexBuffer()    const override;
    uint32_t         IndexCount()     const override;
    const GpuBuffer* InstanceBuffer() const override;
    uint32_t         InstanceCount()  const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

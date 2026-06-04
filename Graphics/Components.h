#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace xcel {

class StaticMesh;

// Human-readable tag attached to every entity created via WindowContext::AddMesh.
struct NameComponent {
    std::string name;
};

// Holds the GPU-resident mesh for an entity.
struct MeshComponent {
    std::shared_ptr<StaticMesh> mesh;
};

// Per-entity model-space transform. Defaults to identity.
// Currently stored for future per-entity draw calls; WindowContext still uses
// a global identity model matrix in the UBO until push-constants are added.
struct TransformComponent {
    glm::mat4 matrix{1.0f};
};

// Controls whether the entity is submitted to the draw call list each frame.
struct VisibilityComponent {
    bool visible = true;
};

} // namespace xcel

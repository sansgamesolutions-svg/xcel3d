
### Graphics (`Graphics/`)

Raw Vulkan renderer. Compiled into the `XcelGraphics` static library.

**Data model** (CPU side):
- `CoordTable` — `std::vector<glm::vec3>` node positions (shared across elements)
- `ScalarTable` — `std::vector<float>` one scalar value per element (e.g., stress)
- `ColorTable` — 256-entry cool-to-warm RGB lookup; `mapScalar(value, min, max)` returns `glm::vec3`
- `PrimitiveSet` — abstract base; `HexPrimitiveSet` holds 8-node hex elements as `array<uint32_t,8>`
- `Mesh` — owns `CoordTable`, `ScalarTable`, and a list of `PrimitiveSet`s
- `StaticMesh : Mesh` — tessellates hexes to triangles and uploads to GPU vertex/index buffers

**Tessellation** (`HexTessellator`):
- Each hex → 6 quad faces → 12 triangles, 24 vertices (duplicated at element boundaries)
- `MeshVertex` = position(12) + normal(12) + color(12) bytes; stride 36
- VTK hex node ordering (nodes 0-7); face normals via `cross(v1-v0, v3-v0)`
- Per-element scalar mapped to RGB at tessellation time (no GPU colormap texture)

**Vulkan infrastructure** (initialization order):
```
VulkanContext (instance → device → queues → command pool)
  → RenderPass (color + depth attachments)
  → Swapchain (images, image views, depth buffer, framebuffers)
  → DescriptorManager (UBO layout, pool, 2× persistently mapped FrameUBO buffers)
  → Pipeline (loads SPIR-V, hardcoded vertex layout matching MeshVertex)
  → CommandRecorder (2 command buffers, re-recorded each frame)
```

**FrameUBO** (std140, 240 bytes): `model[16]`, `view[16]`, `proj[16]`, `lightPos[3]+pad`, `lightColor[3]+pad`, `viewPos[3]+pad`. One UBO per frame-in-flight (double-buffered).

**Camera** (`Camera`): orbit camera; `fitToSphere(center, radius)` → `_radius = radius * 2.5`. Projection flips Y (`proj[1][1] *= -1`) for Vulkan NDC.

**GpuBuffer**: RAII `VkBuffer`+`VkDeviceMemory`; `uploadViaStaging()` uses a one-shot command buffer.


## Design Conventions

- `GpuBuffer` is non-copyable; move-only.
- `StaticMesh::build(ctx, colormap)` uploads to GPU — call once after filling the mesh data, before `app.run()`.
- `Application::addMesh()` must be called before `Application::run()`.
- Builder layer interfaces (`Builder/`) are not yet connected to the Graphics layer; they remain pure-virtual stubs.
- MSVC `/utf-8` is enforced — all source files must be UTF-8 encoded.
## Shaders (`Shaders/`)

- `mesh.vert` / `mesh.frag` - Blinn-Phong shading; one UBO at set=0 binding=0
- `oit_accumulate.frag` (paired with `mesh.vert`) - weighted blended OIT accumulate
  pass; writes `outAccum`/`outReveal` for `OitPass`'s subpass 0
- `oit_composite.vert` / `oit_composite.frag` - buffer-less fullscreen triangle that
  resolves `oit_accumulate.frag`'s targets into the swapchain color attachment in
  `OitPass`'s subpass 1
- Shared GLSL headers (`#include`d via `GL_GOOGLE_include_directive`, with manual
  `#ifndef`/`#define` guards since GLSL has no `#pragma once`):
  - `scene_ubo.glsl` - the `FrameUBO` block (set=0, binding=0)
  - `lighting.glsl` - `LightGpu` struct + the shared `BlinnPhong(...)` function
  - `bindless_textures.glsl` - bindless `textures[]`/`texSampler` (set=1)
  - `material.glsl` - the `MaterialPC` push-constant block
  - Used by `mesh.vert`/`mesh.frag` and `oit_accumulate.frag` to avoid duplicating
    lighting/material code between the opaque and OIT fragment shaders
- Compiled to SPIR-V by glslc via CMake custom commands; output goes to `bin/shaders/`

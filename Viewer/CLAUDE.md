
### Viewer (`Viewer/`)

Compiled into the `XcelViewer` executable.

- `Application` — owns GLFW window and all Vulkan objects; double-buffered frame loop with resize handling
- `main.cxx` — builds a 2×2×2 demo hex mesh with distance-from-center scalar, calls `app.run()`

GLFW mouse callbacks: left-drag → `camera.orbit()`, scroll → `camera.zoom()`.

**Shader paths**: The application loads `shaders/mesh.vert.spv` and `shaders/mesh.frag.spv` relative to the working directory. Run the executable from the `bin/` directory.

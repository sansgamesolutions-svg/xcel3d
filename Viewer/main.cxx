#include "Viewer/Application.h"
#include "Platforms/GlfwWindowWidget.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include "Graphics/PrimitiveSet.h"
#include "Graphics/Camera.h"
#include <glm/glm.hpp>
#include <memory>
#include <stdexcept>
#include <iostream>

// Builds a 2x2x2 grid of hexahedral FEM elements (8 elements, 27 nodes).
// The scalar field is the distance of each element's centroid from the grid center,
// demonstrating blue-to-red colormap mapping.
static void buildDemoMesh(xcel::Application& app)
{
    auto coords  = std::make_shared<xcel::CoordTable>();
    auto scalars = std::make_shared<xcel::ElementScalarTable>();
    auto hexSet  = std::make_shared<xcel::HexPrimitiveSet>();

    // 27 nodes on a 3x3x3 lattice with 1.0 spacing
    for (int k = 0; k < 3; ++k)
    for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
        coords->AddCoord({(float)i, (float)j, (float)k});

    auto idx = [](int i, int j, int k) -> uint32_t {
        return (uint32_t)(i + j * 3 + k * 9);
    };

    glm::vec3 center(1.f, 1.f, 1.f);

    for (int k = 0; k < 2; ++k)
    for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i) {
        xcel::HexPrimitiveSet::value_type e = {
            idx(i,   j,   k  ),
            idx(i+1, j,   k  ),
            idx(i+1, j+1, k  ),
            idx(i,   j+1, k  ),
            idx(i,   j,   k+1),
            idx(i+1, j,   k+1),
            idx(i+1, j+1, k+1),
            idx(i,   j+1, k+1),
        };
        hexSet->AddElement(e);

        glm::vec3 centroid(i + 0.5f, j + 0.5f, k + 0.5f);
        scalars->AddScalar(glm::length(centroid - center));
    }

    app.GetWorld().AddMesh("demo", coords, scalars,
                           std::make_shared<xcel::PaletteColor>(),
                           {hexSet});
}

int main()
{
    try {
        auto widget = std::make_unique<xcel::GlfwWindowWidget>(1280, 720, "Xcel3D - FEM Hex Viewer");
        xcel::Application app(std::move(widget));

        buildDemoMesh(app);

        // Fit camera to the 2x2x2 bounding box (center = 1,1,1, radius ~= sqrt(3))
        app.GetCamera().FitToSphere({1.f, 1.f, 1.f}, 1.74f);

        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

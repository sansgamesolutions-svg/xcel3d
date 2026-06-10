#include "Common/ThreadPool.h"
#include "Kernel/ColorTable.h"
#include "Kernel/CoordTable.h"
#include "Kernel/MeshTessellator.h"
#include "Kernel/PrimitiveSet.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/TessellationStrategy.h"
#include <glm/glm.hpp>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void TestMergeUsesScalarOffsetsAndRebasesIndices()
{
    xcel::CoordTable coords({
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    });

    xcel::TrianglePrimitiveSet first;
    xcel::TrianglePrimitiveSet second;
    first.AddElement({0, 1, 2});
    second.AddElement({0, 1, 2});

    xcel::ElementScalarTable scalars({0.0f, 1.0f});
    xcel::MeshColor colors;
    colors.AddColor({1.0f, 0.0f, 0.0f});
    colors.AddColor({0.0f, 1.0f, 0.0f});

    const xcel::MeshTessellationInput inputs[] = {
        {&first, &coords, &scalars, &colors, nullptr, 0},
        {&second, &coords, &scalars, &colors, nullptr, 1},
    };

    const xcel::TessellatedMesh mesh = xcel::TessellateAndMerge(inputs);

    Require(mesh.vertices.size() == 6, "Expected two tessellated triangles");
    Require(mesh.indices.size() == 6, "Expected six merged indices");
    Require(mesh.vertices[0].color == glm::vec3(1.0f, 0.0f, 0.0f),
            "First primitive set used the wrong color");
    Require(mesh.vertices[3].color == glm::vec3(0.0f, 1.0f, 0.0f),
            "Second primitive set ignored its scalar offset");
    Require(mesh.indices[3] == 3 && mesh.indices[5] == 5,
            "Merged indices were not rebased");
}

void TestThreadPoolPropagatesTaskExceptions()
{
    xcel::ThreadPool pool(1);
    auto future = pool.Submit([]() -> int {
        throw std::runtime_error("expected task failure");
    });

    bool exceptionObserved = false;
    try {
        static_cast<void>(future.get());
    } catch (const std::runtime_error&) {
        exceptionObserved = true;
    }
    Require(exceptionObserved, "Worker exception was not propagated through the future");
}

} // namespace

int main()
{
    TestMergeUsesScalarOffsetsAndRebasesIndices();
    TestThreadPoolPropagatesTaskExceptions();
    return 0;
}

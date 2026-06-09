#include "Common/ThreadPool.h"
#include "Graphics/ColorTable.h"
#include "Graphics/CoordTable.h"
#include "Graphics/MeshTessellator.h"
#include "Graphics/PrimitiveSet.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/TessellationStrategy.h"
#include "IO/Core/IStreamSink.h"
#include "IO/Core/MemoryStreamSource.h"
#include "IO/Core/SceneBuilder.h"
#include "IO/Formats/XcelFormat.h"
#include "IO/Scene/MeshData.h"
#include "IO/Scene/SceneDocument.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

class MemoryStreamSink final : public xcel::io::IStreamSink
{
public:
    void Write(const std::byte* buf, size_t size) override
    {
        if (m_position + size > m_data.size())
            m_data.resize(m_position + size);
        std::copy_n(buf, size, m_data.data() + m_position);
        m_position += size;
    }

    void Seek(uint64_t offset) override
    {
        if (offset > m_data.size())
            throw std::out_of_range("MemoryStreamSink::Seek past end");
        m_position = static_cast<size_t>(offset);
    }

    uint64_t Tell() const override
    {
        return m_position;
    }

    void Flush() override {}

    const std::vector<std::byte>& Data() const
    {
        return m_data;
    }

private:
    std::vector<std::byte> m_data;
    size_t                 m_position = 0;
};

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void TestThreadPoolExceptionPropagation()
{
    xcel::ThreadPool pool(1);
    auto future = pool.Submit([]() -> int {
        throw std::runtime_error("expected");
    });

    bool caught = false;
    try {
        (void)future.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Require(caught, "ThreadPool did not propagate the task exception");
}

void TestXcelRoundTrip()
{
    xcel::io::MeshData mesh;
    mesh.name = "all-primitives";
    mesh.coords = std::make_shared<xcel::CoordTable>();
    for (uint32_t i = 0; i < 8; ++i) {
        mesh.coords->AddCoord({
            static_cast<float>(i & 1u),
            static_cast<float>((i >> 1u) & 1u),
            static_cast<float>((i >> 2u) & 1u)});
    }
    mesh.scalars = std::make_shared<xcel::ConstantScalarTable>(1.f, 6);
    mesh.colorTable = std::make_shared<xcel::PaletteColor>();

    auto line = std::make_shared<xcel::LinePrimitiveSet>();
    line->AddElement({0, 1});
    mesh.primSets.push_back(line);

    auto polyline = std::make_shared<xcel::PolylinePrimitiveSet>();
    polyline->AddElement({0, 1, 3, 2});
    mesh.primSets.push_back(polyline);

    auto triangle = std::make_shared<xcel::TrianglePrimitiveSet>();
    triangle->AddElement({0, 1, 2});
    mesh.primSets.push_back(triangle);

    auto quad = std::make_shared<xcel::QuadPrimitiveSet>();
    quad->AddElement({0, 1, 3, 2});
    mesh.primSets.push_back(quad);

    auto tet = std::make_shared<xcel::TetPrimitiveSet>();
    tet->AddElement({0, 1, 2, 4});
    mesh.primSets.push_back(tet);

    auto hex = std::make_shared<xcel::HexPrimitiveSet>();
    hex->AddElement({0, 1, 3, 2, 4, 5, 7, 6});
    mesh.primSets.push_back(hex);

    xcel::io::SceneDocument sourceDocument;
    sourceDocument.AddMesh(std::move(mesh));

    MemoryStreamSink sink;
    xcel::io::XcelFormatWriter writer;
    writer.Write(sourceDocument, sink);

    xcel::io::MemoryStreamSource source(sink.Data());
    xcel::io::SceneBuilder builder;
    xcel::io::XcelFormatReader reader;
    reader.Read(source, builder, nullptr);
    const auto loadedDocument = builder.Build();

    Require(loadedDocument->MeshCount() == 1, "Round-trip mesh count mismatch");
    const auto& loadedMesh = loadedDocument->Mesh(0);
    Require(loadedMesh.name == "all-primitives", "Round-trip mesh name mismatch");
    Require(loadedMesh.coords->Size() == 8, "Round-trip coordinate count mismatch");
    Require(loadedMesh.primSets.size() == 6, "Round-trip primitive set count mismatch");

    const std::array<xcel::PrimitiveType, 6> expectedTypes = {
        xcel::PrimitiveType::PT_LINE,
        xcel::PrimitiveType::PT_POLYLINE,
        xcel::PrimitiveType::PT_TRIANGLE,
        xcel::PrimitiveType::PT_QUAD,
        xcel::PrimitiveType::PT_TETRAHEDRON,
        xcel::PrimitiveType::PT_HEXAHEDRON};

    for (size_t i = 0; i < expectedTypes.size(); ++i) {
        Require(loadedMesh.primSets[i]->Type() == expectedTypes[i],
                "Round-trip primitive type mismatch");
        Require(loadedMesh.primSets[i]->ElementCount() == 1,
                "Round-trip primitive count mismatch");
    }
}

void TestTessellationOffsetAndTransform()
{
    xcel::CoordTable coords;
    coords.AddCoord({0.f, 0.f, 0.f});
    coords.AddCoord({1.f, 0.f, 0.f});
    coords.AddCoord({0.f, 1.f, 0.f});

    xcel::TrianglePrimitiveSet triangles;
    triangles.AddElement({0, 1, 2});

    xcel::ElementScalarTable scalars({0.f, 10.f});
    xcel::PaletteColor colors;
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.f), glm::vec3(5.f, 0.f, 0.f));

    xcel::MeshTessellationInput input{
        &triangles, &coords, &scalars, &colors, nullptr, 1, transform};
    const xcel::TessellatedMesh mesh = xcel::TessellateInput(input);

    Require(mesh.vertices.size() == 3, "Tessellation vertex count mismatch");
    Require(mesh.vertices[0].position.x == 5.f,
            "Tessellation transform was not applied");
    Require(mesh.vertices[0].color.r == 1.f
            && mesh.vertices[0].color.g == 0.f
            && mesh.vertices[0].color.b == 0.f,
            "Tessellation scalar offset was not applied");
}

} // namespace

int main()
{
    try {
        TestThreadPoolExceptionPropagation();
        TestXcelRoundTrip();
        TestTessellationOffsetAndTransform();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

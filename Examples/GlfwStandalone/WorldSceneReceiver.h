#pragma once
#include "IO/Core/ISceneReceiver.h"
#include "Renderer/World.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/PrimitiveSet.h"
#include <stdexcept>

// Bridges ISceneReceiver → World::AddMesh.
// All persistent polymorphic Kernel objects are created here (in the exe),
// so their vtables never live inside a plugin DLL.
class WorldSceneReceiver : public xcel::io::ISceneReceiver
{
public:
    explicit WorldSceneReceiver(xcel::World& world) : m_world(world) {}

    void ReceiveMesh(
        std::string_view             name,
        std::span<const float>       positions,
        xcel::PrimitiveType          primType,
        std::span<const uint32_t>    indices,
        uint32_t                     indicesPerElement,
        std::span<const float>       scalars) override
    {
        if (positions.empty() || indices.empty()) return;

        // ── Coordinates ─────────────────────────────────────────────────────
        auto coords = std::make_shared<xcel::CoordTable>();
        const size_t vertexCount = positions.size() / 3;
        coords->Reserve(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i)
            coords->AddCoord({positions[i*3], positions[i*3+1], positions[i*3+2]});

        // ── Primitive set ────────────────────────────────────────────────────
        const size_t elemCount = indices.size() / indicesPerElement;
        std::vector<std::shared_ptr<xcel::PrimitiveSet>> primSets;

        switch (primType)
        {
        case xcel::PrimitiveType::PT_TRIANGLE:
        {
            auto ps = std::make_shared<xcel::TrianglePrimitiveSet>();
            for (size_t e = 0; e < elemCount; ++e)
            {
                const uint32_t* base = indices.data() + e * 3;
                ps->AddElement({base[0], base[1], base[2]});
            }
            primSets.push_back(std::move(ps));
            break;
        }
        case xcel::PrimitiveType::PT_HEXAHEDRON:
        {
            auto ps = std::make_shared<xcel::HexPrimitiveSet>();
            for (size_t e = 0; e < elemCount; ++e)
            {
                const uint32_t* base = indices.data() + e * 8;
                ps->AddElement({base[0], base[1], base[2], base[3],
                                 base[4], base[5], base[6], base[7]});
            }
            primSets.push_back(std::move(ps));
            break;
        }
        case xcel::PrimitiveType::PT_TETRAHEDRON:
        {
            auto ps = std::make_shared<xcel::TetPrimitiveSet>();
            for (size_t e = 0; e < elemCount; ++e)
            {
                const uint32_t* base = indices.data() + e * 4;
                ps->AddElement({base[0], base[1], base[2], base[3]});
            }
            primSets.push_back(std::move(ps));
            break;
        }
        case xcel::PrimitiveType::PT_QUAD:
        {
            auto ps = std::make_shared<xcel::QuadPrimitiveSet>();
            for (size_t e = 0; e < elemCount; ++e)
            {
                const uint32_t* base = indices.data() + e * 4;
                ps->AddElement({base[0], base[1], base[2], base[3]});
            }
            primSets.push_back(std::move(ps));
            break;
        }
        case xcel::PrimitiveType::PT_LINE:
        {
            auto ps = std::make_shared<xcel::LinePrimitiveSet>();
            for (size_t e = 0; e < elemCount; ++e)
            {
                const uint32_t* base = indices.data() + e * 2;
                ps->AddElement({base[0], base[1]});
            }
            primSets.push_back(std::move(ps));
            break;
        }
        default:
            return; // unsupported type — skip silently
        }

        // ── Scalar table ─────────────────────────────────────────────────────
        std::shared_ptr<xcel::ScalarTable> scalarTable;
        if (scalars.empty())
        {
            scalarTable = std::make_shared<xcel::ConstantScalarTable>(0.f, elemCount);
        }
        else
        {
            auto et = std::make_shared<xcel::ElementScalarTable>();
            for (float v : scalars) et->AddScalar(v);
            scalarTable = std::move(et);
        }

        m_world.AddMesh(std::string(name), coords, scalarTable,
                        std::make_shared<xcel::PaletteColor>(),
                        std::move(primSets));
    }

private:
    xcel::World& m_world;
};

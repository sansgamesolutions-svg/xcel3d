#include "IO/Formats/XcelFormat.h"
#include "IO/Core/IStreamSource.h"
#include "IO/Core/IStreamSink.h"
#include "IO/Core/SceneBuilder.h"
#include "IO/Scene/SceneDocument.h"
#include "IO/Scene/ChunkDescriptor.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include "Graphics/PrimitiveSet.h"
#include "Common/ThreadPool.h"
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace xcel::io {

// ── Binary helpers ──────────────────────────────────────────────────────────

namespace {

constexpr std::array<char, 4> kMagic = {'X', 'C', 'E', 'L'};
constexpr uint32_t            kVersion = 1;
constexpr uint32_t            kMaxTocEntries = 1'000'000;

void ReadExact(IStreamSource& s, std::byte* data, size_t size)
{
    if (s.Read(data, size) != size)
        throw std::runtime_error("Xcel format: unexpected end of stream");
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
void WriteT(IStreamSink& s, const T& v)
{
    s.Write(reinterpret_cast<const std::byte*>(&v), sizeof(T));
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
T ReadT(IStreamSource& s)
{
    T v{};
    ReadExact(s, reinterpret_cast<std::byte*>(&v), sizeof(T));
    return v;
}

void WriteStr(IStreamSink& s, const std::string& str)
{
    if (str.size() > std::numeric_limits<uint16_t>::max())
        throw std::length_error("Xcel format: string exceeds 65535 bytes");
    uint16_t len = static_cast<uint16_t>(str.size());
    WriteT(s, len);
    s.Write(reinterpret_cast<const std::byte*>(str.data()), len);
}

std::string ReadStr(IStreamSource& s)
{
    uint16_t len = ReadT<uint16_t>(s);
    std::string str(len, '\0');
    ReadExact(s, reinterpret_cast<std::byte*>(str.data()), len);
    return str;
}

void Align8(IStreamSink& s)
{
    uint64_t pos = s.Tell();
    uint64_t pad = (8 - (pos % 8)) % 8;
    static const std::byte kZero{};
    for (uint64_t i = 0; i < pad; ++i) s.Write(&kZero, 1);
}

// ── Mesh chunk ──────────────────────────────────────────────────────────────
// Layout: name | coordCount(u32) | coords(3*f32 each)
//         | primSetCount(u32) | for each: type(u8) | elemCount(u32) | indices...

void WriteMeshChunk(IStreamSink& s, const MeshData& mesh)
{
    WriteStr(s, mesh.name);
    uint32_t nCoords = static_cast<uint32_t>(mesh.coords->Size());
    WriteT(s, nCoords);
    for (size_t i = 0; i < nCoords; ++i)
    {
        const glm::vec3& v = (*mesh.coords)[i];
        WriteT(s, v.x); WriteT(s, v.y); WriteT(s, v.z);
    }
    uint32_t nPrimSets = static_cast<uint32_t>(mesh.primSets.size());
    WriteT(s, nPrimSets);
    for (auto& ps : mesh.primSets)
    {
        WriteT(s, static_cast<uint8_t>(ps->Type()));
        uint32_t nElems = static_cast<uint32_t>(ps->ElementCount());
        WriteT(s, nElems);

        auto writeFixed = [&]<typename SetT>(const SetT& set)
            requires requires { set.Elements(); }
        {
            for (const auto& elem : set.Elements())
                for (uint32_t idx : elem)
                    WriteT(s, idx);
        };

        switch (ps->Type())
        {
        case PrimitiveType::PT_HEXAHEDRON:
            writeFixed(static_cast<const HexPrimitiveSet&>(*ps)); break;
        case PrimitiveType::PT_TETRAHEDRON:
            writeFixed(static_cast<const TetPrimitiveSet&>(*ps)); break;
        case PrimitiveType::PT_QUAD:
            writeFixed(static_cast<const QuadPrimitiveSet&>(*ps)); break;
        case PrimitiveType::PT_TRIANGLE:
            writeFixed(static_cast<const TrianglePrimitiveSet&>(*ps)); break;
        case PrimitiveType::PT_LINE:
            writeFixed(static_cast<const LinePrimitiveSet&>(*ps)); break;
        case PrimitiveType::PT_POLYLINE:
        {
            const auto& set = static_cast<const PolylinePrimitiveSet&>(*ps);
            for (const auto& elem : set.Elements()) {
                if (elem.size() > std::numeric_limits<uint32_t>::max())
                    throw std::length_error("Xcel format: polyline is too large");
                WriteT(s, static_cast<uint32_t>(elem.size()));
                for (uint32_t idx : elem)
                    WriteT(s, idx);
            }
            break;
        }
        }
    }
}

MeshData ReadMeshChunk(IStreamSource& s)
{
    MeshData mesh;
    mesh.name         = ReadStr(s);
    mesh.coords       = std::make_shared<xcel::CoordTable>();
    mesh.colorTable   = std::make_shared<xcel::PaletteColor>();

    uint32_t nCoords = ReadT<uint32_t>(s);
    mesh.coords->Reserve(nCoords);
    for (uint32_t i = 0; i < nCoords; ++i)
    {
        float x = ReadT<float>(s), y = ReadT<float>(s), z = ReadT<float>(s);
        mesh.coords->AddCoord({x, y, z});
    }

    uint32_t nPrimSets = ReadT<uint32_t>(s);
    for (uint32_t p = 0; p < nPrimSets; ++p)
    {
        auto type   = static_cast<PrimitiveType>(ReadT<uint8_t>(s));
        uint32_t nE = ReadT<uint32_t>(s);
        switch (type)
        {
        case PrimitiveType::PT_HEXAHEDRON:
        {
            auto hs = std::make_shared<HexPrimitiveSet>();
            for (uint32_t e = 0; e < nE; ++e)
            {
                std::array<uint32_t, 8> idx{};
                for (auto& i : idx) i = ReadT<uint32_t>(s);
                hs->AddElement(idx);
            }
            mesh.primSets.push_back(std::move(hs));
            break;
        }
        case PrimitiveType::PT_TETRAHEDRON:
        {
            auto set = std::make_shared<TetPrimitiveSet>();
            for (uint32_t e = 0; e < nE; ++e) {
                std::array<uint32_t, 4> idx{};
                for (auto& i : idx) i = ReadT<uint32_t>(s);
                set->AddElement(idx);
            }
            mesh.primSets.push_back(std::move(set));
            break;
        }
        case PrimitiveType::PT_QUAD:
        {
            auto set = std::make_shared<QuadPrimitiveSet>();
            for (uint32_t e = 0; e < nE; ++e) {
                std::array<uint32_t, 4> idx{};
                for (auto& i : idx) i = ReadT<uint32_t>(s);
                set->AddElement(idx);
            }
            mesh.primSets.push_back(std::move(set));
            break;
        }
        case PrimitiveType::PT_TRIANGLE:
        {
            auto ts = std::make_shared<TrianglePrimitiveSet>();
            for (uint32_t e = 0; e < nE; ++e)
            {
                std::array<uint32_t, 3> idx{};
                for (auto& i : idx) i = ReadT<uint32_t>(s);
                ts->AddElement(idx);
            }
            mesh.primSets.push_back(std::move(ts));
            break;
        }
        case PrimitiveType::PT_LINE:
        {
            auto set = std::make_shared<LinePrimitiveSet>();
            for (uint32_t e = 0; e < nE; ++e) {
                std::array<uint32_t, 2> idx{};
                for (auto& i : idx) i = ReadT<uint32_t>(s);
                set->AddElement(idx);
            }
            mesh.primSets.push_back(std::move(set));
            break;
        }
        case PrimitiveType::PT_POLYLINE:
        {
            auto set = std::make_shared<PolylinePrimitiveSet>();
            for (uint32_t e = 0; e < nE; ++e) {
                uint32_t nodeCount = ReadT<uint32_t>(s);
                std::vector<uint32_t> idx(nodeCount);
                for (auto& i : idx) i = ReadT<uint32_t>(s);
                set->AddElement(idx);
            }
            mesh.primSets.push_back(std::move(set));
            break;
        }
        default:
            throw std::runtime_error("Xcel format: unknown primitive type");
        }
    }

    // Scalar table: constant zero until an animation frame overrides it.
    size_t totalElems = 0;
    for (auto& ps : mesh.primSets) totalElems += ps->ElementCount();
    mesh.scalars = std::make_shared<xcel::ConstantScalarTable>(0.f, totalElems);

    return mesh;
}

} // namespace

// ── XcelFormatReader ────────────────────────────────────────────────────────

bool XcelFormatReader::CanRead(std::string_view extension) const
{
    return extension == "xcel";
}

void XcelFormatReader::Read(IStreamSource& source, SceneBuilder& out,
                             xcel::ThreadPool* pool)
{
    // Validate header.
    std::array<char, 4> magic{};
    ReadExact(source, reinterpret_cast<std::byte*>(magic.data()), magic.size());
    if (magic != kMagic)
        throw std::runtime_error("XcelFormatReader: invalid magic bytes");

    uint32_t version   = ReadT<uint32_t>(source);
    uint64_t tocOffset = ReadT<uint64_t>(source);
    if (version != kVersion)
        throw std::runtime_error("XcelFormatReader: unsupported version");
    if (tocOffset > source.Size() || source.Size() - tocOffset < sizeof(uint32_t))
        throw std::runtime_error("XcelFormatReader: invalid TOC offset");

    // Read TOC.
    source.Seek(tocOffset);
    uint32_t tocCount = ReadT<uint32_t>(source);
    if (tocCount > kMaxTocEntries)
        throw std::runtime_error("XcelFormatReader: unreasonable TOC size");
    std::vector<ChunkDescriptor> toc(tocCount);
    for (auto& entry : toc)
    {
        entry.type   = static_cast<ChunkType>(ReadT<uint8_t>(source));
        entry.id     = ReadT<uint32_t>(source);
        entry.flags  = ReadT<uint32_t>(source);
        entry.offset = ReadT<uint64_t>(source);
        entry.size   = ReadT<uint64_t>(source);
        entry.name   = ReadStr(source);
        if (entry.offset > source.Size() || entry.size > source.Size() - entry.offset)
            throw std::runtime_error("XcelFormatReader: chunk is outside the stream");
    }

    // A generic IStreamSource exposes one seek cursor, so decoding is ordered.
    // File-level parallelism belongs above this API, where independent sources
    // can be created safely.
    (void)pool;
    for (const auto& entry : toc)
    {
        if (entry.type != ChunkType::Mesh) continue;
        source.Seek(entry.offset);
        out.AddMesh(ReadMeshChunk(source));
    }

    // Register animation frame chunk offsets (lazy — don't decode now).
    for (auto& entry : toc)
    {
        if (entry.type != ChunkType::AnimFrame) continue;
        // entry.flags holds trackId in upper 16 bits, frameIndex in lower 16.
        uint32_t trackId    = (entry.flags >> 16) & 0xFFFF;
        uint32_t frameIndex = entry.flags & 0xFFFF;
        out.AddAnimationFrameAtOffset(trackId, frameIndex,
                                      entry.offset, entry.size);
    }
}

// ── XcelFormatWriter ────────────────────────────────────────────────────────

bool XcelFormatWriter::CanWrite(std::string_view extension) const
{
    return extension == "xcel";
}

void XcelFormatWriter::Write(const SceneDocument& doc, IStreamSink& sink)
{
    // Reserve space for the fixed header (we'll patch tocOffset after writing data).
    sink.Write(reinterpret_cast<const std::byte*>(kMagic.data()), 4);
    WriteT(sink, kVersion);
    uint64_t tocOffsetPlaceholder = 0;
    uint64_t tocOffsetPos         = sink.Tell();
    WriteT(sink, tocOffsetPlaceholder); // patched after chunks

    std::vector<ChunkDescriptor> toc;

    // Write mesh chunks.
    for (size_t i = 0; i < doc.MeshCount(); ++i)
    {
        Align8(sink);
        ChunkDescriptor desc;
        desc.type   = ChunkType::Mesh;
        desc.id     = static_cast<uint32_t>(i);
        desc.name   = doc.Mesh(i).name;
        desc.offset = sink.Tell();
        WriteMeshChunk(sink, doc.Mesh(i));
        desc.size   = sink.Tell() - desc.offset;
        toc.push_back(desc);
    }

    // Write TOC.
    Align8(sink);
    uint64_t tocOffset = sink.Tell();
    WriteT(sink, static_cast<uint32_t>(toc.size()));
    for (auto& entry : toc)
    {
        WriteT(sink, static_cast<uint8_t>(entry.type));
        WriteT(sink, entry.id);
        WriteT(sink, entry.flags);
        WriteT(sink, entry.offset);
        WriteT(sink, entry.size);
        WriteStr(sink, entry.name);
    }
    const uint64_t endOffset = sink.Tell();
    sink.Seek(tocOffsetPos);
    WriteT(sink, tocOffset);
    sink.Seek(endOffset);
    sink.Flush();
}

} // namespace xcel::io

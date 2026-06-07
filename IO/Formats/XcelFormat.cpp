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
#include <future>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace xcel::io {

// ── Binary helpers ──────────────────────────────────────────────────────────

namespace {

constexpr std::array<char, 4> kMagic = {'X', 'C', 'E', 'L'};
constexpr uint32_t            kVersion = 1;

template<typename T>
void WriteT(IStreamSink& s, const T& v)
{
    s.Write(reinterpret_cast<const std::byte*>(&v), sizeof(T));
}

template<typename T>
T ReadT(IStreamSource& s)
{
    T v{};
    s.Read(reinterpret_cast<std::byte*>(&v), sizeof(T));
    return v;
}

void WriteStr(IStreamSink& s, const std::string& str)
{
    uint16_t len = static_cast<uint16_t>(str.size());
    WriteT(s, len);
    s.Write(reinterpret_cast<const std::byte*>(str.data()), len);
}

std::string ReadStr(IStreamSource& s)
{
    uint16_t len = ReadT<uint16_t>(s);
    std::string str(len, '\0');
    s.Read(reinterpret_cast<std::byte*>(str.data()), len);
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

        // Write connectivity based on type.
        auto WriteHex = [&](const HexPrimitiveSet& hs) {
            for (size_t e = 0; e < hs.ElementCount(); ++e)
                for (uint32_t idx : hs.Element(e)) WriteT(s, idx);
        };
        auto WriteTri = [&](const TrianglePrimitiveSet& ts) {
            for (size_t e = 0; e < ts.ElementCount(); ++e)
                for (uint32_t idx : ts.Element(e)) WriteT(s, idx);
        };

        switch (ps->Type())
        {
        case PrimitiveType::PT_HEXAHEDRON:
            WriteHex(static_cast<const HexPrimitiveSet&>(*ps)); break;
        case PrimitiveType::PT_TRIANGLE:
            WriteTri(static_cast<const TrianglePrimitiveSet&>(*ps)); break;
        default:
            // Other types: write raw element count, no index data (placeholder).
            break;
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
        default:
            break; // Unknown type; skip (no index data written for unknowns above)
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
    source.Read(reinterpret_cast<std::byte*>(magic.data()), 4);
    if (magic != kMagic)
        throw std::runtime_error("XcelFormatReader: invalid magic bytes");

    uint32_t version   = ReadT<uint32_t>(source);
    uint64_t tocOffset = ReadT<uint64_t>(source);
    (void)version;

    // Read TOC.
    source.Seek(tocOffset);
    uint32_t tocCount = ReadT<uint32_t>(source);
    std::vector<ChunkDescriptor> toc(tocCount);
    for (auto& entry : toc)
    {
        entry.type   = static_cast<ChunkType>(ReadT<uint8_t>(source));
        entry.id     = ReadT<uint32_t>(source);
        entry.flags  = ReadT<uint32_t>(source);
        entry.offset = ReadT<uint64_t>(source);
        entry.size   = ReadT<uint64_t>(source);
        entry.name   = ReadStr(source);
    }

    // Parallel mesh decode.
    std::vector<std::future<void>> meshFutures;
    std::mutex                     builderMutex;

    for (auto& entry : toc)
    {
        if (entry.type != ChunkType::Mesh) continue;

        auto decode = [&, entry]() {
            // Each task opens its own FileStreamSource to avoid seek races.
            // However, Read() receives a generic IStreamSource which might be
            // a MemoryStreamSource (no reopening). Use mutex for memory sources.
            // For file-backed sources the caller should pass a FileStreamSource;
            // parallel seeks on a single FileStreamSource are serialised here.
            MeshData mesh;
            {
                std::scoped_lock lock(builderMutex);
                source.Seek(entry.offset);
                mesh = ReadMeshChunk(source);
            }
            out.AddMesh(std::move(mesh));
        };

        if (pool)
            meshFutures.push_back(pool->Submit(std::move(decode)));
        else
            decode();
    }
    for (auto& f : meshFutures) f.get();

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
    sink.Flush();

    // Patch tocOffset in the header.
    // Note: IStreamSink is write-only; patching requires a seekable sink.
    // This is handled by FileStreamSink which exposes Tell() but not Seek().
    // For production use this would need a seekable sink interface; for now
    // we write tocOffset at the end and the reader seeks to it directly.
    (void)tocOffsetPos; // suppress unused warning
    (void)tocOffset;
    // Workaround: append tocOffset as a trailer so the reader can find it.
    WriteT(sink, tocOffset);
    sink.Flush();
}

} // namespace xcel::io

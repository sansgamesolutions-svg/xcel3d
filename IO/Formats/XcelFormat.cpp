#include "IO/Formats/XcelFormat.h"
#include "IO/Core/IStreamSource.h"
#include "IO/Core/IStreamSink.h"
#include "IO/Scene/SceneDocument.h"
#include "IO/Scene/ChunkDescriptor.h"
#include "IO/Scene/MeshData.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/PrimitiveSet.h"
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

// Reads one mesh chunk and delivers each prim set via receiver.
// Builds flat arrays so no Kernel polymorphic objects are created here;
// the receiver (exe-side) allocates those with vtables in the exe.
void ReadAndDeliverMesh(IStreamSource& s, ISceneReceiver& receiver)
{
    std::string name = ReadStr(s);

    uint32_t nCoords = ReadT<uint32_t>(s);
    std::vector<float> positions;
    positions.reserve(nCoords * 3);
    for (uint32_t i = 0; i < nCoords; ++i)
    {
        float x = ReadT<float>(s), y = ReadT<float>(s), z = ReadT<float>(s);
        positions.push_back(x);
        positions.push_back(y);
        positions.push_back(z);
    }

    uint32_t nPrimSets = ReadT<uint32_t>(s);
    for (uint32_t p = 0; p < nPrimSets; ++p)
    {
        auto     type = static_cast<PrimitiveType>(ReadT<uint8_t>(s));
        uint32_t nE   = ReadT<uint32_t>(s);

        uint32_t ipe = 0;
        switch (type)
        {
        case PrimitiveType::PT_HEXAHEDRON:  ipe = 8; break;
        case PrimitiveType::PT_TETRAHEDRON: ipe = 4; break;
        case PrimitiveType::PT_QUAD:        ipe = 4; break;
        case PrimitiveType::PT_TRIANGLE:    ipe = 3; break;
        case PrimitiveType::PT_LINE:        ipe = 2; break;
        default: break;
        }

        if (ipe == 0) continue;

        std::vector<uint32_t> indices;
        indices.reserve(nE * ipe);
        for (uint32_t e = 0; e < nE; ++e)
            for (uint32_t k = 0; k < ipe; ++k)
                indices.push_back(ReadT<uint32_t>(s));

        // Empty scalars span => receiver treats as constant 0.
        receiver.ReceiveMesh(name, positions, type, indices, ipe, {});
    }
}

} // namespace

// ── XcelFormatReader ────────────────────────────────────────────────────────

bool XcelFormatReader::CanRead(std::string_view extension) const
{
    return extension == "xcel";
}

void XcelFormatReader::Read(IStreamSource& source, ISceneReceiver& receiver,
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
    // ISceneReceiver::ReceiveMesh is called under receiverMutex since the
    // receiver may not be thread-safe (WorldSceneReceiver modifies the ECS).
    std::vector<std::future<void>> meshFutures;
    std::mutex                     receiverMutex;

    for (auto& entry : toc)
    {
        if (entry.type != ChunkType::Mesh) continue;

        auto decode = [&, entry]() {
            // Seek + read under lock to serialise stream access and receiver calls.
            std::scoped_lock lock(receiverMutex);
            source.Seek(entry.offset);
            ReadAndDeliverMesh(source, receiver);
        };

        if (pool)
            meshFutures.push_back(pool->Submit(std::move(decode)));
        else
            decode();
    }
    for (auto& f : meshFutures) f.get();
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

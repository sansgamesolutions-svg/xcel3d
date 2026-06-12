#pragma once
#include "IO/Core/IFormatReader.h"
#include "IO/Core/IFormatWriter.h"
#include "IO/Core/IStreamSink.h"

namespace xcel::io {

// Native binary .xcel reader/writer.
// On-disk layout:
//   [FileHeader]  magic("XCEL") + version(u32) + tocOffset(u64)
//   [Chunks]      MeshChunk, SkeletonChunk, SceneGraphChunk,
//                 AnimCatalogueChunk, AnimFrameChunk (8-byte aligned)
//   [TOC]         TOCHeader(count u32) + TOCEntry[N]
class XcelFormatReader final : public IFormatReader
{
public:
    bool CanRead(std::string_view extension) const override;
    void Read(IStreamSource& source, ISceneReceiver& receiver,
              xcel::ThreadPool* pool) override;
};

class XcelFormatWriter final : public IFormatWriter
{
public:
    bool CanWrite(std::string_view extension) const override;
    void Write(const SceneDocument& doc, IStreamSink& sink) override;
};

} // namespace xcel::io

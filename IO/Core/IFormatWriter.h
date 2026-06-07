#pragma once
#include <string_view>

namespace xcel::io {

class IStreamSink;
class SceneDocument;

class IFormatWriter
{
public:
    virtual ~IFormatWriter() = default;

    // Returns true if this writer handles `extension`.
    virtual bool CanWrite(std::string_view extension) const = 0;

    // Serialises `doc` into `sink`.
    virtual void Write(const SceneDocument& doc, IStreamSink& sink) = 0;
};

} // namespace xcel::io

#pragma once
#include <string_view>

namespace xcel {
class ThreadPool;
}

namespace xcel::io {

class IStreamSource;
class SceneBuilder;

class IFormatReader
{
public:
    virtual ~IFormatReader() = default;

    // Returns true if this reader handles `extension` (e.g. "fbx", "gltf").
    // Extension is lower-case, without a leading dot.
    virtual bool CanRead(std::string_view extension) const = 0;

    // Parses `source` and populates `out`.
    // May submit parallel sub-tasks to `pool` (nullable — sync fallback when null).
    virtual void Read(IStreamSource& source, SceneBuilder& out,
                      xcel::ThreadPool* pool) = 0;
};

} // namespace xcel::io

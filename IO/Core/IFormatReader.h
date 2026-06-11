#pragma once
#include "IO/Core/ISceneReceiver.h"
#include <string_view>

namespace xcel {
class ThreadPool;
}

namespace xcel::io {

class IStreamSource;

class IFormatReader
{
public:
    virtual ~IFormatReader() = default;

    // Returns true if this reader handles `extension` (e.g. "fbx", "gltf").
    // Extension is lower-case, without a leading dot.
    virtual bool CanRead(std::string_view extension) const = 0;

    // Parses `source` and delivers data via `receiver`.
    // May submit parallel sub-tasks to `pool` (nullable — sync fallback when null).
    // `receiver` must remain alive for the duration of this call (and any sub-tasks).
    virtual void Read(IStreamSource& source, ISceneReceiver& receiver,
                      xcel::ThreadPool* pool) = 0;
};

} // namespace xcel::io

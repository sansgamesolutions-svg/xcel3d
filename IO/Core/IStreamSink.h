#pragma once
#include <cstddef>
#include <cstdint>

namespace xcel::io {

class IStreamSink
{
public:
    virtual ~IStreamSink() = default;

    virtual void     Write(const std::byte* buf, size_t n) = 0;
    virtual void     Seek(uint64_t offset)                 = 0;
    virtual uint64_t Tell()                          const = 0;
    virtual void     Flush()                               = 0;
};

} // namespace xcel::io

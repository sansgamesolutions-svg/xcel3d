#pragma once
#include <cstddef>
#include <cstdint>

namespace xcel::io {

class IStreamSource
{
public:
    virtual ~IStreamSource() = default;

    virtual size_t   Read(std::byte* buf, size_t n) = 0;
    virtual void     Seek(uint64_t offset)           = 0;
    virtual uint64_t Tell()                    const = 0;
    virtual uint64_t Size()                    const = 0;

    bool AtEnd() const { return Tell() >= Size(); }
};

} // namespace xcel::io

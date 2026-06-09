#pragma once
#include "IO/Core/IStreamSource.h"
#include <algorithm>
#include <span>
#include <stdexcept>
#include <vector>

namespace xcel::io {

// Non-owning view over a buffer already in memory (e.g. data pushed from a
// simulation app via shared memory). The caller must keep the buffer alive.
class MemoryStreamSource final : public IStreamSource
{
public:
    explicit MemoryStreamSource(std::span<const std::byte> data)
        : m_data(data) {}

    // Convenience constructor from a byte vector
    explicit MemoryStreamSource(const std::vector<std::byte>& data)
        : m_data(data) {}

    size_t Read(std::byte* buf, size_t n) override
    {
        if (m_pos > m_data.size())
            throw std::runtime_error("MemoryStreamSource position is invalid");
        size_t remaining = m_data.size() - m_pos;
        size_t toRead    = (n < remaining) ? n : remaining;
        std::copy(m_data.data() + m_pos, m_data.data() + m_pos + toRead, buf);
        m_pos += toRead;
        return toRead;
    }

    void Seek(uint64_t offset) override
    {
        if (offset > m_data.size())
            throw std::out_of_range("MemoryStreamSource::Seek past end of stream");
        m_pos = static_cast<size_t>(offset);
    }
    uint64_t Tell()          const override { return m_pos; }
    uint64_t Size()          const override { return m_data.size(); }

private:
    std::span<const std::byte> m_data;
    size_t                     m_pos = 0;
};

} // namespace xcel::io

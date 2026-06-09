#pragma once
#include "IO/Core/IStreamSink.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace xcel::io {

class FileStreamSink final : public IStreamSink
{
public:
    explicit FileStreamSink(const std::filesystem::path& path)
        : m_stream(path, std::ios::binary | std::ios::trunc)
    {
        if (!m_stream)
            throw std::runtime_error("Cannot open file for writing: " + path.string());
    }

    void Write(const std::byte* buf, size_t n) override
    {
        m_stream.write(reinterpret_cast<const char*>(buf), static_cast<std::streamsize>(n));
        if (!m_stream)
            throw std::runtime_error("Failed to write output stream");
    }

    void Seek(uint64_t offset) override
    {
        m_stream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!m_stream)
            throw std::runtime_error("Failed to seek output stream");
    }

    uint64_t Tell() const override
    {
        return static_cast<uint64_t>(const_cast<std::ofstream&>(m_stream).tellp());
    }

    void Flush() override
    {
        m_stream.flush();
        if (!m_stream)
            throw std::runtime_error("Failed to flush output stream");
    }

private:
    std::ofstream m_stream;
};

} // namespace xcel::io

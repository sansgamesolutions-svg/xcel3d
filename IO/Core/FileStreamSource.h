#pragma once
#include "IO/Core/IStreamSource.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace xcel::io {

class FileStreamSource final : public IStreamSource
{
public:
    explicit FileStreamSource(const std::filesystem::path& path)
        : m_stream(path, std::ios::binary)
    {
        if (!m_stream)
            throw std::runtime_error("Cannot open file: " + path.string());

        m_stream.seekg(0, std::ios::end);
        m_size = static_cast<uint64_t>(m_stream.tellg());
        m_stream.seekg(0, std::ios::beg);
    }

    size_t Read(std::byte* buf, size_t n) override
    {
        m_stream.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(n));
        return static_cast<size_t>(m_stream.gcount());
    }

    void Seek(uint64_t offset) override
    {
        m_stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    }

    uint64_t Tell() const override
    {
        return static_cast<uint64_t>(const_cast<std::ifstream&>(m_stream).tellg());
    }

    uint64_t Size() const override { return m_size; }

private:
    std::ifstream m_stream;
    uint64_t      m_size = 0;
};

} // namespace xcel::io

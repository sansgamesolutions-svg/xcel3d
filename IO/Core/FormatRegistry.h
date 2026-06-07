#pragma once
#include "IO/Core/IFormatReader.h"
#include "IO/Core/IFormatWriter.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xcel::io {

// Custom deleter type alias used when a plugin owns the destruction.
using ReaderDeleter = void(*)(IFormatReader*);
using WriterDeleter = void(*)(IFormatWriter*);

class FormatRegistry
{
public:
    // Statically-linked formats (standard delete).
    void RegisterReader(std::unique_ptr<IFormatReader> reader);
    void RegisterWriter(std::unique_ptr<IFormatWriter> writer);

    // Plugin formats: caller supplies a C-ABI destroy function.
    void RegisterReader(IFormatReader* reader, ReaderDeleter deleter);
    void RegisterWriter(IFormatWriter* writer, WriterDeleter deleter);

    // Returns the first reader that CanRead(ext), or nullptr.
    IFormatReader* FindReader(std::string_view extension) const;
    IFormatWriter* FindWriter(std::string_view extension) const;

private:
    std::vector<std::unique_ptr<IFormatReader, ReaderDeleter>> m_readers;
    std::vector<std::unique_ptr<IFormatWriter, WriterDeleter>> m_writers;
};

} // namespace xcel::io

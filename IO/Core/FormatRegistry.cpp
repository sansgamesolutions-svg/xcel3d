#include "IO/Core/FormatRegistry.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace xcel::io {

namespace {
std::string LowerExt(std::string_view ext)
{
    std::string s(ext);
    // Strip leading dot if present.
    if (!s.empty() && s[0] == '.')
        s.erase(0, 1);
    std::ranges::transform(s, s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}
} // namespace

static void DefaultDelete(IFormatReader* p) { delete p; }
static void DefaultWriterDelete(IFormatWriter* p) { delete p; }

void FormatRegistry::RegisterReader(std::unique_ptr<IFormatReader> reader)
{
    m_readers.emplace_back(reader.release(), DefaultDelete);
}

void FormatRegistry::RegisterWriter(std::unique_ptr<IFormatWriter> writer)
{
    m_writers.emplace_back(writer.release(), DefaultWriterDelete);
}

void FormatRegistry::RegisterReader(IFormatReader* reader, ReaderDeleter deleter)
{
    m_readers.emplace_back(reader, deleter);
}

void FormatRegistry::RegisterWriter(IFormatWriter* writer, WriterDeleter deleter)
{
    m_writers.emplace_back(writer, deleter);
}

IFormatReader* FormatRegistry::FindReader(std::string_view extension) const
{
    std::string ext = LowerExt(extension);
    for (auto& r : m_readers)
        if (r->CanRead(ext))
            return r.get();
    return nullptr;
}

IFormatWriter* FormatRegistry::FindWriter(std::string_view extension) const
{
    std::string ext = LowerExt(extension);
    for (auto& w : m_writers)
        if (w->CanWrite(ext))
            return w.get();
    return nullptr;
}

} // namespace xcel::io

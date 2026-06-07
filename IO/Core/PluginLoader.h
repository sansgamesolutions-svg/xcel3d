#pragma once
#include "IO/Core/IFormatPlugin.h"
#include <filesystem>
#include <vector>

namespace xcel::io {

class FormatRegistry;

// Loads IO plugin DLLs/SOs at runtime and registers their readers/writers.
// Keeps DLL handles alive until UnloadAll() is called (typically at shutdown).
class PluginLoader
{
public:
    PluginLoader() = default;
    ~PluginLoader();

    PluginLoader(const PluginLoader&)            = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    // Opens a single plugin DLL, validates apiVersion, registers with registry.
    // Throws std::runtime_error on load failure or version mismatch.
    void Load(const std::filesystem::path& dllPath, FormatRegistry& registry);

    // Scans `dir` for files matching XcelIO_*.dll (Windows) or XcelIO_*.so (POSIX)
    // and calls Load() for each.
    void ScanDir(const std::filesystem::path& dir, FormatRegistry& registry);

    // Unregisters and unloads all plugins. Called automatically by destructor.
    void UnloadAll();

    size_t LoadedCount() const { return m_loaded.size(); }

private:
    struct LoadedPlugin
    {
        void*           handle      = nullptr;
        FnDestroyReader destroyRead = nullptr;
        FnDestroyWriter destroyWrite = nullptr;
    };

    std::vector<LoadedPlugin> m_loaded;
};

} // namespace xcel::io

#include "IO/Core/PluginLoader.h"
#include "IO/Core/FormatRegistry.h"
#include <stdexcept>
#include <string>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #define XCEL_PLUGIN_GLOB "XcelIO_*.dll"
  static void* OpenLib(const std::filesystem::path& p)
  {
      return static_cast<void*>(LoadLibraryW(p.wstring().c_str()));
  }
  static void CloseLib(void* h) { FreeLibrary(static_cast<HMODULE>(h)); }
  static void* GetSym(void* h, const char* name)
  {
      return reinterpret_cast<void*>(
          GetProcAddress(static_cast<HMODULE>(h), name));
  }
#else
  #include <dlfcn.h>
  #include <glob.h>
  #define XCEL_PLUGIN_GLOB "XcelIO_*.so"
  static void* OpenLib(const std::filesystem::path& p)
  {
      return dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
  }
  static void CloseLib(void* h) { dlclose(h); }
  static void* GetSym(void* h, const char* name) { return dlsym(h, name); }
#endif

namespace xcel::io {

namespace {
template<typename Fn>
Fn RequireSym(void* handle, const char* name, const std::filesystem::path& path)
{
    void* sym = GetSym(handle, name);
    if (!sym)
        throw std::runtime_error("Plugin " + path.string()
                                 + " missing symbol: " + name);
    return reinterpret_cast<Fn>(sym);
}
} // namespace

void PluginLoader::Load(const std::filesystem::path& dllPath, FormatRegistry& registry)
{
    void* handle = OpenLib(dllPath);
    if (!handle)
        throw std::runtime_error("Failed to load plugin: " + dllPath.string());

    auto info_fn = RequireSym<FnPluginInfo>(handle, "xcel_plugin_info", dllPath);
    const XcelPluginInfo* info = info_fn();
    if (!info || info->apiVersion != XCEL_IO_API_VERSION)
    {
        CloseLib(handle);
        throw std::runtime_error("Plugin " + dllPath.string()
            + " API version mismatch (expected "
            + std::to_string(XCEL_IO_API_VERSION) + ")");
    }

    auto create  = RequireSym<FnCreateReader> (handle, "xcel_create_reader",  dllPath);
    auto destroy = RequireSym<FnDestroyReader>(handle, "xcel_destroy_reader", dllPath);

    registry.RegisterReader(create(), destroy);

    LoadedPlugin lp;
    lp.handle      = handle;
    lp.destroyRead = destroy;

    if (info->supportsWrite)
    {
        auto cw = RequireSym<FnCreateWriter> (handle, "xcel_create_writer",  dllPath);
        auto dw = RequireSym<FnDestroyWriter>(handle, "xcel_destroy_writer", dllPath);
        registry.RegisterWriter(cw(), dw);
        lp.destroyWrite = dw;
    }

    m_loaded.push_back(lp);
}

void PluginLoader::ScanDir(const std::filesystem::path& dir, FormatRegistry& registry)
{
    if (!std::filesystem::is_directory(dir))
        return;

    for (auto& entry : std::filesystem::directory_iterator(dir))
    {
        const auto& p = entry.path();
#if defined(_WIN32)
        if (p.extension() != ".dll") continue;
#else
        if (p.extension() != ".so") continue;
#endif
        std::string stem = p.stem().string();
        if (stem.rfind("XcelIO_", 0) != 0) continue;

        try { Load(p, registry); }
        catch (const std::exception&) { /* skip bad plugins */ }
    }
}

void PluginLoader::UnloadAll()
{
    for (auto& lp : m_loaded)
        CloseLib(lp.handle);
    m_loaded.clear();
}

PluginLoader::~PluginLoader() { UnloadAll(); }

} // namespace xcel::io

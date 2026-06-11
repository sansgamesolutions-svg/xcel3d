#pragma once
#include "IO/Core/FormatRegistry.h"
#include "IO/Core/PluginLoader.h"
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

namespace xcel {
class ThreadPool;
}

namespace xcel::io {

class SceneDocument;

class IOManager
{
public:
    IOManager() = default;
    ~IOManager() = default;

    IOManager(const IOManager&)            = delete;
    IOManager& operator=(const IOManager&) = delete;

    // ── Format registration ──────────────────────────────────────────────────

    void RegisterReader(std::unique_ptr<IFormatReader> reader);
    void RegisterWriter(std::unique_ptr<IFormatWriter> writer);

    // Load a single plugin DLL.
    void LoadPlugin(const std::filesystem::path& dllPath);

    // Scan a directory and load all XcelIO_* plugins found.
    void ScanPluginDir(const std::filesystem::path& dir);

    // ── Async load / save ────────────────────────────────────────────────────

    // Dispatches the load to `pool`; returns a shared_future that resolves to
    // the completed SceneDocument. Non-blocking on the caller's thread.
    [[nodiscard]]
    std::shared_future<std::shared_ptr<SceneDocument>>
        LoadAsync(std::filesystem::path path, xcel::ThreadPool& pool);

    // Dispatches a save to `pool`.
    [[nodiscard]]
    std::shared_future<void>
        SaveAsync(std::filesystem::path path,
                  std::shared_ptr<SceneDocument> doc,
                  xcel::ThreadPool& pool);

    // ── Polling ──────────────────────────────────────────────────────────────

    // Call once per frame on the main thread.
    // Moves any completed loads into `out`; leaves pending ones in the queue.
    void Poll(std::vector<std::shared_ptr<SceneDocument>>& out);

private:
    // m_pluginLoader must be declared BEFORE m_registry so it is destroyed
    // AFTER m_registry. Destruction order is reverse of declaration order.
    // The registry holds raw function pointers (xcel_destroy_reader) that live
    // in the plugin DLLs; those DLLs must still be loaded when the registry
    // calls the deleters — FreeLibrary must happen last.
    PluginLoader   m_pluginLoader;
    FormatRegistry m_registry;

    std::mutex     m_pendingMutex;
    std::vector<std::shared_future<std::shared_ptr<SceneDocument>>> m_pending;
};

} // namespace xcel::io

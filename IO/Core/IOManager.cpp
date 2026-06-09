#include "IO/Core/IOManager.h"
#include "IO/Core/FileStreamSource.h"
#include "IO/Core/FileStreamSink.h"
#include "IO/Core/SceneBuilder.h"
#include "IO/Scene/SceneDocument.h"
#include "Common/ThreadPool.h"
#include <algorithm>
#include <stdexcept>

namespace xcel::io {

IOManager::~IOManager()
{
    std::vector<std::shared_future<std::shared_ptr<SceneDocument>>> pendingLoads;
    std::vector<std::shared_future<void>> pendingSaves;
    {
        std::scoped_lock lock(m_pendingMutex);
        pendingLoads = m_pending;
        pendingSaves = m_pendingSaves;
    }
    for (const auto& future : pendingLoads)
        future.wait();
    for (const auto& future : pendingSaves)
        future.wait();

    m_registry.Clear();
    m_pluginLoader.UnloadAll();
}

void IOManager::RegisterReader(std::unique_ptr<IFormatReader> reader)
{
    m_registry.RegisterReader(std::move(reader));
}

void IOManager::RegisterWriter(std::unique_ptr<IFormatWriter> writer)
{
    m_registry.RegisterWriter(std::move(writer));
}

void IOManager::LoadPlugin(const std::filesystem::path& dllPath)
{
    m_pluginLoader.Load(dllPath, m_registry);
}

void IOManager::ScanPluginDir(const std::filesystem::path& dir)
{
    m_pluginLoader.ScanDir(dir, m_registry);
}

std::shared_future<std::shared_ptr<SceneDocument>>
IOManager::LoadAsync(std::filesystem::path path, xcel::ThreadPool& pool)
{
    std::string ext = path.extension().string();
    IFormatReader* reader = m_registry.FindReader(ext);
    if (!reader)
        throw std::runtime_error("No reader registered for extension: " + ext);

    auto future = pool.Submit([reader, p = std::move(path), &pool]()
        -> std::shared_ptr<SceneDocument>
    {
        FileStreamSource source(p);
        SceneBuilder     builder;
        // This task already occupies a worker from pool. Running nested work on
        // the same pool and waiting for it can deadlock when workers are exhausted.
        reader->Read(source, builder, nullptr);
        auto doc = builder.Build();
        doc->SetSourcePath(p);
        return doc;
    }).share();

    {
        std::scoped_lock lock(m_pendingMutex);
        m_pending.push_back(future);
    }
    return future;
}

std::shared_future<void>
IOManager::SaveAsync(std::filesystem::path path,
                     std::shared_ptr<SceneDocument> doc,
                     xcel::ThreadPool& pool)
{
    std::string ext = path.extension().string();
    IFormatWriter* writer = m_registry.FindWriter(ext);
    if (!writer)
        throw std::runtime_error("No writer registered for extension: " + ext);

    auto future = pool.Submit([writer, p = std::move(path), d = std::move(doc)] {
        FileStreamSink sink(p);
        writer->Write(*d, sink);
    }).share();
    {
        std::scoped_lock lock(m_pendingMutex);
        m_pendingSaves.push_back(future);
    }
    return future;
}

void IOManager::Poll(std::vector<std::shared_ptr<SceneDocument>>& out)
{
    std::scoped_lock lock(m_pendingMutex);
    std::vector<std::shared_future<std::shared_ptr<SceneDocument>>> stillPending;

    for (auto& f : m_pending)
    {
        if (f.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            out.push_back(f.get());
        else
            stillPending.push_back(std::move(f));
    }
    m_pending = std::move(stillPending);

    std::erase_if(m_pendingSaves, [](const auto& future) {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
}

} // namespace xcel::io

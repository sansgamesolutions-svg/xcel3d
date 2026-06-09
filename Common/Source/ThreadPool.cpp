#include "Common/ThreadPool.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <vector>

namespace xcel {

ThreadPool::ThreadPool(size_t threadCount)
    {
    if (threadCount == 0)
        threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0)
        threadCount = 1;

    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    m_workCv.wait(lock, [this] {
                        return m_stopping || !m_tasks.empty();
                    });
                    if (m_stopping && m_tasks.empty())
                        return;
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }

                task();

                {
                    std::lock_guard lock(m_mutex);
                    --m_pendingTasks;
                    m_completeCv.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
    }
    m_workCv.notify_all();
    for (auto& w : m_workers)
        w.join();
}

void ThreadPool::Enqueue(std::function<void()> task)
{
    {
        std::lock_guard lock(m_mutex);
        if (m_stopping)
            throw std::runtime_error("ThreadPool::Submit called while stopping");
        ++m_pendingTasks;
        m_tasks.push(std::move(task));
    }
    m_workCv.notify_one();
}

void ThreadPool::WaitAll()
{
    std::unique_lock lock(m_mutex);
    m_completeCv.wait(lock, [this] {
        return m_pendingTasks == 0;
    });
}

size_t ThreadPool::ThreadCount() const
{
    return m_workers.size();
}

} // namespace xcel

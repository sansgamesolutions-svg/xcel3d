#include "Common/ThreadPool.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace xcel {

struct ThreadPool::Impl {
    std::vector<std::thread>          workers;
    std::queue<std::function<void()>> tasks;
    std::mutex                        mutex;
    std::condition_variable           workCv;
    std::condition_variable           completeCv;
    size_t                            pendingTasks{0};
    bool                              stopping{false};
};

ThreadPool::ThreadPool(size_t threadCount)
    : m_impl(std::make_unique<Impl>())
{
    if (threadCount == 0)
        threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0)
        threadCount = 1;

    m_impl->workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        m_impl->workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_impl->mutex);
                    m_impl->workCv.wait(lock, [this] {
                        return m_impl->stopping || !m_impl->tasks.empty();
                    });
                    if (m_impl->stopping && m_impl->tasks.empty())
                        return;
                    task = std::move(m_impl->tasks.front());
                    m_impl->tasks.pop();
                }

                task();

                {
                    std::lock_guard lock(m_impl->mutex);
                    --m_impl->pendingTasks;
                    m_impl->completeCv.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(m_impl->mutex);
        m_impl->stopping = true;
    }
    m_impl->workCv.notify_all();
    for (auto& w : m_impl->workers)
        w.join();
}

void ThreadPool::Enqueue(std::function<void()> task) {
    {
        std::lock_guard lock(m_impl->mutex);
        ++m_impl->pendingTasks;
        m_impl->tasks.push(std::move(task));
    }
    m_impl->workCv.notify_one();
}

void ThreadPool::WaitAll() {
    std::unique_lock lock(m_impl->mutex);
    m_impl->completeCv.wait(lock, [this] {
        return m_impl->pendingTasks == 0;
    });
}

size_t ThreadPool::ThreadCount() const {
    return m_impl->workers.size();
}

} // namespace xcel

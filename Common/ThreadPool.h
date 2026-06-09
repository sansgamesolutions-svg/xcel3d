#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace xcel {

class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount = 0); // 0 → hardware_concurrency
    ~ThreadPool();

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<typename F>
        requires std::invocable<F>
    auto Submit(F&& task) -> std::future<std::invoke_result_t<F>>
    {
        using R = std::invoke_result_t<F>;
        auto packaged = std::make_shared<std::packaged_task<R()>>(
            std::forward<F>(task));
        auto future = packaged->get_future();

        Enqueue([packaged = std::move(packaged)] {
            (*packaged)();
        });

        return future;
    }

    void   WaitAll();
    size_t ThreadCount() const;

private:
    void Enqueue(std::function<void()> task);

    std::vector<std::thread>          m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex                        m_mutex;
    std::condition_variable           m_workCv;
    std::condition_variable           m_completeCv;
    size_t                            m_pendingTasks = 0;
    bool                              m_stopping     = false;
};

} // namespace xcel

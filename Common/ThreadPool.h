#pragma once
#include <functional>
#include <future>
#include <memory>
#include <type_traits>

namespace xcel {

class ThreadPool {
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
        auto promise = std::make_shared<std::promise<R>>();
        auto future  = promise->get_future();

        Enqueue([p = std::move(promise), t = std::forward<F>(task)]() mutable {
            if constexpr (std::is_void_v<R>) {
                t();
                p->set_value();
            } else {
                p->set_value(t());
            }
        });

        return future;
    }

    void   WaitAll();
    size_t ThreadCount() const;

private:
    void Enqueue(std::function<void()> task);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel

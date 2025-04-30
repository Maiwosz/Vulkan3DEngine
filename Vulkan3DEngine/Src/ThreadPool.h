#pragma once
#include <deque>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// Only define the concept if it hasn't been defined elsewhere
#ifndef CALLABLE_CONCEPT_DEFINED
#define CALLABLE_CONCEPT_DEFINED
template<typename F, typename... Args>
concept Callable = requires(F f, Args... args) {
    { std::invoke(f, args...) } -> std::same_as<typename std::invoke_result<F, Args...>::type>;
};
#endif

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();
    size_t getThreadCount() { return m_threadCount; }

    template<class F, class ...Args>
    auto enqueue(F&& f, Args && ...args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;
        auto task = std::make_shared< std::packaged_task<return_type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            if (m_stop) throw std::runtime_error("enqueue on stopped ThreadPool");
            m_tasks.emplace_back([task]() { (*task)(); });
        }
        m_condition.notify_one();
        return res;
    }
private:
    static ThreadPool* m_pool;
    std::vector<std::thread> m_workers;
    std::deque<std::function<void()>> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    bool m_stop;
    size_t m_threadCount;
};
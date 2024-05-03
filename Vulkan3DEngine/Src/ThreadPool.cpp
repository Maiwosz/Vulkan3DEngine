#include "ThreadPool.h"

ThreadPool* ThreadPool::m_pool = nullptr;

ThreadPool::ThreadPool(size_t threads) : m_stop(false), m_threadCount(threads)
{
    for (size_t i = 0; i < threads; ++i) {
        m_workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->m_queue_mutex);
                    this->m_condition.wait(lock, [this] { return this->m_stop || !this->m_tasks.empty(); });
                    if (this->m_stop && this->m_tasks.empty()) return;
                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop_front();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_stop = true;
    }
    m_condition.notify_all();
    for (std::thread& worker : m_workers) worker.join();
}

ThreadPool* ThreadPool::get()
{
    return m_pool;
}

void ThreadPool::create(size_t threads)
{
    if (ThreadPool::m_pool) throw std::exception("ThreadPool already created");
    ThreadPool::m_pool = new ThreadPool(threads);
}

void ThreadPool::release()
{
    if (!ThreadPool::m_pool)
    {
        return;
    }
    delete ThreadPool::m_pool;
}



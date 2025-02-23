//
// Created by garrett on 2/23/25.
//


#include "thread_pool.hpp"

ThreadPool::ThreadPool() : m_stop(false) {};


void ThreadPool::enqueue(std::function<void()> task) {
    std::unique_lock lock(m_queue_mutex);
    m_tasks.push(task);
    m_condition.notify_one();
}

void ThreadPool::start(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_queue_mutex);
                    m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    task = m_tasks.front();
                    m_tasks.pop();
                }
                task();
            }
        });
    }
}

// Created by garrett on 2/23/25.
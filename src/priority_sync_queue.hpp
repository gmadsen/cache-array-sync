//
// Created by garrett on 2/24/25.
//
/*
Advanced Queue Features
1. Priority-based queueing: Some file operations may be more urgent than others.
2. Bounded queueing: Set limits to prevent memory exhaustion.
3. Work stealing: Allow idle threads to take work from busy threads.
4. Back-pressure mechanisms: Slow down producers when consumers can't keep up.
*/

#ifndef PRIORITY_SYNC_QUEUE_HPP
#define PRIORITY_SYNC_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <optional>
#include <chrono>
#include <atomic>

// Forward declaration
class SyncTask;

// Priority levels for different types of operations
enum class SyncPriority {
    CRITICAL = 0,  // Essential operations (e.g., config files)
    HIGH = 1,      // User-initiated operations
    NORMAL = 2,    // Regular file changes
    LOW = 3,       // Bulk operations or non-essential files
    BACKGROUND = 4 // Periodic checks, cleanup, etc.
};

// A task representing a file sync operation
class SyncTask {
public:
    SyncTask(std::string path,
             std::string operation,
             SyncPriority priority = SyncPriority::NORMAL)
        : m_path(std::move(path)),
          m_operation(std::move(operation)),
          m_priority(priority),
          m_timestamp(std::chrono::system_clock::now()),
          m_retryCount(0),
          m_status("pending"),
          m_taskId(generateTaskId()) {}

    // Getters
    const std::string& getPath() const { return m_path; }
    const std::string& getOperation() const { return m_operation; }
    SyncPriority getPriority() const { return m_priority; }
    auto getTimestamp() const { return m_timestamp; }
    int getRetryCount() const { return m_retryCount; }
    const std::string& getStatus() const { return m_status; }
    const std::string& getTaskId() const { return m_taskId; }

    // Setters
    void incrementRetry() { m_retryCount++; }
    void setStatus(const std::string& status) { m_status = status; }

    // Task comparison for priority queue - lower priority value means higher priority
    bool operator<(const SyncTask& other) const {
        return m_priority > other.m_priority; // Reversed for priority_queue
    }

private:
    std::string m_path;      // File path
    std::string m_operation; // Operation type (sync, delete, etc.)
    SyncPriority m_priority; // Task priority
    std::chrono::system_clock::time_point m_timestamp; // Task creation time
    int m_retryCount;        // Number of retry attempts
    std::string m_status;    // Current status (pending, in_progress, completed, failed)
    std::string m_taskId;    // Unique task identifier

    // Generate a unique task ID
    static std::string generateTaskId() {
        static std::atomic<uint64_t> counter{0};
        uint64_t id = ++counter;
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        uint64_t timestamp = now_ms.time_since_epoch().count();

        return std::to_string(timestamp) + "-" + std::to_string(id);
    }
};

// A thread-safe priority queue for sync tasks
class PrioritySyncQueue {
public:
    PrioritySyncQueue(size_t maxSize = 10000)
        : m_maxSize(maxSize), m_shutdown(false) {}

    // Add a task to the queue with timeout and back-pressure
    bool enqueue(SyncTask task, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(m_mutex);

        // Wait until there's room in the queue or timeout
        bool hasRoom = m_notFull.wait_for(lock, timeout, [this] {
            return m_tasks.size() < m_maxSize || m_shutdown;
        });

        if (!hasRoom || m_shutdown) {
            return false; // Queue is full or shutting down
        }

        m_tasks.push(std::move(task));
        m_notEmpty.notify_one();
        return true;
    }

    // Get the next task from the queue
    std::optional<SyncTask> dequeue(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(m_mutex);

        // Wait for a task or timeout
        bool hasTask = m_notEmpty.wait_for(lock, timeout, [this] {
            return !m_tasks.empty() || m_shutdown;
        });

        if (!hasTask || (m_tasks.empty() && m_shutdown)) {
            return std::nullopt; // No tasks or shutting down
        }

        SyncTask task = std::move(m_tasks.top());
        m_tasks.pop();
        m_notFull.notify_one();
        return task;
    }

    // Check if the queue is empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.empty();
    }

    // Get the current size of the queue
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

    // Prepare for shutdown
    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    std::priority_queue<SyncTask> m_tasks;
    size_t m_maxSize;
    bool m_shutdown;
};

#endif // PRIORITY_SYNC_QUEUE_HPP
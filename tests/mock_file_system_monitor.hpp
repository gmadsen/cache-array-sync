//
// Created by garrett on 2/24/25.
//

#ifndef MOCK_FILE_SYSTEM_MONITOR_HPP
#define MOCK_FILE_SYSTEM_MONITOR_HPP

#include "file_system_monitor.hpp"
#include <map>
#include <string>
#include <queue>

// This is a mock class that simulates the behavior of FileSystemMonitor
// without making actual inotify system calls
class MockFileSystemMonitor : public FileSystemMonitor {
public:
    MockFileSystemMonitor() : FileSystemMonitor() {
        // Override the inotifyFd to avoid actual system calls
        m_inotifyFd = -999; // This won't be used for actual system calls
    }

    // Override addWatch to not make actual system calls
    void addWatch(const std::string& path) override {
        // Instead of calling inotify_add_watch, just store the path
        m_watches[path] = m_nextWatchDescriptor++;

        // Notify that we're watching this path
        if (m_callback) {
            m_callback(path);
        }
    }

    // Override removeWatch to not make actual system calls
    void removeWatch(const std::string& path) override {
        // Just remove from our internal map
        m_watches.erase(path);
    }

    // Mock generating a file system event
    void simulateEvent(const std::string& path, const std::string& action, int mask = 0) {
        FSEvent event;
        event.path = path;
        event.action = action;
        event.timestamp = std::chrono::system_clock::now();
        event.mask = mask;

        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_event_queue.push(event);
        }

        // Call callback if set
        if (m_callback) {
            m_callback(path);
        }
    }

    // Override getNextEvent to pull from our mocked queue
    std::optional<FSEvent> getNextEvent() override {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (m_event_queue.empty()) {
            return std::nullopt;
        }

        FSEvent event = m_event_queue.front();
        m_event_queue.pop();
        return event;
    }

    // Override empty check
    bool empty() override {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return m_event_queue.empty();
    }

private:
    std::map<std::string, int> m_watches;
    int m_nextWatchDescriptor = 1;
};

#endif // MOCK_FILE_SYSTEM_MONITOR_HPP


//
// Created by Garrett
//
#ifndef FILE_SYSTEM_MONITOR_HPP
#define FILE_SYSTEM_MONITOR_HPP



#include <string>
#include <queue>
#include <mutex>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <optional>

/// going to make a class that will monitor the file system for changes using the inotify API
/// and will notify the user of any changes that occur
class FileSystemMonitor {
public:
    /// @brief A structure to hold file system event information
    struct FSEvent {
        std::string path;
        std::string action;
        std::chrono::system_clock::time_point timestamp;
        int mask;
    };

    FileSystemMonitor();
    FileSystemMonitor(const FileSystemMonitor&) = delete;
    FileSystemMonitor& operator=(const FileSystemMonitor&) = delete;
    FileSystemMonitor(FileSystemMonitor&&) = delete;
    FileSystemMonitor& operator=(FileSystemMonitor&&) = delete;


    /// @brief  Add a watch to the file system monitor
    /// @param path  
    void addWatch(const std::string& path);

    /// @brief  Get the next file system event
    /// @return 
    std::optional<FSEvent> getNextEvent();

    /// @brief Remove a watch from the file system monitor
    /// @param path
    void removeWatch(const std::string& path);

    /// @brief Stop the file system monitor 
    void stop();

    /// @brief Set the callback function to be called when a file system event occurs 
    /// @param cb 
    void setCallback(std::function<void(const std::string&)> cb);

    bool empty();

private:
    std::function<void(const std::string&)> m_callback;
    int m_inotifyFd;
    std::unordered_map<int, std::string> m_watch_descriptors;
    std::queue<FSEvent> m_event_queue;
    std::mutex m_queue_mutex;

};

#endif //FILE_SYSTEM_MONITOR_HPP








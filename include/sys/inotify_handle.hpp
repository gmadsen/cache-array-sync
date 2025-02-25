
#ifndef INOTIFY_HANDLE_HPP
#define INOTIFY_HANDLE_HPP

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <system_error>
#include <cerrno>

namespace sys {

class InotifyHandle {
private:
    int m_fd = -1;

public:
    InotifyHandle() : m_fd(inotify_init1(IN_NONBLOCK)) {
        if (m_fd == -1) {
            throw std::system_error(errno, std::system_category(), "inotify_init1 failed");
        }
    }
    
    ~InotifyHandle() {
        if (m_fd != -1) {
            close(m_fd);
        }
    }
    
    // Prevent copying
    InotifyHandle(const InotifyHandle&) = delete;
    InotifyHandle& operator=(const InotifyHandle&) = delete;
    
    // Allow moving
    InotifyHandle(InotifyHandle&& other) noexcept : m_fd(other.m_fd) {
        other.m_fd = -1;
    }
    
    InotifyHandle& operator=(InotifyHandle&& other) noexcept {
        if (this != &other) {
            if (m_fd != -1) {
                close(m_fd);
            }
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }
    
    // Get the raw file descriptor
    int fd() const { return m_fd; }
    
    // Add a watch
    int addWatch(const std::string& path, uint32_t mask) {
        int wd = inotify_add_watch(m_fd, path.c_str(), mask);
        if (wd == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to add watch for: " + path);
        }
        return wd;
    }
    
    // Remove a watch
    void removeWatch(int watchDescriptor) {
        if (inotify_rm_watch(m_fd, watchDescriptor) == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to remove watch: " + std::to_string(watchDescriptor));
        }
    }
    
    // Read events (non-blocking)
    std::vector<inotify_event> readEvents() {
        std::vector<inotify_event> events;
        
        // Buffer for reading events
        char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        
        ssize_t length = read(m_fd, buffer, sizeof(buffer));
        if (length == -1) {
            if (errno == EAGAIN) {
                // No data available
                return events;
            }
            throw std::system_error(errno, std::system_category(), "Failed to read inotify events");
        }
        
        // Process events
        char* ptr = buffer;
        while (ptr < buffer + length) {
            auto event = reinterpret_cast<struct inotify_event*>(ptr);
            events.push_back(*event);
            ptr += sizeof(struct inotify_event) + event->len;
        }
        
        return events;
    }
};
}

#endif // INOTIFY_HANDLE_HPP
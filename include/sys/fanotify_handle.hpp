
#ifndef FANOTIFY_HANDLE_HPP
#define FANOTIFY_HANDLE_HPP

#include <sys/fanotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <system_error>
#include <cerrno>

#define PATH_MAX 4096

namespace sys {

class FanotifyHandle {
private:
    int m_fd = -1;

public:
    FanotifyHandle(unsigned int flags = FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK,
                 int openFlags = O_RDONLY) 
        : m_fd(fanotify_init(flags, openFlags)) {
        if (m_fd == -1) {
            throw std::system_error(errno, std::system_category(), "fanotify_init failed");
        }
    }
    
    ~FanotifyHandle() {
        if (m_fd != -1) {
            close(m_fd);
        }
    }
    
    // Prevent copying
    FanotifyHandle(const FanotifyHandle&) = delete;
    FanotifyHandle& operator=(const FanotifyHandle&) = delete;
    
    // Allow moving
    FanotifyHandle(FanotifyHandle&& other) noexcept : m_fd(other.m_fd) {
        other.m_fd = -1;
    }
    
    FanotifyHandle& operator=(FanotifyHandle&& other) noexcept {
        if (this != &other) {
            if (m_fd != -1) {
                close(m_fd);
            }
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }
    
    int fd() const { return m_fd; }
    
    // Add a mark for a path
    void addMark(const std::string& path, uint64_t mask, 
                unsigned int flags = FAN_MARK_ADD) {
        if (fanotify_mark(m_fd, flags, mask, AT_FDCWD, path.c_str()) == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to add fanotify mark for: " + path);
        }
    }
    
    // Add a mark for a mount point
    void addMountMark(const std::string& path, uint64_t mask) {
        addMark(path, mask, FAN_MARK_ADD | FAN_MARK_MOUNT);
    }
    
    // Remove a mark
    void removeMark(const std::string& path, uint64_t mask) {
        if (fanotify_mark(m_fd, FAN_MARK_REMOVE, mask, AT_FDCWD, path.c_str()) == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to remove fanotify mark for: " + path);
        }
    }
    
    // Read events
    std::vector<std::pair<fanotify_event_metadata, std::string>> readEvents() {
        std::vector<std::pair<fanotify_event_metadata, std::string>> events;
        
        char buffer[4096];
        ssize_t length = read(m_fd, buffer, sizeof(buffer));
        
        if (length == -1) {
            if (errno == EAGAIN) {
                // No data available
                return events;
            }
            throw std::system_error(errno, std::system_category(), 
                "Failed to read fanotify events");
        }
        
        fanotify_event_metadata* metadata = reinterpret_cast<fanotify_event_metadata*>(buffer);
        
        while (FAN_EVENT_OK(metadata, length)) {
            std::string path;
            
            if (metadata->fd >= 0) {
                // Get file path from fd
                char fdPath[PATH_MAX];
                snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", metadata->fd);
                
                char filePath[PATH_MAX];
                ssize_t linkLen = readlink(fdPath, filePath, sizeof(filePath) - 1);
                
                if (linkLen != -1) {
                    filePath[linkLen] = '\0';
                    path = filePath;
                }
                
                // Close the file descriptor
                close(metadata->fd);
            }
            
            events.emplace_back(*metadata, path);
            metadata = FAN_EVENT_NEXT(metadata, length);
        }
        
        return events;
    }
    
    // Respond to permission events
    void respondToEvent(int fd, bool allow) {
        fanotify_response response;
        response.fd = fd;
        response.response = allow ? FAN_ALLOW : FAN_DENY;
        
        if (write(m_fd, &response, sizeof(response)) != sizeof(response)) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to respond to fanotify event");
        }
    }
};

} // namespace sys

#endif // FANOTIFY_HANDLE_HPP

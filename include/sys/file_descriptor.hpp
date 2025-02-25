#ifndef FILE_DESCRIPTOR_HPP
#define FILE_DESCRIPTOR_HPP

#include <unistd.h>
#include <fcntl.h>
#include <system_error>
#include <sys/stat.h>

namespace sys {

class FileDescriptor {
private:
    int m_fd = -1;

public:
    FileDescriptor() = default;
    
    explicit FileDescriptor(int fd) : m_fd(fd) {
        if (m_fd == -1) {
            throw std::system_error(errno, std::system_category(), "Invalid file descriptor");
        }
    }
    
    FileDescriptor(const std::string& path, int flags, mode_t mode = 0) {
        m_fd = open(path.c_str(), flags, mode);
        if (m_fd == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to open file: " + path);
        }
    }
    
    ~FileDescriptor() {
        if (m_fd != -1) {
            close(m_fd);
        }
    }
    
    // Prevent copying
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    
    // Allow moving
    FileDescriptor(FileDescriptor&& other) noexcept : m_fd(other.m_fd) {
        other.m_fd = -1;
    }
    
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
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
    
    bool isValid() const { return m_fd != -1; }
    
    // Read data
    ssize_t read(void* buffer, size_t bufferSize) {
        ssize_t result = ::read(m_fd, buffer, bufferSize);
        if (result == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to read from file");
        }
        return result;
    }
    
    // Write data
    ssize_t write(const void* buffer, size_t bufferSize) {
        ssize_t result = ::write(m_fd, buffer, bufferSize);
        if (result == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to write to file");
        }
        return result;
    }
    
    // Set file position
    off_t seek(off_t offset, int whence) {
        off_t result = lseek(m_fd, offset, whence);
        if (result == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to seek in file");
        }
        return result;
    }
    
    // Get file size
    off_t size() {
        struct stat st;
        if (fstat(m_fd, &st) == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to get file size");
        }
        return st.st_size;
    }
};
}
#endif  // FILE_DESCRIPTOR_HPP
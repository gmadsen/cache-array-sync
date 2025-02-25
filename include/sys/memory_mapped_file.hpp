#ifndef MEMORY_MAPPED_FILE_HPP
#define MEMORY_MAPPED_FILE_HPP

#include <span>
#include <string>
#include <system_error>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>
#include "file_descriptor.hpp"

namespace sys {

class MemoryMappedFile {
private:
    FileDescriptor m_fd;
    void* m_mappedAddr = nullptr;
    size_t m_size = 0;
    bool m_writable;

public:
    MemoryMappedFile(const std::string& path, bool writable = false)
        : m_fd(path, writable ? (O_RDWR | O_CREAT) : O_RDONLY, 0644),
          m_writable(writable) {
        
        m_size = m_fd.size();
        if (m_size == 0) {
            if (writable) {
                // Create an empty file for writing
                if (ftruncate(m_fd.fd(), 1) == -1) {
                    throw std::system_error(errno, std::system_category(), 
                        "Failed to initialize file size");
                }
                m_size = 1;
            } else {
                throw std::runtime_error("Cannot map empty file in read-only mode");
            }
        }
        
        int prot = PROT_READ;
        if (writable) {
            prot |= PROT_WRITE;
        }
        
        m_mappedAddr = mmap(nullptr, m_size, prot, MAP_SHARED, m_fd.fd(), 0);
        if (m_mappedAddr == MAP_FAILED) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to memory map file: " + path);
        }
    }
    
    ~MemoryMappedFile() {
        if (m_mappedAddr != nullptr && m_mappedAddr != MAP_FAILED) {
            munmap(m_mappedAddr, m_size);
        }
    }
    
    // Prevent copying
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    
    // Allow moving
    MemoryMappedFile(MemoryMappedFile&& other) noexcept
        : m_fd(std::move(other.m_fd)),
          m_mappedAddr(other.m_mappedAddr),
          m_size(other.m_size),
          m_writable(other.m_writable) {
        
        other.m_mappedAddr = nullptr;
        other.m_size = 0;
    }
    
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept {
        if (this != &other) {
            // Clean up existing mapping
            if (m_mappedAddr != nullptr && m_mappedAddr != MAP_FAILED) {
                munmap(m_mappedAddr, m_size);
            }
            
            // Take ownership of other's resources
            m_fd = std::move(other.m_fd);
            m_mappedAddr = other.m_mappedAddr;
            m_size = other.m_size;
            m_writable = other.m_writable;
            
            // Reset other
            other.m_mappedAddr = nullptr;
            other.m_size = 0;
        }
        return *this;
    }
    
    // Get raw pointer to mapped memory
    void* data() { return m_mappedAddr; }
    const void* data() const { return m_mappedAddr; }
    
    // Get size of mapped region
    size_t size() const { return m_size; }
    
    // Resize the file and mapping (only for writable mappings)
    void resize(size_t newSize) {
        if (!m_writable) {
            throw std::runtime_error("Cannot resize read-only mapping");
        }
        
        // Unmap current region
        if (m_mappedAddr != nullptr && m_mappedAddr != MAP_FAILED) {
            if (munmap(m_mappedAddr, m_size) == -1) {
                throw std::system_error(errno, std::system_category(), 
                    "Failed to unmap file for resizing");
            }
            m_mappedAddr = nullptr;
        }
        
        // Resize the file
        if (ftruncate(m_fd.fd(), newSize) == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to resize file");
        }
        
        // Remap the file
        m_size = newSize;
        m_mappedAddr = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, 
                          MAP_SHARED, m_fd.fd(), 0);
        
        if (m_mappedAddr == MAP_FAILED) {
            m_mappedAddr = nullptr;
            throw std::system_error(errno, std::system_category(), 
                "Failed to remap file after resize");
        }
    }
    
    // Flush changes to disk
    void flush(size_t offset = 0, size_t length = 0) {
        if (!m_writable) {
            return;  // No need to flush read-only mapping
        }
        
        if (length == 0) {
            length = m_size;
        }
        
        if (offset + length > m_size) {
            throw std::out_of_range("Flush range exceeds file size");
        }
        
        if (msync(static_cast<char*>(m_mappedAddr) + offset, length, MS_SYNC) == -1) {
            throw std::system_error(errno, std::system_category(), 
                "Failed to flush memory-mapped file");
        }
    }
    
    // Get a span view of the mapped memory
    std::span<const std::byte> bytes() const {
        return std::span<const std::byte>(
            static_cast<const std::byte*>(m_mappedAddr), m_size);
    }
    
    std::span<std::byte> bytes() {
        return std::span<std::byte>(
            static_cast<std::byte*>(m_mappedAddr), m_size);
    }
};

} // namespace sys

#endif // MEMORY_MAPPED_FILE_HPP
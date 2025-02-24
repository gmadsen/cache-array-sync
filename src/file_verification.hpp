//
// Created by garrett on 2/24/25.
//

#ifndef FILE_VERIFICATION_HPP
#define FILE_VERIFICATION_HPP

#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <functional>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <thread>
#include <future>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

// Class to verify file integrity and synchronization
class FileVerification {
public:
    // Verification methods available
    enum class VerifyMethod {
        SIZE_ONLY,       // Just compare file sizes
        TIMESTAMP,       // Compare timestamps
        FAST_HASH,       // MD5 for faster but less secure verification
        SECURE_HASH,     // SHA-256 for more secure verification
        FULL_COMPARE     // Byte-by-byte comparison for absolute certainty
    };

#endif // FILE_VERIFICATION_HPP

    // Result of verification
    struct VerifyResult {
        bool matches;
        std::string sourceHash;
        std::string destHash;
        std::string errorMessage;
        std::chrono::milliseconds duration;
    };

    FileVerification() = default;

    // Verify a single file pair
    VerifyResult verifyFile(const std::string& sourcePath,
                          const std::string& destPath,
                          VerifyMethod method = VerifyMethod::FAST_HASH) {
        auto startTime = std::chrono::high_resolution_clock::now();
        VerifyResult result;
        result.matches = false;

        // Basic existence check
        if (!fs::exists(sourcePath)) {
            result.errorMessage = "Source file does not exist";
            return finishResult(result, startTime);
        }

        if (!fs::exists(destPath)) {
            result.errorMessage = "Destination file does not exist";
            return finishResult(result, startTime);
        }

        // Check file size first (quick check)
        uintmax_t sourceSize = fs::file_size(sourcePath);
        uintmax_t destSize = fs::file_size(destPath);

        if (sourceSize != destSize) {
            result.errorMessage = "File sizes don't match";
            return finishResult(result, startTime);
        }

        // If we only need size verification, we're done
        if (method == VerifyMethod::SIZE_ONLY) {
            result.matches = true;
            return finishResult(result, startTime);
        }

        // Check timestamps if needed
        if (method == VerifyMethod::TIMESTAMP) {
            auto sourceTime = fs::last_write_time(sourcePath);
            auto destTime = fs::last_write_time(destPath);

            // Allow a small difference in timestamps (1 second)
            auto timeDiff = sourceTime - destTime;
            auto absDiff = timeDiff > fs::file_time_type::duration::zero() ?
                           timeDiff : fs::file_time_type::duration::zero() - timeDiff;

            result.matches = (absDiff <= std::chrono::seconds(1));
            if (!result.matches) {
                result.errorMessage = "Timestamps don't match within threshold";
            }

            return finishResult(result, startTime);
        }

        // For hash and full comparison methods, we need to read the files
        switch (method) {
            case VerifyMethod::FAST_HASH:
                result.sourceHash = calculateMD5(sourcePath);
                result.destHash = calculateMD5(destPath);
                result.matches = (result.sourceHash == result.destHash);
                if (!result.matches) {
                    result.errorMessage = "MD5 checksums don't match";
                }
                break;

            case VerifyMethod::SECURE_HASH:
                result.sourceHash = calculateSHA256(sourcePath);
                result.destHash = calculateSHA256(destPath);
                result.matches = (result.sourceHash == result.destHash);
                if (!result.matches) {
                    result.errorMessage = "SHA-256 checksums don't match";
                }
                break;

            case VerifyMethod::FULL_COMPARE:
                bool equalContent = compareFileContent(sourcePath, destPath);
                result.matches = equalContent;
                if (!equalContent) {
                    result.errorMessage = "File contents don't match";
                }
                break;
        }

        return finishResult(result, startTime);
    }

    // Verify a directory pair recursively
    std::vector<std::pair<std::string, VerifyResult>> verifyDirectory(
        const std::string& sourceDir,
        const std::string& destDir,
        VerifyMethod method = VerifyMethod::FAST_HASH,
        bool parallel = true,
        int maxThreads = 4) {

        std::vector<std::pair<std::string, VerifyResult>> results;
        std::mutex resultsMutex;

        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
            VerifyResult result;
            result.matches = false;
            result.errorMessage = "Source directory does not exist or is not a directory";
            results.emplace_back("", result);
            return results;
        }

        if (!fs::exists(destDir) || !fs::is_directory(destDir)) {
            VerifyResult result;
            result.matches = false;
            result.errorMessage = "Destination directory does not exist or is not a directory";
            results.emplace_back("", result);
            return results;
        }

        // Collect all files to verify
        std::vector<std::pair<std::string, std::string>> filePairs;
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string relPath = entry.path().string().substr(sourceDir.length());
                if (relPath[0] == '/' || relPath[0] == '\\') {
                    relPath = relPath.substr(1);
                }

                std::string destPath = fs::path(destDir) / relPath;
                if (fs::exists(destPath) && fs::is_regular_file(destPath)) {
                    filePairs.emplace_back(entry.path().string(), destPath);
                } else {
                    // Missing file in destination
                    VerifyResult result;
                    result.matches = false;
                    result.errorMessage = "File missing in destination";

                    std::lock_guard<std::mutex> lock(resultsMutex);
                    results.emplace_back(relPath, result);
                }
            }
        }

        // Check for extra files in destination
        for (const auto& entry : fs::recursive_directory_iterator(destDir)) {
            if (entry.is_regular_file()) {
                std::string relPath = entry.path().string().substr(destDir.length());
                if (relPath[0] == '/' || relPath[0] == '\\') {
                    relPath = relPath.substr(1);
                }

                std::string sourcePath = fs::path(sourceDir) / relPath;
                if (!fs::exists(sourcePath) || !fs::is_regular_file(sourcePath)) {
                    // Extra file in destination
                    VerifyResult result;
                    result.matches = false;
                    result.errorMessage = "Extra file in destination";

                    std::lock_guard<std::mutex> lock(resultsMutex);
                    results.emplace_back(relPath, result);
                }
            }
        }

        // Verify all file pairs
        if (parallel && filePairs.size() > 1) {
            // Use a thread pool for parallel verification
            int numThreads = std::min(maxThreads, static_cast<int>(filePairs.size()));
            std::vector<std::future<void>> futures;

            // Divide work among threads
            for (int i = 0; i < numThreads; ++i) {
                futures.push_back(std::async(std::launch::async, [this, &filePairs, &results, &resultsMutex, method, i, numThreads]() {
                    for (size_t j = i; j < filePairs.size(); j += numThreads) {
                        const auto& pair = filePairs[j];

                        std::string relPath = pair.first;
                        if (relPath.find(fs::path(pair.first).root_path().string()) == 0) {
                            relPath = relPath.substr(fs::path(pair.first).root_path().string().length());
                        }

                        VerifyResult result = verifyFile(pair.first, pair.second, method);

                        std::lock_guard<std::mutex> lock(resultsMutex);
                        results.emplace_back(relPath, result);
                    }
                }));
            }

            // Wait for all verification tasks to complete
            for (auto& future : futures) {
                future.wait();
            }
        } else {
            // Sequential verification
            for (const auto& pair : filePairs) {
                std::string relPath = pair.first;
                if (relPath.find(fs::path(pair.first).root_path().string()) == 0) {
                    relPath = relPath.substr(fs::path(pair.first).root_path().string().length());
                }

                VerifyResult result = verifyFile(pair.first, pair.second, method);
                results.emplace_back(relPath, result);
            }
        }

        return results;
    }

    // Calculate a hash for a file
    static std::string calculateMD5(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            return "";
        }

        MD5_CTX md5Context;
        MD5_Init(&md5Context);

        char buffer[8192];
        while (file.good()) {
            file.read(buffer, sizeof(buffer));
            MD5_Update(&md5Context, buffer, file.gcount());
        }

        unsigned char result[MD5_DIGEST_LENGTH];
        MD5_Final(result, &md5Context);

        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            ss << std::setw(2) << static_cast<int>(result[i]);
        }

        return ss.str();
    }

    // Calculate SHA-256 hash for a file
    static std::string calculateSHA256(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            return "";
        }

        SHA256_CTX sha256Context;
        SHA256_Init(&sha256Context);

        char buffer[8192];
        while (file.good()) {
            file.read(buffer, sizeof(buffer));
            SHA256_Update(&sha256Context, buffer, file.gcount());
        }

        unsigned char result[SHA256_DIGEST_LENGTH];
        SHA256_Final(result, &sha256Context);

        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::setw(2) << static_cast<int>(result[i]);
        }

        return ss.str();
    }

    // Compare two files byte by byte
    static bool compareFileContent(const std::string& file1Path, const std::string& file2Path) {
        std::ifstream file1(file1Path, std::ios::binary);
        std::ifstream file2(file2Path, std::ios::binary);

        if (!file1 || !file2) {
            return false;
        }

        const size_t BUFFER_SIZE = 8192;
        char buffer1[BUFFER_SIZE];
        char buffer2[BUFFER_SIZE];

        while (file1.good() && file2.good()) {
            file1.read(buffer1, BUFFER_SIZE);
            file2.read(buffer2, BUFFER_SIZE);

            size_t bytesRead1 = file1.gcount();
            size_t bytesRead2 = file2.gcount();

            if (bytesRead1 != bytesRead2) {
                return false;
            }

            if (memcmp(buffer1, buffer2, bytesRead1) != 0) {
                return false;
            }

            if (bytesRead1 < BUFFER_SIZE) {
                break;
            }
        }

        return file1.eof() && file2.eof();
    }

    // Helper to finish a result with timing
    VerifyResult finishResult(VerifyResult& result, const std::chrono::time_point<std::chrono::high_resolution_clock>& startTime) {
        auto endTime = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        return result;
    }

private:
    // File verification cache to avoid redundant operations
    struct CacheEntry {
        std::string hash;
        std::chrono::system_clock::time_point timestamp;
        uintmax_t fileSize;
    };

    std::unordered_map<std::string, CacheEntry> m_hashCache;
    std::mutex m_cacheMutex;

    // Cache a hash result
    void cacheHash(const std::string& filePath, const std::string& hash) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);

        CacheEntry entry;
        entry.hash = hash;
        entry.timestamp = std::chrono::system_clock::now();

        try {
            entry.fileSize = fs::file_size(filePath);
        } catch (...) {
            entry.fileSize = 0;
        }

        m_hashCache[filePath] = entry;
    }

    // Check if a cached hash is still valid
    bool isCacheValid(const std::string& filePath, std::string& cachedHash) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);

        auto it = m_hashCache.find(filePath);
        if (it == m_hashCache.end()) {
            return false;
        }

        // Check if the file has been modified since caching
        try {
            auto currentSize = fs::file_size(filePath);
            auto lastModified = fs::last_write_time(filePath);

            // Convert to system_clock time point
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                std::chrono::file_clock::to_sys(lastModified));

            // If file size changed or file was modified after we cached the hash
            if (currentSize != it->second.fileSize || sctp > it->second.timestamp) {
                return false;
            }

            cachedHash = it->second.hash;
            return true;
        } catch (...) {
            return false;
        }
    }

public:
    // Create a cache summary
    std::string getCacheSummary() {
        std::lock_guard<std::mutex> lock(m_cacheMutex);

        std::stringstream ss;
        ss << "Cache entries: " << m_hashCache.size() << std::endl;

        size_t totalMemory = 0;
        for (const auto& entry : m_hashCache) {
            // Rough estimate of memory usage
            totalMemory += entry.first.size() + entry.second.hash.size() + sizeof(entry.second);
        }

        ss << "Estimated memory usage: " << (totalMemory / 1024) << " KB" << std::endl;

        return ss.str();
    }

    // Clear the cache
    void clearCache() {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_hashCache.clear();
    }
};
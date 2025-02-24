//
// Created by garrett on 2/24/25.
//

#ifndef TRANSACTION_LOG_HPP
#define TRANSACTION_LOG_HPP

#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <json/json.h>  // Uses jsoncpp library
#include <atomic>
#include <optional>

namespace fs = std::filesystem;

// A class to manage transaction logging and recovery
class TransactionLog {
public:
    // Types of operations for the transaction log
    enum class OperationType {
        COPY,
        MOVE,
        DELETE,
        METADATA_UPDATE
    };

    // Status of a transaction
    enum class TransactionStatus {
        PENDING,
        IN_PROGRESS,
        COMPLETED,
        FAILED,
        ROLLED_BACK
    };

    // A single transaction record
    struct TransactionRecord {
        std::string id;
        OperationType operation;
        std::string sourcePath;
        std::string destPath;
        TransactionStatus status;
        std::chrono::system_clock::time_point timestamp;
        std::string errorMessage;
        std::optional<std::string> checksum;

        // Convert to JSON for storage
        Json::Value toJson() const {
            Json::Value json;
            json["id"] = id;
            json["operation"] = static_cast<int>(operation);
            json["sourcePath"] = sourcePath;
            json["destPath"] = destPath;
            json["status"] = static_cast<int>(status);
            json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count();
            json["errorMessage"] = errorMessage;
            if (checksum) {
                json["checksum"] = *checksum;
            }
            return json;
        }

        // Create from JSON
        static TransactionRecord fromJson(const Json::Value& json) {
            TransactionRecord record;
            record.id = json["id"].asString();
            record.operation = static_cast<OperationType>(json["operation"].asInt());
            record.sourcePath = json["sourcePath"].asString();
            record.destPath = json["destPath"].asString();
            record.status = static_cast<TransactionStatus>(json["status"].asInt());
            record.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(json["timestamp"].asInt64()));
            record.errorMessage = json["errorMessage"].asString();
            if (json.isMember("checksum")) {
                record.checksum = json["checksum"].asString();
            }
            return record;
        }
    };

    TransactionLog(const std::string& logDir)
        : m_logDir(logDir),
          m_nextId(1),
          m_isOpen(false) {
        // Create log directory if it doesn't exist
        if (!fs::exists(m_logDir)) {
            fs::create_directories(m_logDir);
        }

        // Find the current log file or create a new one
        initializeLog();
    }

    ~TransactionLog() {
        close();
    }

    // Open the transaction log
    bool open() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isOpen) return true;

        m_logStream.open(m_currentLogPath, std::ios::app);
        if (!m_logStream) {
            return false;
        }

        m_isOpen = true;
        return true;
    }

    // Close the transaction log
    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isOpen) return;

        m_logStream.close();
        m_isOpen = false;
    }

    // Log a new transaction
    std::string logTransaction(OperationType operation,
                           const std::string& sourcePath,
                           const std::string& destPath = "",
                           const std::optional<std::string>& checksum = std::nullopt) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isOpen && !open()) {
            return "";
        }

        std::string id = generateTransactionId();
        TransactionRecord record{
            id,
            operation,
            sourcePath,
            destPath,
            TransactionStatus::PENDING,
            std::chrono::system_clock::now(),
            "",
            checksum
        };

        writeRecord(record);
        return id;
    }

    // Update transaction status
    bool updateTransactionStatus(const std::string& id,
                              TransactionStatus status,
                              const std::string& errorMessage = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isOpen && !open()) {
            return false;
        }

        // Find transaction in memory cache or load from file
        auto record = findTransaction(id);
        if (!record) {
            return false;
        }

        record->status = status;
        record->errorMessage = errorMessage;
        record->timestamp = std::chrono::system_clock::now();

        writeRecord(*record);
        return true;
    }

    // Get transactions with a specific status
    std::vector<TransactionRecord> getTransactionsByStatus(TransactionStatus status) {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<TransactionRecord> result;
        loadAllTransactions();

        for (const auto& record : m_transactionCache) {
            if (record.second.status == status) {
                result.push_back(record.second);
            }
        }

        return result;
    }

    // Load all in-progress transactions for recovery
    std::vector<TransactionRecord> getPendingTransactions() {
        std::vector<TransactionRecord> pending;

        // Combine transactions in IN_PROGRESS and PENDING states
        auto inProgress = getTransactionsByStatus(TransactionStatus::IN_PROGRESS);
        auto pendingOnes = getTransactionsByStatus(TransactionStatus::PENDING);

        pending.insert(pending.end(), inProgress.begin(), inProgress.end());
        pending.insert(pending.end(), pendingOnes.begin(), pendingOnes.end());

        return pending;
    }

    // Rotate log files when they get too large
    bool rotateLogIfNeeded(size_t maxSize = 10 * 1024 * 1024) {  // Default 10MB
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!fs::exists(m_currentLogPath)) {
            return true;  // No need to rotate if file doesn't exist
        }

        uintmax_t fileSize = fs::file_size(m_currentLogPath);
        if (fileSize < maxSize) {
            return true;  // No need to rotate yet
        }

        // Close current log
        if (m_isOpen) {
            m_logStream.close();
            m_isOpen = false;
        }

        // Create a new log file
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_r(&now_time_t, &timeinfo);

        char buffer[80];
        strftime(buffer, 80, "%Y%m%d-%H%M%S", &timeinfo);

        std::string newLogPath = m_logDir + "/sync_log_" + buffer + ".json";

        // Archive the old log
        std::string archivePath = m_logDir + "/archive/";
        if (!fs::exists(archivePath)) {
            fs::create_directories(archivePath);
        }

        fs::path oldPath(m_currentLogPath);
        std::string oldFilename = oldPath.filename().string();
        fs::rename(m_currentLogPath, archivePath + oldFilename);

        // Set new log file as current
        m_currentLogPath = newLogPath;

        // Clear cache and re-open log
        m_transactionCache.clear();
        return open();
    }

private:
    std::string m_logDir;
    std::string m_currentLogPath;
    std::ofstream m_logStream;
    std::mutex m_mutex;
    std::atomic<uint64_t> m_nextId;
    bool m_isOpen;

    // In-memory cache of transactions
    std::unordered_map<std::string, TransactionRecord> m_transactionCache;

    // Initialize the log system
    void initializeLog() {
        // Find the most recent log file or create a new one
        std::vector<fs::path> logFiles;
        for (const auto& entry : fs::directory_iterator(m_logDir)) {
            if (entry.is_regular_file() && entry.path().string().find("sync_log_") != std::string::npos) {
                logFiles.push_back(entry.path());
            }
        }

        if (logFiles.empty()) {
            // Create a new log file
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            struct tm timeinfo;
            localtime_r(&now_time_t, &timeinfo);

            char buffer[80];
            strftime(buffer, 80, "%Y%m%d-%H%M%S", &timeinfo);

            m_currentLogPath = m_logDir + "/sync_log_" + buffer + ".json";
        } else {
            // Sort by modification time to find the newest
            std::sort(logFiles.begin(), logFiles.end(), [](const fs::path& a, const fs::path& b) {
                return fs::last_write_time(a) > fs::last_write_time(b);
            });

            m_currentLogPath = logFiles[0].string();
        }
    }

    // Generate a unique transaction ID
    std::string generateTransactionId() {
        uint64_t id = m_nextId++;
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        uint64_t timestamp = now_ms.time_since_epoch().count();

        return "tx-" + std::to_string(timestamp) + "-" + std::to_string(id);
    }

    // Write a transaction record to the log
    void writeRecord(const TransactionRecord& record) {
        Json::Value json = record.toJson();
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";  // Don't add indentation
        std::string jsonStr = Json::writeString(builder, json) + "\n";

        m_logStream << jsonStr;
        m_logStream.flush();

        // Update the cache
        m_transactionCache[record.id] = record;
    }

    // Find a transaction by ID
    std::optional<TransactionRecord> findTransaction(const std::string& id) {
        // Check the cache first
        auto it = m_transactionCache.find(id);
        if (it != m_transactionCache.end()) {
            return it->second;
        }

        // If not in cache, load all transactions
        loadAllTransactions();

        // Check again
        it = m_transactionCache.find(id);
        if (it != m_transactionCache.end()) {
            return it->second;
        }

        return std::nullopt;
    }

    // Load all transactions from the current log file
    void loadAllTransactions() {
        if (!fs::exists(m_currentLogPath)) {
            return;
        }

        // Close the log if it's open
        bool wasOpen = m_isOpen;
        if (m_isOpen) {
            m_logStream.close();
            m_isOpen = false;
        }

        // Open for reading
        std::ifstream inFile(m_currentLogPath);
        if (!inFile) {
            if (wasOpen) {
                open();  // Reopen if it was open
            }
            return;
        }

        // Clear the cache
        m_transactionCache.clear();

        // Read line by line
        std::string line;
        Json::CharReaderBuilder builder;
        Json::Value json;
        JSONCPP_STRING errs;

        while (std::getline(inFile, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            if (Json::parseFromStream(builder, iss, &json, &errs)) {
                TransactionRecord record = TransactionRecord::fromJson(json);
                m_transactionCache[record.id] = record;

                // Update next ID if needed
                if (record.id.find("tx-") == 0) {
                    size_t lastDash = record.id.find_last_of('-');
                    if (lastDash != std::string::npos) {
                        uint64_t id = std::stoull(record.id.substr(lastDash + 1));
                        if (id >= m_nextId) {
                            m_nextId = id + 1;
                        }
                    }
                }
            }
        }

        inFile.close();

        // Reopen for appending if needed
        if (wasOpen) {
            open();
        }
    }
};

#endif // TRANSACTION_LOG_HPP
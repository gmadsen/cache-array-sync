//
// Created by garrett on 2/24/25.
//

#ifndef ROBUST_SYNC_MANAGER_HPP
#define ROBUST_SYNC_MANAGER_HPP

#include "file_verification.hpp"
#include "transaction_log.hpp"
#include "priority_sync_queue.hpp"
#include "configuration.hpp"
#include "metrics_collector.hpp"
#include "file_system_monitor.hpp"

#include <filesystem>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <functional>
#include <future>
#include <memory>
#include <chrono>

namespace fs = std::filesystem;

// Robust synchronization manager with transaction logging, verification,
// and priority-based queuing
class RobustSyncManager {
public:
    RobustSyncManager(
        std::shared_ptr<Configuration> config,
        std::unique_ptr<MetricsCollector> metrics,
        const std::string& logDir = "/var/log/file_sync")
        : m_config(config),
          m_metrics(std::move(metrics)),
          m_transactionLog(logDir),
          m_running(false) {

        // Initialize the transaction log
        if (!m_transactionLog.open()) {
            throw std::runtime_error("Failed to open transaction log");
        }

        // Set up file verification
        m_fileVerifier = std::make_unique<FileVerification>();
    }

    ~RobustSyncManager() {
        stop();
    }

    // Start the sync manager
    void start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_running) {
            return;
        }

        m_running = true;

        // Start worker threads
        for (int i = 0; i < m_config->num_threads; ++i) {
            m_workers.emplace_back(&RobustSyncManager::workerThread, this);
        }

        // Start recovery thread
        m_recoveryThread = std::thread(&RobustSyncManager::recoveryWorker, this);

        // Start consistency check thread
        m_consistencyThread = std::thread(&RobustSyncManager::consistencyWorker, this);

        m_metrics->recordMetric("sync_manager", "started");
    }

    // Stop the sync manager
    void stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) {
            return;
        }

        m_running = false;
        m_syncQueue.shutdown();

        // Wait for worker threads to finish
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        // Wait for recovery thread
        if (m_recoveryThread.joinable()) {
            m_recoveryThread.join();
        }

        // Wait for consistency thread
        if (m_consistencyThread.joinable()) {
            m_consistencyThread.join();
        }

        m_workers.clear();

        // Close transaction log
        m_transactionLog.close();

        m_metrics->recordMetric("sync_manager", "stopped");
    }

    // Schedule a file for synchronization
    bool syncFile(const std::string& path, SyncPriority priority = SyncPriority::NORMAL) {
        if (!m_running) {
            return false;
        }

        SyncTask task(path, "SYNC", priority);
        bool queued = m_syncQueue.enqueue(task);

        if (queued) {
            m_metrics->recordMetric("file_queued", path);
        } else {
            m_metrics->recordMetric("file_queue_failed", path);
        }

        return queued;
    }

    // Schedule a batch of files
    bool batchSync(const std::vector<std::string>& paths, SyncPriority priority = SyncPriority::NORMAL) {
        if (!m_running) {
            return false;
        }

        bool allQueued = true;

        for (const auto& path : paths) {
            SyncTask task(path, "SYNC", priority);
            if (!m_syncQueue.enqueue(task)) {
                allQueued = false;
                m_metrics->recordMetric("file_queue_failed", path);
            } else {
                m_metrics->recordMetric("file_queued", path);
            }
        }

        return allQueued;
    }

    // Trigger a consistency check
    void performConsistencyCheck() {
        m_consistencyCheckRequested = true;
    }

    // Get current queue statistics
    std::string getQueueStats() {
        std::stringstream ss;
        ss << "Queue size: " << m_syncQueue.size() << std::endl;

        // Additional stats could be added here

        return ss.str();
    }

    // Get transaction log statistics
    std::string getTransactionStats() {
        std::stringstream ss;

        auto pendingTransactions = m_transactionLog.getPendingTransactions();
        ss << "Pending transactions: " << pendingTransactions.size() << std::endl;

        // Additional stats could be added here

        return ss.str();
    }

private:
    std::shared_ptr<Configuration> m_config;
    std::unique_ptr<MetricsCollector> m_metrics;
    std::unique_ptr<FileVerification> m_fileVerifier;
    TransactionLog m_transactionLog;
    PrioritySyncQueue m_syncQueue;

    std::vector<std::thread> m_workers;
    std::thread m_recoveryThread;
    std::thread m_consistencyThread;

    std::mutex m_mutex;
    std::atomic<bool> m_running;
    std::atomic<bool> m_consistencyCheckRequested{false};

    // Worker thread function to process tasks from the queue
    void workerThread() {
        while (m_running) {
            auto taskOpt = m_syncQueue.dequeue(std::chrono::milliseconds(100));

            if (taskOpt) {
                SyncTask task = *taskOpt;
                processTask(task);
            }
        }
    }

    // Process a single sync task
    void processTask(const SyncTask& task) {
        const std::string& sourcePath = task.getPath();

        // Determine destination path (this would be based on your configuration)
        std::string destPath = determineDestinationPath(sourcePath);

        // Log the transaction
        std::string txId = m_transactionLog.logTransaction(
            TransactionLog::OperationType::COPY,
            sourcePath,
            destPath
        );

        if (txId.empty()) {
            m_metrics->recordMetric("tx_log_failed", sourcePath);
            return;
        }

        m_metrics->recordMetric("tx_started", txId);

        // Update transaction status to in-progress
        m_transactionLog.updateTransactionStatus(
            txId,
            TransactionLog::TransactionStatus::IN_PROGRESS
        );

        // Perform the actual sync operation
        bool success = performSyncOperation(sourcePath, destPath);

        // Verify the sync was successful
        bool verified = false;
        std::string errorMsg;

        if (success) {
            auto result = m_fileVerifier->verifyFile(sourcePath, destPath);
            verified = result.matches;
            errorMsg = result.errorMessage;

            m_metrics->recordMetric("sync_verification",
                                    verified ? "success" : "failed: " + errorMsg);
        } else {
            errorMsg = "Sync operation failed";
        }

        // Update transaction status based on result
        if (success && verified) {
            m_transactionLog.updateTransactionStatus(
                txId,
                TransactionLog::TransactionStatus::COMPLETED
            );
            m_metrics->recordMetric("tx_completed", txId);
        } else {
            m_transactionLog.updateTransactionStatus(
                txId,
                TransactionLog::TransactionStatus::FAILED,
                errorMsg
            );
            m_metrics->recordMetric("tx_failed", txId + ": " + errorMsg);

            // Handle retry logic if needed
            if (task.getRetryCount() < 3) {
                SyncTask retryTask = task;
                const_cast<SyncTask&>(retryTask).incrementRetry();
                const_cast<SyncTask&>(retryTask).setStatus("retry");

                // Requeue with a delay
                std::this_thread::sleep_for(std::chrono::seconds(5));
                m_syncQueue.enqueue(retryTask);
                m_metrics->recordMetric("tx_retry", txId);
            }
        }
    }

    // Determine the destination path for a source file
    std::string determineDestinationPath(const std::string& sourcePath) {
        // This would be based on your specific synchronization rules
        // For now, this is a dummy implementation

        // Example: Replace source directory with destination directory
        std::string sourceRoot = "/path/to/source";
        std::string destRoot = "/path/to/destination";

        if (sourcePath.find(sourceRoot) == 0) {
            return destRoot + sourcePath.substr(sourceRoot.length());
        }

        return "/path/to/destination/" + fs::path(sourcePath).filename().string();
    }

    // Perform the actual synchronization operation
    bool performSyncOperation(const std::string& sourcePath, const std::string& destPath) {
        try {
            // Make sure destination directory exists
            fs::path destDir = fs::path(destPath).parent_path();
            if (!fs::exists(destDir)) {
                fs::create_directories(destDir);
            }

            // Copy the file with overwrite
            fs::copy(sourcePath, destPath, fs::copy_options::overwrite_existing);

            // Preserve timestamps
            auto sourceTime = fs::last_write_time(sourcePath);
            fs::last_write_time(destPath, sourceTime);

            return true;
        } catch (const std::exception& e) {
            m_metrics->recordMetric("sync_error", std::string(e.what()) + ": " + sourcePath);
            return false;
        }
    }

    // Worker to handle recovery of failed transactions
    void recoveryWorker() {
        while (m_running) {
            // Run recovery every minute
            std::this_thread::sleep_for(std::chrono::minutes(1));

            if (!m_running) break;

            try {
                // Get all pending and in-progress transactions
                auto pendingTransactions = m_transactionLog.getPendingTransactions();

                if (!pendingTransactions.empty()) {
                    m_metrics->recordMetric("recovery_started",
                                         "found " + std::to_string(pendingTransactions.size()) + " transactions");
                }

                for (const auto& tx : pendingTransactions) {
                    // Skip if the transaction is too new (< 5 minutes)
                    auto now = std::chrono::system_clock::now();
                    auto txAge = now - tx.timestamp;
                    if (txAge < std::chrono::minutes(5)) {
                        continue;
                    }

                    // Try to recover the transaction
                    recoverTransaction(tx);
                }
            } catch (const std::exception& e) {
                m_metrics->recordMetric("recovery_error", e.what());
            }
        }
    }

    // Recover a failed transaction
    void recoverTransaction(const TransactionLog::TransactionRecord& tx) {
        m_metrics->recordMetric("tx_recovery_attempt", tx.id);

        // Check if source still exists
        if (!fs::exists(tx.sourcePath)) {
            m_transactionLog.updateTransactionStatus(
                tx.id,
                TransactionLog::TransactionStatus::FAILED,
                "Source file no longer exists"
            );
            m_metrics->recordMetric("tx_recovery_failed", tx.id + ": source missing");
            return;
        }

        // Create a sync task for the file
        SyncTask task(tx.sourcePath, "RECOVERY", SyncPriority::HIGH);

        // Queue it for processing
        if (m_syncQueue.enqueue(task)) {
            m_metrics->recordMetric("tx_recovery_queued", tx.id);
        } else {
            m_metrics->recordMetric("tx_recovery_queue_failed", tx.id);
        }
    }

    // Worker to perform periodic consistency checks
    void consistencyWorker() {
        while (m_running) {
            // Run consistency check every 6 hours or when requested
            for (int i = 0; i < 360 && m_running; ++i) { // 6 hours = 360 minutes
                std::this_thread::sleep_for(std::chrono::minutes(1));
                if (m_consistencyCheckRequested) {
                    break;
                }
            }

            if (!m_running) break;

            m_consistencyCheckRequested = false;

            try {
                performFullConsistencyCheck();
            } catch (const std::exception& e) {
                m_metrics->recordMetric("consistency_check_error", e.what());
            }
        }
    }

    // Perform a full consistency check between source and destination
    void performFullConsistencyCheck() {
        m_metrics->recordMetric("consistency_check", "started");

        // Source and destination directories (from config in real implementation)
        std::string sourceDir = "/path/to/source";
        std::string destDir = "/path/to/destination";

        // Verify directories recursively
        auto results = m_fileVerifier->verifyDirectory(
            sourceDir,
            destDir,
            FileVerification::VerifyMethod::FAST_HASH,
            true,
            m_config->num_threads
        );

        int totalFiles = 0;
        int mismatches = 0;

        // Process results and queue mismatched files for sync
        for (const auto& result : results) {
            totalFiles++;

            if (!result.second.matches) {
                mismatches++;

                // Create full path
                std::string fullPath = (fs::path(sourceDir) / result.first).string();

                // Queue for sync
                SyncTask task(fullPath, "CONSISTENCY", SyncPriority::LOW);
                m_syncQueue.enqueue(task);

                m_metrics->recordMetric("consistency_mismatch", result.first);
            }
        }

        m_metrics->recordMetric("consistency_check_complete",
                            "Files: " + std::to_string(totalFiles) +
                            ", Mismatches: " + std::to_string(mismatches));
    }
};

#endif // ROBUST_SYNC_MANAGER_HPP
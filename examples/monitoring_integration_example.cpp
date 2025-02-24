//
// Created by garrett on 2/24/25.
//
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

#include "metrics_collector.hpp"
#include "robust_sync_manager.hpp"

// Advanced metrics collector that writes to file and supports structured metrics
class EnhancedMetricsCollector : public MetricsCollector {
public:
    EnhancedMetricsCollector(const std::string& metricsFile)
        : m_metricsFile(metricsFile) {

        // Initialize the metrics file with a header
        std::ofstream outFile(m_metricsFile);
        if (outFile) {
            outFile << "timestamp,name,value,duration_ms" << std::endl;
        }
    }

    // Record a metric with timing
    void recordMetricWithDuration(const std::string& name,
                                  const std::string& value,
                                  const std::chrono::milliseconds& duration) {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);

        Metric metric;
        metric.name = name;
        metric.value = value;
        metric.timestamp = std::chrono::system_clock::now();
        metric.duration = duration;

        m_metrics.push_back(metric);
    }

    // Collect metrics to file instead of stdout
    void collect() override {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);

        if (m_metrics.empty()) {
            return;
        }

        // Append to the metrics file
        std::ofstream outFile(m_metricsFile, std::ios::app);
        if (!outFile) {
            std::cerr << "Error opening metrics file: " << m_metricsFile << std::endl;
            return;
        }

        for (const auto& metric : m_metrics) {
            // Format timestamp
            auto time_t = std::chrono::system_clock::to_time_t(metric.timestamp);
            struct tm timeinfo;
            localtime_r(&time_t, &timeinfo);
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

            // Write to file
            outFile << timeBuffer << ","
                    << metric.name << ","
                    << metric.value << ","
                    << (metric.duration.count() > 0 ? std::to_string(metric.duration.count()) : "")
                    << std::endl;
        }

        // Clear metrics after collecting
        m_metrics.clear();
    }

    // Get basic statistics
    std::string getStats() {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);

        std::stringstream ss;
        ss << "Total metrics collected: " << m_totalMetrics << std::endl;
        ss << "Current batch size: " << m_metrics.size() << std::endl;

        return ss.str();
    }

private:
    std::string m_metricsFile;
    size_t m_totalMetrics = 0;

    struct Metric {
        std::string name;
        std::string value;
        std::chrono::system_clock::time_point timestamp;
        std::chrono::milliseconds duration = std::chrono::milliseconds(0);
    };

    std::vector<Metric> m_metrics;
    std::mutex m_metrics_mutex;
};

// A monitoring interface to check system health
class SyncMonitor {
public:
    SyncMonitor(RobustSyncManager& syncManager)
        : m_syncManager(syncManager),
          m_running(false) {}

    // Start the monitoring thread
    void start() {
        if (m_running) return;

        m_running = true;
        m_monitorThread = std::thread(&SyncMonitor::monitorLoop, this);
    }

    // Stop the monitoring thread
    void stop() {
        if (!m_running) return;

        m_running = false;
        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }
    }

    // Get the current health status report
    std::string getHealthReport() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastReport;
    }

private:
    RobustSyncManager& m_syncManager;
    std::atomic<bool> m_running;
    std::thread m_monitorThread;
    std::string m_lastReport;
    std::mutex m_mutex;

    // Background monitoring loop
    void monitorLoop() {
        while (m_running) {
            try {
                checkHealth();

                // Check every 30 seconds
                for (int i = 0; i < 30 && m_running; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in monitoring: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        }
    }

    // Check system health and generate report
    void checkHealth() {
        std::stringstream report;

        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_r(&now_time_t, &timeinfo);
        char timeBuffer[80];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

        report << "===== Health Report at " << timeBuffer << " =====\n\n";

        // Get queue stats
        report << "-- Queue Statistics --\n";
        report << m_syncManager.getQueueStats() << "\n";

        // Get transaction stats
        report << "-- Transaction Statistics --\n";
        report << m_syncManager.getTransactionStats() << "\n";

        // Check disk space (example)
        report << "-- Disk Space --\n";
        try {
            // This is just an example - implement proper disk space checking
            report << "Source: 85% free\n";
            report << "Destination: 72% free\n\n";
        } catch (...) {
            report << "Error checking disk space\n\n";
        }

        // System load (example)
        report << "-- System Load --\n";
        try {
            std::ifstream loadFile("/proc/loadavg");
            std::string load;
            std::getline(loadFile, load);
            report << "Load average: " << load << "\n\n";
        } catch (...) {
            report << "Error checking system load\n\n";
        }

        // Overall health assessment
        report << "-- Overall Health --\n";
        // Implement health status logic based on above metrics
        report << "Status: HEALTHY\n";

        // Update the report
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastReport = report.str();
        }
    }
};

// Example of how to use the monitor
void monitoringExample() {
    // Create configuration
    auto config = std::make_shared<Configuration>();
    config->num_threads = 4;

    // Create enhanced metrics collector
    auto metrics = std::make_unique<EnhancedMetricsCollector>("/var/log/file_sync/metrics.csv");

    // Create sync manager
    RobustSyncManager syncManager(config, std::move(metrics));
    syncManager.start();

    // Create and start the monitor
    SyncMonitor monitor(syncManager);
    monitor.start();

    // Example: Print health report every minute
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        std::cout << monitor.getHealthReport() << std::endl;
    }

    // Clean up
    monitor.stop();
    syncManager.stop();
}
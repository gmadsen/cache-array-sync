//
// Created by garrett on 2/24/25.
//
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <string>
#include <vector>

#include "configuration.hpp"
#include "thread_pool.hpp"
#include "file_system_monitor.hpp"
#include "metrics_collector.hpp"
#include "robust_sync_manager.hpp"

// Global flag for graceful shutdown
std::atomic<bool> running(true);

// Signal handler
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);  // Ctrl+C
        std::signal(SIGTERM, signalHandler); // Termination request

        // Initialize configuration
        auto config = std::make_shared<Configuration>();
        config->num_threads = std::thread::hardware_concurrency();

        // Set up metrics collector
        auto metrics = std::make_unique<MetricsCollector>();

        // Create log directory if it doesn't exist
        std::string logDir = "/var/log/file_sync";
        fs::create_directories(logDir);

        // Create the robust sync manager
        RobustSyncManager syncManager(config, std::move(metrics), logDir);

        // Start the sync manager
        syncManager.start();
        std::cout << "Sync manager started with " << config->num_threads << " threads" << std::endl;

        // Set up file system monitor
        FileSystemMonitor monitor;

        // Add watches to directories (customize these paths)
        std::string watchDir = "/path/to/watch";
        monitor.addWatch(watchDir);

        // Setup callback from monitor to sync manager
        monitor.setCallback([&syncManager](const std::string& path) {
            // Queue file for synchronization
            syncManager.syncFile(path);

            // If it's a directory, we might want to add it to the watch list
            if (fs::is_directory(path)) {
                // This would require extending your FileSystemMonitor class
                // to support adding watches from the callback
            }
        });

        // Main event loop
        std::cout << "Entering main event loop" << std::endl;
        while (running) {
            // Process file system events
            while (!monitor.empty() && running) {
                auto event = monitor.getNextEvent();
                if (event) {
                    // The callback will handle queuing the file for sync
                    std::cout << "Detected event: " << event->action <<
                                 " on " << event->path << std::endl;
                }
            }

            // Periodic tasks
            static auto lastConsistencyCheck = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();

            // Run consistency check every 12 hours
            if (now - lastConsistencyCheck > std::chrono::hours(12)) {
                std::cout << "Triggering scheduled consistency check" << std::endl;
                syncManager.performConsistencyCheck();
                lastConsistencyCheck = now;
            }

            // Prevent CPU overload
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Graceful shutdown
        std::cout << "Shutting down sync manager..." << std::endl;
        syncManager.stop();

        std::cout << "File synchronization service stopped" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
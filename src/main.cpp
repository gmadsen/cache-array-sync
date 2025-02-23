// main file for cache synchronization
// Created by: Garrett Madsen





#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <csignal>


#include "configuration.hpp"
#include "thread_pool.hpp"
#include "file_system_monitor.hpp"
#include "metrics_collector.hpp"
#include "sync_manager.hpp"

std::atomic<bool> running(true);

void eventLoop(ThreadPool& pool, FileSystemMonitor& monitor, MetricsCollector& metrics, SyncManager& sync_manager) {
    while (running) {
        while (!monitor.empty())   {
            // Process all pending events
            auto event = monitor.getNextEvent();
            pool.enqueue([&] () {
                // Decides whether to copy/move/delete based on timestamps, checksums, or filesystem metadata.
                sync_manager.syncFile(event.value().path);
            });
        }
        // Periodic consistency check (every 5 mins)
        static auto last_check = std::chrono::steady_clock::now();
        if (std::chrono::steady_clock::now() - last_check > std::chrono::minutes(5)) {
            pool.enqueue([&]() {
                sync_manager.performConsistencyCheck(); // Check for consistency
            });
            last_check = std::chrono::steady_clock::now();
        }

        // Periodic metrics collection (every 1 min)
        metrics.collect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for 100ms to prevent CPU overload
    }
}

int main() {

    ThreadPool pool;
     pool.start(std::thread::hardware_concurrency()); // Create a ThreadPool with the number of threads equal to the number of hardware threads

    FileSystemMonitor monitor;                  // Set up inotify/fanotify
    monitor.addWatch("/path/to/watch"); // Set up inotify/fanotify

    auto metrics = std::make_unique<MetricsCollector>();                          // Initialize metrics collector
    Configuration config;
    SyncManager sync_manager{std::make_shared<Configuration>(config), std::move(metrics)};                        // Create a SyncManager

    std::thread eventThread(eventLoop, std::ref(pool), std::ref(monitor), std::ref(*metrics), std::ref(sync_manager)); // Start the event loop in a separate thread

    // Graceful shutdown handling
    std::signal(SIGINT, [](int) {running = false;}); // Handle SIGINT (Ctrl+C) to stop the event loop

    eventThread.join(); // Wait for the event loop to finish

    return 0;
}
//
// Created by garrett on 2/24/25.
//
#include <gtest/gtest.h>
#include "mock_file_system_monitor.hpp"
#include "sync_manager.hpp"
#include "configuration.hpp"
#include "metrics_collector.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>

class MockFileSystemMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test that event simulation works
TEST_F(MockFileSystemMonitorTest, SimulateEvent) {
    MockFileSystemMonitor monitor;

    std::string eventPath;
    bool callbackCalled = false;

    // Set up callback
    monitor.setCallback([&callbackCalled, &eventPath](const std::string& path) {
        callbackCalled = true;
        eventPath = path;
    });

    // Simulate an event
    monitor.simulateEvent("/test/path", "MODIFY", 0);

    // Check that the callback was called
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(eventPath, "/test/path");

    // Check that we can retrieve the event
    EXPECT_FALSE(monitor.empty());

    auto event = monitor.getNextEvent();
    EXPECT_TRUE(event.has_value());
    EXPECT_EQ(event->path, "/test/path");
    EXPECT_EQ(event->action, "MODIFY");

    // Queue should be empty now
    EXPECT_TRUE(monitor.empty());
}

// Test multiple events
TEST_F(MockFileSystemMonitorTest, MultipleEvents) {
    MockFileSystemMonitor monitor;

    // Simulate multiple events
    monitor.simulateEvent("/test/path1", "CREATE", 1);
    monitor.simulateEvent("/test/path2", "MODIFY", 2);
    monitor.simulateEvent("/test/path3", "DELETE", 4);

    // Check that we can retrieve all events in order
    EXPECT_FALSE(monitor.empty());

    auto event1 = monitor.getNextEvent();
    EXPECT_TRUE(event1.has_value());
    EXPECT_EQ(event1->path, "/test/path1");
    EXPECT_EQ(event1->action, "CREATE");
    EXPECT_EQ(event1->mask, 1);

    auto event2 = monitor.getNextEvent();
    EXPECT_TRUE(event2.has_value());
    EXPECT_EQ(event2->path, "/test/path2");
    EXPECT_EQ(event2->action, "MODIFY");
    EXPECT_EQ(event2->mask, 2);

    auto event3 = monitor.getNextEvent();
    EXPECT_TRUE(event3.has_value());
    EXPECT_EQ(event3->path, "/test/path3");
    EXPECT_EQ(event3->action, "DELETE");
    EXPECT_EQ(event3->mask, 4);

    // Queue should be empty after retrieving all events
    EXPECT_TRUE(monitor.empty());
}

// Test watching multiple paths
TEST_F(MockFileSystemMonitorTest, WatchMultiplePaths) {
    MockFileSystemMonitor monitor;

    std::vector<std::string> paths = {
        "/test/path1",
        "/test/path2",
        "/test/path3"
    };

    std::vector<std::string> notifiedPaths;

    // Set up callback to record notifications
    monitor.setCallback([&notifiedPaths](const std::string& path) {
        notifiedPaths.push_back(path);
    });

    // Add watches for all paths
    for (const auto& path : paths) {
        monitor.addWatch(path);
    }

    // Verify all paths were notified via callback
    EXPECT_EQ(notifiedPaths.size(), paths.size());
    for (const auto& path : paths) {
        EXPECT_TRUE(std::find(notifiedPaths.begin(), notifiedPaths.end(), path) != notifiedPaths.end());
    }
}

// Test concurrent event handling
TEST_F(MockFileSystemMonitorTest, ConcurrentEvents) {
    MockFileSystemMonitor monitor;
    std::atomic<int> callbackCount{0};

    // Set up callback to count calls
    monitor.setCallback([&callbackCount](const std::string&) {
        callbackCount++;
    });

    // Simulate events from multiple threads
    const int numThreads = 5;
    const int eventsPerThread = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&monitor, i, eventsPerThread]() {
            for (int j = 0; j < eventsPerThread; ++j) {
                std::string path = "/test/thread" + std::to_string(i) + "/event" + std::to_string(j);
                monitor.simulateEvent(path, "MODIFY", i * 100 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Small delay to mix events
            }
        });
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    // Verify callback was called for each event
    EXPECT_EQ(callbackCount, numThreads * eventsPerThread);

    // Verify we can retrieve all events
    int eventCount = 0;
    while (!monitor.empty()) {
        auto event = monitor.getNextEvent();
        EXPECT_TRUE(event.has_value());
        eventCount++;
    }

    EXPECT_EQ(eventCount, numThreads * eventsPerThread);
}

// Test integration with SyncManager
TEST_F(MockFileSystemMonitorTest, IntegrationWithSyncManager) {
    MockFileSystemMonitor monitor;
    std::shared_ptr<Configuration> config = std::make_shared<Configuration>();
    std::unique_ptr<MetricsCollector> metrics = std::make_unique<MetricsCollector>();

    // Redirect cout to avoid cluttering test output
    std::stringstream dummyStream;
    auto originalBuffer = std::cout.rdbuf();
    std::cout.rdbuf(dummyStream.rdbuf());

    // Create SyncManager
    SyncManager syncManager(config, std::move(metrics));

    // Setup event processing
    std::atomic<int> processedFiles{0};
    std::vector<std::string> processedPaths;
    std::mutex pathsMutex;

    // Thread to process events
    std::thread processingThread([&]() {
        while (processedFiles < 10) { // Process up to 10 events
            if (!monitor.empty()) {
                auto event = monitor.getNextEvent();
                if (event) {
                    syncManager.syncFile(event->path);

                    // Record that we processed this path
                    {
                        std::lock_guard<std::mutex> lock(pathsMutex);
                        processedPaths.push_back(event->path);
                    }

                    processedFiles++;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Simulate events
    for (int i = 0; i < 10; ++i) {
        std::string path = "/test/integration/file" + std::to_string(i) + ".txt";
        monitor.simulateEvent(path, "MODIFY", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Wait for processing to complete
    processingThread.join();

    // Restore cout
    std::cout.rdbuf(originalBuffer);

    // Verify all paths were processed
    EXPECT_EQ(processedFiles, 10);

    // Check all expected paths were processed
    std::lock_guard<std::mutex> lock(pathsMutex);
    EXPECT_EQ(processedPaths.size(), 10);
    for (int i = 0; i < 10; ++i) {
        std::string expectedPath = "/test/integration/file" + std::to_string(i) + ".txt";
        bool found = std::find(processedPaths.begin(), processedPaths.end(), expectedPath) != processedPaths.end();
        EXPECT_TRUE(found) << "Path not processed: " << expectedPath;
    }
}
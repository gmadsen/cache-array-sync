//
// Created by garrett on 2/24/25.
//
#include <gtest/gtest.h>
#include "thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <thread>

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test that the thread pool can execute a simple task
TEST_F(ThreadPoolTest, ExecutesSingleTask) {
    ThreadPool pool;
    pool.start(2);

    std::atomic<bool> taskExecuted{false};

    pool.enqueue([&taskExecuted]() {
        taskExecuted = true;
    });

    // Give some time for the task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(taskExecuted);
}

// Test that the thread pool can execute multiple tasks
TEST_F(ThreadPoolTest, ExecutesMultipleTasks) {
    ThreadPool pool;
    pool.start(4);

    std::atomic<int> counter{0};
    const int taskCount = 100;

    for(int i = 0; i < taskCount; ++i) {
        pool.enqueue([&counter]() {
            counter++;
        });
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(counter, taskCount);
}

// Test that tasks are distributed among threads
TEST_F(ThreadPoolTest, TasksAreDistributed) {
    ThreadPool pool;
    const int numThreads = 4;
    pool.start(numThreads);

    std::mutex mutex;
    std::set<std::thread::id> threadIds;
    const int taskCount = 100;

    for(int i = 0; i < taskCount; ++i) {
        pool.enqueue([&mutex, &threadIds]() {
            std::lock_guard<std::mutex> lock(mutex);
            threadIds.insert(std::this_thread::get_id());
            // Small sleep to force thread switching
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // We should see tasks executed on different threads
    EXPECT_GT(threadIds.size(), 1);
    // Ideally, all threads should be used
    EXPECT_LE(threadIds.size(), numThreads);
}

// Test concurrent enqueueing
TEST_F(ThreadPoolTest, ConcurrentEnqueuing) {
    ThreadPool pool;
    pool.start(2);

    std::atomic<int> counter{0};
    const int taskCount = 1000;
    const int numEnqueuers = 5;

    std::vector<std::thread> enqueuers;
    for(int i = 0; i < numEnqueuers; ++i) {
        enqueuers.emplace_back([&]() {
            for(int j = 0; j < taskCount/numEnqueuers; ++j) {
                pool.enqueue([&counter]() {
                    counter++;
                });
            }
        });
    }

    // Wait for all enqueuers to finish
    for(auto& t : enqueuers) {
        t.join();
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_EQ(counter, taskCount);
}
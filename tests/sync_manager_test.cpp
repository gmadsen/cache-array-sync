//
// Created by garrett on 2/24/25.
//
#include <gtest/gtest.h>
#include "sync_manager.hpp"
#include "configuration.hpp"
#include "metrics_collector.hpp"
#include <sstream>
#include <memory>

class SyncManagerTest : public ::testing::Test {
protected:
    std::shared_ptr<Configuration> config;
    std::unique_ptr<MetricsCollector> metrics;

    // For capturing console output
    std::streambuf* originalBuffer;
    std::stringstream capturedOutput;

    void SetUp() override {
        // Create configuration and metrics for testing
        config = std::make_shared<Configuration>();
        metrics = std::make_unique<MetricsCollector>();

        // Redirect cout to our stringstream
        originalBuffer = std::cout.rdbuf();
        std::cout.rdbuf(capturedOutput.rdbuf());
    }

    void TearDown() override {
        // Restore original cout buffer
        std::cout.rdbuf(originalBuffer);
    }

    // Helper to get captured output and reset stream
    std::string getCapturedOutput() {
        std::string output = capturedOutput.str();
        capturedOutput.str("");
        return output;
    }
};

// Test SyncManager construction
TEST_F(SyncManagerTest, Construction) {
    EXPECT_NO_THROW({
        SyncManager manager(config, std::move(metrics));
    });

    // Reset metrics for other tests
    metrics = std::make_unique<MetricsCollector>();
}

// Test syncData() with no arguments
TEST_F(SyncManagerTest, SyncDataNoArgs) {
    SyncManager manager(config, std::move(metrics));

    manager.syncData();
    std::string output = getCapturedOutput();

    EXPECT_TRUE(output.find("Syncing data") != std::string::npos);
}

// Test syncData() with string argument
TEST_F(SyncManagerTest, SyncDataWithString) {
    SyncManager manager(config, std::move(metrics));

    manager.syncData("test_data");
    std::string output = getCapturedOutput();

    EXPECT_TRUE(output.find("Syncing data: test_data") != std::string::npos);
}

// Test syncData() with vector argument
TEST_F(SyncManagerTest, SyncDataWithVector) {
    SyncManager manager(config, std::move(metrics));

    std::vector<std::string> dataVector = {"item1", "item2", "item3"};
    manager.syncData(dataVector);
    std::string output = getCapturedOutput();

    EXPECT_TRUE(output.find("item1") != std::string::npos);
    EXPECT_TRUE(output.find("item2") != std::string::npos);
    EXPECT_TRUE(output.find("item3") != std::string::npos);
}

// Test syncFile()
TEST_F(SyncManagerTest, SyncFile) {
    SyncManager manager(config, std::move(metrics));

    manager.syncFile("/path/to/test/file.txt");
    std::string output = getCapturedOutput();

    EXPECT_TRUE(output.find("Syncing file: /path/to/test/file.txt") != std::string::npos);
}

// Test batchSync()
TEST_F(SyncManagerTest, BatchSync) {
    SyncManager manager(config, std::move(metrics));

    std::vector<std::string> paths = {"/path1", "/path2", "/path3"};
    manager.batchSync(paths);
    std::string output = getCapturedOutput();

    EXPECT_TRUE(output.find("/path1") != std::string::npos);
    EXPECT_TRUE(output.find("/path2") != std::string::npos);
    EXPECT_TRUE(output.find("/path3") != std::string::npos);
}

// Test performConsistencyCheck()
TEST_F(SyncManagerTest, PerformConsistencyCheck) {
    SyncManager manager(config, std::move(metrics));

    manager.performConsistencyCheck();
    std::string output = getCapturedOutput();

    EXPECT_TRUE(output.find("Performing consistency check") != std::string::npos);
}

// More advanced tests would include mocking the filesystem interactions
// and testing various edge cases and failure scenarios
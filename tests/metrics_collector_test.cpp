//
// Created by garrett on 2/24/25.
//
#include <gtest/gtest.h>
#include "metrics_collector.hpp"
#include <sstream>
#include <thread>

class MetricsCollectorTest : public ::testing::Test {
protected:
    // Redirect std::cout to our stringstream
    std::streambuf* originalBuffer;
    std::stringstream capturedOutput;

    void SetUp() override {
        // Redirect cout to our stringstream
        originalBuffer = std::cout.rdbuf();
        std::cout.rdbuf(capturedOutput.rdbuf());
    }

    void TearDown() override {
        // Restore original cout buffer
        std::cout.rdbuf(originalBuffer);
    }
};

// Test that metrics can be recorded
TEST_F(MetricsCollectorTest, RecordMetric) {
    MetricsCollector collector;
    collector.recordMetric("test_metric", "test_value");

    // Since the metrics are stored internally, we don't have a direct way to test
    // until we call collect()

    // Clear the output from setup
    capturedOutput.str("");

    // Call collect to output the metrics
    collector.collect();

    // Check if the output contains our metric
    std::string output = capturedOutput.str();
    EXPECT_TRUE(output.find("test_metric: test_value") != std::string::npos);
}

// Test that collect clears metrics after outputting them
TEST_F(MetricsCollectorTest, CollectClearsMetrics) {
    MetricsCollector collector;
    collector.recordMetric("test_metric", "test_value");

    // Call collect once to output metrics
    capturedOutput.str("");
    collector.collect();
    std::string firstOutput = capturedOutput.str();

    // Call collect again - should output nothing
    capturedOutput.str("");
    collector.collect();
    std::string secondOutput = capturedOutput.str();

    // Check that the first output contained our metric
    EXPECT_TRUE(firstOutput.find("test_metric: test_value") != std::string::npos);

    // Check that the second output is empty (or at least doesn't contain our metric)
    EXPECT_TRUE(secondOutput.find("test_metric: test_value") == std::string::npos);
}

// Test that multiple metrics can be recorded and collected
TEST_F(MetricsCollectorTest, MultipleMetrics) {
    MetricsCollector collector;
    collector.recordMetric("metric1", "value1");
    collector.recordMetric("metric2", "value2");
    collector.recordMetric("metric3", "value3");

    capturedOutput.str("");
    collector.collect();
    std::string output = capturedOutput.str();

    EXPECT_TRUE(output.find("metric1: value1") != std::string::npos);
    EXPECT_TRUE(output.find("metric2: value2") != std::string::npos);
    EXPECT_TRUE(output.find("metric3: value3") != std::string::npos);
}

// Test thread safety with concurrent recordMetric calls
TEST_F(MetricsCollectorTest, ConcurrentRecording) {
    MetricsCollector collector;
    const int numThreads = 10;
    const int metricsPerThread = 100;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&collector, i, metricsPerThread]() {
            for (int j = 0; j < metricsPerThread; ++j) {
                std::string metricName = "thread" + std::to_string(i) + "_metric" + std::to_string(j);
                std::string metricValue = "value" + std::to_string(j);
                collector.recordMetric(metricName, metricValue);
            }
        });
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    // Collect and count metrics
    capturedOutput.str("");
    collector.collect();
    std::string output = capturedOutput.str();

    // Simple counting of lines to verify all metrics were recorded
    int lineCount = 0;
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            lineCount++;
        }
    }

    // We should have recorded numThreads * metricsPerThread metrics
    EXPECT_EQ(lineCount, numThreads * metricsPerThread);
}
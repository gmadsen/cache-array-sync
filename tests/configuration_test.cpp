//
// Created by garrett on 2/24/25.
//
#include <gtest/gtest.h>
#include "configuration.hpp"

class ConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test default configuration values
TEST_F(ConfigurationTest, DefaultValues) {
    Configuration config;
    EXPECT_EQ(config.num_threads, 1);
}

// Test updating configuration values
TEST_F(ConfigurationTest, UpdateValues) {
    Configuration config;
    config.num_threads = 4;
    EXPECT_EQ(config.num_threads, 4);
}

// Add more tests as you expand your Configuration class functionality
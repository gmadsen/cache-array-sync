# File Synchronization Project Testing Guide

This guide explains how to set up and run tests for the file synchronization project.

## Directory Structure

After setting up the test environment, your project structure should look like:

```
file_sync/
├── CMakeLists.txt
├── build/               # Build directory (auto-created)
├── include/             # Header files
│   ├── configuration.hpp
│   ├── file_system_monitor.hpp
│   ├── metrics_collector.hpp
│   ├── sync_manager.hpp
│   └── thread_pool.hpp
├── src/                 # Source files
│   ├── configuration.cpp
│   ├── file_system_monitor.cpp
│   ├── main.cpp
│   ├── metrics_collector.cpp
│   ├── sync_manager.cpp
│   └── thread_pool.cpp
├── tests/               # Test files
│   ├── CMakeLists.txt
│   ├── configuration_test.cpp
│   ├── file_system_monitor_test.cpp
│   ├── metrics_collector_test.cpp
│   ├── mock_file_system_monitor.hpp
│   ├── sync_manager_test.cpp
│   └── thread_pool_test.cpp
└── build_and_test.sh    # Convenience script
```

## Setting Up the Test Environment

1. **Dependencies**:
    - GTest will be automatically downloaded by CMake
    - Make sure you have CMake (3.14+) installed
    - You'll need a C++17-compatible compiler

2. **Creating Test Files**:
    - Copy the provided test files to your `tests/` directory
    - Create a new `tests/CMakeLists.txt` file using the provided template
    - Update your project's root `CMakeLists.txt` to include testing support

3. **Building the Project**:
   ```bash
   mkdir -p build
   cd build
   cmake ..
   cmake --build .
   ```

## Running Tests

You can run the tests in several ways:

### Using CTest
From the build directory:
```bash
ctest --output-on-failure
```

### Running Individual Tests
From the build directory:
```bash
./tests/file_sync_tests
```

You can also run specific test groups:
```bash
./tests/file_sync_tests --gtest_filter=ThreadPoolTest.*
```

### Using the Convenience Script
```bash
chmod +x build_and_test.sh
./build_and_test.sh
```

## Writing New Tests

To add a new test file:

1. Create a new test file in the `tests/` directory (e.g., `new_feature_test.cpp`)
2. Add the file to `TEST_SOURCES` in `tests/CMakeLists.txt`
3. Follow the Test-Driven Development approach:
    - Write tests for a feature before implementing it
    - Run the tests and see them fail
    - Implement the feature until the tests pass
    - Refactor the code while keeping tests passing

## Mocking External Dependencies

For testing components that depend on external systems (like the file system), we use mock objects:

- `mock_file_system_monitor.hpp` provides a mock implementation of `FileSystemMonitor`
- This allows testing without making actual system calls
- Use mock objects for network operations, file system operations, and other external dependencies

## Test Coverage

To check test coverage, you can use gcov/lcov:

1. Add coverage flags to CMake:
   ```cmake
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
   ```

2. Build and run the tests:
   ```bash
   cmake --build . && ctest
   ```

3. Generate coverage report:
   ```bash
   lcov --capture --directory . --output-file coverage.info
   lcov --remove coverage.info '/usr/*' --output-file coverage.info
   genhtml coverage.info --output-directory coverage_report
   ```

4. View the report:
   ```bash
   firefox coverage_report/index.html
   ```

## Test Categories

The tests are organized by component:

- **ThreadPoolTest**: Tests for the thread pool implementation
- **ConfigurationTest**: Tests for configuration management
- **FileSystemMonitorTest**: Tests for file system monitoring functionality
- **MockFileSystemMonitorTest**: Tests using the mock file system monitor
- **MetricsCollectorTest**: Tests for metrics collection
- **SyncManagerTest**: Tests for synchronization management

## Integration Tests

Some tests (especially in `MockFileSystemMonitorTest`) demonstrate how components work together:

- These tests verify that multiple components integrate correctly
- They help identify issues that might not be visible in unit tests

## Tips for TDD with System Calls

When working with system calls like inotify:

1. Create abstractions around system calls (like the FileSystemMonitor)
2. Create mock versions of these abstractions for testing
3. Write unit tests using the mocks
4. Add a small number of integration tests with real system calls
5. Mark integration tests that depend on system capabilities so they can be skipped in environments where they won't work

## Troubleshooting

- If tests fail due to system resources (like too many open files), you might need to adjust system limits
- For issues with inotify, check that your system supports it and has sufficient watch descriptors (`/proc/sys/fs/inotify/max_user_watches`)
- If mock tests pass but real implementation fails, it's likely due to differences between the mock and real behavior
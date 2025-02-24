# Collect all test source files
set(TEST_SOURCES
        thread_pool_test.cpp
        configuration_test.cpp
        file_system_monitor_test.cpp
        metrics_collector_test.cpp
        sync_manager_test.cpp
        mock_file_system_monitor_test.cpp
)

# Define library target for the actual code (excluding main.cpp)
set(LIB_SOURCES
        ${CMAKE_SOURCE_DIR}/src/configuration.cpp
        ${CMAKE_SOURCE_DIR}/src/file_system_monitor.cpp
        ${CMAKE_SOURCE_DIR}/src/metrics_collector.cpp
        ${CMAKE_SOURCE_DIR}/src/sync_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/thread_pool.cpp
)

# Create a library for our core functionality (to be used by tests)
add_library(file_sync_lib STATIC ${LIB_SOURCES})
target_include_directories(file_sync_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Create test executable
add_executable(file_sync_tests ${TEST_SOURCES})
target_link_libraries(file_sync_tests PRIVATE
        file_sync_lib
        GTest::gtest
        GTest::gtest_main
        pthread
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(file_sync_tests)

# Create a target for running all tests
add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS file_sync_tests
)
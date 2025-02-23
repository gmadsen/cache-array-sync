cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CacheArraySync)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the executable
add_executable(CacheArraySync src/main.cpp
        src/sync_manager.cpp
        src/file_system_monitor.cpp
        src/thread_pool.cpp
        src/thread_pool.hpp
        src/metrics_collector.cpp
        src/metrics_collector.hpp
        src/configuration.cpp
        src/configuration.hpp)
        # src/inotify_example.cpp)

# Include the configuration header
include_directories(${PROJECT_SOURCE_DIR})

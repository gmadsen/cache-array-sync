cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CacheArraySync)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing with CTest
enable_testing()

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Main application source files
set(SOURCES
        src/configuration.cpp
        src/file_system_monitor.cpp
        src/metrics_collector.cpp
        src/sync_manager.cpp
        src/thread_pool.cpp
        src/main.cpp
)

# Create the main executable
add_executable(file_sync ${SOURCES})
target_include_directories(file_sync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Add test directory
add_subdirectory(tests)



# Include the configuration header
#include_directories(${PROJECT_SOURCE_DIR})

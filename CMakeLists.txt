cmake_minimum_required(VERSION 3.20)
project(BufferManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(BUFMGR_BUILD_TESTS "Build the unit tests" ON)
option(BUFMGR_BUILD_BENCHMARKS "Build the benchmark suite" ON)

# Warnings for our own targets only (not third-party dependencies).
add_library(bufmgr_warnings INTERFACE)
target_compile_options(bufmgr_warnings INTERFACE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(buffer_manager
  src/status.cpp
  src/file.cpp
  src/buffer_manager.cpp
  src/clock_buffer_manager.cpp
  src/lru_buffer_manager.cpp
)
target_include_directories(buffer_manager PUBLIC ${PROJECT_SOURCE_DIR}/src/include)
target_link_libraries(buffer_manager PRIVATE bufmgr_warnings)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUFMGR_BUILD_TESTS)
  include(FetchContent)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # use extraction time for timestamps
  endif()
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)

  set(BUFMGR_TESTS
    lru_buffer_manager_test
    clock_buffer_manager_test
  )
  foreach(test_name IN LISTS BUFMGR_TESTS)
    add_executable(${test_name} test/${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/test/include)
    target_link_libraries(${test_name} PRIVATE buffer_manager GTest::gtest_main bufmgr_warnings)
    gtest_discover_tests(${test_name})
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
if(BUFMGR_BUILD_BENCHMARKS)
  add_executable(benchmark benchmark/benchmark.cpp)
  target_include_directories(benchmark PRIVATE ${PROJECT_SOURCE_DIR}/benchmark/include)
  target_link_libraries(benchmark PRIVATE buffer_manager bufmgr_warnings)
endif()

# ---------------------------------------------------------------------------
# Submission
# ---------------------------------------------------------------------------
set(BUFMGR_SUBMISSION_FILES
  src/include/lru_buffer_manager.h
  src/lru_buffer_manager.cpp
  src/include/clock_buffer_manager.h
  src/clock_buffer_manager.cpp
  benchmark/benchmark.cpp
)
add_custom_target(submit
  COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/submission.zip
  COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/submission.zip --format=zip ${BUFMGR_SUBMISSION_FILES}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Creating ${CMAKE_BINARY_DIR}/submission.zip"
  VERBATIM
)

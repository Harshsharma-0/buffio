cmake_minimum_required(VERSION 3.16)
project(buffio VERSION 0.1 LANGUAGES CXX)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)


# Public include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)



# Library target
add_library(buffio
  src/clock.cpp
  src/thread.cpp
  src/sockbroker.cpp
  src/scheduler.cpp
  src/fd.cpp
  src/fiber.cpp

)

target_include_directories(buffio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

enable_testing()
add_subdirectory(tests)

# Example executable
add_executable(async_example examples/async_example.cpp)
target_link_libraries(async_example PRIVATE buffio)


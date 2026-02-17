cmake_minimum_required(VERSION 3.16)
project(buffio VERSION 0.1 LANGUAGES CXX)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



# Public include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)



# Library target
add_library(buffio
  src/lfcore.cpp
  src/clock.cpp
  src/thread.cpp
  src/sockbroker.cpp
  src/fd.cpp
  src/scheduler.cpp
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

add_executable(hello_world_example examples/hello_world.cpp)
target_link_libraries(hello_world_example PRIVATE buffio)

add_executable(yield_example examples/yield_example.cpp)
target_link_libraries(yield_example PRIVATE buffio)

add_executable(connect_example examples/connect_example.cpp)
target_link_libraries(connect_example PRIVATE buffio)

add_executable(echo_server_example examples/echo_server_example.cpp)
target_link_libraries(echo_server_example PRIVATE buffio)

add_executable(async_echo_example examples/async_echo_example.cpp)
target_link_libraries(async_echo_example PRIVATE buffio)

add_executable(file_read_example examples/file_read_example.cpp)
target_link_libraries(file_read_example PRIVATE buffio)

add_executable(echo_write_example examples/echo_write_example.cpp)
target_link_libraries(echo_write_example PRIVATE buffio)


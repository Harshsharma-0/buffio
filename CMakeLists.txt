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


file(GLOB SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

# Iterate through the list of found source files
foreach(file ${SOURCE_FILES})
    get_filename_component(executable_name ${file} NAME_WE)
    add_executable(${executable_name} ${file})
    target_link_libraries(${executable_name} PRIVATE buffio)

endforeach()

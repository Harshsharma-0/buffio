cmake_minimum_required(VERSION 3.16)
project(buffio VERSION 0.1 LANGUAGES CXX)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(DEFINED BUFFIO_BUILD_TYPE)
elseif()
  message(FATAL_ERROR "NO BUILD TYPE SPECIFIED, please specify one [Debug,Release]")
endif()

set(CMAKE_BUILD_TYPE Debug)


# Public include directory
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/utility
)

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__linux__ "" HAVE_LINUX_MACRO)

if(HAVE_LINUX_MACRO)
  if(DEFINED linux_backend)
     if(linux_backend STREQUAL "epoll")
       set(BUFFIO_OS_LINUX_IO_URING 1)
      elseif(linux_backend STREQUAL "io_uring")
       set(BUFFIO_OS_LINUX_EPOLL 1)
      else()
        message(FATAL_ERROR "UNSUPPORTED backend ${linux_backend},supported backends [epoll,io_uring]")
      endif()
  else()
    message(FATAL_ERROR "specify a  backend for ${buffio_os} ,supported backends [epoll,io_uring], -Dlinux_backend")
  endif()

else()
  message(FATAL_ERROR "UNSUPPORTED PLATFORM , supported platform are [linux]")
endif()



# Library target
add_library(buffio
  src/lfcore.cpp
  src/core/instance.cpp
  src/instance.cpp
  src/file.cpp
  src/worker.cpp
)

target_include_directories(buffio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utility>
)
target_link_libraries(buffio PRIVATE pthread)
#enable_testing()
add_executable(hello ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello_world.cpp)
target_link_libraries(hello PRIVATE buffio)

#add_subdirectory(tests)


#file(GLOB SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

# Iterate through the list of found source files
#
#foreach(file ${SOURCE_FILES})
#    get_filename_component(executable_name ${file} NAME_WE)
#    add_executable("buffio_${executable_name}" ${file})
#    target_link_libraries("buffio_${executable_name}" PRIVATE buffio)
#
#endforeach()


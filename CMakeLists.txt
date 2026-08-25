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

set(BUFFIO_WORKER_QUEUE_ORDER 4 CACHE STRING
    "Worker queue order")

set(BUFFIO_MAX_WORKER 16 CACHE STRING
    "Maximum number of workers")

set(BUFFIO_FILE_VEC_MAX 16 CACHE STRING
    "Maximum number of workers")

option(BUFFIO_WORKER_DEFAULT
  "Use default worker backend" ON)

option(BUFFIO_WORKER_IO_URING
    "Use io_uring worker backend" OFF)

option(BUFFIO_WORKER_IOCP
    "Use IOCP worker backend" OFF)

# Public include directory
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/utility
)

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__linux__ "" HAVE_LINUX_MACRO)
check_cxx_symbol_exists(_MSC_VER "" HAVE_MSC_VER)
check_cxx_symbol_exists(_WIN32 "" HAVE_WIN)

if(HAVE_LINUX_MACRO)

  option(BUFFIO_OS_LINUX "platfrom is set to linux" ON)

  if(BUFFIO_WORKER_IO_URING)
    set(BUFFIO_BACKEND_NAME "worker/iouring.hpp")
    set(BUFFIO_BACKEND_SRC
      "src/backend/worker/iouring.cpp" 
      "src/backend/fs/linux.cpp"
      "src/backend/core/linux.cpp"
    )
    set(BUFFIO_LINK_LIBRARY pthread uring)
    set(BUFFIO_BACKEND_IO_URING ON)

  else()
    set(BUFFIO_BACKEND_NAME "worker/default.hpp")
    set(BUFFIO_BACKEND_SRC
      "src/backend/worker/default.cpp"
      "src/backend/fs/linux.cpp"
      "src/backend/core/linux.cpp"
    )
    set(BUFFIO_LINK_LIBRARY pthread)

  endif()

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config/config_linux.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/buffio/config.hpp
   )

 
elseif(HAVE_MSC_VER || HAVE_WIN)

  option(BUFFIO_OS_WINDOWS "platfrom is set to linux" ON)
  set(BUFFIO_LINK_LIBRARY )

  set(BUFFIO_BACKEND_NAME "worker/iocp.hpp")
  set(BUFFIO_BACKEND_SRC 
      "src/backend/worker/iocp.cpp"
      "src/backend/fs/windows.cpp"
      "src/backend/core/windows.cpp"
      )

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config/config_windows.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/buffio/config.hpp
  )

else()
  message(FATAL_ERROR "UNSUPPORTED PLATFORM , supported platform are [linux]")
endif()

set(BUFFIO_WORKER_BACKEND_INCLUDE 
  "#include \"backend/${BUFFIO_BACKEND_NAME}\"")

 configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/buffio/worker.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/buffio/worker.hpp
  )


# Library target
add_library(buffio
  src/core/task.cpp
  src/core/promise.cpp
  ${BUFFIO_BACKEND_SRC}
  src/lfcore.cpp
  src/thread.cpp
  src/fs.cpp
)


target_include_directories(buffio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utility>
)

target_link_libraries(buffio PRIVATE ${BUFFIO_LINK_LIBRARY})
#enable_testing()


add_executable(hello 
  examples/hello_world.cpp
)

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


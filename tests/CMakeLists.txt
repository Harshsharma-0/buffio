add_executable(buffio_tests
    test_queue.cpp
    test_schedular.cpp
)

target_link_libraries(buffio_tests PRIVATE buffio)


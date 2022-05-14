cmake_minimum_required(VERSION 3.10)


if (NOT BuildWithMSVC)
    link_libraries(atomic pthread)
endif ()


add_executable(mpmc_list_queue_test mpmc_list_queue_test.cpp)
if (NOT BuildWithMSVC)
    set_target_properties(mpmc_list_queue_test PROPERTIES COMPILE_FLAGS "-save-temps -fverbose-asm -Wa,-adhlmn=main.lst")
endif()

add_executable(mpmc_array_queue_test mpmc_array_queue_test.cpp)

add_executable(single_produce_test single_produce_test.cpp)

add_executable(mpsc_list_queue_test mpsc_list_queue_test.cpp)

add_executable(blocking_queue_test blocking_queue_test.cpp)

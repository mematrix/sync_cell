cmake_minimum_required(VERSION 3.10)


add_executable(link_list_queue_test link_list_queue_test.cpp)
if (NOT BuildWithMSVC)
    target_link_libraries(link_list_queue_test atomic pthread)
    set_target_properties(link_list_queue_test PROPERTIES COMPILE_FLAGS "-save-temps -fverbose-asm -Wa,-adhlmn=main.lst")
endif()

add_executable(mpmc_array_queue_test mpmc_array_queue_test.cpp)
if (NOT BuildWithMSVC)
    target_link_libraries(mpmc_array_queue_test atomic pthread)
endif ()

add_executable(single_produce_test single_produce_test.cpp)
if (NOT BuildWithMSVC)
    target_link_libraries(single_produce_test pthread)
endif ()

cmake_minimum_required(VERSION 3.10)


add_executable(link_list_queue_test link_list_queue_test.cpp)
target_link_libraries(link_list_queue_test atomic pthread)
set_target_properties(link_list_queue_test PROPERTIES COMPILE_FLAGS "-save-temps -fverbose-asm -Wa,-adhlmn=main.lst")

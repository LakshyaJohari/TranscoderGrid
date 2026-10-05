cmake_minimum_required(VERSION 3.20)
project(single_machine_demo)

add_executable(single_machine_demo src/main.cpp)
target_include_directories(single_machine_demo PRIVATE ${CMAKE_SOURCE_DIR}/common/include)
target_link_libraries(single_machine_demo PRIVATE common)

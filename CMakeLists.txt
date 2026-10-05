cmake_minimum_required(VERSION 3.26)
project(distributed_ffmpeg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_ADVANCED_MILESTONES "Enable master/worker/test/docker scaffolding" ON)

add_subdirectory(common)
add_subdirectory(samples/single_machine)

if(ENABLE_ADVANCED_MILESTONES)
	add_subdirectory(master)
	add_subdirectory(worker)
	enable_testing()
	add_subdirectory(tests)
endif()


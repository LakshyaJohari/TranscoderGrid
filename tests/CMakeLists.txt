cmake_minimum_required(VERSION 3.26)
project(distributed_ffmpeg_tests)

find_package(GTest CONFIG REQUIRED)

add_executable(test_split_strategy common/test_split_strategy.cpp)
target_link_libraries(test_split_strategy PRIVATE common GTest::gtest GTest::gtest_main)

add_executable(test_scheduler common/test_scheduler.cpp)
target_link_libraries(test_scheduler PRIVATE common GTest::gtest GTest::gtest_main)

add_executable(test_job_state_machine common/test_job_state_machine.cpp)
target_link_libraries(test_job_state_machine PRIVATE common GTest::gtest GTest::gtest_main)

add_executable(test_ffmpeg_executor common/test_ffmpeg_executor.cpp)
target_link_libraries(test_ffmpeg_executor PRIVATE common GTest::gtest GTest::gmock GTest::gtest_main)

add_executable(test_storage common/test_storage.cpp)
target_link_libraries(test_storage PRIVATE common GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_split_strategy)
gtest_discover_tests(test_scheduler)
gtest_discover_tests(test_job_state_machine)
gtest_discover_tests(test_ffmpeg_executor)
gtest_discover_tests(test_storage)


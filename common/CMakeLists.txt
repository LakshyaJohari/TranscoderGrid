cmake_minimum_required(VERSION 3.26)
project(common_lib)

add_library(common STATIC
  src/TimeBasedSplitStrategy.cpp
  src/FFmpegExecutor.cpp
  src/LeastLoadedScheduler.cpp
  src/RoundRobinScheduler.cpp
  src/LocalFSStorageClient.cpp
  src/MinIOStorageClient.cpp
)

target_include_directories(common PUBLIC include)
target_compile_definitions(common PUBLIC "_CRT_SECURE_NO_WARNINGS")


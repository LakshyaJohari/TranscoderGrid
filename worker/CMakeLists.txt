cmake_minimum_required(VERSION 3.26)
project(worker_app)

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Proto codegen
add_library(worker_proto
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/jobservice.proto
)

protobuf_generate(
  TARGET worker_proto
  LANGUAGE cpp
  OUT_VAR proto_srcs
)

protobuf_generate(
  TARGET worker_proto
  LANGUAGE grpc
  GENERATE_EXTENSIONS .grpc.pb.cc .grpc.pb.h
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
  OUT_VAR grpc_srcs
)

target_link_libraries(worker_proto PUBLIC protobuf::libprotobuf gRPC::grpc_cpp)

add_executable(worker
  src/main.cpp
  src/WorkerServiceImpl.cpp
)

target_include_directories(worker PUBLIC include)
target_link_libraries(worker PRIVATE common worker_proto CLI11::CLI11 nlohmann_json::nlohmann_json spdlog::spdlog)

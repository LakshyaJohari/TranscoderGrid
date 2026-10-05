cmake_minimum_required(VERSION 3.26)
project(master_app)

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Proto codegen
add_library(master_proto
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/jobservice.proto
)

protobuf_generate(
  TARGET master_proto
  LANGUAGE cpp
  OUT_VAR proto_srcs
)

protobuf_generate(
  TARGET master_proto
  LANGUAGE grpc
  GENERATE_EXTENSIONS .grpc.pb.cc .grpc.pb.h
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
  OUT_VAR grpc_srcs
)

target_link_libraries(master_proto PUBLIC protobuf::libprotobuf gRPC::grpc_cpp)

add_executable(master
  src/main.cpp
  src/Master.cpp
  src/WorkerProxy.cpp
  src/JobDispatcher.cpp
  src/WorkerRegistry.cpp
  src/JobSpecBuilder.cpp
)

target_include_directories(master PUBLIC include)
target_link_libraries(master PRIVATE common master_proto CLI11::CLI11 nlohmann_json::nlohmann_json spdlog::spdlog)

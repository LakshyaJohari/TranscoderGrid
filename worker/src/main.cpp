#include <iostream>
#include <memory>
#include <string>

#include <CLI/CLI.hpp>
#include <grpcpp/ext/proto_stream_reflect_service.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>

#include "common/FFmpegExecutor.h"
#include "worker/WorkerServiceImpl.h"

// Note: Proto-generated headers will be available after cmake build
// #include "jobservice.pb.h"
// #include "jobservice.grpc.pb.h"

int main(int argc, char **argv)
{
    CLI::App app{"Distributed FFmpeg Worker"};

    std::string workerId = "worker-1";
    uint16_t port = 50052;

    app.add_option("--worker-id", workerId, "Worker ID (default: worker-1)");
    app.add_option("--port", port, "gRPC listening port (default: 50052)");

    CLI11_PARSE(app, argc, argv);

    // Initialize logging
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting worker: id={}, port={}", workerId, port);

    // Create gRPC service
    auto executor = std::make_unique<FFmpegExecutor>();
    auto service = std::make_unique<WorkerServiceImpl>(workerId, std::move(executor));

    // Build and start the gRPC server
    grpc::ServerBuilder builder;
    std::string server_address = std::string("0.0.0.0:") + std::to_string(port);
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server)
    {
        spdlog::error("Failed to start gRPC server");
        return 1;
    }

    spdlog::info("Worker listening on {}", server_address);

    // Wait for shutdown signal
    server->Wait();

    return 0;
}

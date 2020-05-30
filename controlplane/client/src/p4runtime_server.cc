#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

#include "grpc_out/p4/v1/p4runtime.grpc.pb.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

using ::GRPC_NAMESPACE_ID::Server;
using ::GRPC_NAMESPACE_ID::ServerBuilder;
using ::GRPC_NAMESPACE_ID::Status;

// Server
using ::P4_NAMESPACE_ID::P4Runtime;
// Data structures
using ::P4_CONFIG_NAMESPACE_ID::P4Info;
using ::P4_NAMESPACE_ID::ReadResponse;
using ::P4_NAMESPACE_ID::WriteRequest;

class P4RuntimeServer final : public P4Runtime::Service {
  public:
    explicit P4RuntimeServer() {}

    P4Info GetP4Info() {
      std::cout << "Returning P4Info file" << std::endl;
      return P4Info();
    }

    Status SetFwdPipeConfig() {
      std::cout << "Setting forwarding pipeline config" << std::endl;
      return Status::OK;
    }

    Status Write(WriteRequest* request) {
      std::cout << "Receiving write request" << std::endl;
      return Status::OK;
    }

    Status WriteUpdate(WriteRequest* update) {
      std::cout << "Receiving write-update request" << std::endl;
      return Status::OK;
    }

    ReadResponse ReadOne() {
      std::cout << "Receiving single read request" << std::endl;
      return ReadResponse();
    }

    std::string APIVersion() {
      std::cout << "Returning API version" << std::endl;
      std::string version = "1.23-r4";
      return version;
    }
};

void run_server() {
  std::string server_address("0.0.0.0:50051");
  P4RuntimeServer service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, ::GRPC_NAMESPACE_ID::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  run_server();

  return 0;
}
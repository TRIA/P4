#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

#include "grpc_out/p4/v1/p4runtime.grpc.pb.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

// using ::GRPC_NAMESPACE_ID::ServerContext;
using ::GRPC_NAMESPACE_ID::Server;
using ::GRPC_NAMESPACE_ID::ServerBuilder;
using ::GRPC_NAMESPACE_ID::ServerContext;
using ::GRPC_NAMESPACE_ID::ServerWriter;
using ::GRPC_NAMESPACE_ID::Status;
using ::GRPC_NAMESPACE_ID::WriteOptions;

// Server
using ::P4_NAMESPACE_ID::P4Runtime;
// Data structures
using ::P4_CONFIG_NAMESPACE_ID::P4Info;
using ::P4_NAMESPACE_ID::CapabilitiesRequest;
using ::P4_NAMESPACE_ID::CapabilitiesResponse;
using ::P4_NAMESPACE_ID::GetForwardingPipelineConfigRequest;
using ::P4_NAMESPACE_ID::GetForwardingPipelineConfigResponse;
using ::P4_NAMESPACE_ID::ReadRequest;
using ::P4_NAMESPACE_ID::ReadResponse;
using ::P4_NAMESPACE_ID::WriteRequest;

class P4RuntimeServer final : public P4Runtime::Service {
  public:
    explicit P4RuntimeServer() {}

    Status GetForwardingPipelineConfig(ServerContext* context, const GetForwardingPipelineConfigRequest* request,
        GetForwardingPipelineConfigResponse* response) override {
      std::cout << "Returning P4Info file" << std::endl;
      std::string fwdPipelineCfg = "Sample pipeline cfg";
      response->mutable_config()->mutable_p4info()->ParseFromString(fwdPipelineCfg);
      return Status::OK;
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

    Status Read(ServerContext* context, const ReadRequest* request,
        ServerWriter<ReadResponse>* writer) override {
      std::cout << "Receiving single read request" << std::endl;
      ReadResponse response = ReadResponse();
      std::string readEntry = "Sample entry";
      response.ParseFromString(readEntry);
      WriteOptions options;
      writer->WriteLast(response, options);
      return Status::OK;
    }

    Status Capabilities(ServerContext* context, const CapabilitiesRequest* request, 
        CapabilitiesResponse* response) override {
      std::cout << "Returning API version" << std::endl;
      std::string version = "1.23-r4";
      response->set_p4runtime_api_version(version.c_str());
      return Status::OK;
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
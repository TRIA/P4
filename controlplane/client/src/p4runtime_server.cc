// #include <fcntl.h>
#include <fstream>
#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>
#include <sstream>
// #include <unistd.h>

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
using ::P4_NAMESPACE_ID::ForwardingPipelineConfig;
using ::P4_NAMESPACE_ID::GetForwardingPipelineConfigRequest;
using ::P4_NAMESPACE_ID::GetForwardingPipelineConfigResponse;
using ::P4_NAMESPACE_ID::ReadRequest;
using ::P4_NAMESPACE_ID::ReadResponse;
using ::P4_NAMESPACE_ID::SetForwardingPipelineConfigRequest;
using ::P4_NAMESPACE_ID::SetForwardingPipelineConfigResponse;
using ::P4_NAMESPACE_ID::WriteRequest;

class P4RuntimeServer final : public P4Runtime::Service {
  public:
    explicit P4RuntimeServer() {
      // p4InfoPath_ = "../cfg/p4info.txt";
      // binaryCfgPath_ = "../cfg/bmv2.json";
      // p4Info_ = P4Info().New();
      p4Info_ = P4Info();
      binaryCfg_ = "";
    }

    Status SetForwardingPipelineConfig(ServerContext* context, const SetForwardingPipelineConfigRequest* request, 
        SetForwardingPipelineConfigResponse* response) override {
      std::cout << "\n" << "Receiving SetForwardingPipelineConfig request" << std::endl;

      // P4Info p4InfoRequest = request->config().p4info();
      // p4Info_ = &p4InfoRequest;
      p4Info_ = request->config().p4info();
      binaryCfg_ = request->config().p4_device_config();

      // Debug info to show cfg from forwarding pipeline
      printForwardingPipelineInfo();

      return Status::OK;
    }

    Status GetForwardingPipelineConfig(ServerContext* context, const GetForwardingPipelineConfigRequest* request,
        GetForwardingPipelineConfigResponse* response) override {
      std::cout << "\n" << "Receiving GetForwardingPipelineConfig request" << std::endl;

      if (binaryCfg_.empty()) {
        std::cerr << "Device configuration is empty (maybe due to lack of previous push of configuration)" << std::endl;
        return Status::CANCELLED;
      }
      if (p4Info_.ByteSizeLong() == 0) {
        std::cerr << "P4 information is empty (maybe due to lack of previous push of configuration)" << std::endl;
        return Status::CANCELLED;
      }

      // Debug info to show cfg from forwarding pipeline
      printForwardingPipelineInfo();

      // FIXME: when commented, client receives nothing (as expected). When uncommented, segfault in server
      // response->mutable_config()->set_allocated_p4info(&p4Info_);
      response->mutable_config()->set_p4_device_config(binaryCfg_);

      return Status::OK;
    }

    Status Write(WriteRequest* request) {
      std::cout << "\n" << "Receiving Write request" << std::endl;

      return Status::OK;
    }

    Status WriteUpdate(WriteRequest* update) {
      std::cout << "\n" << "Receiving WriteUpdate request" << std::endl;

      return Status::OK;
    }

    Status Read(ServerContext* context, const ReadRequest* request,
        ServerWriter<ReadResponse>* writer) override {
      std::cout << "\n" << "Receiving Read request" << std::endl;

      ReadResponse response = ReadResponse();
      std::string readEntry = "Sample entry";
      response.ParseFromString(readEntry);
      WriteOptions options;
      writer->WriteLast(response, options);

      return Status::OK;
    }

    Status Capabilities(ServerContext* context, const CapabilitiesRequest* request, 
        CapabilitiesResponse* response) override {
      std::cout << "\n" << "Receiving Capabilities request" << std::endl;

      std::string version = "1.23-r4";
      response->set_p4runtime_api_version(version.c_str());

      return Status::OK;
    }

  private:
    // std::string p4InfoPath_;
    // std::string binaryCfgPath_;
    // P4Info* p4Info_;
    P4Info p4Info_;
    std::string binaryCfg_;

    void printP4Info() {
      std::cout << "Arch: " << p4Info_.pkg_info().arch() << std::endl;
      std::cout << "Table size: " << p4Info_.tables_size() << std::endl;
      std::string tableName;
      for (::p4::config::v1::Table table : p4Info_.tables()) {
        tableName = table.preamble().name();
        std::cout << "Table name: " << tableName << std::endl;
        std::cout << "Table id: " << table.preamble().id() << std::endl;
        for (::p4::config::v1::MatchField matchField : table.match_fields()) {
          std::cout << "\tMatch name (table=" << tableName << "): " << table.preamble().name() << std::endl;
          std::cout << "\tMatch type (table=" << tableName << "): " << table.preamble().GetTypeName() << std::endl;
        }
        for (::p4::config::v1::ActionRef actionRef : table.action_refs()) {
          std::cout << "\tAction ref. id (table=" << tableName << "): " << actionRef.id() << std::endl;
        }
      }
      std::string actionName;
      for (::p4::config::v1::Action action : p4Info_.actions()) {
        actionName = action.preamble().name();
        std::cout << "Action name: " << actionName << std::endl;
        std::cout << "Action id: " << action.preamble().id() << std::endl;

        for (p4::config::v1::Action_Param param : action.params()) {
          std::cout << "\tAction param name (action=" << actionName << "): " << param.name() << std::endl;
          std::cout << "\tAction param id (action=" << actionName << "): " << param.id() << std::endl;
          std::cout << "\tAction param bitwidth (action=" << actionName << "): " << param.bitwidth() << std::endl;
        }
      }
    }

    void printForwardingPipelineInfo() {
      std::cout << "Obtained P4 device config (first 100 char.): " << std::endl;
      std::cout << binaryCfg_.substr(0,100) << std::endl;
      std::cout << "Obtained P4 info (first 100 char.): " << std::endl;
      // std::cout << (*p4Info_).SerializeAsString().substr(0,100) << std::endl;
      std::cout << p4Info_.SerializeAsString().substr(0,100) << std::endl;
      printP4Info();
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
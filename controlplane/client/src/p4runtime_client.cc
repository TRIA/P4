#include <fcntl.h>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sstream>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "p4runtime_client.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

using ::GRPC_NAMESPACE_ID::Channel;
using ::GRPC_NAMESPACE_ID::ClientContext;
using ::GRPC_NAMESPACE_ID::Status;
using ::PROTOBUF_NAMESPACE_ID::io::IstreamInputStream;
using ::PROTOBUF_NAMESPACE_ID::io::FileInputStream;
using ::PROTOBUF_NAMESPACE_ID::io::ZeroCopyInputStream;
// Service / Stub / gRPC generated with methods
using ::P4_NAMESPACE_ID::P4Runtime;
// Channel
using ::GRPC_NAMESPACE_ID::CreateChannel;
using ::GRPC_NAMESPACE_ID::InsecureChannelCredentials;
// Data structures
using ::P4_CONFIG_NAMESPACE_ID::P4Info;
using ::P4_NAMESPACE_ID::CapabilitiesRequest;
using ::P4_NAMESPACE_ID::CapabilitiesResponse;
using ::P4_NAMESPACE_ID::Entity;
using ::P4_NAMESPACE_ID::ForwardingPipelineConfig;
using ::P4_NAMESPACE_ID::GetForwardingPipelineConfigRequest;
using ::P4_NAMESPACE_ID::GetForwardingPipelineConfigResponse;
using ::P4_NAMESPACE_ID::ReadRequest;
using ::P4_NAMESPACE_ID::ReadResponse;
using ::P4_NAMESPACE_ID::SetForwardingPipelineConfigRequest;
using ::P4_NAMESPACE_ID::SetForwardingPipelineConfigResponse;
using ::P4_NAMESPACE_ID::StreamMessageRequest;
using ::P4_NAMESPACE_ID::Update;
using ::P4_NAMESPACE_ID::WriteRequest;
using ::P4_NAMESPACE_ID::WriteResponse;

P4RuntimeClient::P4RuntimeClient(std::string bindAddress,
                std::string config,
                ::PROTOBUF_NAMESPACE_ID::uint64 deviceId,
                std::string electionId) {
  channel_ = CreateChannel(bindAddress, InsecureChannelCredentials());
  stub_ = P4Runtime::NewStub(channel_);

  auto configIndex = config.find_first_of(",");
  p4InfoPath_ = config.substr(0, configIndex);
  binaryCfgPath_ = config.substr(configIndex + 1);

  deviceId_ = deviceId;

  auto electionIdIndex = electionId.find_first_of(",");
  ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
  ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
  electionId_ = ::P4_NAMESPACE_ID::Uint128().New();
  electionId_->set_high(electionIdHigh);
  electionId_->set_low(electionIdLow);

  ClientContext context;
  std::unique_ptr<streamType_> stream_ = stub_->StreamChannel(&context);

  SetUp();
}

// RPC methods

// See reference/p4runtime-shell-python/p4runtime.py#get_p4info
P4Info P4RuntimeClient::GetP4Info() {
  std::cout << "Retrieving P4Info file" << std::endl;
  GetForwardingPipelineConfigRequest request = GetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  request.set_response_type(request.P4INFO_AND_COOKIE);

  ClientContext context;
  GetForwardingPipelineConfigResponse response;
  try {
    stub_->GetForwardingPipelineConfig(&context, request, &response);
  } catch (...) {
    const std::string errorMessage = "Cannot get the configuration from ForwardingPipelineConfig";
    HandleException(errorMessage.c_str());
  }
  return response.config().p4info();
}

// See reference/p4runtime-shell-python/p4runtime.py#set_fwd_pipe_config
Status P4RuntimeClient::SetFwdPipeConfig() {
  std::cout << "Setting forwarding pipeline config" << std::endl;
  SetForwardingPipelineConfigRequest request = SetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  SetElectionId(request.mutable_election_id());
  request.set_action(SetForwardingPipelineConfigRequest().VERIFY_AND_COMMIT);

  int fd = open(p4InfoPath_.c_str(), O_RDONLY);
  ZeroCopyInputStream* p4InfoFile = new FileInputStream(fd);
  std::ifstream binaryCfgFileIfStream(binaryCfgPath_);
  ZeroCopyInputStream* binaryCfgFile = new IstreamInputStream(&binaryCfgFileIfStream, -1);
  std::stringstream binaryCfgFileStream;
  binaryCfgFileStream << binaryCfgFileIfStream.rdbuf();
  std::string binaryCfgFileStr = binaryCfgFileStream.str();

  // Data (1st arg) merged into given Message (2nd arg)
  ::PROTOBUF_NAMESPACE_ID::TextFormat::Merge(p4InfoFile, &request);

  ForwardingPipelineConfig* config = request.mutable_config();
  config->set_allocated_p4_device_config(&binaryCfgFileStr);

  ClientContext* context;
  ::P4_NAMESPACE_ID::SetForwardingPipelineConfigResponse* response;

  return stub_->SetForwardingPipelineConfig(context, request, response);
}

// See reference/p4runtime-shell-python/p4runtime.py#write
Status P4RuntimeClient::Write(::P4_NAMESPACE_ID::WriteRequest* request) {
  std::cout << "Submitting writing request" << std::endl;
  request->set_device_id(deviceId_);
  deviceId_ = request->device_id();
  SetElectionId(request->mutable_election_id());
  WriteResponse response;
  ClientContext context;
  return stub_->Write(&context, *request, &response);
}

// See reference/p4runtime-shell-python/p4runtime.py#write_update
Status P4RuntimeClient::WriteUpdate(WriteRequest* update) {
  std::cout << "Submitting write-update request" << std::endl;
  WriteRequest request = WriteRequest();
  request.set_device_id(deviceId_);
  SetElectionId(request.mutable_election_id());
  // Extend request.updates with the new update object provided
  for (Update singleUpdate : *(update->mutable_updates())) {
    // Note: add already allocated objects in memory. Might use "Add" isntead
    request.mutable_updates()->AddAllocated(&singleUpdate);
  }
  WriteResponse response;
  ClientContext context;
  return stub_->Write(&context, request, &response);
}

// See reference/p4runtime-shell-python/p4runtime.py#read_one
ReadResponse P4RuntimeClient::ReadOne() {
  std::cout << "Submitting single read request" << std::endl;
  ReadRequest request = ReadRequest();
  request.set_device_id(deviceId_);
  for (Entity singleEntity : *request.mutable_entities()) {
    // Note: add already allocated objects in memory. Might use "Add" isntead
    request.mutable_entities()->AddAllocated(&singleEntity);
  }
  ClientContext context;
  ReadResponse response = ReadResponse();
  stub_->Read(&context, request).get()->Read(&response);
  return response;
}

// See reference/p4runtime-shell-python/p4runtime.py#api_version
std::string P4RuntimeClient::APIVersion() {
  std::cout << "Fetching version of the API" << std::endl;
  CapabilitiesRequest request = CapabilitiesRequest();
  CapabilitiesResponse response = CapabilitiesResponse();
  ClientContext context;
  try {
    stub_->Capabilities(&context, request, &response);
  } catch (...) {
    const std::string errorMessage = "Cannot retrieve information on the API version";
    HandleException(errorMessage.c_str());
  }
  return response.p4runtime_api_version();
}

// Ancillary methods

// TODO: finalise. See reference/p4runtime-shell-python/p4runtime.py#set_up_stream
void P4RuntimeClient::SetUp() {
  // Setup queues
  std::cout << "SOME 20 " << std::endl;
  std::queue<streamType_> streamQueueIn_();
  std::cout << "SOME 21 " << std::endl;
  std::queue<streamType_> streamQueueOut_();
  std::cout << "SOME 22 " << std::endl;
  grpc::ClientContext context;
  std::cout << "SOME 23 " << std::endl;
  stream_ = stub_->StreamChannel(&context);
  std::cout << "SOME 24 " << std::endl;
  Handshake();
  std::cout << "SOME 25 " << std::endl;
}

// TODO: finalise. Check reference/p4runtime-shell-python/p4runtime.py#tear_down
void P4RuntimeClient::TearDown() {
  if (!streamQueueOut_.empty()) {
    streamQueueOut_.push(NULL);
  }
  // channel_->~Channel();
  channel_.reset();
}

// TODO: finalise. Check reference/p4runtime-shell-python/p4runtime.py#handshake
void P4RuntimeClient::Handshake() {
  std::cout << "SOME 30 " << std::endl;
  StreamMessageRequest request = StreamMessageRequest();
  std::cout << "SOME 31 " << std::endl;
  const ::P4_NAMESPACE_ID::MasterArbitrationUpdate arbitration = request.arbitration();
  std::cout << "SOME 32 " << std::endl;
  SetElectionId(const_cast<::P4_NAMESPACE_ID::Uint128*>(&arbitration.election_id()));
  std::cout << "SOME 33 " << std::endl;
  std::cout << "SOME 34 " << std::endl;
  streamQueueOut_.push(&request);
  std::cout << "SOME 35 " << std::endl;
}

void P4RuntimeClient::SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId) {
  electionId_ = electionId;
  electionId_->set_high(electionId_->high());
  electionId_->set_low(electionId_->low());
}

void P4RuntimeClient::SetElectionId(std::string electionId) {
  try {
    auto electionIdIndex = electionId.find_first_of(",");
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
    electionId_->set_high(electionIdHigh);
    electionId_->set_low(electionIdLow);
  } catch (...) {
    const std::string errorMessage = "Cannot parse electionId with value = " + electionId;
    HandleException(errorMessage.c_str());
  }
}

void P4RuntimeClient::HandleException(const char* errorMessage) {
  std::cerr << errorMessage << std::endl;
  throw errorMessage;
}

#include "p4runtime_ns_undef.inc"
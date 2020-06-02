#include <fcntl.h>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sstream>
#include <unistd.h>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "p4runtime_client.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

using ::GRPC_NAMESPACE_ID::Channel;
using ::GRPC_NAMESPACE_ID::ClientContext;
using ::GRPC_NAMESPACE_ID::ClientReader;
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

// NOTE: any fix should check gRPC status codes at https://grpc.github.io/grpc/core/md_doc_statuscodes.html

// See reference/p4runtime-shell-python/p4runtime.py#set_fwd_pipe_config
Status P4RuntimeClient::SetFwdPipeConfig() {
  std::cout << "Setting forwarding pipeline config" << std::endl;
  SetForwardingPipelineConfigRequest request = SetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  SetElectionId(request.mutable_election_id());
  request.set_action(SetForwardingPipelineConfigRequest().VERIFY_AND_COMMIT);

  std::ifstream p4InfoFileIfStream("./" + p4InfoPath_);
  if (!p4InfoFileIfStream.is_open()) {
    const std::string errorMessage = "Cannot open file " + p4InfoPath_;
    HandleException(errorMessage.c_str());
  }
  ZeroCopyInputStream* p4InfoFile = new IstreamInputStream(&p4InfoFileIfStream, -1);

  std::ifstream binaryCfgFileIfStream("./" + binaryCfgPath_);
  if (!binaryCfgFileIfStream.is_open()) {
    const std::string errorMessage = "Cannot open file " + binaryCfgPath_;
    HandleException(errorMessage.c_str());
  }
  ZeroCopyInputStream* binaryCfgFile = new IstreamInputStream(&binaryCfgFileIfStream, -1);
  std::stringstream binaryCfgFileStream;
  binaryCfgFileStream << binaryCfgFileIfStream.rdbuf();
  std::string binaryCfgFileStr = binaryCfgFileStream.str();

  ForwardingPipelineConfig* config = ForwardingPipelineConfig().New();
  config->set_p4_device_config(binaryCfgFileStr);
  // P4Info p4Info = P4Info();
  // p4Info.ParseFromBoundedZeroCopyStream(p4InfoFile, p4InfoFile->ByteCount());
  P4Info p4Info = request.config().p4info();
  ::PROTOBUF_NAMESPACE_ID::TextFormat::Merge(p4InfoFile, &p4Info);
  // The current information (already allocated in memory) is shared with the config object
  config->set_allocated_p4info(&p4Info);
  request.set_allocated_config(config);

  ClientContext context;
  SetForwardingPipelineConfigResponse response = SetForwardingPipelineConfigResponse();
  Status status = stub_->SetForwardingPipelineConfig(&context, request, &response);

  // Free the memory right after the request to the server (otherwise expect segfault)
  config->release_p4info();
  config->release_p4_device_config();

  return status;
}

// See reference/p4runtime-shell-python/p4runtime.py#get_p4info
// TODO
P4Info P4RuntimeClient::GetP4Info() {
  std::cout << "\n" << "Retrieving P4Info file" << std::endl;
  GetForwardingPipelineConfigRequest request = GetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  request.set_response_type(request.P4INFO_AND_COOKIE);
  GetForwardingPipelineConfigResponse response = GetForwardingPipelineConfigResponse();
  ClientContext context;
  const std::string errorMessage = "Cannot get configuration from the forwarding pipeline";
  Status status = stub_->GetForwardingPipelineConfig(&context, request, &response);
  HandleStatus(status, errorMessage.c_str());
  return response.config().p4info();
}

// See reference/p4runtime-shell-python/p4runtime.py#write
// TODO
Status P4RuntimeClient::Write(WriteRequest* request) {
  std::cout << "\n" << "Submitting write request" << std::endl;
  request->set_device_id(deviceId_);
  deviceId_ = request->device_id();
  SetElectionId(request->mutable_election_id());
  WriteResponse response;
  ClientContext context;
  return stub_->Write(&context, *request, &response);
}

// See reference/p4runtime-shell-python/p4runtime.py#write_update
// TODO
Status P4RuntimeClient::WriteUpdate(WriteRequest* update) {
  std::cout << "\n" << "Submitting write-update request" << std::endl;
  WriteRequest request = WriteRequest();
  request.set_device_id(deviceId_);
  SetElectionId(request.mutable_election_id());
  // Extend request.updates with the new update object provided
  for (Update singleUpdate : *(update->mutable_updates())) {
    // Note: add already allocated objects in memory. Might use "Add" instead
    request.mutable_updates()->AddAllocated(&singleUpdate);
  }
  WriteResponse response;
  ClientContext context;
  return stub_->Write(&context, request, &response);
}

// See reference/p4runtime-shell-python/p4runtime.py#read_one
// TODO
ReadResponse P4RuntimeClient::ReadOne() {
  std::cout << "\n" << "Submitting single read request" << std::endl;
  ReadRequest request = ReadRequest();
  request.set_device_id(deviceId_);
  for (Entity singleEntity : *request.mutable_entities()) {
    // Note: add already allocated objects in memory. Might use "Add" instead
    request.mutable_entities()->AddAllocated(&singleEntity);
  }
  ClientContext context;
  ReadResponse response = ReadResponse();
  std::unique_ptr<ClientReader<ReadResponse> > clientReader = stub_->Read(&context, request);
  clientReader->Read(&response);
  clientReader->Finish();
  return response;
}

// See reference/p4runtime-shell-python/p4runtime.py#api_version
std::string P4RuntimeClient::APIVersion() {
  std::cout << "\n" << "Fetching version of the API" << std::endl;

  CapabilitiesRequest request = CapabilitiesRequest();
  CapabilitiesResponse response = CapabilitiesResponse();
  ClientContext context;

  Status status = stub_->Capabilities(&context, request, &response);
  const std::string errorMessage = "Cannot retrieve information on the API version";
  HandleStatus(status, errorMessage.c_str());

  return response.p4runtime_api_version();
}

// Ancillary methods

// TODO: finalise. See reference/p4runtime-shell-python/p4runtime.py#set_up_stream
void P4RuntimeClient::SetUp() {
  // Setup queues
  std::queue<streamType_> streamQueueIn_();
  std::queue<streamType_> streamQueueOut_();
  grpc::ClientContext context;
  stream_ = stub_->StreamChannel(&context);
  Handshake();
}

// TODO: finalise. Check reference/p4runtime-shell-python/p4runtime.py#tear_down
void P4RuntimeClient::TearDown() {
  if (!streamQueueOut_.empty()) {
    streamQueueOut_.push(NULL);
  }
  channel_.reset();
  // channel_->~Channel();
}

// TODO: finalise. Check reference/p4runtime-shell-python/p4runtime.py#handshake
void P4RuntimeClient::Handshake() {
  StreamMessageRequest request = StreamMessageRequest();
  const ::P4_NAMESPACE_ID::MasterArbitrationUpdate arbitration = request.arbitration();
  SetElectionId(const_cast<::P4_NAMESPACE_ID::Uint128*>(&arbitration.election_id()));
  streamQueueOut_.push(&request);
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
  std::cerr << "Exception: " << errorMessage << std::endl;
  throw errorMessage;
}

void P4RuntimeClient::HandleStatus(Status status, const char* errorMessage) {
  if (!status.ok()) {
    std::cout << errorMessage << ". Error code: " << status.error_code() << std::endl;
  }
}

#include "p4runtime_ns_undef.inc"
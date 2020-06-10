#include <ctime>
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
using ::P4_NAMESPACE_ID::MasterArbitrationUpdate;
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

  // auto electionIdIndex = electionId.find_first_of(",");
  // ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
  // ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
  // electionId_ = ::P4_NAMESPACE_ID::Uint128().New();
  // electionId_->set_high(electionIdHigh);
  // electionId_->set_low(electionIdLow);
  // INFO: it is not the same?
  SetElectionId(electionId);
  // std::cout << "0. electionId = " << electionId << std::endl;
  // std::cout << "0. electionId.high = " << electionIdHigh << std::endl;
  // std::cout << "0. electionId.low = " << electionIdLow << std::endl;

  ClientContext context;
  std::unique_ptr<streamType_> stream_ = stub_->StreamChannel(&context);

  SetUpStream();
}

// RPC methods

// NOTE: any fix should check gRPC status codes at https://grpc.github.io/grpc/core/md_doc_statuscodes.html

// See reference/p4runtime-shell-python/p4runtime.py#set_fwd_pipe_config
// TODO: validate
Status P4RuntimeClient::SetFwdPipeConfig() {
  std::cout << "Setting forwarding pipeline config" << std::endl;
  SetForwardingPipelineConfigRequest request = SetForwardingPipelineConfigRequest();
  request.set_action(request.VERIFY_AND_COMMIT);
  request.set_device_id(deviceId_);
  std::cout << "election_id (content - high) = " << electionId_->high() << std::endl;
  std::cout << "election_id (content - low) = " << electionId_->low() << std::endl;
  // NOTE: this is probably feeding something that is not proper
  // This provides "Warning: obtained error=3" (INVALID_ARGUMENT)
  // SetElectionId(request.mutable_election_id());
  // TEST - REMOVE later
  // This should be already in the request (when queuing system is properly working?). No need to add manually
  request.set_allocated_election_id(electionId_);
  // std::cout << "request.mutable_election_id (address) = " << request.mutable_election_id() << std::endl;
  // std::cout << "request.mutable_election_id (address) - high = " << request.mutable_election_id()->high() << std::endl;
  // std::cout << "request.mutable_election_id (address) - low = " << request.mutable_election_id()->low() << std::endl;
  // // std::cout << "request.mutable_election_id (content - high) = " << (*request.mutable_election_id()).high() << std::endl;
  // // std::cout << "request.mutable_election_id (content - low) = " << (*request.mutable_election_id()).low() << std::endl;

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

  // DEBUG. NOTE: python version has only device_id, election_id, action
  // std::cout << request.SerializeAsString() << std::endl;
  // std::cout << request.device_id() << std::endl;
  // std::cout << request.election_id().high() << ", " << request.election_id().low() << std::endl;
  // std::cout << request.action() << std::endl;

  ClientContext context;
  SetForwardingPipelineConfigResponse response = SetForwardingPipelineConfigResponse();
  Status status = stub_->SetForwardingPipelineConfig(&context, request, &response);

  // Free the memory right after the request to the server (otherwise expect segfault)
  config->release_p4info();
  config->release_p4_device_config();
  request.release_election_id();

  return status;
}

// See reference/p4runtime-shell-python/p4runtime.py#get_p4info
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

// TODO: validate
Status P4RuntimeClient::WriteInternal(WriteRequest* request) {
  request->set_device_id(deviceId_);
  deviceId_ = request->device_id();
  // TODO: uncomment when queueing system, threadings, etc are implemented
  // SetElectionId(request->mutable_election_id());

  WriteResponse response = WriteResponse();
  ClientContext context;
  const std::string errorMessage = "Cannot issue the Write command";
  Status status = stub_->Write(&context, *request, &response);
  HandleStatus(status, errorMessage.c_str());
  return status;
}

// See reference/p4runtime-shell-python/p4runtime.py#write
Status P4RuntimeClient::Write(WriteRequest* request) {
  std::cout << "\n" << "Submitting write request" << std::endl;
  return WriteInternal(request);
}

// See reference/p4runtime-shell-python/p4runtime.py#write_update
// TODO: validate
Status P4RuntimeClient::WriteUpdate(WriteRequest* update) {
  std::cout << "\n" << "Submitting write-update request" << std::endl;
  WriteRequest* request = WriteRequest().New();
  // Extend request.updates with the new update object provided
  for (Update singleUpdate : *(update->mutable_updates())) {
    // Note: add already allocated objects in memory. Might use "Add" instead
    request->mutable_updates()->AddAllocated(&singleUpdate);
  }
  return WriteInternal(request);
}

// See reference/p4runtime-shell-python/p4runtime.py#read_one
// TODO: validate
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

// TODO: complete. See reference/p4runtime-shell-python/p4runtime.py#set_up_stream
void P4RuntimeClient::SetUpStream() {
  // Setup queues
  std::queue<streamType_> streamQueueIn_ = std::queue<streamType_>();
  std::queue<streamType_> streamQueueOut_();
  grpc::ClientContext context;
  stream_ = stub_->StreamChannel(&context);
  Handshake();
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#tear_down
void P4RuntimeClient::TearDown() {
  if (!streamQueueOut_.empty()) {
    std::cout << "Cleaning up stream" << std::endl;
    streamQueueOut_.push(NULL);
    // NOTE: translate this command (and create new object)
    // self.stream_recv_thread.join()
  }
  channel_.reset();
  // NOTE: translate this command
  //channel_.close();
}

// TODO: check if this works. Check reference/p4runtime-shell-python/p4runtime.py#get_stream_packet
// NOTE: FIXME - if this takes as much as "timeout" it is because some condition is not met
// (queueIn is still empty!) - this has to be set inside SetUpStream()
StreamMessageRequest* P4RuntimeClient::GetStreamPacket(std::string type_, long timeout=1) {
    StreamMessageRequest* request;
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    // DEBUG
    std::cout << "QUEUE EMPTY? = " << streamQueueIn_.empty() << std::endl;
    std::cout << "QUEUE SIZE = " << streamQueueIn_.size() << std::endl;
    while (true) {
      std::chrono::duration<long> timeoutDuration(timeout);
      auto remaining = timeoutDuration - (std::chrono::system_clock::now() - start);

      if (remaining < std::chrono::seconds{0}) {
        std::cout << "RETURNING WHEN TIMEOUT EXPIRES" << std::endl;
        break;
      }
      if (!streamQueueIn_.empty()) {
        request = streamQueueIn_.front();
        std::cout << "QUEUE NOT EMPTY. RETRIEVING ARBITRATION = " << std::endl;
        std::cout << "ARBITRATION . DEVICE ID = " << request->mutable_arbitration()->device_id() << std::endl;
        std::cout << "ARBITRATION . ELECTION ID - HIGH = " << request->mutable_arbitration()->election_id().high() << std::endl;
        std::cout << "ARBITRATION . ELECTION ID - LOW = " << request->mutable_arbitration()->election_id().low() << std::endl;
        std::cout << "ARBITRATION . (ROLE ID?) = " << request->mutable_arbitration()->role().id() << std::endl;
        streamQueueIn_.pop();
        // streamQueueIn_.pop(&request);
      }
      if (request == NULL) {
        std::cout << "REQUEST IS NULL. RETURNING" << std::endl;
        return NULL;
      }
      // NOTE: translate this command
      // if request.HasField(type_):
      if (request->IsInitialized() && !request->has_arbitration()) {
        std::cout << "REQUEST HAS NO ARBITRATION: " << request->has_arbitration() << std::endl;
        continue;
      }
    }
    return request;
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#handshake
void P4RuntimeClient::Handshake() {

  // NOTE: this block should be removed & implemented correctly (not manually) inside P4RuntimeClient::SetUpStream()
  StreamMessageRequest* requestIn = StreamMessageRequest().New();
  const MasterArbitrationUpdate* arbitrationTmpIn = &requestIn->arbitration();
  MasterArbitrationUpdate arbitrationIn = *arbitrationTmpIn;
  arbitrationIn.set_device_id(deviceId_);
  requestIn->set_allocated_arbitration(&arbitrationIn);
  streamQueueIn_.push(requestIn);
  std::cout << "SIZE OF QUEUE_IN 1 = " << streamQueueIn_.size() << std::endl;
  // streamQueueIn_.push(requestIn);
  // std::cout << "SIZE OF QUEUE_IN 2 = " << streamQueueIn_.size() << std::endl;
  // NOTE: end of block

  // StreamMessageRequest* request = &StreamMessageRequest();
  StreamMessageRequest* request = StreamMessageRequest().New();
  // const StreamMessageRequest constRequest;

  // const MasterArbitrationUpdate arbitrationTmp = request.arbitration();
  // MasterArbitrationUpdate arbitration = arbitrationTmp;
  // arbitration.set_device_id(deviceId_);

  const MasterArbitrationUpdate* arbitrationTmp = &request->arbitration();
  MasterArbitrationUpdate arbitration = *arbitrationTmp;
  arbitration.set_device_id(deviceId_);
  // NOTE: commented to avoid setting electionId_ = (0, 0). May not be properly configured
  // The arbitration object is not properly filled in here
  // // SetElectionId(const_cast<::P4_NAMESPACE_ID::Uint128*>(&arbitration.election_id()));
  // SetElectionId(arbitration.mutable_election_id());
  // arbitration.set_allocated_election_id(electionId_);

  // request.set_allocated_arbitration(&arbitration);
  request->set_allocated_arbitration(&arbitration);

  // constRequest = &request;

  // streamQueueOut_.push(&request);
  streamQueueOut_.push(request);

  // NOTE: (FIXME?) this introduces delays as it waits for as much time as defined in the timeout
  StreamMessageRequest* reply = GetStreamPacket("arbitration", 2);
  // if (reply == NULL) {
  //   std::cout << "Failed to establish session with server" << std::endl;
  //   exit(1);
  // }

  // ::P4_NAMESPACE_ID::Uint128 electionIdTmp = reply->arbitration().election_id();
  // SetElectionId(&electionIdTmp);

  // bool isMaster = reply->arbitration().status().code() == ::GRPC_NAMESPACE_ID::StatusCode::OK;
  // std::cout << "Session established, client is master (true) or slave (false)? " << isMaster << std::endl;
  // if (!isMaster) {
  //     std::cout << "You are not master, you only have read access to the server" << std::endl;
  // }

  // FIXME: set to avoid Segfault. But it does not makes sense to release something that is needed
  // request.release_arbitration();
}

void P4RuntimeClient::SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId) {
  std::cout << "1 . electionId = " << electionId << std::endl;
  electionId_ = electionId;
  electionId_->set_high(electionId->high());
  electionId_->set_low(electionId->low());
  // electionId_->set_high((*electionId).high());
  // electionId_->set_low((*electionId).low());
  std::cout << "1 . electionId_.high = " << electionId_->high() << std::endl;
  std::cout << "1 . electionId_.low = " << electionId_->low() << std::endl;
}

void P4RuntimeClient::SetElectionId(std::string electionId) {
  try {
    std::cout << "2 . electionId = " << electionId << std::endl;
    auto electionIdIndex = electionId.find_first_of(",");
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
    std::cout << "2 . electionId.high = " << electionIdHigh << std::endl;
    std::cout << "2 . electionId.low = " << electionIdLow << std::endl;
    electionId_ = ::P4_NAMESPACE_ID::Uint128().New();
    electionId_->set_high(electionIdHigh);
    electionId_->set_low(electionIdLow);
    std::cout << "2 . electionId_.high = " << electionId_->high() << std::endl;
    std::cout << "2 . electionId_.low = " << electionId_->low() << std::endl;
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
    std::cerr << errorMessage << ". Error code: " << status.error_code() << std::endl;
  }
}

#include "p4runtime_ns_undef.inc"
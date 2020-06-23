#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sstream>
#include <unistd.h>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "client.h"
// Import declarations after any other
#include "../common/ns_def.inc"

using ::GRPC_NAMESPACE_ID::Channel;
using ::GRPC_NAMESPACE_ID::ClientContext;
using ::GRPC_NAMESPACE_ID::ClientReader;
using ::GRPC_NAMESPACE_ID::CompletionQueue;
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
using ::P4_NAMESPACE_ID::StreamMessageResponse;
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
  std::cout << "SetFwdPipeConfig. election_id (content - low) = " << electionId_->low() << std::endl;
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
  // // request->set_allocated_election_id(electionId_);

  WriteResponse response = WriteResponse();
  ClientContext context;
  const std::string errorMessage = "Cannot issue the Write command";
  std::cout << "\n" << "WriteInternal. Trace 1" << std::endl;
  Status status = stub_->Write(&context, *request, &response);
  std::cout << "\n" << "WriteInternal. Trace 2" << std::endl;
  HandleStatus(status, errorMessage.c_str());
  std::cout << "\n" << "WriteInternal. Trace 3" << std::endl;
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

  // std::cout << "B3-0 > content: " << response.p4runtime_api_version() << std::endl;
  // CompletionQueue cq;
//   std::cout << "B3-1" << std::endl;
//   std::unique_ptr<grpc::ClientAsyncResponseReader<CapabilitiesResponse> > rpc(
//     stub_->AsyncCapabilities(&context, request, &cq));
//   std::cout << "B3-1a" << std::endl;
//   rpc->StartCall();
//   std::cout << "B3-2" << std::endl;
//   Status status;
//   rpc->Finish(&response, &status, (void*)1);
//   std::cout << "B3-3" << std::endl;
//   void* got_tag;
//   bool ok = false;
//   // cq.Next(&got_tag, &ok);
//   GPR_ASSERT(cq.Next(&got_tag, &ok));
//   // Verify that the result from "cq" corresponds, by its tag, our previous request.
//   GPR_ASSERT(got_tag == (void*)1);
//   // ... and that the request was completed successfully. Note that "ok"
//   // corresponds solely to the request for updates introduced by Finish().
//   GPR_ASSERT(ok);
//   std::cout << "B3-4" << std::endl;
//   if (ok && got_tag == (void*)1) {
//     // check response and status
//     std::cout << "B3-5" << std::endl;
//     std::cout << "B3-5 > content: " << response.p4runtime_api_version() << std::endl;
//     return response.p4runtime_api_version();
//   }
//   std::cout << "B3-6" << std::endl;
//   return "no-version";
}

// Ancillary methods

// TODO: complete. See reference/p4runtime-shell-python/p4runtime.py#set_up_stream
void P4RuntimeClient::SetUpStream() {
  ClientContext context;
  // Setup queues
  std::deque<streamType_> streamQueueIn_();
  std::deque<streamType_> streamQueueOut_();
  // grpc::ClientContext context;
  std::cout << "SetUpStream. Setting up stream from channel" << std::endl;
  stream_ = stub_->StreamChannel(&context);

  // Create an async stream
  // GPR_ASSERT(streamQueue_.GOT_EVENT == 1);
  // streamAsync_ = stub_->PrepareAsyncStreamChannel(&context, &streamQueue_);
  // // void* tag;
  // // streamAsync_ = stub_->AsyncStreamChannel(&context, &streamQueue_, &tag);

  // Order matters here
  CheckForNewIncomingMessagesInStreamOnBackground();
  CheckForNewOutgoingMessagesInQueueOnBackground();
  Handshake();
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#tear_down
void P4RuntimeClient::TearDown() {
  if (!streamQueueOut_.empty()) {
    std::cout << "TearDown. Cleaning up stream" << std::endl;
    streamQueueOut_.push_back(NULL);
    streamOutgoingThread_.join();
    streamIncomingThread_.join();
  }
  stream_->Finish();
  channel_.reset();
  // NOTE: translate this command
  //channel_.close();
}

// // TODO: complete and check if this works. Check reference/p4runtime-shell-python/p4runtime.py#get_stream_packet
// // NOTE: FIXME - if this takes as much as "timeout" it is because some condition is not met
// // (queueIn is still empty!) - this has to be set inside SetUpStream()
// StreamMessageResponse* P4RuntimeClient::GetStreamPacket(std::string type_, long timeout=1) {
//     StreamMessageResponse* request;
//     std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
//     std::cout << "GetStreamPacket. Size for queueOUT = " << streamQueueOut_.size() << std::endl;
//     std::cout << "GetStreamPacket. Size for queueIN = " << streamQueueIn_.size() << std::endl;
//     while (true) {
//       std::chrono::duration<long> timeoutDuration(timeout);
//       auto remaining = timeoutDuration - (std::chrono::system_clock::now() - start);

//       if (remaining < std::chrono::seconds{0}) {
//         std::cout << "GetStreamPacket. Returning when timeout expires" << std::endl;
//         break;
//       }

//       request = streamQueueIn_.front();
//       if (request == NULL) {
//         std::cout << "GetStreamPacket. Returning due to NULL request" << std::endl;
//       } else if (request->IsInitialized() && !request->has_arbitration()) {
//         std::cout << "GetStreamPacket. Request has no arbitration" << std::endl;
//         continue;
//       }

//       if (!streamQueueIn_.empty()) {
//         std::cout << "GetStreamPacket. Arbitration device ID = " << request->mutable_arbitration()->device_id() << std::endl;
//         std::cout << "GetStreamPacket. Arbitration election ID (high) = " << request->mutable_arbitration()->election_id().high() << std::endl;
//         std::cout << "GetStreamPacket. Arbitration election ID (low) = " << request->mutable_arbitration()->election_id().low() << std::endl;
//         std::cout << "GetStreamPacket. Arbitration role ID = " << request->mutable_arbitration()->role().id() << std::endl;
//         streamQueueIn_.pop_front();
//         std::cout << "GetStreamPacket. Size for queueIN = " << streamQueueIn_.size() << std::endl;
//       }
//     }
//     return request;
// }

// TODO: complete and check if this works. Check reference/p4runtime-shell-python/p4runtime.py#get_stream_packet
// NOTE: FIXME - if this takes as much as "timeout" it is because some condition is not met
// (queueIn is still empty!) - this has to be set inside SetUpStream()
StreamMessageResponse* P4RuntimeClient::GetStreamPacket(std::string type_, long timeout=1) {
    StreamMessageResponse* request;
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    std::cout << "GetStreamPacket. Size for queueOUT = " << streamQueueOut_.size() << std::endl;
    std::cout << "GetStreamPacket. Size for queueIN = " << streamQueueIn_.size() << std::endl;
    while (true) {
      std::chrono::duration<long> timeoutDuration(timeout);
      auto remaining = timeoutDuration - (std::chrono::system_clock::now() - start);

      if (remaining < std::chrono::seconds{0}) {
        std::cout << "GetStreamPacket. Returning when timeout expires" << std::endl;
        break;
      }

      if (!streamQueueIn_.empty()) {
        request = streamQueueIn_.front();
        std::cout << "GetStreamPacket. Arbitration device ID = " << request->mutable_arbitration()->device_id() << std::endl;
        std::cout << "GetStreamPacket. Arbitration election ID (high) = " << request->mutable_arbitration()->election_id().high() << std::endl;
        std::cout << "GetStreamPacket. Arbitration election ID (low) = " << request->mutable_arbitration()->election_id().low() << std::endl;
        std::cout << "GetStreamPacket. Arbitration role ID = " << request->mutable_arbitration()->role().id() << std::endl;
        streamQueueIn_.pop_front();
        std::cout << "GetStreamPacket. Size for queueIN = " << streamQueueIn_.size() << std::endl;
      }

      if (request == NULL) {
        std::cout << "GetStreamPacket. Returning due to NULL request" << std::endl;
        break;
      } else if (request->IsInitialized() && !request->has_arbitration()) {
        std::cout << "GetStreamPacket. Request has no arbitration" << std::endl;
        continue;
      }
    }
    return request;
}

void P4RuntimeClient::CheckForNewIncomingMessagesInStream() {
    // // DEBUG
    // StreamMessageRequest *requestOut = StreamMessageRequest().New();
    // StreamMessageResponse* requestIn = StreamMessageResponse().New();
    // const MasterArbitrationUpdate* arbitrationTmp = &requestIn->arbitration();
    // MasterArbitrationUpdate arbitration = *arbitrationTmp;
    // arbitration.set_device_id(deviceId_);
    // requestIn->set_allocated_arbitration(&arbitration);
    // requestOut->set_allocated_arbitration(&arbitration);
    // std::cout << "CheckForNewMessagesInStream. Debug: write to queueIN\n" << std::endl;
    // streamQueueIn_.push_back(requestIn);
    // std::cout << "CheckForNewMessagesInStream. Debug: write to stream\n" << std::endl;
    // const StreamMessageRequest *requestOutConst = requestOut;
    // stream_->Write(*requestOutConst);
    // // END DEBUG
    std::cout << "CheckForNewMessagesInStream\n" << std::endl;
    ::P4_NAMESPACE_ID::StreamMessageResponse* messageResponse;
    while (stream_->Read(messageResponse)) {
      std::cout << "CheckForNewMessagesInStream. Reading streams from message\n" << std::endl;
      streamQueueIn_.push_back(messageResponse);
      std::cout << "CheckForNewMessagesInStream. Pushing message response to queueIN\n" << std::endl;
      std::cout << "CheckForNewMessagesInStream. Size for queueOUT = " << streamQueueOut_.size() << std::endl;
      std::cout << "CheckForNewMessagesInStream. Size for queueIN = " << streamQueueIn_.size() << std::endl;
    }
}

void P4RuntimeClient::CheckForNewIncomingMessagesInStreamOnBackground() {
  try {
    streamIncomingThread_ = std::thread(&P4RuntimeClient::CheckForNewIncomingMessagesInStream, this);
  } catch (...) {
    std::cerr << "CheckForNewIncomingMessagesInStreamOnBackground. StreamChannel error. Closing stream" << std::endl;
    streamQueueIn_.push_back(NULL);
  }
}

void P4RuntimeClient::CheckForNewOutgoingMessagesInQueue() {
  StreamMessageRequest* request;
  std::cout << "CheckForNewOutgoingMessagesInQueue" << std::endl;
  while (true) {
    if (!streamQueueOut_.empty()) {
      request = streamQueueOut_.front();
      std::cout << "CheckForNewOutgoingMessagesInQueue. Device ID of obtained request = " << request->mutable_arbitration()->device_id() << std::endl;
      streamQueueOut_.pop_front();
      std::cout << "CheckForNewOutgoingMessagesInQueue. Size for queueOUT = " << streamQueueOut_.size() << std::endl;
      std::cout << "CheckForNewOutgoingMessagesInQueue. Size for queueIN = " << streamQueueIn_.size() << std::endl;
    }

    if (request == NULL) {
      std::cout << "CheckForNewOutgoingMessagesInQueue. Returning due to NULL request" << std::endl;
      break;
    } else {
      const StreamMessageRequest* requestConst = request;
      std::cout << "CheckForNewOutgoingMessagesInQueue. 2" << std::endl;
      WriteRequest writeRequest = WriteRequest();
      std::cout << "CheckForNewOutgoingMessagesInQueue. Sending Write request to server" << std::endl;
      writeRequest.CopyFrom(*requestConst);
      std::cout << "CheckForNewOutgoingMessagesInQueue. 3" << std::endl;
      Status status = Write(&writeRequest);
      std::cout << "CheckForNewOutgoingMessagesInQueue. Status of sent Write request" << status.error_code() << std::endl;
    }
  }
}

void P4RuntimeClient::CheckForNewOutgoingMessagesInQueueOnBackground() {
  try {
    streamOutgoingThread_ = std::thread(&P4RuntimeClient::CheckForNewOutgoingMessagesInQueue, this);
  } catch (...) {
    std::cerr << "CheckForNewOutgoingMessagesInQueueOnBackground. StreamChannel error. Closing stream" << std::endl;
    streamQueueOut_.push_back(NULL);
  }
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#handshake
void P4RuntimeClient::Handshake() {

  StreamMessageRequest* requestOut = StreamMessageRequest().New();
  const MasterArbitrationUpdate* arbitrationTmp = &requestOut->arbitration();
  MasterArbitrationUpdate arbitration = *arbitrationTmp;
  arbitration.set_device_id(deviceId_);
  requestOut->set_allocated_arbitration(&arbitration);
  streamQueueOut_.push_back(requestOut);
  std::cout << "Handshake. Pushing message response to queueOUT" << std::endl;

  // DEBUG . COMMENT THIS WHEN CONNECTIVITY IS SOLVED
  // StreamMessageResponse* requestIn = StreamMessageResponse().New();
  // requestIn->set_allocated_arbitration(&arbitration);
  // // Simulate some reply from the server: comment and remove in the future
  // // std::cout << "Handshake. Debug: write to queueIN\n" << std::endl;
  // // streamQueueIn_.push_back(requestIn);
  // std::cout << "Handshake. Debug: write to stream\n" << std::endl;
  // const StreamMessageRequest *requestOutConst = requestOut;
  // stream_->Write(*requestOutConst);
  // END DEBUG

  // NOTE: (FIXME?) this introduces delays as it waits for as much time as defined in the timeout
  StreamMessageResponse* reply = GetStreamPacket("arbitration", 2);

  if (reply == NULL) {
    std::cerr << "Handshake. Failed to establish session with server" << std::endl;
    exit(1);
  }
  std::cout << "Handshake. Get arbitration stream. Status code = " << reply->arbitration().status().code() << std::endl;

  bool isMaster = reply->arbitration().status().code() == ::GRPC_NAMESPACE_ID::StatusCode::OK;
  std::string role = isMaster ? "master" : "slave";
  std::cout << "Handshake. Session established. Role for controller = " << role << std::endl;
  if (!isMaster) {
      std::cout << "Handshake. You only have read access to the server" << std::endl;
  }
}

void P4RuntimeClient::SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId) {
  std::cout << "SetElectionId. electionId = " << electionId << std::endl;
  electionId_ = electionId;
  electionId_->set_high(electionId->high());
  electionId_->set_low(electionId->low());
  std::cout << "SetElectionId. electionId_.high = " << electionId_->high() << std::endl;
  std::cout << "SetElectionId. electionId_.low = " << electionId_->low() << std::endl;
}

void P4RuntimeClient::SetElectionId(std::string electionId) {
  try {
    std::cout << "SetElectionId. electionId_ (str) = " << electionId << std::endl;
    auto electionIdIndex = electionId.find_first_of(",");
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
    ::P4_NAMESPACE_ID::Uint128* electionId = ::P4_NAMESPACE_ID::Uint128().New();
    electionId->set_high(electionIdHigh);
    electionId->set_low(electionIdLow);
    SetElectionId(electionId);
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

#include "../common/ns_undef.inc"

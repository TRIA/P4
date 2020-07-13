// #include <chrono>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sstream>
#include <unistd.h>

// TEST
#include "absl/numeric/int128.h"

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
// iata structures
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
  SetElectionId(electionId);

  SetUpStream();
}

// RPC methods

// NOTE: any fix should check gRPC status codes at https://grpc.github.io/grpc/core/md_doc_statuscodes.html

// See reference/p4runtime-shell-python/p4runtime.py#set_fwd_pipe_config
// TODO: validate
Status P4RuntimeClient::SetFwdPipeConfig() {
  std::cout << "Setting forwarding pipeline config" << std::endl;
  SetForwardingPipelineConfigRequest request = SetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  request.set_allocated_election_id(electionId_);
  request.set_action(request.VERIFY_AND_COMMIT);

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
  P4Info p4Info = response.config().p4info();
  PrintP4Info(p4Info);
  return p4Info;
}

Status P4RuntimeClient::Write(std::list<P4TableEntry*> entries, bool update) {
  ::P4_NAMESPACE_ID::WriteRequest request = ::P4_NAMESPACE_ID::WriteRequest();
  std::list<P4TableEntry*>::iterator it;
  std::list<P4Parameter>::iterator param_it;
  P4TableEntry * entry;
  int iteration=0;

  if (update) {
    request.add_updates()->set_type(::P4_NAMESPACE_ID::Update::MODIFY);
  } else {
    request.add_updates()->set_type(::P4_NAMESPACE_ID::Update::INSERT);
  }

  for (it=entries.begin(); it!=entries.end(); ++it) {
    entry = *it;

    ::P4_NAMESPACE_ID::Entity * p4Entity = new ::P4_NAMESPACE_ID::Entity();
    p4Entity->mutable_table_entry()->set_table_id(entry->table_id);
    ::P4_NAMESPACE_ID::TableAction * p4EntityTableAction = new ::P4_NAMESPACE_ID::TableAction();
    ::P4_NAMESPACE_ID::Action * p4EntityAction = new ::P4_NAMESPACE_ID::Action();
    p4EntityAction->set_action_id(entry->action.action_id);
    
    for (param_it=entry->action.parameters.begin(); param_it!=entry->action.parameters.end(); ++param_it) {
      p4EntityAction->add_params()->set_param_id(param_it->id);
      p4EntityAction->mutable_params(param_it->id - 1)->set_value(param_it->value);
    }

    p4EntityTableAction->set_allocated_action(p4EntityAction);
    p4Entity->mutable_table_entry()->set_allocated_action(p4EntityTableAction);
    request.mutable_updates(iteration)->set_allocated_entity(p4Entity);
    iteration++;
  }

  request.set_device_id(deviceId_);
  request.mutable_election_id()->set_high(electionId_->high());
  request.mutable_election_id()->set_low(electionId_->low());

  WriteResponse response = WriteResponse();
  ClientContext context;
  const std::string errorMessage = "Error executing Write command";
  Status status = stub_->Write(&context, request, &response);
  HandleStatus(status, errorMessage.c_str());
  return status;
}

std::list<P4TableEntry*> P4RuntimeClient::Read(std::list<P4TableEntry*> filter) {
  std::list<P4TableEntry*>::iterator it;
  std::list<P4TableEntry*> result;
  P4TableEntry * entry;
  ::P4_NAMESPACE_ID::ReadRequest request = ::P4_NAMESPACE_ID::ReadRequest();
  ClientContext context;
 
  request.set_device_id(deviceId_); 
  for(it=filter.begin(); it!=filter.end(); ++it){
    entry = *it;

    ::P4_NAMESPACE_ID::Action *p4EntityAction = new ::P4_NAMESPACE_ID::Action();
    p4EntityAction->set_action_id(entry->action.action_id);
    ::P4_NAMESPACE_ID::TableAction * p4EntityTableAction = new ::P4_NAMESPACE_ID::TableAction();
    p4EntityTableAction->set_allocated_action(p4EntityAction);
    ::P4_NAMESPACE_ID::TableEntry * p4EntityTableEntry = new ::P4_NAMESPACE_ID::TableEntry();
    p4EntityTableEntry->set_table_id(entry->table_id);
    p4EntityTableEntry->mutable_action()->set_allocated_action(p4EntityAction);
    request.add_entities()->set_allocated_table_entry(p4EntityTableEntry);
  }

  ReadResponse response = ReadResponse();
  std::unique_ptr<ClientReader<ReadResponse> > clientReader = stub_->Read(&context, request);
  clientReader->Read(&response);
  clientReader->Finish();

  for(int i=0; i<response.entities_size(); i++) {
    entry = new P4TableEntry();
    entry->table_id = response.entities().Get(i).table_entry().table_id();
    result.push_back(entry);
  }

  return result;  
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
// INFO:
// 1) https://github.com/p4lang/p4runtime-shell/blob/master/p4runtime_sh/p4runtime.py#L138
// 2) https://github.com/p4lang/p4runtime-shell/blob/master/p4runtime_sh/test.py#L40
// 3) https://github.com/p4lang/p4runtime/blob/16c55eebd887c949b59d6997bb2d841a59c6bb32/docs/v1/P4Runtime-Spec.mdk#L673 (edited) 
void P4RuntimeClient::SetUpStream() {
  ClientContext context;
  // Setup queues
  std::deque<streamType_> streamQueueIn_();
  std::deque<streamType_> streamQueueOut_();
  std::cout << "SetUpStream. Setting up stream from channel" << std::endl;
  stream_ = stub_->StreamChannel(&context);
  // Setup first the never-ending loops that listen to outgoing and incoming messages
  ReadOutgoingMessagesFromQueueInBg();
  ReadIncomingMessagesFromStreamInBg();
  // Perform the first connection with the device
  Handshake();
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#tear_down
void P4RuntimeClient::TearDown() {
  std::cout << "TearDown. Cleaning up stream, queues, threads and channel" << std::endl;
  // Appending NULL to the queues as an indication to determine that queues operations should finalise
  streamQueueOut_.push_back(NULL);
  streamQueueIn_.push_back(NULL);
  streamOutgoingThread_.join();
  stream_->Finish();
  streamIncomingThread_.join();
  channel_.reset();
}

// TODO: complete and check if this works. Check reference/p4runtime-shell-python/p4runtime.py#get_stream_packet
StreamMessageResponse* P4RuntimeClient::GetStreamPacket(int expectedType=-1, long timeout=1) {
    StreamMessageResponse* response = NULL;
    int responseType = -1;
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    std::chrono::duration<long> timeoutDuration(timeout);
    while (true) {
      auto remaining = timeoutDuration - (std::chrono::system_clock::now() - start);

      if (remaining < std::chrono::seconds{0}) {
        break;
      }

      if (!streamQueueIn_.empty()) {
        std::cout << "GetStreamPacket. BEFORE - Size for queueIN = " << streamQueueIn_.size() << std::endl;
        response = streamQueueIn_.front();

        if (response != NULL) {
          responseType = response->update_case();
          // Get out if an unexpected packet is retrieved
          // Details in p4runtime.pb.h:
          //   kArbitration = 1, kPacket = 2, kDigestAck = 3, kOther = 4, UPDATE_NOT_SET = 0
          // Note: -1 to indicate that we do not care about the type of packet (if that ever happens)
          if (expectedType != -1 && responseType != expectedType) {
            std::cout << "GetStreamPacket. Returning due to unexpected type obtained (received = " << responseType << ", expected = " << expectedType << ")" << std::endl;
            break;
          }
          // DEBUG
          CheckResponseType(response);
          // DEBUG - END

          streamQueueIn_.pop_front();
          std::cout << "GetStreamPacket. AFTER - Size for queueIN = " << streamQueueIn_.size() << std::endl;
        } else {
          std::cout << "GetStreamPacket. Returning due to NULL response" << std::endl;
          break;
        }
      }
    }
    return response;
}

void P4RuntimeClient::ReadIncomingMessagesFromStream() {
  // It only works initialising the response. Do not assign to NULL
  StreamMessageResponse* response = StreamMessageResponse().New();

  while (true) {
    // DEBUG
    if (!streamQueueIn_.empty()) {
      response = streamQueueIn_.front();
      if (response == NULL) {
        std::cout << "ReadIncomingMessagesFromStream. Reading NULL message not possible. Exiting thread" << std::endl;
        break;
      }
      // DEBUG
      CheckResponseType(response);
      // DEBUG - END
      // FIXME: Do something about this, do not just ignore the message and pop
      streamQueueIn_.pop_front();
    } else {
      try {
        // FIXME: This, and other operations with stream_, may be related to some segfaults
        // if (stream_ != NULL && stream_->Read(response)) {
        if (stream_ != NULL
          && ::grpc_connectivity_state::GRPC_CHANNEL_READY == channel_->GetState(false)
          && stream_->Read(response)) {
          std::cout << "ReadIncomingMessagesFromStream. Reading from stream" << std::endl;
          streamQueueIn_.push_back(response);
        }
      } catch (...) {
      }
    }
  }
}

void P4RuntimeClient::ReadIncomingMessagesFromStreamInBg() {
  try {
    streamIncomingThread_ = std::thread(&P4RuntimeClient::ReadIncomingMessagesFromStream, this);
  } catch (...) {
    std::cerr << "ReadIncomingMessagesFromStreamInBg. StreamChannel error. Closing stream" << std::endl;
    streamQueueIn_.push_back(NULL);
  }
}

void P4RuntimeClient::ReadOutgoingMessagesFromQueue() {
  StreamMessageRequest* request;
  while (true) {
    if (!streamQueueOut_.empty()) {
      std::cout << "ReadOutgoingMessagesFromQueue. BEFORE - Size for queueOUT = " << streamQueueOut_.size() << "." << std::endl;
      request = streamQueueOut_.front();
      if (request != NULL) {
        if (request->has_arbitration()) {
          std::cout << "ReadOutgoingMessagesFromQueue. Reading request. Arbitration for device ID = " << request->mutable_arbitration()->device_id() << "." << std::endl;
        }

        const StreamMessageRequest* requestConst = request;
        bool messageSent = false;
        // if (stream_ != NULL) {
        if (stream_ != NULL && ::grpc_connectivity_state::GRPC_CHANNEL_READY == channel_->GetState(false)) {
          messageSent = stream_->Write(*requestConst);
        }

        if (messageSent) {
          std::cout << "ReadOutgoingMessagesFromQueue. Sent Write request to server" << std::endl;
          streamQueueOut_.pop_front();
          std::cout << "ReadOutgoingMessagesFromQueue. AFTER - Size for queueOUT = " << streamQueueOut_.size() << "." << std::endl;
        }
      } else {
        std::cout << "ReadOutgoingMessagesFromQueue. Writing NULL message not possible. Exiting thread" << std::endl;
        // std::terminate();
        break;
      }
    }
  }
}

void P4RuntimeClient::ReadOutgoingMessagesFromQueueInBg() {
  try {
    streamOutgoingThread_ = std::thread(&P4RuntimeClient::ReadOutgoingMessagesFromQueue, this);
  } catch (...) {
    std::cerr << "ReadOutgoingMessagesFromQueueInBg. StreamChannel error. Closing stream" << std::endl;
    streamQueueOut_.push_back(NULL);
  }
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#handshake
void P4RuntimeClient::Handshake() {

  // Generate arbitration message
  StreamMessageRequest* requestOut = StreamMessageRequest().New();
  requestOut->mutable_arbitration()->set_device_id(deviceId_);
  requestOut->mutable_arbitration()->mutable_election_id()->set_high(electionId_->high());
  requestOut->mutable_arbitration()->mutable_election_id()->set_low(electionId_->low());
  streamQueueOut_.push_back(requestOut);
  std::cout << "Handshake. Pushing arbitration request to QueueOUT" << std::endl;

  StreamMessageResponse* reply = GetStreamPacket(::P4_NAMESPACE_ID::StreamMessageRequest::kArbitration, 2);
  if (reply == NULL) {
    std::cerr << "Handshake failed. Session could not be established with server" << std::endl;
    exit(1);
  }

  bool isMaster = reply->arbitration().status().code() == ::GRPC_NAMESPACE_ID::StatusCode::OK;
  std::string role = isMaster ? "master (R/W)" : "slave (R/O)";
  std::cout << "Handshake succeeded. Controller has " << role << " access to the server" << std::endl;
}

void P4RuntimeClient::SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId) {
  electionId_ = electionId;
  electionId_->set_high(electionId->high());
  electionId_->set_low(electionId->low());
}

void P4RuntimeClient::SetElectionId(std::string electionId) {
  try {
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

void P4RuntimeClient::CheckResponseType(::P4_NAMESPACE_ID::StreamMessageResponse* response) {
    int responseType = response->update_case();
    switch (responseType) {
    case ::P4_NAMESPACE_ID::StreamMessageRequest::kArbitration: {
      std::cout << "GetStreamPacket. Is arbitration" << std::endl;
      std::cout << "GetStreamPacket. Arbitration message = " << response->arbitration().SerializeAsString() << std::endl;
      std::cout << "GetStreamPacket. Device_id = " << response->arbitration().device_id() << std::endl;
      std::cout << "GetStreamPacket. Election_id - high = " << response->arbitration().election_id().high() << std::endl;
      std::cout << "GetStreamPacket. Election_id - low = " << response->arbitration().election_id().low() << std::endl;
      break;
    }
    case ::P4_NAMESPACE_ID::StreamMessageRequest::kPacket: {
      std::cout << "GetStreamPacket. Is a packet" << std::endl;
      break;
    }
    case ::P4_NAMESPACE_ID::StreamMessageRequest::kDigestAck:
    case ::P4_NAMESPACE_ID::StreamMessageRequest::UPDATE_NOT_SET:
      std::cout << "GetStreamPacket. Is a digest" << std::endl;
      break;
  }
}

void P4RuntimeClient::PrintP4Info(::P4_CONFIG_NAMESPACE_ID::P4Info p4Info_) {
  std::cout << "Printing P4Info\n" << std::endl;
  int tableSize = p4Info_.tables_size();
  std::cout << "Number of tables: " << tableSize << std::endl;
  if (tableSize == 0) {
    return;
  }
  std::cout << "Arch: " << p4Info_.pkg_info().arch() << std::endl;
  for (::P4_CONFIG_NAMESPACE_ID::Table table : p4Info_.tables()) {
    std::cout << "  Table id: " << table.preamble().id() << std::endl;
    std::cout << "  Table name: " << table.preamble().name() << std::endl;
    for (::P4_CONFIG_NAMESPACE_ID::MatchField matchField : table.match_fields()) {
      std::cout << "    Match id: " << matchField.id() << std::endl;
      std::cout << "    Match name: " << matchField.name() << std::endl;
      std::cout << "    Match type: " << matchField.match_type() << std::endl;
      std::cout << "    Match case: " << matchField.match_case() << std::endl;
    }
    for (::P4_CONFIG_NAMESPACE_ID::ActionRef actionRef : table.action_refs()) {
      for (::P4_CONFIG_NAMESPACE_ID::Action action : p4Info_.actions()) {
        if (actionRef.id() == action.preamble().id()) {
          std::cout << "    Action id: " << action.preamble().id() << std::endl;
          std::cout << "    Action name: " << action.preamble().name() << std::endl;
          for (::P4_CONFIG_NAMESPACE_ID::Action_Param param : action.params()) {
            std::cout << "      Action param id: " << param.id() << std::endl;
            std::cout << "      Action param name: " << param.name() << std::endl;
            std::cout << "      Action param bitwidth: " << param.bitwidth() << std::endl;
          }
        }
      }
    }
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

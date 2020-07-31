#include <fstream>
#include <grpcpp/grpcpp.h>
#include <sstream>

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
  std::cout << "Retrieving P4Info file" << std::endl;
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

Status P4RuntimeClient::InsertEntry(std::list<P4TableEntry*> entries) {
  return WriteEntry(entries, ::P4_NAMESPACE_ID::Update::INSERT);
}

Status P4RuntimeClient::ModifyEntry(std::list<P4TableEntry*> entries) {
  return WriteEntry(entries, ::P4_NAMESPACE_ID::Update::MODIFY);
}

Status P4RuntimeClient::DeleteEntry(std::list<P4TableEntry*> entries) {
  return WriteEntry(entries, ::P4_NAMESPACE_ID::Update::DELETE);
}

std::string P4RuntimeClient::EncodeParamValue(uint16_t value, size_t bitwidth) {
  char lsb, msb;
  std::string res;

  // From most to least significant bits (msb|lsb), retrieve specific octet by shifting
  // as many positions to the right as the most significant it in the octect
  // (that is, shift(bitwidth - most_significant_bit_position); e.g.,
  // "msb" will shift 16 bits - 16 bits, "lsb" will shift 16 bits - 8 bits)
  msb = value & 0xFF;
  lsb = value >> 8;
  res.push_back(msb);
  if (lsb != 0) {
    res.push_back(lsb);
  }

  // Fill in the remaining bytes (computed as the expected "nbytes" - sizeof generated string)
  // with leading zeros in order to fit the full bitwidth expected per value
  size_t nbytes = (bitwidth + 7) / 8;
  int remaining_zeros = nbytes - res.size();
  std::string res_byte = "";
  while (remaining_zeros-- > 0) {
    msb = 0 & 0xFF;
    res_byte.push_back(msb);
    res = res_byte + res;
    res_byte = "";
  }

  return res;
}

uint16_t P4RuntimeClient::DecodeParamValue(const std::string str) {
  uint16_t res;

  res = str[0];
  if (str.length() > 1) {
    res = res | uint16_t(str[1]) << 8;
  }

  return res;
}

// Write will call, among others, the next classes:
// ./stratum/bazel-stratum/external/com_github_p4lang_PI/proto/frontend/src/device_mgr.cpp
// ./stratum/bazel-stratum/external/com_github_p4lang_PI/proto/frontend/src/common.cpp
::GRPC_NAMESPACE_ID::Status P4RuntimeClient::WriteEntry(std::list<P4TableEntry*> entries,
    ::P4_NAMESPACE_ID::Update::Type updateType) {
  ::P4_NAMESPACE_ID::WriteRequest request = ::P4_NAMESPACE_ID::WriteRequest();
  request.add_updates();
  ::P4_NAMESPACE_ID::Update * update = request.mutable_updates(0);
  ::P4_NAMESPACE_ID::Entity * entity = new ::P4_NAMESPACE_ID::Entity();
  ::P4_NAMESPACE_ID::TableEntry * entity_table_entry = new ::P4_NAMESPACE_ID::TableEntry();
  ::P4_NAMESPACE_ID::TableAction * entity_table_action = new ::P4_NAMESPACE_ID::TableAction();
  ::P4_NAMESPACE_ID::Action * entity_action = new ::P4_NAMESPACE_ID::Action();
  ::P4_NAMESPACE_ID::FieldMatch_LPM * field_match_lpm = new ::P4_NAMESPACE_ID::FieldMatch_LPM();
  std::list<P4TableEntry*>::iterator it;
  std::list<P4Parameter>::iterator param_it;
  std::list<P4Match>::iterator match_it;
  P4TableEntry * entry;

  // Default timeout is infinite
  int64_t default_timeout_ns = 0;

  // // Initial checks (segfault here)
  // if (entry->action.default_action && ::P4_NAMESPACE_ID::Update::INSERT == updateType) {
  //   // The proper error would be FAILED_PRECONDITION (9) or so:
  //   // Default actions cannot be INSERTED in tables
  //   const std::string errorMessage = "Cannot INSERT entry for default action";
  //   Status status = Status::CANCELLED;
  //   HandleStatus(status, errorMessage.c_str());
  //   return status;
  // }

  // Level1. Update
  update->set_type(updateType);
  update->set_allocated_entity(entity);
  std::cout << "Write . Setting entity type = " << update->type() << std::endl;

  int iteration = 0;
  for (it = entries.begin(); it != entries.end(); ++it) {
    entry = *it;

    // Level2. Entity
    entity->set_allocated_table_entry(entity_table_entry);

    // Level3. TableEntry
    entity_table_entry->set_table_id(entry->table_id);
    entity_table_entry->set_allocated_action(entity_table_action);

    std::cout << "Write . Setting table id = " << entity_table_entry->table_id() << std::endl;

    // Level4. TableAction
    entity_table_action->set_allocated_action(entity_action);

    // Level5. Action
    entity_action->set_action_id(entry->action.action_id);
    std::cout << "Write . Setting action id = " << entity_action->action_id() << std::endl;

    // Insert action parameters
    for (param_it = entry->action.parameters.begin(); param_it != entry->action.parameters.end(); ++param_it) {
      // Perform any expected conversion to bytestring at this point
      // param_it->value = std::stoi(param_it->value, 0, 16);
      // Level5. Action
      entity_action->add_params();
      // Level6. Action_Param
      ::P4_NAMESPACE_ID::Action_Param * entity_table_action_param = entity_action->mutable_params(param_it->id - 1);
      entity_table_action_param->set_param_id(param_it->id);
      entity_table_action_param->set_value(EncodeParamValue(param_it->value, param_it->bitwidth));
      std::cout << "Write . Setting param number = " << entity_table_action_param->param_id() << ", value = " << 
        static_cast<std::string>(entity_table_action_param->value()) << std::endl;
    }

    // Level3. TableEntry
    // Set default action and TTL for entry (in nanoseconds, 0 is infinite)
    entity_table_entry->set_is_default_action(entry->action.default_action);
    // NOTE: currently ide_timeout is always set to 0, as "pow(10, 9)" for a second is rejected
    if (entry->action.default_action) {
      entity_table_entry->set_idle_timeout_ns(0);
    } else {
      if (entry->timeout_ns > 0) {
        entity_table_entry->set_idle_timeout_ns(entry->timeout_ns);
      } else {
        entity_table_entry->set_idle_timeout_ns(default_timeout_ns);
      }
    }
    std::cout << "Write . Setting timeout = " << entity_table_entry->idle_timeout_ns() << " ns" << std::endl;

    // Insert match (if any)
    for (match_it = entry->matches.begin(); match_it != entry->matches.end(); ++match_it) {

      ::P4_NAMESPACE_ID::FieldMatch * field_match;
      ::P4_NAMESPACE_ID::FieldMatch_Exact * field_match_exact;
      ::P4_NAMESPACE_ID::FieldMatch_Ternary * field_match_ternary;
      ::P4_NAMESPACE_ID::FieldMatch_LPM * field_match_lpm;
      ::P4_NAMESPACE_ID::FieldMatch_Range * field_match_range;
      ::P4_NAMESPACE_ID::FieldMatch_Optional * field_match_optional;
      ::PROTOBUF_NAMESPACE_ID::Any * field_match_other;

      // Level3. TableEntry
      entity_table_entry->add_match();

      // Perform any expected conversion to bytestring at this point
      // match->value = std::stoi(match->value, 0, 16);
      std::cout << "Write . Setting match type = " << match_it->type << ", value = " 
        << match_it->value << std::endl;
      field_match = entity_table_entry->mutable_match(0);
      field_match->set_field_id(match_it->field_id);

      // Level4. FieldMatch
      switch (match_it->type) {
        case P4MatchType::exact : {
          field_match_exact = new ::P4_NAMESPACE_ID::FieldMatch_Exact();
          field_match_exact->set_value(EncodeParamValue(match_it->value, match_it->bitwidth));
          field_match->set_allocated_exact(field_match_exact);
          break;
        }
        case P4MatchType::ternary : {
          field_match_ternary = new ::P4_NAMESPACE_ID::FieldMatch_Ternary();
          field_match_ternary->set_value(EncodeParamValue(match_it->value, match_it->bitwidth));
          field_match_ternary->set_mask(match_it->ternary_mask);
          field_match->set_allocated_ternary(field_match_ternary);
          entity_table_entry->set_priority(entry->priority);
          break;
        }
        case P4MatchType::lpm : {
          field_match_lpm = new ::P4_NAMESPACE_ID::FieldMatch_LPM();
          field_match_lpm->set_value(EncodeParamValue(match_it->value, match_it->bitwidth));
          field_match_lpm->set_prefix_len(match_it->lpm_prefix);
          field_match->set_allocated_lpm(field_match_lpm);
          break;
        }
        case P4MatchType::range : {
          field_match_range = new ::P4_NAMESPACE_ID::FieldMatch_Range();
          field_match_range->set_high(match_it->range_high);
          field_match_range->set_high(match_it->range_low);
          field_match->set_allocated_range(field_match_range);
          entity_table_entry->set_priority(entry->priority);
          break;
        }
        case P4MatchType::optional : {
          field_match_optional = new ::P4_NAMESPACE_ID::FieldMatch_Optional();
          field_match->set_allocated_optional(field_match_optional);
          entity_table_entry->set_priority(entry->priority);
          break;
        }
        case P4MatchType::other : {
          field_match_other = new ::PROTOBUF_NAMESPACE_ID::Any();
          field_match->set_allocated_other(field_match_other);
          break;
        }
        default:
          std::cout << "Provided match is not expected" << std::endl;
      }
    }

    // request.mutable_updates(iteration)->set_allocated_entity(entity);
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

// TODO: investigate how to read all available table entries (e.g., no filter defined)
std::list<P4TableEntry*> P4RuntimeClient::ReadEntry(std::list<P4TableEntry*> filter) {
  std::list<P4TableEntry*>::iterator it;
  std::list<P4TableEntry*> result;
  P4TableEntry * filter_entry;
  P4TableEntry * entry;
  P4Match * match;
  P4Parameter * param;
  ::P4_NAMESPACE_ID::ReadRequest request = ::P4_NAMESPACE_ID::ReadRequest();
  ClientContext context;
 
  request.set_device_id(deviceId_);
  for (it = filter.begin(); it != filter.end(); ++it) {
    filter_entry = *it;
    ::P4_NAMESPACE_ID::Action *p4EntityAction = new ::P4_NAMESPACE_ID::Action();
    p4EntityAction->set_action_id(filter_entry->action.action_id);
    ::P4_NAMESPACE_ID::TableAction * p4EntityTableAction = new ::P4_NAMESPACE_ID::TableAction();
    p4EntityTableAction->set_allocated_action(p4EntityAction);
    ::P4_NAMESPACE_ID::TableEntry * p4EntityTableEntry = new ::P4_NAMESPACE_ID::TableEntry();
    p4EntityTableEntry->set_table_id(filter_entry->table_id);
    std::cout << "Read . Requested table id = " << filter_entry->table_id << std::endl;
    std::cout << "Read . Requested action id = " << filter_entry->action.action_id << std::endl;
    p4EntityTableEntry->mutable_action()->set_allocated_action(p4EntityAction);
    request.add_entities()->set_allocated_table_entry(p4EntityTableEntry);
  }

  ReadResponse response = ReadResponse();
  std::unique_ptr<ClientReader<ReadResponse> > clientReader = stub_->Read(&context, request);
  clientReader->Read(&response);
  clientReader->Finish();
  bool read_entry_matches_filter = false;

  std::cout << "Read . Entities available = " << response.entities_size() << std::endl;
  for (int i = 0; i < response.entities_size(); i++) {
    const p4::v1::TableEntry rentry = response.entities().Get(i).table_entry();

    // Check returned entries against those that were sent in the filter
    for (it = filter.begin(); it != filter.end(); ++it) {
      filter_entry = *it;
      if ((filter_entry->table_id == rentry.table_id()) &&
        (filter_entry->action.action_id == rentry.action().action().action_id())) {
          read_entry_matches_filter = true;
        }
    }
    // If there is no match, skip the current iteration (i.e., do not output entries that were not requested)
    if (!read_entry_matches_filter) {
      continue;
    }

    entry = new P4TableEntry();
    entry->table_id = rentry.table_id();
    std::cout << "Read . Fetching table id = " << entry->table_id << std::endl;
    entry->action.action_id = rentry.action().action().action_id();
    std::cout << "Read . Fetching action id = " << entry->action.action_id << std::endl;
    std::cout << "Read . Fetching param size = " << rentry.action().action().params().size() << std::endl;
    for (int p = 0; p < rentry.action().action().params_size(); p++) {
      param = new P4Parameter();
      if (rentry.action().action().params().size() > 0) {
        param->id = rentry.action().action().params(p).param_id();
        param->value = DecodeParamValue(rentry.action().action().params(p).value());
        std::cout << "Read . Fetching param id = " << param->id << ", value = " << param->value << std::endl;
      }
      entry->action.parameters.push_back(*param);
    }
    entry->timeout_ns = rentry.idle_timeout_ns();
    std::cout << "Read . Fetching timeout = " << entry->timeout_ns << std::endl;

    std::cout << "Read . Fetching match size = " << rentry.match_size() << std::endl;
    for (int m = 0; m < rentry.match_size(); m++) {
      match = new P4Match();
      match->field_id = rentry.match(m).field_id();
      if (rentry.match(m).has_exact()) {
        match->type = P4MatchType::exact;
        match->value = DecodeParamValue(rentry.match(m).exact().value());
      } else if (rentry.match(m).has_ternary()) {
        match->type = P4MatchType::ternary;
        match->value = DecodeParamValue(rentry.match(m).ternary().value());
        match->ternary_mask = rentry.match(m).ternary().mask();
        entry->priority = rentry.priority();
      } else if (rentry.match(m).has_lpm()) {
        match->type = P4MatchType::lpm;
        match->value = DecodeParamValue(rentry.match(m).lpm().value());
        match->lpm_prefix = rentry.match(m).lpm().prefix_len();
      } else if (rentry.match(m).has_range()) {
        match->type = P4MatchType::range;
        match->range_low = rentry.match(m).range().low();
        match->range_high = rentry.match(m).range().high();
        entry->priority = rentry.priority();
      } else if (rentry.match(m).has_optional()) {
        match->type = P4MatchType::optional;
        match->value = DecodeParamValue(rentry.match(m).optional().value());
        entry->priority = rentry.priority();
      } else if (rentry.match(m).has_other()) {
        match->type = P4MatchType::other;
      }
      std::cout << "Read . Fetching match type = " << match->type << ", value = " <<
        match->value << std::endl;
      entry->matches.push_back(*match);
    }

    result.push_back(entry);
  }

  return result;  
}

std::string P4RuntimeClient::APIVersion() {
  std::cout << "Fetching version of the API" << std::endl;

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
  std::cout << "SetUpStream. Setting up stream from channel" << std::endl;
  stream_ = stub_->StreamChannel(&context);
  inThreadStop_ = false;
  outThreadStop_ = false;
  // Setup first the never-ending loops that listen to outgoing and incoming messages
  ReadOutgoingMessagesFromQueueInBg();
  ReadIncomingMessagesFromStreamInBg();
  // Perform the first connection with the device
  Handshake();
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#tear_down
void P4RuntimeClient::TearDown() {
  std::cout << "TearDown. Cleaning up stream, queues, threads and channel" << std::endl;
  qOutMtx_.lock();
  outThreadStop_ = true;
  qOutMtx_.unlock();
  qInMtx_.lock();
  inThreadStop_ = true;
  qInMtx_.unlock();

  streamOutgoingThread_.join();
  streamIncomingThread_.join();
  stream_->Finish();
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
      
      qInMtx_.lock();
      if (!streamQueueIn_.empty()) {
        std::cout << "GetStreamPacket. BEFORE - Size for queueIN = " << streamQueueIn_.size() << std::endl;
        response = streamQueueIn_.front();
        streamQueueIn_.pop();
        qInMtx_.unlock();

        if (response == NULL) {
          std::cout << "GetStreamPacket. Returning due to NULL response" << std::endl;
          break;
        }

        responseType = response->update_case();
        // Get out if an unexpected packet is retrieved
        // Details in p4runtime.pb.h:
        //   kArbitration = 1, kPacket = 2, kDigestAck = 3, kOther = 4, UPDATE_NOT_SET = 0
        // Note: -1 to indicate that we do not care about the type of packet (if that ever happens)
        if (expectedType != -1 && responseType != expectedType) {
          std::cout << "GetStreamPacket. Returning due to unexpected type obtained (received = " << responseType << ", expected = " << expectedType << ")" << std::endl;
          return response;
        }

        // DEBUG
        CheckResponseType(response);
        // DEBUG - END
        std::cout << "GetStreamPacket. Returning with proper response" << std::endl;
        return response;
      } else {
        qInMtx_.unlock();
      }
    }

    return NULL;
}

void P4RuntimeClient::ReadIncomingMessagesFromStream() {
  StreamMessageResponse* response;

  while (true) {
    qInMtx_.lock();
    if (inThreadStop_) {
      qInMtx_.unlock();
      std::cout << "ReadIncomingMessagesFromStream. Exiting thread, I must stop" << std::endl;
      break;
    }

    qInMtx_.unlock();
    try {
      if (stream_ == NULL || ::grpc_connectivity_state::GRPC_CHANNEL_READY != channel_->GetState(false)) {
        continue;
      }
      response = StreamMessageResponse().New();
      if (stream_->Read(response)) {
        std::cout << "ReadIncomingMessagesFromStream. Reading from stream" << std::endl;
        qInMtx_.lock();
        streamQueueIn_.push(response);
        qInMtx_.unlock();
      } else {
        delete response;
      }
    } catch (...) {
    }
  }
}

void P4RuntimeClient::ReadIncomingMessagesFromStreamInBg() {
  try {
    streamIncomingThread_ = std::thread(&P4RuntimeClient::ReadIncomingMessagesFromStream, this);
  } catch (...) {
    std::cerr << "ReadIncomingMessagesFromStreamInBg. StreamChannel error. Closing stream" << std::endl;
    qInMtx_.lock();
    inThreadStop_ = true;
    qInMtx_.unlock();
  }
}

void P4RuntimeClient::ReadOutgoingMessagesFromQueue() {
  StreamMessageRequest* request;
  while (true) {
    qOutMtx_.lock();
    if (outThreadStop_) {
      qOutMtx_.unlock();
      std::cout << "ReadOutgoingMessagesFromQueue. Exiting thread I must stop" << std::endl;
      if (stream_ != NULL) {
        stream_->WritesDone();
     }
      break;
    }

    if (!streamQueueOut_.empty() && stream_ != NULL && ::grpc_connectivity_state::GRPC_CHANNEL_READY == channel_->GetState(false)) {
      std::cout << "ReadOutgoingMessagesFromQueue. BEFORE - Size for queueOUT = " << streamQueueOut_.size() << "." << std::endl;
      request = streamQueueOut_.front();
      streamQueueOut_.pop();
      qOutMtx_.unlock();
      
      if (request == NULL) {
        std::cout << "Ignoring NULL request" << std::endl;
        continue;
      } 

      if (request->has_arbitration()) {
        std::cout << "ReadOutgoingMessagesFromQueue. Reading request. Arbitration for device ID = " << request->mutable_arbitration()->device_id() << "." << std::endl;
      }

      const StreamMessageRequest* requestConst = request;
      bool messageSent = stream_->Write(*requestConst);

      if (messageSent) {
        std::cout << "ReadOutgoingMessagesFromQueue. Sent Write request to server" << std::endl;
      } else {
        std::cout << "ReadOutgoingMessageFromQueue. Could not send message to server" << std::endl;
      } 
    } else {
      qOutMtx_.unlock();
    }
  }
}

void P4RuntimeClient::ReadOutgoingMessagesFromQueueInBg() {
  try {
    streamOutgoingThread_ = std::thread(&P4RuntimeClient::ReadOutgoingMessagesFromQueue, this);
  } catch (...) {
    std::cerr << "ReadOutgoingMessagesFromQueueInBg. StreamChannel error. Closing stream" << std::endl;
    qOutMtx_.lock();
    outThreadStop_ = true;
    qOutMtx_.unlock();
  }
}

// TODO: complete. Check reference/p4runtime-shell-python/p4runtime.py#handshake
void P4RuntimeClient::Handshake() {

  // Generate arbitration message
  StreamMessageRequest* requestOut = StreamMessageRequest().New();
  requestOut->mutable_arbitration()->set_device_id(deviceId_);
  requestOut->mutable_arbitration()->mutable_election_id()->set_high(electionId_->high());
  requestOut->mutable_arbitration()->mutable_election_id()->set_low(electionId_->low());
  qOutMtx_.lock();
  streamQueueOut_.push(requestOut);
  qOutMtx_.unlock();
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
      std::cout << "    Match bitwidth: " << matchField.bitwidth() << std::endl;
      std::cout << "    Match type: " << matchField.match_type() << std::endl;
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

::PROTOBUF_NAMESPACE_ID::uint32 P4RuntimeClient::GetP4TableIdFromName(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4Info_, std::string tableName) {
  for (::P4_CONFIG_NAMESPACE_ID::Table table : p4Info_.tables()) {
    if (tableName == table.preamble().name()) {
      return table.preamble().id();
    }
  }
  return 0L;
}

::PROTOBUF_NAMESPACE_ID::uint32 P4RuntimeClient::GetP4ActionIdFromName(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4Info_, std::string actionName) {
  for (::P4_CONFIG_NAMESPACE_ID::Action action : p4Info_.actions()) {
    if (actionName == action.preamble().name()) {
      return action.preamble().id();
    }
  }
  return 0L;
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
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

P4RuntimeClient::P4RuntimeClient(std::string bind_address,
                std::string config,
                ::PROTOBUF_NAMESPACE_ID::uint64 device_id,
                std::string election_id) {
  channel_ = CreateChannel(bind_address, InsecureChannelCredentials());
  stub_ = P4Runtime::NewStub(channel_);

  // if (!logFile_.is_open()) {
  //   logFile_.open(logFilePath_);
  // }

  auto configIndex = config.find_first_of(",");
  p4InfoPath_ = config.substr(0, configIndex);
  binaryCfgPath_ = config.substr(configIndex + 1);

  deviceId_ = device_id;
  SetElectionId(election_id);

  SetUpStream();
}

// RPC methods

Status P4RuntimeClient::SetFwdPipeConfig() {
  std::cout << "Setting forwarding pipeline config" << std::endl;
  SetForwardingPipelineConfigRequest request = SetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  request.set_allocated_election_id(electionId_);
  request.set_action(request.VERIFY_AND_COMMIT);

  std::ifstream p4info_file_ifstream("./" + p4InfoPath_);
  if (!p4info_file_ifstream.is_open()) {
    const std::string error_message = "Cannot open file " + p4InfoPath_;
    HandleException(error_message.c_str());
  }
  ZeroCopyInputStream* p4info_file = new IstreamInputStream(&p4info_file_ifstream, -1);

  std::ifstream binary_cfg_file_ifstream("./" + binaryCfgPath_);
  if (!binary_cfg_file_ifstream.is_open()) {
    const std::string error_message = "Cannot open file " + binaryCfgPath_;
    HandleException(error_message.c_str());
  }
  ZeroCopyInputStream* binary_cfg_file = new IstreamInputStream(&binary_cfg_file_ifstream, -1);
  std::stringstream binary_cfg_file_stream;
  binary_cfg_file_stream << binary_cfg_file_ifstream.rdbuf();
  std::string binary_cfg_file_str = binary_cfg_file_stream.str();

  ForwardingPipelineConfig* config = ForwardingPipelineConfig().New();
  config->set_p4_device_config(binary_cfg_file_str);
  P4Info p4info = request.config().p4info();
  ::PROTOBUF_NAMESPACE_ID::TextFormat::Merge(p4info_file, &p4info);
  // The current information (already allocated in memory) is shared with the config object
  config->set_allocated_p4info(&p4info);
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
  P4Info p4info = response.config().p4info();
  PrintP4Info(p4info);
  return p4info;
}

Status P4RuntimeClient::InsertTableEntry(std::list<P4TableEntry*> entries) {
  return WriteTableEntry(entries, ::P4_NAMESPACE_ID::Update::INSERT);
}

Status P4RuntimeClient::ModifyTableEntry(std::list<P4TableEntry*> entries) {
  return WriteTableEntry(entries, ::P4_NAMESPACE_ID::Update::MODIFY);
}

Status P4RuntimeClient::DeleteTableEntry(std::list<P4TableEntry*> entries) {
  return WriteTableEntry(entries, ::P4_NAMESPACE_ID::Update::DELETE);
}

std::string P4RuntimeClient::EncodeParamValue(uint16_t value, size_t bitwidth) {
  return encode_param_value(value, bitwidth);
}

uint16_t P4RuntimeClient::DecodeParamValue(const std::string str) {
  return decode_param_value(str);
}

// Write will call, among others, the next classes:
// ./stratum/bazel-stratum/external/com_github_p4lang_PI/proto/frontend/src/device_mgr.cpp
// ./stratum/bazel-stratum/external/com_github_p4lang_PI/proto/frontend/src/common.cpp
::GRPC_NAMESPACE_ID::Status P4RuntimeClient::WriteTableEntry(std::list<P4TableEntry*> entries,
    ::P4_NAMESPACE_ID::Update::Type update_type) {
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
  update->set_type(update_type);
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
  const std::string error_message = "Error executing Write command";
  Status status = stub_->Write(&context, request, &response);
  HandleStatus(status, error_message.c_str());
  return status;
}

// TODO: investigate how to read all available table entries (e.g., no filter defined)
std::list<P4TableEntry*> P4RuntimeClient::ReadTableEntry(std::list<P4TableEntry*> filter) {
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
    ::P4_NAMESPACE_ID::Action *p4_entity_action = new ::P4_NAMESPACE_ID::Action();
    p4_entity_action->set_action_id(filter_entry->action.action_id);
    ::P4_NAMESPACE_ID::TableAction * p4_entity_table_action = new ::P4_NAMESPACE_ID::TableAction();
    p4_entity_table_action->set_allocated_action(p4_entity_action);
    ::P4_NAMESPACE_ID::TableEntry * p4_entity_table_entry = new ::P4_NAMESPACE_ID::TableEntry();
    p4_entity_table_entry->set_table_id(filter_entry->table_id);
    std::cout << "Read . Requested table id = " << filter_entry->table_id << std::endl;
    std::cout << "Read . Requested action id = " << filter_entry->action.action_id << std::endl;
    p4_entity_table_entry->mutable_action()->set_allocated_action(p4_entity_action);
    request.add_entities()->set_allocated_table_entry(p4_entity_table_entry);
  }

  ReadResponse response = ReadResponse();
  std::unique_ptr<ClientReader<ReadResponse> > client_reader = stub_->Read(&context, request);
  client_reader->Read(&response);
  client_reader->Finish();
  bool read_entry_matches_filter = false;

  std::cout << "Read . Entities available = " << response.entities_size() << std::endl;
  for (int i = 0; i < response.entities_size(); i++) {
    const p4::v1::TableEntry ret_entry = response.entities().Get(i).table_entry();

    // Check returned entries against those that were sent in the filter
    for (it = filter.begin(); it != filter.end(); ++it) {
      filter_entry = *it;
      if ((filter_entry->table_id == ret_entry.table_id()) &&
        (filter_entry->action.action_id == ret_entry.action().action().action_id())) {
          // TODO: filter by match value!
          read_entry_matches_filter = true;
        }
    }
    // If there is no match, skip the current iteration (i.e., do not output entries that were not requested)
    if (!read_entry_matches_filter) {
      continue;
    }

    entry = new P4TableEntry();
    entry->table_id = ret_entry.table_id();
    std::cout << "Read . Fetching table id = " << entry->table_id << std::endl;
    entry->action.action_id = ret_entry.action().action().action_id();
    std::cout << "Read . Fetching action id = " << entry->action.action_id << std::endl;
    std::cout << "Read . Fetching param size = " << ret_entry.action().action().params().size() << std::endl;
    for (int p = 0; p < ret_entry.action().action().params_size(); p++) {
      param = new P4Parameter();
      if (ret_entry.action().action().params().size() > 0) {
        param->id = ret_entry.action().action().params(p).param_id();
        param->value = DecodeParamValue(ret_entry.action().action().params(p).value());
        std::cout << "Read . Fetching param id = " << param->id << ", value = " << param->value << std::endl;
      }
      entry->action.parameters.push_back(*param);
    }
    entry->timeout_ns = ret_entry.idle_timeout_ns();
    std::cout << "Read . Fetching timeout = " << entry->timeout_ns << std::endl;

    std::cout << "Read . Fetching match size = " << ret_entry.match_size() << std::endl;
    for (int m = 0; m < ret_entry.match_size(); m++) {
      match = new P4Match();
      match->field_id = ret_entry.match(m).field_id();
      if (ret_entry.match(m).has_exact()) {
        match->type = P4MatchType::exact;
        match->value = DecodeParamValue(ret_entry.match(m).exact().value());
      } else if (ret_entry.match(m).has_ternary()) {
        match->type = P4MatchType::ternary;
        match->value = DecodeParamValue(ret_entry.match(m).ternary().value());
        match->ternary_mask = ret_entry.match(m).ternary().mask();
        entry->priority = ret_entry.priority();
      } else if (ret_entry.match(m).has_lpm()) {
        match->type = P4MatchType::lpm;
        match->value = DecodeParamValue(ret_entry.match(m).lpm().value());
        match->lpm_prefix = ret_entry.match(m).lpm().prefix_len();
      } else if (ret_entry.match(m).has_range()) {
        match->type = P4MatchType::range;
        match->range_low = ret_entry.match(m).range().low();
        match->range_high = ret_entry.match(m).range().high();
        entry->priority = ret_entry.priority();
      } else if (ret_entry.match(m).has_optional()) {
        match->type = P4MatchType::optional;
        match->value = DecodeParamValue(ret_entry.match(m).optional().value());
        entry->priority = ret_entry.priority();
      } else if (ret_entry.match(m).has_other()) {
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

std::list<P4DirectCounterEntry*> P4RuntimeClient::ReadDirectCounterEntry(std::list<P4DirectCounterEntry*> filter) {
  std::list<P4DirectCounterEntry*>::iterator it;
  std::list<P4DirectCounterEntry*> result;
  P4DirectCounterEntry * filter_entry;
  P4DirectCounterEntry * entry;
  ::P4_NAMESPACE_ID::TableEntry *p4_entity_direct_counter_entry_table = new ::P4_NAMESPACE_ID::TableEntry();
  ::P4_NAMESPACE_ID::ReadRequest request = ::P4_NAMESPACE_ID::ReadRequest();
  ClientContext context;
 
  request.set_device_id(deviceId_);
  for (it = filter.begin(); it != filter.end(); ++it) {
    filter_entry = *it;
    ::P4_NAMESPACE_ID::DirectCounterEntry *p4_entity_direct_counter_entry = 
      new ::P4_NAMESPACE_ID::DirectCounterEntry();
    p4_entity_direct_counter_entry_table->set_table_id(filter_entry->table_entry.table_id);
    p4_entity_direct_counter_entry->set_allocated_table_entry(p4_entity_direct_counter_entry_table);
    std::cout << "Read . Requested table id = " << filter_entry->table_entry.table_id << std::endl;
    request.add_entities()->set_allocated_direct_counter_entry(p4_entity_direct_counter_entry);
  }

  ReadResponse response = ReadResponse();
  std::unique_ptr<ClientReader<ReadResponse> > client_reader = stub_->Read(&context, request);
  client_reader->Read(&response);
  client_reader->Finish();
  bool read_entry_matches_filter = false;

  std::cout << "Read . Entities available = " << response.entities_size() << std::endl;
  for (int i = 0; i < response.entities_size(); i++) {
    const p4::v1::CounterEntry ret_entry = response.entities().Get(i).counter_entry();
    entry = new P4DirectCounterEntry();
    entry->data = ret_entry.data();
    std::cout << "Read . Fetching counter data (byte count) = " << entry->data.byte_count() << std::endl;
    std::cout << "Read . Fetching counter data (packet count) = " << entry->data.packet_count() << std::endl;
    result.push_back(entry);
  }

  return result;  
}

std::list<P4DirectCounterEntry*> P4RuntimeClient::ReadDirectCounterEntries() {
  std::list<P4DirectCounterEntry *> counter_entries;
  P4DirectCounterEntry counter_entry;
  // All counters retrieved by using ID = 0
  counter_entry.table_entry.table_id = 0;
  counter_entries.push_back(&counter_entry);
  return ReadDirectCounterEntry(counter_entries);  
}

std::list<P4CounterEntry*> P4RuntimeClient::ReadIndirectCounterEntry(std::list<P4CounterEntry*> filter) {
  std::list<P4CounterEntry*>::iterator it;
  std::list<P4CounterEntry*> result;
  P4CounterEntry * filter_entry;
  P4CounterEntry * entry;
  ::P4_NAMESPACE_ID::ReadRequest request = ::P4_NAMESPACE_ID::ReadRequest();
  ClientContext context;
 
  request.set_device_id(deviceId_);
  for (it = filter.begin(); it != filter.end(); ++it) {
    filter_entry = *it;
    ::P4_NAMESPACE_ID::CounterEntry *p4_entity_indirect_counter_entry = new ::P4_NAMESPACE_ID::CounterEntry();
    // Counter == 0: provide all counter cells for all counters (as per P4Runtime spec)
    p4_entity_indirect_counter_entry->set_counter_id(filter_entry->counter_id);
    std::cout << "Read . Requested counter id = " << filter_entry->counter_id << std::endl;
    // No index defined: provide all counter cells (as per P4Runtime spec)
    if (filter_entry->index.index() >= 0) {
      ::P4_NAMESPACE_ID::Index *p4_entity_indirect_counter_entry_index = new ::P4_NAMESPACE_ID::Index();
      p4_entity_indirect_counter_entry_index->set_index(filter_entry->index.index());
      std::cout << "Read . Requested index = " << filter_entry->index.index() << std::endl;
      p4_entity_indirect_counter_entry->set_allocated_index(p4_entity_indirect_counter_entry_index);
    }
    request.add_entities()->set_allocated_counter_entry(p4_entity_indirect_counter_entry);
  }

  ReadResponse response = ReadResponse();
  std::unique_ptr<ClientReader<ReadResponse> > client_reader = stub_->Read(&context, request);
  client_reader->Read(&response);
  client_reader->Finish();
  bool read_entry_matches_filter = false;

  std::cout << "Read . Entities available = " << response.entities_size() << std::endl;
  for (int i = 0; i < response.entities_size(); i++) {
    const p4::v1::CounterEntry ret_entry = response.entities().Get(i).counter_entry();
    entry = new P4CounterEntry();
    entry->counter_id = ret_entry.counter_id();
    entry->index = ret_entry.index();
    entry->data = ret_entry.data();
    if (entry->data.byte_count() > 0 || entry->data.packet_count() > 0) {
      std::cout << "Read . Fetching counter id=" << entry->counter_id <<
      ", index=" << entry->index.index() << ", values: bytes=" <<
      entry->data.byte_count() << ", packets=" << entry->data.packet_count() << std::endl;
    } else {
      std::cout << "Read . Fetching counter id=" << entry->counter_id <<
      ", index=" << entry->index.index() << " values: empty" << std::endl;
    }
    result.push_back(entry);
  }

  return result;  
}

std::list<P4CounterEntry*> P4RuntimeClient::ReadIndirectCounterEntries() {
  std::list<P4CounterEntry *> counter_entries;
  P4CounterEntry counter_entry;
  // All counters retrieved by using ID = 0
  counter_entry.counter_id = 0;
  // All indexes retrieved by leaving the index unset. This is done by
  //using a specific flag (negative number, which cannot be used anyway)
  ::P4_NAMESPACE_ID::Index *p4_indirect_counter_entry_index = new ::P4_NAMESPACE_ID::Index();
  p4_indirect_counter_entry_index->set_index(-1);
  counter_entry.index = *p4_indirect_counter_entry_index;
  counter_entries.push_back(&counter_entry);
  return ReadIndirectCounterEntry(counter_entries);  
}

std::string P4RuntimeClient::APIVersion() {
  std::cout << "Fetching version of the API" << std::endl;

  CapabilitiesRequest request = CapabilitiesRequest();
  CapabilitiesResponse response = CapabilitiesResponse();
  ClientContext context;

  Status status = stub_->Capabilities(&context, request, &response);
  const std::string error_message = "Cannot retrieve information on the API version";
  HandleStatus(status, error_message.c_str());
  return response.p4runtime_api_version();
}

// Ancillary methods

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

void P4RuntimeClient::TearDown() {
  std::cout << "TearDown. Cleaning up queues and threads" << std::endl;
  // if (logFile_.is_open()) {
  //   logFile_.close();
  // }

  qOutMtx_.lock();
  outThreadStop_ = true;
  qOutMtx_.unlock();
  qInMtx_.lock();
  inThreadStop_ = true;
  qInMtx_.unlock();

  // FIXME: one of these may produce "aborted (core dumped)"
  // NOTE: when not there, a "segmentation fault (core dumped)" occurs
  if (streamOutgoingThread_.joinable()) {
    streamOutgoingThread_.join();
  }
  if (streamIncomingThread_.joinable()) {
    streamIncomingThread_.join();
  }
  stream_->Finish();
  // FIXME: a "segmentation fault (core dumped)" occurs at some point after this
}

StreamMessageResponse* P4RuntimeClient::GetStreamPacket(int expected_type=-1, long timeout=1) {
    StreamMessageResponse* response = NULL;
    int response_type = -1;
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    std::chrono::duration<long> timeout_duration(timeout);
    while (true) {
      auto remaining = timeout_duration - (std::chrono::system_clock::now() - start);
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

        response_type = response->update_case();
        // Get out if an unexpected packet is retrieved
        // Details in p4runtime.pb.h:
        //   kArbitration = 1, kPacket = 2, kDigestAck = 3, kOther = 4, UPDATE_NOT_SET = 0
        // Note: -1 to indicate that we do not care about the type of packet (if that ever happens)
        if (expected_type != -1 && response_type != expected_type) {
          std::cout << "GetStreamPacket. Returning due to unexpected type obtained (received = " << response_type << ", expected = " << expected_type << ")" << std::endl;
          return response;
        }

        print_response_type(response);
        std::cout << "GetStreamPacket. Returning with proper response" << std::endl;
        return response;
      }
      qInMtx_.unlock();
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

      const StreamMessageRequest* request_const = request;
      bool message_sent = stream_->Write(*request_const);

      if (message_sent) {
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

void P4RuntimeClient::Handshake() {
  // Generate arbitration message
  StreamMessageRequest* request_out = StreamMessageRequest().New();
  request_out->mutable_arbitration()->set_device_id(deviceId_);
  request_out->mutable_arbitration()->mutable_election_id()->set_high(electionId_->high());
  request_out->mutable_arbitration()->mutable_election_id()->set_low(electionId_->low());
  qOutMtx_.lock();
  streamQueueOut_.push(request_out);
  qOutMtx_.unlock();
  std::cout << "Handshake. Pushing arbitration request to QueueOUT" << std::endl;

  StreamMessageResponse* reply = GetStreamPacket(::P4_NAMESPACE_ID::StreamMessageRequest::kArbitration, 2);
  if (reply == NULL) {
    std::cerr << "Handshake failed. Session could not be established with server" << std::endl;
    exit(1);
  }

  bool is_master = reply->arbitration().status().code() == ::GRPC_NAMESPACE_ID::StatusCode::OK;
  std::string role = is_master ? "master (R/W)" : "slave (R/O)";
  std::cout << "Handshake succeeded. Controller has " << role << " access to the server" << std::endl;
}

void P4RuntimeClient::SetElectionId(::P4_NAMESPACE_ID::Uint128* election_id) {
  electionId_ = election_id;
  electionId_->set_high(election_id->high());
  electionId_->set_low(election_id->low());
}

void P4RuntimeClient::SetElectionId(std::string election_id) {
  try {
    auto election_id_index = election_id.find_first_of(",");
    ::PROTOBUF_NAMESPACE_ID::uint64 election_id_high = std::stoull(election_id.substr(0, election_id_index));
    ::PROTOBUF_NAMESPACE_ID::uint64 election_id_low = std::stoull(election_id.substr(election_id_index + 1));
    ::P4_NAMESPACE_ID::Uint128* election_id = ::P4_NAMESPACE_ID::Uint128().New();
    election_id->set_high(election_id_high);
    election_id->set_low(election_id_low);
    SetElectionId(election_id);
  } catch (...) {
    const std::string error_message = "Cannot parse electionId with value = " + election_id;
    HandleException(error_message.c_str());
  }
}

void P4RuntimeClient::PrintP4Info(::P4_CONFIG_NAMESPACE_ID::P4Info p4info) {
  print_p4info(p4info);
}

::PROTOBUF_NAMESPACE_ID::uint32 P4RuntimeClient::GetP4TableIdFromName(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info, std::string table_name) {
  return get_p4_table_id_from_name(p4info, table_name);
}

::PROTOBUF_NAMESPACE_ID::uint32 P4RuntimeClient::GetP4ActionIdFromName(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info, std::string action_name) {
  return get_p4_action_id_from_name(p4info, action_name);
}

::PROTOBUF_NAMESPACE_ID::uint32 P4RuntimeClient::GetP4IndirectCounterIdFromName(
  ::P4_CONFIG_NAMESPACE_ID::P4Info p4info, std::string counter_name) {
  return get_p4_indirect_counter_id_from_name(p4info, counter_name);
}

std::list<::PROTOBUF_NAMESPACE_ID::uint32> P4RuntimeClient::GetP4IndirectCounterIds(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info) {
  return get_p4_indirect_counter_ids(p4info);
}

void P4RuntimeClient::HandleException(const char* error_message) {
  std::cerr << "Exception: " << error_message << std::endl;
  throw error_message;
}

void P4RuntimeClient::HandleStatus(Status status, const char* error_message) {
  if (!status.ok()) {
    std::cerr << error_message << ". Error code: " << status.error_code() << std::endl;
  }
}

#include "../common/ns_undef.inc"

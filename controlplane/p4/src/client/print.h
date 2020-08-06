#include <grpcpp/grpcpp.h>


void print_p4info(::P4_CONFIG_NAMESPACE_ID::P4Info p4info) {
  std::cout << "Printing P4Info\n" << std::endl;
  std::cout << "Arch: " << p4info.pkg_info().arch() << std::endl;
  int table_size = p4info.tables_size();
  std::cout << "Number of tables: " << table_size << std::endl;
  for (::P4_CONFIG_NAMESPACE_ID::Table table : p4info.tables()) {
    std::cout << "  Table id: " << table.preamble().id() << std::endl;
    std::cout << "  Table name: " << table.preamble().name() << std::endl;
    for (::P4_CONFIG_NAMESPACE_ID::MatchField match_field : table.match_fields()) {
      std::cout << "    Match id: " << match_field.id() << std::endl;
      std::cout << "    Match name: " << match_field.name() << std::endl;
      std::cout << "    Match bitwidth: " << match_field.bitwidth() << std::endl;
      std::cout << "    Match type: " << match_field.match_type() << std::endl;
    }
    for (::P4_CONFIG_NAMESPACE_ID::ActionRef action_ref : table.action_refs()) {
      for (::P4_CONFIG_NAMESPACE_ID::Action action : p4info.actions()) {
        if (action_ref.id() == action.preamble().id()) {
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

  std::cout << "Number of direct counters: " << p4info.direct_counters_size() << std::endl;
  for (::P4_CONFIG_NAMESPACE_ID::DirectCounter counter : p4info.direct_counters()) {
    std::cout << "  Counter id: " << counter.preamble().id() << std::endl;
    std::cout << "  Counter table id: " << counter.direct_table_id() << std::endl;
  }

  std::cout << "Number of indirect counters: " << p4info.counters_size() << std::endl;
  for (::P4_CONFIG_NAMESPACE_ID::Counter counter : p4info.counters()) {
    std::cout << "  Counter id: " << counter.preamble().id() << std::endl;
  }
}

void print_response_type(::P4_NAMESPACE_ID::StreamMessageResponse* response) {
    int response_type = response->update_case();
    switch (response_type) {
    case ::P4_NAMESPACE_ID::StreamMessageRequest::kArbitration: {
      std::cout << "CheckResponseType. Is arbitration" << std::endl;
      std::cout << "CheckResponseType. Arbitration message = " << response->arbitration().SerializeAsString() << std::endl;
      std::cout << "CheckResponseType. Device_id = " << response->arbitration().device_id() << std::endl;
      std::cout << "CheckResponseType. Election_id - high = " << response->arbitration().election_id().high() << std::endl;
      std::cout << "CheckResponseType. Election_id - low = " << response->arbitration().election_id().low() << std::endl;
      break;
    }
    case ::P4_NAMESPACE_ID::StreamMessageRequest::kPacket: {
      std::cout << "CheckResponseType. Is a packet" << std::endl;
      break;
    }
    case ::P4_NAMESPACE_ID::StreamMessageRequest::kDigestAck:
    case ::P4_NAMESPACE_ID::StreamMessageRequest::UPDATE_NOT_SET:
      std::cout << "CheckResponseType. Is a digest" << std::endl;
      break;
  }

  // void print_to_log(std::ofstream log_file, std::string content, bool is_error) {
  //   const auto current_time = std::chrono::system_clock::now();
  //   std::stringstream timestamp;
  //   timestamp << "[" << std::chrono::duration_cast<std::chrono::seconds>(
  //     current_time.time_since_epoch()).count() << "]";
  //   content = timestamp.str() + "]" + content;
  //   if (is_error) {
  //     content = "[ERROR] " + content;
  //   }
  //   log_file << content;
  // }

  // void print_to_log_and_std(std::ofstream log_file, std::string content, bool is_error) {
  //   print_to_log(log_file, content, is_error);
  //   if (is_error) {
  //     std::cerr << content << std::endl;
  //   } else {
  //     std::cout << content << std::endl;
  //   }
  // }
}
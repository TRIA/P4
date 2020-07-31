#include <grpcpp/grpcpp.h>

// #include <unistd.h>
#include "client.h"
// Import declarations after any other
#include "../common/ns_def.inc"

using ::GRPC_NAMESPACE_ID::Status;
using ::P4_CONFIG_NAMESPACE_ID::P4Info;

int is_substring_of(std::string substring, std::string string) {
  size_t position_start = string.find(substring);
  return (position_start != std::string::npos) ? position_start : -1;
}

std::string parse_arguments(int num_args, char** args, std::string arg_name, std::string default_value) {
  std::string arg_val;

  while (num_args > 1) {
    arg_val = args[num_args-1];
    int position_start = is_substring_of(arg_name, arg_val);
    if (position_start >= 0) {
      position_start += arg_name.size();
      if (arg_val[position_start] == '=') {
        arg_val = arg_val.substr(position_start + 1);
        return arg_val;
      } else {
        std::cout << "Argument syntax: " << arg_name << "=<value>" << std::endl;
        return default_value;
      }
    } else {
      arg_val = "";
    }
    num_args--;
  }

  return arg_val.length() == 0 ? default_value : arg_val;
}

void segfault_sigaction(int signal, siginfo_t *si, void *arg) {
    printf("Caught segfault at address=%p\n", si->si_addr);
    exit(1);
}

void setup_segfault() {
  int *foo = NULL;
  struct sigaction sa;

  memset(&sa, 0, sizeof(struct sigaction));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = segfault_sigaction;
  sa.sa_flags   = SA_SIGINFO;

  sigaction(SIGSEGV, &sa, NULL);

  // Cause a seg fault
  *foo = 1;
}

void handle_status(Status status) {
  if (!status.ok()) {
    std::cerr << "Warning: obtained error=" << status.error_code() << std::endl;
    // TEST AND REMOVE
    std::cout << "status.error_details: " << static_cast<std::string>(status.error_details()) << std::endl;
    std::cout << "status.error_message: " << static_cast<std::string>(status.error_message()) << std::endl;
    // ---
  } else {
    std::cout << "Success: obtained status=" << status.error_code() << std::endl;
  }
}

int main(int argc, char** argv) {

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  // INFO: uncomment to hide memory stack trace after p4RuntimeClient.SetFwdPipeConfig() issue
  // setup_segfault();

  std::cout << "\n** Running the client for the EDF control plane **" << std::endl;

  // Parse arguments given to the P4Runtime client
  // TODO: consider implemeting another argument to run a specific RPC method
  const std::string grpc_server_addr = parse_arguments(argc, argv, "--grpc-addr", "localhost:50001");
  const std::string config_paths = parse_arguments(argc, argv, "--config", "../cfg/p4info.txt,../cfg/bmv2.json");
  const std::string election_id = parse_arguments(argc, argv, "--election-id", "0,1");
  const ::PROTOBUF_NAMESPACE_ID::uint64 deviceId = 1;
  std::cout << "\nP4RuntimeClient running with arguments:" << std::endl;
  std::cout << "\t--grpc-addr=" << grpc_server_addr << std::endl;
  std::cout << "\t--config=" << config_paths << std::endl;
  std::cout << "\t--election-id=" << election_id << "\n" << std::endl;

  // Instantiate the client. It uses a non-authenticated channel, which models a connection to an endpoint
  // (as specified by the "--target" argument). The actual RPCs are created out of this channel.
  P4RuntimeClient p4RuntimeClient(grpc_server_addr, config_paths, deviceId, election_id);
  Status status;

  std::cout << "\n-------------- GetP4Info (before pushing pipeline) --------------" << std::endl;
  P4Info p4Info = p4RuntimeClient.GetP4Info();
  if (p4Info.tables_size() == 0) {
    std::cout << "Warning: forwarding pipeline configuration should be pushed before calling this" << std::endl;
  }

  std::cout << "\n-------------- SetFwdPipeConfig --------------" << std::endl;
  if (p4Info.tables_size() == 0) {
    status = p4RuntimeClient.SetFwdPipeConfig();
    handle_status(status);
  } else {
    std::cout << "Warning: forwarding pipeline configuration already pushed" << std::endl;
  }

  std::cout << "\n-------------- GetP4Info (after pushing pipeline) --------------" << std::endl;
  p4Info = p4RuntimeClient.GetP4Info();

  std::cout << "\n-------------- InsertEntry --------------" << std::endl;
  std::list<P4TableEntry *> entries;
  P4TableEntry entry;
  P4Match match;
  P4Parameter param;

  // Step 1: insert a new entry with action "NoAction" (not the default action;
  // otherwise table would no longer supporting it)
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "NoAction");
  entry.action.default_action = false;
  entry.timeout_ns = 0;
  entries.push_back(&entry);
  status = p4RuntimeClient.InsertEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  // Step 2: insert a new entry with action "MyIngress.ipv4_forward"
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "MyIngress.ipv4_forward");
  entry.action.default_action = false;
  entry.timeout_ns = 0;
  match.field_id = 1;
  match.type = P4MatchType::lpm;
  // 32 bits / 4 bytes
  // IP value = "10.0.0.2" to integer
  // TODO: revisit proper value
  // match.value = 167772162;
  match.value = 2;
  match.bitwidth = 32;
  match.lpm_prefix = 32;
  entry.matches.push_back(match);
  param.id = 1;
  // 48 bits / 6 bytes
  // MAC value = "00:00:00:00:00:02" to integer
  param.value = 2;
  param.bitwidth = 48;
  entry.action.parameters.push_back(param);
  param.id = 2;
  // 9 bits / 2 bytes
  param.value = 2;
  param.bitwidth = 9;
  entry.action.parameters.push_back(param);
  entries.push_back(&entry);
  status = p4RuntimeClient.InsertEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  // Step 3: insert a new entry with action "MyIngress.efcp_forward"
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.efcp_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "MyIngress.efcp_forward");
  entry.action.default_action = false;
  entry.timeout_ns = 0;
  match.field_id = 1;
  match.type = P4MatchType::exact;
  // 16 bits / 2 bytes
  match.value = 2;
  match.bitwidth = 16;
  entry.matches.push_back(match);
  param.id = 1;
  param.value = 0;
  param.bitwidth = 12;
  entry.action.parameters.push_back(param);
  // 48 bits / 6 bytes
  // MAC value = "00:00:00:00:00:02" to integer
  param.id = 2;
  param.value = 2;
  param.bitwidth = 48;
  entry.action.parameters.push_back(param);
  param.id = 3;
  // 9 bits / 2 bytes
  param.value = 2;
  param.bitwidth = 9;
  entry.action.parameters.push_back(param);
  entries.push_back(&entry);
  status = p4RuntimeClient.InsertEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  std::cout << "\n-------------- ModifyEntry --------------" << std::endl;
  // Update entry in ipv4_lpm with a given match, just changing 1st param to MAC for h1
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "MyIngress.ipv4_forward");
  entry.action.default_action = false;
  match.field_id = 1;
  match.type = P4MatchType::lpm;
  // Match value cannot be changed (search process matches against it)
  match.value = 2;
  match.bitwidth = 32;
  match.lpm_prefix = 32;
  entry.matches.push_back(match);
  param.id = 1;
  // 48 bits / 6 bytes
  // MAC value = "00:00:00:00:00:01" to integer
  param.value = 1;
  param.bitwidth = 48;
  entry.action.parameters.push_back(param);
  param.id = 2;
  // 9 bits / 2 bytes
  param.value = 1;
  param.bitwidth = 9;
  entry.action.parameters.push_back(param);
  entries.push_back(&entry);
  status = p4RuntimeClient.ModifyEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  std::cout << "\n-------------- ReadEntry --------------" << std::endl;
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "MyIngress.ipv4_forward");
  entries.push_back(&entry);
  std::list<P4TableEntry *> result = p4RuntimeClient.ReadEntry(entries);
  if (result.size() > 0) {
    std::cout << "Success: retrieved entry for table id = " << result.front()->table_id << std::endl;
  } else {
    std::cerr << "Warning: no entry retrieved" << std::endl;
  }
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  std::cout << "\n-------------- DeleteEntry --------------" << std::endl;
  // Step 1: delete entry with action "NoAction"
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "NoAction");
  entry.action.default_action = false;
  entry.timeout_ns = 0;
  entries.push_back(&entry);
  status = p4RuntimeClient.DeleteEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  // Step 2: delete entry with action "MyIngress.ipv4_forward"
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "MyIngress.ipv4_forward");
  entry.action.default_action = false;
  entry.timeout_ns = 0;
  match.field_id = 1;
  match.type = P4MatchType::lpm;
  match.value = 2;
  match.bitwidth = 32;
  entry.matches.push_back(match);
  entries.push_back(&entry);
  status = p4RuntimeClient.DeleteEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  // Step 3: delete entry with action "MyIngress.efcp_forward"
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.efcp_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "MyIngress.efcp_forward");
  entry.action.default_action = false;
  entry.timeout_ns = 0;
  match.field_id = 1;
  match.type = P4MatchType::exact;
  match.value = 2;
  match.bitwidth = 16;
  entry.matches.push_back(match);
  entries.push_back(&entry);
  status = p4RuntimeClient.DeleteEntry(entries);
  handle_status(status);
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  std::cout << "\n-------------- ReadEntry --------------" << std::endl;
  entry.table_id = p4RuntimeClient.GetP4TableIdFromName(p4Info, "MyIngress.ipv4_lpm");
  entry.action.action_id = p4RuntimeClient.GetP4ActionIdFromName(p4Info, "NoAction");
  entries.push_back(&entry);
  result = p4RuntimeClient.ReadEntry(entries);
  if (result.size() > 0) {
    std::cout << "Success: retrieved entry for table id = " << result.front()->table_id << std::endl;
  } else {
    std::cerr << "Warning: no entry retrieved" << std::endl;
  }
  entries.clear();
  entry.matches.clear();
  entry.action.parameters.clear();

  std::cout << "\n-------------- APIVersion --------------" << std::endl;
  std::string version = p4RuntimeClient.APIVersion();
  if (version.length() > 0) {
    std::cout << "Success: obtained API version=" << version << std::endl;
  } else {
    std::cerr << "Warning: no API version obtained" << std::endl;
  }

  p4RuntimeClient.TearDown();

  return 0;
}
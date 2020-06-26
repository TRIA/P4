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
  P4RuntimeClient p4RuntimeClient = P4RuntimeClient(grpc_server_addr, config_paths, deviceId, election_id);
  Status status;

  std::cout << "-------------- SetFwdPipeConfig --------------" << std::endl;
  status = p4RuntimeClient.SetFwdPipeConfig();
  handle_status(status);

  std::cout << "-------------- GetP4Info --------------" << std::endl;
  P4Info p4Info = p4RuntimeClient.GetP4Info();
  if (p4Info.ByteSizeLong() > 0) {
    std::cout << "Number of tables: " << p4Info.tables_size() << std::endl;
    std::cout << "Number of action profiles: " << p4Info.action_profiles_size() << std::endl;
    std::cout << "Number of actions: " << p4Info.actions_size() << std::endl;
  }

  std::cout << "-------------- Write --------------" << std::endl;
  ::P4_NAMESPACE_ID::WriteRequest writeRequest = ::P4_NAMESPACE_ID::WriteRequest();
  status = p4RuntimeClient.Write(&writeRequest);
  handle_status(status);

  std::cout << "-------------- WriteUpdate --------------" << std::endl;
  writeRequest = ::P4_NAMESPACE_ID::WriteRequest();
  status = p4RuntimeClient.WriteUpdate(&writeRequest);
  handle_status(status);

  std::cout << "-------------- ReadOne --------------" << std::endl;
  ::P4_NAMESPACE_ID::ReadResponse response = p4RuntimeClient.ReadOne();
  std:: string responseData = response.SerializeAsString();
  if (responseData.size() > 0) {
    std::cout << "Success: obtained response data=" << responseData << std::endl;
  } else {
    std::cerr << "Warning: no response data obtained" << std::endl;
  }

  std::cout << "-------------- APIVersion --------------" << std::endl;
  std::string version = p4RuntimeClient.APIVersion();
  if (version.length() > 0) {
    std::cout << "Success: obtained API version=" << version << std::endl;
  } else {
    std::cerr << "Warning: no API version obtained" << std::endl;
  }

  p4RuntimeClient.TearDown();

  return 0;
}

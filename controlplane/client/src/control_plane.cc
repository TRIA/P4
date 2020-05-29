#include <grpcpp/grpcpp.h>
#include "p4runtime_client.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

using ::GRPC_NAMESPACE_ID::CreateChannel;
using ::P4_CONFIG_NAMESPACE_ID::P4Info;

std::string* parseArguments(char** argv, std::string argName) {
  std::string argValue = "";
  std::string argVal = argv[1];

  size_t startPos = argVal.find(argName);
  if (startPos != std::string::npos) {
    startPos += argName.size();
    if (argVal[startPos] == '=') {
      argValue = argVal.substr(startPos + 1);
    } else {
      std::cout << "The correct argument syntax is " << argName << "=" << std::endl;
      return 0;
    }
  } else {
    std::cout << "The acceptable argument is " << argName << "=" << std::endl;
    return 0;
  }
  return &argValue;
}

int main(int argc, char** argv) {

  // Instantiate the client. It uses a non-authenticated channel, which models a connection to an endpoint
  // (as specified by the "--target" argument). The actual RPCs are created out of this channel.
  std::string targetAddress;
  std::string configPaths;
  std::string electionId;

  if (argc > 1) {
    targetAddress = *parseArguments(argv, "--target");
    configPaths = *parseArguments(argv, "--config");
    electionId = *parseArguments(argv, "--election-id");
  } else {
    targetAddress = "localhost:50051";
    configPaths = "p4src/build/p4info.txt,p4src/build/bmv2.json";
    electionId = "0,1";
  }

  const ::PROTOBUF_NAMESPACE_ID::uint64 deviceId = 1;
  P4RuntimeClient p4RuntimeClient(targetAddress, configPaths, deviceId, electionId);

  try {
    P4Info p4Info = p4RuntimeClient.GetP4Info();
  } catch (...) {
    const std::string errorMessage = "Cannot get the configuration from ForwardingPipelineConfig";
    std::cerr << errorMessage << std::endl;
    throw errorMessage;
  }

  p4RuntimeClient.TearDown();

  return 0;
}
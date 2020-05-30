#include <grpcpp/grpcpp.h>
#include "p4runtime_client.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

using ::P4_CONFIG_NAMESPACE_ID::P4Info;

int isSubstringOf(std::string subString, std::string string) {
  size_t startPosition = string.find(subString);
  return (startPosition != std::string::npos) ? startPosition : -1;
}

std::string parseArguments(int numArgs, char** args, std::string argName, std::string defaultValue) {
  std::string argValue;
  std::string argVal;

  while (numArgs > 1) {
    argVal = args[numArgs-1];
    int startPosition = isSubstringOf(argName, argVal);
    if (startPosition >= 0) {
      startPosition += argName.size();
      if (argVal[startPosition] == '=') {
        argVal = argVal.substr(startPosition + 1);
        return argVal;
      } else {
        std::cout << "Argument syntax: " << argName << "=<value>" << std::endl;
        return defaultValue;
      }
    } else {
      argVal = "";
    }
    numArgs--;
  }

  return argVal == "" ? defaultValue : argVal;
}

int main(int argc, char** argv) {

  std::cout << "\n** Running the client for the EDF control plane **\n\n" << std::endl;

  // Parse arguments given to the P4Runtime client
  const std::string targetAddress = parseArguments(argc, argv, "--target", "localhost:50051");
  const std::string configPaths = parseArguments(argc, argv, "--config", "p4src/build/p4info.txt,p4src/build/bmv2.json");
  const std::string electionId = parseArguments(argc, argv, "--election-id", "0,1");
  const ::PROTOBUF_NAMESPACE_ID::uint64 deviceId = 1;
  std::cout << "P4RuntimeClient running with arguments:" << std::endl;
  std::cout << "\t--target=" << targetAddress << std::endl;
  std::cout << "\t--config=" << configPaths << std::endl;
  std::cout << "\t--election-id=" << electionId << std::endl;

  // Instantiate the client. It uses a non-authenticated channel, which models a connection to an endpoint
  // (as specified by the "--target" argument). The actual RPCs are created out of this channel.
  P4RuntimeClient p4RuntimeClient = P4RuntimeClient(targetAddress, configPaths, deviceId, electionId);

  // TODO: consider implemeting another argument to run a specific RPC method
  try {
    P4Info p4Info = p4RuntimeClient.GetP4Info();
    std::cout << "Obtained P4Info data: " << p4Info.SerializeAsString() << std::endl;
  } catch (...) {
    const std::string errorMessage = "Cannot get the configuration from ForwardingPipelineConfig";
    std::cerr << errorMessage << std::endl;
    throw errorMessage;
  }

  p4RuntimeClient.SetFwdPipeConfig();

  p4RuntimeClient.TearDown();

  return 0;
}

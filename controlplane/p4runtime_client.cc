#include <fcntl.h>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sstream>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "p4runtime_client.h"
// Import declarations after any other
#include "p4runtime_ns_def.inc"

using ::GRPC_NAMESPACE_ID::Channel;
using ::GRPC_NAMESPACE_ID::ClientContext;
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

  auto electionIdIndex = electionId.find_first_of(",");
  ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
  ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
  electionId_->set_high(electionIdHigh);
  electionId_->set_low(electionIdLow);

  ::GRPC_NAMESPACE_ID::ClientContext* context;
  std::unique_ptr<streamType_> stream_ = stub_->StreamChannel(context);
}

// RPC methods

// @parse_p4runtime_error
// def get_p4info(self):
//     logging.debug("Retrieving P4Info file")
//     req = p4runtime_pb2.GetForwardingPipelineConfigRequest()
//     req.device_id = self.device_id
//     req.response_type = p4runtime_pb2.GetForwardingPipelineConfigRequest.P4INFO_AND_COOKIE
//     rep = self.stub.GetForwardingPipelineConfig(req)
//     return rep.config.p4info
P4Info P4RuntimeClient::GetP4Info() {
  std::cout << "Retrieving P4 info object" << std::endl;
  GetForwardingPipelineConfigRequest request = GetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  request.set_response_type(request.P4INFO_AND_COOKIE);

  ClientContext context;
  GetForwardingPipelineConfigResponse response;
  try {
    stub_->GetForwardingPipelineConfig(&context, request, &response);
  } catch (...) {
    const std::string errorMessage = "Cannot get the configuration from ForwardingPipelineConfig";
    HandleException(errorMessage.c_str());
  }
  return response.config().p4info();
}

// @parse_p4runtime_error
// def set_fwd_pipe_config(self, p4info_path, bin_path):
//     logging.debug("Setting forwarding pipeline config")
//     req = p4runtime_pb2.SetForwardingPipelineConfigRequest()
//     req.device_id = self.device_id
//     election_id = req.election_id
//     election_id.high = self.election_id[0]
//     election_id.low = self.election_id[1]
//     req.action = p4runtime_pb2.SetForwardingPipelineConfigRequest.VERIFY_AND_COMMIT
//     with open(p4info_path, 'r') as f1:
//         with open(bin_path, 'rb') as f2:
//             try:
//                 google.protobuf.text_format.Merge(f1.read(), req.config.p4info)
//             except google.protobuf.text_format.ParseError:
//                 logging.error("Error when parsing P4Info")
//                 raise
//             req.config.p4_device_config = f2.read()
//     return self.stub.SetForwardingPipelineConfig(req)
Status P4RuntimeClient::SetFwdPipeConfig() {
  std::cout << "Setting forwarding pipeline config" << std::endl;
  SetForwardingPipelineConfigRequest request = SetForwardingPipelineConfigRequest();
  request.set_device_id(deviceId_);
  SetElectionId(request.mutable_election_id());
  request.set_action(SetForwardingPipelineConfigRequest().VERIFY_AND_COMMIT);

  int fd = open(p4InfoPath_.c_str(), O_RDONLY);
  ZeroCopyInputStream* p4InfoFile = new FileInputStream(fd);

  std::ifstream binaryCfgFile(binaryCfgPath_);
  ZeroCopyInputStream* binaryCfgFile = new IstreamInputStream(&binaryCfgFile, -1);
  std::stringstream binaryCfgFileStr;
  binaryCfgFileStr << binaryCfgFile.rdbuf();

  // Data (1st arg) merged into given Message (2nd arg)
  ::PROTOBUF_NAMESPACE_ID::TextFormat::Merge(p4InfoFile, &request);

  ForwardingPipelineConfig* config = request.mutable_config();
  config->set_allocated_p4_device_config(&binaryCfgFileStr.str());

  ClientContext* context;
  ::P4_NAMESPACE_ID::SetForwardingPipelineConfigResponse* response;

  return stub_->SetForwardingPipelineConfig(context, request, response);
}

// @parse_p4runtime_write_error
// def write(self, req):
//     req.device_id = self.device_id
//     election_id = req.election_id
//     election_id.high = self.election_id[0]
//     election_id.low = self.election_id[1]
//     return self.stub.Write(req)
Status P4RuntimeClient::Write(::P4_NAMESPACE_ID::WriteRequest* request) {
  std::cout << "Submitting writing request" << std::endl;
  request->set_device_id(deviceId_);
  deviceId_ = request->device_id();
  SetElectionId(request->mutable_election_id());
  WriteResponse response;
  ClientContext context;
  return stub_->Write(&context, *request, &response);
}

// @parse_p4runtime_write_error
// def write_update(self, update):
//     req = p4runtime_pb2.WriteRequest()
//     req.device_id = self.device_id
//     election_id = req.election_id
//     election_id.high = self.election_id[0]
//     election_id.low = self.election_id[1]
//     req.updates.extend([update])
//     return self.stub.Write(req)
Status P4RuntimeClient::WriteUpdate(WriteRequest* update) {
  std::cout << "Submitting write-update request" << std::endl;
  WriteRequest request = WriteRequest();
  request.set_device_id(deviceId_);
  SetElectionId(request.mutable_election_id());
  // Extend request.updates with the new update object provided
  for (Update singleUpdate : *(update->mutable_updates())) {
    // Note: add already allocated objects in memory. Might use "Add" isntead
    request.mutable_updates()->AddAllocated(&singleUpdate);
  }
  WriteResponse response;
  ClientContext context;
  return stub_->Write(&context, request, &response);
}

// # Decorator is useless here: in case of server error, the exception is raised during the
// # iteration (when next() is called).
// @parse_p4runtime_error
// def read_one(self, entity):
//     req = p4runtime_pb2.ReadRequest()
//     req.device_id = self.device_id
//     req.entities.extend([entity])
//     return self.stub.Read(req)
ReadResponse P4RuntimeClient::ReadOne() {
  std::cout << "Submitting single read request" << std::endl;
  ReadRequest request = ReadRequest();
  request.set_device_id(deviceId_);
  for (Entity singleEntity : *request.mutable_entities()) {
    // Note: add already allocated objects in memory. Might use "Add" isntead
    request.mutable_entities()->AddAllocated(&singleEntity);
  }
  ClientContext context;
  ReadResponse response = ReadResponse();
  stub_->Read(&context, request).get()->Read(&response);
  return response;
}

// @parse_p4runtime_error
// def api_version(self):
//     req = p4runtime_pb2.CapabilitiesRequest()
//     rep = self.stub.Capabilities(req)
//     return rep.p4runtime_api_version
std::string P4RuntimeClient::APIVersion() {
  std::cout << "Fetching version of the API" << std::endl;
  CapabilitiesRequest request = CapabilitiesRequest();
  CapabilitiesResponse response = CapabilitiesResponse();
  ClientContext context;
  try {
    stub_->Capabilities(&context, request, &response);
  } catch (...) {
    const std::string errorMessage = "Cannot retrieve information on the API version";
    HandleException(errorMessage.c_str());
  }
  return response.p4runtime_api_version();
}

// Ancillary methods

// TODO: finalise

// def set_up_stream(self):
//     self.stream_out_q = queue.Queue()
//     self.stream_in_q = queue.Queue()

//     def stream_req_iterator():
//         while True:
//             p = self.stream_out_q.get()
//             if p is None:
//                 break
//             yield p

//     def stream_recv_wrapper(stream):
//         @parse_p4runtime_error
//         def stream_recv():
//             for p in stream:
//                 self.stream_in_q.put(p)
//         try:
//             stream_recv()
//         except P4RuntimeException as e:
//             logging.critical("StreamChannel error, closing stream")
//             logging.critical(e)
//             self.stream_in_q.put(None)

//     self.stream = self.stub.StreamChannel(stream_req_iterator())
//     self.stream_recv_thread = threading.Thread(
//         target=stream_recv_wrapper, args=(self.stream,))
//     self.stream_recv_thread.start()

//     self.handshake()
void P4RuntimeClient::SetUp() {
  // Setup queues
  std::queue<streamType_> streamQueueIn_();
  std::queue<streamType_> streamQueueOut_();
  grpc::ClientContext* context;
  stream_ = stub_->StreamChannel(context);
  Handshake();
}

// TODO: finalise

// def tear_down(self):
//     if self.stream_out_q:
//         logging.debug("Cleaning up stream")
//         self.stream_out_q.put(None)
//         self.stream_recv_thread.join()
//     self.channel.close()
//     del self.channel  # avoid a race condition if channel deleted when process terminates
void P4RuntimeClient::TearDown() {
  if (!streamQueueOut_.empty()) {
    std::cout << "Cleaning up stream" << std::endl;
    streamQueueOut_.push(NULL);
  }
  channel_->~Channel();
}

// TODO: finalise

// def handshake(self):
//     req = p4runtime_pb2.StreamMessageRequest()
//     arbitration = req.arbitration
//     arbitration.device_id = self.device_id
//     election_id = arbitration.election_id
//     election_id.high = self.election_id[0]
//     election_id.low = self.election_id[1]
//     self.stream_out_q.put(req)

//     rep = self.get_stream_packet("arbitration", timeout=2)
//     if rep is None:
//         logging.critical("Failed to establish session with server")
//         sys.exit(1)
//     is_master = (rep.arbitration.status.code == code_pb2.OK)
//     logging.debug("Session established, client is '{}'".format(
//         'master' if is_master else 'slave'))
//     if not is_master:
//         print("You are not master, you only have read access to the server")
void P4RuntimeClient::Handshake() {
  StreamMessageRequest request = StreamMessageRequest();
  const ::P4_NAMESPACE_ID::MasterArbitrationUpdate arbitration = request.arbitration();
  SetElectionId(const_cast<::P4_NAMESPACE_ID::Uint128*>(&arbitration.election_id()));
  streamQueueOut_.push(&request);
}

void P4RuntimeClient::SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId) {
  electionId_ = electionId;
  electionId_->set_high(electionId_->high());
  electionId_->set_low(electionId_->low());
}

void P4RuntimeClient::SetElectionId(std::string electionId) {
  try {
    auto electionIdIndex = electionId.find_first_of(",");
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdHigh = std::stoull(electionId.substr(0, electionIdIndex));
    ::PROTOBUF_NAMESPACE_ID::uint64 electionIdLow = std::stoull(electionId.substr(electionIdIndex + 1));
    electionId_->set_high(electionIdHigh);
    electionId_->set_low(electionIdLow);
  } catch (...) {
    const std::string errorMessage = "Cannot parse electionId with value = " + electionId;
    HandleException(errorMessage.c_str());
  }
}

void P4RuntimeClient::HandleException(const char* errorMessage) {
  std::cerr << errorMessage << std::endl;
  throw errorMessage;
}

#include "p4runtime_ns_undef.inc"
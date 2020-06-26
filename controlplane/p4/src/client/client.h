#ifndef P4RUNTIME_CLIENT_H
#define P4RUNTIME_CLIENT_H

#include <queue>
#include <thread>

#include "../common/grpc_out/p4/config/v1/p4info.pb.h"
#include "../common/grpc_out/p4/v1/p4runtime.grpc.pb.h"
#include "../common/grpc_out/p4/v1/p4runtime.pb.h"

// Import declarations after any other
#include "../common/ns_def.inc"

class P4RuntimeClient {

  public:

    P4RuntimeClient(std::string bindAddress, 
                    std::string config,
                    ::PROTOBUF_NAMESPACE_ID::uint64 deviceId,
                    std::string electionId);

    // RPC methods
    ::GRPC_NAMESPACE_ID::Status SetFwdPipeConfig();
    ::P4_CONFIG_NAMESPACE_ID::P4Info GetP4Info();
    ::GRPC_NAMESPACE_ID::Status Write(::P4_NAMESPACE_ID::WriteRequest* request);
    ::GRPC_NAMESPACE_ID::Status WriteUpdate(::P4_NAMESPACE_ID::WriteRequest* update);
    ::P4_NAMESPACE_ID::ReadResponse ReadOne();
    std::string APIVersion();

    // Ancillary methods
    void SetUpStream();
    void TearDown();

  private:

    // Client definition
    std::shared_ptr<::GRPC_NAMESPACE_ID::Channel> channel_;
    std::unique_ptr<::P4_NAMESPACE_ID::P4Runtime::Stub> stub_;
    std::string p4InfoPath_;
    std::string binaryCfgPath_;
    ::PROTOBUF_NAMESPACE_ID::uint64 deviceId_;
    ::P4_NAMESPACE_ID::Uint128* electionId_;

    // Ancillary for RPC
    ::GRPC_NAMESPACE_ID::Status WriteInternal(::P4_NAMESPACE_ID::WriteRequest* request);

    // Ancillary
    typedef ::GRPC_NAMESPACE_ID::ClientReaderWriter<
      ::P4_NAMESPACE_ID::StreamMessageRequest, ::P4_NAMESPACE_ID::StreamMessageResponse> streamType_;
    typedef ::GRPC_NAMESPACE_ID::ClientAsyncReaderWriter<
      ::P4_NAMESPACE_ID::StreamMessageRequest, ::P4_NAMESPACE_ID::StreamMessageResponse> streamAsyncType_;

    std::deque<::P4_NAMESPACE_ID::StreamMessageResponse*> streamQueueIn_;
    std::deque<::P4_NAMESPACE_ID::StreamMessageRequest*> streamQueueOut_;
    std::unique_ptr<streamType_> stream_;
    std::unique_ptr<streamAsyncType_> streamAsync_;
    std::thread streamIncomingThread_;
    std::thread streamOutgoingThread_;

    void Handshake();
    void SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId);
    void SetElectionId(std::string electionId);
    ::P4_NAMESPACE_ID::StreamMessageResponse* GetStreamPacket(int expectedType, long timeout);
    void CheckResponseType(::P4_NAMESPACE_ID::StreamMessageResponse* response);
    void ReadOutgoingMessagesFromQueue();
    void ReadOutgoingMessagesFromQueueInBg();
    void ReadIncomingMessagesFromStream();
    void ReadIncomingMessagesFromStreamInBg();
    void HandleException(const char* errorMessage);
    void HandleStatus(::GRPC_NAMESPACE_ID::Status status, const char* errorMessage);

};

#include "../common/ns_undef.inc"
#endif

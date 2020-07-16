#ifndef P4RUNTIME_CLIENT_H
#define P4RUNTIME_CLIENT_H

#include <mutex>
#include <queue>
#include <math.h>
#include <thread>

#include "../common/grpc_out/p4/config/v1/p4info.pb.h"
#include "../common/grpc_out/p4/v1/p4runtime.grpc.pb.h"
#include "../common/grpc_out/p4/v1/p4runtime.pb.h"

// Import declarations after any other
#include "../common/ns_def.inc"

struct P4Parameter {
  uint32_t id;
  std::string value;
};

struct P4Action {
  uint32_t action_id;
  // Flag to indicate whether the action is the default choice for a table
  bool default_action;
  std::list<P4Parameter> parameters;
};

// Using same IDs as in spec
enum P4MatchType {
  exact = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kExact,
  ternary = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kTernary,
  lpm = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kLpm,
  range = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kRange,
  optional = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kOptional,
  other = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kOther
};

struct P4Match {
  uint32_t field_id;
  P4MatchType type;
  // Note: bitstring with the minimum size to express the value
  std::string value;
  // Only required for LPM
  // ::PROTOBUF_NAMESPACE_ID::int32 lpm_prefix;
  int32_t lpm_prefix;
  // Only required for Ternary
  std::string ternary_mask;
  // Required for Range
  std::string range_low;
  std::string range_high;
};

struct P4TableEntry {
  uint32_t table_id;
  P4Action action;
  std::list<P4Match> matches;
  // Only available for Ternary, Range or Optional matches
  int32_t priority;
  // Only available for non-default Actions. Use nanoseconds
  int64_t timeout_ns;
};

class P4RuntimeClient {

  public:

    P4RuntimeClient(std::string bindAddress, 
                    std::string config,
                    ::PROTOBUF_NAMESPACE_ID::uint64 deviceId,
                    std::string electionId);

    // RPC methods
    ::GRPC_NAMESPACE_ID::Status SetFwdPipeConfig();
    ::P4_CONFIG_NAMESPACE_ID::P4Info GetP4Info();
    ::GRPC_NAMESPACE_ID::Status InsertEntry(std::list<P4TableEntry*> entries);
    ::GRPC_NAMESPACE_ID::Status ModifyEntry(std::list<P4TableEntry*> entries);
    ::GRPC_NAMESPACE_ID::Status DeleteEntry(std::list<P4TableEntry*> entries);
    std::list<P4TableEntry*> ReadEntry(std::list<P4TableEntry*> query);
    std::string APIVersion();

    // Ancillary methods
    void SetUpStream();
    void TearDown();

    // P4-related methods
    ::PROTOBUF_NAMESPACE_ID::uint32 GetP4TableIdFromName(::P4_CONFIG_NAMESPACE_ID::P4Info p4Info_,
        std::string tableName);
    ::PROTOBUF_NAMESPACE_ID::uint32 GetP4ActionIdFromName(::P4_CONFIG_NAMESPACE_ID::P4Info p4Info_,
        std::string actionName);

  private:

    // Client definition
    std::shared_ptr<::GRPC_NAMESPACE_ID::Channel> channel_;
    std::unique_ptr<::P4_NAMESPACE_ID::P4Runtime::Stub> stub_;

    // Parameters
    std::string p4InfoPath_;
    std::string binaryCfgPath_;
    ::PROTOBUF_NAMESPACE_ID::uint64 deviceId_;
    ::P4_NAMESPACE_ID::Uint128* electionId_;

    // Stream (synchronous), queues and threads
    typedef ::GRPC_NAMESPACE_ID::ClientReaderWriter<
      ::P4_NAMESPACE_ID::StreamMessageRequest, ::P4_NAMESPACE_ID::StreamMessageResponse> streamType_;
    std::unique_ptr<streamType_> stream_;
    std::queue<::P4_NAMESPACE_ID::StreamMessageResponse*> streamQueueIn_;
    std::mutex qInMtx_;
    std::queue<::P4_NAMESPACE_ID::StreamMessageRequest*> streamQueueOut_;
    std::mutex qOutMtx_;
    std::thread streamIncomingThread_;
    bool inThreadStop_;
    std::thread streamOutgoingThread_;
    bool outThreadStop_;

    // RPC methods
    ::GRPC_NAMESPACE_ID::Status WriteEntry(std::list<P4TableEntry*> entries,
      ::P4_NAMESPACE_ID::Update::Type updateType);

    // Ancillary methods
    void Handshake();
    void SetElectionId(::P4_NAMESPACE_ID::Uint128* electionId);
    void SetElectionId(std::string electionId);
    void HandleException(const char* errorMessage);
    void HandleStatus(::GRPC_NAMESPACE_ID::Status status, const char* errorMessage);

    ::P4_NAMESPACE_ID::StreamMessageResponse* GetStreamPacket(int expectedType, long timeout);
    void CheckResponseType(::P4_NAMESPACE_ID::StreamMessageResponse* response);
    void ReadOutgoingMessagesFromQueue();
    void ReadOutgoingMessagesFromQueueInBg();
    void ReadIncomingMessagesFromStream();
    void ReadIncomingMessagesFromStreamInBg();

    // P4-info related methods
    void PrintP4Info(::P4_CONFIG_NAMESPACE_ID::P4Info p4Info_);

};

#include "../common/ns_undef.inc"
#endif

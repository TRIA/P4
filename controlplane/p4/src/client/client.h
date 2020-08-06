#ifndef P4RUNTIME_CLIENT_H
#define P4RUNTIME_CLIENT_H

#include <mutex>
#include <queue>
#include <math.h>
#include <thread>
// #include <iostream>
// #include <fstream>

#include "../common/grpc_out/p4/config/v1/p4info.pb.h"
#include "../common/grpc_out/p4/v1/p4runtime.grpc.pb.h"
#include "../common/grpc_out/p4/v1/p4runtime.pb.h"

// Import P4-related declarations after any other
#include "../common/ns_def.inc"

#include "p4_structs.h"
#include "conversions.h"
#include "print.h"


class P4RuntimeClient {

  public:

    P4RuntimeClient(std::string bind_address, 
                    std::string config,
                    ::PROTOBUF_NAMESPACE_ID::uint64 device_id,
                    std::string election_id);

    // RPC methods: configuration
    ::GRPC_NAMESPACE_ID::Status SetFwdPipeConfig();
    ::P4_CONFIG_NAMESPACE_ID::P4Info GetP4Info();
    // RPC methods: tables
    ::GRPC_NAMESPACE_ID::Status InsertTableEntry(
                    std::list<P4TableEntry*> entries);
    ::GRPC_NAMESPACE_ID::Status ModifyTableEntry(
                    std::list<P4TableEntry*> entries);
    ::GRPC_NAMESPACE_ID::Status DeleteTableEntry(
                    std::list<P4TableEntry*> entries);
    std::list<P4TableEntry*> ReadTableEntry(
                    std::list<P4TableEntry*> filter);
    // RPC methods: counters
    std::list<P4DirectCounterEntry*> ReadDirectCounterEntry(
                    std::list<P4DirectCounterEntry*> filter);
    std::list<P4DirectCounterEntry*> ReadDirectCounterEntries();
    std::list<P4CounterEntry*> ReadIndirectCounterEntry(
                    std::list<P4CounterEntry*> filter);
    std::list<P4CounterEntry*> ReadIndirectCounterEntries();
    // RPC methods: others
    std::string APIVersion();

    // Ancillary methods
    void SetUpStream();
    void TearDown();

    // P4-related methods
    ::PROTOBUF_NAMESPACE_ID::uint32 GetP4TableIdFromName(
                    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info,
                    std::string table_name);
    ::PROTOBUF_NAMESPACE_ID::uint32 GetP4ActionIdFromName(
                    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info,
                    std::string action_name);
    ::PROTOBUF_NAMESPACE_ID::uint32 GetP4IndirectCounterIdFromName(
                    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info,
                    std::string counter_name);
    std::list<::PROTOBUF_NAMESPACE_ID::uint32> GetP4IndirectCounterIds(
                    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info);

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
    ::GRPC_NAMESPACE_ID::Status WriteTableEntry(
                    std::list<P4TableEntry*> entries,
                    ::P4_NAMESPACE_ID::Update::Type update_type);

    // Ancillary methods
    void Handshake();
    void SetElectionId(
                    ::P4_NAMESPACE_ID::Uint128* election_id);
    void SetElectionId(
                    std::string election_id);
    void HandleException(
                    const char* error_message);
    void HandleStatus(
                    ::GRPC_NAMESPACE_ID::Status status,
                    const char* error_message);

    ::P4_NAMESPACE_ID::StreamMessageResponse* GetStreamPacket(
                    int expected_type,
                    long timeout);
    void ReadOutgoingMessagesFromQueue();
    void ReadOutgoingMessagesFromQueueInBg();
    void ReadIncomingMessagesFromStream();
    void ReadIncomingMessagesFromStreamInBg();
    std::string EncodeParamValue(
                    uint16_t value,
                    size_t bitwidth);
    uint16_t DecodeParamValue(
                    const std::string str);

    // P4-info related methods
    void PrintP4Info(
                    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info);

    // // Log
    // std::string logFilePath_;
    // std::ofstream logFile_;
    // void PrintToLog(std::string content, bool is_error=false);
    // void PrintToLogAndStd(std::string content, bool is_error=false);
};

#include "../common/ns_undef.inc"
#endif
// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/common/p4_service.h"

#include <functional>
#include <sstream>  // IWYU pragma: keep
#include <utility>

#include "gflags/gflags.h"
#include "google/protobuf/any.pb.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/server_writer_wrapper.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"

DEFINE_string(forwarding_pipeline_configs_file,
              "/var/run/stratum/pipeline_cfg.pb.txt",
              "The latest set of verified ForwardingPipelineConfig protos "
              "pushed to the switch. This file is updated whenever "
              "ForwardingPipelineConfig proto for switching node is added or "
              "modified.");
DEFINE_string(write_req_log_file, "/var/log/stratum/p4_writes.pb.txt",
              "The log file for all the individual write request updates and "
              "the corresponding result. The format for each line is: "
              "<timestamp>;<node_id>;<update proto>;<status>.");
DEFINE_int32(max_num_controllers_per_node, 5,
             "Max number of controllers that can manage a node.");
DEFINE_int32(max_num_controller_connections, 20,
             "Max number of active/inactive streaming connections from outside "
             "controllers (for all of the nodes combined).");

namespace stratum {
namespace hal {

// TODO(unknown): This class move possibly big configs in memory. See if there
// is a way to make this more efficient.

P4Service::P4Service(OperationMode mode, SwitchInterface* switch_interface,
                     AuthPolicyChecker* auth_policy_checker,
                     ErrorBuffer* error_buffer)
    : node_id_to_controllers_(),
      connection_ids_(),
      forwarding_pipeline_configs_(nullptr),
      mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

P4Service::~P4Service() {}

::util::Status P4Service::Setup(bool warmboot) {
  // If we are coupled mode and are coldbooting, we wait for controller to push
  // the forwarding pipeline config. We do not do anything here.
  // TODO(unknown): This will be removed when we completely move to
  // standalone mode.
  if (!warmboot && mode_ == OPERATION_MODE_COUPLED) {
    LOG(INFO) << "Skipped pushing the saved forwarding pipeline config(s) in "
              << "coupled mode when coldbooting.";
    return ::util::OkStatus();
  }

  return PushSavedForwardingPipelineConfigs(warmboot);
}

::util::Status P4Service::Teardown() {
  {
    absl::WriterMutexLock l(&controller_lock_);
    node_id_to_controllers_.clear();
    connection_ids_.clear();
  }
  {
    absl::WriterMutexLock l(&packet_in_thread_lock_);
    // Unregister writers and close PacketIn Channels.
    for (const auto& pair : packet_in_channels_) {
      auto status =
          switch_interface_->UnregisterPacketReceiveWriter(pair.first);
      if (!status.ok()) {
        LOG(ERROR) << status;
      }
      pair.second->Close();
    }
    packet_in_channels_.clear();
    // Join threads.
    for (const auto& tid : packet_in_reader_tids_) {
      int ret = pthread_join(tid, nullptr);
      if (ret) {
        LOG(ERROR) << "Failed to join thread " << tid << " with error " << ret
                   << ".";
      }
    }
  }
  {
    absl::WriterMutexLock l(&config_lock_);
    forwarding_pipeline_configs_ = nullptr;
  }

  return ::util::OkStatus();
}

::util::Status P4Service::PushSavedForwardingPipelineConfigs(bool warmboot) {
  // Try to read the saved forwarding pipeline configs for all the nodes and
  // push them to the nodes.
  LOG(INFO) << "Pushing the saved forwarding pipeline configs read from "
            << FLAGS_forwarding_pipeline_configs_file << "...";
  absl::WriterMutexLock l(&config_lock_);
  ForwardingPipelineConfigs configs;
  ::util::Status status =
      ReadProtoFromTextFile(FLAGS_forwarding_pipeline_configs_file, &configs);
  if (!status.ok()) {
    if (!warmboot && status.error_code() == ERR_FILE_NOT_FOUND) {
      // Not a critical error. If coldboot, we don't even return error.
      LOG(WARNING) << "No saved forwarding pipeline config found at "
                   << FLAGS_forwarding_pipeline_configs_file
                   << ". This is normal when the switch is just installed and "
                   << "no master controller is connected yet.";
      return ::util::OkStatus();
    }
    error_buffer_->AddError(
        status,
        "Could not read the saved forwarding pipeline configs: ", GTL_LOC);
    return status;
  }
  if (configs.node_id_to_config_size() == 0) {
    LOG(WARNING) << "Empty forwarding pipeline configs file: "
                 << FLAGS_forwarding_pipeline_configs_file << ".";
    return ::util::OkStatus();
  }

  // Push the forwarding pipeline config for all the nodes we know about. Push
  // the config to hardware only if it is a coldboot setup.
  forwarding_pipeline_configs_ = absl::make_unique<ForwardingPipelineConfigs>();
  if (!warmboot) {
    for (const auto& e : configs.node_id_to_config()) {
      ::util::Status error =
          switch_interface_->PushForwardingPipelineConfig(e.first, e.second);
      if (!error.ok()) {
        error_buffer_->AddError(
            error,
            absl::StrCat("Failed to push the saved forwarding pipeline configs "
                         "for node ",
                         e.first, ": "),
            GTL_LOC);
        APPEND_STATUS_IF_ERROR(status, error);
      } else {
        (*forwarding_pipeline_configs_->mutable_node_id_to_config())[e.first] =
            e.second;
      }
    }
  } else {
    // In the case of warmboot, the assumption is that the configs saved into
    // file are the latest configs which were already pushed to one or more
    // nodes.
    *forwarding_pipeline_configs_ = configs;
  }

  return status;
}

namespace {

// TODO(unknown): This needs to be changed later per p4 runtime error
// reporting scheme.
::grpc::Status ToGrpcStatus(const ::util::Status& status,
                            const std::vector<::util::Status>& details) {
  // We need to create a ::google::rpc::Status and populate it with all the
  // details, then convert it to ::grpc::Status.
  ::google::rpc::Status from;
  if (!status.ok()) {
    from.set_code(ToGoogleRpcCode(status.CanonicalCode()));
    from.set_message(status.error_message());
    // Add individual errors only when the top level error code is not OK.
    for (const auto& detail : details) {
      // Each individual detail is converted to another ::google::rpc::Status,
      // which is then serialized as one proto any in 'from' message above.
      ::p4::v1::Error error;
      if (!detail.ok()) {
        error.set_canonical_code(ToGoogleRpcCode(detail.CanonicalCode()));
        error.set_code(detail.error_code());
        error.set_message(detail.error_message());
      } else {
        error.set_code(::google::rpc::OK);
      }
      from.add_details()->PackFrom(error);
    }
  } else {
    from.set_code(::google::rpc::OK);
  }

  return ::grpc::Status(ToGrpcCode(from.code()), from.message(),
                        from.SerializeAsString());
}

// Helper to facilitate logging the write requests to the desired log file.
void LogWriteRequest(uint64 node_id, const ::p4::v1::WriteRequest& req,
                     const std::vector<::util::Status>& results,
                     const absl::Time timestamp) {
  if (results.size() != req.updates_size()) {
    LOG(ERROR) << "Size mismatch: " << results.size()
               << " != " << req.updates_size() << ". Did not log anything!";
    return;
  }
  std::string msg = "";
  std::string ts =
      absl::FormatTime("%Y-%m-%d %H:%M:%E6S", timestamp, absl::LocalTimeZone());
  for (size_t i = 0; i < results.size(); ++i) {
    absl::StrAppend(&msg, ts, ";", node_id, ";",
                    req.updates(i).ShortDebugString(), ";",
                    results[i].error_message(), "\n");
  }
  ::util::Status status =
      WriteStringToFile(msg, FLAGS_write_req_log_file, /*append=*/true);
  if (!status.ok()) {
    LOG_EVERY_N(ERROR, 50) << "Failed to log the write request: "
                           << status.error_message();
  }
}

}  // namespace

::grpc::Status P4Service::Write(::grpc::ServerContext* context,
                                const ::p4::v1::WriteRequest* req,
                                ::p4::v1::WriteResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service, Write, context);

  if (!req->updates_size()) return ::grpc::Status::OK;  // Nothing to do.

  // BEGIN
  std::cout << "Write -> after auth" << std::endl;
  // END

  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  // BEGIN
  std::cout << "Write -> node_id = " << node_id << std::endl;
  // END
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  // Require valid election_id for Write.
  absl::uint128 election_id =
      absl::MakeUint128(req->election_id().high(), req->election_id().low());
  // BEGIN
  std::cout << "Write -> election_id.high = " << req->election_id().high() << ", election_id.low = " << req->election_id().low() << std::endl;
  // END
  if (election_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid election ID.");
  }

  // Make sure this node already has a master controller and the given
  // election_id and the uri of the client matches those of the master.
  if (!IsWritePermitted(node_id, election_id, context->peer())) {
    // BEGIN
    std::cout << "Write -> cannot write" << std::endl;
    // END
    return ::grpc::Status(::grpc::StatusCode::PERMISSION_DENIED,
                          "Write from non-master is not permitted.");
  }

  std::vector<::util::Status> results = {};
  absl::Time timestamp = absl::Now();
  // BEGIN
  std::cout << "Write -> before calling WriteForwardingEntries" << std::endl;
  // END
  ::util::Status status =
      switch_interface_->WriteForwardingEntries(*req, &results);
  // BEGIN
  std::cout << "Write -> status = " << status.error_code() << std::endl;
  // END
  if (!status.ok()) {
    LOG(ERROR) << "Failed to write forwarding entries to node " << node_id
               << ": " << status.error_message();
  }

  // Log debug info for future debugging.
  LogWriteRequest(node_id, *req, results, timestamp);

  // BEGIN
  std::cout << "Write -> end" << std::endl;
  // END

  return ToGrpcStatus(status, results);
}

::grpc::Status P4Service::Read(
    ::grpc::ServerContext* context, const ::p4::v1::ReadRequest* req,
    ::grpc::ServerWriter<::p4::v1::ReadResponse>* writer) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service, Read, context);

  // BEGIN
  std::cout << "Read -> after auth" << std::endl;
  // END

  if (!req->entities_size()) return ::grpc::Status::OK;
  // BEGIN
  std::cout << "Read -> device_id = " << req->device_id() << std::endl;
  // END
  if (req->device_id() == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  ServerWriterWrapper<::p4::v1::ReadResponse> wrapper(writer);
  std::vector<::util::Status> details = {};
  ::util::Status status =
      switch_interface_->ReadForwardingEntries(*req, &wrapper, &details);
  // BEGIN
  std::cout << "Read -> status = " << status.error_code() << std::endl;
  // END
  if (!status.ok()) {
    LOG(ERROR) << "Failed to read forwarding entries from node "
               << req->device_id() << ": " << status.error_message();
  }

  // BEGIN
  std::cout << "Read -> end" << std::endl;
  // END
  return ToGrpcStatus(status, details);
}

// BEGIN test Carolina
void printP4Info(std::string binaryCfg_, p4::config::v1::P4Info p4Info_) {
  std::cout << "printP4Info." << std::endl;
  std::cout << "Arch: " << p4Info_.pkg_info().arch() << std::endl;
  std::cout << "Table size: " << p4Info_.tables_size() << std::endl;
  std::string tableName;
  for (::p4::config::v1::Table table : p4Info_.tables()) {
    tableName = table.preamble().name();
    std::cout << "Table name: " << tableName << std::endl;
    std::cout << "Table id: " << table.preamble().id() << std::endl;
    for (::p4::config::v1::MatchField matchField : table.match_fields()) {
      std::cout << "\tMatch name (table=" << tableName << "): " << table.preamble().name() << std::endl;
      std::cout << "\tMatch type (table=" << tableName << "): " << table.preamble().GetTypeName() << std::endl;
    }
    for (::p4::config::v1::ActionRef actionRef : table.action_refs()) {
      std::cout << "\tAction ref. id (table=" << tableName << "): " << actionRef.id() << std::endl;
    }
  }
  std::string actionName;
  for (::p4::config::v1::Action action : p4Info_.actions()) {
    actionName = action.preamble().name();
    std::cout << "Action name: " << actionName << std::endl;
    std::cout << "Action id: " << action.preamble().id() << std::endl;

    for (p4::config::v1::Action_Param param : action.params()) {
      std::cout << "\tAction param name (action=" << actionName << "): " << param.name() << std::endl;
      std::cout << "\tAction param id (action=" << actionName << "): " << param.id() << std::endl;
      std::cout << "\tAction param bitwidth (action=" << actionName << "): " << param.bitwidth() << std::endl;
    }
  }
}

void printForwardingPipelineInfo(std::string binaryCfg_, p4::config::v1::P4Info p4Info_) {
  std::cout << "printForwardingPipelineInfo." << std::endl;
  std::cout << "Obtained P4 device config (first 100 char.): " << std::endl;
  std::cout << binaryCfg_.substr(0,100) << std::endl;
  std::cout << "Obtained P4 info (first 100 char.): " << std::endl;
  // std::cout << (*p4Info_).SerializeAsString().substr(0,100) << std::endl;
  std::cout << p4Info_.SerializeAsString().substr(0,100) << std::endl;
  printP4Info(binaryCfg_, p4Info_);
}
// END test Carolina

::grpc::Status P4Service::SetForwardingPipelineConfig(
    ::grpc::ServerContext* context,
    const ::p4::v1::SetForwardingPipelineConfigRequest* req,
    ::p4::v1::SetForwardingPipelineConfigResponse* resp) {
  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> start." << std::endl;
  LOG(INFO) << "SetForwardingPipelineConfig -> start.";
  // END test Carolina
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service,
                           SetForwardingPipelineConfig, context);

  // BEGIN test Carolina
  uint64 node_id_tmp = req->device_id();
  LOG(INFO) << "SetForwardingPipelineConfig -> node_id_tmp = " << node_id_tmp << ".";
  absl::uint128 election_id_tmp = absl::MakeUint128(req->election_id().high(),
                                                req->election_id().low());
  LOG(INFO) << "SetForwardingPipelineConfig -> election_id_tmp = " << election_id_tmp << ".";
  p4::config::v1::P4Info p4Info_ = req->config().p4info();
  std::string binaryCfg_ = req->config().p4_device_config();
  printForwardingPipelineInfo(binaryCfg_, p4Info_);
  // LOG(INFO) << "SetForwardingPipelineConfig -> binary cfg = " << binaryCfg_ << ".";
  // LOG(INFO) << "SetForwardingPipelineConfig -> p4Info cfg = " << p4Info_.SerializeAsString() << ".";
  // END test Carolina

  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> node_id = " << node_id << "." << std::endl;
  // END test Carolina
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  // We need valid election ID for SetForwardingPipelineConfig RPC
  absl::uint128 election_id = absl::MakeUint128(req->election_id().high(),
                                                req->election_id().low());
  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> election_id = " << election_id << "." << std::endl;
  // END test Carolina
  if (election_id == 0) {
    return ::grpc::Status(
        ::grpc::StatusCode::INVALID_ARGUMENT,
        absl::StrCat("Invalid election ID for node ", node_id, "."));
  }
  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> after node_id and election_id" << std::endl;
  // END test Carolina

  // Make sure this node already has a master controller and the given
  // election_id and the uri of the client matches those of the
  // master. According to the P4Runtime specification, only master can perform
  // SetForwardingPipelineConfig RPC.
  if (!IsWritePermitted(node_id, election_id, context->peer())) {
    // BEGIN test Carolina
    std::cout << "SetForwardingPipelineConfig -> IsWritePermitted -> NOT." << std::endl;
    // END test Carolina
    return ::grpc::Status(
        ::grpc::StatusCode::PERMISSION_DENIED,
        absl::StrCat("SetForwardingPipelineConfig from non-master is not "
                     "permitted for node ",
                     node_id, "."));
  }

  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> IsWritePermitted -> YES." << std::endl;
  // END test Carolina

  ::util::Status status = ::util::OkStatus();
  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> action = " << req->action() << "." << std::endl;
  // END test Carolina
  switch (req->action()) {
    case ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY:
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> action = verify." << std::endl;
      // END test Carolina
      APPEND_STATUS_IF_ERROR(status,
                             switch_interface_->VerifyForwardingPipelineConfig(
                                 node_id, req->config()));
      break;
    case ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT:
    case ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_SAVE: {
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> action = verify and something else." << std::endl;
      // END test Carolina
      absl::WriterMutexLock l(&config_lock_);
      // configs_to_save_in_file will have a copy of the configs that will be
      // saved in file at the end. Note that this copy may NOT be the same as
      // forwarding_pipeline_configs_.
      ForwardingPipelineConfigs configs_to_save_in_file;
      if (forwarding_pipeline_configs_ != nullptr) {
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 1a." << std::endl;
        // END test Carolina
        configs_to_save_in_file = *forwarding_pipeline_configs_;
      } else {
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 1b." << std::endl;
        // END test Carolina
        forwarding_pipeline_configs_ =
            absl::make_unique<ForwardingPipelineConfigs>();
      }
      ::util::Status error;
      if (req->action() ==
          ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT) {
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 2a." << std::endl;
        // END test Carolina
        error = switch_interface_->PushForwardingPipelineConfig(
             node_id, req->config());
      } else {  // VERIFY_AND_SAVE
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 2b." << std::endl;
        // END test Carolina
        error = switch_interface_->SaveForwardingPipelineConfig(
             node_id, req->config());
      }
      APPEND_STATUS_IF_ERROR(status, error);
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> trace 3." << std::endl;
      // END test Carolina
      // If the config push was successful or reported reboot required, save
      // the config in file. But only mutate the internal copy if we status
      // was OK.
      // TODO(unknown): this may not be appropriate for the VERIFY_AND_SAVE ->
      // COMMIT sequence of operations.
      if (error.ok() || error.error_code() == ERR_REBOOT_REQUIRED) {
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 4." << std::endl;
        // END test Carolina
        (*configs_to_save_in_file.mutable_node_id_to_config())[node_id] =
            req->config();
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 5." << std::endl;
        // END test Carolina
        APPEND_STATUS_IF_ERROR(
            status,
            WriteProtoToTextFile(configs_to_save_in_file,
                                 FLAGS_forwarding_pipeline_configs_file));
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 6." << std::endl;
        // END test Carolina
      }
      if (error.ok()) {
        // BEGIN test Carolina
        std::cout << "SetForwardingPipelineConfig -> trace 7." << std::endl;
        // END test Carolina
        (*forwarding_pipeline_configs_->mutable_node_id_to_config())[node_id] =
            req->config();
      }
      break;
    }
    case ::p4::v1::SetForwardingPipelineConfigRequest::COMMIT: {
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> trace 8." << std::endl;
      // END test Carolina
      ::util::Status error = switch_interface_->CommitForwardingPipelineConfig(
           node_id);
      APPEND_STATUS_IF_ERROR(status, error);
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> trace 9." << std::endl;
      // END test Carolina
      break;
    }
    case ::p4::v1::SetForwardingPipelineConfigRequest::RECONCILE_AND_COMMIT:
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> trace 10." << std::endl;
      // END test Carolina
      return ::grpc::Status(
          ::grpc::StatusCode::UNIMPLEMENTED,
          "RECONCILE_AND_COMMIT action not supported yet");
    default:
      // BEGIN test Carolina
      std::cout << "SetForwardingPipelineConfig -> trace 11." << std::endl;
      // END test Carolina
      return ::grpc::Status(
          ::grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Invalid action passed for node ", node_id, "."));
  }

  if (!status.ok()) {
    // BEGIN test Carolina
    std::cout << "SetForwardingPipelineConfig -> trace 12." << std::endl;
    // END test Carolina
    error_buffer_->AddError(
        status,
        absl::StrCat("Failed to set forwarding pipeline config for node ",
                     node_id, ": "),
        GTL_LOC);
    // BEGIN test Carolina
    std::cout << "SetForwardingPipelineConfig -> trace 13." << std::endl;
    // END test Carolina
    return ::grpc::Status(ToGrpcCode(status.CanonicalCode()),
                          status.error_message());
  }

  // BEGIN test Carolina
  std::cout << "SetForwardingPipelineConfig -> trace 14." << std::endl;
  // END test Carolina
  return ::grpc::Status::OK;
}

::grpc::Status P4Service::GetForwardingPipelineConfig(
    ::grpc::ServerContext* context,
    const ::p4::v1::GetForwardingPipelineConfigRequest* req,
    ::p4::v1::GetForwardingPipelineConfigResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service,
                           GetForwardingPipelineConfig, context);

  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  absl::ReaderMutexLock l(&config_lock_);
  if (forwarding_pipeline_configs_ == nullptr ||
      forwarding_pipeline_configs_->node_id_to_config_size() == 0) {
    return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                          "No valid forwarding pipeline config has been pushed "
                          "for any node so far.");
  }
  auto it = forwarding_pipeline_configs_->node_id_to_config().find(node_id);
  if (it == forwarding_pipeline_configs_->node_id_to_config().end()) {
    return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                          absl::StrCat("Invalid node id or no valid forwarding "
                                       "pipeline config has been pushed for "
                                       "node ",
                                       node_id, " yet."));
  }

  switch (req->response_type()) {
    case p4::v1::GetForwardingPipelineConfigRequest::ALL: {
      *resp->mutable_config() = it->second;
      break;
    }
    case p4::v1::GetForwardingPipelineConfigRequest::COOKIE_ONLY: {
      *resp->mutable_config()->mutable_cookie() = it->second.cookie();
      break;
    }
    case p4::v1::GetForwardingPipelineConfigRequest::P4INFO_AND_COOKIE: {
      *resp->mutable_config()->mutable_p4info() = it->second.p4info();
      *resp->mutable_config()->mutable_cookie() = it->second.cookie();
      break;
    }
    case p4::v1::GetForwardingPipelineConfigRequest::DEVICE_CONFIG_AND_COOKIE: {
      *resp->mutable_config()->mutable_p4_device_config() =
          it->second.p4_device_config();
      *resp->mutable_config()->mutable_cookie() = it->second.cookie();
      break;
    }
    default:
      return ::grpc::Status(
          ::grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Invalid action passed for node ", node_id, "."));
  }


  return ::grpc::Status::OK;
}

::grpc::Status P4Service::StreamChannel(
    ::grpc::ServerContext* context, ServerStreamChannelReaderWriter* stream) {
  std::cout << "StreamChannel before auth" << std::endl;
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service, StreamChannel,
                           context);
  std::cout << "StreamChannel after auth" << std::endl;

  // Here are the rules:
  // 1- When a client (aka controller) connects for the first time, we do not do
  //    anything until a MasterArbitrationUpdate proto is received.
  // 2- After MasterArbitrationUpdate is received at any time (we can receive
  //    this many time), the controller becomes/stays master or slave.
  // 3- At any point of time, only the master stream is capable of sending
  //    and receiving packets.

  // First thing to do is to find a new ID for this connection.
  auto ret = FindNewConnectionId();
  std::cout << "StreamChannel ret is OK = " << ret.ok() << std::endl;
  if (!ret.ok()) {
    return ::grpc::Status(ToGrpcCode(ret.status().CanonicalCode()),
                          ret.status().error_message());
  }
  uint64 connection_id = ret.ValueOrDie();
  std::cout << "StreamChannel connection_id = " << connection_id << std::endl;

  // The ID of the node this stream channel corresponds to. This is MUST NOT
  // change after it is set for the first time.
  uint64 node_id = 0;

  // The cleanup object. Will call RemoveController() upon exit.
  auto cleaner = gtl::MakeCleanup([this, &node_id, &connection_id]() {
    this->RemoveController(node_id, connection_id);
  });
  std::cout << "StreamChannel" << std::endl;

  ::p4::v1::StreamMessageRequest req;
  while (stream->Read(&req)) {
    std::cout << "StreamChannel reading request = " << req.SerializeAsString() << std::endl;
    switch (req.update_case()) {
      case ::p4::v1::StreamMessageRequest::kArbitration: {
        std::cout << "StreamChannel > is arbitration message? = " << req.has_arbitration() << std::endl;
        std::cout << "StreamChannel > arbitration message = " << req.arbitration().SerializeAsString() << std::endl;
        std::cout << "StreamChannel > device_id = " << req.arbitration().device_id() << std::endl;
        std::cout << "StreamChannel > node_id = " << node_id << std::endl;
        if (req.arbitration().device_id() == 0) {
          std::cout << "StreamChannel > bad device Id = " << req.arbitration().device_id() << std::endl;
          return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                                "Invalid node (aka device) ID.");
        } else if (node_id == 0) {
          node_id = req.arbitration().device_id();
        } else if (node_id != req.arbitration().device_id()) {
          std::stringstream ss;
          ss << "Node (aka device) ID for this stream has changed. Was "
             << node_id << ", now is " << req.arbitration().device_id() << ".";
          std::cout << "StreamChannel > bad device Id = " << req.arbitration().device_id() << " != " << node_id << std::endl;
          return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, ss.str());
        }
        absl::uint128 election_id =
            absl::MakeUint128(req.arbitration().election_id().high(),
                              req.arbitration().election_id().low());
        std::cout << "StreamChannel > election_id - high = " << req.arbitration().election_id().high() << std::endl;
        std::cout << "StreamChannel > election_id - low = " << req.arbitration().election_id().low() << std::endl;
        if (election_id == 0) {
          return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                                "Invalid election ID.");
        }
        // Try to add the controller to controllers_.
        std::cout << "StreamChannel > About to call AddOrModifyController - adding controller for node_id = " << node_id << ". Size: " << node_id_to_controllers_.size() << std::endl;
        auto status = AddOrModifyController(node_id, connection_id, election_id,
                                            context->peer(), stream);
        if (!status.ok()) {
          std::cout << "StreamChannel > Not adding the controller properly..." << status.error_code() << std::endl;
          return ::grpc::Status(ToGrpcCode(status.CanonicalCode()),
                                status.error_message());
        }
        break;
      }
      case ::p4::v1::StreamMessageRequest::kPacket: {
        std::cout << "StreamChannel > IsMasterController (true=master, false=slave) = " << IsMasterController(node_id, connection_id) << std::endl;
        // If this stream is not the master stream do not do anything.
        if (!IsMasterController(node_id, connection_id)) break;
        // If master, try to transmit the packet. No error reporting.
        ::util::Status status =
            switch_interface_->TransmitPacket(node_id, req.packet());
        if (!status.ok()) {
          LOG_EVERY_N(INFO, 500) << "Failed to transmit packet: " << status;
        }
        break;
      }
      case ::p4::v1::StreamMessageRequest::kDigestAck:
      case ::p4::v1::StreamMessageRequest::UPDATE_NOT_SET:
        std::cout << "StreamChannel - update not set" << std::endl;
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                              "Need to specify either arbitration or packet.");
        break;
    }
  }

  return ::grpc::Status::OK;
}

::grpc::Status P4Service::Capabilities(
    ::grpc::ServerContext* context,
    const ::p4::v1::CapabilitiesRequest* request,
    ::p4::v1::CapabilitiesResponse* response) {
  response->set_p4runtime_api_version(STRINGIFY(P4RUNTIME_VER));
  return ::grpc::Status::OK;
}

::util::StatusOr<uint64> P4Service::FindNewConnectionId() {
  absl::WriterMutexLock l(&controller_lock_);
  if (static_cast<int>(connection_ids_.size()) >=
      FLAGS_max_num_controller_connections) {
    return MAKE_ERROR(ERR_NO_RESOURCE)
           << "Can have max " << FLAGS_max_num_controller_connections
           << " active/inactive streams for all the node.";
  }
  uint64 max_connection_id =
      connection_ids_.empty() ? 0 : *connection_ids_.end();
  for (uint64 i = 1; i <= max_connection_id + 1; ++i) {
    if (!connection_ids_.count(i)) {
      connection_ids_.insert(i);
      return i;
    }
  }
  return 0;
}

::util::Status P4Service::AddOrModifyController(
    uint64 node_id, uint64 connection_id, absl::uint128 election_id,
    const std::string& uri, ServerStreamChannelReaderWriter* stream) {
  // To be called by all the threads handling controller connections.
  absl::WriterMutexLock l(&controller_lock_);
  auto it = node_id_to_controllers_.find(node_id);
  if (it == node_id_to_controllers_.end()) {
    absl::WriterMutexLock l(&packet_in_thread_lock_);
    // This is the first time we are hearing about this node. Lets try to add
    // an RX packet writer for it. If the node_id is invalid, registration will
    // fail.
    std::shared_ptr<Channel<::p4::v1::PacketIn>> channel =
        Channel<::p4::v1::PacketIn>::Create(128);
    // Create the writer and register with the SwitchInterface.
    auto writer = std::make_shared<ChannelWriterWrapper<::p4::v1::PacketIn>>(
        ChannelWriter<::p4::v1::PacketIn>::Create(channel));
    RETURN_IF_ERROR(
        switch_interface_->RegisterPacketReceiveWriter(node_id, writer));
    // Create the reader and pass it to a new thread.
    auto reader = ChannelReader<::p4::v1::PacketIn>::Create(channel);
    pthread_t tid = 0;
    int ret = pthread_create(
        &tid, nullptr, PacketReceiveThreadFunc,
        new ReaderArgs<::p4::v1::PacketIn>{this, std::move(reader), node_id});
    if (ret) {
      // Clean up state and return error.
      RETURN_IF_ERROR(
          switch_interface_->UnregisterPacketReceiveWriter(node_id));
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to create packet-in receiver thread for node "
             << node_id << " with error " << ret << ".";
    }
    // Store Channel and tid for Teardown().
    packet_in_reader_tids_.push_back(tid);
    packet_in_channels_[node_id] = channel;
    node_id_to_controllers_[node_id] = {};
    it = node_id_to_controllers_.find(node_id);
    std::cout << "AddOrModifyController - adding controller for node_id = " << node_id << ". Size: " << node_id_to_controllers_.size() << std::endl;
  }

  // Need to see if this controller was master before we process this new
  // request.
  bool was_master = (!it->second.empty() &&
                     connection_id == it->second.begin()->connection_id());

  // Need to check we do not go beyond the max number of connections per node.
  if (static_cast<int>(it->second.size()) >=
      FLAGS_max_num_controllers_per_node) {
    return MAKE_ERROR(ERR_NO_RESOURCE)
           << "Cannot have more than " << FLAGS_max_num_controllers_per_node
           << " controllers for node (aka device) with ID " << node_id << ".";
  }

  // Next see if this is a new controller for this node, or this is an existing
  // one. If there exist a controller with this connection_id remove it first.
  auto cont = std::find_if(
      it->second.begin(), it->second.end(),
      [=](const Controller& c) { return c.connection_id() == connection_id; });
  if (cont != it->second.end()) {
    it->second.erase(cont);
  }

  // Now add the controller to the set of controllers for this node. The add
  // will possibly lead to a new master.
  Controller controller(connection_id, election_id, uri, stream);
  it->second.insert(controller);

  // Find the most updated master. Also find out if this controller is master
  // after this new Controller instance was inserted.
  auto master = it->second.begin();  // points to master
  bool is_master = (election_id == master->election_id());

  // Now we need to do the following:
  // - If this new controller is master (no matter if it was a master before
  //   or not), we need to send its election_id to all connected controllers
  //   for this node. The arbitration token sent back to all the connected
  //   controllers will have OK status for the master and non-OK for slaves.
  // - The controller was master but it is not master now, this means a master
  //   change. We need to notify all connected controllers in this case as well.
  // - If this new controller is not master now and it was not master before,
  //   we just need to send the arbitration token with non-OK status to this
  //   controller.
  ::p4::v1::StreamMessageResponse resp;
  resp.mutable_arbitration()->set_device_id(node_id);
  resp.mutable_arbitration()->mutable_election_id()->set_high(
      master->election_id_high());
  resp.mutable_arbitration()->mutable_election_id()->set_low(
      master->election_id_low());
  if (is_master || was_master) {
    resp.mutable_arbitration()->mutable_status()->set_code(::google::rpc::OK);
    for (const auto& c : it->second) {
      if (!c.stream()->Write(resp)) {
        return MAKE_ERROR(ERR_INTERNAL)
               << "Failed to write to a stream for node " << node_id << ".";
      }
      // For non masters.
      resp.mutable_arbitration()->mutable_status()->set_code(
          ::google::rpc::ALREADY_EXISTS);
      resp.mutable_arbitration()->mutable_status()->set_message(
          "You are not my master!");
    }
  } else {
    resp.mutable_arbitration()->mutable_status()->set_code(
        ::google::rpc::ALREADY_EXISTS);
    resp.mutable_arbitration()->mutable_status()->set_message(
        "You are not my master!");
    if (!stream->Write(resp)) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to write to a stream for node " << node_id << ".";
    }
  }

  LOG(INFO) << "Controller " << controller.Name() << " is connected as "
            << (is_master ? "MASTER" : "SLAVE")
            << " for node (aka device) with ID " << node_id << ".";

  return ::util::OkStatus();
}

void P4Service::RemoveController(uint64 node_id, uint64 connection_id) {
  absl::WriterMutexLock l(&controller_lock_);
  connection_ids_.erase(connection_id);
  auto it = node_id_to_controllers_.find(node_id);
  if (it == node_id_to_controllers_.end()) return;
  auto controller = std::find_if(
      it->second.begin(), it->second.end(),
      [=](const Controller& c) { return c.connection_id() == connection_id; });
  if (controller != it->second.end()) {
    // Need to see if we are removing a master. Removing a master means
    // mastership change.
    bool is_master =
        controller->connection_id() == it->second.begin()->connection_id();
    // Get the name of the controller before removing it for logging purposes.
    std::string name = controller->Name();
    it->second.erase(controller);
    // Log the transition. Very useful for debugging. Also if there was a change
    // in mastership, let all other controller know.
    if (is_master) {
      if (it->second.empty()) {
        LOG(INFO) << "Controller " << name << " which was MASTER for node "
                  << "(aka device) with ID " << node_id
                  << " is disconnected. The node is "
                  << "now orphan :(";
      } else {
        LOG(INFO) << "Controller " << name << " which was MASTER for node "
                  << "(aka device) with ID " << node_id
                  << " is disconnected. New master is "
                  << it->second.begin()->Name();
        // We need to let all the connected controller know about this
        // mastership change.
        ::p4::v1::StreamMessageResponse resp;
        resp.mutable_arbitration()->set_device_id(node_id);
        resp.mutable_arbitration()->mutable_election_id()->set_high(
            it->second.begin()->election_id_high());
        resp.mutable_arbitration()->mutable_election_id()->set_low(
            it->second.begin()->election_id_low());
        resp.mutable_arbitration()->mutable_status()->set_code(
            ::google::rpc::OK);
        for (const auto& c : it->second) {
          c.stream()->Write(resp);  // Best effort.
          // For non masters.
          resp.mutable_arbitration()->mutable_status()->set_code(
              ::google::rpc::ALREADY_EXISTS);
          resp.mutable_arbitration()->mutable_status()->set_message(
              "You are not my master!");
        }
      }
    } else {
      if (it->second.empty()) {
        LOG(INFO) << "Controller " << name << " which was SLAVE for node "
                  << "(aka device) with ID " << node_id
                  << " is disconnected. The node is now orphan :(";
      } else {
        LOG(INFO) << "Controller " << name << " which was SLAVE for node "
                  << "(aka device) with ID " << node_id << " is disconnected.";
      }
    }
  }
}

bool P4Service::IsWritePermitted(uint64 node_id, absl::uint128 election_id,
                                 const std::string& uri) const {
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controllers_.find(node_id);
  // BEGIN test Carolina
  // Show mappings of nodes to controllers
  std::cout << "IsWritePermitted -> 1" << std::endl;
  std::cout << "IsWritePermitted -> node_id_to_controllers_.size() = " << node_id_to_controllers_.size() << std::endl;
  for (std::pair<uint64, std::set<Controller, ControllerComp>> entry : node_id_to_controllers_) {
    std::cout << "IsWritePermitted -> id #node = " << entry.first << std::endl;
    std::cout << "IsWritePermitted -> size / list controllers = " << entry.second.size() << std::endl;
  }
  std::cout << "IsWritePermitted -> 2" << std::endl;
  std::cout << "IsWritePermitted -> 2 (wrong condition 1 > ) = it == node_id_to_controllers_.end() = " << (it == node_id_to_controllers_.end()) << std::endl;
  std::cout << "IsWritePermitted -> 2 (wrong condition 2 > ) = it->second.empty() = " << it->second.empty() << std::endl;
  if (it == node_id_to_controllers_.end() || it->second.empty()) {
    std::cout << "IsWritePermitted -> 2a (wrong condition)" << std::endl;
    // std::cout << "IsWritePermitted -> it = " << it._M_node << std::endl;
    std::cout << "IsWritePermitted -> last node of mapping (id # node) = " << node_id_to_controllers_.end()->first << std::endl;
    std::cout << "IsWritePermitted -> last node of mapping (size / list controllers) = " << node_id_to_controllers_.end()->second.size() << std::endl;
  }
  std::cout << "IsWritePermitted -> 3" << std::endl;
  // if (it->second.begin()->election_id() == election_id) {
  //   std::cout << "IsWritePermitted -> 3b" << std::endl;
  //   // std::cout << "IsWritePermitted -> it->second.begin()->election_id() = " << it->second.begin()->election_id() << std::endl;
  //   std::cout << "IsWritePermitted -> election_id = " << election_id << std::endl;
  // }
  std::cout << "IsWritePermitted -> 4" << std::endl;
  // END test Carolina
  if (it == node_id_to_controllers_.end() || it->second.empty()) return false;
  // TODO(unknown): Find a way to check for uri as well.
  return it->second.begin()->election_id() == election_id;
}

bool P4Service::IsMasterController(uint64 node_id, uint64 connection_id) const {
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controllers_.find(node_id);
  if (it == node_id_to_controllers_.end() || it->second.empty()) return false;
  return it->second.begin()->connection_id() == connection_id;
}

void* P4Service::PacketReceiveThreadFunc(void* arg) {
  auto* args = reinterpret_cast<ReaderArgs<::p4::v1::PacketIn>*>(arg);
  auto* p4_service = args->p4_service;
  auto node_id = args->node_id;
  auto reader = std::move(args->reader);
  delete args;
  return p4_service->ReceivePackets(node_id, std::move(reader));
}

void* P4Service::ReceivePackets(
    uint64 node_id, std::unique_ptr<ChannelReader<::p4::v1::PacketIn>> reader) {
  do {
    ::p4::v1::PacketIn packet_in;
    // Block on next packet RX from Channel.
    int code = reader->Read(&packet_in, absl::InfiniteDuration()).error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle PacketIn.
    PacketReceiveHandler(node_id, packet_in);
  } while (true);
  return nullptr;
}

void P4Service::PacketReceiveHandler(uint64 node_id,
                                     const ::p4::v1::PacketIn& packet) {
  // We send the packets only to the master controller stream for this node.
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controllers_.find(node_id);
  if (it == node_id_to_controllers_.end() || it->second.empty()) return;
  ::p4::v1::StreamMessageResponse resp;
  *resp.mutable_packet() = packet;
  it->second.begin()->stream()->Write(resp);
}

}  // namespace hal
}  // namespace stratum

#!/bin/bash

ready_signal=/tmp/.stratum_bmv2.compiled
[[ -f ${ready_signal} ]] && rm -f ${ready_signal}

# Copy class with debug options for logic being served
cp -p /p4_service.cc /stratum/stratum/hal/lib/common/
# Copy class with debug options for bmv2 handler
cp -p /bmv2_switch.cc /stratum/stratum/hal/lib/bmv2/

# Fix for internal cfg error
sudo mkdir -p /etc/stratum
sudo chown -R $(whoami):$(whoami) /etc/stratum -R

# Build code
bazel build //stratum/hal/lib/common/...

# Copy chassis configuration for bmv2 model
cp -p /stratum/stratum/hal/bin/bmv2/dummy.json /stratum/bazel-bin/stratum/hal/bin/bmv2/

# Copy changes to external libraries
#cp -p /deps.bzl /stratum/bazel/
com_github_p4lang_pi_path="/stratum/bazel-stratum/external/com_github_p4lang_PI/proto/frontend/src/"
if [[ -d ${com_github_p4lang_pi_path} ]]; then
    cp -p /device_mgr.cpp ${com_github_p4lang_pi_path}
    cp -p /action_helpers.cpp ${com_github_p4lang_pi_path}
    cp -p /common.cpp ${com_github_p4lang_pi_path}
else
    echo "WARNING: EDF-used path \"${com_github_p4lang_pi_path}\" does not exist. No custom code will be applied"
fi

# Build code
bazel build //stratum/hal/bin/bmv2:stratum_bmv2

cd bazel-bin/stratum/hal/bin/bmv2

# Indicate server is properly built/generated
touch ${ready_signal}

# Run server
./stratum_bmv2 --initial-pipeline=dummy.json

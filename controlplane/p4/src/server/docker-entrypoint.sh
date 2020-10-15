#!/bin/bash

ready_signal=/tmp/.stratum_bmv2.compiled
[[ -f ${ready_signal} ]] && rm -f ${ready_signal}

#[[ -d ~/.cache/bazel ]] && sudo chown -R $(whoami):$(whoami) ~/.cache/bazel/
ls -lah ~/.cache
mkdir -p ~/.cache/bazel
[[ -d ~/.cache/bazel ]] && sudo chown -R $(whoami):$(whoami) ~/.cache/bazel/

sudo chown -R $(whoami):$(whoami) /stratum/stratum/hal/lib/common/
# Copy class with debug options for logic being served
cp -p /p4_service.cc /stratum/stratum/hal/lib/common/
sudo chown -R $(whoami):$(whoami) /stratum/stratum/hal/lib/bmv2/
# Copy class with debug options for bmv2 handler
cp -p /bmv2_switch.cc /stratum/stratum/hal/lib/bmv2/

# Fix for internal cfg error
sudo mkdir -p /etc/stratum
sudo chown -R $(whoami):$(whoami) /etc/stratum/

# Build code
sudo bazel build //stratum/hal/lib/common/...
echo "Result of first bazel build: $?"

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
#[[ -d ~/.cache/bazel ]] && sudo chown -R $(whoami):$(whoami) ~/.cache/bazel/
sudo mkdir -p ~/.cache/bazel/_bazel_gitlab-runner
ls -lah ~/.cache/bazel/_bazel_gitlab-runner
sudo chown $(whoami):$(whoami) -R ~/.cache
#sudo chown $(whoami):$(whoami) -R /tmp

bazel build //stratum/hal/bin/bmv2:stratum_bmv2
echo "Result of second bazel build: $?"

# Copy chassis configuration for bmv2 model
sudo chown -R $(whoami):$(whoami) /stratum/bazel-bin/stratum/hal/bin/bmv2/
cp -p /stratum/stratum/hal/bin/bmv2/dummy.json /stratum/bazel-bin/stratum/hal/bin/bmv2/

sudo chown -R $(whoami):$(whoami) bazel-bin/stratum/hal/bin/bmv2
cd bazel-bin/stratum/hal/bin/bmv2

# Indicate server is properly built/generated
sudo touch ${ready_signal}
sudo chown -R $(whoami):$(whoami) ${ready_signal}

# Run server
./stratum_bmv2 --initial-pipeline=dummy.json

echo 0

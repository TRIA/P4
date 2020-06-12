#!/bin/bash

# Copy class with debug options for bmv2 model
cp -p /p4_service.cc /stratum/stratum/hal/lib/common/
# Copy chassis configuration for bmv2 model
cp -p /stratum/stratum/hal/bin/bmv2/dummy.json /stratum/bazel-bin/stratum/hal/bin/bmv2/

# Build code
bazel build //stratum/hal/lib/common/...
bazel build //stratum/hal/bin/bmv2:stratum_bmv2

# Run server
cd bazel-bin/stratum/hal/bin/bmv2
./stratum_bmv2 --initial-pipeline=dummy.json

#!/bin/bash

# Clone SDKLT repo
cd $HOME
git clone https://github.com/Broadcom-Network-Switching-Software/SDKLT.git

# Export necessary environment variables
export YAML="/home/npl/libyaml"
export YAML_LIBDIR="$YAML/src/.libs"
export SDKLT="/home/npl/SDKLT"
export SDK="$SDKLT/src"

# Build target native_thsim. It will use XGSSIM as a simulated Tomahawk device running on your local machine
cd $SDK/appl/demo
make -s TARGET_PLATFORM=native_thsim

# Run the router
$SDK/appl/demo/build/native_thsim/sdklt

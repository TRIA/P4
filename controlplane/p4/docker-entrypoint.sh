#!/bin/bash

SRC_PATH=${PWD}/p4/src/common

# Copy generated gRPC classes where these can be used for compilation
cp -R grpc_out ${SRC_PATH}/

# Infinite loop to temporarily provide a working environment. TODO: remove
echo ""
echo ""
echo "Infinite loop. This will be kept on hold until the container is stopped"
echo "Use a new terminal to attach (\"make edf-cp-client-attach\") or stop (\"make edf-cp-client-stop\") the client"
echo ""
while true; do
    sleep 60
done

#!/bin/bash

ready_signal=.p4rt_cp.finished
[[ -f ${ready_signal} ]] && rm -f ${ready_signal}

EDF_CP_CLIENT_BIN=edf-cp-client

cd p4

CFG_REL_PATH=cfg
SRC_PATH=${PWD}/src
BIN_PATH=${PWD}/bin
TEST_PATH=${PWD}/test

# Copy generated gRPC classes where these can be used for compilation
cp -R ../grpc_out ${SRC_PATH}/common/

# Compile client
make ${EDF_CP_CLIENT_BIN}

# Run client
${BIN_PATH}/${EDF_CP_CLIENT_BIN} --grpc-addr=edf-dp-server:28000 --config=${CFG_REL_PATH}/p4info.txt,${CFG_REL_PATH}/bmv2.json --election-id=0,1 

touch ${ready_signal}

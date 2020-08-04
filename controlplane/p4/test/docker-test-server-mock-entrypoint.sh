#!/bin/bash

EDF_CP_SERVER_BIN=edf-cp-server-mock
EDF_CP_CLIENT_BIN=edf-cp-client

cd p4

CFG_REL_PATH=cfg
SRC_PATH=${PWD}/src
BIN_PATH=${PWD}/bin
TEST_PATH=${PWD}/test

# Copy generated gRPC classes where these can be used for compilation
cp -R ../grpc_out ${SRC_PATH}/common/

# Compile server and client
make ${EDF_CP_SERVER_BIN}
make ${EDF_CP_CLIENT_BIN}

# Run server (in background)
nohup ${BIN_PATH}/${EDF_CP_SERVER_BIN} > ${TEST_PATH}/${EDF_CP_SERVER_BIN}.log 2>&1 &
echo $! > ${TEST_PATH}/${EDF_CP_SERVER_BIN}.pid

# Run client
${BIN_PATH}/${EDF_CP_CLIENT_BIN} --grpc-addr=localhost:50051 --config=${CFG_REL_PATH}/p4info.txt,${CFG_REL_PATH}/bmv2.json --election-id=0,1 2>&1

# Kill server
kill -9 $(cat ${TEST_PATH}/${EDF_CP_SERVER_BIN}.pid)
rm -f ${TEST_PATH}/${EDF_CP_SERVER_BIN}.pid

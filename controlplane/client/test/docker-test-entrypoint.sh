#!/bin/bash

EDF_CP_SERVER_BIN=edf-cp-server
EDF_CP_CLIENT_BIN=edf-cp-client

CLIENT_PATH=${PWD}/client
TEST_PATH=${CLIENT_PATH}/test
SRC_PATH=${CLIENT_PATH}/src

# Copy generated gRPC classes where these can be used for compilation
cp -R grpc_out ${SRC_PATH}/

cd ${SRC_PATH}

# Compile server and client
make ${EDF_CP_SERVER_BIN}
make ${EDF_CP_CLIENT_BIN}

# Run server (in background)
nohup ./${EDF_CP_SERVER_BIN} > ${TEST_PATH}/${EDF_CP_SERVER_BIN}.log 2>&1 &
echo $! > ${TEST_PATH}/${EDF_CP_SERVER_BIN}.pid

# Run client
./${EDF_CP_CLIENT_BIN} --grpc-addr=localhost:50051 --config=../cfg/p4info.txt,../cfg/bmv2.json --election-id=0,1 > ${TEST_PATH}/${EDF_CP_CLIENT_BIN}.log 2>&1

# Kill server
kill -9 $(cat ${TEST_PATH}/${EDF_CP_SERVER_BIN}.pid)
rm -f ${TEST_PATH}/${EDF_CP_SERVER_BIN}.pid

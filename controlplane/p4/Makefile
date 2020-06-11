#
# Containers (vars)
#

GRPC_RELEASE_DEFAULT := v1.29.1
# If gRPC release not provided, fallback to the default one
GRPC_RELEASE := $(or ${grpc_release}, $(GRPC_RELEASE_DEFAULT))

# Control plane variables (client binary)
EDF_CONTROL_C_NAME := edf-cp-client
EDF_CONTROL_C_T_NAME := ${EDF_CONTROL_C_NAME}_test
EDF_CONTROL_TAG := ubuntu18
EDF_CFG_PATH := ${PWD}/cfg
EDF_CLIENT_SRC_PATH := ${PWD}/src/client
EDF_COMMON_SRC_PATH := ${PWD}/src/common
EDF_TEST_PATH := ${PWD}/test
EDF_CLIENT_R_PATH := /opt/p4runtime-cpp

# Data plane variables (using a modified Mininet for P4/bmv2-enabled switches, emulating the data plane)
EDF_DATA_C_T_NAME := mininet-p4
EDF_DATA_PATH := ${PWD}/../../dataplane/p4
EDF_DATA_ENV_PATH := ${EDF_DATA_PATH}/env

# Common
EDF_C_NET := rina-controlplane
CURRENT := ${PWD}

# Check whether specific common (C) and test (C_T) exist and if these run
DOCKER_INSTALLED = $(shell which docker >/dev/null && echo "true" || echo "false")
ifeq ($(DOCKER_INSTALLED),true)
	EDF_CONTROL_C_EXISTS := $(if $(strip $(shell docker ps -a | grep ${EDF_CONTROL_C_NAME})),$(patsubst '',false,true),false)
	EDF_CONTROL_C_RUNS := false
	ifeq ($(shell docker inspect -f '{{.State.Running}}' ${EDF_CONTROL_C_NAME}), true)
		EDF_CONTROL_C_RUNS := true
	endif
	EDF_CONTROL_C_T_EXISTS := $(if $(strip $(shell docker ps -a | grep ${EDF_CONTROL_C_T_NAME})),$(patsubst '',false,true),false)
	EDF_CONTROL_C_T_RUNS := false
	ifeq ($(shell docker inspect -f '{{.State.Running}}' ${EDF_CONTROL_C_T_NAME}), true)
		EDF_CONTROL_C_T_RUNS := true
	endif
else
	EDF_CONTROL_C_EXISTS := false
	EDF_CONTROL_C_RUNS := false
	EDF_CONTROL_C_T_RUNS := false
	EDF_CONTROL_C_T_EXISTS := false
endif

#
# Code (vars)
#

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= ${HOST_SYSTEM}
GRPC_BUILDDIR := ${EDF_COMMON_SRC_PATH}/grpc_out
EDF_DEPENDENCIES := $(shell find ${GRPC_BUILDDIR} -type f -name "*.cc")
EDF_DEPENDENCIES_OBJ := ${EDF_DEPENDENCIES:%.cc=%.o}
#EDF_SOURCES := $(shell find ${PWD} -maxdepth 1 -type f -name "*.cc")
EDF_SOURCES := ${EDF_CLIENT_SRC_PATH}/main.cc
EDF_SOURCES_OBJ := ${EDF_SOURCES:%.cc=%.o}
EDF_CP_CLIENT_BIN := edf-cp-client
EDF_CP_SERVER_MOCK_BIN := edf-cp-server-mock
CXX := g++
CXXFLAGS += -std=c++17
LDFLAGS += -L/usr/local/lib $(shell pkg-config --libs protobuf grpc++) -pthread
ifeq (${SYSTEM},Darwin)
	LDFLAGS += -lgrpc++_reflection
else
	LDFLAGS += -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed -Wno-deprecated
endif
LDFLAGS += -ldl

default:
	$(error Please specify a make target (see README.md))

#
# Containers (logic)
#

edf-cp-docker-net-create:
	docker network create ${EDF_C_NET} || true

edf-cp-docker-net-rm:
	docker network rm ${EDF_C_NET} || true

# Client environment setup

edf-cp-client-clean:
	@echo "Clean-up any related image and container"
	docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker image rm ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	make edf-cp-docker-net-rm

edf-cp-client-setup:
	mkdir -p ${EDF_CFG_PATH}
	mkdir -p ${GRPC_BUILDDIR}
	sudo chown ${USER}:${USER} -R ${GRPC_BUILDDIR}

edf-cp-client-build: edf-cp-client-setup
	@echo "Building and tagging image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}"
	docker build --build-arg GRPC_RELEASE=${GRPC_RELEASE} -f Dockerfile -t ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} .

edf-cp-client-start: edf-cp-client-build edf-cp-docker-net-create
	@echo "Starting image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} in container ${EDF_CONTROL_C_NAME}"
	cp -Rp ${EDF_DATA_PATH}/model/build/* ${EDF_CFG_PATH}/
	${EDF_CONTROL_C_EXISTS} && docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker run -it -v ${PWD}:${EDF_CLIENT_R_PATH}/p4 --network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/docker-entrypoint.sh --name ${EDF_CONTROL_C_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	#-v ${PWD}/docker-entrypoint.sh:${EDF_CLIENT_R_PATH}/docker-entrypoint.sh 

edf-cp-client-attach:
	@echo "Attaching to container ${EDF_CONTROL_C_NAME}"
	docker exec -it ${EDF_CONTROL_C_NAME} /bin/bash

edf-cp-client-stop:
	${EDF_CONTROL_C_EXISTS} && ${EDF_CONTROL_C_RUNS} && docker stop ${EDF_CONTROL_C_NAME} || true

edf-cp-client-restart: edf-cp-client-stop edf-cp-client-start

# Data plane model generation

edf-dp-model-build:
	mkdir -p ${EDF_CFG_PATH}
	cp -Rp ${EDF_DATA_PATH}/model/build/* ${EDF_CFG_PATH}/

# Tests

# Automated test with a mocked gRPC server
edf-cp-test-mock: edf-cp-client-build edf-cp-docker-net-create
	@echo "Starting container ${EDF_CONTROL_C_T_NAME} with image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}, then running a quick test"
	make edf-dp-model-build
	${EDF_CONTROL_C_T_EXISTS} && docker rm -f ${EDF_CONTROL_C_T_NAME} || true
	docker run -d -v ${PWD}:/${EDF_CLIENT_R_PATH}/p4 \
	--network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/p4/test/docker-test-entrypoint.sh \
	--name ${EDF_CONTROL_C_T_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	sleep 90s
	docker logs ${EDF_CONTROL_C_T_NAME} > ${EDF_TEST_PATH}/${EDF_CP_CLIENT_BIN}.log
	sudo chown ${USER}:${USER} -R ${EDF_TEST_PATH}/*.log
	sudo chown ${USER}:${USER} -R ${GRPC_BUILDDIR}
	make edf-cp-docker-net-rm

edf-dp-mininet-start: edf-cp-docker-net-create
	cd ${EDF_DATA_PATH} && make mn-single-start && cd ${CURRENT}

edf-dp-mininet-stop:
	cd ${EDF_DATA_PATH} && make mn-single-stop && cd ${CURRENT}

# Manual test with bmv2's P4 virtual switches running in Mininet (mn-stratum)
# Note: Dockerfile for mn-stratum in https://github.com/stratum/stratum/blob/master/tools/mininet/Dockerfile
edf-cp-test-mininet: edf-cp-client-build
	@echo "Starting containers with images ${EDF_DATA_C_T_NAME} and ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} for manual tests"
	@echo "Remove manually (\"make edf-cp-test-mininet-stop\")"
	make edf-dp-model-build
	make edf-dp-mininet-start
	make edf-cp-client-start

edf-cp-test-mininet-clean: edf-cp-client-clean edf-dp-mininet-stop edf-cp-docker-net-rm

#
# Code (logic)
#

# Compiles a given "*.cc" file into an ".o" object under this folder (recursive)
%.o: %.cc
	${CXX} ${CXXFLAGS} ${LDFLAGS} -I${GRPC_BUILDDIR} -I${dir $<} -c $< -o $@

# Compiles all dependency files that were found under this folder (recursive)
sources-all: ${EDF_DEPENDENCIES_OBJ} ${EDF_SOURCES_OBJ}

${EDF_CP_CLIENT_BIN}: sources-all src/client/client.o
	${CXX} ${CXXFLAGS} -I${GRPC_BUILDDIR} ${EDF_DEPENDENCIES_OBJ} ${EDF_SOURCES_OBJ} src/client/client.o ${LDFLAGS} -o ${PWD}/bin/$@

${EDF_CP_SERVER_MOCK_BIN}: sources-all src/server/server.o
	${CXX} ${CXXFLAGS} -I${GRPC_BUILDDIR} ${EDF_DEPENDENCIES_OBJ} src/server/server.o ${LDFLAGS} -o ${PWD}/bin/$@

all: ${EDF_CP_CLIENT_BIN} ${EDF_CP_SERVER_MOCK_BIN}

clean:
	rm -f ${EDF_SOURCES_OBJ} src/client/client.o src/server/server.o bin/${EDF_CP_CLIENT_BIN} bin/${EDF_CP_SERVER_MOCK_BIN}

clean-deep: clean
	rm -f ${EDF_DEPENDENCIES_OBJ}

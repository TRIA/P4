#
# Code (vars)
#

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= ${HOST_SYSTEM}
EDF_COMMON_SRC_PATH := ${PWD}/src/common
EDF_CFG_PATH := ${PWD}/cfg
EDF_CLIENT_SRC_PATH := ${PWD}/src/client
EDF_REFERENCE_SRC_PATH := ${PWD}/src/reference
EDF_SERVER_SRC_PATH := ${PWD}/src/server
GRPC_BUILDDIR := ${EDF_COMMON_SRC_PATH}/grpc_out
GRPC_BUILDDIR_EXISTS := $(shell test -d ${GRPC_BUILDDIR} && echo "true" || echo "false")
ifeq (${GRPC_BUILDDIR_EXISTS},true)
	EDF_DEPENDENCIES := $(shell find ${GRPC_BUILDDIR} -type f -name "*.cc")
	EDF_DEPENDENCIES_OBJ := ${EDF_DEPENDENCIES:%.cc=%.o}
else
	EDF_DEPENDENCIES := ""
	EDF_DEPENDENCIES_OBJ := ""
endif
#EDF_SOURCES := $(shell find ${PWD} -maxdepth 1 -type f -name "*.cc")
EDF_SOURCES := ${EDF_CLIENT_SRC_PATH}/main.cc
EDF_SOURCES_OBJ := ${EDF_SOURCES:%.cc=%.o}
EDF_CP_CLIENT_BIN := edf-cp-client
EDF_CP_CLIENT_READY_SIGNAL := ${PWD}/.p4rt_cp.finished
EDF_DP_SERVER_BIN := edf-dp-server
EDF_DP_SERVER_MOCK_BIN := ${EDF_DP_SERVER_BIN}-mock
EDP_DP_SERVER_READY_SIGNAL := ${PWD}/env/tmp/.stratum_bmv2.compiled
CXX := g++
CXXFLAGS += -std=c++17 -g -O0
LDFLAGS += -L/usr/local/lib $(shell pkg-config --libs protobuf grpc++) -pthread
ifeq (${SYSTEM},Darwin)
	LDFLAGS += -lgrpc++_reflection
else
	LDFLAGS += -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed -Wno-deprecated
endif
LDFLAGS += -ldl

#
# Containers (vars)
#

GRPC_RELEASE_DEFAULT := v1.29.1
# If gRPC release not provided, fallback to the default one
GRPC_RELEASE := $(or ${grpc_release}, $(GRPC_RELEASE_DEFAULT))

# Control plane variables (client binary)
EDF_CONTROL_C_NAME := ${EDF_CP_CLIENT_BIN}
EDF_CONTROL_C_T_NAME := ${EDF_CONTROL_C_NAME}_test
EDF_CONTROL_TAG := ubuntu18
EDF_CLIENT_R_PATH := /opt/p4runtime-cpp
EDF_TEST_PATH := ${PWD}/test

# Data plane variables (using a modified Mininet for P4/bmv2-enabled switches, emulating the data plane)
EDF_DATA_C_T_NAME := mininet-p4
EDF_DATA_S_NAME := ${EDF_DP_SERVER_BIN}
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

default:
	$(error Please specify a make target (see README.md))

#
# Containers (logic)
#

edf-cp-docker-net-create:
	docker network create ${EDF_C_NET} || true

edf-cp-docker-net-rm:
	docker network rm ${EDF_C_NET} || true

# Data plane model generation

edf-dp-v1model-build:
	mkdir -p ${EDF_CFG_PATH}
	current=${PWD} && cd ${EDF_DATA_PATH} && \
	make efcp-v1model-build && cd ${current}
	cp -Rp ${EDF_DATA_PATH}/build/arch/v1model/* ${EDF_CFG_PATH}/

# Client environment setup

edf-cp-setup:
	mkdir -p ${EDF_CFG_PATH}
	mkdir -p ${GRPC_BUILDDIR}
	sudo chown ${USER}:${USER} -R ${GRPC_BUILDDIR}

edf-cp-client-clean:
	@echo "Clean-up any related image and container"
	docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker image rm ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	make edf-cp-docker-net-rm

edf-cp-client-test-clean:
	@echo "Clean-up any related image and container"
	docker rm -f ${EDF_CONTROL_C_T_NAME} || true
	rm -f ${EDF_CP_CLIENT_READY_SIGNAL}

edf-cp-client-build: edf-cp-setup
	@echo "Building and tagging image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}"
	sudo rm -rf ${PWD}/env/tmp/
	docker build --build-arg GRPC_RELEASE=${GRPC_RELEASE} -f Dockerfile -t ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} .

edf-cp-client-start: edf-cp-client-build edf-cp-docker-net-create
	@echo "Starting image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} in container ${EDF_CONTROL_C_NAME}"
	make edf-dp-model-build
	${EDF_CONTROL_C_EXISTS} && docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker run -it -v ${PWD}:${EDF_CLIENT_R_PATH}/p4 --network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/docker-entrypoint.sh --name ${EDF_CONTROL_C_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true

edf-cp-client-attach:
	@echo "Attaching to container ${EDF_CONTROL_C_NAME}"
	docker exec -it ${EDF_CONTROL_C_NAME} /bin/bash

edf-cp-client-stop:
	${EDF_CONTROL_C_EXISTS} && ${EDF_CONTROL_C_RUNS} && docker stop ${EDF_CONTROL_C_NAME} || true

edf-cp-client-restart: edf-cp-client-stop edf-cp-client-start

# Server environment setup

edf-dp-server-build: edf-cp-setup
	@echo "Cloning Stratum if not there"
	[ ! -d ${EDF_SERVER_SRC_PATH}/stratum ] && git clone https://github.com/stratum/stratum.git ${EDF_SERVER_SRC_PATH}/stratum
	@echo "Copying custom Stratum's Dockerfile.dev"
	cp -p ${EDF_REFERENCE_SRC_PATH}/stratum/Dockerfile.dev ${EDF_SERVER_SRC_PATH}/stratum/
	@echo "Building and tagging image ${EDF_DATA_S_NAME}:${GRPC_RELEASE}"
	cd ${EDF_SERVER_SRC_PATH}/stratum && \
	docker build -t stratum-dev --build-arg USER_NAME="${USER}" --build-arg USER_ID="1000" -f Dockerfile.dev .

edf-dp-server-start: edf-dp-server-build edf-cp-docker-net-create
	@echo "Starting image stratum-dev in container ${EDF_DATA_S_NAME}"
	docker rm -f ${EDF_DATA_S_NAME} || true
	cd ${EDF_SERVER_SRC_PATH} && \
	mkdir -p ${PWD}/env/tmp/ && \
	echo "Running container ${EDF_DATA_S_NAME} (non-privileged user=${USER})" && \
	docker run -d -v ${PWD}/env/tmp/:/tmp -v ${PWD}/src/server/stratum/:/stratum \
	-v /home/${USER}/.cache:/home/${USER}/.cache \
	-v ${EDF_SERVER_SRC_PATH}/docker-entrypoint.sh:/docker-entrypoint.sh \
	-v ${EDF_REFERENCE_SRC_PATH}/stratum/hal/lib/common/p4_service.cc:/p4_service.cc \
	-v ${EDF_REFERENCE_SRC_PATH}/stratum/hal/lib/bmv2/bmv2_switch.cc:/bmv2_switch.cc \
	-v ${EDF_REFERENCE_SRC_PATH}/stratum/bazel/deps.bzl:/deps.bzl \
	-v ${EDF_REFERENCE_SRC_PATH}/external/com_github_p4lang_PI/proto/frontend/src/device_mgr.cpp:/device_mgr.cpp \
	-v ${EDF_REFERENCE_SRC_PATH}/external/com_github_p4lang_PI/proto/frontend/src/action_helpers.cpp:/action_helpers.cpp \
	-v ${EDF_REFERENCE_SRC_PATH}/external/com_github_p4lang_PI/proto/frontend/src/common.cpp:/common.cpp \
	-e NP4_INSTALL=/usr -p 28000:28000 -p 9339:9339 -w /stratum \
	--network=${EDF_C_NET} \
	--entrypoint /docker-entrypoint.sh \
	--user ${USER} --name ${EDF_DATA_S_NAME} stratum-dev

edf-dp-server-attach:
	@echo "Attaching to container ${EDF_DATA_S_NAME}"
	docker exec -it ${EDF_DATA_S_NAME} /bin/bash

edf-dp-server-stop:
	docker stop ${EDF_DATA_S_NAME} || true

edf-dp-server-log:
	#docker exec -it ${EDF_DATA_S_NAME} tail -1000f $(shell docker exec -it ${EDF_DATA_S_NAME} find /tmp -name *stratum_bmv2*ERROR*)
	docker logs -f ${EDF_DATA_S_NAME}

edf-dp-server-clean:
	docker rm -f ${EDF_DATA_S_NAME} || true
	rm -f ${EDP_DP_SERVER_READY_SIGNAL}
	[ -d ${EDF_SERVER_SRC_PATH}/stratum ] && sudo chown ${USER}:${USER} -R ${EDF_SERVER_SRC_PATH}/stratum || true


# Tests

# Manual test with bmv2's P4 virtual switches running in Mininet (mn-stratum)
# Note: Dockerfile for mn-stratum in https://github.com/stratum/stratum/blob/master/tools/mininet/Dockerfile
## FIXME: NOT WORKING ("edf-cp-client-start" not starting). Either run all containers as daemons or use docker-compose
edf-cp-test-mininet: edf-cp-client-build
	@echo "Starting containers with images ${EDF_DATA_C_T_NAME} and ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} for manual tests"
	@echo "Remove manually (\"make edf-cp-test-mininet-stop\")"
	make edf-dp-v1model-build
	make edf-dp-mininet-start
	make edf-cp-client-start

edf-cp-test-mininet-clean: edf-cp-client-clean edf-dp-mininet-stop edf-cp-docker-net-rm

# Automated test with a mocked gRPC server
edf-cp-test-mock: edf-cp-client-build edf-cp-docker-net-create
	@echo "Starting container ${EDF_CONTROL_C_T_NAME} with image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}, then running a quick test"
	make edf-dp-v1model-build
	${EDF_CONTROL_C_T_EXISTS} && docker rm -f ${EDF_CONTROL_C_T_NAME} || true
	docker run -d -v ${PWD}:/${EDF_CLIENT_R_PATH}/p4 \
	--network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/p4/test/docker-test-server-mock-entrypoint.sh \
	--name ${EDF_CONTROL_C_T_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	sleep 90s
	docker logs ${EDF_CONTROL_C_T_NAME} > ${EDF_TEST_PATH}/${EDF_CP_CLIENT_BIN}.log
	sudo chown ${USER}:${USER} -R ${EDF_TEST_PATH}/*.log
	sudo chown ${USER}:${USER} -R ${GRPC_BUILDDIR}
	make edf-cp-docker-net-rm

edf-dp-mininet-start: edf-cp-docker-net-create
	cd ${EDF_DATA_PATH} && make mn-start && cd ${CURRENT}

edf-dp-mininet-stop:
	cd ${EDF_DATA_PATH} && make mn-stop && cd ${CURRENT}

edf-dp-mininet-log:
	cd ${EDF_DATA_PATH} && make mn-log && cd ${CURRENT}

# Automated test with the Stratum-enabled bmv2 server
## NOTE: does not fully work in CI environments with gitlab-runner. Test rather in local
edf-cp-test-stratum: edf-cp-client-build
	@echo "Starting containers with images ${EDF_DATA_C_T_NAME} and ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} for manual tests"
	@echo "Remove manually (\"make edf-cp-test-mininet-stop\")"
	rm -f ${EDP_DP_SERVER_READY_SIGNAL} ${EDF_CP_CLIENT_READY_SIGNAL}
	make edf-dp-v1model-build
	make edf-dp-server-clean
	make edf-dp-server-start
	${EDF_CONTROL_C_T_EXISTS} && docker rm -f ${EDF_CONTROL_C_T_NAME} || true
	sleep 5
	# This one will close the DP server soon, thus not allowing it to fail, thus returning success
	while [ ! -f ${EDP_DP_SERVER_READY_SIGNAL} ] && [ "$(docker inspect -f '{{.State.Running}}' ${EDF_DATA_S_NAME})" != "false" ]; do sleep 10; done
	docker run -d -v ${PWD}:/${EDF_CLIENT_R_PATH}/p4 \
	--network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/p4/test/docker-test-server-entrypoint.sh \
	--name ${EDF_CONTROL_C_T_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	sleep 5
	while [ ! -f ${EDF_CP_CLIENT_READY_SIGNAL} ] && [ "$(docker inspect -f '{{.State.Running}}' ${EDF_CONTROL_C_T_NAME})" != "false" ]; do sleep 2; done
	docker logs ${EDF_CONTROL_C_T_NAME} > ${EDF_TEST_PATH}/${EDF_CONTROL_C_NAME}.log
	docker logs ${EDF_DATA_S_NAME} > ${EDF_TEST_PATH}/${EDF_DATA_S_NAME}.log
	#sudo cp -p $(docker inspect --format='{{.LogPath}}' ${EDF_DATA_S_NAME}) ${EDF_TEST_PATH}/${EDF_DATA_S_NAME}.log
	sleep 1
	sudo chown ${USER}:${USER} -R ${EDF_TEST_PATH}/*.log
	sudo chown ${USER}:${USER} -R ${GRPC_BUILDDIR}
	[ -f ${EDP_DP_SERVER_READY_SIGNAL} ] && [ -f ${EDF_CP_CLIENT_READY_SIGNAL} ] && make edf-cp-test-stratum-clean

edf-cp-test-stratum-clean: edf-cp-client-test-clean edf-dp-server-clean
	sudo rm -rf ${PWD}/env/tmp/

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

${EDF_DP_SERVER_MOCK_BIN}: sources-all src/server/server.o
	${CXX} ${CXXFLAGS} -I${GRPC_BUILDDIR} ${EDF_DEPENDENCIES_OBJ} src/server/server.o ${LDFLAGS} -o ${PWD}/bin/$@

# TODO: REMOVE
#client_test: sources-all src/client/client_test.o
#	${CXX} ${CXXFLAGS} -I${GRPC_BUILDDIR} ${EDF_DEPENDENCIES_OBJ} src/client/client_test.o ${LDFLAGS} -o ${PWD}/bin/$@
# END: REMOVE

all: ${EDF_CP_CLIENT_BIN} ${EDF_DP_SERVER_MOCK_BIN}

clean:
	rm -f ${EDF_SOURCES_OBJ} src/client/client.o src/server/server.o bin/${EDF_CP_CLIENT_BIN} bin/${EDF_DP_SERVER_MOCK_BIN}

clean-deep: clean
	rm -f ${EDF_DEPENDENCIES_OBJ}

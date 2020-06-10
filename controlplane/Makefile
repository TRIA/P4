GRPC_RELEASE_DEFAULT := v1.29.1
# If gRPC release not provided, fallback to the default one
GRPC_RELEASE := $(or ${grpc_release}, $(GRPC_RELEASE_DEFAULT))

# Control plane variables (client binary)
EDF_CONTROL_C_NAME := edf-cp-client
EDF_CONTROL_C_T_NAME := ${EDF_CONTROL_C_NAME}_test
EDF_CONTROL_TAG := ubuntu18
EDF_CLIENT_CFG := ${PWD}/client/cfg
EDF_CLIENT_SRC := ${PWD}/client/src
EDF_CLIENT_TEST := ${PWD}/client/test
EDF_CLIENT_R_PATH := /opt/p4runtime-cpp

# Data plane variables (using a modified Mininet for P4/bmv2-enabled switches, emulating the data plane)
EDF_DATA_C_T_NAME := mininet-p4
EDF_DATA_PATH := dataplane/p4
EDF_DATA_ENV_PATH := ${EDF_DATA_PATH}/env

# Common
EDF_C_NET := rina-controlplane
CURRENT := ${PWD}

# Check whether specific common (C) and test (C_T) exist and if these run
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

default:
	$(error Please specify a make target (see README.md))

# --------------------
# Container-related
# --------------------

edf-cp-docker-net-create:
	docker network create ${EDF_C_NET} || true

edf-cp-docker-net-rm:
	docker network rm ${EDF_C_NET} || true

# --------------------
# Client environment setup
# --------------------

edf-cp-client-clean:
	@echo "Clean-up any related image and container"
	docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker image rm ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	make edf-cp-docker-net-rm

edf-cp-client-setup:
	mkdir -p ${EDF_CLIENT_CFG}
	mkdir -p ${EDF_CLIENT_SRC}/grpc_out
	sudo chown ${USER}:${USER} -R ${EDF_CLIENT_SRC}/grpc_out

edf-cp-client-build: edf-cp-client-setup
	@echo "Building and tagging image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}"
	docker build --build-arg GRPC_RELEASE=${GRPC_RELEASE} -f Dockerfile -t ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} .

edf-cp-client-start: edf-cp-client-build edf-cp-docker-net-create
	@echo "Starting image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} in container ${EDF_CONTROL_C_NAME}"
	cp -Rp ${PWD}/../${EDF_DATA_PATH}/model/build/* ${EDF_CLIENT_CFG}/
	${EDF_CONTROL_C_EXISTS} && docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker run -it -v ${PWD}/client:${EDF_CLIENT_R_PATH}/client --network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/docker-entrypoint.sh --name ${EDF_CONTROL_C_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true

edf-cp-client-attach:
	@echo "Attaching to container ${EDF_CONTROL_C_NAME}"
	docker exec -it ${EDF_CONTROL_C_NAME} /bin/bash

edf-cp-client-stop:
	${EDF_CONTROL_C_EXISTS} && ${EDF_CONTROL_C_RUNS} && docker stop ${EDF_CONTROL_C_NAME} || true

edf-cp-client-restart: edf-cp-client-stop edf-cp-client-start

# --------------------
# Data plane model generation
# --------------------

edf-dp-model-build:
	mkdir -p ${EDF_CLIENT_CFG}
	cp -Rp ${PWD}/../${EDF_DATA_PATH}/model/build/* ${EDF_CLIENT_CFG}/

# --------------------
# Tests
# --------------------

# Automated test with a mocked gRPC server
edf-cp-test-mock: edf-cp-client-build edf-cp-docker-net-create
	@echo "Starting container ${EDF_CONTROL_C_T_NAME} with image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}, then running a quick test"
	make edf-dp-model-build
	${EDF_CONTROL_C_T_EXISTS} && docker rm -f ${EDF_CONTROL_C_T_NAME} || true
	docker run -d -v ${PWD}/client:/${EDF_CLIENT_R_PATH}/client --network=${EDF_C_NET} \
	--entrypoint ${EDF_CLIENT_R_PATH}/client/test/docker-test-entrypoint.sh \
	--name ${EDF_CONTROL_C_T_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true
	sleep 1m
	sudo chown ${USER}:${USER} -R ${EDF_CLIENT_TEST}/*.log
	sudo chown ${USER}:${USER} -R ${EDF_CLIENT_SRC}/grpc_out
	make edf-cp-docker-net-rm

edf-dp-mininet-start: edf-cp-docker-net-create
	cd ../${EDF_DATA_PATH} && make mn-single-start && cd ${CURRENT}

edf-dp-mininet-stop:
	cd ../${EDF_DATA_PATH} && make mn-single-stop && cd ${CURRENT}

# Manual test with bmv2's P4 virtual switches running in Mininet (mn-stratum)
# Note: Dockerfile for mn-stratum in https://github.com/stratum/stratum/blob/master/tools/mininet/Dockerfile
edf-cp-test-mininet: edf-cp-client-build
	@echo "Starting containers with images ${EDF_DATA_C_T_NAME} and ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} for manual tests"
	@echo "Remove manually (\"make edf-cp-test-mininet-stop\")"
	make edf-dp-model-build
	make edf-dp-mininet-start
	make edf-cp-client-start

edf-cp-test-mininet-clean: edf-cp-client-clean edf-dp-mininet-stop edf-cp-docker-net-rm

GRPC_RELEASE_DEFAULT := v1.29.1
GRPC_RELEASE := $(grpc_release)
# If release is not provided, fallback to the default one
GRPC_RELEASE := $(or $(grpc_release), $(GRPC_RELEASE_DEFAULT))

EDF_CONTROL_C_NAME := edf-cp-client
EDF_CONTROL_TAG := ubuntu18
#P4R_SERVER_IP := 127.0.0.1
#P4R_SERVER_PORT := 50002

default:
	$(error Please specify a make target (see README.md))

edf-cp-client-reset:
	@echo "Clean-up any related image and container"
	docker rm -f ${EDF_CONTROL_C_NAME} || true
	docker image rm ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} || true

edf-cp-client-build:
	@echo "Building and tagging image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}"
	docker build --build-arg GRPC_RELEASE=${GRPC_RELEASE} -f Dockerfile -t ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} .

edf-cp-client-run: edf-cp-client-build
	@echo "Running image ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE} in container ${EDF_CONTROL_C_NAME}, then attaching to it"
	docker run -it -v ${PWD}/client/src:/opt/p4runtime-cpp \
	--entrypoint /bin/bash --name ${EDF_CONTROL_C_NAME} ${EDF_CONTROL_C_NAME}:${GRPC_RELEASE}

edf-cp-client-attach:
	@echo "Attach to container ${EDF_CONTROL_C_NAME}"
	docker exec -it ${EDF_CONTROL_C_NAME} /bin/bash

edf-cp-client-start: edf-cp-client-build edf-cp-client-run

edf-cp-client-restart: edf-cp-client-reset edf-cp-client-start

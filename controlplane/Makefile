GRPC_RELEASE_DEFAULT := v1.29.1
GRPC_RELEASE := $(grpc_release)
# If release is not provided, fallback to the default one
GRPC_RELEASE := $(or $(grpc_release), $(GRPC_RELEASE_DEFAULT))

GRPC_C_NAME := grpc-cpp-env
GRPC_TAG := ubuntu18
P4R_C_NAME := p4lang/p4runtime-sh
P4R_SERVER_IP := 127.0.0.1
P4R_SERVER_PORT := 50002

default:
	$(error Please specify a make target (see README.md))

edf-cp-client-reset:
	@echo "Clean-up any related image and container"
	docker rm -f ${GRPC_C_NAME} || true
	docker image rm ${GRPC_C_NAME}:${GRPC_RELEASE} || true

edf-cp-client-build:
	@echo "Building and tagging image ${GRPC_C_NAME}:${GRPC_RELEASE}"
	docker build --build-arg GRPC_RELEASE=${GRPC_RELEASE} -f Dockerfile -t ${GRPC_C_NAME}:${GRPC_RELEASE} .

edf-cp-client-run: edf-cp-client-build
	@echo "Running image ${GRPC_C_NAME}:${GRPC_RELEASE} in container ${GRPC_C_NAME}, then attaching to it"
	#docker run -it -v p4runtime_cpp:${GRPC_C_ENTRYPOINT_PATH}/p4runtime-shell-cpp --entrypoint /bin/bash --name ${GRPC_C_NAME} ${GRPC_C_NAME}:${GRPC_RELEASE}
#	docker run -it -v p4runtime_cpp:/p4runtime-shell-cpp \
	--grpc-addr ${P4R_SERVER_IP}:${P4R_SERVER_PORT} \
	--device-id 0 --election-id 0,1 \
	--name ${GRPC_C_NAME} ${GRPC_C_NAME}:${GRPC_RELEASE}
	# IMPORTANT: overriding the entrypoint here (since "p4lang/p4runtime-shell" expects to run a server)
#	docker run -it -v p4runtime_cpp:/opt/p4runtime-shell-cpp -v pr4runtime:/opt/p4runtime
#	docker run -it -v ${PWD}/p4runtime-shell-cpp:/opt/p4runtime-shell-cpp -v ${PWD}/p4runtime:/opt/p4runtime
	
	#docker run -it -v ${PWD}/p4runtime-shell-cpp:/opt/p4runtime-shell-cpp:rw,rshared
	docker run -it -v ${PWD}/client:/opt/p4runtime-cpp \
	--entrypoint /bin/bash --name ${GRPC_C_NAME} ${GRPC_C_NAME}:${GRPC_RELEASE}

edf-cp-client-attach:
	@echo "Attach to container ${GRPC_C_NAME}"
	docker exec -it ${GRPC_C_NAME} /bin/bash

edf-cp-client-start: edf-cp-client-build edf-cp-client-run

edf-cp-client-restart: edf-cp-client-reset edf-cp-client-start

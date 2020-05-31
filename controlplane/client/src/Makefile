HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= ${HOST_SYSTEM}
GRPC_BUILDDIR := grpc_out
EDF_DEPENDENCIES := $(shell find ${GRPC_BUILDDIR} -type f -name "*.cc")
EDF_DEPENDENCIES_OBJ := ${EDF_DEPENDENCIES:%.cc=%.o}
#EDF_SOURCES := $(shell find ${PWD} -maxdepth 1 -type f -name "*.cc")
EDF_SOURCES := control_plane.cc
EDF_SOURCES_OBJ := ${EDF_SOURCES:%.cc=%.o}
EDF_CP_CLIENT_BIN := edf-cp-client
EDF_CP_SERVER_BIN := edf-cp-server
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

# Compiles a given "*.cc" file into an ".o" object under this folder (recursive)
%.o: %.cc
	${CXX} ${CXXFLAGS} ${LDFLAGS} -I${GRPC_BUILDDIR} -I${dir $<} -c $< -o $@

# Compiles all dependency files that were found under this folder (recursive)
sources-all: ${EDF_DEPENDENCIES_OBJ} ${EDF_SOURCES_OBJ}

${EDF_CP_CLIENT_BIN}: sources-all p4runtime_client.o
	${CXX} ${CXXFLAGS} -I${GRPC_BUILDDIR} ${EDF_DEPENDENCIES_OBJ} ${EDF_SOURCES_OBJ} p4runtime_client.o ${LDFLAGS} -o $@

${EDF_CP_SERVER_BIN}: sources-all p4runtime_server.o
	${CXX} ${CXXFLAGS} -I${GRPC_BUILDDIR} ${EDF_DEPENDENCIES_OBJ} p4runtime_server.o ${LDFLAGS} -o $@

all: ${EDF_CP_CLIENT_BIN} ${EDF_CP_SERVER_BIN}

clean:
	rm -f ${EDF_SOURCES_OBJ} p4runtime_client.o p4runtime_server.o ${EDF_CP_CLIENT_BIN} ${EDF_CP_SERVER_BIN}

clean-deep: clean
	rm -f ${EDF_DEPENDENCIES_OBJ}

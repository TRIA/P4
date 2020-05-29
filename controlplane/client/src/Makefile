HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
GRPC_BUILDDIR := grpc_out
EDF_DEPENDENCIES := $(shell find $(GRPC_BUILDDIR) -type f -name "*.cc")
EDF_DEPENDENCIES_O := $(EDF_DEPENDENCIES:%.cc=%.o)
EDF_SOURCES := $(shell find $(PWD) -maxdepth 1 -type f -name "*.cc")
EDF_SOURCES_O := $(EDF_SOURCES:%.cc=%.o)
CXX := g++
CXXFLAGS += -std=c++17
LDFLAGS += -L/usr/local/lib $(shell pkg-config --libs protobuf grpc++) -pthread
ifeq ($(SYSTEM),Darwin)
LDFLAGS += -lgrpc++_reflection
else
LDFLAGS += -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed -Wno-deprecated
endif
LDFLAGS += -ldl

default:
	$(error Please specify a make target (see README.md))

# Compiles all dependency files that were found under this folder (recursive)
edf-cp-source-compile: $(EDF_DEPENDENCIES_O) $(EDF_SOURCES_O)

# Compiles a given "*.cc" file into an ".o" object under this folder (recursive)
%.o: %.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -I$(GRPC_BUILDDIR) -I$(dir $<) -c $< -o $@

control_plane: edf-cp-source-compile
	$(CXX) $(CXXFLAGS) -I$(GRPC_BUILDDIR) $(EDF_DEPENDENCIES_O) $(EDF_SOURCES_O) $(LDFLAGS) -o $@

clean:
	rm -f $(EDF_SOURCES_O) control_plane

clean-full: clean
	rm -f $(EDF_DEPENDENCIES_O)

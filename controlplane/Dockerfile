# Third-party dependencies to run P4Runtime with C++
FROM p4lang/p4runtime-sh AS p4runtime-cpp-deps

SHELL ["/bin/bash", "-c"]

ARG GRPC_RELEASE
ENV RELEASE_TAG $GRPC_RELEASE
ENV BASE_PATH_GRPC /opt/grpc
ENV BASE_PATH_P4R /opt/p4runtime
ENV GOOGLE_PROTO_DIR ${BASE_PATH_P4R}/googleapis

##
# Install requirements
#
RUN apt-get update
RUN apt-get install -y sudo build-essential cmake gcc git pkg-config
## Install openssl (to use instead of boringssl)
RUN apt-get install -y libssl-dev
## Install bash for easier operation in the future
RUN apt-get install -y bash

##
# Download gRPC and initialise submodules. Install third-party libraries and also gRPC
#
RUN git clone -b $RELEASE_TAG https://github.com/grpc/grpc --recursive ${BASE_PATH_GRPC}
WORKDIR ${BASE_PATH_GRPC}
RUN git submodule update --init
## Note: those previously installed (in the p4lang/p4runtime-sh image) should not be installed now
WORKDIR ${BASE_PATH_GRPC}/test/distrib/cpp
# Remote location for script: https://raw.githubusercontent.com/grpc/grpc/${RELEASE_TAG}/test/distrib/cpp/run_distrib_test_cmake.sh
RUN ./run_distrib_test_cmake.sh

##
# Download p4runtime repository
#
RUN git clone https://github.com/p4lang/p4runtime --recursive ${BASE_PATH_P4R}

##
# Download googleapis repository
#
RUN git clone --depth 1 https://github.com/googleapis/googleapis.git ${GOOGLE_PROTO_DIR}

# Get back to the original directory
WORKDIR ${BASE_PATH_GRPC}

#
# ====================================================================
#

# Build on top of the image where 3rd-party dependencies are installed
# for python-based p4runtime-sh and extend with dependencies for C++

## NOTE: either copy generated files and libraries from gRPC (manually)
## or just inherit from the previous image, like in:
FROM p4runtime-cpp-deps

LABEL maintainer="Carolina Fernandez <carolina.fernandez@i2cat.net>"
LABEL description="Access methods for P4Runtime based on C++"

ENV BASE_PATH_EDF /opt/p4runtime-cpp

# Copy gRPC binaries
#COPY --from=p4runtime-cpp-deps /usr/local/bin/grpc* /usr/local/bin/
#COPY --from=p4runtime-cpp-deps /usr/local/lib/libgrpc* /usr/local/lib/
#RUN mkdir -p /usr/local/lib/cmake
#COPY --from=p4runtime-cpp-deps /usr/local/lib/cmake/grpc /usr/local/lib/cmake/
#RUN mkdir -p /usr/local/lib/pkgconfig
#COPY --from=p4runtime-cpp-deps /usr/local/lib/pkgconfig/grpc* /usr/local/lib/pkgconfig/
#COPY --from=p4runtime-cpp-deps /usr/local/share/grpc /usr/local/share/
#COPY --from=p4runtime-cpp-deps /usr/local/include/grpc /usr/local/include/
#COPY --from=p4runtime-cpp-deps /usr/local/include/grpcpp /usr/local/include/
#COPY --from=p4runtime-cpp-deps /usr/local/include/grpc++ /usr/local/include/
# Copy protobuf binaries
## TODO

# The following is adapted from Python3's "p4lang/p4runtime-shell's"
# Dockerfile to be used with C++

## Compile proto files
ENV PROTO_DIR ${BASE_PATH_P4R}/proto
ENV PROTOS="$PROTO_DIR/p4/v1/p4data.proto \
$PROTO_DIR/p4/v1/p4runtime.proto \
$PROTO_DIR/p4/config/v1/p4info.proto \
$PROTO_DIR/p4/config/v1/p4types.proto \
$GOOGLE_PROTO_DIR/google/rpc/status.proto \
$GOOGLE_PROTO_DIR/google/rpc/code.proto"
ENV PROTOFLAGS "-I$GOOGLE_PROTO_DIR -I$PROTO_DIR"
ENV PROTO_BUILD_DIR ${BASE_PATH_P4R}/gcc_out

#COPY p4runtime ${BASE_PATH_P4R}/

WORKDIR ${BASE_PATH_P4R}

RUN mkdir -p $PROTO_BUILD_DIR && \
    echo $PWD && ls -lah && \
#    mv /opt/googleapis $GOOGLE_PROTO_DIR && \
    protoc $PROTOS --cpp_out $PROTO_BUILD_DIR $PROTOFLAGS \
        --grpc_out $PROTO_BUILD_DIR --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) && \
    rm -rf $GOOGLE_PROTO_DIR

RUN echo "\
Generated C++ classes from P4Runtime proto files located at ${PROTO_BUILD_DIR}\
\
" >> /etc/motd

# Get back to the EDF base directory
WORKDIR ${BASE_PATH_EDF}

# P4-based control plane for the EDF RINA router

Components of the control plane interacting with the P4-based RINA router

## Preparation

The full environment must be set up in order to operate with the control plane.

### Data plane program compilation

First, compile the P4 program to be used in the switch. This will generate the expected configuration files that will be used later.

```bash
current=${PWD}
cd ../dataplane/p4
make efcp-build
cd $current
```

### Control plane environment

Now, spin up a container with the environment for running the control plane. This will take plenty of time, as this installs all the dependencies like protbufs, gRPC and other libraries. Wait for it to finish before running the control plane client (or the mocked gRPC server).

```bash
make edf-cp-client-run
```

### Data plane server

#### Option 1: virtual target in Mininet

In the typical scenario you will run a container with a Mininet instance that runs a bmv2 switch model. The process will run on address `0.0.0.0:50001`.

```bash
current=${PWD}
cd ../dataplane/p4
make env-start
cd $current
```

#### Option 2: mocked server

This option is quicker to quickly verify the proper working of the RPC methods. The process will run on address `0.0.0.0:50051`.
Run the following in a new terminal to attach to the container created in "Control plane environment".

```bash
make edf-cp-client-attach
cd src
make edf-cp-server
./edf-cp-server
```

### Control plane client

At this point, the control plane client can be compiled.
Run the following in a new terminal to attach to the container created in "Control plane environment".

The client will be the entry point to interact the RPC methods exposed by P4Runtime.
Run the program, providing any suitable argument. If none is provided, defaults will be taken and printed.

```bash
make edf-cp-client-attach
cd src
make edf-cp-client
# Option 1 for data plane server
./edf-cp-client --grpc-addr=localhost:50001 --config=../cfg/p4info.txt,../cfg/bmv2.json --election-id=0,1
# Option 2 for data plane server
./edf-cp-client --grpc-addr=localhost:50051 --config=../cfg/p4info.txt,../cfg/bmv2.json --election-id=0,1
```

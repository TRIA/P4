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

### Data plane server

#### Option 1: virtual target in Mininet

In the typical scenario you will run a container with a Mininet instance that runs a bmv2 switch model. The process will run on address `0.0.0.0:50001`.

```bash
make edf-dp-mininet-start
```

To attach to the logs, type the following in a new terminal:
```bash
make edf-dp-mininet-log
```

### Option 2: single bmv2 switch model

Similar to option 1, but the bmv2 model runs directly from the Stratum source. The process will run on address `0.0.0.0:28000`.

```bash
make edf-dp-server-start
```

To attach to the logs, type the following in a new terminal:
```bash
make edf-dp-server-log
```

#### Option 3: mocked server (deprecated)

This option is quicker to quickly verify the proper working of the RPC methods. The process will run on address `0.0.0.0:50051`.
Run the following in a new terminal to attach to the container created in "Control plane environment".

```bash
make edf-cp-client-attach
cd p4
make edf-dp-server-mock
./bin/edf-dp-server-mock
```

### Control plane client

Now, spin up a container with the environment for running the control plane. This will take plenty of time, as this installs all the dependencies like protobufs, gRPC and other libraries. Wait for it to finish before running the control plane client (or the mocked gRPC server).

```bash
make edf-cp-client-start
```

Once finished, the control plane client can be compiled. Run the following in a new terminal to attach to the container created before.

The client will be the entry point to interact the RPC methods exposed by P4Runtime.
Run the program, providing any suitable argument. If none is provided, defaults will be taken and printed.

```bash
make edf-cp-client-attach
cd p4
make edf-cp-client
# Option 1 for data plane server
./bin/edf-cp-client --grpc-addr=mn-stratum:50001 --config=cfg/p4info.txt,cfg/bmv2.json --election-id=0,1
# Option 2 for data plane server
./bin/edf-cp-client --grpc-addr=edf-dp-server:28000 --config=cfg/p4info.txt,cfg/bmv2.json --election-id=0,1
# Option 3 for data plane server (deprecated)
./bin/edf-cp-client --grpc-addr=localhost:50051 --config=cfg/p4info.txt,cfg/bmv2.json --election-id=0,1
```

### Extra: all-in-one testing

The following will compile and run the control plane client, so that it interacts with a compatible server. This expects the configuration files generated during the "Data plane program compilation" step.

```bash
# Use mininet for manual testing (option #1)
make edf-cp-test-mininet
# Use Stratum-enabled bmv2 server as an automated test (option #2, run locally)
make edf-cp-test-stratum
# Use mocked server as an automated test (option #3, deprecated)
make edf-cp-test-mock
```

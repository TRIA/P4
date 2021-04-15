# P4-based data plane for the EDF RINA router

## Structure

The _env_ folder contains scripts and files to be used for deployment.
The _model_ folder contains the data plane code for the P4 target.

## Usage

**Note**: the following environment and tests are based on the *v1model* architecture only.

# Automatic verification of EFCP and IPv4 transmission
make edf-dp-test-v1model-stratum

# Manual verification of EFCP and IPv4 transmission

## From your host: spin up the containers
make env-v1model-start

## From your host: attach to the mininet CLI
make mn-cli

## From your host: capture just one ingress packet and print content
make mn-bmv2-sniff-eth2
## Inside the mininet CLI: send an EFCP PDU from h1 to h2
mininet> h1 python tests/h1_send_efcp_h2.py

## From your host: capture just one egress packet and print content
make mn-bmv2-sniff-eth1
## Inside the mininet CLI: send an EFCP PDU from h1 to the CPU
mininet> h1 python tests/h1_send_efcp_cpu.py

## From your host: capture just one ingress packet and print content
make mn-bmv2-sniff-eth1
## Inside the mininet CLI: send an IPv4 packet from h2 to h1
mininet> h2 python tests/h2_send_ipv4_h1.py

## Stop containers after use
make env-v1model-stop
```

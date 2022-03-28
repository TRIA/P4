#ifndef _HEADERS_
#define _HEADERS_

struct egress_metadata_t {
    bool checksum_pdu_efcp;
    bool checksum_pdu_ipv4;

    bool checksum_err_efcp_egprs;
    bool checksum_err_ipv4_egprs;

    bit<19> enq_qdepth;
}

struct metadata_t {
    bool checksum_err_efcp_igprs;
    bool checksum_err_ipv4_igprs;

    bit<16> checksum_error;
}

struct empty_header_t {}

struct empty_metadata_t {}

// Header bridged between Ingress and Egress. This is the only way to
// associate some data to packets going from Ingreat to Egress. I seem to
// remember that this "problem" is new with the most recent version of
// the P4 language so people coming from an earlier version might be
// surprised by this. This works well, though, but you need to make sure
// you will not emit this at the output of the Egress stage.

header bridged_h {
    // I no longer remember why this is required but it is... This is
    // probably a requirement of the platform. You will need to modify
    // it if you add anything in this header.
    bit<5> _pad;

    // This identify a packet that has to go through VLAN processing.
    bit<1> is_vlan;

    // Just plain old broadcast
    bit<1> is_broadcast;

    // Port on which the packet has been received. This is the most
    // critical piece of information that I need in this header. Without
    // this, we cannot determine on what port a packet should not be
    // resent.
    PortId_t ingress_port;
}

struct header_t {
    bridged_h   bridged;

    ethernet_h  ethernet;
    dot1q_h     dot1q;
    efcp_h      efcp;
    ipv4_h      ipv4;
}

#endif /* _HEADERS_ */


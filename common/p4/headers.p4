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

// Header bridged between Ingress and Egress. Not to be emitted out of
// egress.
header bridged_h {
    bit<6> _pad;

    bit<1> is_replicated;

    // Port on which the packet has been received.
    PortId_t ingress_port;
}

struct header_t {
    bridged_h   bridged;

    ethernet_h  ethernet;
    dot1q_h     dot1q;
    efcp_h      efcp;
    ipv4_h      ipv4;
}

struct empty_header_t {}

struct empty_metadata_t {}

#endif /* _HEADERS_ */


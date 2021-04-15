/* -*- P4_16 -*- */
#include <core.p4>
#include <v1model.p4>
#include "../../common/headers.p4"

/*************************************************************************
*********************** C O N S T A N T S ********************************
*************************************************************************/
/*
CPU_PORT specifies the P4 port number associated to packet-in and packet-out.
All packets forwarded via this port will be delivered to the controller as
PacketIn messages. Similarly, PacketOut messages from the controller will be
seen by the P4 pipeline as coming from the CPU_PORT.
*/
#define CPU_PORT 255

/*
CPU_CLONE_SESSION_ID specifies the mirroring session for packets to be cloned
to the CPU port. Packets associated with this session ID will be cloned to
the CPU_PORT as well as being transmitted via their egress port as set by the
bridging/routing/acl table. For cloning to work, the P4Runtime controller
needs first to insert a CloneSessionEntry that maps this session ID to the
CPU_PORT.
*/
#define CPU_CLONE_SESSION_ID 99


/*************************************************************************
*********************** M E T A D A T A  *******************************
*************************************************************************/


/*
 * All metadata, globally used in the program, also  needs to be assembed
 * into a single struct. 
 */

struct metadata {
    /* In our case it is empty */
}


// Declare user-defined errors that may be signaled during parsing
error {
    wrong_pdu_type
}

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/
parser SwitchIngressParser(packet_in packet,
                out header_t hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata) {

    state start {
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_EFCP: parse_efcp;
            ETHERTYPE_DOT1Q: parse_dot1q;
            ETHERTYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_dot1q {
        packet.extract(hdr.dot1q);
        transition select(hdr.dot1q.proto_id) {
            ETHERTYPE_EFCP: parse_efcp;
            ETHERTYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        verify((hdr.efcp.pdu_type == DATA_TRANSFER ||
                hdr.efcp.pdu_type == LAYER_MANAGEMENT ||
                hdr.efcp.pdu_type == ACK_ONLY ||
                hdr.efcp.pdu_type == NACK_ONLY ||
                hdr.efcp.pdu_type == ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pdu_type == NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pdu_type == FLOW_CONTROL_ONLY ||
                hdr.efcp.pdu_type == SELECTIVE_ACK ||
                hdr.efcp.pdu_type == SELECTIVE_NACK ||
                hdr.efcp.pdu_type == SELECTIVE_ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pdu_type == SELECTIVE_NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pdu_type == CONTROL_ACK ||
                hdr.efcp.pdu_type == RENDEVOUS)
            , error.wrong_pdu_type);
        transition accept;
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition accept;
    }
}

/*************************************************************************
************   C H E C K S U M    V E R I F I C A T I O N   *************
*************************************************************************/

/*
Verifies the checksum of the supplied data.  If this method detects
that a checksum of the data is not correct, then the value of the
standard_metadata checksum_error field will be equal to 1 when the
packet begins ingress processing.
*/

control SwitchIngressVerifyChecksum(inout header_t hdr,
                        inout metadata meta) {
    apply {
/*
* Verify checksum for EFCP packets
*/
            verify_checksum(hdr.efcp.isValid(),
            { hdr.efcp.ver,
              hdr.efcp.dst_addr,
              hdr.efcp.src_addr,
              hdr.efcp.qos_id,
              hdr.efcp.dst_cep_id,
              hdr.efcp.src_cep_id,
              hdr.efcp.pdu_type,
              hdr.efcp.flags,
              hdr.efcp.len,
              hdr.efcp.seqnum },
            hdr.efcp.hdr_checksum,
            HashAlgorithm.csum16);

/*
* Verify checksum for IPv4 packets
*/
            verify_checksum(hdr.ipv4.isValid(),
            { hdr.ipv4.version,
	          hdr.ipv4.ihl,
              hdr.ipv4.diffserv,
              hdr.ipv4.total_len,
              hdr.ipv4.identification,
              hdr.ipv4.flags,
              hdr.ipv4.frag_offset,
              hdr.ipv4.ttl,
              hdr.ipv4.protocol,
              hdr.ipv4.src_addr,
              hdr.ipv4.dst_addr
              },
            hdr.ipv4.hdr_checksum,
            HashAlgorithm.csum16);
    }
}

/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/
control SwitchIngress(inout header_t hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

/* 
 * Counters
 */
    counter(64, CounterType.packets) ipv4_counter;
    counter(64, CounterType.packets) efcp_counter;

/*
 *
/*
Primitive action mark_to_drop, have the side effect of assigning an
implementation specific value DROP_PORT to this field (511 decimal for
simple_switch by default, but can be changed through the --drop-port
target-specific command-line option), such that if egress_spec has that value
at the end of ingress processing, the packet will be dropped and not stored in
the packet buffer, nor sent to egress processing.
*/
    action drop() {
        mark_to_drop(standard_metadata);
    }

/*
 * EFCP forwarding action
 */
    action efcp_forward(bit<12> vlan_id, mac_addr_t dst_mac, egress_spec dst_port) {
        hdr.dot1q.vlan_id = vlan_id;
        standard_metadata.egress_spec = dst_port;
        hdr.ethernet.src_addr = hdr.ethernet.dst_addr;
        hdr.ethernet.dst_addr = dst_mac;
        efcp_counter.count((bit<32>) standard_metadata.ingress_port);
    }

/*
 * IPv4 forwarding action
 */
    action ipv4_forward(bit<12> vlan_id, mac_addr_t dst_mac, egress_spec dst_port) {
        hdr.dot1q.vlan_id = vlan_id;
        standard_metadata.egress_spec = dst_port;
        hdr.ethernet.src_addr = hdr.ethernet.dst_addr;
        hdr.ethernet.dst_addr = dst_mac;
        hdr.ipv4.ttl = hdr.ipv4.ttl - 1;
        ipv4_counter.count((bit<32>) standard_metadata.ingress_port);
    }

/*
 * EFCP exact table
 */
    table efcp_exact {
        key = {
            hdr.efcp.dst_addr: exact;
        }
        actions = {
            efcp_forward;
            drop;
            NoAction;
        }
        size = 1024;
        default_action = NoAction();
    }

/*
 * IPv4 long prefix match table
 */
    table ipv4_lpm {
        key = {
            hdr.ipv4.dst_addr: lpm;
        }
        actions = {
            ipv4_forward;
            drop;
            NoAction;
        }
        size = 1024;
        default_action = drop();
    }

    apply {
        if (hdr.efcp.isValid() &&
            standard_metadata.checksum_error == 0 &&
            standard_metadata.parser_error == error.NoError) {
                if (hdr.efcp.pdu_type == LAYER_MANAGEMENT) {
                    standard_metadata.egress_spec = CPU_PORT;
                } else {
                    efcp_exact.apply();
                }
        } else if (hdr.ipv4.isValid() &&
            standard_metadata.checksum_error == 0) {
                ipv4_lpm.apply();
        } else {
            drop();
        }
    }
}
/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control SwitchEgress(inout header_t hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {

    action mark_ecn() {
        hdr.efcp.flags = hdr.efcp.flags | 0x01;
    }

    apply {
        if (standard_metadata.enq_qdepth >= ECN_THRESHOLD) {
            mark_ecn();
        }

        if (hdr.dot1q.vlan_id == 0) {
            hdr.dot1q.setInvalid();
        }
    }
}

/*************************************************************************
*************   C H E C K S U M    C O M P U T A T I O N   **************
*************************************************************************/

control SwitchEgressComputeChecksum(inout header_t hdr, inout metadata meta) {
    apply {
/*
* Compute checksum for EFCP packets
*/
    update_checksum(
        hdr.efcp.isValid(),
            { hdr.efcp.ver,
              hdr.efcp.dst_addr,
              hdr.efcp.src_addr,
              hdr.efcp.qos_id,
              hdr.efcp.dst_cep_id,
              hdr.efcp.src_cep_id,
              hdr.efcp.pdu_type,
              hdr.efcp.flags,
              hdr.efcp.len,
              hdr.efcp.seqnum },
            hdr.efcp.hdr_checksum,
            HashAlgorithm.csum16);

/*
* Compute checksum for IPv4 packets
*/
    update_checksum(
        hdr.ipv4.isValid(),
            { hdr.ipv4.version,
              hdr.ipv4.ihl,
              hdr.ipv4.diffserv,
              hdr.ipv4.total_len,
              hdr.ipv4.identification,
              hdr.ipv4.flags,
              hdr.ipv4.frag_offset,
              hdr.ipv4.ttl,
              hdr.ipv4.protocol,
              hdr.ipv4.src_addr,
              hdr.ipv4.dst_addr },
            hdr.ipv4.hdr_checksum,
            HashAlgorithm.csum16);
   }
}

/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/

control SwitchEgressDeparser(packet_out packet, in header_t hdr) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.dot1q);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
    }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

V1Switch(
SwitchIngressParser(),
SwitchIngressVerifyChecksum(),
SwitchIngress(),
SwitchEgress(),
SwitchEgressComputeChecksum(),
SwitchEgressDeparser()
) main;
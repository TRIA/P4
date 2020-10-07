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
    WrongPDUtype
}

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/
parser MyParser(packet_in packet,
                out header_t hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata) {

    state start {
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            IPV4_ETYPE: parse_ipv4;
            default: accept;
        }
    }

    state parse_vlan {
        packet.extract(hdr.vlan);
        transition select(hdr.vlan.etherType) {
            EFCP_ETYPE: parse_efcp;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        verify((hdr.efcp.pduType == DATA_TRANSFER ||
                hdr.efcp.pduType == LAYER_MANAGEMENT ||
                hdr.efcp.pduType == ACK_ONLY ||
                hdr.efcp.pduType == NACK_ONLY ||
                hdr.efcp.pduType == ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == FLOW_CONTROL_ONLY ||
                hdr.efcp.pduType == SELECTIVE_ACK ||
                hdr.efcp.pduType == SELECTIVE_NACK ||
                hdr.efcp.pduType == SELECTIVE_ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == SELECTIVE_NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == CONTROL_ACK ||
                hdr.efcp.pduType == RENDEVOUS)
            , error.WrongPDUtype);
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

control MyVerifyChecksum(inout header_t hdr,
                        inout metadata meta) {
    apply {
/*
* Verify checksum for EFCP packets
*/
            verify_checksum(hdr.efcp.isValid(),
            { hdr.efcp.ver,
	      hdr.efcp.dstAddr,
              hdr.efcp.srcAddr,
              hdr.efcp.qosID,
              hdr.efcp.dstCEPID,
              hdr.efcp.srcCEPID,
              hdr.efcp.pduType,
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
              hdr.ipv4.srcAddr,
              hdr.ipv4.dstAddr
              },
            hdr.ipv4.hdr_checksum,
            HashAlgorithm.csum16);
    }
}

/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/
control MyIngress(inout header_t hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

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
    action efcp_forward(bit<12> vlan_id, mac_addr_t dstAddr, egress_spec port) {
        hdr.vlan.vlan_id = vlan_id;
        standard_metadata.egress_spec = port;
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;
        hdr.ethernet.dstAddr = dstAddr;
    }

/*
 * IPv4 forwarding action
 */
    action ipv4_forward(mac_addr_t dstAddr, egress_spec port) {
        standard_metadata.egress_spec = port;
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;
        hdr.ethernet.dstAddr = dstAddr;
        hdr.ipv4.ttl = hdr.ipv4.ttl - 1;
    }

/*
 * EFCP exact table
 */
    table efcp_lpm {
        key = {
            hdr.efcp.dstAddr: exact;
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
            hdr.ipv4.dstAddr: lpm;
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
                if (hdr.efcp.pduType == LAYER_MANAGEMENT) {
                    standard_metadata.egress_spec = CPU_PORT;
                } else {
                    efcp_lpm.apply();
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

control MyEgress(inout header_t hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {

    action mark_ecn() {
        hdr.efcp.flags = hdr.efcp.flags | 0x01;
    }

    apply {
        if (standard_metadata.enq_qdepth >= ECN_THRESHOLD) {
            mark_ecn();
        }

        if (hdr.vlan.vlan_id == 0) {
            hdr.vlan.setInvalid();
        }
    }
}

/*************************************************************************
*************   C H E C K S U M    C O M P U T A T I O N   **************
*************************************************************************/

control MyComputeChecksum(inout header_t hdr, inout metadata meta) {
    apply {
/*
* Compute checksum for EFCP packets
*/
	update_checksum(
	    hdr.efcp.isValid(),
            { hdr.efcp.ver,
	      hdr.efcp.dstAddr,
              hdr.efcp.srcAddr,
              hdr.efcp.qosID,
              hdr.efcp.dstCEPID,
              hdr.efcp.srcCEPID,
              hdr.efcp.pduType,
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
              hdr.ipv4.srcAddr,
              hdr.ipv4.dstAddr },
            hdr.ipv4.hdr_checksum,
            HashAlgorithm.csum16);
   }
}

/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/

control MyDeparser(packet_out packet, in header_t hdr) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.vlan);

    }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

V1Switch(
MyParser(),
MyVerifyChecksum(),
MyIngress(),
MyEgress(),
MyComputeChecksum(),
MyDeparser()
) main;
